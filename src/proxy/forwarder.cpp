#include "forwarder.hpp"
#include "../http/parser.hpp"
#include "../logging/logger.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <cstring>

namespace proxy::forwarder {

Forwarder::Forwarder(std::string backend_host, uint16_t backend_port)
    : backend_host_(std::move(backend_host)), backend_port_(backend_port) {}

http::HttpResponse Forwarder::forward(http::HttpRequest req, const std::string& client_ip) {
    auto start_time = std::chrono::steady_clock::now();

    req.headers["X-Request-ID"] = req.request_id;
    if (!client_ip.empty()) {
        req.headers["X-Forwarded-For"] = client_ip;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logging::Logger::get_instance().log_error("Failed to create socket for backend");
        return http::HttpResponse{502, "Bad Gateway", "HTTP/1.1", {}, "Failed to connect to backend"};
    }

    // Use getaddrinfo for DNS resolution (supports Docker service names)
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string port_str = std::to_string(backend_port_);
    int gai_err = getaddrinfo(backend_host_.c_str(), port_str.c_str(), &hints, &res);
    if (gai_err != 0 || res == nullptr) {
        close(sock);
        logging::Logger::get_instance().log_error("DNS resolution failed for " + backend_host_);
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
        return http::HttpResponse{502, "Bad Gateway", "HTTP/1.1", {}, "Connection refused"};
    }
    freeaddrinfo(res);

    std::string req_str = http::HttpParser::serialize_request(req);
    ssize_t sent = send(sock, req_str.c_str(), req_str.size(), 0);
    if (sent < 0 || (size_t)sent != req_str.size()) {
        close(sock);
        return http::HttpResponse{502, "Bad Gateway", "HTTP/1.1", {}, "Failed to send request to backend"};
    }

    std::string res_raw;
    char buffer[4096];
    while (true) {
        ssize_t bytes_read = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_read > 0) {
            res_raw.append(buffer, bytes_read);
            auto parse_result = http::HttpParser::parse_response(res_raw);
            if (parse_result.state == http::HttpParser::ParseState::COMPLETE) {
                close(sock);
                
                auto end_time = std::chrono::steady_clock::now();
                std::chrono::duration<double, std::milli> latency = end_time - start_time;
                logging::Logger::get_instance().log_request(
                    req.request_id, req.method, req.uri, parse_result.response->status_code, 
                    latency.count(), backend_host_ + ":" + std::to_string(backend_port_)
                );

                return std::move(*parse_result.response);
            } else if (parse_result.state == http::HttpParser::ParseState::ERROR) {
                close(sock);
                return http::HttpResponse{502, "Bad Gateway", "HTTP/1.1", {}, "Invalid response from backend"};
            }
        } else if (bytes_read == 0) {
            break;
        } else {
            close(sock);
            return http::HttpResponse{504, "Gateway Timeout", "HTTP/1.1", {}, "Timeout reading from backend"};
        }
    }
    
    close(sock);
    auto parse_result = http::HttpParser::parse_response(res_raw);
    if (parse_result.state == http::HttpParser::ParseState::COMPLETE) {
        return std::move(*parse_result.response);
    }
    return http::HttpResponse{502, "Bad Gateway", "HTTP/1.1", {}, "Incomplete response from backend"};
}

}
