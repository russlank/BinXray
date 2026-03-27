// SPDX-License-Identifier: MIT
//
// BinaryDocument.h
//
// Owns the in-memory copy of a loaded binary file.  Provides a static
// helper (`loadFileBytes`) suitable for background loading via
// `std::async`, and a non-static `loadFromFile` convenience wrapper.
//
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace BinXray::Core {

struct BinaryLoadResult {
    bool success;
    std::vector<std::uint8_t> bytes;
    std::wstring path;
    std::wstring error;
};

class BinaryDocument {
public:
    [[nodiscard]] static BinaryLoadResult loadFileBytes(const std::wstring& path);

    [[nodiscard]] bool loadFromFile(const std::wstring& path);
    void replace(std::vector<std::uint8_t>&& bytes, std::wstring&& path);
    void loadSampleData();

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept;
    [[nodiscard]] const std::wstring& sourcePath() const noexcept;

private:
    std::vector<std::uint8_t> m_bytes;
    std::wstring m_sourcePath;
};

} // namespace BinXray::Core
