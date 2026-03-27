// SPDX-License-Identifier: MIT
//
// TransitionSeeker.h
//
// Provides a byte-pair transition scanner that locates offsets in a binary
// document where a specific (from -> to) byte transition occurs. Used by
// the seeking overlay in the UI to map a transition-plot coordinate back
// to concrete file positions.
//
// Design note: this module is intentionally free of UI dependencies so it
// can be tested in isolation.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace BinXray::Core {

/// Result of scanning for a specific (fromByte -> toByte) transition.
///
/// `transitionCount` is the total number of matches in the scanned range.
/// `offsets` holds at most `maxResults` of those match positions (the index
/// of the *second* byte in each matching pair), in ascending order.
struct SeekResult {
    std::uint8_t fromByte         = 0;   ///< Previous byte value (row in matrix).
    std::uint8_t toByte           = 0;   ///< Current byte value (column in matrix).
    std::uint32_t transitionCount = 0;   ///< Total matches found (may exceed offsets.size()).

    /// Absolute file offsets of the matching transitions.  Each offset `i`
    /// satisfies `bytes[i-1] == fromByte && bytes[i] == toByte`.  Stored in
    /// ascending order; at most `maxResults` entries are kept.
    std::vector<std::size_t> offsets;
};

/// Scans `bytes[rangeStart .. rangeEnd)` for consecutive pairs matching
/// `(fromByte, toByte)`.  Returns up to `maxResults` offsets to avoid
/// unbounded memory consumption for dense transitions.
///
/// @param bytes       Full document byte vector.
/// @param rangeStart  Inclusive start of scan window.
/// @param rangeEnd    Exclusive end of scan window.
/// @param fromByte    Previous-byte value to match (matrix row).
/// @param toByte      Current-byte value to match (matrix column).
/// @param maxResults  Maximum number of offsets to collect.
/// @return A SeekResult with the total count and collected offsets.
[[nodiscard]] SeekResult findTransitionOffsets(
    const std::vector<std::uint8_t>& bytes,
    std::size_t rangeStart,
    std::size_t rangeEnd,
    std::uint8_t fromByte,
    std::uint8_t toByte,
    std::size_t maxResults);

} // namespace BinXray::Core
