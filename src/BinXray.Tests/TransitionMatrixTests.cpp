// SPDX-License-Identifier: MIT

#include "Core/TransitionMatrix.h"

#include <iostream>
#include <memory>
#include <vector>

namespace {

bool expectTrue(bool value, const char* caseName) {
    if (value) {
        return true;
    }

    std::cout << "[FAIL] " << caseName << std::endl;
    return false;
}

bool expectEqual(std::uint32_t actual, std::uint32_t expected, const char* caseName) {
    if (actual == expected) {
        return true;
    }

    std::cout << "[FAIL] " << caseName << " expected=" << expected << " actual=" << actual << std::endl;
    return false;
}

bool expectEqual(std::uint8_t actual, std::uint8_t expected, const char* caseName) {
    if (actual == expected) {
        return true;
    }

    std::cout << "[FAIL] " << caseName << " expected=" << static_cast<unsigned int>(expected)
              << " actual=" << static_cast<unsigned int>(actual) << std::endl;
    return false;
}

bool expectEqual(std::size_t actual, std::size_t expected, const char* caseName) {
    if (actual == expected) {
        return true;
    }

    std::cout << "[FAIL] " << caseName << " expected=" << expected << " actual=" << actual << std::endl;
    return false;
}

std::size_t toIndex(std::uint8_t from, std::uint8_t to) {
    return static_cast<std::size_t>(from) * BinXray::Core::TransitionMatrix::kDimension + static_cast<std::size_t>(to);
}

} // namespace

bool runTransitionMatrixTests() {
    bool passed = true;

    {
        const std::vector<std::uint8_t> bytes = {0x10, 0x20, 0x30, 0x20, 0x20};
        auto matrix = std::make_unique<BinXray::Core::TransitionMatrix>();
        matrix->compute(bytes, 0, bytes.size());

        passed = expectEqual(matrix->count(0x10, 0x20), 1, "full range count 0x10->0x20") && passed;
        passed = expectEqual(matrix->count(0x20, 0x30), 1, "full range count 0x20->0x30") && passed;
        passed = expectEqual(matrix->count(0x30, 0x20), 1, "full range count 0x30->0x20") && passed;
        passed = expectEqual(matrix->count(0x20, 0x20), 1, "full range count 0x20->0x20") && passed;
        passed = expectEqual(matrix->maxCount(), 1, "full range max count") && passed;

        matrix->compute(bytes, 1, 4);
        passed = expectEqual(matrix->count(0x20, 0x30), 1, "window count 0x20->0x30") && passed;
        passed = expectEqual(matrix->count(0x30, 0x20), 1, "window count 0x30->0x20") && passed;
        passed = expectEqual(matrix->count(0x10, 0x20), 0, "window excludes first edge") && passed;
    }

    {
        const std::vector<std::uint8_t> bytes = {0x01, 0x02, 0x01, 0x02};
        auto matrix = std::make_unique<BinXray::Core::TransitionMatrix>();
        matrix->compute(bytes, 0, bytes.size());

        BinXray::Core::TransitionMatrix::Luminance buffer = {};
        matrix->renderLuminance(BinXray::Core::TransitionMatrix::RenderMode::Binary, buffer);
        passed = expectEqual(buffer[toIndex(0x01, 0x02)], static_cast<std::uint8_t>(255), "binary intensity present") && passed;
        passed = expectEqual(buffer[toIndex(0x22, 0x22)], static_cast<std::uint8_t>(0), "binary intensity absent") && passed;

        matrix->renderLuminance(BinXray::Core::TransitionMatrix::RenderMode::Linear, buffer);
        passed = expectEqual(buffer[toIndex(0x01, 0x02)], static_cast<std::uint8_t>(2), "linear intensity density=2") && passed;

        matrix->renderLuminance(BinXray::Core::TransitionMatrix::RenderMode::Normalized, buffer);
        passed = expectEqual(buffer[toIndex(0x01, 0x02)], static_cast<std::uint8_t>(128), "normalized intensity density=2") && passed;
        passed = expectEqual(buffer[toIndex(0x02, 0x01)], static_cast<std::uint8_t>(0), "normalized intensity density=1") && passed;
    }

    {
        std::vector<std::uint8_t> bytes(301, 0xAA);
        auto matrix = std::make_unique<BinXray::Core::TransitionMatrix>();
        matrix->compute(bytes, 0, bytes.size());
        BinXray::Core::TransitionMatrix::Luminance buffer = {};
        matrix->renderLuminance(BinXray::Core::TransitionMatrix::RenderMode::Linear, buffer);
        passed = expectEqual(buffer[toIndex(0xAA, 0xAA)], static_cast<std::uint8_t>(255), "linear clamp density>255") && passed;
    }

    {
        const std::vector<std::uint8_t> bytes = {0x01, 0x02, 0x03};
        auto matrix = std::make_unique<BinXray::Core::TransitionMatrix>();
        matrix->compute(bytes, 0, 999);
        passed = expectEqual(matrix->startOffset(), static_cast<std::size_t>(0), "overshoot range clamped start") && passed;
        passed = expectEqual(matrix->endOffset(), bytes.size(), "overshoot range clamped end") && passed;
        passed = expectEqual(matrix->count(0x01, 0x02), 1, "overshoot range count 0x01->0x02") && passed;

        matrix->compute(bytes, 2, 1);
        passed = expectEqual(matrix->maxCount(), static_cast<std::uint32_t>(0), "inverted range maxCount zero") && passed;
        passed = expectEqual(matrix->count(0x01, 0x02), static_cast<std::uint32_t>(0), "inverted range has no counts") && passed;
    }

    if (passed) {
        std::cout << "[PASS] TransitionMatrixTests" << std::endl;
    }

    return passed;
}
