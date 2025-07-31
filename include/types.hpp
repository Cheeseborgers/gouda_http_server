//
// Created by fason on 29/07/25.
//

#ifndef TYPES_HPP
#define TYPES_HPP

#include <regex>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using RoutePattern = std::regex;

using RequestId = uint64_t;

#endif //TYPES_HPP
