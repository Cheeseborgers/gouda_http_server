//
// Created by fason on 28/07/25.
//
#ifndef HTTP_STATUS_HPP
#define HTTP_STATUS_HPP

#include <string_view>

#include "types.hpp"

/**
 * @brief Enumeration of standard HTTP status codes as defined by RFCs.
 */
enum class HttpStatusCode : u16 {
    //
    // 1xx – Informational
    //

    CONTINUE = 100,            ///< Request received, continuing process.
    SWITCHING_PROTOCOLS = 101, ///< Switching to another protocol.
    PROCESSING = 102,          ///< WebDAV: Server has received and is processing the request.
    EARLY_HINTS = 103,         ///< Hints that the server is likely to send the final response.

    //
    // 2xx – Success
    //

    OK = 200,                            ///< Request succeeded.
    CREATED = 201,                       ///< Resource created.
    ACCEPTED = 202,                      ///< Request accepted, processing pending.
    NON_AUTHORITATIVE_INFORMATION = 203, ///< Metadata returned from a transforming proxy.
    NO_CONTENT = 204,                    ///< No content to return.
    RESET_CONTENT = 205,                 ///< Reset the document view.
    PARTIAL_CONTENT = 206,               ///< Partial resource delivered.
    MULTI_STATUS = 207,                  ///< WebDAV: Multiple responses.
    ALREADY_REPORTED = 208,              ///< WebDAV: Members already reported.
    IM_USED = 226,                       ///< RFC 3229: Delta encoding response.

    //
    // 3xx – Redirection
    //

    MULTIPLE_CHOICES = 300,   ///< Multiple options for the resource.
    MOVED_PERMANENTLY = 301,  ///< Resource moved permanently.
    FOUND = 302,              ///< Resource found (temporary).
    SEE_OTHER = 303,          ///< See another URI using GET.
    NOT_MODIFIED = 304,       ///< Resource not modified.
    USE_PROXY = 305,          ///< Deprecated: Use proxy.
    SWITCH_PROXY = 306,       ///< No longer used.
    TEMPORARY_REDIRECT = 307, ///< Resource temporarily at another URI.
    PERMANENT_REDIRECT = 308, ///< Resource permanently at another URI.

    //
    // 4xx – Client Errors
    //

    BAD_REQUEST = 400,                     ///< Malformed request.
    UNAUTHORIZED = 401,                    ///< Authentication required.
    PAYMENT_REQUIRED = 402,                ///< Reserved for future use.
    FORBIDDEN = 403,                       ///< Request understood but refused.
    NOT_FOUND = 404,                       ///< Resource not found.
    METHOD_NOT_ALLOWED = 405,              ///< HTTP method not allowed.
    NOT_ACCEPTABLE = 406,                  ///< Resource not acceptable.
    PROXY_AUTHENTICATION_REQUIRED = 407,   ///< Authentication with proxy required.
    REQUEST_TIMEOUT = 408,                 ///< Request timed out.
    CONFLICT = 409,                        ///< Request conflict.
    GONE = 410,                            ///< Resource no longer available.
    LENGTH_REQUIRED = 411,                 ///< Content-Length header missing.
    PRECONDITION_FAILED = 412,             ///< Preconditions failed.
    PAYLOAD_TOO_LARGE = 413,               ///< Payload too large.
    URI_TOO_LONG = 414,                    ///< URI too long.
    UNSUPPORTED_MEDIA_TYPE = 415,          ///< Unsupported media type.
    RANGE_NOT_SATISFIABLE = 416,           ///< Requested range not satisfiable.
    EXPECTATION_FAILED = 417,              ///< Expectation failed.
    IM_A_TEAPOT = 418,                     ///< April Fools joke (RFC 2324).
    MISDIRECTED_REQUEST = 421,             ///< Request misdirected to wrong server.
    UNPROCESSABLE_ENTITY = 422,            ///< WebDAV: Semantic errors in request.
    LOCKED = 423,                          ///< WebDAV: Resource is locked.
    FAILED_DEPENDENCY = 424,               ///< WebDAV: Failed due to another request.
    TOO_EARLY = 425,                       ///< Premature request.
    UPGRADE_REQUIRED = 426,                ///< Protocol upgrade required.
    PRECONDITION_REQUIRED = 428,           ///< Requires conditional request.
    TOO_MANY_REQUESTS = 429,               ///< Rate limit exceeded.
    REQUEST_HEADER_FIELDS_TOO_LARGE = 431, ///< Request headers too large.
    UNAVAILABLE_FOR_LEGAL_REASONS = 451,   ///< Access denied due to legal reasons.

    //
    // 5xx – Server Errors
    //

    INTERNAL_SERVER_ERROR = 500,          ///< Internal server error.
    NOT_IMPLEMENTED = 501,                ///< Not implemented.
    BAD_GATEWAY = 502,                    ///< Invalid response from upstream server.
    SERVICE_UNAVAILABLE = 503,            ///< Service temporarily unavailable.
    GATEWAY_TIMEOUT = 504,                ///< Upstream server timeout.
    HTTP_VERSION_NOT_SUPPORTED = 505,     ///< HTTP version unsupported.
    VARIANT_ALSO_NEGOTIATES = 506,        ///< Negotiation failed.
    INSUFFICIENT_STORAGE = 507,           ///< WebDAV: Storage capacity full.
    LOOP_DETECTED = 508,                  ///< WebDAV: Infinite loop detected.
    NOT_EXTENDED = 510,                   ///< Further extensions required.
    NETWORK_AUTHENTICATION_REQUIRED = 511 ///< Network authentication required.
};

constexpr std::array<std::string_view, 512> http_status_messages = [] {
    std::array<std::string_view, 512> arr{};

    arr[100] = "Continue";
    arr[101] = "Switching Protocols";
    arr[102] = "Processing";
    arr[103] = "Early Hints";

    arr[200] = "OK";
    arr[201] = "Created";
    arr[202] = "Accepted";
    arr[203] = "Non-Authoritative Information";
    arr[204] = "No Content";
    arr[205] = "Reset Content";
    arr[206] = "Partial Content";
    arr[207] = "Multi-Status";
    arr[208] = "Already Reported";
    arr[226] = "IM Used";

    arr[300] = "Multiple Choices";
    arr[301] = "Moved Permanently";
    arr[302] = "Found";
    arr[303] = "See Other";
    arr[304] = "Not Modified";
    arr[305] = "Use Proxy";
    arr[306] = "Switch Proxy";
    arr[307] = "Temporary Redirect";
    arr[308] = "Permanent Redirect";

    arr[400] = "Bad Request";
    arr[401] = "Unauthorized";
    arr[402] = "Payment Required";
    arr[403] = "Forbidden";
    arr[404] = "Not Found";
    arr[405] = "Method Not Allowed";
    arr[406] = "Not Acceptable";
    arr[407] = "Proxy Authentication Required";
    arr[408] = "Request Timeout";
    arr[409] = "Conflict";
    arr[410] = "Gone";
    arr[411] = "Length Required";
    arr[412] = "Precondition Failed";
    arr[413] = "Payload Too Large";
    arr[414] = "URI Too Long";
    arr[415] = "Unsupported Media Type";
    arr[416] = "Range Not Satisfiable";
    arr[417] = "Expectation Failed";
    arr[418] = "I'm a teapot";
    arr[421] = "Misdirected Request";
    arr[422] = "Unprocessable Entity";
    arr[423] = "Locked";
    arr[424] = "Failed Dependency";
    arr[425] = "Too Early";
    arr[426] = "Upgrade Required";
    arr[428] = "Precondition Required";
    arr[429] = "Too Many Requests";
    arr[431] = "Request Header Fields Too Large";
    arr[451] = "Unavailable For Legal Reasons";

    arr[500] = "Internal Server Error";
    arr[501] = "Not Implemented";
    arr[502] = "Bad Gateway";
    arr[503] = "Service Unavailable";
    arr[504] = "Gateway Timeout";
    arr[505] = "HTTP Version Not Supported";
    arr[506] = "Variant Also Negotiates";
    arr[507] = "Insufficient Storage";
    arr[508] = "Loop Detected";
    arr[510] = "Not Extended";
    arr[511] = "Network Authentication Required";

    return arr;
}();

constexpr std::string_view status_code_to_string_view(HttpStatusCode code) {
    if (const u16 index = static_cast<u16>(code); index < http_status_messages.size() && !http_status_messages[index].empty()) {
        return http_status_messages[index];
    }
    return "Unknown";
}

#endif // HTTP_STATUS_HPP
