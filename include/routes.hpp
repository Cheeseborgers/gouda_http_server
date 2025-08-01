#ifndef ROUTES_HPP
#define ROUTES_HPP

#include "http_status.hpp"
#include "http_structs.hpp"
#include "http_utils.hpp"
#include "logger.hpp"
#include "router.hpp"
#include <nlohmann/json.hpp>

inline void setup_routes() {
    using enum HttpStatusCode;
    using enum HttpMethod;

    // Logging middleware
    Router::add_middleware([](const HttpRequest& request, const std::optional<json>&, const std::function<HttpResponse()>& next) {
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
            return HttpResponse(INTERNAL_SERVER_ERROR, json{{"error", "Internal server error"}}.dump(), CONTENT_TYPE_JSON.data());
        }
        return response;
    });

    // Authentication middleware
    Router::add_middleware([](const HttpRequest& request, const std::optional<json>&, const std::function<HttpResponse()>& next) {
        if (request.path.starts_with("/user/")) {
            const auto it = request.headers.find("Authorization");
            if (it == request.headers.end() || it->second != "Bearer dummy_token") {
                return make_response(UNAUTHORIZED, CONTENT_TYPE_JSON, json{{"error", "Unauthorized"}}.dump());
            }
        }
        return next();
    });

    // Static routes
    Router::add_route(GET, "/", [](const HttpRequest&, const HttpRequestParams&, const std::optional<json>&) {
        return make_response(OK, CONTENT_TYPE_PLAIN, "Welcome to the home page!");
    });
    Router::add_route(GET, "/about", [](const HttpRequest&, const HttpRequestParams&, const std::optional<json>&) {
        return make_response(OK, CONTENT_TYPE_PLAIN, "About page: This is a simple server.");
    });
    Router::add_route(POST, "/echo", [](const HttpRequest& request, const HttpRequestParams&, const std::optional<json>&) {
        return make_response(OK, CONTENT_TYPE_PLAIN, request.body);
    });
    Router::add_route(POST, "/json", [](const HttpRequest& request, const HttpRequestParams&, const std::optional<json>& json_body) {
        if (!json_body) {
            return make_response(BAD_REQUEST, CONTENT_TYPE_JSON, json{{"error", "Missing or invalid JSON body"}}.dump());
        }
        try {
            std::string name = json_body->value("name", "Unknown");
            json response_json = {
                {"status", "received"},
                {"name", name},
                {"size", request.body.size()}
            };
            return make_response(OK, CONTENT_TYPE_JSON, response_json.dump());
        } catch (const json::exception& e) {
            LOG_ERROR(std::format("Router: JSON processing error: {}", e.what()));
            return make_response(BAD_REQUEST, CONTENT_TYPE_JSON, json{{"error", "Invalid JSON structure"}}.dump());
        }
    });

    // Query parameter test route
    Router::add_route(GET, "/query", [](const HttpRequest& request, const HttpRequestParams&, const std::optional<json>&) {
       json response_body;
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
    Router::add_route(POST, "/form", [](const HttpRequest& request, const HttpRequestParams&, const std::optional<json>&) {
        if (request.form_params.empty()) {
            return make_response(BAD_REQUEST, CONTENT_TYPE_JSON, json{{"error", "No form data or invalid Content-Type"}}.dump());
        }
        json response_body;
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
    Router::add_route(GET, "/user/:id", [](const HttpRequest&, const HttpRequestParams& params, const std::optional<json>&) {
        json response_json = {
            {"id", params.at("id")},
            {"message", "User found"}
        };
        return make_response(OK, CONTENT_TYPE_JSON, response_json.dump());
    });
    Router::add_route(PUT, "/user/:id", [](const HttpRequest&, const HttpRequestParams& params, const std::optional<json>& json_body) {
        if (!json_body) {
            return make_response(BAD_REQUEST, CONTENT_TYPE_JSON, json{{"error", "Missing JSON body"}}.dump());
        }
        json response_json = {
            {"id", params.at("id")},
            {"message", "User updated"},
            {"data", *json_body}
        };
        return make_response(OK, CONTENT_TYPE_JSON, response_json.dump());
    });
    Router::add_route(DELETE, "/user/:id", [](const HttpRequest&, const HttpRequestParams& params, const std::optional<json>&) {
        json response_json = {
            {"id", params.at("id")},
            {"message", "User deleted"}
        };
        return make_response(OK, CONTENT_TYPE_JSON, response_json.dump());
    });
    Router::add_route(PATCH, "/user/:id", [](const HttpRequest&, const HttpRequestParams& params, const std::optional<json>& json_body) {
        if (!json_body) {
            return make_response(BAD_REQUEST, CONTENT_TYPE_JSON, json{{"error", "Missing JSON body"}}.dump());
        }
        json response_json = {
            {"id", params.at("id")},
            {"message", "User patched"},
            {"data", *json_body}
        };
        return make_response(OK, CONTENT_TYPE_JSON, response_json.dump());
    });
}

#endif // ROUTES_HPP