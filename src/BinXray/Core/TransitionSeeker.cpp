// SPDX-License-Identifier: MIT

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
    result.toByte = toByte;
    result.transitionCount = 0;

    const std::size_t fileSize = bytes.size();
    const std::size_t start = std::min(rangeStart, fileSize);
    const std::size_t end = std::min(rangeEnd, fileSize);

    if (end <= start + 1) {
        return result;
    }

    // Scan consecutive byte pairs within [start, end).
    // Transition at index i means bytes[i-1] -> bytes[i].
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
