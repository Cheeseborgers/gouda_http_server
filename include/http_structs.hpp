#ifndef HTTP_STRUCTS_HPP
#define HTTP_STRUCTS_HPP

#include <algorithm>
#include <cctype>
#include <format>
#include <map>
#include <string>
#include <variant>

#include "http_constants.hpp"
#include "http_status.hpp"
#include "types.hpp"

using HttpRequestParams = std::map<std::string, std::string>;

//
// Case-insensitive comparator for header keys
//
struct CaseInsensitiveCompare {
    bool operator()(const std::string &a, const std::string &b) const
    {
        return std::ranges::lexicographical_compare(
            a, b, [](const unsigned char ac, const unsigned char bc) { return std::tolower(ac) < std::tolower(bc); });
    }
};

//
// HTTP method enum
//
enum class HttpMethod : u8 { UNKNOWN, GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH, TRACE, CONNECT };

enum class HttpVersion : u8 {
    HTTP_0_9, // HTTP/0.9
    HTTP_1_0, // HTTP/1.0
    HTTP_1_1, // HTTP/1.1
    HTTP_2_0, // HTTP/2
    HTTP_3_0  // HTTP/3
};

struct HttpRequestRange {
    u64 start;
    u64 end; // Inclusive
};

struct HttpStreamData {
    std::filesystem::path file_path;
    u64 file_size;
    u64 offset; // For range requests
};

//
// HTTP request
//
struct HttpRequest {
    HttpMethod method{HttpMethod::UNKNOWN};
    HttpVersion version{HttpVersion::HTTP_1_1};
    std::string path;
    std::map<std::string, std::string, CaseInsensitiveCompare> headers;
    std::string body;
    std::string raw; // full raw request
    std::optional<HttpRequestRange> range;
    std::map<std::string, std::vector<std::string>> query_params;
    std::map<std::string, std::vector<std::string>> form_params;
};

//
// HTTP response
//
struct HttpResponse {
    HttpStatusCode status_code{HttpStatusCode::OK};
    std::string content_type{"text/plain; charset=utf-8"};  // Default content-type
    std::map<std::string, std::string, CaseInsensitiveCompare> headers;
    std::variant<std::string, HttpStreamData> body;

    HttpResponse() {
        // Set default headers on construction
        set_default_headers();
    }

    explicit HttpResponse(HttpStatusCode status_code,
                          const std::string &body = {},
                          std::string_view content_type_ = "text/plain; charset=utf-8")
        : status_code{status_code}, content_type{content_type_}, body{body}
    {
        set_default_headers();
        set_header("Content-Type", content_type);
    }

    explicit HttpResponse(HttpStatusCode status_code,
                          HttpStreamData body,
                          std::string_view content_type_ = "application/octet-stream")
        : status_code{status_code}, content_type{content_type_}, body{std::move(body)}
    {
        set_default_headers();
        set_header("Content-Type", content_type);
    }

    void set_header(std::string_view key, std::string_view value) {
        headers[std::string(key)] = std::string(value);
    }

private:
    void set_default_headers() {
        if (!headers.contains("X-Powered-By")) {
            headers["X-Powered-By"] = POWERED_BY_TEXT;
        }

        if (!headers.contains("Server")) {
            headers["Server"] = SERVER_NAME_VER;
        }

        // Content-Type is set in constructors explicitly
    }
};

//
// Helpers
//
// Convert Method enum to string
inline std::string method_to_string(const HttpMethod method)
{
    switch (method) {
        case HttpMethod::GET:
            return "GET";
        case HttpMethod::POST:
            return "POST";
        case HttpMethod::PUT:
            return "PUT";
        case HttpMethod::DELETE:
            return "DELETE";
        case HttpMethod::HEAD:
            return "HEAD";
        case HttpMethod::OPTIONS:
            return "OPTIONS";
        case HttpMethod::PATCH:
            return "PATCH";
        case HttpMethod::TRACE:
            return "TRACE";
        case HttpMethod::CONNECT:
            return "CONNECT";
        default:
            return "UNKNOWN";
    }
}

// Parse method string to Method enum
inline HttpMethod get_method(std::string_view method_str)
{
    if (method_str == "GET")
        return HttpMethod::GET;
    if (method_str == "POST")
        return HttpMethod::POST;
    if (method_str == "PUT")
        return HttpMethod::PUT;
    if (method_str == "DELETE")
        return HttpMethod::DELETE;
    if (method_str == "HEAD")
        return HttpMethod::HEAD;
    if (method_str == "OPTIONS")
        return HttpMethod::OPTIONS;
    if (method_str == "PATCH")
        return HttpMethod::PATCH;
    if (method_str == "TRACE")
        return HttpMethod::TRACE;
    if (method_str == "CONNECT")
        return HttpMethod::CONNECT;
    return HttpMethod::UNKNOWN;
}

inline std::string http_version_to_string(const HttpVersion version)
{
    switch (version) {
        case HttpVersion::HTTP_0_9:
            return "HTTP/0.9";
        case HttpVersion::HTTP_1_0:
            return "HTTP/1.0";
        case HttpVersion::HTTP_1_1:
            return "HTTP/1.1";
        case HttpVersion::HTTP_2_0:
            return "HTTP/2";
        case HttpVersion::HTTP_3_0:
            return "HTTP/3";
        default:
            return "UNKNOWN";
    }
}

inline HttpVersion string_to_http_version(std::string_view str)
{
    static const std::unordered_map<std::string, HttpVersion> map = {{"HTTP/0.9", HttpVersion::HTTP_0_9},
                                                                     {"HTTP/1.0", HttpVersion::HTTP_1_0},
                                                                     {"HTTP/1.1", HttpVersion::HTTP_1_1},
                                                                     {"HTTP/2", HttpVersion::HTTP_2_0},
                                                                     {"HTTP/3", HttpVersion::HTTP_3_0}};
    const auto it = map.find(str.data());
    return it != map.end() ? it->second : HttpVersion::HTTP_1_1; // Default to 1.1
}

#endif // HTTP_STRUCTS_HPP
