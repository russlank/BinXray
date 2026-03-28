// SPDX-License-Identifier: MIT
//
// CrosshairCoords.h
//
// Computes the transition-plot and trigram-plot coordinates implied by a
// selected byte offset in a binary document.  These helpers extract the
// coordinate-mapping logic from the rendering code so it can be unit-tested
// independently.
//
// Key insight: a byte at offset `i` participates in multiple sliding windows:
//   2D (pairs):    (bytes[i-1]→bytes[i]) AND (bytes[i]→bytes[i+1])
//   3D (trigrams): (bytes[i-2],bytes[i-1],bytes[i]) AND
//                  (bytes[i-1],bytes[i],bytes[i+1]) AND
//                  (bytes[i],bytes[i+1],bytes[i+2])
//
// The multi-point functions return all valid coordinates for a given offset.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace BinXray::Core {

/// 2D transition-plot coordinate: (fromByte, toByte) pair.
struct TransitionCoord {
    std::uint8_t fromByte;  ///< Previous byte (row in the 256x256 matrix).
    std::uint8_t toByte;    ///< Current byte  (column in the matrix).
};

/// 3D trigram-plot coordinate: (x, y, z) triple.
struct TrigramCoord {
    std::uint8_t x;  ///< First byte of the trigram.
    std::uint8_t y;  ///< Second byte.
    std::uint8_t z;  ///< Third byte.
};

/// Fixed-capacity result for 2D transition coordinates (max 2).
struct TransitionCoordSet {
    TransitionCoord coords[2];
    std::size_t     count = 0;  ///< 0, 1, or 2.
};

/// Fixed-capacity result for 3D trigram coordinates (max 3).
struct TrigramCoordSet {
    TrigramCoord coords[3];
    std::size_t  count = 0;  ///< 0, 1, 2, or 3.
};

// ── Single-point helpers (kept for backward compatibility) ───────────

/// Derive a single 2D transition coordinate (bytes[offset-1] → bytes[offset]).
/// Returns std::nullopt when offset is 0 or out of range.
[[nodiscard]] inline std::optional<TransitionCoord>
transitionCoordAt(const std::vector<std::uint8_t>& bytes, std::size_t selectedOffset) noexcept {
    if (selectedOffset == 0 || selectedOffset >= bytes.size()) {
        return std::nullopt;
    }
    return TransitionCoord{bytes[selectedOffset - 1], bytes[selectedOffset]};
}

/// Derive a single 3D trigram coordinate (bytes[offset-2], bytes[offset-1], bytes[offset]).
/// Returns std::nullopt when fewer than two preceding bytes are available.
[[nodiscard]] inline std::optional<TrigramCoord>
trigramCoordAt(const std::vector<std::uint8_t>& bytes, std::size_t selectedOffset) noexcept {
    if (selectedOffset < 2 || selectedOffset >= bytes.size()) {
        return std::nullopt;
    }
    return TrigramCoord{bytes[selectedOffset - 2], bytes[selectedOffset - 1], bytes[selectedOffset]};
}

// ── Multi-point helpers ──────────────────────────────────────────────

/// Return all transition pairs that involve the byte at `selectedOffset`.
///   pair 0: bytes[offset-1] → bytes[offset]    (selected byte as "to")
///   pair 1: bytes[offset]   → bytes[offset+1]  (selected byte as "from")
[[nodiscard]] inline TransitionCoordSet
transitionCoordsAt(const std::vector<std::uint8_t>& bytes, std::size_t selectedOffset) noexcept {
    TransitionCoordSet result{};
    if (selectedOffset >= bytes.size()) {
        return result;
    }
    // Pair where selected byte is the "to" (second in pair).
    if (selectedOffset > 0) {
        result.coords[result.count++] = {bytes[selectedOffset - 1], bytes[selectedOffset]};
    }
    // Pair where selected byte is the "from" (first in pair).
    if (selectedOffset + 1 < bytes.size()) {
        result.coords[result.count++] = {bytes[selectedOffset], bytes[selectedOffset + 1]};
    }
    return result;
}

/// Return all trigrams that involve the byte at `selectedOffset`.
///   trigram 0: (bytes[offset-2], bytes[offset-1], bytes[offset])      as z
///   trigram 1: (bytes[offset-1], bytes[offset],   bytes[offset+1])    as y
///   trigram 2: (bytes[offset],   bytes[offset+1], bytes[offset+2])    as x
[[nodiscard]] inline TrigramCoordSet
trigramCoordsAt(const std::vector<std::uint8_t>& bytes, std::size_t selectedOffset) noexcept {
    TrigramCoordSet result{};
    if (selectedOffset >= bytes.size()) {
        return result;
    }
    // Trigram where selected byte is the third (z).
    if (selectedOffset >= 2) {
        result.coords[result.count++] = {
            bytes[selectedOffset - 2], bytes[selectedOffset - 1], bytes[selectedOffset]};
    }
    // Trigram where selected byte is the second (y).
    if (selectedOffset >= 1 && selectedOffset + 1 < bytes.size()) {
        result.coords[result.count++] = {
            bytes[selectedOffset - 1], bytes[selectedOffset], bytes[selectedOffset + 1]};
    }
    // Trigram where selected byte is the first (x).
    if (selectedOffset + 2 < bytes.size()) {
        result.coords[result.count++] = {
            bytes[selectedOffset], bytes[selectedOffset + 1], bytes[selectedOffset + 2]};
    }
    return result;
}

} // namespace BinXray::Core
