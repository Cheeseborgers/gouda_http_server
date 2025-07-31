//
// Created by fason on 26/07/25.
//

#ifndef SOCKET_WRAPPER_H
#define SOCKET_WRAPPER_H

#include <cstring>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/socket.h>

#include "http_structs.hpp"
#include "logger.hpp"
#include "types.hpp"

struct HostDetails {
    std::string host;
    u16 port;

    [[nodiscard]] std::string to_string() const {
        return host + ":" + std::to_string(port);
    }
};

// RAII wrapper for socket descriptor
class Socket {
public:
    enum class Type : u8 { Client, Server };

    explicit Socket(const int fd, const Type type) : m_fd{fd}, m_type{type}
    {
        if (m_fd == -1) {
            throw std::runtime_error(std::strerror(errno));
        }
    }

    ~Socket()
    {
        if (m_fd != -1)
            ::close(m_fd);
    }

    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    Socket(Socket &&other) noexcept : m_fd{other.m_fd}, m_type{other.m_type} { other.m_fd = -1; }

    Socket &operator=(Socket &&other) noexcept
    {
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

    [[nodiscard]] bool bind(const addrinfo *p) const {
        if (m_type == Type::Client) {
            return false;
        }

        return ::bind(m_fd, p->ai_addr, p->ai_addrlen) != -1;
    }

    [[nodiscard]] bool set_reuse() const {
        if (m_type == Type::Client) {
            return false;
        }

        constexpr int yes{1};
        return setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) != -1;
    }

    [[nodiscard]] bool set_recv_timeout(const std::chrono::seconds seconds) const {
        if (m_type == Type::Server) {
            return false;
        }

        const timeval recv_timeout{.tv_sec = seconds.count(), .tv_usec = 0};
        return setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) == 0;
    }

    [[nodiscard]] bool set_send_timeout(const std::chrono::seconds seconds) const {
        if (m_type == Type::Server) {
            return false;
        }

        const timeval send_timeout{.tv_sec = seconds.count(), .tv_usec = 0};
        return setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout)) == 0;
    }


    [[nodiscard]] bool listen(const int backlog) const {
        if (m_type == Type::Client) {
            return false;
        }

        return ::listen(m_fd, backlog) != -1;
    }

    [[nodiscard]] ssize_t send(const char* data, const size_t length) const {
        return ::send(m_fd, data, length, 0);
    }

    [[nodiscard]] bool is_valid() const {
        return m_fd != -1;
    }

    [[nodiscard]] int get() const { return m_fd; }
    [[nodiscard]] Type get_type() const { return m_type; }

private:
    int m_fd;
    Type m_type;
};

#endif // SOCKET_WRAPPER_H
