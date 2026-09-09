#include "forwarder.hpp"
#include "../http/parser.hpp"
#include "../logging/logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <chrono>

namespace proxy::forwarder {

http::HttpResponse Forwarder::forward(http::HttpRequest req, std::shared_ptr<proxy::lb::Backend> backend, const std::string& client_ip) {
    if (!backend) {
        return http::HttpResponse{503, "Service Unavailable", "HTTP/1.1", {}, "No healthy backends available"};
    }

    backend->active_connections.fetch_add(1);

    auto start_time = std::chrono::steady_clock::now();

    // 1. Connection Pooling: Try to get an idle socket
    int sock = backend->pop_idle_socket();
    
    // If no idle socket, create a new connection
    if (sock < 0) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            backend->active_connections.fetch_sub(1);
            logging::Logger::get_instance().log_error("Failed to create socket for backend " + backend->id);
            return http::HttpResponse{502, "Bad Gateway", "HTTP/1.1", {}, "Failed to connect to backend"};
        }

        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        std::string port_str = std::to_string(backend->port);
        int gai_err = getaddrinfo(backend->host.c_str(), port_str.c_str(), &hints, &res);
        if (gai_err != 0 || res == nullptr) {
            close(sock);
            backend->active_connections.fetch_sub(1);
            logging::Logger::get_instance().log_error("DNS resolution failed for " + backend->host);
            return http::HttpResponse{502, "Bad Gateway", "HTTP/1.1", {}, "DNS resolution failed for backend"};
        }

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

        if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
            close(sock);
            freeaddrinfo(res);
            
            // Passive health check: record failure
            int failures = backend->consecutive_failures.fetch_add(1) + 1;
            if (failures >= 3 && backend->healthy.load()) {
                backend->healthy.store(false);
                logging::Logger::get_instance().log_warn("Backend " + backend->id + " marked UNHEALTHY (passive)");
            }
            
            backend->active_connections.fetch_sub(1);
            return http::HttpResponse{502, "Bad Gateway", "HTTP/1.1", {}, "Connection refused"};
        }
        freeaddrinfo(res);
    }

    // Set trace headers
    if (!req.request_id.empty()) {
        req.headers["X-Request-ID"] = req.request_id;
    }
    if (!client_ip.empty()) {
        req.headers["X-Forwarded-For"] = client_ip;
    }

    // Always request keep-alive so we can pool it
    req.headers["Connection"] = "keep-alive";

    std::string req_str = http::HttpParser::serialize_request(req);
    ssize_t sent = send(sock, req_str.c_str(), req_str.size(), 0);
    if (sent < 0) {
        close(sock);
        backend->consecutive_failures.fetch_add(1);
        backend->active_connections.fetch_sub(1);
        return http::HttpResponse{502, "Bad Gateway", "HTTP/1.1", {}, "Failed to send request"};
    }

    std::string raw_resp;
    char buf[4096];
    while (true) {
        ssize_t bytes = recv(sock, buf, sizeof(buf), 0);
        if (bytes < 0) {
            close(sock);
            backend->consecutive_failures.fetch_add(1);
            backend->active_connections.fetch_sub(1);
            return http::HttpResponse{504, "Gateway Timeout", "HTTP/1.1", {}, "Backend read timeout"};
        } else if (bytes == 0) {
            break; // Connection closed by backend
        }
        raw_resp.append(buf, bytes);

        auto parse_res = http::HttpParser::parse_response(raw_resp);
        if (parse_res.state == http::HttpParser::ParseState::COMPLETE) {
            auto end_time = std::chrono::steady_clock::now();
            double latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            logging::Logger::get_instance().log_request(
                req.request_id, req.method, req.uri, parse_res.response->status_code, 
                latency_ms, backend->id
            );

            // Success, reset passive failure counter
            backend->consecutive_failures.store(0);
            backend->active_connections.fetch_sub(1);

            // Determine if we can pool the socket
            bool keep_alive = true;
            auto it = parse_res.response->headers.find("Connection");
            if (it != parse_res.response->headers.end() && it->second == "close") {
                keep_alive = false;
            }

            if (keep_alive) {
                backend->push_idle_socket(sock);
            } else {
                close(sock);
            }

            return parse_res.response.value();
        } else if (parse_res.state == http::HttpParser::ParseState::ERROR) {
            break;
        }
    }

    close(sock);
    backend->consecutive_failures.fetch_add(1);
    backend->active_connections.fetch_sub(1);
    return http::HttpResponse{502, "Bad Gateway", "HTTP/1.1", {}, "Invalid response from backend"};
}

}
