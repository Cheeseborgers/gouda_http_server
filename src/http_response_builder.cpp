//
// Created by fason on 28/07/25.
//

#include "../include/http_response_builder.h"

#include <format>
#include <sstream>

std::string HttpResponseBuilder::build(const HttpResponse &response)
{
    std::ostringstream oss;
    const int code = static_cast<int>(response.status_code);
    const std::string reason = status_text(response.status_code);

    oss << std::format("HTTP/1.1 {} {}\r\n", code, reason);
    oss << std::format("Content-Type: {}\r\n", response.content_type);

    std::visit(
        [&](const auto &body) {
            if constexpr (std::is_same_v<std::decay_t<decltype(body)>, std::string>) {
                oss << std::format("Content-Length: {}\r\n", body.size());
            }
            else if constexpr (std::is_same_v<std::decay_t<decltype(body)>, HttpStreamData>) {
                oss << std::format("Content-Length: {}\r\n", body.file_size);
            }
        },
        response.body);

    for (const auto &[key, value] : response.headers) {
        if (key == "Content-Type" || key == "Content-Length") {
            continue;
        }
        oss << std::format("{}: {}\r\n", key, value);
    }

    oss << "\r\n";

    std::visit(
        [&](const auto &body) {
            if constexpr (std::is_same_v<std::decay_t<decltype(body)>, std::string>) {
                oss << body;
            }
            // StreamData: no body in headers
        },
        response.body);

    return oss.str();
}

std::string HttpResponseBuilder::build_headers_only(const HttpResponse& response) {
    std::ostringstream oss;
    const int code = static_cast<int>(response.status_code);
    const std::string reason = status_text(response.status_code);

    oss << std::format("HTTP/1.1 {} {}\r\n", code, reason);
    oss << std::format("Content-Type: {}\r\n", response.content_type);

    std::visit([&](const auto& body) {
        if constexpr (std::is_same_v<std::decay_t<decltype(body)>, std::string>) {
            oss << std::format("Content-Length: {}\r\n", body.size());
        } else if constexpr (std::is_same_v<std::decay_t<decltype(body)>, HttpStreamData>) {
            oss << std::format("Content-Length: {}\r\n", body.file_size);
        }
    }, response.body);

    for (const auto& [key, value] : response.headers) {
        oss << std::format("{}: {}\r\n", key, value);
    }

    oss << "\r\n";
    return oss.str();
}
