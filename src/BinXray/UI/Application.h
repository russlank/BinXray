// SPDX-License-Identifier: MIT
//
// Application.h
//
// Top-level application class that owns the Win32 window, D3D11 device,
// ImGui context, binary document, and all panel state.  `initialize()`
// creates the window and D3D resources; `run()` enters the message loop;
// `shutdown()` tears everything down.
//
// The workspace is rendered as three side-by-side columns:
//   Left   – controls (file, display options, seeking options, 3D rotation).
//   Centre – transition plot or 3D trigram scatter + hex view (+ seek list).
//   Right  – bitmap ribbon overview with cursor triangles.
//
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
#include "Core/TrigramPlot.h"
#include "UI/HexViewPanel.h"

#include <future>
#include <optional>
#include <string>
#include <vector>

namespace BinXray::UI {

/// Persistent seek state for the transition-plot crosshair / highlight feature.
///
/// The UI layer owns this struct because it tracks presentation concerns
/// (freeze toggle, coordinate display).  The actual offset scanning lives
/// in Core::findTransitionOffsets().
struct SeekState {
    bool seekEnabled       = false;   ///< Master toggle for the seeking feature.
    bool coordEnabled      = true;    ///< Show crosshair + coordinate labels.
    bool snapEnabled       = false;   ///< Snap crosshair to nearest non-empty cell.
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
    /// Render the 3D byte-trigram scatter plot with orthographic projection.
    void draw3DPlot();
    void drawSeekAddressList();
    void drawRibbonColumn();

    void updateSeekFromPlot(float originX, float originY, float plotSize);
    void invalidateSeek();

    // ---- D3D11 lifecycle ----------------------------------------------------
    bool createDeviceD3D(HWND hWnd);
    void cleanupDeviceD3D();
    void createRenderTarget();
    void cleanupRenderTarget();
    void renderFrame();

    // ---- Window & device state ----------------------------------------------
    HINSTANCE m_hInstance;
    HWND m_hWnd;
    bool m_running;
    bool m_initialized;

    IDXGISwapChain* m_swapChain;
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_deviceContext;
    ID3D11RenderTargetView* m_mainRenderTargetView;

    // ---- Document & analysis state ------------------------------------------
    Core::BinaryDocument m_document;
    std::size_t m_selectedOffset;
    Core::TransitionMatrix m_transitionMatrix;
    Core::TransitionMatrix::Luminance m_matrixLuminance;

    bool m_scaleEnabled;
    bool m_normalizeEnabled;
    bool m_heatMapEnabled;
    bool m_3dModeEnabled;          ///< When true, centre column shows trigram scatter instead of 2D matrix.
    bool m_fullViewEnabled;
    int m_blockSize;
    std::size_t m_windowStartOffset;
    std::size_t m_windowEndOffset;
    bool m_matrixDirty;

    // ---- Async file I/O ------------------------------------------------------
    bool m_isLoadingFile;
    std::future<Core::BinaryLoadResult> m_asyncLoadFuture;
    std::wstring m_loadingPath;
    std::wstring m_lastLoadError;

    // ---- UI panels & seeking -------------------------------------------------
    int m_ribbonWidth;
    HexViewPanel m_hexViewPanel;
    SeekState m_seek;
    std::optional<std::size_t> m_seekScrollTarget;  ///< One-shot scroll target for hex view.

    // ---- 3D trigram plot state -----------------------------------------------
    Core::TrigramPlot m_trigramPlot;   ///< 256^3 byte-trigram density accumulator.
    float m_3dYaw;                     ///< Current yaw (radians, wraps at 2pi).
    float m_3dPitch;                   ///< Current pitch (radians, clamped ±1.5).
    bool  m_3dAutoRotate;              ///< When true, yaw advances each frame.
    float m_3dAutoRotateSpeed;         ///< Degrees per frame.
    float m_3dElevationDeg;            ///< Adjustable elevation in degrees (-89..89).
    float m_3dPointOpacity;            ///< Alpha multiplier for scatter dots (0.05..1.0).
    int   m_3dBgMode;                  ///< 0 = Black, 1 = White, 2 = Custom.
    float m_3dBgCustomColor[3];        ///< RGB triplet for Custom background mode.
};

} // namespace BinXray::UI
