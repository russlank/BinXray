// SPDX-License-Identifier: MIT
//
// HexViewPanel.h
//
// Renders a scrollable hex dump of binary data with optional seek-offset
// highlighting and programmatic scroll-to-offset navigation.
// Uses ImGuiListClipper for virtualized rendering so arbitrarily large
// ranges remain responsive.
//
#pragma once

#include "Core/BinaryDocument.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace BinXray::UI {

/// Stateless panel that renders a hex-view of a BinaryDocument byte range.
class HexViewPanel {
public:
    /// Render a standalone hex-view window (ImGui::Begin / End).
    void draw(const Core::BinaryDocument& document, std::size_t& selectedOffset) const;

    /// Render hex-view content inside an already-open ImGui child region.
    ///
    /// @param seekHighlightOffsets  If non-null, sorted ascending offsets to
    ///                              highlight in gold (from the seeker).
    /// @param scrollToOffset        If set, the view will scroll to place
    ///                              this offset near the top of the viewport.
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

    /// O(log n) membership test against a sorted offset vector.
    [[nodiscard]] static bool isInHighlightSet(std::size_t offset,
                                               const std::vector<std::size_t>* offsets) noexcept;
};

} // namespace BinXray::UI
