// SPDX-License-Identifier: MIT
//
// TransitionSeeker.cpp  --  byte-pair transition offset scanner.
//

#include "TransitionSeeker.h"

#include <algorithm>

namespace BinXray::Core {

SeekResult findTransitionOffsets(
    const std::vector<std::uint8_t>& bytes,
    std::size_t rangeStart,
    std::size_t rangeEnd,
    std::uint8_t fromByte,
    std::uint8_t toByte,
    std::size_t maxResults) {

    SeekResult result;
    result.fromByte = fromByte;
    result.toByte   = toByte;
    result.transitionCount = 0;

    // Clamp to valid bounds so callers need not pre-validate.
    const std::size_t fileSize = bytes.size();
    const std::size_t start    = std::min(rangeStart, fileSize);
    const std::size_t end      = std::min(rangeEnd,   fileSize);

    // Need at least two bytes to form a pair.
    if (end <= start + 1) {
        return result;
    }

    // Pre-allocate a reasonable capacity to avoid repeated reallocs.
    const std::size_t reserveHint = std::min(maxResults, std::size_t{64});
    result.offsets.reserve(reserveHint);

    // Linear scan of consecutive byte pairs within [start, end).
    // A transition at index i means bytes[i-1] -> bytes[i].
    for (std::size_t i = start + 1; i < end; ++i) {
        if (bytes[i - 1] == fromByte && bytes[i] == toByte) {
            ++result.transitionCount;
            if (result.offsets.size() < maxResults) {
                result.offsets.push_back(i);
            }
        }
    }

    return result;
}

} // namespace BinXray::Core
