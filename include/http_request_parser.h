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
};

#endif //HTTP_REQUEST_PARSER_H
