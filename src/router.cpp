#include "router.hpp"
#include "http_utils.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>

#include "logger.hpp"

std::filesystem::path Router::s_static_files_directory{"static"};
std::string Router::s_static_url_prefix{"/assets/"};
std::unordered_map<HttpMethod, std::vector<Router::Route>> Router::s_routes_by_method;
std::vector<Router::Middleware> Router::s_middlewares;

HttpResponse Router::route(const HttpRequest& request, const std::optional<json>& json_body) {
    using enum HttpStatusCode;
    std::map<std::string, std::string> params;
    std::smatch matches;

    static const std::unordered_map<std::string, std::string> mime_types = {
        {".html", "text/html"}, {".css", "text/css"}, {".js", "application/javascript"},
        {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
        {".gif", "image/gif"}, {".svg", "image/svg+xml"}, {".json", "application/json"},
        {".ico", "image/x-icon"}, {".txt", CONTENT_TYPE_PLAIN.data()}
    };

    static const std::string error_404_html = R"(
<!DOCTYPE html>
<html>
<head><title>404 Not Found</title></head>
<body>
<h1>404 Not Found</h1>
<p>The requested resource was not found on the server.</p>
</body>
</html>
)";
    static const std::string error_403_html = R"(
<!DOCTYPE html>
<html>
<head><title>403 Forbidden</title></head>
<body>
<h1>403 Forbidden</h1>
<p>Access to the requested resource is denied.</p>
</body>
</html>
)";
    static const std::string error_500_html = R"(
<!DOCTYPE html>
<html>
<head><title>500 Internal Server Error</title></head>
<body>
<h1>500 Internal Server Error</h1>
<p>An error occurred while processing your request.</p>
</body>
</html>
)";

    auto handle_static_file = [&]() -> HttpResponse {
        if (request.method != HttpMethod::GET || !request.path.starts_with(s_static_url_prefix)) {
            return {};
        }

        auto it = request.headers.find("Accept");
        bool prefers_html = it != request.headers.end() && it->second.find("text/html") != std::string::npos;
        std::string error_content_type = prefers_html ? "text/html" : CONTENT_TYPE_JSON.data();

        std::string relative_path_str = request.path.substr(s_static_url_prefix.size());
        if (relative_path_str.find("..") != std::string::npos) {
            LOG_ERROR(std::format("Path traversal attempt detected in relative path: {}", relative_path_str));
            return HttpResponse(FORBIDDEN, prefers_html ? error_403_html : json{{"error", "Access denied"}}.dump(),
                                error_content_type);
        }

        auto relative_path = std::filesystem::path(relative_path_str);
        std::error_code ec;
        auto full_path = s_static_files_directory / relative_path;

        auto canonical_path = std::filesystem::weakly_canonical(full_path, ec);
        if (ec) {
            LOG_ERROR(std::format("Failed to resolve path {}: {}", full_path.string(), ec.message()));
            return HttpResponse(INTERNAL_SERVER_ERROR,
                                prefers_html ? error_500_html : json{{"error", "Failed to resolve file"}}.dump(),
                                error_content_type);
        }

        auto canonical_root = std::filesystem::canonical(s_static_files_directory, ec);
        if (ec || !canonical_path.string().starts_with(canonical_root.string())) {
            LOG_ERROR(std::format("Path traversal detected: {} not within {}", canonical_path.string(),
                                  canonical_root.string()));
            return HttpResponse(FORBIDDEN, prefers_html ? error_403_html : json{{"error", "Access denied"}}.dump(),
                                error_content_type);
        }

        if (!std::filesystem::exists(full_path) || std::filesystem::is_directory(full_path)) {
            LOG_DEBUG(std::format("Static file not found or is directory: {}", full_path.string()));
            return HttpResponse(NOT_FOUND, prefers_html ? error_404_html : json{{"error", "File not found"}}.dump(),
                                error_content_type);
        }

        uint64_t file_size = std::filesystem::file_size(full_path, ec);
        if (ec) {
            LOG_ERROR(std::format("Failed to get file size for {}: {}", full_path.string(), ec.message()));
            return HttpResponse(INTERNAL_SERVER_ERROR,
                                prefers_html ? error_500_html : json{{"error", "Failed to read file"}}.dump(),
                                error_content_type);
        }

        auto ext = full_path.extension().string();
        auto it_mime = mime_types.find(ext);
        std::string content_type = it_mime != mime_types.end() ? it_mime->second : "application/octet-stream";

        constexpr uint64_t stream_threshold = DEFAULT_STREAM_THRESHOLD; // 1MB
        HttpResponse response;
        if (request.range) {
            uint64_t start = request.range->start;
            uint64_t end = request.range->end == 0 ? file_size - 1 : std::min(request.range->end, file_size - 1);
            if (start >= file_size || start > end) {
                LOG_DEBUG(std::format("Invalid range request for {}: {}-{}", full_path.string(), start, end));
                return HttpResponse(RANGE_NOT_SATISFIABLE,
                                    prefers_html ? error_500_html : json{{"error", "Invalid range"}}.dump(),
                                    error_content_type);
            }
            response = HttpResponse(PARTIAL_CONTENT, HttpStreamData{full_path, end - start + 1, start}, content_type);
            response.set_header("Content-Range", std::format("bytes {}-{}/{}", start, end, file_size));
            response.set_header("Accept-Ranges", "bytes");
            LOG_DEBUG(std::format("Serving static file (range): {} (type: {}, range: {}-{})",
                                  full_path.string(), content_type, start, end));
        } else if (file_size <= stream_threshold) {
            std::ifstream file(full_path, std::ios::binary);
            if (!file) {
                LOG_ERROR(std::format("Failed to open file: {}", full_path.string()));
                return HttpResponse(INTERNAL_SERVER_ERROR,
                                    prefers_html ? error_500_html : json{{"error", "Failed to read file"}}.dump(),
                                    error_content_type);
            }
            std::string content((std::istreambuf_iterator<char>(file)), {});
            file.close();
            response = HttpResponse(OK, content, content_type);
            response.set_header("Accept-Ranges", "bytes");
            LOG_DEBUG(std::format("Serving static file (string): {} (type: {}, size: {})",
                                  full_path.string(), content_type, file_size));
        } else {
            response = HttpResponse(OK, HttpStreamData{full_path, file_size, 0}, content_type);
            response.set_header("Accept-Ranges", "bytes");
            LOG_DEBUG(std::format("Serving static file (stream): {} (type: {}, size: {})",
                                  full_path.string(), content_type, file_size));
        }
        return response;
    };

    const Route *matched_route = nullptr;
    const auto it = s_routes_by_method.find(request.method);
    if (it != s_routes_by_method.end()) {
        for (auto &route : it->second) {
            if (std::regex_match(request.path, matches, route.pattern)) {
                matched_route = &route;
                break;
            }
        }
    }

    std::function<HttpResponse()> handler = [&] {
        if (auto static_response = handle_static_file(); (std::holds_alternative<std::string>(static_response.body) &&
                                                        !std::get<std::string>(static_response.body).empty()) ||
                                                       std::holds_alternative<HttpStreamData>(static_response.body)) {
            return static_response;
        }
        const auto it_accept = request.headers.find("Accept");
        const bool prefers_html = it_accept != request.headers.end() && contains_ignore_case(it_accept->second, CONTENT_TYPE_HTML.data());
        const std::string content_type = prefers_html ? CONTENT_TYPE_HTML.data() : CONTENT_TYPE_JSON.data();
        if (!matched_route) {
            if (it == s_routes_by_method.end()) {
                return HttpResponse(METHOD_NOT_ALLOWED,
                                    prefers_html ? error_500_html : json{{"error", "Method not allowed"}}.dump(),
                                    content_type);
            }
            return HttpResponse(NOT_FOUND, prefers_html ? error_404_html : json{{"error", "Page not found"}}.dump(),
                                content_type);
        }
        for (size_t i = 0; i < matched_route->param_names.size(); ++i) {
            params[matched_route->param_names[i]] = matches[i + 1];
        }
        return matched_route->handler(request, params, json_body);
    };

    for (auto &s_middleware : std::ranges::reverse_view(s_middlewares)) {
        auto next_handler = handler;
        handler = [&request, json_body, middleware = s_middleware, next_handler]() {
            return middleware(request, json_body, next_handler);
        };
    }

    return handler();
}

void Router::set_static_files_directory(std::string_view fs_path, std::string_view url_prefix) {
    if (fs_path.empty() || url_prefix.empty() || url_prefix[0] != '/') {
        LOG_ERROR(std::format("Invalid static files configuration: fs_path='{}', url_prefix='{}'", fs_path, url_prefix));
        return;
    }

    std::error_code ec;
    auto canonical_path = std::filesystem::canonical(fs_path, ec);
    if (ec || !std::filesystem::is_directory(canonical_path)) {
        LOG_ERROR(std::format("Invalid static files directory: {} ({})", fs_path, ec.message()));
        return;
    }
    s_static_files_directory = canonical_path;
    s_static_url_prefix = url_prefix;
    if (s_static_url_prefix.back() != '/') {
        s_static_url_prefix += '/';
    }
    LOG_INFO(std::format("Static files configured: directory='{}', url_prefix='{}'", s_static_files_directory.string(),
                         s_static_url_prefix));
}