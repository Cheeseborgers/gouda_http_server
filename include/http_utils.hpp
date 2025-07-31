// http_utils.hpp
#ifndef HTTP_UTILS_HPP
#define HTTP_UTILS_HPP

#include <string_view>
#include <utility>
#include <vector>
#include "http_structs.hpp"
#include "logger.hpp"

inline HttpResponse make_response(
    HttpStatusCode code,
    std::string_view content_type,
    std::string_view body) {
    HttpResponse response(code, std::string(body), std::string(content_type));
    return response;
}

inline HttpResponse make_response(
    HttpStatusCode code,
    std::string_view content_type,
    HttpStreamData stream_data) {
    HttpResponse response(code, std::move(stream_data), std::string(content_type));
    return response;
}

// Trim leading and trailing whitespace
inline  std::string_view trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return sv;
}

// Split headers block into lines using CRLF
inline std::vector<std::string_view> split_lines(std::string_view headers_block) {
    std::vector<std::string_view> lines;
    size_t start = 0;
    while (start < headers_block.size()) {
        const size_t end = headers_block.find("\r\n", start);
        if (end == std::string_view::npos) {
            lines.emplace_back(headers_block.substr(start));
            break;
        }
        lines.emplace_back(headers_block.substr(start, end - start));
        start = end + 2;
    }
    return lines;
}

inline std::string urldecode(std::string_view sv, RequestId request_id) {
    std::string result;
    result.reserve(sv.size());
    LOG_DEBUG(std::format("Request[{}]: urldecode input: {}", request_id, sv));
    for (size_t i = 0; i < sv.size(); ++i) {
        if (sv[i] == '%' && i + 2 < sv.size() && std::isxdigit(static_cast<unsigned char>(sv[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(sv[i + 2]))) {
            try {
                std::string hex = std::string(sv.substr(i + 1, 2));
                char decoded = static_cast<char>(std::stoi(hex, nullptr, 16));
                result += decoded;
                LOG_DEBUG(std::format("Request[{}]: Decoded percent-encoding: %{} -> {}", request_id, hex, decoded));
                i += 2;
            } catch (const std::exception& e) {
                LOG_WARNING(std::format("Request[{}]: Invalid percent-encoding in '{}': {}", request_id, sv, e.what()));
                result += sv[i];
            }
            } else {
                result += sv[i];
            }
    }
    // Second pass to convert '+' to space after percent-decoding
    std::string final_result;
    final_result.reserve(result.size());
    for (char c : result) {
        if (c == '+') {
            final_result += ' ';
            LOG_DEBUG(std::format("Request[{}]: Decoded '+' to space", request_id));
        } else {
            final_result += c;
        }
    }
    LOG_DEBUG(std::format("Request[{}]: urldecode output: {}", request_id, final_result));
    return final_result;
}

inline void parse_query_params(std::string_view query, std::map<std::string, std::vector<std::string>>& params,
                                  RequestId request_id, bool debug) {
    if (query.empty()) {
        if (debug) {
            LOG_DEBUG(std::format("Request[{}]: No query parameters", request_id));
        }
        return;
    }
    LOG_DEBUG(std::format("Request[{}]: Parsing query: {}", request_id, query));
    size_t start = 0;
    while (start < query.size()) {
        size_t end = query.find('&', start);
        if (end == std::string_view::npos) end = query.size();
        std::string_view pair = trim(query.substr(start, end - start));
        if (pair.empty()) {
            start = end + 1;
            continue;
        }
        size_t eq_pos = pair.find('=');
        std::string key, value;
        if (eq_pos != std::string_view::npos) {
            key = urldecode(trim(pair.substr(0, eq_pos)), request_id);
            value = urldecode(trim(pair.substr(eq_pos + 1)), request_id);
        } else {
            key = urldecode(trim(pair), request_id);
            value = "";
        }
        if (key.empty()) {
            LOG_WARNING(std::format("Request[{}]: Empty query parameter key in '{}'", request_id, pair));
        } else {
            params[key].push_back(value);
            if (debug) {
                LOG_DEBUG(std::format("Request[{}]: Parsed query param: {}={}", request_id, key, value));
            }
        }
        start = end + 1;
    }
}

inline std::string to_lowercase(std::string_view sv) {
    std::string result(sv); // Copy the view into a mutable string
    std::ranges::transform(result, result.begin(),
                           [](const unsigned char c) { return std::tolower(c); });
    return result;
}

#endif // HTTP_UTILS_HPP
