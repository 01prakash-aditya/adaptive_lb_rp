#include "server.hpp"
#include "../http/parser.hpp"
#include "../logging/logger.hpp"
#include "../metrics/registry.hpp"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace proxy::server {

Server::Server(uint16_t port, 
               std::vector<std::shared_ptr<lb::Backend>> backends,
               const std::string& strategy_name,
               double rate_limit_rps)
    : port_(port) {
    
    // Initialize LB strategy
    if (strategy_name == "wrr") {
        load_balancer_ = std::make_unique<lb::WeightedRoundRobinLB>(backends);
    } else if (strategy_name == "lc") {
        load_balancer_ = std::make_unique<lb::LeastConnectionsLB>(backends);
    } else if (strategy_name == "iphash") {
        load_balancer_ = std::make_unique<lb::IPHashLB>(backends);
    } else {
        load_balancer_ = std::make_unique<lb::RoundRobinLB>(backends);
    }

    // Rate Limiter: global limit, and a strict per-IP limit
    rate_limiter_ = std::make_unique<ratelimit::RateLimiter>(
        rate_limit_rps * 10, rate_limit_rps,   // Global bucket
        rate_limit_rps, rate_limit_rps         // Per-IP bucket
    );

    // Health Checker: checks every 5 seconds, max 3 failures
    health_checker_ = std::make_unique<lb::HealthChecker>(backends, std::chrono::seconds(5), 3);

    forwarder_ = std::make_unique<forwarder::Forwarder>();

    // Cache: 1000 items max, 60s TTL
    cache_ = std::make_unique<cache::LRUCache>(1000, std::chrono::seconds(60));
}

Server::~Server() {
    stop();
}

void Server::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void Server::run() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        logging::Logger::get_instance().log_error("Failed to create server socket");
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(server_fd_);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logging::Logger::get_instance().log_error("Bind failed");
        return;
    }

    if (listen(server_fd_, SOMAXCONN) < 0) {
        logging::Logger::get_instance().log_error("Listen failed");
        return;
    }

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        logging::Logger::get_instance().log_error("epoll_create1 failed");
        return;
    }

    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev);

    running_ = true;
    logging::Logger::get_instance().log_info("Server started on port " + std::to_string(port_));

    // Start background health checking
    health_checker_->start();

    const int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];

    auto last_timeout_check = std::chrono::steady_clock::now();

    while (running_) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server_fd_) {
                handle_accept();
            } else {
                if (events[i].events & EPOLLIN) {
                    handle_read(events[i].data.fd);
                }
                if (events[i].events & EPOLLOUT) {
                    handle_write(events[i].data.fd);
                }
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    close_connection(events[i].data.fd);
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        if (now - last_timeout_check > std::chrono::seconds(5)) {
            check_timeouts();
            last_timeout_check = now;
        }
    }

    health_checker_->stop();
    close(server_fd_);
    close(epoll_fd_);
}

void Server::stop() {
    running_ = false;
}

void Server::handle_accept() {
    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                continue;
            }
        }

        set_nonblocking(client_fd);
        
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip, INET_ADDRSTRLEN);

        Connection conn;
        conn.fd = client_fd;
        conn.client_ip = ip;
        conn.last_activity = std::chrono::steady_clock::now();
        connections_[client_fd] = std::move(conn);

        metrics::MetricsRegistry::get_instance().active_connections++;

        struct epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);
    }
}

void Server::handle_read(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;
    Connection& conn = it->second;
    conn.last_activity = std::chrono::steady_clock::now();

    char buf[4096];
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            conn.read_buffer.append(buf, n);
        } else if (n == 0) {
            close_connection(fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            close_connection(fd);
            return;
        }
    }

    auto parse_result = http::HttpParser::parse_request(conn.read_buffer);
    if (parse_result.state == http::HttpParser::ParseState::COMPLETE) {
        conn.state = ConnectionState::FORWARDING;
        metrics::MetricsRegistry::get_instance().requests_total++;
        
        // 0. Intercept /metrics for Prometheus
        if (parse_result.request->uri == "/metrics") {
            std::string metrics_body = metrics::MetricsRegistry::get_instance().generate_prometheus_metrics();
            http::HttpResponse resp{200, "OK", "HTTP/1.1", 
                {{"Content-Type", "text/plain; version=0.0.4"}, {"Connection", "close"}}, 
                metrics_body};
            conn.write_buffer = http::HttpParser::serialize_response(resp);
            conn.state = ConnectionState::WRITING_RESPONSE;
            struct epoll_event ev{};
            ev.events = EPOLLOUT | EPOLLET;
            ev.data.fd = fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
            return;
        }

        // 1. Check Rate Limiter
        if (!rate_limiter_->allow_request(conn.client_ip)) {
            metrics::MetricsRegistry::get_instance().rate_limit_exceeded_total++;
            http::HttpResponse resp{429, "Too Many Requests", "HTTP/1.1", {{"Connection", "close"}}, "Rate limit exceeded"};
            conn.write_buffer = http::HttpParser::serialize_response(resp);
        } else {
            // 2. Check LRU Cache for GET requests
            std::string cache_key = parse_result.request->method + ":" + parse_result.request->uri;
            std::optional<http::HttpResponse> cached_resp = std::nullopt;
            
            if (parse_result.request->method == "GET") {
                cached_resp = cache_->get(cache_key);
            }

            if (cached_resp) {
                // Cache HIT
                metrics::MetricsRegistry::get_instance().cache_hits_total++;
                cached_resp->headers["X-Cache"] = "HIT";
                conn.write_buffer = http::HttpParser::serialize_response(*cached_resp);
            } else {
                // Cache MISS
                metrics::MetricsRegistry::get_instance().cache_misses_total++;
                auto backend = load_balancer_->get_next(conn.client_ip);
                
                try {
                    auto response = forwarder_->forward(*parse_result.request, backend, conn.client_ip);
                    
                    if (response.status_code >= 500) {
                        metrics::MetricsRegistry::get_instance().backend_errors_total++;
                    } else if (parse_result.request->method == "GET" && response.status_code == 200) {
                        // Insert into cache
                        cache_->put(cache_key, response);
                    }

                    response.headers["X-Cache"] = "MISS";
                    conn.write_buffer = http::HttpParser::serialize_response(response);
                } catch (...) {
                    metrics::MetricsRegistry::get_instance().backend_errors_total++;
                    http::HttpResponse resp{502, "Bad Gateway", "HTTP/1.1", {{"Connection", "close"}}, "Backend error"};
                    conn.write_buffer = http::HttpParser::serialize_response(resp);
                }
            }
        }

        conn.state = ConnectionState::WRITING_RESPONSE;
        
        struct epoll_event ev{};
        ev.events = EPOLLOUT | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    } else if (parse_result.state == http::HttpParser::ParseState::ERROR) {
        close_connection(fd);
    }
}

void Server::handle_write(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;
    Connection& conn = it->second;
    conn.last_activity = std::chrono::steady_clock::now();

    while (conn.write_offset < conn.write_buffer.size()) {
        ssize_t n = write(fd, conn.write_buffer.c_str() + conn.write_offset, conn.write_buffer.size() - conn.write_offset);
        if (n > 0) {
            conn.write_offset += n;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            close_connection(fd);
            return;
        }
    }

    if (conn.write_offset == conn.write_buffer.size()) {
        // Keep-alive parsing for client connection would go here, 
        // for simplicity of the proxy we close the connection when done writing.
        close_connection(fd);
    }
}

void Server::close_connection(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    connections_.erase(fd);
    metrics::MetricsRegistry::get_instance().active_connections--;
}

void Server::check_timeouts() {
    auto now = std::chrono::steady_clock::now();
    std::vector<int> to_close;
    for (const auto& [fd, conn] : connections_) {
        if (now - conn.last_activity > std::chrono::seconds(30)) {
            to_close.push_back(fd);
        }
    }
    for (int fd : to_close) {
        close_connection(fd);
    }
}

}
