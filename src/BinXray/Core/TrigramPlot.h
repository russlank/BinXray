// SPDX-License-Identifier: MIT
//
// TrigramPlot.h
//
// Accumulates 3D byte-trigram scatter data from a binary document range.
// Each consecutive triple (bytes[i], bytes[i+1], bytes[i+2]) maps to a
// point in a 256^3 cube.  The density at each voxel is tracked so the UI
// can colour points by frequency (grayscale or heat-map).
//
// The public interface mirrors TransitionMatrix: compute() scans a range,
// then the UI iterates over non-empty voxels for rendering.
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace BinXray::Core {

/// Compact record for a non-empty voxel in the 256^3 trigram cube.
struct TrigramPoint {
    std::uint8_t  x;      ///< First byte value  (bytes[i]).
    std::uint8_t  y;      ///< Second byte value (bytes[i+1]).
    std::uint8_t  z;      ///< Third byte value  (bytes[i+2]).
    std::uint32_t count;  ///< Number of occurrences.
};

/// 3D byte-trigram density accumulator.
class TrigramPlot {
public:
    static constexpr std::size_t kDimension = 256;

    TrigramPlot();

    /// Scan bytes in [startInclusive, endExclusive) and accumulate trigrams.
    void compute(const std::vector<std::uint8_t>& bytes,
                 std::size_t startInclusive,
                 std::size_t endExclusive);

    /// Highest count across all voxels (for normalisation).
    [[nodiscard]] std::uint32_t maxCount() const noexcept { return m_maxCount; }

    /// Non-empty voxels collected after the last compute().
    [[nodiscard]] const std::vector<TrigramPoint>& points() const noexcept { return m_points; }

    /// Map a raw count to 0-255 intensity using the same modes as TransitionMatrix.
    [[nodiscard]] static std::uint8_t mapIntensity(std::uint32_t count,
                                                    std::uint32_t maxCount,
                                                    bool scale,
                                                    bool normalize) noexcept;

private:
    std::vector<TrigramPoint> m_points;
    std::uint32_t             m_maxCount;
};

} // namespace BinXray::Core
