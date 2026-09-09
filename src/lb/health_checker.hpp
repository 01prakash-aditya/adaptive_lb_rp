#pragma once
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include "backend.hpp"

namespace proxy::lb {

class HealthChecker {
public:
    HealthChecker(std::vector<std::shared_ptr<Backend>> backends, 
                  std::chrono::milliseconds interval = std::chrono::seconds(5),
                  int max_failures = 3);
    ~HealthChecker();

    void start();
    void stop();

private:
    std::vector<std::shared_ptr<Backend>> backends_;
    std::chrono::milliseconds interval_;
    int max_failures_;
    
    std::atomic<bool> running_{false};
    std::thread check_thread_;

    void run_checks();
    bool ping_backend(const std::shared_ptr<Backend>& backend);
};

}
