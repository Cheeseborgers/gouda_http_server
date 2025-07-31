//
// Created by fason on 28/07/25.
//
#ifndef HTTP_STATUS_HPP
#define HTTP_STATUS_HPP

#include <string>

enum class HttpStatusCode {
    // 1xx – Informational
    CONTINUE = 100,
    SWITCHING_PROTOCOLS = 101,
    PROCESSING = 102,

    // 2xx – Success
    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    NO_CONTENT = 204,
    PARTIAL_CONTENT = 206,

    // 3xx – Redirection
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    SEE_OTHER = 303,
    NOT_MODIFIED = 304,
    TEMPORARY_REDIRECT = 307,
    PERMANENT_REDIRECT = 308,

    // 4xx – Client Errors
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    CONFLICT = 409,
    PAYLOAD_TOO_LARGE = 413,
    UNSUPPORTED_MEDIA_TYPE = 415,
    TOO_MANY_REQUESTS = 429,
    RANGE_NOT_SATISFIABLE = 416,

    // 5xx – Server Errors
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
    GATEWAY_TIMEOUT = 504,
    HTTP_VERSION_NOT_SUPPORTED = 505
};

inline std::string status_text(const HttpStatusCode code) {
    switch (code) {
        // 1xx – Informational
        case HttpStatusCode::CONTINUE: return "Continue";
        case HttpStatusCode::SWITCHING_PROTOCOLS: return "Switching Protocols";
        case HttpStatusCode::PROCESSING: return "Processing";

        // 2xx – Success
        case HttpStatusCode::OK: return "OK";
        case HttpStatusCode::CREATED: return "Created";
        case HttpStatusCode::ACCEPTED: return "Accepted";
        case HttpStatusCode::NO_CONTENT: return "No Content";
        case HttpStatusCode::PARTIAL_CONTENT: return "Partial Content";

        // 3xx – Redirection
        case HttpStatusCode::MOVED_PERMANENTLY: return "Moved Permanently";
        case HttpStatusCode::FOUND: return "Found";
        case HttpStatusCode::SEE_OTHER: return "See Other";
        case HttpStatusCode::NOT_MODIFIED: return "Not Modified";
        case HttpStatusCode::TEMPORARY_REDIRECT: return "Temporary Redirect";
        case HttpStatusCode::PERMANENT_REDIRECT: return "Permanent Redirect";

        // 4xx – Client Errors
        case HttpStatusCode::BAD_REQUEST: return "Bad Request";
        case HttpStatusCode::UNAUTHORIZED: return "Unauthorized";
        case HttpStatusCode::FORBIDDEN: return "Forbidden";
        case HttpStatusCode::NOT_FOUND: return "Not Found";
        case HttpStatusCode::METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HttpStatusCode::CONFLICT: return "Conflict";
        case HttpStatusCode::PAYLOAD_TOO_LARGE: return "Payload Too Large";
        case HttpStatusCode::UNSUPPORTED_MEDIA_TYPE: return "Unsupported Media Type";
        case HttpStatusCode::TOO_MANY_REQUESTS: return "Too Many Requests";
        case HttpStatusCode::RANGE_NOT_SATISFIABLE: return "Range Not Satisfiable";

        // 5xx – Server Errors
        case HttpStatusCode::INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HttpStatusCode::NOT_IMPLEMENTED: return "Not Implemented";
        case HttpStatusCode::BAD_GATEWAY: return "Bad Gateway";
        case HttpStatusCode::SERVICE_UNAVAILABLE: return "Service Unavailable";
        case HttpStatusCode::GATEWAY_TIMEOUT: return "Gateway Timeout";
        case HttpStatusCode::HTTP_VERSION_NOT_SUPPORTED: return "HTTP Version Not Supported";

        default: return "Unknown";
    }
}

#endif // HTTP_STATUS_HPP
