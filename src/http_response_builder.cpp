//
// Created by fason on 28/07/25.
//

#include "http_response_builder.h"

#include <format>
#include <sstream>

#include "logger.hpp"

std::string HttpResponseBuilder::build(const HttpResponse& response) {
    std::string result;
    result.reserve(MAX_RESPONSE_SIZE);

    result += std::string(http_version_to_string_view(HttpVersion::HTTP_1_1)) + " ";
    result += std::to_string(static_cast<u16>(response.status_code)) + " ";
    result += status_code_to_string_view(response.status_code);
    result += "\r\n";

    for (const auto& [key, value] : response.headers) {
        result += key + ": " + value + "\r\n";
    }

    std::visit([&](const auto& body) {
        if constexpr (std::is_same_v<std::decay_t<decltype(body)>, std::string>) {
            result += "Content-Length: " + std::to_string(body.size()) + "\r\n";
            result += "\r\n";
            result += body;
        } else if constexpr (std::is_same_v<std::decay_t<decltype(body)>, HttpStreamData>) {
            result += "Content-Length: " + std::to_string(body.file_size) + "\r\n";
            result += "\r\n";
        } else if constexpr (std::is_same_v<std::decay_t<decltype(body)>, WebSocketResponseData>) {
            result += "Sec-WebSocket-Accept: " + body.accept_key + "\r\n";
            if (body.protocol) {
                result += "Sec-WebSocket-Protocol: " + *body.protocol + "\r\n";
            }
            if (body.extensions) {
                result += "Sec-WebSocket-Extensions: " + *body.extensions + "\r\n";
            }
            result += "\r\n";
        }
    }, response.body);

    return result;
}

std::string HttpResponseBuilder::build_headers_only(const HttpResponse& response) {
    std::string result;
    result.reserve(MAX_RESPONSE_SIZE);

    result += std::string(http_version_to_string_view(HttpVersion::HTTP_1_1)) + " ";
    result += std::to_string(static_cast<u16>(response.status_code)) + " ";
    result += status_code_to_string_view(response.status_code);
    result += "\r\n";

    for (const auto& [key, value] : response.headers) {
        result += key + ": " + value + "\r\n";
    }
    result += "\r\n";

    return result;
}

std::string HttpResponseBuilder::build_websocket_frame(const WebSocketFrame& frame, bool debug, RequestId request_id) {
    std::string result;
    result.reserve(14 + frame.payload_length); // Max header size + payload

    // First byte: FIN and opcode
    uint8_t first_byte = (frame.fin ? 0x80 : 0x00) | (frame.opcode & 0x0F);
    result += static_cast<char>(first_byte);

    // Second byte: Mask and payload length
    uint8_t second_byte = frame.mask ? 0x80 : 0x00;
    if (frame.payload_length <= 125) {
        second_byte |= static_cast<uint8_t>(frame.payload_length);
        result += static_cast<char>(second_byte);
    } else if (frame.payload_length <= 0xFFFF) {
        second_byte |= 126;
        result += static_cast<char>(second_byte);
        result += static_cast<char>((frame.payload_length >> 8) & 0xFF);
        result += static_cast<char>(frame.payload_length & 0xFF);
    } else {
        second_byte |= 127;
        result += static_cast<char>(second_byte);
        for (int i = 7; i >= 0; --i) {
            result += static_cast<char>((frame.payload_length >> (i * 8)) & 0xFF);
        }
    }

    // Masking key (server-to-client frames typically don’t mask)
    if (frame.mask && frame.masking_key) {
        for (const auto& byte : *frame.masking_key) {
            result += static_cast<char>(byte);
        }
    }

    // Payload (unmasked for server-to-client)
    result += frame.payload;

    if (debug) {
        LOG_DEBUG(std::format("Request[{}]: Built WebSocket frame: FIN={}, Opcode={}, Mask={}, Payload Length={}, Payload={}",
                              request_id, frame.fin, frame.opcode, frame.mask, frame.payload_length, frame.payload));
    }

    return result;
}
