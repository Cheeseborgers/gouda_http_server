#include "client_handler.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <random>
#include <string_view>
#include <sys/socket.h>
#include "http_request_parser.h"
#include "http_response_builder.h"
#include "router.hpp"
#include "types.hpp"
#include <nlohmann/json.hpp>

ClientHandler::ClientHandler(Socket sock, const ClientHandlerConfig &config) : m_sock(std::move(sock)), m_config(config) {
    sockaddr_in client_addr{};
    socklen_t addr_len{sizeof(client_addr)};
    if (getpeername(m_sock.get(), reinterpret_cast<sockaddr *>(&client_addr), &addr_len) == 0) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        m_host_details.host = ip_str;
        m_host_details.port = client_addr.sin_port;
    } else {
        m_host_details.host = "unknown";
        m_host_details.port = 0;
        LOG_ERROR(std::format("Client[FD:{}]: Failed to get client info: {}", m_sock.get(), std::strerror(errno)));
    }
    LOG_INFO(std::format("Client[FD:{}][{}]: ClientHandler created", m_sock.get(), m_host_details.to_string()));
    set_socket_timeouts();
}

void ClientHandler::handle() const {
    LOG_INFO(std::format("Client[FD:{}][{}]: Handling client connection", m_sock.get(), m_host_details.to_string()));
    int handled_requests = 0;
    while (handled_requests < m_config.max_requests) {
        auto result = process_single_request();
        if (!result.has_value()) {
            LOG_INFO(std::format("Client[FD:{}][{}]: Request processing failed, closing connection", m_sock.get(),
                                 m_host_details.to_string()));
            break;
        }
        if (!result.value()) {
            LOG_INFO(std::format("Client[FD:{}][{}]: Connection closed per request", m_sock.get(),
                                 m_host_details.to_string()));
            break;
        }
        handled_requests++;
    }
}

void ClientHandler::set_socket_timeouts() const {
    if (!m_sock.set_recv_timeout(m_config.recv_timeout)) {
        LOG_ERROR(std::format("Client[FD:{}][{}]: Failed to set recv timeout: {}", m_sock.get(),
                              m_host_details.to_string(), std::strerror(errno)));
    }
    if (!m_sock.set_send_timeout(m_config.send_timeout)) {
        LOG_ERROR(std::format("Client[FD:{}][{}]: Failed to set send timeout: {}", m_sock.get(),
                              m_host_details.to_string(), std::strerror(errno)));
    }
}

[[nodiscard]] bool ClientHandler::should_keep_alive(const HttpRequest &request) const {
    const auto it = request.headers.find("Connection");
    if (it != request.headers.end()) {
        return std::string_view(it->second) == "keep-alive";
    }
    return request.version == HttpVersion::HTTP_1_1;
}

std::optional<std::string> ClientHandler::read_headers(std::string &buffer, RequestId request_id) const {
    char temp[REQUEST_HEADERS_BUFFER_SIZE];
    size_t header_end = std::string::npos;
    while (buffer.size() < m_config.max_header_size) {
        ssize_t bytes_received = recv(m_sock.get(), temp, sizeof(temp), 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                LOG_INFO(std::format("Client[FD:{}][{}]: Request[{}]: Connection closed by client", m_sock.get(),
                                     m_host_details.to_string(), request_id));
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_WARNING(std::format("Client[FD:{}][{}]: Request[{}]: recv timeout", m_sock.get(),
                                        m_host_details.to_string(), request_id));
            } else {
                LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: recv error: {}", m_sock.get(),
                                      m_host_details.to_string(), request_id, std::strerror(errno)));
            }
            return std::nullopt;
        }
        buffer.append(temp, bytes_received);
        if (m_config.debug) {
            std::string_view chunk(temp, bytes_received);
            LOG_DEBUG(std::format("Client[FD:{}][{}]: Request[{}]: Received chunk ({} bytes): [{}]", m_sock.get(),
                                  m_host_details.to_string(), request_id, bytes_received, escape_string(chunk)));
            LOG_DEBUG(std::format("Client[FD:{}][{}]: Request[{}]: Hex: {}", m_sock.get(), m_host_details.to_string(),
                                  request_id, to_hex(chunk)));
        }
        header_end = buffer.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            header_end += 4;
            return buffer.substr(0, header_end);
        }
        const size_t alt_end = buffer.find("\n\n");
        if (alt_end != std::string::npos) {
            header_end = alt_end + 2;
            std::string normalized = buffer.substr(0, alt_end) + "\r\n\r\n" + buffer.substr(alt_end + 2);
            buffer = std::move(normalized);
            return buffer.substr(0, header_end);
        }
    }
    LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Headers too large", m_sock.get(), m_host_details.to_string(),
                          request_id));
    return std::nullopt;
}

[[nodiscard]] std::optional<size_t> ClientHandler::get_content_length(const std::string &headers, RequestId request_id) const {
    size_t count = 0;
    size_t pos = 0;
    size_t last_value_start = std::string::npos;
    while ((pos = headers.find("Content-Length:", pos)) != std::string::npos) {
        count++;
        pos += sizeof("Content-Length:") - 1;
        const size_t value_start = headers.find_first_of("0123456789", pos);
        if (value_start != std::string::npos) {
            last_value_start = value_start;
        }
    }
    if (count > 1) {
        LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Multiple Content-Length headers", m_sock.get(),
                              m_host_details.to_string(), request_id));
        return std::nullopt;
    }
    if (count == 0 || last_value_start == std::string::npos) {
        return 0;
    }
    try {
        std::string value = headers.substr(last_value_start);
        const size_t value_end = value.find_first_not_of("0123456789");
        if (value_end != std::string::npos) {
            value = value.substr(0, value_end);
        }
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);
        if (value.empty()) {
            LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Empty Content-Length value", m_sock.get(),
                                  m_host_details.to_string(), request_id));
            return std::nullopt;
        }
        size_t length = std::stoul(value);
        if (length > m_config.max_content_length) {
            LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Content-Length too large: {}", m_sock.get(),
                                  m_host_details.to_string(), request_id, length));
            return std::nullopt;
        }
        return length;
    } catch (const std::exception &e) {
        LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Invalid Content-Length value: {}", m_sock.get(),
                              m_host_details.to_string(), request_id, e.what()));
        return std::nullopt;
    }
}

bool ClientHandler::read_body(std::string &buffer, size_t content_length, const size_t header_end, RequestId request_id) const {
    size_t current_body_size = buffer.size() - header_end;
    char temp[REQUEST_BODY_BUFFER_SIZE];
    while (current_body_size < content_length && buffer.size() < m_config.max_content_length + header_end) {
        const ssize_t bytes_received = recv(m_sock.get(), temp, sizeof(temp), 0);
        if (bytes_received <= 0) {
            LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: recv error or disconnect during body read",
                                  m_sock.get(), m_host_details.to_string(), request_id));
            return false;
        }
        buffer.append(temp, bytes_received);
        current_body_size += bytes_received;
        if (m_config.debug) {
            LOG_DEBUG(std::format("Client[FD:{}][{}]: Request[{}]: Reading body... ({} / {})", m_sock.get(),
                                  m_host_details.to_string(), request_id, current_body_size, content_length));
        }
    }
    if (current_body_size < content_length) {
        LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Body incomplete (expected {})", m_sock.get(),
                              m_host_details.to_string(), request_id, content_length));
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<std::string> ClientHandler::read_requests(RequestId request_id) const {
    std::string buffer;
    buffer.reserve(REQUEST_BUFFER_SIZE);

    const auto headers = read_headers(buffer, request_id);
    if (!headers) {
        return std::nullopt;
    }

    const size_t header_end = headers->size();
    const auto content_length = get_content_length(*headers, request_id);
    if (!content_length) return std::nullopt;
    if (*content_length > 0) {
        if (!read_body(buffer, *content_length, header_end, request_id)) {
            return std::nullopt;
        }
    }
    LOG_INFO(std::format("Client[FD:{}][{}]: Request[{}]: Received ({} bytes)", m_sock.get(),
                         m_host_details.to_string(), request_id, buffer.size()));
    return buffer;
}

void ClientHandler::send_raw(const HttpResponse &response, RequestId request_id) const {
    std::visit([&](const auto& body) {
        if constexpr (std::is_same_v<std::decay_t<decltype(body)>, std::string>) {
            const std::string raw = HttpResponseBuilder::build(response);
            size_t sent_total = 0;
            while (sent_total < raw.size()) {
                const ssize_t sent = m_sock.send(raw.data() + sent_total, raw.size() - sent_total);
                if (sent == -1) {
                    LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Send error: {}", m_sock.get(),
                                          m_host_details.to_string(), request_id, std::strerror(errno)));
                    return;
                }
                sent_total += sent;
            }
            LOG_INFO(std::format("Client[FD:{}][{}]: Request[{}]: Sent {} bytes (status: {})",
                                 m_sock.get(), m_host_details.to_string(), request_id, sent_total,
                                 static_cast<int>(response.status_code)));
        } else if constexpr (std::is_same_v<std::decay_t<decltype(body)>, HttpStreamData>) {
            std::ifstream file(body.file_path, std::ios::binary);
            if (!file) {
                LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Failed to open file for streaming: {}",
                                      m_sock.get(), m_host_details.to_string(), request_id, body.file_path.string()));
                HttpResponse error_response(HttpStatusCode::INTERNAL_SERVER_ERROR,
                                           json{{"error", "Failed to stream file"}}.dump(),
                                           CONTENT_TYPE_JSON.data());
                const std::string raw = HttpResponseBuilder::build(error_response);
                size_t sent_total = 0;
                while (sent_total < raw.size()) {
                    const ssize_t sent = m_sock.send(raw.data() + sent_total, raw.size() - sent_total);
                    if (sent == -1) {
                        LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Send error: {}", m_sock.get(),
                                              m_host_details.to_string(), request_id, std::strerror(errno)));
                        return;
                    }
                    sent_total += sent;
                }
                return;
            }
            const std::string headers = HttpResponseBuilder::build_headers_only(response);
            size_t sent_total = 0;
            while (sent_total < headers.size()) {
                const ssize_t sent = m_sock.send(headers.data() + sent_total, headers.size() - sent_total);
                if (sent == -1) {
                    LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Send error: {}", m_sock.get(),
                                          m_host_details.to_string(), request_id, std::strerror(errno)));
                    file.close();
                    return;
                }
                sent_total += sent;
            }
            std::vector<char> buffer(m_config.stream_buffer_size);
            uint64_t bytes_to_send = body.file_size;
            uint64_t bytes_sent = 0;
            file.seekg(body.offset);
            while (bytes_sent < bytes_to_send) {
                size_t chunk_size = std::min(m_config.stream_buffer_size, bytes_to_send - bytes_sent);
                file.read(buffer.data(), chunk_size);
                size_t bytes_read = file.gcount();
                if (bytes_read == 0) {
                    break;
                }

                size_t chunk_sent_total = 0;
                while (chunk_sent_total < bytes_read) {
                    const ssize_t sent = m_sock.send(buffer.data() + chunk_sent_total, bytes_read - chunk_sent_total);
                    if (sent == -1) {
                        LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Send error: {}", m_sock.get(),
                                              m_host_details.to_string(), request_id, std::strerror(errno)));
                        file.close();
                        return;
                    }
                    chunk_sent_total += sent;
                }
                bytes_sent += chunk_sent_total;
                LOG_DEBUG(std::format("Client[FD:{}][{}]: Request[{}]: Streamed {} bytes of {} (offset: {}, file_size: {}, remaining: {})",
                                      m_sock.get(), m_host_details.to_string(), request_id, chunk_sent_total,
                                      body.file_path.string(), body.offset, body.file_size, bytes_to_send - bytes_sent));
            }
            file.close();
            LOG_INFO(std::format("Client[FD:{}][{}]: Request[{}]: Sent {} bytes (status: {}, streamed)",
                                 m_sock.get(), m_host_details.to_string(), request_id, bytes_sent,
                                 static_cast<int>(response.status_code)));
        }
    }, response.body);
}

[[nodiscard]] std::optional<bool> ClientHandler::process_single_request() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    RequestId request_id = gen();
    std::string buffer;
    buffer.reserve(REQUEST_BUFFER_SIZE);
    bool keep_alive = false;

    auto raw_requests = read_requests(request_id);
    if (!raw_requests) {
        return std::nullopt;
    }

    size_t processed_bytes = 0;
    while (processed_bytes < raw_requests->size()) {
        RequestId current_request_id = gen();
        std::string single_request;
        size_t request_end = raw_requests->find("\r\n\r\n", processed_bytes);
        if (request_end == std::string::npos) {
            request_end = raw_requests->find("\n\n", processed_bytes);
            if (request_end != std::string::npos) {
                request_end += 2;
                single_request = raw_requests->substr(processed_bytes, request_end - processed_bytes) + "\r\n\r\n";
            } else {
                LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Incomplete request in pipeline", m_sock.get(),
                                      m_host_details.to_string(), current_request_id));
                return std::nullopt;
            }
        } else {
            request_end += 4;
            single_request = raw_requests->substr(processed_bytes, request_end - processed_bytes);
        }

        auto content_length = get_content_length(single_request, current_request_id);
        if (!content_length) {
            return std::nullopt;
        }

        std::optional<json> json_body;
        if (*content_length > 0) {
            size_t body_start = request_end;
            size_t body_end = body_start + *content_length;
            if (body_end > raw_requests->size()) {
                if (!read_body(*raw_requests, *content_length, request_end, current_request_id)) {
                    return std::nullopt;
                }
                body_end = request_end + *content_length;
            }
            std::string body = raw_requests->substr(body_start, *content_length);
            if (single_request.find(CONTENT_TYPE_JSON_FULL) != std::string::npos ||
                single_request.find(CONTENT_TYPE_PLAIN_FULL) != std::string::npos) {
                body.erase(
                    std::find_if(body.rbegin(), body.rend(), [](const unsigned char ch) { return !std::isspace(ch); })
                        .base(),
                    body.end());
            }
            processed_bytes = body_end;
            if (single_request.find(CONTENT_TYPE_JSON_FULL) != std::string::npos) {
                try {
                    json_body = json::parse(body);
                    LOG_INFO(std::format("Client[FD:{}][{}]: Request[{}]: Parsed JSON body", m_sock.get(),
                                         m_host_details.to_string(), current_request_id));
                } catch (const json::parse_error &e) {
                    LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: JSON parse error: {}", m_sock.get(),
                                          m_host_details.to_string(), current_request_id, e.what()));
                    send_raw(HttpResponse{HttpStatusCode::BAD_REQUEST, "Invalid JSON", CONTENT_TYPE_PLAIN.data()},
                             current_request_id);
                    return std::nullopt;
                }
            }
            single_request += body;
        } else {
            processed_bytes = request_end;
        }

        auto request = HttpRequestParser::parse(single_request, m_config.debug, current_request_id);
        if (!request) {
            send_raw(HttpResponse{HttpStatusCode::BAD_REQUEST, "Malformed request", CONTENT_TYPE_PLAIN.data()},
                     current_request_id);
            return std::nullopt;
        }

        if (request->range) {
            LOG_DEBUG(std::format("Client[FD:{}][{}]: Request[{}]: Parsed Range header: bytes={}-{}",
                                  m_sock.get(), m_host_details.to_string(), current_request_id,
                                  request->range->start, request->range->end));
        } else {
            LOG_DEBUG(std::format("Client[FD:{}][{}]: Request[{}]: No Range header",
                                  m_sock.get(), m_host_details.to_string(), current_request_id));
        }

        if (request->version == HttpVersion::HTTP_1_1 && !request->headers.contains("host")) {
            LOG_ERROR(std::format("Client[FD:{}][{}]: Request[{}]: Missing Host header", m_sock.get(),
                                  m_host_details.to_string(), current_request_id));
            send_raw(HttpResponse{HttpStatusCode::BAD_REQUEST, "Missing Host header", CONTENT_TYPE_PLAIN.data()},
                     current_request_id);
            return std::nullopt;
        }

        keep_alive = should_keep_alive(*request);
        HttpResponse response = Router::route(*request, json_body);
        response.set_header("Connection", keep_alive ? "keep-alive" : "close");
        send_raw(response, current_request_id);

        if (!keep_alive) {
            LOG_INFO(std::format("Client[FD:{}][{}]: Request[{}]: Connection closed", m_sock.get(),
                                 m_host_details.to_string(), current_request_id));
            break;
        }

        LOG_INFO(std::format("Client[FD:{}][{}]: Request[{}]: Processed {} bytes, keep-alive: {}", m_sock.get(),
                             m_host_details.to_string(), current_request_id, processed_bytes, keep_alive));
    }

    if (processed_bytes < raw_requests->size()) {
        LOG_WARNING(std::format("Client[FD:{}][{}]: Request[{}]: Partial pipeline data remaining ({} bytes)",
                                m_sock.get(), m_host_details.to_string(), request_id,
                                raw_requests->size() - processed_bytes));
    }

    return keep_alive;
}