// SPDX-License-Identifier: MIT
//
// CrosshairCoordsTests.cpp  --  unit tests for Core crosshair coordinate helpers.
//
// Covers both single-point (transitionCoordAt / trigramCoordAt) and
// multi-point (transitionCoordsAt / trigramCoordsAt) functions.
//
// Multi-point semantics:
//   2D: byte at offset i participates in up to 2 pairs
//       pair 0: (bytes[i-1] → bytes[i])    — selected byte as "to"
//       pair 1: (bytes[i]   → bytes[i+1])  — selected byte as "from"
//   3D: byte at offset i participates in up to 3 trigrams
//       trigram 0: (bytes[i-2], bytes[i-1], bytes[i])    — as z
//       trigram 1: (bytes[i-1], bytes[i],   bytes[i+1])  — as y
//       trigram 2: (bytes[i],   bytes[i+1], bytes[i+2])  — as x
//

#include "Core/CrosshairCoords.h"

#include <iostream>
#include <vector>

namespace {

bool expectTrue(bool condition, const char* caseName) {
    if (condition) return true;
    std::cout << "[FAIL] " << caseName << std::endl;
    return false;
}

bool expectFalse(bool condition, const char* caseName) {
    if (!condition) return true;
    std::cout << "[FAIL] " << caseName << " (expected nullopt)" << std::endl;
    return false;
}

bool expectU8(std::uint8_t actual, std::uint8_t expected, const char* caseName) {
    if (actual == expected) return true;
    std::cout << "[FAIL] " << caseName
              << " expected=" << static_cast<int>(expected)
              << " actual=" << static_cast<int>(actual) << std::endl;
    return false;
}

bool expectSz(std::size_t actual, std::size_t expected, const char* caseName) {
    if (actual == expected) return true;
    std::cout << "[FAIL] " << caseName
              << " expected=" << expected << " actual=" << actual << std::endl;
    return false;
}

} // namespace

bool runCrosshairCoordsTests() {
    using BinXray::Core::transitionCoordAt;
    using BinXray::Core::trigramCoordAt;
    using BinXray::Core::transitionCoordsAt;
    using BinXray::Core::trigramCoordsAt;

    bool passed = true;

    // =======================================================================
    // Single-point backward-compat: transitionCoordAt
    // =======================================================================

    // 1. Empty data -- always nullopt.
    {
        const std::vector<std::uint8_t> empty;
        passed = expectFalse(transitionCoordAt(empty, 0).has_value(), "2D empty offset=0") && passed;
        passed = expectFalse(transitionCoordAt(empty, 1).has_value(), "2D empty offset=1") && passed;
    }

    // 2. Single byte -- offset 0 has no predecessor.
    {
        const std::vector<std::uint8_t> one = {0x42};
        passed = expectFalse(transitionCoordAt(one, 0).has_value(), "2D single byte offset=0") && passed;
        passed = expectFalse(transitionCoordAt(one, 1).has_value(), "2D single byte offset=1 OOB") && passed;
    }

    // 3. Two bytes -- offset 1 is the first valid pair.
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB};
        passed = expectFalse(transitionCoordAt(data, 0).has_value(), "2D two bytes offset=0") && passed;
        auto c = transitionCoordAt(data, 1);
        passed = expectTrue(c.has_value(), "2D two bytes offset=1 has_value") && passed;
        if (c) {
            passed = expectU8(c->fromByte, 0xAA, "2D two bytes fromByte") && passed;
            passed = expectU8(c->toByte, 0xBB, "2D two bytes toByte") && passed;
        }
    }

    // 4. Out of range offset.
    {
        const std::vector<std::uint8_t> data = {0x10, 0x20, 0x30};
        passed = expectFalse(transitionCoordAt(data, 3).has_value(), "2D offset==size OOB") && passed;
        passed = expectFalse(transitionCoordAt(data, 100).has_value(), "2D offset>>size OOB") && passed;
    }

    // 5. Boundary byte values 0x00 and 0xFF.
    {
        const std::vector<std::uint8_t> data = {0x00, 0xFF};
        auto c = transitionCoordAt(data, 1);
        passed = expectTrue(c.has_value(), "2D boundary has_value") && passed;
        if (c) {
            passed = expectU8(c->fromByte, 0x00, "2D boundary fromByte") && passed;
            passed = expectU8(c->toByte, 0xFF, "2D boundary toByte") && passed;
        }
    }

    // =======================================================================
    // Single-point backward-compat: trigramCoordAt
    // =======================================================================

    // 6. Empty data.
    {
        const std::vector<std::uint8_t> empty;
        passed = expectFalse(trigramCoordAt(empty, 0).has_value(), "3D empty offset=0") && passed;
    }

    // 7. Two bytes -- not enough for a trigram at any offset.
    {
        const std::vector<std::uint8_t> two = {0xAA, 0xBB};
        passed = expectFalse(trigramCoordAt(two, 0).has_value(), "3D two bytes offset=0") && passed;
        passed = expectFalse(trigramCoordAt(two, 1).has_value(), "3D two bytes offset=1") && passed;
    }

    // 8. Three bytes -- offset 2 is the first valid trigram.
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB, 0xCC};
        auto c = trigramCoordAt(data, 2);
        passed = expectTrue(c.has_value(), "3D three bytes offset=2 has_value") && passed;
        if (c) {
            passed = expectU8(c->x, 0xAA, "3D three bytes x") && passed;
            passed = expectU8(c->y, 0xBB, "3D three bytes y") && passed;
            passed = expectU8(c->z, 0xCC, "3D three bytes z") && passed;
        }
    }

    // 9. SIZE_MAX offset (extreme out-of-range).
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x03};
        passed = expectFalse(transitionCoordAt(data, static_cast<std::size_t>(-1)).has_value(),
                             "2D SIZE_MAX OOB") && passed;
        passed = expectFalse(trigramCoordAt(data, static_cast<std::size_t>(-1)).has_value(),
                             "3D SIZE_MAX OOB") && passed;
    }

    // =======================================================================
    // Multi-point 2D: transitionCoordsAt
    // =======================================================================

    // 10. Empty data -- count 0.
    {
        const std::vector<std::uint8_t> empty;
        auto r = transitionCoordsAt(empty, 0);
        passed = expectSz(r.count, 0, "multi2D empty") && passed;
    }

    // 11. Single byte, offset 0 -- only a "from" pair is possible if offset+1 exists.
    //     With only 1 byte there is no offset+1, so count=0.
    {
        const std::vector<std::uint8_t> one = {0x42};
        auto r = transitionCoordsAt(one, 0);
        passed = expectSz(r.count, 0, "multi2D single byte") && passed;
    }

    // 12. Two bytes, offset 0 -- no predecessor, but successor exists → 1 pair (as "from").
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB};
        auto r = transitionCoordsAt(data, 0);
        passed = expectSz(r.count, 1, "multi2D offset=0 count") && passed;
        if (r.count >= 1) {
            // Pair as "from": (bytes[0] → bytes[1])
            passed = expectU8(r.coords[0].fromByte, 0xAA, "multi2D offset=0 fromByte") && passed;
            passed = expectU8(r.coords[0].toByte, 0xBB, "multi2D offset=0 toByte") && passed;
        }
    }

    // 13. Two bytes, offset 1 (last) -- predecessor exists, no successor → 1 pair (as "to").
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB};
        auto r = transitionCoordsAt(data, 1);
        passed = expectSz(r.count, 1, "multi2D last of 2 count") && passed;
        if (r.count >= 1) {
            passed = expectU8(r.coords[0].fromByte, 0xAA, "multi2D last of 2 fromByte") && passed;
            passed = expectU8(r.coords[0].toByte, 0xBB, "multi2D last of 2 toByte") && passed;
        }
    }

    // 14. Three bytes, offset 1 (middle) -- both predecessor and successor → 2 pairs.
    {
        const std::vector<std::uint8_t> data = {0x10, 0x20, 0x30};
        auto r = transitionCoordsAt(data, 1);
        passed = expectSz(r.count, 2, "multi2D middle count") && passed;
        if (r.count >= 2) {
            // Pair 0: as "to" → (0x10 → 0x20)
            passed = expectU8(r.coords[0].fromByte, 0x10, "multi2D mid pair0 from") && passed;
            passed = expectU8(r.coords[0].toByte, 0x20, "multi2D mid pair0 to") && passed;
            // Pair 1: as "from" → (0x20 → 0x30)
            passed = expectU8(r.coords[1].fromByte, 0x20, "multi2D mid pair1 from") && passed;
            passed = expectU8(r.coords[1].toByte, 0x30, "multi2D mid pair1 to") && passed;
        }
    }

    // 15. Five bytes, offset 2 (interior) -- 2 pairs.
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto r = transitionCoordsAt(data, 2);
        passed = expectSz(r.count, 2, "multi2D interior count") && passed;
        if (r.count >= 2) {
            passed = expectU8(r.coords[0].fromByte, 0x02, "multi2D int pair0 from") && passed;
            passed = expectU8(r.coords[0].toByte, 0x03, "multi2D int pair0 to") && passed;
            passed = expectU8(r.coords[1].fromByte, 0x03, "multi2D int pair1 from") && passed;
            passed = expectU8(r.coords[1].toByte, 0x04, "multi2D int pair1 to") && passed;
        }
    }

    // 16. Self-transition -- both pairs are (0x55 → 0x55).
    {
        const std::vector<std::uint8_t> data = {0x55, 0x55, 0x55};
        auto r = transitionCoordsAt(data, 1);
        passed = expectSz(r.count, 2, "multi2D self-trans count") && passed;
        if (r.count >= 2) {
            passed = expectU8(r.coords[0].fromByte, 0x55, "multi2D self p0 from") && passed;
            passed = expectU8(r.coords[0].toByte, 0x55, "multi2D self p0 to") && passed;
            passed = expectU8(r.coords[1].fromByte, 0x55, "multi2D self p1 from") && passed;
            passed = expectU8(r.coords[1].toByte, 0x55, "multi2D self p1 to") && passed;
        }
    }

    // 17. OOB offset returns count=0.
    {
        const std::vector<std::uint8_t> data = {0x10, 0x20};
        auto r = transitionCoordsAt(data, 5);
        passed = expectSz(r.count, 0, "multi2D OOB") && passed;
    }

    // 18. Boundary bytes (0x00, 0xFF) -- offset 0 as "from".
    {
        const std::vector<std::uint8_t> data = {0x00, 0xFF};
        auto r = transitionCoordsAt(data, 0);
        passed = expectSz(r.count, 1, "multi2D boundary count") && passed;
        if (r.count >= 1) {
            passed = expectU8(r.coords[0].fromByte, 0x00, "multi2D boundary from") && passed;
            passed = expectU8(r.coords[0].toByte, 0xFF, "multi2D boundary to") && passed;
        }
    }

    // =======================================================================
    // Multi-point 3D: trigramCoordsAt
    // =======================================================================

    // 19. Empty data.
    {
        const std::vector<std::uint8_t> empty;
        auto r = trigramCoordsAt(empty, 0);
        passed = expectSz(r.count, 0, "multi3D empty") && passed;
    }

    // 20. Single byte -- no trigrams possible (only "x" role needs +2 ahead).
    {
        const std::vector<std::uint8_t> one = {0x42};
        auto r = trigramCoordsAt(one, 0);
        passed = expectSz(r.count, 0, "multi3D single byte") && passed;
    }

    // 21. Two bytes -- not enough for any trigram at any offset.
    {
        const std::vector<std::uint8_t> two = {0xAA, 0xBB};
        passed = expectSz(trigramCoordsAt(two, 0).count, 0, "multi3D 2 bytes off=0") && passed;
        passed = expectSz(trigramCoordsAt(two, 1).count, 0, "multi3D 2 bytes off=1") && passed;
    }

    // 22. Three bytes, offset 0 -- only x-role: (bytes[0],bytes[1],bytes[2]) → 1 trigram.
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB, 0xCC};
        auto r = trigramCoordsAt(data, 0);
        passed = expectSz(r.count, 1, "multi3D 3bytes off=0 count") && passed;
        if (r.count >= 1) {
            passed = expectU8(r.coords[0].x, 0xAA, "multi3D 3bytes off=0 x") && passed;
            passed = expectU8(r.coords[0].y, 0xBB, "multi3D 3bytes off=0 y") && passed;
            passed = expectU8(r.coords[0].z, 0xCC, "multi3D 3bytes off=0 z") && passed;
        }
    }

    // 23. Three bytes, offset 1 -- y-role AND x-role? y needs [0,1,2], x needs [1,2,3]=OOB → 1.
    //     Actually: z-role needs offset>=2 (no), y-role needs offset>=1 && offset+1<3 (yes: [0,1,2]),
    //     x-role needs offset+2<3 → 1+2=3, not<3, so no. → 1 trigram.
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB, 0xCC};
        auto r = trigramCoordsAt(data, 1);
        passed = expectSz(r.count, 1, "multi3D 3bytes off=1 count") && passed;
        if (r.count >= 1) {
            // y-role: (bytes[0], bytes[1], bytes[2])
            passed = expectU8(r.coords[0].x, 0xAA, "multi3D 3bytes off=1 x") && passed;
            passed = expectU8(r.coords[0].y, 0xBB, "multi3D 3bytes off=1 y") && passed;
            passed = expectU8(r.coords[0].z, 0xCC, "multi3D 3bytes off=1 z") && passed;
        }
    }

    // 24. Three bytes, offset 2 -- z-role only: (bytes[0],bytes[1],bytes[2]) → 1 trigram.
    {
        const std::vector<std::uint8_t> data = {0xAA, 0xBB, 0xCC};
        auto r = trigramCoordsAt(data, 2);
        passed = expectSz(r.count, 1, "multi3D 3bytes off=2 count") && passed;
        if (r.count >= 1) {
            passed = expectU8(r.coords[0].x, 0xAA, "multi3D 3bytes off=2 x") && passed;
            passed = expectU8(r.coords[0].y, 0xBB, "multi3D 3bytes off=2 y") && passed;
            passed = expectU8(r.coords[0].z, 0xCC, "multi3D 3bytes off=2 z") && passed;
        }
    }

    // 25. Five bytes, offset 2 (interior) -- all 3 roles.
    //     z-role: (bytes[0],bytes[1],bytes[2]) = (01,02,03)
    //     y-role: (bytes[1],bytes[2],bytes[3]) = (02,03,04)
    //     x-role: (bytes[2],bytes[3],bytes[4]) = (03,04,05)
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto r = trigramCoordsAt(data, 2);
        passed = expectSz(r.count, 3, "multi3D interior count") && passed;
        if (r.count >= 3) {
            // trigram 0 (z-role)
            passed = expectU8(r.coords[0].x, 0x01, "multi3D int t0 x") && passed;
            passed = expectU8(r.coords[0].y, 0x02, "multi3D int t0 y") && passed;
            passed = expectU8(r.coords[0].z, 0x03, "multi3D int t0 z") && passed;
            // trigram 1 (y-role)
            passed = expectU8(r.coords[1].x, 0x02, "multi3D int t1 x") && passed;
            passed = expectU8(r.coords[1].y, 0x03, "multi3D int t1 y") && passed;
            passed = expectU8(r.coords[1].z, 0x04, "multi3D int t1 z") && passed;
            // trigram 2 (x-role)
            passed = expectU8(r.coords[2].x, 0x03, "multi3D int t2 x") && passed;
            passed = expectU8(r.coords[2].y, 0x04, "multi3D int t2 y") && passed;
            passed = expectU8(r.coords[2].z, 0x05, "multi3D int t2 z") && passed;
        }
    }

    // 26. Five bytes, offset 0 -- only x-role: (0,1,2).
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto r = trigramCoordsAt(data, 0);
        passed = expectSz(r.count, 1, "multi3D off=0 count") && passed;
        if (r.count >= 1) {
            passed = expectU8(r.coords[0].x, 0x01, "multi3D off=0 x") && passed;
            passed = expectU8(r.coords[0].y, 0x02, "multi3D off=0 y") && passed;
            passed = expectU8(r.coords[0].z, 0x03, "multi3D off=0 z") && passed;
        }
    }

    // 27. Five bytes, offset 1 -- y-role and x-role (2 trigrams).
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto r = trigramCoordsAt(data, 1);
        passed = expectSz(r.count, 2, "multi3D off=1 count") && passed;
        if (r.count >= 2) {
            // y-role: (bytes[0], bytes[1], bytes[2])
            passed = expectU8(r.coords[0].x, 0x01, "multi3D off=1 t0 x") && passed;
            passed = expectU8(r.coords[0].y, 0x02, "multi3D off=1 t0 y") && passed;
            passed = expectU8(r.coords[0].z, 0x03, "multi3D off=1 t0 z") && passed;
            // x-role: (bytes[1], bytes[2], bytes[3])
            passed = expectU8(r.coords[1].x, 0x02, "multi3D off=1 t1 x") && passed;
            passed = expectU8(r.coords[1].y, 0x03, "multi3D off=1 t1 y") && passed;
            passed = expectU8(r.coords[1].z, 0x04, "multi3D off=1 t1 z") && passed;
        }
    }

    // 28. Five bytes, offset 4 (last) -- only z-role: (bytes[2],bytes[3],bytes[4]).
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto r = trigramCoordsAt(data, 4);
        passed = expectSz(r.count, 1, "multi3D last count") && passed;
        if (r.count >= 1) {
            passed = expectU8(r.coords[0].x, 0x03, "multi3D last x") && passed;
            passed = expectU8(r.coords[0].y, 0x04, "multi3D last y") && passed;
            passed = expectU8(r.coords[0].z, 0x05, "multi3D last z") && passed;
        }
    }

    // 29. Five bytes, offset 3 -- z-role and y-role (2 trigrams).
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto r = trigramCoordsAt(data, 3);
        passed = expectSz(r.count, 2, "multi3D off=3 count") && passed;
        if (r.count >= 2) {
            // z-role: (bytes[1], bytes[2], bytes[3])
            passed = expectU8(r.coords[0].x, 0x02, "multi3D off=3 t0 x") && passed;
            passed = expectU8(r.coords[0].y, 0x03, "multi3D off=3 t0 y") && passed;
            passed = expectU8(r.coords[0].z, 0x04, "multi3D off=3 t0 z") && passed;
            // y-role: (bytes[2], bytes[3], bytes[4])
            passed = expectU8(r.coords[1].x, 0x03, "multi3D off=3 t1 x") && passed;
            passed = expectU8(r.coords[1].y, 0x04, "multi3D off=3 t1 y") && passed;
            passed = expectU8(r.coords[1].z, 0x05, "multi3D off=3 t1 z") && passed;
        }
    }

    // 30. All-same bytes -- all 3 trigrams are identical.
    {
        const std::vector<std::uint8_t> data = {0x77, 0x77, 0x77, 0x77, 0x77};
        auto r = trigramCoordsAt(data, 2);
        passed = expectSz(r.count, 3, "multi3D same count") && passed;
        for (std::size_t i = 0; i < r.count; ++i) {
            passed = expectU8(r.coords[i].x, 0x77, "multi3D same x") && passed;
            passed = expectU8(r.coords[i].y, 0x77, "multi3D same y") && passed;
            passed = expectU8(r.coords[i].z, 0x77, "multi3D same z") && passed;
        }
    }

    // 31. OOB offset returns count=0.
    {
        const std::vector<std::uint8_t> data = {0x01, 0x02, 0x03};
        passed = expectSz(trigramCoordsAt(data, 5).count, 0, "multi3D OOB") && passed;
        passed = expectSz(trigramCoordsAt(data, static_cast<std::size_t>(-1)).count, 0,
                           "multi3D SIZE_MAX") && passed;
    }

    // 32. 2D multi vs single consistency: single should match multi's first entry for interior.
    {
        const std::vector<std::uint8_t> data = {0x10, 0x20, 0x30, 0x40};
        auto single = transitionCoordAt(data, 2);
        auto multi  = transitionCoordsAt(data, 2);
        passed = expectTrue(single.has_value() && multi.count >= 1, "2D multi/single exist") && passed;
        if (single && multi.count >= 1) {
            passed = expectU8(single->fromByte, multi.coords[0].fromByte, "2D multi/single from") && passed;
            passed = expectU8(single->toByte, multi.coords[0].toByte, "2D multi/single to") && passed;
        }
    }

    // 33. 3D multi vs single consistency.
    {
        const std::vector<std::uint8_t> data = {0x10, 0x20, 0x30, 0x40, 0x50};
        auto single = trigramCoordAt(data, 3);
        auto multi  = trigramCoordsAt(data, 3);
        passed = expectTrue(single.has_value() && multi.count >= 1, "3D multi/single exist") && passed;
        if (single && multi.count >= 1) {
            passed = expectU8(single->x, multi.coords[0].x, "3D multi/single x") && passed;
            passed = expectU8(single->y, multi.coords[0].y, "3D multi/single y") && passed;
            passed = expectU8(single->z, multi.coords[0].z, "3D multi/single z") && passed;
        }
    }

    if (passed) {
        std::cout << "[PASS] CrosshairCoordsTests" << std::endl;
    }

    return passed;
}
