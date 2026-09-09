#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <memory>

namespace proxy::ratelimit {

class TokenBucket {
public:
    TokenBucket(double capacity, double refill_rate_per_sec);
    
    // Attempt to consume 1 token. Returns true if allowed, false if rate limited.
    bool consume();

private:
    double capacity_;
    double refill_rate_;
    double tokens_;
    std::chrono::steady_clock::time_point last_refill_;
    std::mutex mutex_;
};

class RateLimiter {
public:
    RateLimiter(double global_capacity, double global_refill, 
                double ip_capacity, double ip_refill);

    bool allow_request(const std::string& client_ip);

private:
    TokenBucket global_bucket_;
    
    double ip_capacity_;
    double ip_refill_;
    
    std::mutex map_mutex_;
    // Using pointers to avoid copying the mutex inside TokenBucket
    std::unordered_map<std::string, std::unique_ptr<TokenBucket>> ip_buckets_;
};

}
