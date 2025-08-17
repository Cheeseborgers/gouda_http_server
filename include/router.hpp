//
// Created by fason on 28/07/25.
//

#ifndef ROUTER_H
#define ROUTER_H

#include <regex>

#include "http_structs.hpp"
#include "http_utils.hpp"
#include "types.hpp"

class Router {
public:
    using RouteHandler =
        std::function<HttpResponse(const HttpRequest &, const HttpRequestParams &, const std::optional<Json> &)>;

    using WebSocketHandler = std::function<std::string(const WebSocketFrame &, ConnectionId, RequestId)>;

    using Middleware = std::function<HttpResponse(const HttpRequest &, const std::optional<Json> &,
                                                  const std::function<HttpResponse()> &)>;
    struct Route {
        HttpMethod method;
        RoutePattern pattern; // Regex pattern
        RouteHandler handler;
        std::vector<std::string> param_names;
        WebSocketHandler websocket_handler; // Optional Websocket handler
    };

    static void add_middleware(Middleware middleware) { s_middlewares.push_back(std::move(middleware)); }

    static void add_route(const HttpMethod method, const std::string &path, RouteHandler handler,
                          WebSocketHandler ws_handler = nullptr)
    {
        std::string regex_path = path;
        std::vector<std::string> param_names;
        const std::regex param_regex(R"(:([a-zA-Z_][a-zA-Z0-9_]*))");
        std::sregex_iterator it(path.begin(), path.end(), param_regex);
        const std::sregex_iterator end;

        while (it != end) {
            param_names.push_back((*it)[1].str());
            ++it;
        }

        regex_path = std::regex_replace(regex_path, param_regex, R"(([^/]+))");
        s_routes_by_method[method].emplace_back(
            Route{method, std::regex("^" + regex_path + "$"), std::move(handler), std::move(param_names), ws_handler});
    }

    // Route HTTP or WebSocket requests
    static HttpResponse route(const HttpRequest &request, const std::optional<Json> &json_body = std::nullopt,
                              ConnectionId connection_id = 0, RequestId request_id = 0);

    static void set_static_files_directory(std::string_view fs_path, std::string_view url_prefix = "/static/");

    static bool client_prefers_html(const HttpRequest &request);

    static WebSocketHandler get_websocket_handler(const HttpRequest& request);

private:
    static const Route *match_route(const HttpRequest &request, std::smatch &matches);
    static bool has_method_routes(HttpMethod method);
    static bool is_valid_static_response(const HttpResponse &resp);

    [[nodiscard]] static HttpResponse handle_static_file(const HttpRequest &request, ConnectionId connection_id,
                                                         RequestId request_id);

private:
    static std::filesystem::path s_static_files_directory;
    static std::string s_static_url_prefix;
    static std::unordered_map<HttpMethod, std::vector<Route>> s_routes_by_method;
    static std::vector<Middleware> s_middlewares;
};

#endif // ROUTER_H
