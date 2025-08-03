#ifndef HTTPSERVER_H
#define HTTPSERVER_H
#include <expected>
#include <arpa/inet.h>
#include <memory>
#include <netdb.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include "client_handler.h"
#include "socket_wrapper.h"
#include "thread_pool.hpp"
#include "logger.hpp"

class Server {
public:
    Server(HostDetails host_details, int backlog, size_t thread_pool_size, int poll_timeout_ms = 100);

    void run();

private:
    void setup_signal_handler();
    [[nodiscard]] std::expected<void, std::string> setup();
    void accept_and_handle();

    static void *get_in_addr(sockaddr *sa) {
        if (sa->sa_family == AF_INET) {
            return &reinterpret_cast<sockaddr_in *>(sa)->sin_addr;
        }
        return &reinterpret_cast<sockaddr_in6 *>(sa)->sin6_addr;
    }

    static void signal_handler(const int sig) {
        if (s_instance) {
            s_instance->m_running = false;
            write(STDERR_FILENO, "Signal received\n", sig);
        }
    }

private:
    std::atomic<bool> m_running;
    HostDetails m_host_details;
    std::optional<Socket> m_sock;
    int m_backlog;
    int m_poll_timeout_ms;
    ThreadPool m_thread_pool;
    static Server* s_instance;
};

#endif // HTTPSERVER_H