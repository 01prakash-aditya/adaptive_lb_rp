#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <chrono>
#include "../proxy/forwarder.hpp"

namespace proxy::server {

class Server {
public:
    Server(uint16_t port, const std::string& backend_addr);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void run();
    void stop();

private:
    uint16_t port_;
    std::string backend_addr_;
    std::unique_ptr<forwarder::Forwarder> forwarder_;
    int epoll_fd_ = -1;
    int server_fd_ = -1;
    std::atomic<bool> running_{false};

    enum class ConnectionState {
        READING_REQUEST,
        FORWARDING,
        WRITING_RESPONSE
    };

    struct Connection {
        int fd;
        std::string client_ip;
        ConnectionState state = ConnectionState::READING_REQUEST;
        std::string read_buffer;
        std::string write_buffer;
        size_t write_offset = 0;
        std::chrono::steady_clock::time_point last_activity;
    };

    std::unordered_map<int, Connection> connections_;

    void set_nonblocking(int fd);
    void handle_accept();
    void handle_read(int fd);
    void handle_write(int fd);
    void close_connection(int fd);
    void check_timeouts();
};

}
