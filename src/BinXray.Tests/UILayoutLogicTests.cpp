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

    if (passed) {
        std::cout << "[PASS] UILayoutLogicTests" << std::endl;
    }

    return passed;
}

