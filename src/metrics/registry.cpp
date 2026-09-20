#include "registry.hpp"

namespace proxy::metrics {

std::string MetricsRegistry::generate_prometheus_metrics() const {
    std::ostringstream oss;
    
    oss << "# HELP proxy_requests_total Total number of HTTP requests processed\n";
    oss << "# TYPE proxy_requests_total counter\n";
    oss << "proxy_requests_total " << requests_total.load() << "\n\n";

    oss << "# HELP proxy_cache_hits_total Total number of cache hits\n";
    oss << "# TYPE proxy_cache_hits_total counter\n";
    oss << "proxy_cache_hits_total " << cache_hits_total.load() << "\n\n";

    oss << "# HELP proxy_cache_misses_total Total number of cache misses\n";
    oss << "# TYPE proxy_cache_misses_total counter\n";
    oss << "proxy_cache_misses_total " << cache_misses_total.load() << "\n\n";

    oss << "# HELP proxy_rate_limit_exceeded_total Total number of requests rejected due to rate limiting (429)\n";
    oss << "# TYPE proxy_rate_limit_exceeded_total counter\n";
    oss << "proxy_rate_limit_exceeded_total " << rate_limit_exceeded_total.load() << "\n\n";

    oss << "# HELP proxy_backend_errors_total Total number of backend connection errors or 5xx responses\n";
    oss << "# TYPE proxy_backend_errors_total counter\n";
    oss << "proxy_backend_errors_total " << backend_errors_total.load() << "\n\n";

    oss << "# HELP proxy_active_connections Current number of active client connections\n";
    oss << "# TYPE proxy_active_connections gauge\n";
    oss << "proxy_active_connections " << active_connections.load() << "\n";

    return oss.str();
}

} // namespace proxy::metrics
