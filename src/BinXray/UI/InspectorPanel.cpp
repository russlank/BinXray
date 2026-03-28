// SPDX-License-Identifier: MIT

#include "InspectorPanel.h"

#include "Core/ByteFormatter.h"
#include "imgui.h"

#include <cctype>

namespace BinXray::UI {

void InspectorPanel::draw(const Core::BinaryDocument& document, std::size_t selectedOffset) const {
    ImGui::Begin("Inspector");

    const auto& bytes = document.bytes();
    if (bytes.empty()) {
        ImGui::TextUnformatted("No byte selected.");
        ImGui::End();
        return;
    }

    if (selectedOffset >= bytes.size()) {
        selectedOffset = bytes.size() - 1;
    }

    const std::uint8_t value = bytes[selectedOffset];
    const char printable = std::isprint(static_cast<unsigned char>(value))
        ? static_cast<char>(value)
        : '.';

    ImGui::Text("Offset: 0x%zX (%zu)", selectedOffset, selectedOffset);
    ImGui::Text("Hex: %s", Core::formatByteHex(value).c_str());
    ImGui::Text("Decimal: %u", static_cast<unsigned int>(value));
    ImGui::Text("ASCII: %c", printable);

    ImGui::Separator();
    ImGui::TextUnformatted("Planned next phase:");
    ImGui::BulletText("binary structure templates");
    ImGui::BulletText("seek/search");
    ImGui::BulletText("diff and entropy views");

    ImGui::End();
}

} // namespace BinXray::UI
