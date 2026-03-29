// SPDX-License-Identifier: MIT
//
// UILayoutLogic.h
//
// Header-only helpers for UI layout and hit-testing rules used by
// Application.cpp. Keeping this logic in pure functions makes it easy
// to validate with lightweight non-visual tests.
//
#pragma once

#include <algorithm>
#include <cstddef>

namespace BinXray::UI::Layout {

struct WorkspacePolicy {
    float leftRatio;
    float leftMin;
    float leftMax;
    float rightRatio;
    float rightMin;
    float rightMax;
    float centerMin;
    float leftHardMin;
    float rightHardMin;
    float centerHardMin;
};

struct WorkspaceWidths {
    float left;
    float center;
    float right;
};

[[nodiscard]] inline WorkspaceWidths computeWorkspaceWidths(
    float totalWidth,
    float spacing,
    const WorkspacePolicy& policy) noexcept {
    WorkspaceWidths result{};
    result.left = std::clamp(totalWidth * policy.leftRatio, policy.leftMin, policy.leftMax);
    result.right = std::clamp(totalWidth * policy.rightRatio, policy.rightMin, policy.rightMax);
    result.center = totalWidth - result.left - result.right - (2.0F * spacing);

    if (result.center < policy.centerMin) {
        const float deficit = policy.centerMin - result.center;
        const float leftReduction = std::min(
            deficit * 0.5F,
            std::max(0.0F, result.left - policy.leftHardMin));
        result.left -= leftReduction;
        result.right -= (deficit - leftReduction);
        result.right = std::max(policy.rightHardMin, result.right);
        result.center = totalWidth - result.left - result.right - (2.0F * spacing);
    }

    if (result.center < policy.centerHardMin) {
        const float minCenter = std::max(0.0F, std::min(policy.centerHardMin, totalWidth - (2.0F * spacing)));
        const float sideBudget = std::max(0.0F, totalWidth - minCenter - (2.0F * spacing));
        const float sideSum = result.left + result.right;
        if (sideSum > 0.0F && sideBudget < sideSum) {
            const float scale = sideBudget / sideSum;
            result.left *= scale;
            result.right *= scale;
        }
        result.center = std::max(0.0F, totalWidth - result.left - result.right - (2.0F * spacing));
    }

    return result;
}

[[nodiscard]] inline bool containsPoint(
    float pointX,
    float pointY,
    float rectX,
    float rectY,
    float rectWidth,
    float rectHeight) noexcept {
    return pointX >= rectX &&
           pointX < (rectX + rectWidth) &&
           pointY >= rectY &&
           pointY < (rectY + rectHeight);
}

[[nodiscard]] inline bool canSplitHexAndAddresses(
    float fullWidth,
    float spacing,
    float minHexWidth,
    float minAddressWidth) noexcept {
    return fullWidth >= (minHexWidth + minAddressWidth + spacing);
}

[[nodiscard]] inline std::size_t computeRibbonRowCount(
    std::size_t byteCount,
    std::size_t ribbonWidth) noexcept {
    if (byteCount == 0 || ribbonWidth == 0) {
        return 0;
    }
    return ((byteCount - 1) / ribbonWidth) + 1;
}

[[nodiscard]] inline bool shouldShowSeekOffsets(
    bool is3DMode,
    bool seekEnabled,
    bool seekValid,
    std::size_t offsetCount) noexcept {
    return !is3DMode && seekEnabled && seekValid && offsetCount > 0;
}

} // namespace BinXray::UI::Layout

