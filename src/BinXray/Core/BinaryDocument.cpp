// SPDX-License-Identifier: MIT
//
// BinaryDocument.cpp  --  binary file loading and in-memory storage.
//

#include "BinaryDocument.h"

#include <fstream>
#include <utility>
#include <vector>

namespace BinXray::Core {

BinaryLoadResult BinaryDocument::loadFileBytes(const std::wstring& path) {
    BinaryLoadResult result = {};
    result.success = false;
    result.path = path;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        result.error = L"Unable to open file for reading.";
        return result;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff fileSize = file.tellg();
    if (fileSize < 0) {
        result.error = L"Unable to determine file size.";
        return result;
    }
    file.seekg(0, std::ios::beg);

    if (fileSize > 0) {
        result.bytes.resize(static_cast<std::size_t>(fileSize));
        file.read(reinterpret_cast<char*>(result.bytes.data()), fileSize);
        if (static_cast<std::streamoff>(file.gcount()) != fileSize) {
            result.bytes.clear();
            result.error = L"I/O error while reading file.";
            return result;
        }
    }

    result.success = true;
    return result;
}

bool BinaryDocument::loadFromFile(const std::wstring& path) {
    BinaryLoadResult result = loadFileBytes(path);
    if (!result.success) {
        return false;
    }

    replace(std::move(result.bytes), std::move(result.path));
    return true;
}

void BinaryDocument::replace(std::vector<std::uint8_t>&& bytes, std::wstring&& path) {
    m_bytes = std::move(bytes);
    m_sourcePath = std::move(path);
}

void BinaryDocument::loadSampleData() {
    m_sourcePath.clear();
    m_bytes.resize(1024);
    for (std::size_t i = 0; i < m_bytes.size(); ++i) {
        m_bytes[i] = static_cast<std::uint8_t>(i % 256);
    }
}

const std::vector<std::uint8_t>& BinaryDocument::bytes() const noexcept {
    return m_bytes;
}

const std::wstring& BinaryDocument::sourcePath() const noexcept {
    return m_sourcePath;
}

} // namespace BinXray::Core
