// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace BinXray::Core {

/// Resolved seek result for a (from, to) byte-pair coordinate on the transition matrix.
struct SeekResult {
    std::uint8_t fromByte = 0;
    std::uint8_t toByte   = 0;
    std::uint32_t transitionCount = 0;
    /// Absolute offsets where byte[i-1]==fromByte && byte[i]==toByte within the active range.
    std::vector<std::size_t> offsets;
};

/// Scans the document byte range for all offsets matching a given (from, to) transition pair.
/// Returns at most `maxResults` offsets to avoid unbounded memory for dense transitions.
[[nodiscard]] SeekResult findTransitionOffsets(
    const std::vector<std::uint8_t>& bytes,
    std::size_t rangeStart,
    std::size_t rangeEnd,
    std::uint8_t fromByte,
    std::uint8_t toByte,
    std::size_t maxResults);

} // namespace BinXray::Core
