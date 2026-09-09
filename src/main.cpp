#include "server/server.hpp"
#include "logging/logger.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <csignal>

proxy::server::Server* g_server = nullptr;

void signal_handler(int signal) {
    proxy::logging::Logger::get_instance().log_info("Received signal " + std::to_string(signal) + ", shutting down...");
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    std::string backends_str = "127.0.0.1:9001:1";
    std::string strategy = "rr";
    double rate_limit = 1000.0; // Default high limit

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--backends" && i + 1 < argc) {
            backends_str = argv[++i];
        } else if (arg == "--strategy" && i + 1 < argc) {
            strategy = argv[++i];
        } else if (arg == "--ratelimit" && i + 1 < argc) {
            rate_limit = std::stod(argv[++i]);
        }
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Parse backends (format: host:port:weight,host:port:weight)
    std::vector<std::shared_ptr<proxy::lb::Backend>> backends;
    std::stringstream ss(backends_str);
    std::string token;
    int b_idx = 1;
    while (std::getline(ss, token, ',')) {
        size_t c1 = token.find(':');
        size_t c2 = token.rfind(':');
        
        std::string host = token;
        uint16_t bport = 80;
        int weight = 1;
        
        if (c1 != std::string::npos) {
            host = token.substr(0, c1);
            if (c1 == c2) {
                bport = std::stoi(token.substr(c1 + 1));
            } else {
                bport = std::stoi(token.substr(c1 + 1, c2 - c1 - 1));
                weight = std::stoi(token.substr(c2 + 1));
            }
        }
        
        std::string b_id = "backend-" + std::to_string(b_idx++);
        backends.push_back(std::make_shared<proxy::lb::Backend>(b_id, host, bport, weight));
    }

    proxy::logging::Logger::get_instance().log_info("Starting proxy server");
    proxy::logging::Logger::get_instance().log_info(
        "Config - Port: " + std::to_string(port) + 
        ", Strategy: " + strategy + 
        ", Rate Limit: " + std::to_string(rate_limit) + " req/sec");

    for (const auto& b : backends) {
        proxy::logging::Logger::get_instance().log_info(
            "Registered Backend: " + b->id + " -> " + b->host + ":" + std::to_string(b->port) + " (weight: " + std::to_string(b->weight) + ")"
        );
    }

    proxy::server::Server server(port, backends, strategy, rate_limit);
    g_server = &server;
    
    server.run();

    return 0;
}
