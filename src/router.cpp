#include "router.hpp"
#include "http_utils.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>

#include "file_cache.hpp"
#include "logger.hpp"

std::filesystem::path Router::s_static_files_directory{"static"};
std::string Router::s_static_url_prefix{"/assets/"};
std::unordered_map<HttpMethod, std::vector<Router::Route>> Router::s_routes_by_method;
std::vector<Router::Middleware> Router::s_middlewares;

HttpResponse Router::route(const HttpRequest &request, const std::optional<Json> &json_body)
{
    using enum HttpStatusCode;
    HttpRequestParams params;
    std::smatch matches;

    const Route *matched_route = match_route(request, matches);

    std::function<HttpResponse()> handler = [&] {
        if (HttpResponse static_response = handle_static_file(request); is_valid_static_response(static_response)) {
            return static_response;
        }

        const bool prefers_html = client_prefers_html(request);
        const std::string content_type = prefers_html ? "text/html; charset=utf-8" : "application/json";

        if (!matched_route) {
            if (!has_method_routes(request.method)) {
                return HttpResponse(METHOD_NOT_ALLOWED,
                                    prefers_html ? ERROR_500_HTML.data() : Json{{"error", "Method not allowed"}}.dump(),
                                    content_type);
            }
            return HttpResponse(NOT_FOUND,
                                prefers_html ? ERROR_404_HTML.data() : Json{{"error", "Page not found"}}.dump(),
                                content_type);
        }

        for (size_t i = 0; i < matched_route->param_names.size(); ++i) {
            params[matched_route->param_names[i]] = matches[i + 1];
        }

        return matched_route->handler(request, params, json_body);
    };

    for (auto &middleware : std::ranges::reverse_view(s_middlewares)) {
        auto next = handler;
        handler = [&request, json_body, middleware, next]() { return middleware(request, json_body, next); };
    }

    return handler();
}

void Router::set_static_files_directory(std::string_view fs_path, std::string_view url_prefix)
{
    if (fs_path.empty() || url_prefix.empty() || url_prefix[0] != '/') {
        LOG_ERROR(
            std::format("Invalid static files configuration: fs_path='{}', url_prefix='{}'", fs_path, url_prefix));
        return;
    }

    std::error_code ec;
    const auto canonical_path = std::filesystem::canonical(fs_path, ec);
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

bool Router::client_prefers_html(const HttpRequest &request)
{
    const auto it = request.headers.find("Accept");
    return it != request.headers.end() && contains_ignore_case(it->second, "text/html");
}


// Private -------------------------
const Router::Route *Router::match_route(const HttpRequest &request, std::smatch &matches)
{
    const auto it = s_routes_by_method.find(request.method);
    if (it == s_routes_by_method.end()) {
        return nullptr;
    }

    for (auto &route : it->second) {
        if (std::regex_match(request.path, matches, route.pattern)) {
            return &route;
        }
    }

    return nullptr;
}

bool Router::has_method_routes(const HttpMethod method) { return s_routes_by_method.contains(method); }

bool Router::is_valid_static_response(const HttpResponse &resp)
{
    return (std::holds_alternative<std::string>(resp.body) && !std::get<std::string>(resp.body).empty()) ||
           std::holds_alternative<HttpStreamData>(resp.body);
}

[[nodiscard]] HttpResponse Router::handle_static_file(const HttpRequest &request)
{
    using enum HttpStatusCode;

    static const std::unordered_map<std::string, std::string> mime_types = {{".html", "text/html"},
                                                                            {".css", "text/css"},
                                                                            {".js", "application/javascript"},
                                                                            {".png", "image/png"},
                                                                            {".jpg", "image/jpeg"},
                                                                            {".jpeg", "image/jpeg"},
                                                                            {".gif", "image/gif"},
                                                                            {".svg", "image/svg+xml"},
                                                                            {".json", "application/json"},
                                                                            {".ico", "image/x-icon"},
                                                                            {".txt", CONTENT_TYPE_PLAIN.data()}};

    if (request.method != HttpMethod::GET || !request.path.starts_with(s_static_url_prefix)) {
        return {};
    }

    const bool prefers_html = client_prefers_html(request);
    const std::string_view error_content_type = prefers_html ? "text/html; charset=utf-8" : "application/json";

    std::string relative_path_str = request.path.substr(s_static_url_prefix.size());
    if (relative_path_str.find("..") != std::string::npos) {
        LOG_ERROR(std::format("Path traversal attempt detected in relative path: {}", relative_path_str));
        return HttpResponse(FORBIDDEN, prefers_html ? ERROR_403_HTML.data() : Json{{"error", "Access denied"}}.dump(),
                            error_content_type);
    }

    auto relative_path = std::filesystem::path(relative_path_str);
    std::error_code ec;
    auto full_path = s_static_files_directory / relative_path;

    auto canonical_path = std::filesystem::weakly_canonical(full_path, ec);
    if (ec) {
        LOG_ERROR(std::format("Failed to resolve path {}: {}", full_path.string(), ec.message()));
        return HttpResponse(INTERNAL_SERVER_ERROR,
                            prefers_html ? ERROR_500_HTML.data() : Json{{"error", "Failed to resolve file"}}.dump(),
                            error_content_type);
    }

    auto canonical_root = std::filesystem::canonical(s_static_files_directory, ec);
    if (ec || !canonical_path.string().starts_with(canonical_root.string())) {
        LOG_ERROR(
            std::format("Path traversal detected: {} not within {}", canonical_path.string(), canonical_root.string()));
        return HttpResponse(FORBIDDEN, prefers_html ? ERROR_403_HTML.data() : Json{{"error", "Access denied"}}.dump(),
                            error_content_type);
    }

    if (!std::filesystem::exists(full_path) || std::filesystem::is_directory(full_path)) {
        LOG_DEBUG(std::format("Static file not found or is directory: {}", full_path.string()));
        return HttpResponse(NOT_FOUND, prefers_html ? ERROR_404_HTML.data() : Json{{"error", "File not found"}}.dump(),
                            error_content_type);
    }

    uint64_t file_size = std::filesystem::file_size(full_path, ec);
    if (ec) {
        LOG_ERROR(std::format("Failed to get file size for {}: {}", full_path.string(), ec.message()));
        return HttpResponse(INTERNAL_SERVER_ERROR,
                            prefers_html ? ERROR_500_HTML.data() : Json{{"error", "Failed to read file"}}.dump(),
                            error_content_type);
    }

    auto ext = full_path.extension().string();
    auto it_mime = mime_types.find(ext);
    std::string content_type = it_mime != mime_types.end() ? it_mime->second : "application/octet-stream";

    auto last_modified = std::filesystem::last_write_time(full_path, ec);
    if (ec) {
        LOG_ERROR(std::format("Failed to get last modified time for {}: {}", full_path.string(), ec.message()));
        return HttpResponse(INTERNAL_SERVER_ERROR,
                            prefers_html ? ERROR_500_HTML.data()
                                         : Json{{"error", "Failed to read file metadata"}}.dump(),
                            error_content_type);
    }

    constexpr uint64_t stream_threshold = DEFAULT_STREAM_THRESHOLD; // 1MB
    HttpResponse response;

    FileCache::FileCacheEntry cache_entry;
    if (file_size <= stream_threshold && FileCache::get(full_path.string(), cache_entry, last_modified)) {
        if (request.range) {
            uint64_t start = request.range->start;
            uint64_t end = request.range->end == 0 ? file_size - 1 : request.range->end;
            if (start >= file_size || start > end || end >= file_size) {
                LOG_DEBUG(std::format("Invalid range request for {}: {}-{}, file_size: {}", full_path.string(), start,
                                      end, file_size));
                HttpResponse resp(RANGE_NOT_SATISFIABLE,
                                  prefers_html ? ERROR_416_HTML.data() : Json{{"error", "Invalid range"}}.dump(),
                                  error_content_type);
                resp.set_header("Content-Range", std::format("bytes */{}", file_size));
                return resp;
            }
            std::string content = cache_entry.content.substr(start, end - start + 1);
            response = HttpResponse(PARTIAL_CONTENT, content, content_type);
            response.set_header("Content-Range", std::format("bytes {}-{}/{}", start, end, file_size));
            response.set_header("Accept-Ranges", "bytes");
            response.set_header("Last-Modified", format_last_modified(last_modified));
            LOG_DEBUG(std::format("Serving cached file (range): {} (type: {}, range: {}-{})", full_path.string(),
                                  content_type, start, end));
        }
        else {
            response = HttpResponse(OK, cache_entry.content, content_type);
            response.set_header("Accept-Ranges", "bytes");
            response.set_header("Last-Modified", format_last_modified(last_modified));
            LOG_DEBUG(std::format("Serving cached file: {} (type: {}, size: {})", full_path.string(), content_type,
                                  file_size));
        }
    }
    else {
        if (file_size <= stream_threshold) {
            std::ifstream file(full_path, std::ios::binary);
            if (!file) {
                LOG_ERROR(std::format("Failed to open file: {}", full_path.string()));
                return HttpResponse(INTERNAL_SERVER_ERROR,
                                    prefers_html ? ERROR_500_HTML.data()
                                                 : Json{{"error", "Failed to read file"}}.dump(),
                                    error_content_type);
            }
            std::string content((std::istreambuf_iterator<char>(file)), {});
            file.close();
            FileCache::put(full_path.string(), content, last_modified);
            if (request.range) {
                uint64_t start = request.range->start;
                uint64_t end = request.range->end == 0 ? file_size - 1 : request.range->end;
                if (start >= file_size || start > end || end >= file_size) {
                    LOG_DEBUG(std::format("Invalid range request for {}: {}-{}, file_size: {}", full_path.string(),
                                          start, end, file_size));
                    HttpResponse resp(RANGE_NOT_SATISFIABLE,
                                      prefers_html ? ERROR_416_HTML.data() : Json{{"error", "Invalid range"}}.dump(),
                                      error_content_type);
                    resp.set_header("Content-Range", std::format("bytes */{}", file_size));
                    return resp;
                }
                std::string range_content = content.substr(start, end - start + 1);
                response = HttpResponse(PARTIAL_CONTENT, range_content, content_type);
                response.set_header("Content-Range", std::format("bytes {}-{}/{}", start, end, file_size));
                response.set_header("Accept-Ranges", "bytes");
                response.set_header("Last-Modified", format_last_modified(last_modified));
                LOG_DEBUG(std::format("Serving static file (range): {} (type: {}, range: {}-{})", full_path.string(),
                                      content_type, start, end));
            }
            else {
                response = HttpResponse(OK, content, content_type);
                response.set_header("Accept-Ranges", "bytes");
                response.set_header("Last-Modified", format_last_modified(last_modified));
                LOG_DEBUG(std::format("Serving static file: {} (type: {}, size: {})", full_path.string(), content_type,
                                      file_size));
            }
        }
        else {
            if (request.range) {
                uint64_t start = request.range->start;
                uint64_t end = request.range->end == 0 ? file_size - 1 : request.range->end;
                if (start >= file_size || start > end || end >= file_size) {
                    LOG_DEBUG(std::format("Invalid range request for {}: {}-{}, file_size: {}", full_path.string(),
                                          start, end, file_size));
                    HttpResponse resp(RANGE_NOT_SATISFIABLE,
                                      prefers_html ? ERROR_416_HTML.data() : Json{{"error", "Invalid range"}}.dump(),
                                      error_content_type);
                    resp.set_header("Content-Range", std::format("bytes */{}", file_size));
                    return resp;
                }
                response =
                    HttpResponse(PARTIAL_CONTENT, HttpStreamData{full_path, end - start + 1, start}, content_type);
                response.set_header("Content-Range", std::format("bytes {}-{}/{}", start, end, file_size));
                response.set_header("Accept-Ranges", "bytes");
                response.set_header("Last-Modified", format_last_modified(last_modified));
                LOG_DEBUG(std::format("Serving static file (range, stream): {} (type: {}, range: {}-{})",
                                      full_path.string(), content_type, start, end));
            }
            else {
                response = HttpResponse(OK, HttpStreamData{full_path, file_size, 0}, content_type);
                response.set_header("Accept-Ranges", "bytes");
                response.set_header("Last-Modified", format_last_modified(last_modified));
                LOG_DEBUG(std::format("Serving static file (stream): {} (type: {}, size: {})", full_path.string(),
                                      content_type, file_size));
            }
        }
    }

    response.set_header("Cache-Control", "max-age=3600");
    return response;
}