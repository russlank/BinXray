// SPDX-License-Identifier: MIT
//
// TransitionMatrix.cpp  --  256x256 byte-pair transition density matrix.
//

#include "TransitionMatrix.h"

#include <algorithm>

namespace BinXray::Core {

TransitionMatrix::TransitionMatrix()
    : m_counts(),
      m_startOffset(0),
      m_endOffset(0),
      m_maxCount(0) {
}

void TransitionMatrix::compute(const std::vector<std::uint8_t>& bytes, std::size_t startOffsetInclusive, std::size_t endOffsetExclusive) {
    m_counts.fill(0);
    m_maxCount = 0;

    const std::size_t fileSize = bytes.size();
    const std::size_t clampedStart = std::min(startOffsetInclusive, fileSize);
    const std::size_t clampedEnd = std::min(endOffsetExclusive, fileSize);
    m_startOffset = clampedStart;
    m_endOffset = clampedEnd;

    if (clampedEnd <= clampedStart + 1) {
        return;
    }

    for (std::size_t index = clampedStart + 1; index < clampedEnd; ++index) {
        const std::uint8_t previous = bytes[index - 1];
        const std::uint8_t current = bytes[index];
        const std::size_t matrixIndex = toIndex(previous, current);
        ++m_counts[matrixIndex];
    }

    // Single pass over 64 K entries is faster than a data-dependent
    // std::max inside the hot accumulation loop above.
    for (const auto c : m_counts) {
        if (c > m_maxCount) m_maxCount = c;
    }
}

std::uint32_t TransitionMatrix::count(std::uint8_t from, std::uint8_t to) const noexcept {
    return m_counts[toIndex(from, to)];
}

std::uint32_t TransitionMatrix::maxCount() const noexcept {
    return m_maxCount;
}

std::size_t TransitionMatrix::startOffset() const noexcept {
    return m_startOffset;
}

std::size_t TransitionMatrix::endOffset() const noexcept {
    return m_endOffset;
}

const TransitionMatrix::Counts& TransitionMatrix::counts() const noexcept {
    return m_counts;
}

void TransitionMatrix::renderLuminance(RenderMode mode, Luminance& output) const {
    for (std::size_t index = 0; index < m_counts.size(); ++index) {
        output[index] = mapIntensity(m_counts[index], mode);
    }
}

TransitionMatrix::Luminance TransitionMatrix::renderLuminance(RenderMode mode) const {
    Luminance output = {};
    renderLuminance(mode, output);
    return output;
}

std::size_t TransitionMatrix::toIndex(std::uint8_t from, std::uint8_t to) noexcept {
    return static_cast<std::size_t>(from) * kDimension + static_cast<std::size_t>(to);
}

std::uint8_t TransitionMatrix::mapIntensity(std::uint32_t density, RenderMode mode) noexcept {
    if (density == 0) {
        return 0;
    }

    if (mode == RenderMode::Binary) {
        return 255;
    }

    if (mode == RenderMode::Linear) {
        if (density >= 255) {
            return 255;
        }
        return static_cast<std::uint8_t>(density);
    }

    // Legacy normalized mode: I = 255 - floor(255 / d), d > 0.
    return static_cast<std::uint8_t>(255U - (255U / density));
}

} // namespace BinXray::Core
