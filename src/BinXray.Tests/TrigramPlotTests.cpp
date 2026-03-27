// SPDX-License-Identifier: MIT
//
// TrigramPlotTests.cpp  --  unit tests for Core::TrigramPlot.
//
// Covers: empty data, too-few bytes, single trigram, overlapping trigrams,
// sub-range scanning, boundary clamping, inverted ranges, repeated patterns,
// maxCount tracking, and mapIntensity modes (binary, linear, normalized).
//

#include "Core/TrigramPlot.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

bool expectEq(std::size_t actual, std::size_t expected, const char* caseName) {
    if (actual == expected) return true;
    std::cout << "[FAIL] " << caseName
              << " expected=" << expected << " actual=" << actual << std::endl;
    return false;
}

bool expectU32(std::uint32_t actual, std::uint32_t expected, const char* caseName) {
    if (actual == expected) return true;
    std::cout << "[FAIL] " << caseName
              << " expected=" << expected << " actual=" << actual << std::endl;
    return false;
}

bool expectU8(std::uint8_t actual, std::uint8_t expected, const char* caseName) {
    if (actual == expected) return true;
    std::cout << "[FAIL] " << caseName
              << " expected=" << static_cast<int>(expected)
              << " actual=" << static_cast<int>(actual) << std::endl;
    return false;
}

} // namespace

bool runTrigramPlotTests() {
    using BinXray::Core::TrigramPlot;
    using BinXray::Core::TrigramPoint;

    bool passed = true;
    TrigramPlot plot;

    // 1. Empty data -- no points, maxCount 0.
    {
        const std::vector<std::uint8_t> empty;
        plot.compute(empty, 0, 0);
        passed = expectEq(plot.points().size(), 0, "empty: points") && passed;
        passed = expectU32(plot.maxCount(), 0, "empty: maxCount") && passed;
    }

    // 2. Single byte -- not enough for a trigram.
    {
        const std::vector<std::uint8_t> one = {0x42};
        plot.compute(one, 0, 1);
        passed = expectEq(plot.points().size(), 0, "single byte: points") && passed;
        passed = expectU32(plot.maxCount(), 0, "single byte: maxCount") && passed;
    }

    // 3. Two bytes -- still not enough (need 3).
    {
        const std::vector<std::uint8_t> two = {0x10, 0x20};
        plot.compute(two, 0, 2);
        passed = expectEq(plot.points().size(), 0, "two bytes: points") && passed;
    }

    // 4. Exactly three bytes -- one trigram.
    {
        const std::vector<std::uint8_t> three = {0x01, 0x02, 0x03};
        plot.compute(three, 0, 3);
        passed = expectEq(plot.points().size(), 1, "three bytes: points") && passed;
        passed = expectU32(plot.maxCount(), 1, "three bytes: maxCount") && passed;
        if (!plot.points().empty()) {
            const auto& pt = plot.points()[0];
            passed = expectU8(pt.x, 0x01, "three bytes: x") && passed;
            passed = expectU8(pt.y, 0x02, "three bytes: y") && passed;
            passed = expectU8(pt.z, 0x03, "three bytes: z") && passed;
            passed = expectU32(pt.count, 1, "three bytes: count") && passed;
        }
    }

    // 5. Four bytes -- two overlapping trigrams with distinct coordinates.
    {
        const std::vector<std::uint8_t> four = {0x0A, 0x0B, 0x0C, 0x0D};
        plot.compute(four, 0, 4);
        passed = expectEq(plot.points().size(), 2, "four bytes: points") && passed;
        passed = expectU32(plot.maxCount(), 1, "four bytes: maxCount") && passed;
    }

    // 6. Repeated pattern -- same trigram accumulates count.
    {
        // 0x10,0x20,0x30 repeated: bytes[0..2] and bytes[3..5].
        const std::vector<std::uint8_t> data = {
            0x10, 0x20, 0x30, 0x10, 0x20, 0x30
        };
        plot.compute(data, 0, data.size());
        // Trigrams: (10,20,30) at i=0, (20,30,10) at i=1, (30,10,20) at i=2, (10,20,30) at i=3
        // So (10,20,30) appears twice, the others once each.
        passed = expectU32(plot.maxCount(), 2, "repeated: maxCount") && passed;
        // Find the twice-occurring point.
        bool foundDouble = false;
        for (const auto& pt : plot.points()) {
            if (pt.x == 0x10 && pt.y == 0x20 && pt.z == 0x30) {
                passed = expectU32(pt.count, 2, "repeated: (10,20,30) count") && passed;
                foundDouble = true;
            }
        }
        if (!foundDouble) {
            std::cout << "[FAIL] repeated: (10,20,30) not found" << std::endl;
            passed = false;
        }
    }

    // 7. Sub-range scan -- only considers bytes in [start, end).
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
        // Sub-range [1,4) -> bytes {0xBB, 0xCC, 0xDD} -> one trigram.
        plot.compute(data, 1, 4);
        passed = expectEq(plot.points().size(), 1, "sub-range: points") && passed;
        if (!plot.points().empty()) {
            const auto& pt = plot.points()[0];
            passed = expectU8(pt.x, 0xBB, "sub-range: x") && passed;
            passed = expectU8(pt.y, 0xCC, "sub-range: y") && passed;
            passed = expectU8(pt.z, 0xDD, "sub-range: z") && passed;
        }
    }

    // 8. Inverted range (start > end) -- should produce no points.
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        plot.compute(data, 3, 1);
        passed = expectEq(plot.points().size(), 0, "inverted range: points") && passed;
    }

    // 9. Range exceeds vector size -- clamped to actual size.
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB, 0xCC};
        plot.compute(data, 0, 999);
        passed = expectEq(plot.points().size(), 1, "overshoot range: points") && passed;
        passed = expectU32(plot.maxCount(), 1, "overshoot range: maxCount") && passed;
    }

    // 10. Sub-range too small (< 3 bytes in effective range).
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        plot.compute(data, 1, 3);  // Only bytes[1] and bytes[2] -> 2 bytes, no trigram.
        passed = expectEq(plot.points().size(), 0, "sub-range too small: points") && passed;
    }

    // 11. All-zero bytes -- single voxel at (0,0,0).
    {
        const std::vector<std::uint8_t> data(10, 0x00);
        plot.compute(data, 0, data.size());
        passed = expectEq(plot.points().size(), 1, "all-zeros: points") && passed;
        if (!plot.points().empty()) {
            const auto& pt = plot.points()[0];
            passed = expectU8(pt.x, 0, "all-zeros: x") && passed;
            passed = expectU8(pt.y, 0, "all-zeros: y") && passed;
            passed = expectU8(pt.z, 0, "all-zeros: z") && passed;
            passed = expectU32(pt.count, 8, "all-zeros: count") && passed;
        }
    }

    // 12. Boundary byte values (0xFF) -- mapped correctly.
    {
        const std::vector<std::uint8_t> data = {0xFF, 0xFF, 0xFF};
        plot.compute(data, 0, data.size());
        passed = expectEq(plot.points().size(), 1, "0xFF boundary: points") && passed;
        if (!plot.points().empty()) {
            const auto& pt = plot.points()[0];
            passed = expectU8(pt.x, 0xFF, "0xFF boundary: x") && passed;
            passed = expectU8(pt.y, 0xFF, "0xFF boundary: y") && passed;
            passed = expectU8(pt.z, 0xFF, "0xFF boundary: z") && passed;
        }
    }

    // ---- mapIntensity tests -------------------------------------------------

    // 13. Binary mode: any non-zero count -> 255.
    {
        passed = expectU8(TrigramPlot::mapIntensity(0, 100, false, false),
                          0, "binary count=0") && passed;
        passed = expectU8(TrigramPlot::mapIntensity(1, 100, false, false),
                          255, "binary count=1") && passed;
        passed = expectU8(TrigramPlot::mapIntensity(999, 100, false, false),
                          255, "binary count=999") && passed;
    }

    // 14. Linear clamp mode: min(count, 255).
    {
        passed = expectU8(TrigramPlot::mapIntensity(0, 100, true, false),
                          0, "linear count=0") && passed;
        passed = expectU8(TrigramPlot::mapIntensity(1, 100, true, false),
                          1, "linear count=1") && passed;
        passed = expectU8(TrigramPlot::mapIntensity(128, 200, true, false),
                          128, "linear count=128") && passed;
        passed = expectU8(TrigramPlot::mapIntensity(255, 300, true, false),
                          255, "linear count=255") && passed;
        passed = expectU8(TrigramPlot::mapIntensity(500, 1000, true, false),
                          255, "linear count>255") && passed;
    }

    // 15. Normalized mode: count * 255 / maxCount.
    {
        passed = expectU8(TrigramPlot::mapIntensity(0, 100, true, true),
                          0, "normalized count=0") && passed;
        passed = expectU8(TrigramPlot::mapIntensity(100, 100, true, true),
                          255, "normalized count=max") && passed;
        passed = expectU8(TrigramPlot::mapIntensity(50, 100, true, true),
                          127, "normalized count=half") && passed;
        // maxCount = 0 edge case -> 0.
        passed = expectU8(TrigramPlot::mapIntensity(1, 0, true, true),
                          0, "normalized maxCount=0") && passed;
    }

    // 16. Compute clears previous state.
    {
        const std::vector<std::uint8_t> data1 = {0x01, 0x02, 0x03};
        plot.compute(data1, 0, data1.size());
        passed = expectEq(plot.points().size(), 1, "recompute phase1: points") && passed;

        const std::vector<std::uint8_t> empty;
        plot.compute(empty, 0, 0);
        passed = expectEq(plot.points().size(), 0, "recompute phase2: cleared") && passed;
        passed = expectU32(plot.maxCount(), 0, "recompute phase2: maxCount") && passed;
    }

    if (passed) {
        std::cout << "[PASS] TrigramPlotTests" << std::endl;
    }

    return passed;
}
