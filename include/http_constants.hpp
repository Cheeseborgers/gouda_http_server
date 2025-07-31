//
// Created by fason on 28/07/25.
//

#ifndef HTTP_CONSTANT_HPP
#define HTTP_CONSTANT_HPP
#include <string_view>

static constexpr std::string_view CONTENT_TYPE_PLAIN = "text/plain";
static constexpr std::string_view CONTENT_TYPE_HTML = "text/html;";
static constexpr std::string_view CONTENT_TYPE_JSON = "application/json";
static constexpr std::string_view CONTENT_TYPE_PLAIN_FULL = "Content-Type: text/plain";
static constexpr std::string_view CONTENT_TYPE_HTML_FULL = "Content-Type: text/html";
static constexpr std::string_view CONTENT_TYPE_JSON_FULL = "Content-Type: application/json";

static constexpr int REQUEST_BUFFER_SIZE{1024};
static constexpr int REQUEST_BODY_BUFFER_SIZE{512};
static constexpr int REQUEST_HEADERS_BUFFER_SIZE{512};

static constexpr uint8_t DEFAULT_RECV_TIMEOUT{10};
static constexpr uint8_t DEFAULT_SEND_TIMEOUT{5};
static constexpr uint8_t DEFAULT_MAX_REQUESTS{100};
static constexpr size_t DEFAULT_MAX_HEADER_SIZE{8192};
static constexpr size_t DEFAULT_MAX_CONTENT_LENGTH{1024 * 1024};
static constexpr size_t DEFAULT_MAX_STREAM_BUFFER_SIZE{64 * 1024};
static constexpr uint64_t DEFAULT_STREAM_THRESHOLD{1 * 1024 * 1024}; // 1MB

#endif //HTTP_CONSTANT_HPP
