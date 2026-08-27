#include "server/server.hpp"
#include "logging/logger.hpp"
#include <iostream>
#include <string>
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
    std::string backend = "127.0.0.1:9001";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--backend" && i + 1 < argc) {
            backend = argv[++i];
        }
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    proxy::logging::Logger::get_instance().log_info("Starting proxy server");
    proxy::logging::Logger::get_instance().log_info("Config - Port: " + std::to_string(port) + ", Backend: " + backend);

    proxy::server::Server server(port, backend);
    g_server = &server;
    
    server.run();

    return 0;
}
