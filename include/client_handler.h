#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <chrono>
#include <optional>
#include <string>

#include "http_structs.hpp"
#include "logger.hpp"
#include "socket_wrapper.h"

struct ClientHandlerConfig {
    std::chrono::seconds recv_timeout = std::chrono::seconds(DEFAULT_RECV_TIMEOUT);
    std::chrono::seconds send_timeout = std::chrono::seconds(DEFAULT_SEND_TIMEOUT);
    int max_requests = DEFAULT_MAX_REQUESTS;
    size_t max_header_size = DEFAULT_MAX_HEADER_SIZE;           // 8KB max for headers
    size_t max_content_length = DEFAULT_MAX_CONTENT_LENGTH;     // 1MB max for body
    bool debug = true;                                         // Enable verbose logging
    size_t stream_buffer_size = DEFAULT_MAX_STREAM_BUFFER_SIZE; // 64KB max for stream buffer
};

class ClientHandler {
public:
    explicit ClientHandler(Socket sock, const ClientHandlerConfig &config = ClientHandlerConfig{});

    // Allow move construction
    ClientHandler(ClientHandler&&) = default;
    ClientHandler& operator=(ClientHandler&&) = default;

    // Delete copy
    ClientHandler(const ClientHandler&) = delete;
    ClientHandler& operator=(const ClientHandler&) = delete;

    void handle() const;

private:
    void set_socket_timeouts() const;
    [[nodiscard]] bool should_keep_alive(const HttpRequest &request) const;
    std::optional<std::string> read_headers(std::string &buffer, RequestId request_id) const;
    [[nodiscard]] std::optional<size_t> get_content_length(const std::string &headers, RequestId request_id) const;
    bool read_body(std::string &buffer, size_t content_length, size_t header_end, RequestId request_id) const;
    [[nodiscard]] std::optional<std::string> read_requests(RequestId request_id) const;
    void send_raw(const HttpResponse &response, RequestId request_id) const;
    void send_error_response(HttpStatusCode code, std::string_view body, std::string_view content_type,
                                        RequestId request_id) const;
    [[nodiscard]] std::optional<bool> process_single_request() const;

private:
    Socket m_sock;
    ClientHandlerConfig m_config;
    HostDetails m_host_details;
};

#endif // CLIENT_HANDLER_H