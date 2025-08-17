//
// Created by fason on 17/08/25.
//

#ifndef WEBSOCKET_HANDLER_HPP
#define WEBSOCKET_HANDLER_HPP

#include "http_structs.hpp"

class Socket;

class WebSocketHandler {
public:
    using HandlerFunc = std::function<std::string(const WebSocketFrame&, ConnectionId, RequestId)>;

    WebSocketHandler(HandlerFunc handler, int timeout_ms);

    bool process_frame(std::string_view data, ConnectionId conn_id, RequestId req_id, const Socket& sock);

private:
    void encode_payload_length(const u64 length, std::string& frame) {
        constexpr char PAYLOAD_16 = 126; // 0x7E
        constexpr char PAYLOAD_64 = 127; // 0x7F

        if (length <= 125) {
            frame += static_cast<char>(length);
        } else if (length <= 0xFFFF) {
            frame += PAYLOAD_16;
            frame += static_cast<char>(length >> 8 & 0xFF);
            frame += static_cast<char>(length & 0xFF);
        } else {
            frame += PAYLOAD_64;
            for (int i = 7; i >= 0; --i) {
                frame += static_cast<char>(length >> (i * 8) & 0xFF);
            }
        }
    }

private:
    HandlerFunc m_handler;
    int m_timeout_ms;
};

#endif //WEBSOCKET_HANDLER_HPP
