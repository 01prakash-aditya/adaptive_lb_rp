#pragma once
#include <string>
#include <memory>
#include "../http/request.hpp"
#include "../http/response.hpp"
#include "../lb/backend.hpp"

namespace proxy::forwarder {

class Forwarder {
public:
    Forwarder() = default;
    
    Forwarder(const Forwarder&) = delete;
    Forwarder& operator=(const Forwarder&) = delete;

    // Forwards the request to the specified backend. Returns the HttpResponse.
    http::HttpResponse forward(http::HttpRequest req, std::shared_ptr<proxy::lb::Backend> backend, const std::string& client_ip = "");
};

}
