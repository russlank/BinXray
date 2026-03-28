// SPDX-License-Identifier: MIT
//
// ByteFormatter.cpp  --  hex formatting utilities.
//

#include "ByteFormatter.h"

#include <cstdio>

namespace BinXray::Core {

std::string formatByteHex(const std::uint8_t value) {
    char buffer[3] = {};
    std::snprintf(buffer, sizeof(buffer), "%02X", static_cast<unsigned int>(value));
    return std::string(buffer);
}

std::string formatOffsetHex(const std::size_t offset) {
    constexpr std::size_t kMaxDigits = sizeof(std::size_t) * 2;
    char buffer[2 + kMaxDigits + 1] = {};
    std::snprintf(buffer, sizeof(buffer), "0x%08zX", offset);
    return std::string(buffer);
}

} // namespace BinXray::Core
