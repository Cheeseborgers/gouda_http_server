#ifndef SOCKET_WRAPPER_H
#define SOCKET_WRAPPER_H

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

using Port = uint16_t;

struct HostDetails {
    std::string host;
    Port port;

    [[nodiscard]] std::string to_string() const { return host + ":" + std::to_string(port); }
};

class Socket {
public:
    enum class Type : uint8_t { Client, Server };

    explicit Socket(const int fd, const Type type) : m_fd{fd}, m_type{type} {
        if (m_fd == -1) {
            throw std::runtime_error(std::strerror(errno));
        }
    }

    ~Socket() {
        if (m_fd != -1) {
            ::close(m_fd);
        }
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : m_fd{other.m_fd}, m_type{other.m_type} {
        other.m_fd = -1;
    }

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (m_fd != -1) {
                ::close(m_fd);
            }
            m_fd = other.m_fd;
            other.m_fd = -1;
            m_type = other.m_type;
        }
        return *this;
    }

    [[nodiscard]] bool bind(const addrinfo* p) const {
        return m_type == Type::Server && ::bind(m_fd, p->ai_addr, p->ai_addrlen) != -1;
    }

    [[nodiscard]] bool connect(const addrinfo* p) const {
        return m_type == Type::Client && ::connect(m_fd, p->ai_addr, p->ai_addrlen) != -1;
    }

    [[nodiscard]] bool listen(const int backlog = SOMAXCONN) const {
        return m_type == Type::Server && ::listen(m_fd, backlog) != -1;
    }

    void shutdown_read() const { ::shutdown(m_fd, SHUT_RD); }
    void shutdown_write() const { ::shutdown(m_fd, SHUT_WR); }

    [[nodiscard]] ssize_t send(const char* data, const size_t length) const {
        return ::send(m_fd, data, length, 0);
    }

    [[nodiscard]] ssize_t send(const std::string& msg) const {
        return send(msg.c_str(), msg.size());
    }

    [[nodiscard]] ssize_t send(const std::string_view msg) const {
        return send(msg.data(), msg.size());
    }

    [[nodiscard]] ssize_t recv(char* buffer, const size_t length) const {
        return ::recv(m_fd, buffer, length, 0);
    }

    [[nodiscard]] bool set_recv_timeout(const std::chrono::seconds seconds) const {
        const timeval tv{.tv_sec = seconds.count(), .tv_usec = 0};
        return ::setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != -1;
    }

    [[nodiscard]] bool set_send_timeout(const std::chrono::seconds seconds) const {
        const timeval tv{.tv_sec = seconds.count(), .tv_usec = 0};
        return ::setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != -1;
    }

    [[nodiscard]] bool set_reuse() const {
        constexpr int yes = 1;
        return ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != -1;
    }

    [[nodiscard]] bool set_non_blocking(const bool enable) const {
        const int flags = fcntl(m_fd, F_GETFL, 0);
        if (flags == -1)
            return false;
        return fcntl(m_fd, F_SETFL, enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) != -1;
    }

    [[nodiscard]] bool is_valid() const { return m_fd != -1; }
    [[nodiscard]] int get() const { return m_fd; }
    [[nodiscard]] Type get_type() const { return m_type; }

    [[nodiscard]] std::optional<std::string> get_peer_address() const {
        sockaddr_storage addr{};
        socklen_t len = sizeof(addr);
        if (::getpeername(m_fd, reinterpret_cast<sockaddr*>(&addr), &len) == -1)
            return std::nullopt;

        char host[NI_MAXHOST], port[NI_MAXSERV];
        if (::getnameinfo(reinterpret_cast<sockaddr*>(&addr), len, host, sizeof(host), port, sizeof(port),
                          NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
            return std::nullopt;
        }

        return std::string{host} + ":" + port;
    }

    friend std::ostream& operator<<(std::ostream& os, const Socket& s) {
        os << "Socket(fd=" << s.m_fd << ", type=" << (s.m_type == Type::Client ? "Client" : "Server") << ")";
        return os;
    }

private:
    int m_fd;
    Type m_type;
};

struct AcceptedSocket {
    Socket socket;
    sockaddr_storage addr;

    [[nodiscard]] std::string to_string() const {
        char ip_str[INET6_ADDRSTRLEN] = {};

        if (addr.ss_family == AF_INET) {
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(&addr);
            inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, sizeof(ip_str));
            return std::format("{}:{}", ip_str, ntohs(ipv4->sin_port));
        }

        if (addr.ss_family == AF_INET6) {
            const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(&addr);
            inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip_str, sizeof(ip_str));
            return std::format("[{}]:{}", ip_str, ntohs(ipv6->sin6_port));
        }

        return "<unknown>";
    }
};

class SocketFactory {
public:
    static std::expected<Socket, std::string> make_server_socket(const Port port, const int backlog = SOMAXCONN) {
        addrinfo hints{}, *res;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        const std::string port_str = std::to_string(port);
        if (const int gai_result = getaddrinfo(nullptr, port_str.c_str(), &hints, &res); gai_result != 0) {
            return std::unexpected(std::format("getaddrinfo: {}", gai_strerror(gai_result)));
        }

        for (const addrinfo* p = res; p != nullptr; p = p->ai_next) {
            const int fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (fd == -1) continue;

            Socket sock(fd, Socket::Type::Server);
            if (!sock.set_reuse()) {
                freeaddrinfo(res);
                return std::unexpected(std::format("setsockopt SO_REUSEADDR: {}", std::strerror(errno)));
            }

            if (sock.bind(p) && sock.listen(backlog)) {
                freeaddrinfo(res);
                return sock;
            }

            ::close(fd);
        }

        freeaddrinfo(res);
        return std::unexpected("server socket setup failed: no valid address found");
    }

    static std::expected<Socket, std::string> make_client_socket(const std::string_view host, const Port port) {
        addrinfo hints{}, *res;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        const std::string port_str = std::to_string(port);
        if (const int gai_result = getaddrinfo(host.data(), port_str.c_str(), &hints, &res); gai_result != 0) {
            return std::unexpected(std::format("getaddrinfo: {}", gai_strerror(gai_result)));
        }

        for (const addrinfo* p = res; p != nullptr; p = p->ai_next) {
            const int fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (fd == -1) continue;

            if (Socket sock(fd, Socket::Type::Client); sock.connect(p)) {
                freeaddrinfo(res);
                return sock;
            }

            ::close(fd);
        }

        freeaddrinfo(res);
        return std::unexpected("client: failed to connect to any resolved address");
    }
};

inline std::expected<AcceptedSocket, std::string> accept_socket(const Socket& server_socket) {
    if (server_socket.get_type() != Socket::Type::Server) {
        return std::unexpected("accept() called on non-server socket");
    }

    sockaddr_storage addr{};
    socklen_t addr_len = sizeof(addr);

    const int client_fd = ::accept(server_socket.get(), reinterpret_cast<sockaddr*>(&addr), &addr_len);
    if (client_fd == -1) {
        return std::unexpected(std::format("accept: {}", std::strerror(errno)));
    }

    return AcceptedSocket{Socket(client_fd, Socket::Type::Client), addr};
}

#endif // SOCKET_WRAPPER_H
