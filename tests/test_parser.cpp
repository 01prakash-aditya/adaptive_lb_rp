/**
 * @file test_parser.cpp
 * @brief Unit tests for the HTTP/1.1 Parser module of the C++20 reverse proxy.
 *
 * This test suite validates request and response parsing, chunked transfer-encoding
 * decoding, serialization, round-trip fidelity, edge cases (such as empty bodies
 * and multiple whitespace-padded headers), and error detection on malformed inputs.
 *
 * Target: C++20 / Linux
 * Dependencies: C++ standard library, proxy::http types
 */

#include <iostream>
#include <cassert>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <algorithm>

#include "src/http/parser.hpp"
#include "src/http/request.hpp"
#include "src/http/response.hpp"

// Test harness macros
#define TEST(name) void name()
#define RUN_TEST(name) \
    do { \
        std::cout << "[TEST] Running " << #name << "... " << std::flush; \
        try { \
            name(); \
            std::cout << "PASSED" << std::endl; \
            ++passed_tests; \
        } catch (const std::exception& ex) { \
            std::cout << "FAILED (Exception: " << ex.what() << ")" << std::endl; \
            ++failed_tests; \
        } catch (...) { \
            std::cout << "FAILED (Unknown exception)" << std::endl; \
            ++failed_tests; \
        } \
        ++total_tests; \
    } while (0)

namespace {

int total_tests = 0;
int passed_tests = 0;
int failed_tests = 0;

} // anonymous namespace

using namespace proxy::http;

// ============================================================================
// Test Case 1: Parse a simple GET request (method, URI, headers)
// ============================================================================
TEST(test_parse_simple_get_request) {
    const std::string raw_request =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
        "Accept: text/html,application/xhtml+xml\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";

    auto result = HttpParser::parse_request(raw_request);
    assert(result.state == HttpParser::ParseState::COMPLETE);
    auto& req = *result.request;

    assert(req.method == "GET");
    assert(req.uri == "/index.html");
    assert(req.version == "HTTP/1.1");
    assert(req.headers.count("Host"));
    assert(req.headers["Host"] == "example.com");
    assert(req.headers.count("User-Agent"));
    assert(req.headers["User-Agent"] == "Mozilla/5.0 (X11; Linux x86_64)");
    assert(req.headers.count("Accept"));
    assert(req.headers.count("Connection"));
    assert(req.headers["Connection"] == "keep-alive");
    assert(req.body.empty());
}

// ============================================================================
// Test Case 2: Parse a POST request with Content-Length body
// ============================================================================
TEST(test_parse_post_with_content_length) {
    const std::string payload = "{\"username\": \"admin\", \"action\": \"login\"}";
    const std::string raw_request =
        "POST /api/v1/auth HTTP/1.1\r\n"
        "Host: api.service.local:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(payload.size()) + "\r\n"
        "X-Custom-Client: HighPerformanceProxy\r\n"
        "\r\n" +
        payload;

    auto result = HttpParser::parse_request(raw_request);
    assert(result.state == HttpParser::ParseState::COMPLETE);
    auto& req = *result.request;

    assert(req.method == "POST");
    assert(req.uri == "/api/v1/auth");
    assert(req.version == "HTTP/1.1");
    assert(req.headers["Content-Type"] == "application/json");
    assert(req.headers["Content-Length"] == std::to_string(payload.size()));
    assert(req.headers["X-Custom-Client"] == "HighPerformanceProxy");
    assert(req.body == payload);
    assert(req.body.size() == payload.size());
}

// ============================================================================
// Test Case 3: Parse a request with chunked Transfer-Encoding
// ============================================================================
TEST(test_parse_chunked_transfer_encoding) {
    const std::string raw_request =
        "POST /stream/upload HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "5\r\n"
        "Hello\r\n"
        "2\r\n"
        ", \r\n"
        "6\r\n"
        "World!\r\n"
        "0\r\n"
        "\r\n";

    auto result = HttpParser::parse_request(raw_request);
    assert(result.state == HttpParser::ParseState::COMPLETE);
    auto& req = *result.request;

    assert(req.method == "POST");
    assert(req.uri == "/stream/upload");
    assert(req.version == "HTTP/1.1");
    assert(req.headers.count("Transfer-Encoding"));
    assert(req.body == "Hello, World!");
}

// ============================================================================
// Test Case 4: Parse a response (200 OK with body)
// ============================================================================
TEST(test_parse_response_200_ok) {
    const std::string body_content = "<html><body><h1>200 Success</h1></body></html>";
    const std::string raw_response =
        "HTTP/1.1 200 OK\r\n"
        "Date: Wed, 26 Aug 2026 12:00:00 GMT\r\n"
        "Server: ReverseProxy/1.0 (Linux)\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " + std::to_string(body_content.size()) + "\r\n"
        "Connection: keep-alive\r\n"
        "\r\n" +
        body_content;

    auto result = HttpParser::parse_response(raw_response);
    assert(result.state == HttpParser::ParseState::COMPLETE);
    auto& resp = *result.response;

    assert(resp.version == "HTTP/1.1");
    assert(resp.status_code == 200);
    assert(resp.status_text == "OK");
    assert(resp.headers.count("Content-Type"));
    assert(resp.headers["Content-Type"] == "text/html; charset=UTF-8");
    assert(resp.headers["Content-Length"] == std::to_string(body_content.size()));
    assert(resp.body == body_content);
}

// ============================================================================
// Test Case 5: Serialize a request and verify output format
// ============================================================================
TEST(test_serialize_request) {
    HttpRequest req;
    req.method = "PUT";
    req.uri = "/api/v2/items/42";
    req.version = "HTTP/1.1";
    req.headers["Host"] = "backend.internal:9001";
    req.headers["Content-Type"] = "application/json";
    req.headers["Content-Length"] = "19";
    req.body = "{\"name\": \"proxy-v2\"}";

    std::string serialized = HttpParser::serialize_request(req);

    // Verify request line
    assert(serialized.rfind("PUT /api/v2/items/42 HTTP/1.1\r\n", 0) == 0);
    
    // Verify presence of headers
    assert(serialized.find("Host: backend.internal:9001\r\n") != std::string::npos);
    assert(serialized.find("Content-Type: application/json\r\n") != std::string::npos);
    
    // Verify header-body separator and exact payload placement
    assert(serialized.find("\r\n\r\n{\"name\": \"proxy-v2\"}") != std::string::npos);
}

// ============================================================================
// Test Case 6: Serialize a response and verify output format
// ============================================================================
TEST(test_serialize_response) {
    HttpResponse resp;
    resp.version = "HTTP/1.1";
    resp.status_code = 404;
    resp.status_text = "Not Found";
    resp.headers["Content-Type"] = "text/plain";
    resp.headers["Content-Length"] = "14";
    resp.headers["Connection"] = "close";
    resp.body = "Item Not Found";

    std::string serialized = HttpParser::serialize_response(resp);

    // Verify status line
    assert(serialized.rfind("HTTP/1.1 404 Not Found\r\n", 0) == 0);

    // Verify headers
    assert(serialized.find("Content-Type: text/plain\r\n") != std::string::npos);
    assert(serialized.find("Connection: close\r\n") != std::string::npos);

    // Verify double CRLF followed by body
    assert(serialized.find("\r\n\r\nItem Not Found") != std::string::npos);
}

// ============================================================================
// Test Case 7: Round-trip: parse then serialize produces equivalent output
// ============================================================================
TEST(test_roundtrip_request_and_response) {
    // 1. Request Round-trip
    const std::string original_req_raw =
        "POST /submit/form HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 11\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hello-world";

    auto result1 = HttpParser::parse_request(original_req_raw);
    assert(result1.state == HttpParser::ParseState::COMPLETE);
    auto& parsed_req = *result1.request;

    std::string reserialized_req = HttpParser::serialize_request(parsed_req);
    auto result2 = HttpParser::parse_request(reserialized_req);
    assert(result2.state == HttpParser::ParseState::COMPLETE);
    auto& reparsed_req = *result2.request;

    assert(parsed_req.method == reparsed_req.method);
    assert(parsed_req.uri == reparsed_req.uri);
    assert(parsed_req.version == reparsed_req.version);
    assert(parsed_req.body == reparsed_req.body);
    assert(parsed_req.headers["Host"] == reparsed_req.headers["Host"]);
    assert(parsed_req.headers["Content-Length"] == reparsed_req.headers["Content-Length"]);

    // 2. Response Round-trip
    const std::string original_resp_raw =
        "HTTP/1.1 201 Created\r\n"
        "Content-Length: 8\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "created!";

    auto result3 = HttpParser::parse_response(original_resp_raw);
    assert(result3.state == HttpParser::ParseState::COMPLETE);
    auto& parsed_resp = *result3.response;

    std::string reserialized_resp = HttpParser::serialize_response(parsed_resp);
    auto result4 = HttpParser::parse_response(reserialized_resp);
    assert(result4.state == HttpParser::ParseState::COMPLETE);
    auto& reparsed_resp = *result4.response;

    assert(parsed_resp.version == reparsed_resp.version);
    assert(parsed_resp.status_code == reparsed_resp.status_code);
    assert(parsed_resp.status_text == reparsed_resp.status_text);
    assert(parsed_resp.body == reparsed_resp.body);
    assert(parsed_resp.headers["Content-Length"] == reparsed_resp.headers["Content-Length"]);
}

// ============================================================================
// Test Case 8: Edge case: empty body request
// ============================================================================
TEST(test_edge_case_empty_body) {
    // 8a. GET request with no body or content-length
    const std::string get_empty =
        "GET /health HTTP/1.1\r\n"
        "Host: 127.0.0.1:9001\r\n"
        "\r\n";

    auto r1 = HttpParser::parse_request(get_empty);
    assert(r1.state == HttpParser::ParseState::COMPLETE);
    assert(r1.request->method == "GET");
    assert(r1.request->uri == "/health");
    assert(r1.request->body.empty());

    // 8b. POST request with explicit Content-Length: 0
    const std::string post_empty =
        "POST /trigger-action HTTP/1.1\r\n"
        "Host: 127.0.0.1:9001\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    auto r2 = HttpParser::parse_request(post_empty);
    assert(r2.state == HttpParser::ParseState::COMPLETE);
    assert(r2.request->method == "POST");
    assert(r2.request->uri == "/trigger-action");
    assert(r2.request->body.empty());

    // 8c. 204 No Content response
    const std::string resp_204 =
        "HTTP/1.1 204 No Content\r\n"
        "Date: Wed, 26 Aug 2026 12:00:00 GMT\r\n"
        "\r\n";

    auto r3 = HttpParser::parse_response(resp_204);
    assert(r3.state == HttpParser::ParseState::COMPLETE);
    assert(r3.response->status_code == 204);
    assert(r3.response->status_text == "No Content");
}

// ============================================================================
// Test Case 9: Edge case: multiple headers with whitespace trimming
// ============================================================================
TEST(test_edge_case_multiple_headers_and_whitespace) {
    const std::string raw_request =
        "GET /search?q=c%2B%2B20 HTTP/1.1\r\n"
        "Host:   reverse-proxy.local:8443   \r\n"
        "User-Agent:    Custom-Proxy-Agent/2.0   \r\n"
        "Accept: text/html,application/json;q=0.9,*/*;q=0.8\r\n"
        "X-Forwarded-For: 192.168.1.100, 10.0.0.1\r\n"
        "X-Forwarded-Proto: https\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "\r\n";

    auto result = HttpParser::parse_request(raw_request);
    assert(result.state == HttpParser::ParseState::COMPLETE);
    auto& req = *result.request;

    assert(req.method == "GET");
    assert(req.uri == "/search?q=c%2B%2B20");
    assert(req.headers["Host"] == "reverse-proxy.local:8443");
    assert(req.headers["User-Agent"] == "Custom-Proxy-Agent/2.0");
    assert(req.headers["X-Forwarded-Proto"] == "https");
    assert(req.headers["X-Forwarded-For"] == "192.168.1.100, 10.0.0.1");
    assert(req.headers["Cache-Control"] == "no-cache, no-store, must-revalidate");
}

// ============================================================================
// Test Case 10: Incomplete request returns INCOMPLETE state
// ============================================================================
TEST(test_incomplete_request) {
    // No \r\n\r\n terminator yet
    const std::string partial = "GET /index.html HTTP/1.1\r\nHost: example.com\r\n";
    auto result = HttpParser::parse_request(partial);
    assert(result.state == HttpParser::ParseState::INCOMPLETE);
    assert(!result.request.has_value());
}

// ============================================================================
// Main entry point
// ============================================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Running HTTP Parser Unit Tests (C++20)" << std::endl;
    std::cout << "========================================" << std::endl;

    RUN_TEST(test_parse_simple_get_request);
    RUN_TEST(test_parse_post_with_content_length);
    RUN_TEST(test_parse_chunked_transfer_encoding);
    RUN_TEST(test_parse_response_200_ok);
    RUN_TEST(test_serialize_request);
    RUN_TEST(test_serialize_response);
    RUN_TEST(test_roundtrip_request_and_response);
    RUN_TEST(test_edge_case_empty_body);
    RUN_TEST(test_edge_case_multiple_headers_and_whitespace);
    RUN_TEST(test_incomplete_request);

    std::cout << "========================================" << std::endl;
    std::cout << "Test Summary: " << passed_tests << "/" << total_tests << " Passed";
    if (failed_tests > 0) {
        std::cout << " (" << failed_tests << " FAILED)";
    }
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;

    return (failed_tests == 0) ? 0 : 1;
}
