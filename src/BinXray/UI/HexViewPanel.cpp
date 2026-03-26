// SPDX-License-Identifier: MIT

#include "HexViewPanel.h"

#include "UIConstants.h"
#include "imgui.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace BinXray::UI {

void HexViewPanel::draw(const Core::BinaryDocument& document, std::size_t& selectedOffset) const {
    ImGui::Begin("Hex View");
    drawContent(document, selectedOffset, 0, document.bytes().size());
    ImGui::End();
}

void HexViewPanel::drawEmbedded(const Core::BinaryDocument& document,
                                std::size_t& selectedOffset,
                                std::size_t rangeStartInclusive,
                                std::size_t rangeEndExclusive) const {
    drawContent(document, selectedOffset, rangeStartInclusive, rangeEndExclusive);
}

void HexViewPanel::drawContent(const Core::BinaryDocument& document,
                               std::size_t& selectedOffset,
                               std::size_t rangeStartInclusive,
                               std::size_t rangeEndExclusive) const {
    const auto& bytes = document.bytes();
    if (bytes.empty()) {
        ImGui::TextUnformatted("No binary data loaded.");
        return;
    }

    const std::size_t clampedStart = std::min(rangeStartInclusive, bytes.size());
    const std::size_t clampedEnd = std::min(rangeEndExclusive, bytes.size());
    const std::size_t rangeStart = std::min(clampedStart, clampedEnd);
    const std::size_t rangeEnd = std::max(clampedStart, clampedEnd);
    const std::size_t rangeLength = rangeEnd - rangeStart;
    if (rangeLength == 0) {
        ImGui::TextUnformatted("Selected range is empty.");
        return;
    }

    const std::size_t bytesPerRow     = Constants::kHexBytesPerRow;
    const std::size_t maxVisibleBytes = std::min(rangeLength, Constants::kHexMaxVisibleBytes);

    ImGui::BeginChild("HexViewScrollRegion");

    for (std::size_t relativeOffset = 0; relativeOffset < maxVisibleBytes; relativeOffset += bytesPerRow) {
        const std::size_t absoluteOffset = rangeStart + relativeOffset;
        char offsetBuf[13];
        std::snprintf(offsetBuf, sizeof(offsetBuf), "0x%08zX", absoluteOffset);
        ImGui::TextUnformatted(offsetBuf);
        ImGui::SameLine();

        char        asciiBuf[Constants::kHexBytesPerRow + 1];
        std::size_t asciiLen = 0;

        for (std::size_t column = 0; column < bytesPerRow && (relativeOffset + column) < maxVisibleBytes; ++column) {
            const std::size_t  index      = absoluteOffset + column;
            const bool         isSelected = (index == selectedOffset);
            const std::uint8_t byteVal    = bytes[index];

            asciiBuf[asciiLen++] = (byteVal >= 0x20 && byteVal <= 0x7E) ? static_cast<char>(byteVal) : '.';

            char hexBuf[3];
            std::snprintf(hexBuf, sizeof(hexBuf), "%02X", static_cast<unsigned int>(byteVal));

            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button,        Constants::kHexSelectedColor);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Constants::kHexSelectedHoveredColor);
            }

            ImGui::PushID(static_cast<int>(index));
            if (ImGui::SmallButton(hexBuf)) {
                selectedOffset = index;
            }
            ImGui::PopID();

            if (isSelected) {
                ImGui::PopStyleColor(2);
            }

            if (column + 1 < bytesPerRow && (relativeOffset + column + 1) < maxVisibleBytes) {
                ImGui::SameLine();
            }
        }

        asciiBuf[asciiLen] = '\0';
        ImGui::SameLine();
        ImGui::TextUnformatted(asciiBuf);
    }

    if (rangeLength > maxVisibleBytes) {
        ImGui::Separator();
        ImGui::Text("Showing first %zu bytes of selected %zu-byte range.", maxVisibleBytes, rangeLength);
    }

    ImGui::EndChild();
}

} // namespace BinXray::UI
