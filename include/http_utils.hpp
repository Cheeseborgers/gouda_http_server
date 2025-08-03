// http_utils.hpp
#ifndef HTTP_UTILS_HPP
#define HTTP_UTILS_HPP

#include "http_structs.hpp"
#include "logger.hpp"
#include <string_view>
#include <utility>
#include <vector>

inline HttpResponse make_response(const HttpStatusCode code, const std::string_view content_type, const std::string_view body)
{
    HttpResponse response(code, std::string(body), std::string(content_type));
    return response;
}

inline HttpResponse make_response(const HttpStatusCode code, const std::string_view content_type, HttpStreamData stream_data)
{
    HttpResponse response(code, std::move(stream_data), std::string(content_type));
    return response;
}

// Trim leading and trailing whitespace
inline std::string_view trim(std::string_view sv)
{
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return sv;
}

// Split headers block into lines using CRLF
inline std::vector<std::string_view> split_lines(std::string_view input)
{
    std::vector<std::string_view> lines;
    for (auto part : std::views::split(input, std::string_view{"\r\n"})) {
        lines.emplace_back(part.begin(), part.end());
    }
    return lines;
}

inline std::string url_decode(std::string_view sv, RequestId request_id)
{
    std::string result;
    result.reserve(sv.size());

    LOG_DEBUG(std::format("Request[{}]: url_decode input: {}", request_id, sv));

    for (size_t i = 0; i < sv.size(); ++i) {
        if (char c = sv[i]; c == '+') {
            result += ' ';
            LOG_DEBUG(std::format("Request[{}]: Decoded '+' to space", request_id));
        }
        else if (c == '%' && i + 2 < sv.size() && std::isxdigit(static_cast<unsigned char>(sv[i + 1])) &&
                 std::isxdigit(static_cast<unsigned char>(sv[i + 2]))) {
            try {
                char hex[3] = {sv[i + 1], sv[i + 2], '\0'};
                char decoded = static_cast<char>(std::stoi(hex, nullptr, 16));
                result += decoded;
                LOG_DEBUG(std::format("Request[{}]: Decoded %{} -> {}", request_id, hex, decoded));
                i += 2;
            }
            catch (const std::exception &e) {
                LOG_WARNING(std::format("Request[{}]: Invalid percent-encoding in '{}': {}", request_id, sv, e.what()));
                result += c;
            }
        }
        else {
            result += c;
        }
    }

    LOG_DEBUG(std::format("Request[{}]: url_decode output: {}", request_id, result));
    return result;
}

inline std::string url_encode(std::string_view sv, RequestId request_id) {
    std::string result;
    result.reserve(sv.size() * 3);  // worst-case expansion

    LOG_DEBUG(std::format("Request[{}]: url_encode input: {}", request_id, sv));

    for (unsigned char c : sv) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else if (c == ' ') {
            result += '+';
            LOG_DEBUG(std::format("Request[{}]: Encoded space as '+'", request_id));
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            result += buf;
            LOG_DEBUG(std::format("Request[{}]: Encoded '{}' as {}", request_id, c, buf));
        }
    }

    LOG_DEBUG(std::format("Request[{}]: url_encode output: {}", request_id, result));
    return result;
}

inline void parse_query_params(std::string_view query, std::map<std::string, std::vector<std::string>> &params,
                               RequestId request_id, const bool debug)
{
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
        if (end == std::string_view::npos)
            end = query.size();
        std::string_view pair = trim(query.substr(start, end - start));
        if (pair.empty()) {
            start = end + 1;
            continue;
        }

        std::string key;
        std::string value;
        if (const size_t eq_pos = pair.find('='); eq_pos != std::string_view::npos) {
            key = url_decode(trim(pair.substr(0, eq_pos)), request_id);
            value = url_decode(trim(pair.substr(eq_pos + 1)), request_id);
        }
        else {
            key = url_decode(trim(pair), request_id);
            value = "";
        }

        if (key.empty()) {
            LOG_WARNING(std::format("Request[{}]: Empty query parameter key in '{}'", request_id, pair));
        }
        else {
            params[key].push_back(value);
            if (debug) {
                LOG_DEBUG(std::format("Request[{}]: Parsed query param: {}={}", request_id, key, value));
            }
        }
        start = end + 1;
    }
}

inline std::string to_lowercase(std::string_view sv)
{
    std::string result(sv); // Copy the view into a mutable string
    std::ranges::transform(result, result.begin(), [](const unsigned char c) { return std::tolower(c); });
    return result;
}

inline std::string escape_string(std::string_view input)
{
    std::string result;
    for (const char c : input) {
        if (c == '\r')
            result += "\\r";
        else if (c == '\n')
            result += "\\n";
        else if (std::isprint(static_cast<unsigned char>(c)))
            result += c;
        else
            result += std::format("\\x{:02x}", static_cast<int>(static_cast<unsigned char>(c)));
    }
    return result;
}

inline std::string to_hex(std::string_view input)
{
    std::string result;
    for (const char c : input) {
        result += std::format("{:02x} ", static_cast<int>(static_cast<unsigned char>(c)));
    }
    return result;
}

// Case-insensitive string contains check
inline bool contains_ignore_case(std::string_view str, std::string_view substr)
{
    if (substr.empty())
        return true;
    if (str.empty())
        return false;

    auto to_lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };

    auto lower_str = str | std::views::transform(to_lower);
    auto lower_substr = substr | std::views::transform(to_lower);

    const auto result = std::ranges::search(lower_str, lower_substr);
    return !result.empty();
}

inline std::string format_last_modified(const std::filesystem::file_time_type& time) {
    try {
        // Convert file_time_type to system_clock::time_point
        const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        const auto tt = std::chrono::system_clock::to_time_t(sctp);
        // Format as HTTP date (RFC 7231): "Day, DD Mon YYYY HH:MM:SS GMT"
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&tt), "%a, %d %b %Y %H:%M:%S GMT");
        return ss.str();
    } catch (const std::exception& e) {
        LOG_ERROR(std::format("Failed to format last modified time: {}", e.what()));
        return "";
    }
}

#endif // HTTP_UTILS_HPP
