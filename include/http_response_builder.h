//
// Created by fason on 28/07/25.
//

#ifndef HTTP_RESPONSE_BUILDER_H
#define HTTP_RESPONSE_BUILDER_H
#include <string>

#include "http_structs.hpp"

class HttpResponseBuilder {
public:
    static std::string build(const HttpResponse& response);
    static std::string build_headers_only(const HttpResponse& response);
};

#endif //HTTP_RESPONSE_BUILDER_H
