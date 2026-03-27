// SPDX-License-Identifier: MIT
//
// TransitionMatrix.h
//
// Computes the 256x256 byte-pair transition density matrix for a selected
// byte range of a binary document.  Each cell (from, to) holds the count
// of consecutive byte pairs `bytes[i-1]==from && bytes[i]==to`.
//
// Three rendering modes convert raw counts to 8-bit luminance values:
//   Binary     -- non-zero -> 255, zero -> 0.
//   Linear     -- min(count, 255).
//   Normalized -- non-linear brightening: 255 - floor(255/count).
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace BinXray::Core {

/// 256x256 byte-pair transition density matrix.
class TransitionMatrix {
public:
    /// Matrix is always 256x256 (one cell per possible byte-pair).
    static constexpr std::size_t kDimension = 256;
    static constexpr std::size_t kCellCount = kDimension * kDimension;

    /// How raw transition counts are mapped to display luminance.
    enum class RenderMode {
        Binary,      ///< Any non-zero count becomes white (255).
        Linear,      ///< Count clamped to [0,255].
        Normalized   ///< Non-linear: 255 - floor(255 / count), for count > 0.
    };

    using Counts    = std::array<std::uint32_t, kCellCount>;
    using Luminance = std::array<std::uint8_t,  kCellCount>;

    TransitionMatrix();

    /// Recompute the matrix from bytes in [startOffsetInclusive, endOffsetExclusive).
    /// Both endpoints are clamped to the vector size.
    void compute(const std::vector<std::uint8_t>& bytes,
                 std::size_t startOffsetInclusive,
                 std::size_t endOffsetExclusive);

    /// Return the transition count for a single (from -> to) pair.
    [[nodiscard]] std::uint32_t count(std::uint8_t from, std::uint8_t to) const noexcept;

    /// Highest count in any cell (useful for scaling).
    [[nodiscard]] std::uint32_t maxCount() const noexcept;

    /// The clamped start/end offsets that were actually used.
    [[nodiscard]] std::size_t startOffset() const noexcept;
    [[nodiscard]] std::size_t endOffset()   const noexcept;

    /// Direct read access to the full counts array (row-major, 256 columns).
    [[nodiscard]] const Counts& counts() const noexcept;

    /// Convert counts to luminance using the specified mode.
    void renderLuminance(RenderMode mode, Luminance& output) const;
    [[nodiscard]] Luminance renderLuminance(RenderMode mode) const;

private:
    /// Map a (from, to) pair to a flat array index.
    static std::size_t  toIndex(std::uint8_t from, std::uint8_t to) noexcept;
    /// Map a single count value to an 8-bit gray level.
    static std::uint8_t mapIntensity(std::uint32_t density, RenderMode mode) noexcept;

    Counts        m_counts;       ///< 256x256 flat row-major transition counts.
    std::size_t   m_startOffset;  ///< Clamped inclusive start of the scanned range.
    std::size_t   m_endOffset;    ///< Clamped exclusive end of the scanned range.
    std::uint32_t m_maxCount;     ///< Maximum count across all cells.
};

} // namespace BinXray::Core
