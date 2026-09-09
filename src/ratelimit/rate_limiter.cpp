#include "rate_limiter.hpp"

namespace proxy::ratelimit {

TokenBucket::TokenBucket(double capacity, double refill_rate_per_sec)
    : capacity_(capacity), refill_rate_(refill_rate_per_sec), tokens_(capacity) 
{
    last_refill_ = std::chrono::steady_clock::now();
}

bool TokenBucket::consume() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - last_refill_;
    
    // Refill tokens
    tokens_ += elapsed.count() * refill_rate_;
    if (tokens_ > capacity_) {
        tokens_ = capacity_;
    }
    last_refill_ = now;

    if (tokens_ >= 1.0) {
        tokens_ -= 1.0;
        return true;
    }
    
    return false;
}

RateLimiter::RateLimiter(double global_capacity, double global_refill, 
                         double ip_capacity, double ip_refill)
    : global_bucket_(global_capacity, global_refill),
      ip_capacity_(ip_capacity), ip_refill_(ip_refill) {}

bool RateLimiter::allow_request(const std::string& client_ip) {
    // 1. Check global limit first
    if (!global_bucket_.consume()) {
        return false;
    }

    // 2. Check per-IP limit
    if (client_ip.empty()) return true;

    TokenBucket* bucket = nullptr;
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        auto it = ip_buckets_.find(client_ip);
        if (it == ip_buckets_.end()) {
            auto new_bucket = std::make_unique<TokenBucket>(ip_capacity_, ip_refill_);
            bucket = new_bucket.get();
            ip_buckets_.emplace(client_ip, std::move(new_bucket));
        } else {
            bucket = it->second.get();
        }
    }

    return bucket->consume();
}

}
