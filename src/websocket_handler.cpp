//
// Created by fason on 17/08/25.
//

#include "websocket_handler.hpp"

#include "http_request_parser.h"
#include "logger.hpp"
#include "socket_wrapper.h"

WebSocketHandler::WebSocketHandler(HandlerFunc handler, const int timeout_ms)
    : m_handler{std::move(handler)}, m_timeout_ms{timeout_ms}
{
}

bool WebSocketHandler::process_frame(const std::string_view data, ConnectionId conn_id, RequestId req_id,
                                     const Socket &sock)
{
    auto frame = HttpRequestParser::parse_websocket_frame(data, true, req_id);
    if (!frame) {
        LOG_DEBUG(std::format("WebSocket[Conn:{}][Req:{}]: Invalid or partial frame", conn_id, req_id));
        return true; // Continue waiting for more data
    }
    if (frame->opcode == 0x8) { // Close frame
        LOG_DEBUG(std::format("WebSocket[Conn:{}][Req:{}]: Received close frame", conn_id, req_id));
        const std::string close_frame = "\x88\x00";
        if (sock.send(close_frame.data(), close_frame.size()) <= 0) {
            LOG_ERROR("Failed to send close frame");
        }

        return false;
    }
    if (frame->opcode == 0x9) { // Ping frame
        LOG_DEBUG(std::format("WebSocket[Conn:{}][Req:{}]: Received ping frame", conn_id, req_id));
        std::string pong_frame = "\x8A";
        encode_payload_length(frame->payload_length, pong_frame);
        pong_frame += frame->payload;
        if (sock.send(pong_frame.data(), pong_frame.size()) <= 0) {
            LOG_ERROR("Failed to send pong frame");
            // TODO: Return false here?
        }

        return true;
    }
    if (frame->opcode == 0x1 || frame->opcode == 0x2) { // Text or binary frame
        LOG_DEBUG(std::format("WebSocket[Conn:{}][Req:{}]: Received {} frame (length: {})", conn_id, req_id,
                              frame->opcode == 0x1 ? "text" : "binary", frame->payload_length));
        const std::string response = m_handler(*frame, conn_id, req_id);
        std::string response_frame;
        response_frame += static_cast<char>(0x80 | frame->opcode); // FIN + same opcode
        encode_payload_length(response.length(), response_frame);
        response_frame += response;
        if (sock.send(response_frame.data(), response_frame.size()) <= 0) {
            LOG_ERROR("Failed to send response");
            // TODO: Return false here?
        }

        LOG_DEBUG(std::format("WebSocket[Conn:{}][Req:{}]: Sent {} frame (length: {})", conn_id, req_id,
                              frame->opcode == 0x1 ? "text" : "binary", response.length()));
        return true;
    }
    return true;
}
