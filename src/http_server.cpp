//
// Created by fason on 26/07/25.
//

#include "http_server.h"

#include <iostream>
#include <poll.h>
#include <print>
#include <utility>

// Define static member
Server *Server::s_instance = nullptr;

Server::Server(HostDetails host_details, const int backlog, const size_t thread_pool_size, const int poll_timeout_ms)
    : m_running{false},
      m_host_details{std::move(host_details)},
      m_backlog{backlog},
      m_poll_timeout_ms{poll_timeout_ms},
      m_thread_pool{thread_pool_size}
{
    if (auto result = setup(); !result) {
        throw std::runtime_error("Server setup failed: " + result.error());
    }

    setup_signal_handler();

    LOG_INFO(std::format("Server started on {}:{}", m_host_details.host, m_host_details.port));
}

void Server::run()
{
    m_running = true;
    LOG_INFO(std::format("Server: waiting for connections..."));
    while (m_running) {
        accept_and_handle();
    }

    LOG_INFO("Server shutting down");
    m_sock.reset(); // Close server socket
    m_thread_pool.stop();
}

// Private ----------------
void Server::setup_signal_handler()
{
    LOG_INFO("Registering signal handlers");
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    s_instance = this;
}

std::expected<void, std::string> Server::setup()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigprocmask(SIG_UNBLOCK, &set, nullptr);

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    addrinfo *servinfo{nullptr};
    const std::string port_str = std::to_string(m_host_details.port);
    if (const int rv = getaddrinfo(nullptr, port_str.c_str(), &hints, &servinfo); rv != 0) {
        return std::unexpected(std::format("getaddrinfo: {}", gai_strerror(rv)));
    }

    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> servinfo_ptr(servinfo, freeaddrinfo);

    for (const addrinfo *p = servinfo; p != nullptr; p = p->ai_next) {
        m_sock = Socket(socket(p->ai_family, p->ai_socktype, p->ai_protocol), Socket::Type::Server);
        if (!m_sock->is_valid()) {
            continue; // try next address
        }
        if (!m_sock->set_reuse()) {
            return std::unexpected(std::format("setsockopt: {}", std::strerror(errno)));
        }
        if (!m_sock->bind(p)) {
            continue; // try next address
        }
        break;
    }

    if (!m_sock.has_value()) {
        return std::unexpected("server: failed to bind");
    }

    if (!m_sock->listen(m_backlog)) {
        return std::unexpected(std::format("listen: {}", std::strerror(errno)));
    }

    return {}; // success
}

void Server::accept_and_handle()
{
    if (!m_sock.has_value()) {
        LOG_ERROR("Error: Server socket not initialized");
        return;
    }

    // Use poll to avoid blocking indefinitely
    pollfd pfd{m_sock->get(), POLLIN, 0};
    const int poll_result{poll(&pfd, 1, m_poll_timeout_ms)};
    if (poll_result < 0) {
        LOG_ERROR(std::format("poll: {}", std::strerror(errno)));
        return;
    }
    if (poll_result == 0 || !(pfd.revents & POLLIN)) {
        return; // Timeout or no events
    }

    sockaddr_storage their_addr{};
    socklen_t sin_size{sizeof their_addr};
    Socket client_sock(accept(m_sock->get(), reinterpret_cast<sockaddr *>(&their_addr), &sin_size),
                       Socket::Type::Client);
    if (!client_sock.is_valid()) {
        LOG_ERROR(std::format("accept: {}", std::strerror(errno)));
        return;
    }

    char s[INET6_ADDRSTRLEN];
    inet_ntop(their_addr.ss_family, get_in_addr(reinterpret_cast<sockaddr *>(&their_addr)), s, sizeof s);

    LOG_INFO(std::format("server: got connection from {}", s));

    m_thread_pool.enqueue([handler = ClientHandler(std::move(client_sock))]() mutable { handler.handle(); });
}
