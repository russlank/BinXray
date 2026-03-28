// SPDX-License-Identifier: MIT

#include "Core/BinaryDocument.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <fstream>
#include <iostream>
#include <vector>

namespace {

bool expectTrue(bool value, const char* caseName) {
    if (value) {
        return true;
    }

    std::cout << "[FAIL] " << caseName << std::endl;
    return false;
}

bool expectEqual(const std::vector<std::uint8_t>& actual, const std::vector<std::uint8_t>& expected, const char* caseName) {
    if (actual == expected) {
        return true;
    }

    std::cout << "[FAIL] " << caseName << " byte mismatch (actual=" << actual.size() << ", expected=" << expected.size() << ")" << std::endl;
    return false;
}

bool expectEqual(const std::wstring& actual, const std::wstring& expected, const char* caseName) {
    if (actual == expected) {
        return true;
    }

    std::wcout << L"[FAIL] " << caseName << L" expected='" << expected << L"' actual='" << actual << L"'" << std::endl;
    return false;
}

std::wstring createTempFilePath() {
    wchar_t tempPath[MAX_PATH] = {};
    if (::GetTempPathW(MAX_PATH, tempPath) == 0) {
        return L"";
    }

    wchar_t tempFilePath[MAX_PATH] = {};
    if (::GetTempFileNameW(tempPath, L"BXR", 0, tempFilePath) == 0) {
        return L"";
    }

    return std::wstring(tempFilePath);
}

std::wstring createMissingTempFilePath() {
    std::wstring tempFilePath = createTempFilePath();
    if (tempFilePath.empty()) {
        return {};
    }

    ::DeleteFileW(tempFilePath.c_str());
    return tempFilePath;
}

bool writeFile(const std::wstring& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    if (!bytes.empty()) {
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    return file.good();
}

} // namespace

bool runBinaryDocumentTests() {
    bool passed = true;

    const std::wstring missingPath = createMissingTempFilePath();
    passed = expectTrue(!missingPath.empty(), "createMissingTempFilePath") && passed;
    if (!missingPath.empty()) {
        const BinXray::Core::BinaryLoadResult missingLoadResult =
            BinXray::Core::BinaryDocument::loadFileBytes(missingPath);
        passed = expectTrue(!missingLoadResult.success, "BinaryDocument::loadFileBytes missing file fails") && passed;
        passed = expectTrue(!missingLoadResult.error.empty(), "BinaryDocument::loadFileBytes missing file has error") && passed;

        BinXray::Core::BinaryDocument unchangedDocument = {};
        unchangedDocument.replace(std::vector<std::uint8_t>{0x11, 0x22}, std::wstring(L"in-memory"));
        passed = expectTrue(!unchangedDocument.loadFromFile(missingPath), "BinaryDocument::loadFromFile missing file fails") && passed;
        passed = expectEqual(unchangedDocument.bytes(), std::vector<std::uint8_t>{0x11, 0x22}, "BinaryDocument::loadFromFile missing file preserves bytes") && passed;
        passed = expectEqual(unchangedDocument.sourcePath(), std::wstring(L"in-memory"), "BinaryDocument::loadFromFile missing file preserves path") && passed;
    }

    const std::wstring emptyPath = createTempFilePath();
    passed = expectTrue(!emptyPath.empty(), "createTempFilePath(empty)") && passed;
    if (!emptyPath.empty()) {
        passed = expectTrue(writeFile(emptyPath, {}), "writeFile(empty)") && passed;
        const BinXray::Core::BinaryLoadResult emptyLoadResult = BinXray::Core::BinaryDocument::loadFileBytes(emptyPath);
        passed = expectTrue(emptyLoadResult.success, "BinaryDocument::loadFileBytes empty file success") && passed;
        passed = expectEqual(emptyLoadResult.path, emptyPath, "BinaryDocument::loadFileBytes empty file path") && passed;
        passed = expectTrue(emptyLoadResult.bytes.empty(), "BinaryDocument::loadFileBytes empty file bytes empty") && passed;
        ::DeleteFileW(emptyPath.c_str());
    }

    const std::wstring tempPath = createTempFilePath();
    passed = expectTrue(!tempPath.empty(), "createTempFilePath") && passed;
    if (!passed) {
        return false;
    }

    const std::vector<std::uint8_t> expectedBytes = {0x42, 0x58, 0x52, 0x00, 0xFF, 0x10, 0x20};
    passed = expectTrue(writeFile(tempPath, expectedBytes), "writeFile(temp)") && passed;

    const BinXray::Core::BinaryLoadResult loadResult = BinXray::Core::BinaryDocument::loadFileBytes(tempPath);
    passed = expectTrue(loadResult.success, "BinaryDocument::loadFileBytes success") && passed;
    passed = expectEqual(loadResult.path, tempPath, "BinaryDocument::loadFileBytes path") && passed;
    passed = expectEqual(loadResult.bytes, expectedBytes, "BinaryDocument::loadFileBytes bytes") && passed;

    BinXray::Core::BinaryDocument document = {};
    passed = expectTrue(document.loadFromFile(tempPath), "BinaryDocument::loadFromFile success") && passed;
    passed = expectEqual(document.sourcePath(), tempPath, "BinaryDocument::sourcePath") && passed;
    passed = expectEqual(document.bytes(), expectedBytes, "BinaryDocument::bytes") && passed;

    ::DeleteFileW(tempPath.c_str());

    if (passed) {
        std::cout << "[PASS] BinaryDocumentTests" << std::endl;
    }

    return passed;
}
