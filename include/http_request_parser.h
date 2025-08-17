//
// Created by fason on 28/07/25.
//

#ifndef HTTP_REQUEST_PARSER_H
#define HTTP_REQUEST_PARSER_H
#include <optional>
#include <string_view>

#include "http_structs.hpp"
#include "types.hpp"

class HttpRequestParser {
public:
    // Parse an HTTP request, with optional debug logging and request ID
    static std::optional<HttpRequest> parse(std::string_view request_view, bool debug = false, RequestId request_id = 0);
    static std::optional<WebSocketFrame> parse_websocket_frame(std::string_view frame_data, bool debug, RequestId request_id);
    static std::string compute_websocket_accept(const std::string& key);
};

#endif //HTTP_REQUEST_PARSER_H
