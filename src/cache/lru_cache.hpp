#pragma once

#include <string>
#include <unordered_map>
#include <list>
#include <mutex>
#include <chrono>
#include <optional>
#include "../http/response.hpp"

namespace proxy::cache {

struct CacheEntry {
    http::HttpResponse response;
    std::chrono::steady_clock::time_point expiry;
};

class LRUCache {
public:
    LRUCache(size_t max_size, std::chrono::seconds default_ttl);

    // Look up an item in the cache. Returns empty optional if miss or expired.
    std::optional<http::HttpResponse> get(const std::string& key);

    // Insert or update an item in the cache
    void put(const std::string& key, const http::HttpResponse& response);

    // Clear all entries
    void clear();

    // Get current size
    size_t size() const;

private:
    size_t max_size_;
    std::chrono::seconds default_ttl_;

    std::list<std::string> lru_list_; // Front is most recently used
    std::unordered_map<std::string, std::pair<CacheEntry, std::list<std::string>::iterator>> cache_map_;

    mutable std::mutex mutex_;

    // Internal helper, must be called with mutex_ held
    void evict_if_needed();
};

} // namespace proxy::cache
