// SPDX-License-Identifier: MIT
//
// TrigramPlot.cpp  --  3D byte-trigram density accumulator.
//

#include "TrigramPlot.h"

#include <algorithm>
#include <cstring>

namespace BinXray::Core {

// Flat 256^3 = 16 777 216 cells.  ~64 MB for uint32_t.
// We allocate on the heap once per compute() and extract non-zero entries.
// Using a thread-local buffer avoids repeated alloc/free per frame when
// the user is scrubbing the analysis window.
namespace {
constexpr std::size_t kCubeSize = 256ULL * 256ULL * 256ULL;

inline std::size_t toIndex(std::uint8_t x, std::uint8_t y, std::uint8_t z) noexcept {
    return (static_cast<std::size_t>(x) << 16) |
           (static_cast<std::size_t>(y) << 8)  |
            static_cast<std::size_t>(z);
}
} // namespace

TrigramPlot::TrigramPlot()
    : m_maxCount(0) {
}

void TrigramPlot::compute(const std::vector<std::uint8_t>& bytes,
                           std::size_t startInclusive,
                           std::size_t endExclusive) {
    m_points.clear();
    m_maxCount = 0;

    const std::size_t fileSize = bytes.size();
    const std::size_t start = std::min(startInclusive, fileSize);
    const std::size_t end   = std::min(endExclusive, fileSize);
    if (end < start + 3) {
        return;
    }

    // Allocate (or reuse) the flat cube.  memset is significantly faster
    // than std::vector::assign for zeroing the 64 MB buffer.
    static thread_local std::vector<std::uint32_t> cube;
    if (cube.size() < kCubeSize) {
        cube.resize(kCubeSize, 0);
    }

    // Track which indices were written so we can zero only those cells
    // after extraction, avoiding a full 64 MB clear on the next call.
    static thread_local std::vector<std::size_t> dirtyIndices;
    dirtyIndices.clear();

    const std::uint8_t* data = bytes.data();
    for (std::size_t i = start; i + 2 < end; ++i) {
        const std::size_t idx = toIndex(data[i], data[i + 1], data[i + 2]);
        if (cube[idx] == 0) {
            dirtyIndices.push_back(idx);
        }
        ++cube[idx];
    }

    // Extract non-zero voxels from only the dirty cells (much smaller than 16M).
    m_points.reserve(dirtyIndices.size());
    for (const std::size_t idx : dirtyIndices) {
        const std::uint32_t c = cube[idx];
        const auto x = static_cast<std::uint8_t>((idx >> 16) & 0xFF);
        const auto y = static_cast<std::uint8_t>((idx >>  8) & 0xFF);
        const auto z = static_cast<std::uint8_t>( idx        & 0xFF);
        m_points.push_back({x, y, z, c});
        if (c > m_maxCount) m_maxCount = c;
        cube[idx] = 0;  // Reset for next call — no full memset needed.
    }
}

std::uint8_t TrigramPlot::mapIntensity(std::uint32_t count,
                                        std::uint32_t maxCount,
                                        bool scale,
                                        bool normalize) noexcept {
    if (count == 0) return 0;
    if (!scale)     return 255;                         // Binary mode
    if (!normalize) return static_cast<std::uint8_t>(std::min(count, 255U)); // Linear clamp
    // Normalized: scale count relative to the observed maximum.
    if (maxCount == 0) return 0;
    return static_cast<std::uint8_t>(
        static_cast<std::uint64_t>(count) * 255 / maxCount);
}

} // namespace BinXray::Core
