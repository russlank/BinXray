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
#include <limits>

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

struct WindowRange {
    std::size_t startInclusive;
    std::size_t endExclusive;
};

enum class RibbonModifierWheelAction {
    None,
    BlockSize,
    RibbonWidth
};

struct RibbonModifierWheelResult {
    int blockSize;
    int ribbonWidth;
    RibbonModifierWheelAction action;
    bool consumed;
};

struct RibbonAutoSlideResult {
    WindowRange range;
    bool moved;
    bool wrapped;
    bool reachedEnd;
    bool shouldContinue;
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

[[nodiscard]] inline WindowRange resizeWindowRangeFromEdge(
    std::size_t fileSize,
    std::size_t currentStartInclusive,
    std::size_t currentEndExclusive,
    std::size_t cursorOffset,
    bool dragStartEdge,
    std::size_t minWindowBytes) noexcept {
    if (fileSize == 0) {
        return {0, 0};
    }

    std::size_t start = std::min(currentStartInclusive, fileSize);
    std::size_t end = std::min(currentEndExclusive, fileSize);
    if (end < start) {
        std::swap(start, end);
    }

    const std::size_t minWindow = std::max<std::size_t>(
        1,
        std::min(minWindowBytes, fileSize));
    if (end - start < minWindow) {
        end = std::min(fileSize, start + minWindow);
        if (end - start < minWindow) {
            start = (end > minWindow) ? (end - minWindow) : 0;
        }
    }

    const std::size_t clampedCursor = std::min(cursorOffset, fileSize - 1);

    if (dragStartEdge) {
        const std::size_t maxStart = (end > minWindow) ? (end - minWindow) : 0;
        start = std::min(clampedCursor, maxStart);
    } else {
        std::size_t desiredEnd = std::min(fileSize, clampedCursor + 1);
        const std::size_t minEnd = std::min(fileSize, start + minWindow);
        if (desiredEnd < minEnd) {
            desiredEnd = minEnd;
        }
        end = desiredEnd;
    }

    if (end - start < minWindow) {
        end = std::min(fileSize, start + minWindow);
        if (end - start < minWindow) {
            start = (end > minWindow) ? (end - minWindow) : 0;
        }
    }

    return {start, end};
}

[[nodiscard]] inline int adjustByStepClamped(
    int currentValue,
    int stepCount,
    int stepSize,
    int minValue,
    int maxValue) noexcept {
    const int lo = std::min(minValue, maxValue);
    const int hi = std::max(minValue, maxValue);
    if (stepCount == 0 || stepSize <= 0) {
        return std::clamp(currentValue, lo, hi);
    }

    const long long proposed =
        static_cast<long long>(currentValue) +
        (static_cast<long long>(stepCount) * static_cast<long long>(stepSize));
    return static_cast<int>(std::clamp<long long>(
        proposed,
        static_cast<long long>(lo),
        static_cast<long long>(hi)));
}

[[nodiscard]] inline RibbonModifierWheelResult applyRibbonModifierWheel(
    bool ctrlHeld,
    bool shiftHeld,
    int wheelSteps,
    int currentBlockSize,
    int currentRibbonWidth,
    int blockSizeMin,
    int blockSizeMax,
    int blockSizeStep,
    int ribbonWidthMin,
    int ribbonWidthMax,
    int ribbonWidthStep) noexcept {
    const int blockMin = std::min(blockSizeMin, blockSizeMax);
    const int blockMax = std::max(blockSizeMin, blockSizeMax);
    const int ribbonMin = std::min(ribbonWidthMin, ribbonWidthMax);
    const int ribbonMax = std::max(ribbonWidthMin, ribbonWidthMax);
    RibbonModifierWheelResult result{
        std::clamp(currentBlockSize, blockMin, blockMax),
        std::clamp(currentRibbonWidth, ribbonMin, ribbonMax),
        RibbonModifierWheelAction::None,
        false
    };

    if (!ctrlHeld || wheelSteps == 0) {
        return result;
    }

    result.consumed = true;
    if (shiftHeld) {
        result.ribbonWidth = adjustByStepClamped(
            result.ribbonWidth,
            wheelSteps,
            ribbonWidthStep,
            ribbonMin,
            ribbonMax);
        result.action = RibbonModifierWheelAction::RibbonWidth;
    } else {
        result.blockSize = adjustByStepClamped(
            result.blockSize,
            wheelSteps,
            blockSizeStep,
            blockMin,
            blockMax);
        result.action = RibbonModifierWheelAction::BlockSize;
    }

    return result;
}

[[nodiscard]] inline bool canAutoSlideWindow(
    std::size_t fileSize,
    std::size_t windowStartInclusive,
    std::size_t windowEndExclusive,
    bool fullViewEnabled,
    float speedRowsPerFrame) noexcept {
    if (fullViewEnabled || fileSize == 0 || speedRowsPerFrame <= 0.0F) {
        return false;
    }

    std::size_t start = std::min(windowStartInclusive, fileSize);
    std::size_t end = std::min(windowEndExclusive, fileSize);
    if (end < start) {
        std::swap(start, end);
    }

    return end > start && (end - start) < fileSize;
}

[[nodiscard]] inline RibbonAutoSlideResult advanceAutoSlideWindow(
    std::size_t fileSize,
    std::size_t currentStartInclusive,
    std::size_t currentEndExclusive,
    std::size_t rowsToAdvance,
    std::size_t bytesPerRow,
    bool repeatFromStart) noexcept {
    RibbonAutoSlideResult result{{0, 0}, false, false, false, false};

    if (fileSize == 0) {
        return result;
    }

    std::size_t start = std::min(currentStartInclusive, fileSize);
    std::size_t end = std::min(currentEndExclusive, fileSize);
    if (end < start) {
        std::swap(start, end);
    }
    if (end <= start) {
        result.range = {start, end};
        return result;
    }

    const std::size_t windowSize = end - start;
    if (windowSize >= fileSize) {
        result.range = {0, fileSize};
        return result;
    }

    const std::size_t rowStride = std::max<std::size_t>(1, bytesPerRow);
    const std::size_t maxStart = fileSize - windowSize;
    if (rowsToAdvance == 0) {
        result.range = {start, start + windowSize};
        result.shouldContinue = true;
        return result;
    }

    // Clamp multiplication to avoid overflow in edge-case arithmetic.
    std::size_t deltaBytes = 0;
    if (rowsToAdvance > (std::numeric_limits<std::size_t>::max() / rowStride)) {
        deltaBytes = std::numeric_limits<std::size_t>::max();
    } else {
        deltaBytes = rowsToAdvance * rowStride;
    }

    std::size_t newStart = start;
    if (!repeatFromStart) {
        if (deltaBytes >= (maxStart - start)) {
            newStart = maxStart;
            result.reachedEnd = true;
        } else {
            newStart = start + deltaBytes;
        }
        result.shouldContinue = newStart < maxStart;
    } else {
        const std::size_t cycleSize = maxStart + 1;
        if (cycleSize > 0) {
            const std::size_t cycleStep = deltaBytes % cycleSize;
            const std::size_t sum = start + cycleStep;
            newStart = (sum >= cycleSize) ? (sum - cycleSize) : sum;
            result.wrapped = (deltaBytes >= cycleSize) || (sum >= cycleSize);
        }
        result.shouldContinue = true;
    }

    result.moved = (newStart != start);
    result.range = {newStart, newStart + windowSize};
    return result;
}

} // namespace BinXray::UI::Layout

