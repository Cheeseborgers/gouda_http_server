#ifndef HTTP_STRUCTS_HPP
#define HTTP_STRUCTS_HPP

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <flat_map>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "http_constants.hpp"
#include "http_status.hpp"
#include "types.hpp"

/**
 * @brief Alias for HTTP request parameters as key-value pairs.
 */
using HttpRequestParams = std::map<std::string, std::string>;

//
// Case-insensitive comparator for header keys
//

/**
 * @brief Case-insensitive comparator for HTTP header keys.
 */
struct CaseInsensitiveCompare {
    /**
     * @brief Compares two strings without case sensitivity.
     * @param a First string
     * @param b Second string
     * @return True if a < b in lowercase comparison
     */
    bool operator()(const std::string &a, const std::string &b) const
    {
        return std::ranges::lexicographical_compare(
            a, b, [](const unsigned char ac, const unsigned char bc) { return std::tolower(ac) < std::tolower(bc); });
    }
};

/**
 * @brief Alias for header map using flat_map with case-insensitive keys.
 */
using HeaderMap = std::flat_map<std::string, std::string, CaseInsensitiveCompare>;

//
// HTTP method enum
//

/**
 * @brief Supported HTTP methods.
 */
enum class HttpMethod : u8 {
    UNKNOWN, ///< Unknown method
    GET,     ///< GET
    POST,    ///< POST
    PUT,     ///< PUT
    DELETE,  ///< DELETE
    HEAD,    ///< HEAD
    OPTIONS, ///< OPTIONS
    PATCH,   ///< PATCH
    TRACE,   ///< TRACE
    CONNECT  ///< CONNECT
};

/**
 * @brief Supported HTTP versions.
 */
enum class HttpVersion : u8 {
    HTTP_0_9, ///< HTTP/0.9
    HTTP_1_0, ///< HTTP/1.0
    HTTP_1_1, ///< HTTP/1.1
    HTTP_2_0, ///< HTTP/2
    HTTP_3_0  ///< HTTP/3
};

/**
 * @brief Represents a byte range for partial HTTP requests.
 */
struct HttpRequestRange {
    u64 start{0}; ///< Starting byte index
    u64 end{0};   ///< Ending byte index (inclusive)
};

/**
 * @brief Represents a streamable HTTP file response.
 */
struct HttpStreamData {
    std::filesystem::path file_path; ///< File path
    u64 file_size{0};                ///< Size of the file
    u64 offset{0};                   ///< Offset for range-based streaming
};

/**
 * @brief Represents a full HTTP request.
 */
struct HttpRequest {
    HttpMethod method{HttpMethod::UNKNOWN};                       ///< HTTP method
    HttpVersion version{HttpVersion::HTTP_1_1};                   ///< HTTP version
    std::string path;                                             ///< Request path (e.g., "/index.html")
    HeaderMap headers;                                            ///< HTTP headers
    std::string body;                                             ///< Request body
    std::string raw;                                              ///< Full raw request string
    std::optional<HttpRequestRange> range;                        ///< Optional byte range
    std::map<std::string, std::vector<std::string>> query_params; ///< Query parameters
    std::map<std::string, std::vector<std::string>> form_params;  ///< Form parameters

    /**
     * @brief Sets or replaces an HTTP request header.
     * @param key Header name
     * @param value Header value
     */
    void set_header(std::string key, std::string value) { headers.emplace(std::move(key), std::move(value)); }

    /**
     * @brief Gets the value of a header if it exists.
     * @param key Header name
     * @return std::optional<std::string_view> containing the value, if present
     */
    [[nodiscard]] std::optional<std::string_view> get_header(const std::string_view key) const
    {
        if (const auto it = headers.find(std::string(key)); it != headers.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief Checks if a header exists.
     * @param key Header name
     * @return bool true if present
     */
    [[nodiscard]] bool has_header(const std::string_view key) const { return headers.contains(std::string(key)); }
};

/**
 * @brief Represents an HTTP response to be sent back to the client.
 */
struct HttpResponse {
    HttpStatusCode status_code{HttpStatusCode::OK};                 ///< HTTP status code
    std::string content_type{std::string(CONTENT_TYPE_PLAIN_UTF8)}; ///< MIME type
    HeaderMap headers;                                              ///< Response headers
    std::variant<std::string, HttpStreamData> body;                 ///< Body (text or file stream)

    /**
     * @brief Default constructor. Sets default headers.
     */
    HttpResponse() { set_default_headers(); }

    /**
     * @brief Constructs a response with plain text body.
     * @param status_code HTTP status code
     * @param body Response body (string)
     * @param content_type_ Content-Type header value
     */
    explicit HttpResponse(const HttpStatusCode status_code, const std::string &body = {},
                          const std::string_view content_type_ = std::string(CONTENT_TYPE_PLAIN_UTF8))
        : status_code{status_code}, content_type{std::string(content_type_)}, body{body}
    {
        set_default_headers();
        set_header("Content-Type", content_type);
    }

    /**
     * @brief Constructs a response with streamable file data.
     * @param status_code HTTP status code
     * @param body File stream data
     * @param content_type_ Content-Type header value
     */
    explicit HttpResponse(const HttpStatusCode status_code, HttpStreamData body,
                          const std::string_view content_type_ = CONTENT_TYPE_OCTET_STREAM)
        : status_code{status_code}, content_type{std::string(content_type_)}, body{std::move(body)}
    {
        set_default_headers();
        set_header("Content-Type", content_type);
    }

    /**
     * @brief Sets or replaces an HTTP response header.
     * @param key Header name
     * @param value Header value
     */
    void set_header(std::string key, std::string value) { headers.emplace(std::move(key), std::move(value)); }

    /**
     * @brief Gets the value of a header if it exists.
     * @param key Header name
     * @return std::optional<std::string_view> containing the value, if present
     */
    [[nodiscard]] std::optional<std::string_view> get_header(const std::string_view key) const
    {
        if (const auto it = headers.find(std::string(key)); it != headers.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * @brief Checks if a header exists.
     * @param key Header name
     * @return bool true if present
     */
    [[nodiscard]] bool has_header(const std::string_view key) const { return headers.contains(std::string(key)); }

private:
    /**
     * @brief Adds default headers like Server and X-Powered-By.
     */
    void set_default_headers()
    {
        headers.try_emplace("X-Powered-By", POWERED_BY_TEXT);
        headers.try_emplace("Server", SERVER_NAME_VERSION);
    }
};

//
// Enum string maps
//

/**
 * @brief Lookup table for converting HttpMethod to string.
 */
constexpr std::array method_strings{"UNKNOWN", "GET",     "POST",  "PUT",   "DELETE",
                                    "HEAD",    "OPTIONS", "PATCH", "TRACE", "CONNECT"};

/**
 * @brief Converts an HttpMethod enum to its string representation.
 * @param method HTTP method
 * @return Corresponding method as string view
 */
constexpr std::string_view method_to_string_view(HttpMethod method)
{
    const auto index = static_cast<std::underlying_type_t<HttpMethod>>(method);
    return index < method_strings.size() ? method_strings[index] : "UNKNOWN";
}

/**
 * @brief Parses a method string to its corresponding HttpMethod enum.
 * @param method_str String representation of the HTTP method
 * @return Corresponding HttpMethod
 */
inline HttpMethod get_method(const std::string_view method_str)
{
    for (size_t i = 0; i < method_strings.size(); ++i) {
        if (method_strings[i] == method_str)
            return static_cast<HttpMethod>(i);
    }
    return HttpMethod::UNKNOWN;
}

/**
 * @brief Lookup table for converting HttpVersion to string.
 */
constexpr std::array version_strings{"HTTP/0.9", "HTTP/1.0", "HTTP/1.1", "HTTP/2", "HTTP/3"};

/**
 * @brief Converts an HttpVersion enum to its string representation.
 * @param version HTTP version
 * @return Corresponding version as string view
 */
constexpr std::string_view http_version_to_string_view(HttpVersion version)
{
    const auto index = static_cast<std::underlying_type_t<HttpVersion>>(version);
    return index < version_strings.size() ? version_strings[index] : "UNKNOWN";
}

/**
 * @brief Parses a version string to its corresponding HttpVersion enum.
 * @param str String representation of the HTTP version
 * @return Corresponding HttpVersion
 */
inline HttpVersion string_to_http_version(const std::string_view str)
{
    static const std::flat_map<std::string_view, HttpVersion> map{{"HTTP/0.9", HttpVersion::HTTP_0_9},
                                                                  {"HTTP/1.0", HttpVersion::HTTP_1_0},
                                                                  {"HTTP/1.1", HttpVersion::HTTP_1_1},
                                                                  {"HTTP/2", HttpVersion::HTTP_2_0},
                                                                  {"HTTP/3", HttpVersion::HTTP_3_0}};
    const auto it = map.find(str);
    return it != map.end() ? it->second : HttpVersion::HTTP_1_1;
}

#endif // HTTP_STRUCTS_HPP
