// SPDX-License-Identifier: MIT
//
// UILayoutLogicTests.cpp  --  lightweight non-visual UI integration tests.
//
// These tests validate pure layout/hit-testing rules used by Application.cpp.
//

#include "UI/UILayoutLogic.h"

#include <cmath>
#include <iostream>

namespace {

bool expectTrue(bool value, const char* caseName) {
    if (value) {
        return true;
    }
    std::cout << "[FAIL] " << caseName << std::endl;
    return false;
}

bool expectFalse(bool value, const char* caseName) {
    return expectTrue(!value, caseName);
}

bool expectSize(std::size_t actual, std::size_t expected, const char* caseName) {
    if (actual == expected) {
        return true;
    }
    std::cout << "[FAIL] " << caseName
              << " expected=" << expected
              << " actual=" << actual << std::endl;
    return false;
}

bool expectInt(int actual, int expected, const char* caseName) {
    if (actual == expected) {
        return true;
    }
    std::cout << "[FAIL] " << caseName
              << " expected=" << expected
              << " actual=" << actual << std::endl;
    return false;
}

bool expectNear(float actual, float expected, float eps, const char* caseName) {
    if (std::fabs(actual - expected) <= eps) {
        return true;
    }
    std::cout << "[FAIL] " << caseName
              << " expected=" << expected
              << " actual=" << actual << std::endl;
    return false;
}

} // namespace

bool runUILayoutLogicTests() {
    using namespace BinXray::UI::Layout;

    bool passed = true;

    constexpr WorkspacePolicy policy{
        0.22F, 220.0F, 360.0F, // left
        0.18F, 180.0F, 320.0F, // right
        260.0F,                // center min
        180.0F,                // left hard min
        160.0F,                // right hard min
        64.0F                  // center hard min
    };

    // 1) Typical desktop width.
    {
        const float spacing = 8.0F;
        const float totalWidth = 1600.0F;
        const WorkspaceWidths w = computeWorkspaceWidths(totalWidth, spacing, policy);
        passed = expectTrue(w.left >= policy.leftHardMin, "workspace regular: left >= hard min") && passed;
        passed = expectTrue(w.right >= policy.rightHardMin, "workspace regular: right >= hard min") && passed;
        passed = expectTrue(w.center >= policy.centerMin, "workspace regular: center >= center min") && passed;
        passed = expectNear(
            w.left + w.center + w.right + (2.0F * spacing),
            totalWidth, 0.05F,
            "workspace regular: width conservation") && passed;
    }

    // 2) Narrow width still yields non-negative columns.
    {
        const float spacing = 8.0F;
        const float totalWidth = 300.0F;
        const WorkspaceWidths w = computeWorkspaceWidths(totalWidth, spacing, policy);
        passed = expectTrue(w.left >= 0.0F, "workspace narrow: left non-negative") && passed;
        passed = expectTrue(w.center >= 0.0F, "workspace narrow: center non-negative") && passed;
        passed = expectTrue(w.right >= 0.0F, "workspace narrow: right non-negative") && passed;
        passed = expectNear(
            w.left + w.center + w.right + (2.0F * spacing),
            totalWidth, 0.05F,
            "workspace narrow: width conservation") && passed;
    }

    // 3) Extremely narrow width remains stable.
    {
        const float spacing = 8.0F;
        const float totalWidth = 20.0F;
        const WorkspaceWidths w = computeWorkspaceWidths(totalWidth, spacing, policy);
        passed = expectTrue(w.left >= 0.0F, "workspace tiny: left non-negative") && passed;
        passed = expectTrue(w.center >= 0.0F, "workspace tiny: center non-negative") && passed;
        passed = expectTrue(w.right >= 0.0F, "workspace tiny: right non-negative") && passed;
    }

    // 4) Point-in-rect hit-testing boundaries.
    {
        passed = expectTrue(containsPoint(10.0F, 10.0F, 10.0F, 10.0F, 20.0F, 20.0F), "containsPoint: top-left inclusive") && passed;
        passed = expectFalse(containsPoint(30.0F, 10.0F, 10.0F, 10.0F, 20.0F, 20.0F), "containsPoint: right edge exclusive") && passed;
        passed = expectFalse(containsPoint(10.0F, 30.0F, 10.0F, 10.0F, 20.0F, 20.0F), "containsPoint: bottom edge exclusive") && passed;
        passed = expectFalse(containsPoint(9.9F, 10.0F, 10.0F, 10.0F, 20.0F, 20.0F), "containsPoint: left outside") && passed;
    }

    // 5) Split threshold helper.
    {
        passed = expectTrue(canSplitHexAndAddresses(340.0F, 10.0F, 220.0F, 110.0F), "split helper: exact threshold") && passed;
        passed = expectFalse(canSplitHexAndAddresses(339.0F, 10.0F, 220.0F, 110.0F), "split helper: below threshold") && passed;
    }

    // 6) Ribbon row count helper.
    {
        passed = expectSize(computeRibbonRowCount(0, 128), 0, "ribbon rows: empty") && passed;
        passed = expectSize(computeRibbonRowCount(1, 128), 1, "ribbon rows: one byte") && passed;
        passed = expectSize(computeRibbonRowCount(128, 128), 1, "ribbon rows: exact one row") && passed;
        passed = expectSize(computeRibbonRowCount(129, 128), 2, "ribbon rows: next row") && passed;
        passed = expectSize(computeRibbonRowCount(9, 1), 9, "ribbon rows: width one") && passed;
        passed = expectSize(computeRibbonRowCount(9, 0), 0, "ribbon rows: zero width guarded") && passed;
    }

    // 7) Seek-offset visibility helper.
    {
        passed = expectTrue(shouldShowSeekOffsets(false, true, true, 1), "seek visible: 2D + valid") && passed;
        passed = expectFalse(shouldShowSeekOffsets(true, true, true, 1), "seek hidden: 3D mode") && passed;
        passed = expectFalse(shouldShowSeekOffsets(false, false, true, 1), "seek hidden: feature disabled") && passed;
        passed = expectFalse(shouldShowSeekOffsets(false, true, false, 1), "seek hidden: invalid") && passed;
        passed = expectFalse(shouldShowSeekOffsets(false, true, true, 0), "seek hidden: no offsets") && passed;
    }

    // 8) Window-edge resize helper.
    {
        // Drag start edge upward (earlier offset) within valid range.
        auto range = resizeWindowRangeFromEdge(1000, 200, 600, 150, true, 2);
        passed = expectSize(range.startInclusive, 150, "resize start: moved upward") && passed;
        passed = expectSize(range.endExclusive, 600, "resize start: end unchanged") && passed;

        // Drag start edge too far down: clamp to preserve min window.
        range = resizeWindowRangeFromEdge(1000, 200, 600, 999, true, 2);
        passed = expectSize(range.startInclusive, 598, "resize start: clamped by min window") && passed;
        passed = expectSize(range.endExclusive, 600, "resize start: clamped end unchanged") && passed;

        // Drag end edge down (later offset) within file.
        range = resizeWindowRangeFromEdge(1000, 200, 600, 750, false, 2);
        passed = expectSize(range.startInclusive, 200, "resize end: start unchanged") && passed;
        passed = expectSize(range.endExclusive, 751, "resize end: moved downward") && passed;

        // Drag end edge above start: clamp to preserve min window.
        range = resizeWindowRangeFromEdge(1000, 200, 600, 50, false, 2);
        passed = expectSize(range.startInclusive, 200, "resize end: start unchanged on clamp") && passed;
        passed = expectSize(range.endExclusive, 202, "resize end: clamped by min window") && passed;

        // File-size edge case.
        range = resizeWindowRangeFromEdge(0, 0, 0, 0, true, 2);
        passed = expectSize(range.startInclusive, 0, "resize empty: start zero") && passed;
        passed = expectSize(range.endExclusive, 0, "resize empty: end zero") && passed;

        // Inverted input range normalizes before resize.
        range = resizeWindowRangeFromEdge(100, 80, 20, 30, true, 4);
        passed = expectSize(range.startInclusive, 30, "resize inverted: normalized start updates from cursor") && passed;
        passed = expectSize(range.endExclusive, 80, "resize inverted: normalized end preserved") && passed;

        // Cursor beyond file clamps to last valid byte.
        range = resizeWindowRangeFromEdge(64, 10, 20, 9999, false, 2);
        passed = expectSize(range.startInclusive, 10, "resize cursor clamp: start unchanged") && passed;
        passed = expectSize(range.endExclusive, 64, "resize cursor clamp: end clamped to file") && passed;

        // minWindow greater than file size expands to full file.
        range = resizeWindowRangeFromEdge(8, 3, 5, 0, false, 1024);
        passed = expectSize(range.startInclusive, 0, "resize min-window>file: start clamped to zero") && passed;
        passed = expectSize(range.endExclusive, 8, "resize min-window>file: full file range") && passed;
    }

    // 9) Step-adjust clamp helper.
    {
        passed = expectInt(adjustByStepClamped(10, 2, 4, 1, 50), 18, "step-clamp: positive delta") && passed;
        passed = expectInt(adjustByStepClamped(10, -3, 4, 1, 50), 1, "step-clamp: lower bound clamp") && passed;
        passed = expectInt(adjustByStepClamped(10, 1000, 1000, 1, 100), 100, "step-clamp: upper bound clamp") && passed;
        passed = expectInt(adjustByStepClamped(10, 0, 4, 1, 50), 10, "step-clamp: zero stepCount") && passed;
        passed = expectInt(adjustByStepClamped(10, 2, 0, 1, 50), 10, "step-clamp: non-positive step size guarded") && passed;
        passed = expectInt(adjustByStepClamped(10, 2, 4, 50, 1), 18, "step-clamp: invalid range normalized") && passed;
    }

    // 10) Ribbon modifier-wheel routing helper.
    {
        // No Ctrl: no action/consume.
        auto wheel = applyRibbonModifierWheel(
            false, false, 1,
            100, 128,
            2, 4096, 256,
            4, 8192, 1);
        passed = expectFalse(wheel.consumed, "ribbon-wheel: no ctrl not consumed") && passed;
        passed = expectTrue(wheel.action == RibbonModifierWheelAction::None, "ribbon-wheel: no ctrl action none") && passed;
        passed = expectInt(wheel.blockSize, 100, "ribbon-wheel: no ctrl block unchanged") && passed;
        passed = expectInt(wheel.ribbonWidth, 128, "ribbon-wheel: no ctrl width unchanged") && passed;

        // Ctrl only: block size changes.
        wheel = applyRibbonModifierWheel(
            true, false, 2,
            100, 128,
            2, 4096, 256,
            4, 8192, 1);
        passed = expectTrue(wheel.consumed, "ribbon-wheel: ctrl consumed") && passed;
        passed = expectTrue(wheel.action == RibbonModifierWheelAction::BlockSize, "ribbon-wheel: ctrl action block") && passed;
        passed = expectInt(wheel.blockSize, 612, "ribbon-wheel: ctrl block updated") && passed;
        passed = expectInt(wheel.ribbonWidth, 128, "ribbon-wheel: ctrl width unchanged") && passed;

        // Ctrl+Shift: ribbon width changes, block preserved.
        wheel = applyRibbonModifierWheel(
            true, true, -3,
            600, 256,
            2, 4096, 256,
            4, 8192, 1);
        passed = expectTrue(wheel.consumed, "ribbon-wheel: ctrl+shift consumed") && passed;
        passed = expectTrue(wheel.action == RibbonModifierWheelAction::RibbonWidth, "ribbon-wheel: ctrl+shift action width") && passed;
        passed = expectInt(wheel.blockSize, 600, "ribbon-wheel: ctrl+shift block unchanged") && passed;
        passed = expectInt(wheel.ribbonWidth, 253, "ribbon-wheel: ctrl+shift width updated") && passed;

        // Ctrl+Shift width clamps at min/max.
        wheel = applyRibbonModifierWheel(
            true, true, 1000,
            600, 8100,
            2, 4096, 256,
            4, 8192, 1);
        passed = expectTrue(wheel.consumed, "ribbon-wheel: ctrl+shift upper clamp still consumed") && passed;
        passed = expectInt(wheel.ribbonWidth, 8192, "ribbon-wheel: ctrl+shift width upper clamp") && passed;

        // Ctrl at lower bound remains clamped but stays consumed.
        wheel = applyRibbonModifierWheel(
            true, false, -1000,
            2, 128,
            2, 4096, 256,
            4, 8192, 1);
        passed = expectTrue(wheel.consumed, "ribbon-wheel: ctrl lower clamp still consumed") && passed;
        passed = expectTrue(wheel.action == RibbonModifierWheelAction::BlockSize, "ribbon-wheel: ctrl lower clamp action block") && passed;
        passed = expectInt(wheel.blockSize, 2, "ribbon-wheel: ctrl lower clamp at min") && passed;

        // Ctrl with zero wheel still no-op.
        wheel = applyRibbonModifierWheel(
            true, false, 0,
            400, 256,
            2, 4096, 256,
            4, 8192, 1);
        passed = expectFalse(wheel.consumed, "ribbon-wheel: zero wheel not consumed") && passed;
        passed = expectTrue(wheel.action == RibbonModifierWheelAction::None, "ribbon-wheel: zero wheel action none") && passed;

        // Reversed bounds are normalized internally.
        wheel = applyRibbonModifierWheel(
            true, false, 1,
            400, 256,
            4096, 2, 256,
            8192, 4, 1);
        passed = expectTrue(wheel.action == RibbonModifierWheelAction::BlockSize, "ribbon-wheel: reversed bounds still block action") && passed;
        passed = expectInt(wheel.blockSize, 656, "ribbon-wheel: reversed block bounds normalized") && passed;
    }

    // 11) Ribbon auto-slide helpers.
    {
        passed = expectFalse(canAutoSlideWindow(0, 0, 0, false, 1.0F), "auto-slide can: empty file") && passed;
        passed = expectFalse(canAutoSlideWindow(100, 0, 50, true, 1.0F), "auto-slide can: full-view enabled") && passed;
        passed = expectFalse(canAutoSlideWindow(100, 0, 100, false, 1.0F), "auto-slide can: full-range window") && passed;
        passed = expectFalse(canAutoSlideWindow(100, 10, 50, false, 0.0F), "auto-slide can: non-positive speed") && passed;
        passed = expectTrue(canAutoSlideWindow(100, 10, 50, false, 0.25F), "auto-slide can: movable window") && passed;

        // Non-repeat: regular forward shift by rows * rowStride.
        auto slide = advanceAutoSlideWindow(1000, 100, 300, 2, 128, false);
        passed = expectSize(slide.range.startInclusive, 356, "auto-slide non-repeat: start advanced") && passed;
        passed = expectSize(slide.range.endExclusive, 556, "auto-slide non-repeat: end advanced") && passed;
        passed = expectTrue(slide.moved, "auto-slide non-repeat: moved") && passed;
        passed = expectFalse(slide.reachedEnd, "auto-slide non-repeat: not at end") && passed;
        passed = expectTrue(slide.shouldContinue, "auto-slide non-repeat: should continue") && passed;

        // Non-repeat: clamp at end and stop.
        slide = advanceAutoSlideWindow(1000, 700, 900, 3, 128, false);
        passed = expectSize(slide.range.startInclusive, 800, "auto-slide non-repeat: clamped start at end") && passed;
        passed = expectSize(slide.range.endExclusive, 1000, "auto-slide non-repeat: clamped end at file") && passed;
        passed = expectTrue(slide.reachedEnd, "auto-slide non-repeat: reached end") && passed;
        passed = expectFalse(slide.shouldContinue, "auto-slide non-repeat: stop at end") && passed;

        // Non-repeat: already at end remains stationary and signals stop.
        slide = advanceAutoSlideWindow(1000, 800, 1000, 1, 128, false);
        passed = expectFalse(slide.moved, "auto-slide non-repeat: stationary at end") && passed;
        passed = expectFalse(slide.shouldContinue, "auto-slide non-repeat: stationary end stop") && passed;

        // Repeat: wraps around and keeps running.
        slide = advanceAutoSlideWindow(1000, 700, 900, 3, 128, true);
        passed = expectSize(slide.range.startInclusive, 283, "auto-slide repeat: wrapped start") && passed;
        passed = expectSize(slide.range.endExclusive, 483, "auto-slide repeat: wrapped end") && passed;
        passed = expectTrue(slide.wrapped, "auto-slide repeat: wrapped flag set") && passed;
        passed = expectTrue(slide.shouldContinue, "auto-slide repeat: continues") && passed;

        // rowsToAdvance == 0 keeps range unchanged and can continue.
        slide = advanceAutoSlideWindow(1000, 250, 450, 0, 128, false);
        passed = expectSize(slide.range.startInclusive, 250, "auto-slide zero rows: start unchanged") && passed;
        passed = expectSize(slide.range.endExclusive, 450, "auto-slide zero rows: end unchanged") && passed;
        passed = expectFalse(slide.moved, "auto-slide zero rows: not moved") && passed;
        passed = expectTrue(slide.shouldContinue, "auto-slide zero rows: continue true") && passed;

        // zero row stride is normalized to one byte per row.
        slide = advanceAutoSlideWindow(100, 10, 30, 5, 0, false);
        passed = expectSize(slide.range.startInclusive, 15, "auto-slide zero stride: normalized to one") && passed;
        passed = expectSize(slide.range.endExclusive, 35, "auto-slide zero stride: window preserved") && passed;

        // Invalid/inverted input range normalizes.
        slide = advanceAutoSlideWindow(100, 80, 20, 1, 10, false);
        passed = expectSize(slide.range.startInclusive, 30, "auto-slide inverted range: normalized then shifted") && passed;
        passed = expectSize(slide.range.endExclusive, 90, "auto-slide inverted range: normalized size preserved") && passed;
    }

    if (passed) {
        std::cout << "[PASS] UILayoutLogicTests" << std::endl;
    }

    return passed;
}

