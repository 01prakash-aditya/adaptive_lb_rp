#pragma once
#include <string>
#include "../http/request.hpp"
#include "../http/response.hpp"

namespace proxy::forwarder {

class Forwarder {
public:
    Forwarder(std::string backend_host, uint16_t backend_port);

    Forwarder(const Forwarder&) = delete;
    Forwarder& operator=(const Forwarder&) = delete;

    http::HttpResponse forward(http::HttpRequest req, const std::string& client_ip = "");

private:
    std::string backend_host_;
    uint16_t backend_port_;
};

}
