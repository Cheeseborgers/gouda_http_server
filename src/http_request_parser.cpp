//
// Created by fason on 28/07/25.
//

#include "../include/http_request_parser.h"

#include <cctype>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../include/http_structs.hpp"
#include "../include/logger.hpp"
#include "http_utils.hpp"

std::optional<HttpRequest> HttpRequestParser::parse(std::string_view request, const bool debug, RequestId request_id)
{
    HttpRequest req;

    const size_t first_line_end = request.find("\r\n");
    if (first_line_end == std::string_view::npos) {
        LOG_ERROR(std::format("Request[{}]: No \\r\\n found in request", request_id));
        return std::nullopt;
    }

    const size_t headers_end = request.find("\r\n\r\n");
    if (headers_end == std::string_view::npos) {
        LOG_ERROR(std::format("Request[{}]: No \\r\\n\\r\\n found in request", request_id));
        return std::nullopt;
    }

    std::string_view first_line = request.substr(0, first_line_end);
    const size_t method_end = first_line.find(' ');
    const size_t path_end = first_line.find(' ', method_end + 1);

    if (method_end == std::string_view::npos || path_end == std::string_view::npos) {
        LOG_ERROR(std::format("Request[{}]: Invalid request line", request_id));
        return std::nullopt;
    }

    req.method = get_method(first_line.substr(0, method_end));
    std::string_view full_path = first_line.substr(method_end + 1, path_end - method_end - 1);
    // Split path and query parameters
    size_t query_start = full_path.find('?');
    if (query_start != std::string_view::npos) {
        req.path = std::string(full_path.substr(0, query_start));
        std::string_view query = full_path.substr(query_start + 1);
        parse_query_params(query, req.query_params, request_id, debug);
    }
    else {
        req.path = std::string(full_path);
    }
    req.version = string_to_http_version(first_line.substr(path_end + 1));
    req.raw = std::string(request);

    if (debug) {
        LOG_DEBUG(std::format("Request[{}]: Parsed first line: {} {} {}", request_id, method_to_string(req.method),
                              req.path, http_version_to_string(req.version)));
        for (const auto &[key, values] : req.query_params) {
            for (const auto &value : values) {
                LOG_DEBUG(std::format("Request[{}]: Parsed query param: {}={}", request_id, key, value));
            }
        }
    }

    std::string_view headers_block = request.substr(first_line_end + 2, headers_end - first_line_end - 2);
    for (const auto &line : split_lines(headers_block)) {
        if (line.empty()) {
            continue;
        }

        const size_t colon_pos = line.find(':');
        if (colon_pos == std::string_view::npos) {
            LOG_WARNING(std::format("Request[{}]: Malformed header line", request_id));
            continue;
        }

        std::string_view key = trim(line.substr(0, colon_pos));
        std::string_view value = trim(line.substr(colon_pos + 1));
        std::string key_lower = to_lowercase(key);
        req.headers[key_lower] = value;

        if (key_lower == "range") {
            std::regex range_regex(R"(bytes=(\d+)-(\d*))");
            std::smatch range_matches;
            std::string value_str(value);
            if (std::regex_match(value_str, range_matches, range_regex)) {
                try {
                    HttpRequestRange range;
                    range.start = std::stoull(range_matches[1].str());
                    range.end = range_matches[2].str().empty() ? 0 : std::stoull(range_matches[2].str());
                    req.range = range;
                    if (debug) {
                        LOG_DEBUG(std::format("Request[{}]: Parsed Range header: bytes={}-{}", request_id, range.start,
                                              range.end));
                    }
                }
                catch (const std::exception &e) {
                    LOG_ERROR(
                        std::format("Request[{}]: Invalid Range header value: {} ({})", request_id, value, e.what()));
                    return std::nullopt;
                }
            }
            else {
                LOG_ERROR(std::format("Request[{}]: Malformed Range header: {}", request_id, value));
                return std::nullopt;
            }
        }

        if (debug) {
            LOG_DEBUG(std::format("Request[{}]: Parsed header: {}: {}", request_id, key, value));
        }
    }

    if (headers_end + 4 < request.size()) {
        std::string_view body = request.substr(headers_end + 4);
        req.body = std::string(body);
        if (debug) {
            LOG_DEBUG(std::format("Request[{}]: Parsed body ({} bytes)", request_id, body.size()));
        }
        if (req.method == HttpMethod::POST) {
            auto content_type_iterator = req.headers.find("content-type");
            if (content_type_iterator != req.headers.end() &&
                content_type_iterator->second.find("application/x-www-form-urlencoded") != std::string::npos) {
                parse_query_params(body, req.form_params, request_id, debug);
                if (debug) {
                    for (const auto &[key, values] : req.form_params) {
                        for (const auto &value : values) {
                            LOG_DEBUG(std::format("Request[{}]: Parsed form param: {}={}", request_id, key, value));
                        }
                    }
                }
            }
        }
    }

    LOG_INFO(std::format("Request[{}]: {} {}", request_id, method_to_string(req.method), req.path));
    return req;
}
