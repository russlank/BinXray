// SPDX-License-Identifier: MIT
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;

#include "Core/BinaryDocument.h"
#include "Core/TransitionMatrix.h"
#include "Core/TransitionSeeker.h"
#include "UI/HexViewPanel.h"

#include <future>
#include <optional>
#include <string>
#include <vector>

namespace BinXray::UI {

/// Persistent seek state for the transition-plot crosshair / highlight feature.
struct SeekState {
    bool seekEnabled       = false;   ///< Master toggle for the seeking feature.
    bool coordEnabled      = true;    ///< Show crosshair + coordinate labels.
    bool frozen            = false;   ///< True while seek is click-locked.
    bool valid             = false;   ///< True when fromByte/toByte hold meaningful data.
    std::uint8_t fromByte  = 0;       ///< Row (previous byte value) under cursor.
    std::uint8_t toByte    = 0;       ///< Column (current byte value) under cursor.
    Core::SeekResult result;          ///< Resolved offsets + transition count.
};

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool initialize(HINSTANCE hInstance);
    int run();
    void shutdown();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    [[nodiscard]] std::optional<std::wstring> promptOpenFilePath() const;
    void startAsyncFileLoad(const std::wstring& path);
    void pollAsyncFileLoad();
    void refreshRangeAfterDocumentChange();
    void setWindowFromCenter(std::size_t centerOffset);
    void rebuildMatrixIfDirty();
    void drawWorkspace();
    void drawControlsColumn();
    void drawCenterColumn();
    void drawMatrixPlot();
    void drawSeekAddressList();
    void drawRibbonColumn();

    void updateSeekFromPlot(float originX, float originY, float plotSize);
    void invalidateSeek();

    bool createDeviceD3D(HWND hWnd);
    void cleanupDeviceD3D();
    void createRenderTarget();
    void cleanupRenderTarget();
    void renderFrame();

    HINSTANCE m_hInstance;
    HWND m_hWnd;
    bool m_running;
    bool m_initialized;

    IDXGISwapChain* m_swapChain;
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    ID3D11RenderTargetView* m_mainRenderTargetView;

    Core::BinaryDocument m_document;
    std::size_t m_selectedOffset;
    Core::TransitionMatrix m_transitionMatrix;
    Core::TransitionMatrix::Luminance m_matrixLuminance;

    bool m_scaleEnabled;
    bool m_normalizeEnabled;
    bool m_fullViewEnabled;
    int m_blockSize;
    std::size_t m_windowStartOffset;
    std::size_t m_windowEndOffset;
    bool m_matrixDirty;

    bool m_isLoadingFile;
    std::future<Core::BinaryLoadResult> m_asyncLoadFuture;
    std::wstring m_loadingPath;
    std::wstring m_lastLoadError;

    int m_ribbonWidth;
    HexViewPanel m_hexViewPanel;
    SeekState m_seek;
};

} // namespace BinXray::UI
