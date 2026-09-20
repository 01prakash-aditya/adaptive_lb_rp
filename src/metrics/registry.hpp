#pragma once

#include <atomic>
#include <string>
#include <sstream>

namespace proxy::metrics {

class MetricsRegistry {
public:
    // Singleton access
    static MetricsRegistry& get_instance() {
        static MetricsRegistry instance;
        return instance;
    }

    // Counters
    std::atomic<uint64_t> requests_total{0};
    std::atomic<uint64_t> cache_hits_total{0};
    std::atomic<uint64_t> cache_misses_total{0};
    std::atomic<uint64_t> rate_limit_exceeded_total{0};
    std::atomic<uint64_t> backend_errors_total{0};

    // Gauges
    std::atomic<int64_t> active_connections{0};

    // Helper to generate Prometheus-formatted payload
    std::string generate_prometheus_metrics() const;

private:
    MetricsRegistry() = default;
    ~MetricsRegistry() = default;
    
    // Non-copyable
    MetricsRegistry(const MetricsRegistry&) = delete;
    MetricsRegistry& operator=(const MetricsRegistry&) = delete;
};

} // namespace proxy::metrics
