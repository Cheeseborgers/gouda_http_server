//
// Created by fason on 17/08/25.
//

#ifndef WEBSOCKET_HPP
#define WEBSOCKET_HPP

#include "types.hpp"

// Helper function to encode websocket payload length
inline void encode_payload_length(const u64 length, std::string& frame) {
    if (length <= 125) {
        frame += static_cast<char>(length);
    } else if (length <= 0xFFFF) {
        frame += '\x7E';
        frame += static_cast<char>((length >> 8) & 0xFF);
        frame += static_cast<char>(length & 0xFF);
    } else {
        frame += '\x7F';
        for (int i = 7; i >= 0; --i) {
            frame += static_cast<char>((length >> (i * 8)) & 0xFF);
        }
    }
}

#endif //WEBSOCKET_HPP
