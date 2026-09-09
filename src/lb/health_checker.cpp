#include "health_checker.hpp"
#include "../logging/logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace proxy::lb {

HealthChecker::HealthChecker(std::vector<std::shared_ptr<Backend>> backends, 
                             std::chrono::milliseconds interval,
                             int max_failures)
    : backends_(std::move(backends)), interval_(interval), max_failures_(max_failures) {}

HealthChecker::~HealthChecker() {
    stop();
}

void HealthChecker::start() {
    if (running_.exchange(true)) return;
    check_thread_ = std::thread(&HealthChecker::run_checks, this);
}

void HealthChecker::stop() {
    if (!running_.exchange(false)) return;
    if (check_thread_.joinable()) {
        check_thread_.join();
    }
}

bool HealthChecker::ping_backend(const std::shared_ptr<Backend>& backend) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string port_str = std::to_string(backend->port);
    
    if (getaddrinfo(backend->host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
        close(sock);
        return false;
    }

    struct timeval tv;
    tv.tv_sec = 2; // 2 seconds timeout for health check connect
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    bool success = (connect(sock, res->ai_addr, res->ai_addrlen) == 0);
    
    freeaddrinfo(res);
    close(sock);
    return success;
}

void HealthChecker::run_checks() {
    logging::Logger::get_instance().log_info("HealthChecker thread started");

    while (running_.load()) {
        for (auto& backend : backends_) {
            if (!running_.load()) break;

            bool is_up = ping_backend(backend);
            
            if (is_up) {
                backend->consecutive_failures.store(0);
                if (!backend->healthy.load()) {
                    backend->healthy.store(true);
                    logging::Logger::get_instance().log_info("Backend " + backend->id + " recovered and is healthy");
                }
            } else {
                int failures = backend->consecutive_failures.fetch_add(1) + 1;
                if (failures >= max_failures_ && backend->healthy.load()) {
                    backend->healthy.store(false);
                    logging::Logger::get_instance().log_warn("Backend " + backend->id + " marked UNHEALTHY");
                }
            }
        }
        
        // Sleep for interval, but check running_ flag periodically
        for (int i = 0; i < 10 && running_.load(); ++i) {
            std::this_thread::sleep_for(interval_ / 10);
        }
    }

    logging::Logger::get_instance().log_info("HealthChecker thread stopped");
}

}
