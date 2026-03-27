// SPDX-License-Identifier: MIT
//
// ByteFormatter.h
//
// Lightweight formatting helpers for hex display of byte values and offsets.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace BinXray::Core {

[[nodiscard]] std::string formatByteHex(std::uint8_t value);
[[nodiscard]] std::string formatOffsetHex(std::size_t offset);

} // namespace BinXray::Core
