//
// Created by fason on 28/07/25.
//

#ifndef HTTP_CONSTANT_HPP
#define HTTP_CONSTANT_HPP
#include <string_view>

static constexpr std::string_view SERVER_NAME_VERSION = "GoudaWebserver/1.0";
static constexpr std::string_view POWERED_BY_TEXT = "C++23 and ☕";

static constexpr std::string_view LOCALHOST = "127.0.0.1";

static constexpr std::string_view CONTENT_TYPE_PLAIN{"text/plain"};
static constexpr std::string_view CONTENT_TYPE_PLAIN_UTF8{"text/plain; charset=utf-8"};
static constexpr std::string_view CONTENT_TYPE_HTML{"text/html"};
static constexpr std::string_view CONTENT_TYPE_JSON{"application/json"};
static constexpr std::string_view CONTENT_TYPE_OCTET_STREAM{"application/octet-stream"};
static constexpr std::string_view CONTENT_TYPE_FORM_URLENCODED{"application/x-www-form-urlencoded"};
static constexpr std::string_view CONTENT_TYPE_PLAIN_FULL{"Content-Type: text/plain"};
static constexpr std::string_view CONTENT_TYPE_HTML_FULL{"Content-Type: text/html"};
static constexpr std::string_view CONTENT_TYPE_JSON_FULL{"Content-Type: application/json"};

static constexpr int REQUEST_BUFFER_SIZE{1024};
static constexpr int REQUEST_BODY_BUFFER_SIZE{512};
static constexpr int REQUEST_HEADERS_BUFFER_SIZE{512};
static constexpr int MAX_RESPONSE_SIZE{1024};

static constexpr int DEFAULT_POLL_INTERVAL{100};
static constexpr std::chrono::seconds DEFAULT_RECV_TIMEOUT{5};
static constexpr std::chrono::seconds DEFAULT_SEND_TIMEOUT{5};
static constexpr std::chrono::seconds DEFAULT_WEBSOCKET_TIMEOUT{60};

static constexpr uint8_t DEFAULT_MAX_REQUESTS{100};
static constexpr size_t DEFAULT_MAX_HEADER_SIZE{8192};
static constexpr size_t DEFAULT_MAX_CONTENT_LENGTH{1024 * 1024};
static constexpr size_t DEFAULT_MAX_STREAM_BUFFER_SIZE{64 * 1024};
static constexpr uint64_t DEFAULT_STREAM_THRESHOLD{1 * 1024 * 1024}; // 1MB
static constexpr size_t DEFAULT_MAX_FILE_CACHE_SIZE{100};

inline constexpr std::string_view ERROR_404_HTML = R"(
<!DOCTYPE html>
<html>
<head><title>404 Not Found</title></head>
<body>
  <h1>404</h1>
  <p>Oops! The page you’re looking for can’t be found.</p>
  <p><a href="/">Return to Home</a></p>
</body>
</html>
)";

inline constexpr std::string_view ERROR_403_HTML = R"(
<!DOCTYPE html>
<html>
<head><title>403 Forbidden</title></head>
<body>
<h1>403 Forbidden</h1>
<p>Access to the requested resource is denied.</p>
</body>
</html>
)";

inline constexpr std::string_view ERROR_500_HTML = R"(
<!DOCTYPE html>
<html>
<head><title>500 Internal Server Error</title></head>
<body>
<h1>500 Internal Server Error</h1>
<p>An error occurred while processing your request.</p>
</body>
</html>
)";

inline constexpr std::string_view ERROR_416_HTML = R"(
<!DOCTYPE html>
<html>
<head><title>416 Range Not Satisfiable</title></head>
<body>
<h1>416 Range Not Satisfiable</h1>
<p>An error occurred while processing your request.</p>
</body>
</html>
)";

#endif //HTTP_CONSTANT_HPP
