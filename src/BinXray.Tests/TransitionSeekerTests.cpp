// SPDX-License-Identifier: MIT
//
// TransitionSeekerTests.cpp  --  unit tests for Core::findTransitionOffsets.
//
// Covers: empty data, single byte, no match, single match, multiple matches,
// maxResults cap, inverted/out-of-bounds ranges, and full-range scans.
//

#include "Core/TransitionSeeker.h"

#include <iostream>
#include <vector>

namespace {

bool expectEqual(std::uint32_t actual, std::uint32_t expected, const char* caseName) {
    if (actual == expected) {
        return true;
    }
    std::cout << "[FAIL] " << caseName
              << " expected=" << expected << " actual=" << actual << std::endl;
    return false;
}

bool expectSizeEqual(std::size_t actual, std::size_t expected, const char* caseName) {
    if (actual == expected) {
        return true;
    }
    std::cout << "[FAIL] " << caseName
              << " expected=" << expected << " actual=" << actual << std::endl;
    return false;
}

bool expectOffsetEqual(std::size_t actual, std::size_t expected, const char* caseName) {
    if (actual == expected) {
        return true;
    }
    std::cout << "[FAIL] " << caseName
              << " offset expected=" << expected << " actual=" << actual << std::endl;
    return false;
}

} // namespace

bool runTransitionSeekerTests() {
    bool passed = true;

    // 1. Empty byte vector returns zero count and no offsets.
    {
        const std::vector<std::uint8_t> empty;
        auto r = BinXray::Core::findTransitionOffsets(empty, 0, 0, 0x00, 0x01, 100);
        passed = expectEqual(r.transitionCount, 0, "empty: count") && passed;
        passed = expectSizeEqual(r.offsets.size(), 0, "empty: offsets") && passed;
    }

    // 2. Single byte  --  no pairs can be formed.
    {
        const std::vector<std::uint8_t> one = {0xAA};
        auto r = BinXray::Core::findTransitionOffsets(one, 0, 1, 0xAA, 0xAA, 100);
        passed = expectEqual(r.transitionCount, 0, "single byte: count") && passed;
        passed = expectSizeEqual(r.offsets.size(), 0, "single byte: offsets") && passed;
    }

    // 3. Two bytes with a matching pair.
    {
        const std::vector<std::uint8_t> two = {0x10, 0x20};
        auto r = BinXray::Core::findTransitionOffsets(two, 0, 2, 0x10, 0x20, 100);
        passed = expectEqual(r.transitionCount, 1, "two bytes match: count") && passed;
        passed = expectSizeEqual(r.offsets.size(), 1, "two bytes match: offsets") && passed;
        if (!r.offsets.empty()) {
            passed = expectOffsetEqual(r.offsets[0], 1, "two bytes match: offset[0]") && passed;
        }
    }

    // 4. Two bytes with no match.
    {
        const std::vector<std::uint8_t> two = {0x10, 0x20};
        auto r = BinXray::Core::findTransitionOffsets(two, 0, 2, 0x20, 0x10, 100);
        passed = expectEqual(r.transitionCount, 0, "two bytes no match: count") && passed;
    }

    // 5. Multiple matches -- ensure offsets are ascending.
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB};
        auto r = BinXray::Core::findTransitionOffsets(data, 0, data.size(), 0xAA, 0xBB, 100);
        passed = expectEqual(r.transitionCount, 3, "multi match: count") && passed;
        passed = expectSizeEqual(r.offsets.size(), 3, "multi match: offsets") && passed;
        if (r.offsets.size() == 3) {
            passed = expectOffsetEqual(r.offsets[0], 1, "multi match: offset[0]") && passed;
            passed = expectOffsetEqual(r.offsets[1], 3, "multi match: offset[1]") && passed;
            passed = expectOffsetEqual(r.offsets[2], 5, "multi match: offset[2]") && passed;
        }
    }

    // 6. maxResults cap -- total count still tallied, offsets capped.
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB, 0xAA, 0xBB, 0xAA, 0xBB};
        auto r = BinXray::Core::findTransitionOffsets(data, 0, data.size(), 0xAA, 0xBB, 2);
        passed = expectEqual(r.transitionCount, 3, "maxResults cap: count") && passed;
        passed = expectSizeEqual(r.offsets.size(), 2, "maxResults cap: offsets") && passed;
    }

    // 7. maxResults = 0 -- should still count but never store offsets.
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x01, 0x02};
        auto r = BinXray::Core::findTransitionOffsets(data, 0, data.size(), 0x01, 0x02, 0);
        passed = expectEqual(r.transitionCount, 2, "maxResults=0: count") && passed;
        passed = expectSizeEqual(r.offsets.size(), 0, "maxResults=0: offsets") && passed;
    }

    // 8. Sub-range scan -- only matches within [rangeStart, rangeEnd).
    {
        // indices: 0:10  1:20  2:10  3:20  4:10  5:20
        const std::vector<std::uint8_t> data = {0x10, 0x20, 0x10, 0x20, 0x10, 0x20};
        // Scan [2, 5) -> pairs at indices 3 and 4: (10->20) at 3, (20->10) at 4
        auto r = BinXray::Core::findTransitionOffsets(data, 2, 5, 0x10, 0x20, 100);
        passed = expectEqual(r.transitionCount, 1, "sub-range: count") && passed;
        if (!r.offsets.empty()) {
            passed = expectOffsetEqual(r.offsets[0], 3, "sub-range: offset[0]") && passed;
        }
    }

    // 9. Range start past end -- degenerate, should return empty.
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x03};
        auto r = BinXray::Core::findTransitionOffsets(data, 5, 2, 0x01, 0x02, 100);
        passed = expectEqual(r.transitionCount, 0, "inverted range: count") && passed;
    }

    // 10. Range exceeds vector size -- clamped safely.
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB};
        auto r = BinXray::Core::findTransitionOffsets(data, 0, 999, 0xAA, 0xBB, 100);
        passed = expectEqual(r.transitionCount, 1, "overshoot range: count") && passed;
    }

    // 11. Self-transition (same byte value).
    {
        const std::vector<std::uint8_t> data = {0xFF, 0xFF, 0xFF};
        auto r = BinXray::Core::findTransitionOffsets(data, 0, data.size(), 0xFF, 0xFF, 100);
        passed = expectEqual(r.transitionCount, 2, "self-transition: count") && passed;
        passed = expectSizeEqual(r.offsets.size(), 2, "self-transition: offsets") && passed;
    }

    // 12. Result struct stores fromByte/toByte correctly.
    {
        const std::vector<std::uint8_t> data = {0xDE, 0xAD};
        auto r = BinXray::Core::findTransitionOffsets(data, 0, data.size(), 0xDE, 0xAD, 100);
        passed = expectEqual(r.fromByte, 0xDE, "result fields: fromByte") && passed;
        passed = expectEqual(r.toByte, 0xAD, "result fields: toByte") && passed;
    }

    if (passed) {
        std::cout << "[PASS] TransitionSeekerTests" << std::endl;
    }

    return passed;
}
