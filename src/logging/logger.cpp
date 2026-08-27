#include "logger.hpp"
#include <iomanip>
#include <sstream>

namespace proxy::logging {

std::string Logger::current_time_iso() const {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

void Logger::log_request(const std::string& request_id, const std::string& method, const std::string& uri, 
                         int status_code, double latency_ms, const std::string& backend_addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "{\"timestamp\":\"" << current_time_iso()
        << "\",\"level\":\"INFO\""
        << ",\"request_id\":\"" << request_id
        << "\",\"method\":\"" << method
        << "\",\"uri\":\"" << uri
        << "\",\"status\":" << status_code
        << ",\"latency_ms\":" << latency_ms
        << ",\"backend\":\"" << backend_addr
        << "\"}";
    std::cout << oss.str() << "\n";
}

void Logger::log_info(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "{\"timestamp\":\"" << current_time_iso()
              << "\",\"level\":\"INFO\",\"message\":\"" << message << "\"}\n";
}

void Logger::log_warn(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "{\"timestamp\":\"" << current_time_iso()
              << "\",\"level\":\"WARN\",\"message\":\"" << message << "\"}\n";
}

void Logger::log_error(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << "{\"timestamp\":\"" << current_time_iso()
              << "\",\"level\":\"ERROR\",\"message\":\"" << message << "\"}\n";
}

}
