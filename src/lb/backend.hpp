#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <deque>
#include <memory>

namespace proxy::lb {

struct Backend {
    std::string id;
    std::string host;
    uint16_t port;
    int weight;

    std::atomic<bool> healthy{true};
    std::atomic<int> active_connections{0};
    std::atomic<int> consecutive_failures{0};

    std::mutex pool_mutex;
    std::deque<int> idle_sockets;

    Backend(std::string id, std::string host, uint16_t port, int weight = 1)
        : id(std::move(id)), host(std::move(host)), port(port), weight(weight) {}

    ~Backend() {
        // We'll rely on the OS to close, or we can close them properly if needed.
        // But for simplicity, we let the connection manager handle cleanup if needed.
    }

    int pop_idle_socket() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        if (idle_sockets.empty()) return -1;
        int fd = idle_sockets.front();
        idle_sockets.pop_front();
        return fd;
    }

    void push_idle_socket(int fd) {
        std::lock_guard<std::mutex> lock(pool_mutex);
        idle_sockets.push_back(fd);
    }
};

}
