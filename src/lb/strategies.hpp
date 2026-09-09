#pragma once
#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include "backend.hpp"

namespace proxy::lb {

class LoadBalancer {
protected:
    std::vector<std::shared_ptr<Backend>> backends_;

public:
    explicit LoadBalancer(std::vector<std::shared_ptr<Backend>> backends)
        : backends_(std::move(backends)) {}
    virtual ~LoadBalancer() = default;

    virtual std::shared_ptr<Backend> get_next(const std::string& client_ip) = 0;

    const std::vector<std::shared_ptr<Backend>>& get_backends() const {
        return backends_;
    }
};

class RoundRobinLB : public LoadBalancer {
private:
    std::atomic<size_t> counter_{0};
public:
    using LoadBalancer::LoadBalancer;
    std::shared_ptr<Backend> get_next(const std::string& client_ip) override;
};

class WeightedRoundRobinLB : public LoadBalancer {
private:
    std::atomic<size_t> counter_{0};
public:
    using LoadBalancer::LoadBalancer;
    std::shared_ptr<Backend> get_next(const std::string& client_ip) override;
};

class LeastConnectionsLB : public LoadBalancer {
public:
    using LoadBalancer::LoadBalancer;
    std::shared_ptr<Backend> get_next(const std::string& client_ip) override;
};

class IPHashLB : public LoadBalancer {
public:
    using LoadBalancer::LoadBalancer;
    std::shared_ptr<Backend> get_next(const std::string& client_ip) override;
};

}
