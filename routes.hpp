#ifndef ROUTES_HPP
#define ROUTES_HPP

#include <nlohmann/json.hpp>

#include "include/http_status.hpp"
#include "include/http_structs.hpp"
#include "include/http_utils.hpp"
#include "include/logger.hpp"
#include "include/router.hpp"
#include "file_cache.hpp"

inline void setup_routes() {
    using enum HttpStatusCode;
    using enum HttpMethod;

    // Logging middleware
    Router::add_middleware([](const HttpRequest& request, const std::optional<Json>&, const std::function<HttpResponse()>& next) {
        LOG_INFO(std::format("Request: {} {}", method_to_string(request.method), request.path));
        auto response = next();
        try {
            std::visit([&](const auto& body) {
                if constexpr (std::is_same_v<std::decay_t<decltype(body)>, std::string>) {
                    LOG_INFO(std::format("Response: {} ({} bytes)", static_cast<int>(response.status_code), body.size()));
                } else if constexpr (std::is_same_v<std::decay_t<decltype(body)>, HttpStreamData>) {
                    LOG_INFO(std::format("Response: {} ({} bytes, streamed)", static_cast<int>(response.status_code), body.file_size));
                } else {
                    LOG_ERROR(std::format("Response: {} (unknown body type)", static_cast<int>(response.status_code)));
                }
            }, response.body);
        } catch (const std::bad_variant_access& e) {
            LOG_ERROR(std::format("Middleware: std::variant access error: {}", e.what()));
            return HttpResponse(INTERNAL_SERVER_ERROR, Json{{"error", "Internal server error"}}.dump(), CONTENT_TYPE_JSON.data());
        }
        return response;
    });

    // Authentication middleware
    Router::add_middleware([](const HttpRequest& request, const std::optional<Json>&, const std::function<HttpResponse()>& next) {
        if (request.path.starts_with("/user/")) {
            const auto it = request.headers.find("Authorization");
            if (it == request.headers.end() || it->second != "Bearer dummy_token") {
                return make_response(UNAUTHORIZED, CONTENT_TYPE_JSON, Json{{"error", "Unauthorized"}}.dump());
            }
        }
        return next();
    });

    // Static routes
    Router::add_route(GET, "/", [](const HttpRequest&, const HttpRequestParams&, const std::optional<Json>&) {
        return make_response(OK, CONTENT_TYPE_PLAIN, "Welcome to the home page!");
    });
    Router::add_route(GET, "/favicon.ico", [](const HttpRequest& request, const HttpRequestParams&, const std::optional<Json>&) {
        const bool prefers_html = Router::client_prefers_html(request);
        const std::string_view content_type = prefers_html ? "text/html; charset=utf-8" : CONTENT_TYPE_JSON.data();
        std::filesystem::path favicon_path = "static/favicon.ico";
        std::error_code ec;

        if (!std::filesystem::exists(favicon_path) || std::filesystem::is_directory(favicon_path)) {
            LOG_DEBUG(std::format("Favicon not found or is directory: {}", favicon_path.string()));
            return HttpResponse(NOT_FOUND,
                                prefers_html ? ERROR_404_HTML.data() : Json{{"error", "Favicon not found"}}.dump(),
                                content_type);
        }

        uint64_t file_size = std::filesystem::file_size(favicon_path, ec);
        if (ec) {
            LOG_ERROR(std::format("Failed to get file size for favicon: {} ({})", favicon_path.string(), ec.message()));
            return HttpResponse(INTERNAL_SERVER_ERROR,
                                prefers_html ? ERROR_500_HTML.data() : Json{{"error", "Failed to read favicon"}}.dump(),
                                content_type);
        }

        auto last_modified = std::filesystem::last_write_time(favicon_path, ec);
        if (ec) {
            LOG_ERROR(std::format("Failed to get last modified time for favicon: {} ({})", favicon_path.string(), ec.message()));
            return HttpResponse(INTERNAL_SERVER_ERROR,
                                prefers_html ? ERROR_500_HTML.data() : Json{{"error", "Failed to read favicon metadata"}}.dump(),
                                content_type);
        }

        FileCache::FileCacheEntry cache_entry;
        if (FileCache::get(favicon_path.string(), cache_entry, last_modified)) {
            HttpResponse response(OK, cache_entry.content, "image/x-icon");
            response.set_header("Cache-Control", "max-age=3600");
            response.set_header("Last-Modified", format_last_modified(last_modified));
            response.set_header("Accept-Ranges", "bytes");
            if (request.range) {
                uint64_t start = request.range->start;
                uint64_t end = request.range->end == 0 ? file_size - 1 : request.range->end;
                if (start >= file_size || start > end || end >= file_size) {
                    LOG_DEBUG(std::format("Invalid range request for favicon: {}-{}, file_size: {}", start, end, file_size));
                    HttpResponse resp(RANGE_NOT_SATISFIABLE,
                                      prefers_html ? ERROR_416_HTML.data() : Json{{"error", "Invalid range"}}.dump(),
                                      content_type);
                    resp.set_header("Content-Range", std::format("bytes */{}", file_size));
                    return resp;
                }
                std::string range_content = cache_entry.content.substr(start, end - start + 1);
                response = HttpResponse(PARTIAL_CONTENT, range_content, "image/x-icon");
                response.set_header("Content-Range", std::format("bytes {}-{}/{}", start, end, file_size));
                response.set_header("Accept-Ranges", "bytes");
                response.set_header("Last-Modified", format_last_modified(last_modified));
                LOG_DEBUG(std::format("Serving cached favicon (range): {} (range: {}-{})", favicon_path.string(), start, end));
            } else {
                LOG_DEBUG(std::format("Serving cached favicon: {} (size: {})", favicon_path.string(), cache_entry.content.size()));
            }
            return response;
        }

        std::ifstream file(favicon_path, std::ios::binary);
        if (!file) {
            LOG_ERROR(std::format("Failed to open favicon: {}", favicon_path.string()));
            return HttpResponse(INTERNAL_SERVER_ERROR,
                                prefers_html ? ERROR_500_HTML.data() : Json{{"error", "Failed to read favicon"}}.dump(),
                                content_type);
        }
        std::string content((std::istreambuf_iterator<char>(file)), {});
        file.close();
        FileCache::put(favicon_path.string(), content, last_modified);

        HttpResponse response(OK, content, "image/x-icon");
        response.set_header("Cache-Control", "max-age=3600");
        response.set_header("Last-Modified", format_last_modified(last_modified));
        response.set_header("Accept-Ranges", "bytes");
        if (request.range) {
            uint64_t start = request.range->start;
            uint64_t end = request.range->end == 0 ? file_size - 1 : request.range->end;
            if (start >= file_size || start > end || end >= file_size) {
                LOG_DEBUG(std::format("Invalid range request for favicon: {}-{}, file_size: {}", start, end, file_size));
                HttpResponse resp(RANGE_NOT_SATISFIABLE,
                                  prefers_html ? ERROR_416_HTML.data() : Json{{"error", "Invalid range"}}.dump(),
                                  content_type);
                resp.set_header("Content-Range", std::format("bytes */{}", file_size));
                return resp;
            }
            std::string range_content = content.substr(start, end - start + 1);
            response = HttpResponse(PARTIAL_CONTENT, range_content, "image/x-icon");
            response.set_header("Content-Range", std::format("bytes {}-{}/{}", start, end, file_size));
            response.set_header("Accept-Ranges", "bytes");
            response.set_header("Last-Modified", format_last_modified(last_modified));
            LOG_DEBUG(std::format("Serving favicon (range): {} (range: {}-{})", favicon_path.string(), start, end));
        } else {
            LOG_DEBUG(std::format("Serving favicon: {} (size: {})", favicon_path.string(), file_size));
        }
        return response;
    });
    Router::add_route(GET, "/about", [](const HttpRequest&, const HttpRequestParams&, const std::optional<Json>&) {
        return make_response(OK, CONTENT_TYPE_PLAIN, "About page: This is a simple server.");
    });
    Router::add_route(POST, "/echo", [](const HttpRequest& request, const HttpRequestParams&, const std::optional<Json>&) {
        return make_response(OK, CONTENT_TYPE_PLAIN, request.body);
    });
    Router::add_route(POST, "/json", [](const HttpRequest& request, const HttpRequestParams&, const std::optional<Json>& json_body) {
        if (!json_body) {
            return make_response(BAD_REQUEST, CONTENT_TYPE_JSON, Json{{"error", "Missing or invalid JSON body"}}.dump());
        }
        try {
            std::string name = json_body->value("name", "Unknown");
            Json response_json = {
                {"status", "received"},
                {"name", name},
                {"size", request.body.size()}
            };
            return make_response(OK, CONTENT_TYPE_JSON, response_json.dump());
        } catch (const Json::exception& e) {
            LOG_ERROR(std::format("Router: JSON processing error: {}", e.what()));
            return make_response(BAD_REQUEST, CONTENT_TYPE_JSON, Json{{"error", "Invalid JSON structure"}}.dump());
        }
    });

    // Query parameter test route
    Router::add_route(GET, "/query", [](const HttpRequest& request, const HttpRequestParams&, const std::optional<Json>&) {
       Json response_body;
       for (const auto& [key, values] : request.query_params) {
           if (values.size() == 1) {
               response_body[key] = values[0]; // Single value as string
           } else {
               response_body[key] = values; // Multiple values as array
           }
       }
       return make_response(OK, CONTENT_TYPE_JSON, response_body.dump());
   });

    // Form data test route
    Router::add_route(POST, "/form", [](const HttpRequest& request, const HttpRequestParams&, const std::optional<Json>&) {
        if (request.form_params.empty()) {
            return make_response(BAD_REQUEST, CONTENT_TYPE_JSON, Json{{"error", "No form data or invalid Content-Type"}}.dump());
        }
        Json response_body;
        for (const auto& [key, values] : request.form_params) {
            if (values.size() == 1) {
                response_body[key] = values[0];
            } else {
                response_body[key] = values;
            }
        }
        return make_response(OK, CONTENT_TYPE_JSON, response_body.dump());
    });

    // Dynamic routes
    Router::add_route(GET, "/user/:id", [](const HttpRequest&, const HttpRequestParams& params, const std::optional<Json>&) {
        Json response_json = {
            {"id", params.at("id")},
            {"message", "User found"}
        };
        return make_response(OK, CONTENT_TYPE_JSON, response_json.dump());
    });
    Router::add_route(PUT, "/user/:id", [](const HttpRequest&, const HttpRequestParams& params, const std::optional<Json>& json_body) {
        if (!json_body) {
            return make_response(BAD_REQUEST, CONTENT_TYPE_JSON, Json{{"error", "Missing JSON body"}}.dump());
        }
        Json response_json = {
            {"id", params.at("id")},
            {"message", "User updated"},
            {"data", *json_body}
        };
        return make_response(OK, CONTENT_TYPE_JSON, response_json.dump());
    });
    Router::add_route(DELETE, "/user/:id", [](const HttpRequest&, const HttpRequestParams& params, const std::optional<Json>&) {
        Json response_json = {
            {"id", params.at("id")},
            {"message", "User deleted"}
        };
        return make_response(OK, CONTENT_TYPE_JSON, response_json.dump());
    });
    Router::add_route(PATCH, "/user/:id", [](const HttpRequest&, const HttpRequestParams& params, const std::optional<Json>& json_body) {
        if (!json_body) {
            return make_response(BAD_REQUEST, CONTENT_TYPE_JSON, Json{{"error", "Missing JSON body"}}.dump());
        }
        Json response_json = {
            {"id", params.at("id")},
            {"message", "User patched"},
            {"data", *json_body}
        };
        return make_response(OK, CONTENT_TYPE_JSON, response_json.dump());
    });
}

#endif // ROUTES_HPP