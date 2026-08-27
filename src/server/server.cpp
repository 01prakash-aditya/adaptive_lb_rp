#include "server.hpp"
#include "../http/parser.hpp"
#include "../logging/logger.hpp"
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

Server::Server(uint16_t port, const std::string& backend_addr)
    : port_(port), backend_addr_(backend_addr) {
    
    size_t colon = backend_addr.find(':');
    std::string host = "127.0.0.1";
    uint16_t bport = 80;
    if (colon != std::string::npos) {
        host = backend_addr.substr(0, colon);
        bport = std::stoi(backend_addr.substr(colon + 1));
    }
    forwarder_ = std::make_unique<forwarder::Forwarder>(host, bport);
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
        auto response = forwarder_->forward(*parse_result.request, conn.client_ip);
        conn.write_buffer = http::HttpParser::serialize_response(response);
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
        close_connection(fd);
    }
}

void Server::close_connection(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    connections_.erase(fd);
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
