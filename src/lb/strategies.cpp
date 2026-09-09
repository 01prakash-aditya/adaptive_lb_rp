#include "strategies.hpp"
#include <functional>
#include <limits>

namespace proxy::lb {

// Utility to find the first healthy backend if the target is unhealthy,
// or just return the target if it's healthy.
// In a full implementation, you'd want to loop to find the *next* healthy one.
static std::shared_ptr<Backend> get_healthy_fallback(
    const std::vector<std::shared_ptr<Backend>>& backends, 
    std::shared_ptr<Backend> target,
    size_t start_idx) 
{
    if (target->healthy.load()) return target;

    for (size_t i = 0; i < backends.size(); ++i) {
        size_t idx = (start_idx + i) % backends.size();
        if (backends[idx]->healthy.load()) {
            return backends[idx];
        }
    }
    return target; // All unhealthy, return target and let it fail
}


std::shared_ptr<Backend> RoundRobinLB::get_next(const std::string&) {
    if (backends_.empty()) return nullptr;
    size_t idx = counter_.fetch_add(1) % backends_.size();
    return get_healthy_fallback(backends_, backends_[idx], idx);
}

std::shared_ptr<Backend> WeightedRoundRobinLB::get_next(const std::string&) {
    if (backends_.empty()) return nullptr;
    
    // Very simple WRR: flatten the pool based on weights.
    // Real WRR is stateful per-backend, but for simplicity we compute on the fly or 
    // we could just do a simple modulus.
    // Let's do a simple modulus over total weight.
    int total_weight = 0;
    for (const auto& b : backends_) total_weight += b->weight;
    
    if (total_weight == 0) return backends_[0];

    size_t count = counter_.fetch_add(1);
    int target_weight = count % total_weight;
    
    size_t idx = 0;
    for (size_t i = 0; i < backends_.size(); ++i) {
        target_weight -= backends_[i]->weight;
        if (target_weight < 0) {
            idx = i;
            break;
        }
    }
    
    return get_healthy_fallback(backends_, backends_[idx], idx);
}

std::shared_ptr<Backend> LeastConnectionsLB::get_next(const std::string&) {
    if (backends_.empty()) return nullptr;
    
    std::shared_ptr<Backend> best = nullptr;
    int min_conns = std::numeric_limits<int>::max();
    size_t best_idx = 0;

    for (size_t i = 0; i < backends_.size(); ++i) {
        if (!backends_[i]->healthy.load()) continue;
        int conns = backends_[i]->active_connections.load();
        if (conns < min_conns) {
            min_conns = conns;
            best = backends_[i];
            best_idx = i;
        }
    }
    
    if (!best) return backends_[0]; // All unhealthy fallback
    return best;
}

std::shared_ptr<Backend> IPHashLB::get_next(const std::string& client_ip) {
    if (backends_.empty()) return nullptr;
    
    size_t hash = std::hash<std::string>{}(client_ip);
    size_t idx = hash % backends_.size();
    
    return get_healthy_fallback(backends_, backends_[idx], idx);
}

}
