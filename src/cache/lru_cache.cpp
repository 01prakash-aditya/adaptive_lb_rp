#include "lru_cache.hpp"

namespace proxy::cache {

LRUCache::LRUCache(size_t max_size, std::chrono::seconds default_ttl)
    : max_size_(max_size > 0 ? max_size : 1), default_ttl_(default_ttl) {}

std::optional<http::HttpResponse> LRUCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_map_.find(key);
    
    if (it == cache_map_.end()) {
        return std::nullopt; // Cache miss
    }

    // Check expiry
    auto now = std::chrono::steady_clock::now();
    if (now > it->second.first.expiry) {
        // Expired, remove from cache
        lru_list_.erase(it->second.second);
        cache_map_.erase(it);
        return std::nullopt;
    }

    // Hit: move to front of LRU list
    lru_list_.erase(it->second.second);
    lru_list_.push_front(key);
    it->second.second = lru_list_.begin();

    return it->second.first.response;
}

void LRUCache::put(const std::string& key, const http::HttpResponse& response) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_map_.find(key);
    auto expiry = std::chrono::steady_clock::now() + default_ttl_;

    if (it != cache_map_.end()) {
        // Update existing entry
        lru_list_.erase(it->second.second);
        lru_list_.push_front(key);
        it->second.first.response = response;
        it->second.first.expiry = expiry;
        it->second.second = lru_list_.begin();
    } else {
        // Insert new entry
        evict_if_needed();
        lru_list_.push_front(key);
        cache_map_[key] = { {response, expiry}, lru_list_.begin() };
    }
}

void LRUCache::evict_if_needed() {
    if (cache_map_.size() >= max_size_) {
        // Evict least recently used (back of the list)
        auto last = lru_list_.back();
        lru_list_.pop_back();
        cache_map_.erase(last);
    }
}

void LRUCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    lru_list_.clear();
    cache_map_.clear();
}

size_t LRUCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_map_.size();
}

} // namespace proxy::cache
