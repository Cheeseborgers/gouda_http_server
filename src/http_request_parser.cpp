//
// Created by fason on 28/07/25.
//

#include "http_request_parser.h"

#include <cctype>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "http_structs.hpp"
#include "http_utils.hpp"
#include "logger.hpp"

std::optional<HttpRequest> HttpRequestParser::parse(std::string_view request_view, const bool debug,
                                                    const RequestId request_id)
{
    HttpRequest request;

    const size_t first_line_end = request_view.find("\r\n");
    if (first_line_end == std::string_view::npos) {
        LOG_ERROR(std::format("Request[{}]: No \\r\\n found in request", request_id));
        return std::nullopt;
    }

    const size_t headers_end = request_view.find("\r\n\r\n");
    if (headers_end == std::string_view::npos) {
        LOG_ERROR(std::format("Request[{}]: No \\r\\n\\r\\n found in request", request_id));
        return std::nullopt;
    }

    std::string_view first_line = request_view.substr(0, first_line_end);
    const size_t method_end = first_line.find(' ');
    const size_t path_end = first_line.find(' ', method_end + 1);

    if (method_end == std::string_view::npos || path_end == std::string_view::npos) {
        LOG_ERROR(std::format("Request[{}]: Invalid request line", request_id));
        return std::nullopt;
    }

    request.method = get_method(first_line.substr(0, method_end));
    std::string_view full_path = first_line.substr(method_end + 1, path_end - method_end - 1);
    if (const size_t query_start = full_path.find('?'); query_start != std::string_view::npos) {
        request.path = std::string(full_path.substr(0, query_start));
        const std::string_view query = full_path.substr(query_start + 1);
        parse_query_params(query, request.query_params, request_id, debug);
    }
    else {
        request.path = std::string(full_path);
    }
    request.version = string_to_http_version(first_line.substr(path_end + 1));
    request.raw = std::string(request_view);

    if (debug) {
        LOG_DEBUG(std::format("Request[{}]: Parsed first line: {} {} {}", request_id,
                              method_to_string_view(request.method), request.path,
                              http_version_to_string_view(request.version)));
        for (const auto &[key, values] : request.query_params) {
            for (const auto &value : values) {
                LOG_DEBUG(std::format("Request[{}]: Parsed query param: {}={}", request_id, key, value));
            }
        }
    }

    bool is_websocket = false;
    WebSocketRequestData ws_data;
    for (const std::string_view headers_block =
             request_view.substr(first_line_end + 2, headers_end - first_line_end - 2);
         const auto &line : split_lines(headers_block)) {
        if (line.empty()) {
            continue;
        }

        const size_t colon_pos = line.find(':');
        if (colon_pos == std::string_view::npos) {
            LOG_WARNING(std::format("Request[{}]: Malformed header line", request_id));
            continue;
        }

        std::string_view key = trim(line.substr(0, colon_pos));
        std::string_view value = trim(line.substr(colon_pos + 1));
        std::string key_lower = to_lowercase(key);
        request.set_header(std::string(key_lower), std::string(value));

        if (key_lower == "range") {
            std::regex range_regex(R"(bytes=(\d+)-(\d*))");
            std::string value_str(value);
            if (std::smatch range_matches; std::regex_match(value_str, range_matches, range_regex)) {
                try {
                    HttpRequestRange range{};
                    range.start = std::stoull(range_matches[1].str());
                    range.end = range_matches[2].str().empty() ? 0 : std::stoull(range_matches[2].str());
                    request.range = range;
                    if (debug) {
                        LOG_DEBUG(std::format("Request[{}]: Parsed Range header: bytes={}-{}", request_id, range.start,
                                              range.end));
                    }
                }
                catch (const std::exception &e) {
                    LOG_ERROR(
                        std::format("Request[{}]: Invalid Range header value: {} ({})", request_id, value, e.what()));
                    return std::nullopt;
                }
            }
            else {
                LOG_ERROR(std::format("Request[{}]: Malformed Range header: {}", request_id, value));
                return std::nullopt;
            }
        }
        else if (key_lower == "upgrade" && to_lowercase(value) == "websocket") {
            is_websocket = true;
        }
        else if (key_lower == "sec-websocket-key") {
            ws_data.key = std::string(value);
        }
        else if (key_lower == "sec-websocket-version") {
            if (value != "13") {
                LOG_WARNING(std::format("Request[{}]: Unsupported WebSocket version: {}", request_id, value));
            }
            else {
                ws_data.version = std::string(value);
            }
        }
        else if (key_lower == "sec-websocket-protocol") {
            ws_data.protocol = std::string(value);
        }
        else if (key_lower == "sec-websocket-extensions") {
            ws_data.extensions = std::string(value);
        }

        if (debug) {
            LOG_DEBUG(std::format("Request[{}]: Parsed header: {}: {}", request_id, key, value));
        }
    }

    if (is_websocket && !ws_data.key.empty()) {
        if (request.method != HttpMethod::GET) {
            LOG_WARNING(std::format("Request[{}]: WebSocket request must use GET method", request_id));
        }
        else if (!request.has_header("connection") ||
                 to_lowercase(request.get_header("connection").value()).find("upgrade") == std::string::npos) {
            LOG_WARNING(std::format("Request[{}]: Missing or invalid Connection header for WebSocket", request_id));
        }
        else if (ws_data.version.empty() || ws_data.version != "13") {
            LOG_WARNING(std::format("Request[{}]: Missing or invalid Sec-WebSocket-Version header", request_id));
        }
        else {
            request.websocket_data = ws_data;
            if (debug) {
                LOG_DEBUG(
                    std::format("Request[{}]: Detected WebSocket upgrade request: key={}", request_id, ws_data.key));
                if (ws_data.protocol) {
                    LOG_DEBUG(std::format("Request[{}]: WebSocket protocol: {}", request_id, *ws_data.protocol));
                }
                if (ws_data.extensions) {
                    LOG_DEBUG(std::format("Request[{}]: WebSocket extensions: {}", request_id, *ws_data.extensions));
                }
            }
        }
    }

    if (headers_end + 4 < request_view.size()) {
        const std::string_view body = request_view.substr(headers_end + 4);
        request.body = std::string(body);
        if (debug) {
            LOG_DEBUG(std::format("Request[{}]: Parsed body ({} bytes)", request_id, body.size()));
        }
        if (request.method == HttpMethod::POST) {
            if (const auto content_type_iterator = request.headers.find("content-type");
                content_type_iterator != request.headers.end() &&
                content_type_iterator->second.find(CONTENT_TYPE_FORM_URLENCODED) != std::string::npos) {
                parse_query_params(body, request.form_params, request_id, debug);
                if (debug) {
                    for (const auto &[key, values] : request.form_params) {
                        for (const auto &value : values) {
                            LOG_DEBUG(std::format("Request[{}]: Parsed form param: {}={}", request_id, key, value));
                        }
                    }
                }
            }
        }
    }

    LOG_DEBUG(std::format("Request[{}]: {} {}", request_id, method_to_string_view(request.method), request.path));
    return request;
}

std::optional<WebSocketFrame> HttpRequestParser::parse_websocket_frame(const std::string_view frame_data,
                                                                       const bool debug, RequestId request_id)
{
    WebSocketFrame frame;

    if (frame_data.size() < 2) {
        LOG_ERROR(std::format("Request[{}]: WebSocket frame too short: {} bytes", request_id, frame_data.size()));
        return std::nullopt;
    }

    // Parse first byte
    frame.fin = (frame_data[0] & 0x80) != 0;  // FIN bit
    frame.rsv1 = (frame_data[0] & 0x40) != 0; // RSV1 bit
    frame.rsv2 = (frame_data[0] & 0x20) != 0; // RSV2 bit
    frame.rsv3 = (frame_data[0] & 0x10) != 0; // RSV3 bit
    frame.opcode = frame_data[0] & 0x0F;      // Opcode (4 bits)

    // Check reserved bits
    if (frame.rsv1 || frame.rsv2 || frame.rsv3) {
        LOG_WARNING(std::format("Request[{}]: Invalid WebSocket frame: RSV1={}, RSV2={}, RSV3={}", request_id,
                                frame.rsv1, frame.rsv2, frame.rsv3));
        return std::nullopt;
    }

    // Parse second byte
    frame.mask = (frame_data[1] & 0x80) != 0;    // MASK bit
    frame.payload_length = frame_data[1] & 0x7F; // Payload length (7 bits)

    size_t offset = 2;

    // Handle extended payload length
    if (frame.payload_length == 126) {
        if (frame_data.size() < offset + 2) {
            LOG_WARNING(std::format("Request[{}]: WebSocket frame too short for extended length ({} bytes)", request_id,
                                    frame_data.size()));
            return std::nullopt;
        }
        frame.payload_length =
            (static_cast<uint64_t>(frame_data[offset]) << 8) | static_cast<uint64_t>(frame_data[offset + 1]);
        offset += 2;
    }
    else if (frame.payload_length == 127) {
        if (frame_data.size() < offset + 8) {
            LOG_WARNING(std::format("Request[{}]: WebSocket frame too short for extended length ({} bytes)", request_id,
                                    frame_data.size()));
            return std::nullopt;
        }
        frame.payload_length =
            (static_cast<uint64_t>(frame_data[offset]) << 56) | (static_cast<uint64_t>(frame_data[offset + 1]) << 48) |
            (static_cast<uint64_t>(frame_data[offset + 2]) << 40) |
            (static_cast<uint64_t>(frame_data[offset + 3]) << 32) |
            (static_cast<uint64_t>(frame_data[offset + 4]) << 24) |
            (static_cast<uint64_t>(frame_data[offset + 5]) << 16) |
            (static_cast<uint64_t>(frame_data[offset + 6]) << 8) | static_cast<uint64_t>(frame_data[offset + 7]);
        offset += 8;
    }

    // Check mask
    if (!frame.mask) {
        LOG_WARNING(std::format("Request[{}]: WebSocket frame missing mask", request_id));
        return std::nullopt;
    }

    // Parse masking key
    if (frame_data.size() < offset + 4) {
        LOG_WARNING(std::format("Request[{}]: WebSocket frame too short for masking key ({} bytes)", request_id,
                                frame_data.size()));
        return std::nullopt;
    }

    std::array<uint8_t, 4> masking_key{};
    std::copy_n(frame_data.begin() + offset, 4, masking_key.begin());
    offset += 4;

    // Check payload length
    if (frame_data.size() < offset + frame.payload_length) {
        LOG_WARNING(std::format("Request[{}]: WebSocket frame too short for payload ({} bytes, expected {})",
                                request_id, frame_data.size(), offset + frame.payload_length));
        return std::nullopt;
    }

    if (frame.payload_length > DEFAULT_MAX_WEBSOCKET_PAYLOAD_SIZE) {
        LOG_ERROR(std::format("Request[{}]: WebSocket frame payload too large: {}", request_id, frame.payload_length));
        return std::nullopt;
    }

    // Extract and unmask payload
    frame.payload = std::string(frame_data.substr(offset, frame.payload_length));
    for (size_t i = 0; i < frame.payload_length; ++i) {
        frame.payload[i] ^= masking_key[i % 4];
    }

    if (debug) {
        LOG_DEBUG(std::format(
            "Request[{}]: Parsed WebSocket frame: FIN={}, Opcode={}, Mask={}, Payload Length={}, Payload={}",
            request_id, frame.fin, frame.opcode, frame.mask, frame.payload_length, frame.payload));
    }

    return frame;
}

std::string HttpRequestParser::compute_websocket_accept(const std::string &key) {
    // TODO: Check what we gonna do here
    const std::string ws_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const std::string input = key + ws_guid;

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(input.c_str()), input.length(), hash);

    BIO *b64{BIO_new(BIO_f_base64())};
    BIO *mem{BIO_new(BIO_s_mem())};
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_push(b64, mem);
    BIO_write(b64, hash, SHA_DIGEST_LENGTH);
    BIO_flush(b64);

    char *base64_data{nullptr};
    const long length{BIO_get_mem_data(mem, &base64_data)};
    std::string result(base64_data, length);

    BIO_free_all(b64);
    return result;
}
