#pragma once
#include <string>
#include <mutex>
#include <chrono>
#include <iostream>

namespace proxy::logging {

class Logger {
public:
    static Logger& get_instance() {
        static Logger instance;
        return instance;
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log_request(const std::string& request_id, const std::string& method, const std::string& uri, 
                     int status_code, double latency_ms, const std::string& backend_addr);
    void log_info(const std::string& message);
    void log_warn(const std::string& message);
    void log_error(const std::string& message);

private:
    Logger() = default;
    std::mutex mutex_;

    std::string current_time_iso() const;
};

}
