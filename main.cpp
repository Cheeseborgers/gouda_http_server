#include <iostream>
#include <stdexcept>

#include "routes.hpp"
#include "http_server.hpp"

constexpr HostDetails DETAILS{LOCALHOST.data(), 8080};
constexpr uint32_t BACKLOG{10};
const size_t THREAD_POOL_SIZE = std::max(4u, 2 * std::thread::hardware_concurrency());
constexpr int POLL_INTERVAL{DEFAULT_POLL_INTERVAL};

int main() {
    try {
        Logger::instance().set_min_level(Logger::Level::DEBUG);
        setup_routes();
        Server server(DETAILS, BACKLOG, THREAD_POOL_SIZE, POLL_INTERVAL);
        server.run();
    } catch (const std::runtime_error& e) {
        LOG_ERROR(std::format("Server Error:\n{}", e.what()));
        return 1;
    }
}