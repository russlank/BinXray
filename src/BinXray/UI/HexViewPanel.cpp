// SPDX-License-Identifier: MIT
//
// HexViewPanel.cpp  --  virtualised hex dump with seek-offset highlighting.
//

#include "HexViewPanel.h"

#include "UIConstants.h"
#include "imgui.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace BinXray::UI {

void HexViewPanel::draw(const Core::BinaryDocument& document, std::size_t& selectedOffset) const {
    ImGui::Begin("Hex View");
    drawContent(document, selectedOffset, 0, document.bytes().size(), nullptr, std::nullopt);
    ImGui::End();
}

void HexViewPanel::drawEmbedded(const Core::BinaryDocument& document,
                                std::size_t& selectedOffset,
                                std::size_t rangeStartInclusive,
                                std::size_t rangeEndExclusive,
                                const std::vector<std::size_t>* seekHighlightOffsets,
                                std::optional<std::size_t> scrollToOffset) const {
    drawContent(document, selectedOffset, rangeStartInclusive, rangeEndExclusive,
                seekHighlightOffsets, scrollToOffset);
}

bool HexViewPanel::isInHighlightSet(std::size_t offset,
                                    const std::vector<std::size_t>* offsets) noexcept {
    if (!offsets || offsets->empty()) {
        return false;
    }
    // The offsets vector from TransitionSeeker is in ascending order (scan order).
    return std::binary_search(offsets->begin(), offsets->end(), offset);
}

void HexViewPanel::drawContent(const Core::BinaryDocument& document,
                               std::size_t& selectedOffset,
                               std::size_t rangeStartInclusive,
                               std::size_t rangeEndExclusive,
                               const std::vector<std::size_t>* seekHighlightOffsets,
                               std::optional<std::size_t> scrollToOffset) const {
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

    const std::size_t bytesPerRow = Constants::kHexBytesPerRow;
    const std::size_t totalRows   = (rangeLength + bytesPerRow - 1) / bytesPerRow;

    ImGui::BeginChild("HexViewScrollRegion");

    // Jump to the row containing the requested offset.
    if (scrollToOffset.has_value()) {
        const std::size_t target = *scrollToOffset;
        if (target >= rangeStart && target < rangeEnd) {
            const std::size_t targetRow = (target - rangeStart) / bytesPerRow;
            const float rowHeight  = ImGui::GetTextLineHeightWithSpacing();
            const float viewHeight = ImGui::GetWindowHeight();
            ImGui::SetScrollY(std::max(0.0F,
                static_cast<float>(targetRow) * rowHeight - viewHeight * 0.3F));
        }
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(totalRows));
    while (clipper.Step()) {
        for (int rowIdx = clipper.DisplayStart; rowIdx < clipper.DisplayEnd; ++rowIdx) {
            const std::size_t relativeOffset = static_cast<std::size_t>(rowIdx) * bytesPerRow;
            const std::size_t absoluteOffset = rangeStart + relativeOffset;
            const std::size_t rowBytes = std::min(bytesPerRow, rangeLength - relativeOffset);

            char offsetBuf[13];
            std::snprintf(offsetBuf, sizeof(offsetBuf), "0x%08zX", absoluteOffset);
            ImGui::TextUnformatted(offsetBuf);
            ImGui::SameLine();

            char        asciiBuf[Constants::kHexBytesPerRow + 1];
            std::size_t asciiLen = 0;

            for (std::size_t column = 0; column < rowBytes; ++column) {
                const std::size_t  index      = absoluteOffset + column;
                const bool         isSelected = (index == selectedOffset);
                const bool         isSeekHit  = isInHighlightSet(index, seekHighlightOffsets);
                const std::uint8_t byteVal    = bytes[index];

                asciiBuf[asciiLen++] = (byteVal >= 0x20 && byteVal <= 0x7E)
                    ? static_cast<char>(byteVal) : '.';

                char hexBuf[3];
                std::snprintf(hexBuf, sizeof(hexBuf), "%02X", static_cast<unsigned int>(byteVal));
                char buttonLabel[32];
                std::snprintf(buttonLabel, sizeof(buttonLabel), "%s##%zu", hexBuf, index);

                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button,        Constants::kHexSelectedColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Constants::kHexSelectedHoveredColor);
                } else if (isSeekHit) {
                    ImGui::PushStyleColor(ImGuiCol_Button,        Constants::kHexSeekHighlightColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Constants::kHexSeekHighlightHovered);
                }

                if (ImGui::SmallButton(buttonLabel)) {
                    selectedOffset = index;
                }

                if (isSelected || isSeekHit) {
                    ImGui::PopStyleColor(2);
                }

                if (column + 1 < rowBytes) {
                    ImGui::SameLine();
                }
            }

            asciiBuf[asciiLen] = '\0';
            ImGui::SameLine();

            // Render ASCII column with per-character colouring to match
            // the hex byte highlight state (selected → white bg/black text,
            // seek-hit → gold).
            for (std::size_t ai = 0; ai < asciiLen; ++ai) {
                const std::size_t aiOffset   = absoluteOffset + ai;
                const bool        aiSelected = (aiOffset == selectedOffset);
                const bool        aiSeekHit  = isInHighlightSet(aiOffset, seekHighlightOffsets);
                const char ch[2] = {asciiBuf[ai], '\0'};
                if (aiSelected) {
                    // White background + black text for clearly visible focus.
                    const ImVec2 textPos = ImGui::GetCursorScreenPos();
                    const ImVec2 textSize = ImGui::CalcTextSize(ch);
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    dl->AddRectFilled(textPos,
                        ImVec2(textPos.x + textSize.x, textPos.y + textSize.y),
                        Constants::kHexFocusedAsciiBg);
                    ImGui::TextColored(
                        ImGui::ColorConvertU32ToFloat4(Constants::kHexFocusedAsciiText),
                        "%s", ch);
                } else if (aiSeekHit) {
                    ImGui::TextColored(
                        ImGui::ColorConvertU32ToFloat4(Constants::kHexSeekHighlightColor),
                        "%s", ch);
                } else {
                    ImGui::TextUnformatted(ch, ch + 1);
                }
                if (ai + 1 < asciiLen) {
                    ImGui::SameLine(0.0F, 0.0F);
                }
            }
        }
    }

    ImGui::EndChild();
}

} // namespace BinXray::UI
