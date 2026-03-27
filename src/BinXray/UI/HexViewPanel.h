// SPDX-License-Identifier: MIT
#pragma once

#include "Core/BinaryDocument.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace BinXray::UI {

class HexViewPanel {
public:
    void draw(const Core::BinaryDocument& document, std::size_t& selectedOffset) const;
    void drawEmbedded(const Core::BinaryDocument& document,
                      std::size_t& selectedOffset,
                      std::size_t rangeStartInclusive,
                      std::size_t rangeEndExclusive,
                      const std::vector<std::size_t>* seekHighlightOffsets = nullptr,
                      std::optional<std::size_t> scrollToOffset = std::nullopt) const;

private:
    void drawContent(const Core::BinaryDocument& document,
                     std::size_t& selectedOffset,
                     std::size_t rangeStartInclusive,
                     std::size_t rangeEndExclusive,
                     const std::vector<std::size_t>* seekHighlightOffsets,
                     std::optional<std::size_t> scrollToOffset) const;

    [[nodiscard]] static bool isInHighlightSet(std::size_t offset,
                                               const std::vector<std::size_t>* offsets) noexcept;
};

} // namespace BinXray::UI
