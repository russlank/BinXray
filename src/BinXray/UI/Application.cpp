// SPDX-License-Identifier: MIT
//
// Application.cpp  --  main application lifecycle, workspace layout, and
//                       panel composition for the BinXray binary analyser.
//

#include "Application.h"

#include "UIConstants.h"
#include "Version.h"
#include "resource.h"

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <commdlg.h>
#include <d3d11.h>
#include <dxgi.h>
#include <utility>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace BinXray::UI {

namespace {
Application* g_applicationInstance = nullptr;
constexpr wchar_t kWindowClass[] = L"BinXrayWindowClass";

/// Maps a 0–255 intensity to a blue→cyan→green→yellow→red heat-map colour.
ImU32 intensityToHeatMap(std::uint8_t intensity) {
    const float t = static_cast<float>(intensity) / 255.0F;
    float r, g, b;
    if (t < 0.25F) {
        r = 0.0F;
        g = t * 4.0F;
        b = 1.0F;
    } else if (t < 0.5F) {
        r = 0.0F;
        g = 1.0F;
        b = 1.0F - (t - 0.25F) * 4.0F;
    } else if (t < 0.75F) {
        r = (t - 0.5F) * 4.0F;
        g = 1.0F;
        b = 0.0F;
    } else {
        r = 1.0F;
        g = 1.0F - (t - 0.75F) * 4.0F;
        b = 0.0F;
    }
    return IM_COL32(
        static_cast<int>(r * 255.0F),
        static_cast<int>(g * 255.0F),
        static_cast<int>(b * 255.0F),
        255);
}

/// Pre-built 256-entry LUT mapping intensity → heat-map ImU32.
/// Avoids repeated per-pixel branching in hot rendering loops.
const ImU32* getHeatMapLUT() {
    static ImU32 lut[256];
    static bool built = false;
    if (!built) {
        for (int i = 0; i < 256; ++i) {
            lut[i] = intensityToHeatMap(static_cast<std::uint8_t>(i));
        }
        built = true;
    }
    return lut;
}

}

static std::wstring utf8ToWide(const char* text) {
    if (!text || text[0] == '\0') {
        return {};
    }

    int size = ::MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (size <= 1) {
        return {};
    }

    std::wstring output(static_cast<size_t>(size - 1), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text, -1, output.data(), size);
    return output;
}

static std::wstring shortCommitWide(const char* commit) {
    std::wstring commitWide = utf8ToWide(commit);
    if (commitWide.size() > 12) {
        commitWide.resize(12);
    }
    return commitWide;
}

Application::Application()
    : m_hInstance(nullptr),
      m_hWnd(nullptr),
      m_running(false),
      m_initialized(false),
      m_swapChain(nullptr),
      m_device(nullptr),
      m_deviceContext(nullptr),
      m_mainRenderTargetView(nullptr),
      m_selectedOffset(0),
      m_transitionMatrix(),
      m_matrixLuminance(),
      m_scaleEnabled(false),
      m_normalizeEnabled(false),
      m_heatMapEnabled(false),
      m_3dModeEnabled(false),
      m_fullViewEnabled(true),
      m_blockSize(Constants::kBlockSizeDefault),
      m_windowStartOffset(0),
      m_windowEndOffset(0),
      m_matrixDirty(true),
      m_isLoadingFile(false),
      m_loadingPath(),
      m_lastLoadError(),
      m_ribbonWidth(Constants::kRibbonWidthDefault),
      m_hexViewPanel(),
      m_seek(),
      m_seekScrollTarget(),
      m_trigramPlot(),
      m_3dYaw(Constants::k3DPlotDefaultYaw),
      m_3dPitch(Constants::k3DPlotDefaultPitch),
      m_3dAutoRotate(false),
      m_3dAutoRotateSpeed(Constants::k3DAutoRotateSpeedDefault),
      m_3dElevationDeg(30.0F),
      m_3dPointOpacity(Constants::k3DOpacityDefault),
      m_3dBgMode(Constants::k3DBgModeDefault),
      m_3dBgCustomColor{Constants::k3DBgCustomDefault[0],
                        Constants::k3DBgCustomDefault[1],
                        Constants::k3DBgCustomDefault[2]} {
    m_matrixLuminance.fill(0);
}

Application::~Application() {
    shutdown();
}

bool Application::initialize(HINSTANCE hInstance) {
    if (m_initialized || g_applicationInstance != nullptr) {
        return false;
    }

    g_applicationInstance = this;
    m_hInstance = hInstance;

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = Application::WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = kWindowClass;
    wc.hIcon   = static_cast<HICON>(::LoadImageW(
        hInstance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    wc.hIconSm = static_cast<HICON>(::LoadImageW(
        hInstance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    ::RegisterClassExW(&wc);

    std::wstring windowTitle = L"Bin X-ray v";
    windowTitle += BXR_VERSION_WSTRING;

    std::wstring branchWide = utf8ToWide(BXR_BUILD_BRANCH);
    std::wstring commitWide = shortCommitWide(BXR_BUILD_COMMIT);
    if (!branchWide.empty() && branchWide != L"unknown") {
        windowTitle += L" [";
        windowTitle += branchWide;
        if (!commitWide.empty() && commitWide != L"unknown") {
            windowTitle += L" @ ";
            windowTitle += commitWide;
        }
        windowTitle += L"]";
    }

    windowTitle += L" - Binary Analyzer";

    m_hWnd = ::CreateWindowW(
        wc.lpszClassName,
        windowTitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        Constants::kWindowInitialX,
        Constants::kWindowInitialY,
        Constants::kWindowInitialWidth,
        Constants::kWindowInitialHeight,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr);

    if (m_hWnd == nullptr) {
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        g_applicationInstance = nullptr;
        return false;
    }

    if (!createDeviceD3D(m_hWnd)) {
        ::DestroyWindow(m_hWnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        g_applicationInstance = nullptr;
        return false;
    }

    ::ShowWindow(m_hWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(m_hWnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(m_hWnd) || !ImGui_ImplDX11_Init(m_device, m_deviceContext)) {
        shutdown();
        return false;
    }

    m_document.loadSampleData();
    m_selectedOffset = 0;
    refreshRangeAfterDocumentChange();
    m_running = true;
    m_initialized = true;
    return true;
}

int Application::run() {
    MSG msg = {};
    while (m_running && msg.message != WM_QUIT) {
        // If nothing requires continuous rendering, sleep until the OS
        // delivers an input/paint/timer message.  This drops CPU and GPU
        // utilization to near-zero when the application is idle.
        if (!needsContinuousRender()) {
            ::MsgWaitForMultipleObjects(0, nullptr, FALSE, INFINITE, QS_ALLINPUT);
        }

        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                m_running = false;
                break;
            }
        }

        if (!m_running) {
            break;
        }

        renderFrame();
    }

    return static_cast<int>(msg.wParam);
}

void Application::shutdown() {
    if (!m_initialized && m_hWnd == nullptr && m_device == nullptr) {
        return;
    }

    m_running = false;
    m_isLoadingFile = false;

    if (m_asyncLoadFuture.valid()) {
        m_asyncLoadFuture.wait();
        m_asyncLoadFuture = {};
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    cleanupDeviceD3D();

    if (m_hWnd != nullptr) {
        ::DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }

    if (m_hInstance != nullptr) {
        ::UnregisterClassW(kWindowClass, m_hInstance);
        m_hInstance = nullptr;
    }

    g_applicationInstance = nullptr;
    m_initialized = false;
}

LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (g_applicationInstance != nullptr) {
        return g_applicationInstance->handleWindowMessage(hWnd, message, wParam, lParam);
    }

    return ::DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT Application::handleWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
        return true;
    }

    switch (message) {
    case WM_SIZE:
        if (m_device != nullptr && wParam != SIZE_MINIMIZED) {
            cleanupRenderTarget();
            m_swapChain->ResizeBuffers(0, static_cast<UINT>(LOWORD(lParam)), static_cast<UINT>(HIWORD(lParam)), DXGI_FORMAT_UNKNOWN, 0);
            createRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) {
            return 0;
        }
        break;
    case WM_DESTROY:
        m_running = false;
        ::PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return ::DefWindowProcW(hWnd, message, wParam, lParam);
}

std::optional<std::wstring> Application::promptOpenFilePath() const {
    wchar_t filePathBuffer[MAX_PATH] = {};

    OPENFILENAMEW openFileName = {};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.hwndOwner = m_hWnd;
    openFileName.lpstrFile = filePathBuffer;
    openFileName.nMaxFile = MAX_PATH;
    openFileName.lpstrFilter = L"All Files\0*.*\0\0";
    openFileName.nFilterIndex = 1;
    openFileName.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (::GetOpenFileNameW(&openFileName) == TRUE) {
        return std::wstring(filePathBuffer);
    }

    return std::nullopt;
}

void Application::startAsyncFileLoad(const std::wstring& path) {
    if (m_isLoadingFile) {
        return;
    }

    m_isLoadingFile = true;
    m_loadingPath = path;
    m_lastLoadError.clear();

    m_asyncLoadFuture = std::async(std::launch::async, [path]() {
        return Core::BinaryDocument::loadFileBytes(path);
    });
}

void Application::pollAsyncFileLoad() {
    if (!m_isLoadingFile || !m_asyncLoadFuture.valid()) {
        return;
    }

    const auto pollStatus = m_asyncLoadFuture.wait_for(std::chrono::milliseconds(0));
    if (pollStatus != std::future_status::ready) {
        return;
    }

    Core::BinaryLoadResult loadResult = m_asyncLoadFuture.get();
    m_asyncLoadFuture = {};
    m_isLoadingFile = false;
    m_loadingPath.clear();

    if (!loadResult.success) {
        m_lastLoadError = loadResult.error.empty() ? L"Unknown error while loading file." : std::move(loadResult.error);
        return;
    }

    m_document.replace(std::move(loadResult.bytes), std::move(loadResult.path));
    m_selectedOffset = 0;
    refreshRangeAfterDocumentChange();
}

void Application::refreshRangeAfterDocumentChange() {
    const std::size_t fileSize = m_document.bytes().size();
    if (fileSize == 0) {
        m_windowStartOffset = 0;
        m_windowEndOffset = 0;
        m_matrixDirty = true;
        invalidateSeek();
        return;
    }

    m_selectedOffset = std::min(m_selectedOffset, fileSize - 1);
    if (m_fullViewEnabled) {
        m_windowStartOffset = 0;
        m_windowEndOffset = fileSize;
    } else {
        setWindowFromCenter(m_selectedOffset);
    }

    m_matrixDirty = true;
    invalidateSeek();
}

void Application::setWindowFromCenter(std::size_t centerOffset) {
    const std::size_t fileSize = m_document.bytes().size();
    if (fileSize == 0) {
        m_windowStartOffset = 0;
        m_windowEndOffset = 0;
        m_matrixDirty = true;
        return;
    }

    const std::size_t clampedCenter = std::min(centerOffset, fileSize - 1);
    const std::size_t blockSize = std::max<std::size_t>(2, static_cast<std::size_t>(m_blockSize));

    if (m_fullViewEnabled || blockSize >= fileSize) {
        m_windowStartOffset = 0;
        m_windowEndOffset = fileSize;
        m_matrixDirty = true;
        return;
    }

    const std::size_t half = blockSize / 2;
    std::size_t start = (clampedCenter > half) ? (clampedCenter - half) : 0;
    std::size_t end = std::min(fileSize, start + blockSize);
    if (end == fileSize && end > blockSize) {
        start = end - blockSize;
    }

    m_windowStartOffset = start;
    m_windowEndOffset = end;
    m_matrixDirty = true;
}

void Application::rebuildMatrixIfDirty() {
    if (!m_matrixDirty) {
        return;
    }

    const auto& bytes = m_document.bytes();
    if (bytes.empty()) {
        m_transitionMatrix.compute(bytes, 0, 0);
        m_matrixLuminance.fill(0);
        m_matrixDirty = false;
        return;
    }

    const std::size_t start = m_fullViewEnabled ? 0 : m_windowStartOffset;
    const std::size_t end = m_fullViewEnabled ? bytes.size() : m_windowEndOffset;
    m_transitionMatrix.compute(bytes, start, end);

    const Core::TransitionMatrix::RenderMode mode = !m_scaleEnabled
        ? Core::TransitionMatrix::RenderMode::Binary
        : (m_normalizeEnabled ? Core::TransitionMatrix::RenderMode::Normalized : Core::TransitionMatrix::RenderMode::Linear);
    m_transitionMatrix.renderLuminance(mode, m_matrixLuminance);

    // Also recompute the trigram plot (shares the same data range).
    m_trigramPlot.compute(bytes, start, end);

    m_matrixDirty = false;
}

void Application::invalidateSeek() {
    if (!m_seek.frozen) {
        m_seek.valid = false;
        m_seek.result = {};
    }
}

bool Application::needsContinuousRender() const noexcept {
    // Auto-rotation advances yaw every frame.
    if (m_3dModeEnabled && m_3dAutoRotate) return true;
    // Async file load needs polling each frame.
    if (m_isLoadingFile) return true;
    // Pending data recomputation.
    if (m_matrixDirty) return true;
    return false;
}

void Application::updateSeekFromPlot(float originX, float originY, float plotSize) {
    if (!m_seek.seekEnabled || m_seek.frozen) {
        return;
    }

    const ImVec2 mousePos = ImGui::GetMousePos();
    const float localX = mousePos.x - originX;
    const float localY = mousePos.y - originY;

    if (localX < 0.0F || localX >= plotSize || localY < 0.0F || localY >= plotSize) {
        m_seek.valid = false;
        m_seek.result = {};
        return;
    }

    constexpr float dim = static_cast<float>(Core::TransitionMatrix::kDimension);
    auto col = static_cast<int>(std::clamp(localX * dim / plotSize, 0.0F, 255.0F));
    auto row = static_cast<int>(std::clamp(localY * dim / plotSize, 0.0F, 255.0F));

    // Snap to nearest non-empty cell when enabled.
    if (m_seek.snapEnabled &&
        m_transitionMatrix.count(static_cast<std::uint8_t>(row),
                                 static_cast<std::uint8_t>(col)) == 0) {
        float bestDistSq = 256.0F * 256.0F * 2.0F;
        int bestRow = -1;
        int bestCol = -1;
        const int maxR = Constants::kSeekSnapMaxRadius;
        for (int r = 1; r <= maxR; ++r) {
            bool found = false;
            for (int dr = -r; dr <= r; ++dr) {
                for (int dc = -r; dc <= r; ++dc) {
                    if (std::abs(dr) != r && std::abs(dc) != r) {
                        continue;
                    }
                    const int cr = row + dr;
                    const int cc = col + dc;
                    if (cr < 0 || cr >= 256 || cc < 0 || cc >= 256) {
                        continue;
                    }
                    if (m_transitionMatrix.count(static_cast<std::uint8_t>(cr),
                                                 static_cast<std::uint8_t>(cc)) > 0) {
                        const float dSq = static_cast<float>(dr * dr + dc * dc);
                        if (dSq < bestDistSq) {
                            bestDistSq = dSq;
                            bestRow = cr;
                            bestCol = cc;
                            found = true;
                        }
                    }
                }
            }
            if (found) {
                break;
            }
        }
        if (bestRow >= 0) {
            row = bestRow;
            col = bestCol;
        } else {
            m_seek.valid = false;
            m_seek.result = {};
            return;
        }
    }

    const auto fromByte = static_cast<std::uint8_t>(row);
    const auto toByte = static_cast<std::uint8_t>(col);

    // Avoid redundant re-scan when cursor stays on the same cell.
    if (m_seek.valid && m_seek.fromByte == fromByte && m_seek.toByte == toByte) {
        return;
    }

    m_seek.fromByte = fromByte;
    m_seek.toByte = toByte;
    m_seek.valid = true;
    m_seek.result = Core::findTransitionOffsets(
        m_document.bytes(),
        m_transitionMatrix.startOffset(),
        m_transitionMatrix.endOffset(),
        fromByte, toByte,
        Constants::kSeekMaxAddresses);
}

bool Application::createDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    const D3D_FEATURE_LEVEL featureLevelArray[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &m_swapChain,
        &m_device,
        &featureLevel,
        &m_deviceContext);

    if (result != S_OK) {
        return false;
    }

    createRenderTarget();
    return true;
}

void Application::cleanupDeviceD3D() {
    cleanupRenderTarget();

    if (m_swapChain != nullptr) {
        m_swapChain->Release();
        m_swapChain = nullptr;
    }

    if (m_deviceContext != nullptr) {
        m_deviceContext->Release();
        m_deviceContext = nullptr;
    }

    if (m_device != nullptr) {
        m_device->Release();
        m_device = nullptr;
    }
}

void Application::createRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    m_device->CreateRenderTargetView(backBuffer, nullptr, &m_mainRenderTargetView);
    backBuffer->Release();
}

void Application::cleanupRenderTarget() {
    if (m_mainRenderTargetView != nullptr) {
        m_mainRenderTargetView->Release();
        m_mainRenderTargetView = nullptr;
    }
}

void Application::drawControlsColumn() {
    bool controlsChanged = false;

    if (ImGui::Button("Open File", ImVec2(-1.0F, 0.0F)) && !m_isLoadingFile) {
        const std::optional<std::wstring> selectedPath = promptOpenFilePath();
        if (selectedPath.has_value()) {
            startAsyncFileLoad(selectedPath.value());
        }
    }

    if (m_isLoadingFile) {
        ImGui::Text("Loading: %ls", m_loadingPath.c_str());
    }

    if (!m_lastLoadError.empty()) {
        ImGui::TextColored(Constants::kErrorTextColor, "Load Error:");
        ImGui::TextWrapped("%ls", m_lastLoadError.c_str());
    }

    ImGui::Separator();
    controlsChanged = ImGui::Checkbox("Scale", &m_scaleEnabled) || controlsChanged;

    ImGui::BeginDisabled(!m_scaleEnabled);
    controlsChanged = ImGui::Checkbox("Normalize", &m_normalizeEnabled) || controlsChanged;
    ImGui::EndDisabled();

    ImGui::Checkbox("Heat Map", &m_heatMapEnabled);

    if (ImGui::Checkbox("3D Mode", &m_3dModeEnabled)) {
        m_matrixDirty = true;
    }

    controlsChanged = ImGui::Checkbox("Full View", &m_fullViewEnabled) || controlsChanged;
    if (ImGui::InputInt("Block Size", &m_blockSize, Constants::kBlockSizeStep, Constants::kBlockSizeFastStep)) {
        m_blockSize = std::max(Constants::kBlockSizeMin, m_blockSize);
        controlsChanged = true;
    }

    ImGui::Separator();
    if (ImGui::InputInt("Ribbon Width", &m_ribbonWidth, Constants::kRibbonWidthStep, Constants::kRibbonWidthFastStep)) {
        m_ribbonWidth = std::clamp(m_ribbonWidth, Constants::kRibbonWidthMin, Constants::kRibbonWidthMax);
    }

    // 3D auto-rotation controls (only visible in 3D mode).
    if (m_3dModeEnabled) {
        ImGui::Separator();
        ImGui::TextColored(Constants::kControlsLabelColor, "3D Rotation");
        ImGui::Checkbox("Auto-Rotate", &m_3dAutoRotate);
        ImGui::SliderFloat("Speed (deg)", &m_3dAutoRotateSpeed,
            Constants::k3DAutoRotateSpeedMin, Constants::k3DAutoRotateSpeedMax, "%.2f");
        ImGui::SliderFloat("Elevation", &m_3dElevationDeg,
            Constants::k3DElevationMin, Constants::k3DElevationMax, "%.1f");

        ImGui::Separator();
        ImGui::TextColored(Constants::kControlsLabelColor, "3D Appearance");
        ImGui::SliderFloat("Opacity", &m_3dPointOpacity,
            Constants::k3DOpacityMin, Constants::k3DOpacityMax, "%.2f");

        ImGui::TextColored(Constants::kControlsLabelColor, "Background");
        ImGui::RadioButton("Black",  &m_3dBgMode, Constants::k3DBgModeBlack);
        ImGui::SameLine();
        ImGui::RadioButton("White",  &m_3dBgMode, Constants::k3DBgModeWhite);
        ImGui::SameLine();
        ImGui::RadioButton("Custom", &m_3dBgMode, Constants::k3DBgModeCustom);
        if (m_3dBgMode == Constants::k3DBgModeCustom) {
            ImGui::ColorEdit3("##BgCustom", m_3dBgCustomColor,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        }
    }

    ImGui::Separator();
    ImGui::TextColored(Constants::kControlsLabelColor, "Seeking");

    // Seeking is not available in 3D mode (2D plot only).
    ImGui::BeginDisabled(m_3dModeEnabled);
    if (ImGui::Checkbox("Enable Seeking", &m_seek.seekEnabled)) {
        if (!m_seek.seekEnabled) {
            m_seek.frozen = false;
            m_seek.valid = false;
            m_seek.result = {};
        }
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!m_seek.seekEnabled || m_3dModeEnabled);
    ImGui::Checkbox("Show Coordinates", &m_seek.coordEnabled);
    ImGui::Checkbox("Snap to Data", &m_seek.snapEnabled);

    if (m_seek.frozen) {
        if (ImGui::Button("Unfreeze", ImVec2(-1.0F, 0.0F))) {
            m_seek.frozen = false;
        }
    } else {
        ImGui::BeginDisabled(!m_seek.valid);
        if (ImGui::Button("Freeze", ImVec2(-1.0F, 0.0F))) {
            m_seek.frozen = true;
        }
        ImGui::EndDisabled();
    }
    ImGui::EndDisabled();

    if (m_seek.valid) {
        ImGui::TextColored(Constants::kControlsLabelColor, "Pair:");
        ImGui::SameLine();
        ImGui::Text("0x%02X -> 0x%02X", m_seek.fromByte, m_seek.toByte);
        ImGui::TextColored(Constants::kControlsLabelColor, "Count:");
        ImGui::SameLine();
        ImGui::Text("%u", m_seek.result.transitionCount);
        if (m_seek.result.offsets.size() < m_seek.result.transitionCount) {
            ImGui::Text("(showing first %zu)", m_seek.result.offsets.size());
        }
    }

    if (controlsChanged) {
        refreshRangeAfterDocumentChange();
    }

    const auto& bytes = m_document.bytes();
    ImGui::Separator();
    ImGui::TextColored(Constants::kControlsLabelColor, "Bytes:");
    ImGui::SameLine();
    ImGui::Text("%zu", bytes.size());
    if (!m_document.sourcePath().empty()) {
        ImGui::TextColored(Constants::kControlsLabelColor, "File:");
        ImGui::SameLine();
        ImGui::TextWrapped("%ls", m_document.sourcePath().c_str());
    } else {
        ImGui::TextColored(Constants::kControlsLabelColor, "File:");
        ImGui::SameLine();
        ImGui::TextUnformatted("sample dataset");
    }

    ImGui::TextColored(Constants::kControlsLabelColor, "Range:");
    ImGui::SameLine();
    ImGui::Text("0x%zX - 0x%zX", m_transitionMatrix.startOffset(), m_transitionMatrix.endOffset());
    ImGui::TextColored(Constants::kControlsLabelColor, "Max Density:");
    ImGui::SameLine();
    ImGui::Text("%u", m_transitionMatrix.maxCount());

    if (!bytes.empty()) {
        m_selectedOffset = std::min(m_selectedOffset, bytes.size() - 1);
        const std::uint8_t value = bytes[m_selectedOffset];
        ImGui::Separator();
        ImGui::TextColored(Constants::kControlsLabelColor, "Selected Offset:");
        ImGui::SameLine();
        ImGui::Text("0x%zX", m_selectedOffset);
        ImGui::TextColored(Constants::kControlsLabelColor, "Selected Byte:");
        ImGui::SameLine();
        ImGui::Text("%u", static_cast<unsigned int>(value));
    }
}

void Application::drawMatrixPlot() {
    const auto& bytes = m_document.bytes();
    if (bytes.empty()) {
        ImGui::TextUnformatted("No data loaded.");
        return;
    }

    ImGui::TextUnformatted("Transition Plot (256 x 256)");
    ImGui::Separator();

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float marginLeft = Constants::kMatrixPlotMarginLeft;
    const float marginTop  = Constants::kMatrixPlotMarginTop;
    const float plotSize = std::max(Constants::kMatrixPlotMinSize,
        std::min(available.x - marginLeft, available.y - marginTop));
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("TransitionPlotCanvas", ImVec2(plotSize + marginLeft, plotSize + marginTop));

    // Plot area starts after the margins (space reserved for coordinate labels).
    const ImVec2 origin(canvasOrigin.x + marginLeft, canvasOrigin.y + marginTop);

    const bool isHovered = ImGui::IsItemHovered();

    // Handle click-to-freeze / click-to-unfreeze toggle.
    if (m_seek.seekEnabled && isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_seek.frozen = !m_seek.frozen;
    }

    // Live-update seek when hovering and not frozen.
    if (isHovered) {
        updateSeekFromPlot(origin.x, origin.y, plotSize);
    } else if (!m_seek.frozen) {
        m_seek.valid = false;
        m_seek.result = {};
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float cellSize = plotSize / static_cast<float>(Core::TransitionMatrix::kDimension);
    const ImU32* heatLUT = m_heatMapEnabled ? getHeatMapLUT() : nullptr;
    for (std::size_t i = 0; i < Core::TransitionMatrix::kCellCount; ++i) {
        const std::uint8_t intensity = m_matrixLuminance[i];
        if (intensity == 0) {
            continue;
        }
        const std::size_t row    = i >> 8;  // i / 256  (kDimension is always 256)
        const std::size_t column = i & 0xFF; // i % 256
        const ImU32  color = heatLUT
            ? heatLUT[intensity]
            : IM_COL32(intensity, intensity, intensity, 255);
        const float  x0    = origin.x + static_cast<float>(column) * cellSize;
        const float  y0    = origin.y + static_cast<float>(row)    * cellSize;
        drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + cellSize, y0 + cellSize), color);
    }

    drawList->AddRect(origin, ImVec2(origin.x + plotSize, origin.y + plotSize), Constants::kMatrixBorderColor, 0.0F, 0, 1.0F);

    // Draw seeking crosshair and coordinate labels.
    if (m_seek.seekEnabled && m_seek.valid && (m_seek.coordEnabled || m_seek.frozen)) {
        const float crossX = origin.x + (static_cast<float>(m_seek.toByte) + 0.5F) * cellSize;
        const float crossY = origin.y + (static_cast<float>(m_seek.fromByte) + 0.5F) * cellSize;

        if (m_seek.coordEnabled) {
            // Vertical line (column = toByte).
            drawList->AddLine(ImVec2(crossX, origin.y), ImVec2(crossX, origin.y + plotSize),
                              Constants::kSeekCrosshairColor, Constants::kSeekCrosshairThickness);
            // Horizontal line (row = fromByte).
            drawList->AddLine(ImVec2(origin.x, crossY), ImVec2(origin.x + plotSize, crossY),
                              Constants::kSeekCrosshairColor, Constants::kSeekCrosshairThickness);

            // Coordinate labels at the edges.
            char rowLabel[8];
            char colLabel[8];
            std::snprintf(rowLabel, sizeof(rowLabel), "%02X", m_seek.fromByte);
            std::snprintf(colLabel, sizeof(colLabel), "%02X", m_seek.toByte);

            const ImVec2 rowLabelSize = ImGui::CalcTextSize(rowLabel);
            const ImVec2 colLabelSize = ImGui::CalcTextSize(colLabel);
            const float pad = 2.0F;

            // Row label: left of the plot, vertically centered on crosshair.
            const ImVec2 rowLabelPos(origin.x - rowLabelSize.x - pad, crossY - rowLabelSize.y * 0.5F);
            drawList->AddRectFilled(ImVec2(rowLabelPos.x - pad, rowLabelPos.y - pad),
                                    ImVec2(rowLabelPos.x + rowLabelSize.x + pad, rowLabelPos.y + rowLabelSize.y + pad),
                                    Constants::kSeekCoordBgColor);
            drawList->AddText(rowLabelPos, Constants::kSeekCoordTextColor, rowLabel);

            // Column label: above the plot, horizontally centered on crosshair.
            const ImVec2 colLabelPos(crossX - colLabelSize.x * 0.5F, origin.y - colLabelSize.y - pad);
            drawList->AddRectFilled(ImVec2(colLabelPos.x - pad, colLabelPos.y - pad),
                                    ImVec2(colLabelPos.x + colLabelSize.x + pad, colLabelPos.y + colLabelSize.y + pad),
                                    Constants::kSeekCoordBgColor);
            drawList->AddText(colLabelPos, Constants::kSeekCoordTextColor, colLabel);
        }

        // Highlight the specific cell.
        const float cellX0 = origin.x + static_cast<float>(m_seek.toByte) * cellSize;
        const float cellY0 = origin.y + static_cast<float>(m_seek.fromByte) * cellSize;
        drawList->AddRect(ImVec2(cellX0, cellY0), ImVec2(cellX0 + cellSize, cellY0 + cellSize),
                          Constants::kSeekHighlightBorder, 0.0F, 0, 2.0F);
    }
}

void Application::draw3DPlot() {
    const auto& bytes = m_document.bytes();
    if (bytes.empty()) {
        ImGui::TextUnformatted("No data loaded.");
        return;
    }

    ImGui::TextUnformatted("Trigram Plot (256 x 256 x 256)");
    ImGui::Separator();

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float plotSize = std::max(Constants::kMatrixPlotMinSize,
        std::min(available.x, available.y));
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("TrigramPlotCanvas", ImVec2(plotSize, plotSize));

    const bool isHovered = ImGui::IsItemHovered();

    // Mouse-drag rotation (only when not auto-rotating).
    if (!m_3dAutoRotate && isHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_3dYaw   += delta.x * Constants::k3DPlotRotationSpeed;
        m_3dPitch += delta.y * Constants::k3DPlotRotationSpeed;
        constexpr float kPitchMax = 1.50F;
        m_3dPitch = std::clamp(m_3dPitch, -kPitchMax, kPitchMax);
    }

    // Reset on double-click.
    if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        m_3dYaw   = Constants::k3DPlotDefaultYaw;
        m_3dPitch = Constants::k3DPlotDefaultPitch;
        m_3dElevationDeg = 30.0F;
    }

    // Auto-rotation: advance yaw each frame; use elevation slider for pitch.
    if (m_3dAutoRotate) {
        constexpr float kDegToRad = 3.14159265F / 180.0F;
        constexpr float kTwoPi   = 2.0F * 3.14159265F;
        m_3dYaw += m_3dAutoRotateSpeed * kDegToRad;
        m_3dYaw  = std::fmod(m_3dYaw, kTwoPi);
        m_3dPitch = m_3dElevationDeg * kDegToRad;
    }

    // Precompute rotation sin/cos.
    const float cosYaw   = std::cos(m_3dYaw);
    const float sinYaw   = std::sin(m_3dYaw);
    const float cosPitch = std::cos(m_3dPitch);
    const float sinPitch = std::sin(m_3dPitch);

    const float halfPlot = plotSize * 0.5F;
    const float cx = canvasOrigin.x + halfPlot;
    const float cy = canvasOrigin.y + halfPlot;
    // Fold the 1/127.5 normalisation into the projection scale so the hot
    // loop only needs a subtract + multiply per coordinate.
    const float rawScale = plotSize * 0.35F;
    const float invNorm  = rawScale / 127.5F;

    // Lambda: project a [0,255] (x,y,z) coordinate to screen (px, py).
    // Uses precomputed invNorm so the inner loop avoids 3 subtractions
    // and 3 divisions per point.
    auto project = [&](float x, float y, float z, float& px, float& py) {
        // Centre to [-scale, +scale]: (val - 127.5) * invNorm.
        const float sx = (x - 127.5F) * invNorm;
        const float sy = (y - 127.5F) * invNorm;
        const float sz = (z - 127.5F) * invNorm;
        // Apply yaw (around Y axis).
        const float rx =  cosYaw * sx + sinYaw * sz;
        const float rz = -sinYaw * sx + cosYaw * sz;
        // Apply pitch (around X axis).
        const float ry = cosPitch * sy - sinPitch * rz;
        // Orthographic projection (drop Z after rotation).
        px = cx + rx;
        py = cy + ry;
    };

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Resolve background colour from the selected mode.
    ImU32 bgColor;
    if (m_3dBgMode == Constants::k3DBgModeWhite) {
        bgColor = Constants::k3DBgColorWhite;
    } else if (m_3dBgMode == Constants::k3DBgModeCustom) {
        bgColor = IM_COL32(
            static_cast<int>(m_3dBgCustomColor[0] * 255.0F),
            static_cast<int>(m_3dBgCustomColor[1] * 255.0F),
            static_cast<int>(m_3dBgCustomColor[2] * 255.0F), 255);
    } else {
        bgColor = Constants::k3DBgColorBlack;
    }

    // Background fill.
    drawList->AddRectFilled(canvasOrigin,
        ImVec2(canvasOrigin.x + plotSize, canvasOrigin.y + plotSize),
        bgColor);

    // Clip all subsequent drawing (wireframe, labels, points) to the canvas.
    const ImVec2 canvasMax = ImVec2(canvasOrigin.x + plotSize,
                                     canvasOrigin.y + plotSize);
    drawList->PushClipRect(canvasOrigin, canvasMax, true);

    // Draw wireframe cube edges (12 edges of a unit cube).
    {
        constexpr float lo = 0.0F;
        constexpr float hi = 255.0F;
        const float corners[8][3] = {
            {lo,lo,lo},{hi,lo,lo},{hi,hi,lo},{lo,hi,lo},
            {lo,lo,hi},{hi,lo,hi},{hi,hi,hi},{lo,hi,hi}
        };
        constexpr int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };
        for (auto& edge : edges) {
            float ax, ay, bx, by;
            project(corners[edge[0]][0], corners[edge[0]][1], corners[edge[0]][2], ax, ay);
            project(corners[edge[1]][0], corners[edge[1]][1], corners[edge[1]][2], bx, by);
            drawList->AddLine(ImVec2(ax, ay), ImVec2(bx, by), Constants::k3DPlotAxisColor, 1.0F);
        }

        // Axis labels at the positive ends.
        float lx, ly;
        project(hi + 12.0F, 0.0F, 0.0F, lx, ly);
        drawList->AddText(ImVec2(lx, ly), Constants::kSeekCoordTextColor, "X");
        project(0.0F, hi + 12.0F, 0.0F, lx, ly);
        drawList->AddText(ImVec2(lx, ly), Constants::kSeekCoordTextColor, "Y");
        project(0.0F, 0.0F, hi + 12.0F, lx, ly);
        drawList->AddText(ImVec2(lx, ly), Constants::kSeekCoordTextColor, "Z");
    }

    // Draw trigram points.
    const auto& points = m_trigramPlot.points();
    const std::uint32_t maxCount = m_trigramPlot.maxCount();
    const float ptRad = Constants::k3DPlotPointSize;
    const ImU32 alphaShift = static_cast<ImU32>(m_3dPointOpacity * 255.0F) << 24;
    const ImU32* heatLUT = m_heatMapEnabled ? getHeatMapLUT() : nullptr;

    for (const auto& pt : points) {
        const std::uint8_t intensity = Core::TrigramPlot::mapIntensity(
            pt.count, maxCount, m_scaleEnabled, m_normalizeEnabled);
        if (intensity == 0) continue;

        const ImU32 color = heatLUT
            ? ((heatLUT[intensity] & 0x00FFFFFF) | alphaShift)
            : IM_COL32(intensity, intensity, intensity, 0) | alphaShift;

        float px, py;
        project(static_cast<float>(pt.x),
                static_cast<float>(pt.y),
                static_cast<float>(pt.z), px, py);

        drawList->AddRectFilled(ImVec2(px - ptRad, py - ptRad),
                                ImVec2(px + ptRad, py + ptRad), color);
    }

    drawList->PopClipRect();

    // Border.
    drawList->AddRect(canvasOrigin,
        ImVec2(canvasOrigin.x + plotSize, canvasOrigin.y + plotSize),
        Constants::kMatrixBorderColor, 0.0F, 0, 1.0F);
}

void Application::drawSeekAddressList() {
    if (!m_seek.seekEnabled || !m_seek.valid || m_seek.result.offsets.empty()) {
        ImGui::TextDisabled("No addresses");
        return;
    }

    ImGui::Text("0x%02X -> 0x%02X (%u)",
                m_seek.fromByte, m_seek.toByte, m_seek.result.transitionCount);
    if (m_seek.result.offsets.size() < m_seek.result.transitionCount) {
        ImGui::TextDisabled("first %zu shown", m_seek.result.offsets.size());
    }
    ImGui::Separator();

    ImGui::BeginChild("SeekAddrScroll");
    for (const std::size_t offset : m_seek.result.offsets) {
        char addrBuf[20];
        std::snprintf(addrBuf, sizeof(addrBuf), "0x%08zX", offset);
        if (ImGui::Selectable(addrBuf, offset == m_selectedOffset)) {
            m_selectedOffset = offset;
            m_seekScrollTarget = offset;
        }
    }
    ImGui::EndChild();
}

void Application::drawCenterColumn() {
    rebuildMatrixIfDirty();

    const float totalHeight = ImGui::GetContentRegionAvail().y;
    const float plotHeight  = std::max(Constants::kMatrixPlotMinHeight, totalHeight * Constants::kMatrixPlotHeightRatio);

    ImGui::BeginChild("MatrixView", ImVec2(0.0F, plotHeight), true);
    if (m_3dModeEnabled) {
        draw3DPlot();
    } else {
        drawMatrixPlot();
    }
    ImGui::EndChild();

    ImGui::Spacing();

    const std::vector<std::size_t>* seekOffsets =
        (m_seek.seekEnabled && m_seek.valid && !m_seek.result.offsets.empty())
            ? &m_seek.result.offsets : nullptr;

    const bool showAddresses = seekOffsets != nullptr;

    if (showAddresses) {
        const float spacing   = ImGui::GetStyle().ItemSpacing.x;
        const float fullWidth = ImGui::GetContentRegionAvail().x;
        const float addrWidth = Constants::kSeekAddressPanelWidth;
        const float hexWidth  = fullWidth - addrWidth - spacing;

        ImGui::BeginChild("HexView", ImVec2(hexWidth, 0.0F), true);
        ImGui::TextUnformatted("Hex View");
        ImGui::Separator();
        m_hexViewPanel.drawEmbedded(
            m_document, m_selectedOffset,
            m_transitionMatrix.startOffset(),
            m_transitionMatrix.endOffset(),
            seekOffsets, m_seekScrollTarget);
        m_seekScrollTarget.reset();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("SeekAddresses", ImVec2(0.0F, 0.0F), true);
        ImGui::TextUnformatted("Addresses");
        ImGui::Separator();
        drawSeekAddressList();
        ImGui::EndChild();
    } else {
        ImGui::BeginChild("HexView", ImVec2(0.0F, 0.0F), true);
        ImGui::TextUnformatted("Hex View");
        ImGui::Separator();
        m_hexViewPanel.drawEmbedded(
            m_document, m_selectedOffset,
            m_transitionMatrix.startOffset(),
            m_transitionMatrix.endOffset(),
            seekOffsets, m_seekScrollTarget);
        m_seekScrollTarget.reset();
        ImGui::EndChild();
    }
}

void Application::drawRibbonColumn() {
    const auto& bytes = m_document.bytes();
    if (bytes.empty()) {
        ImGui::TextUnformatted("Ribbon unavailable: no data loaded.");
        return;
    }

    const std::size_t ribbonWidth = static_cast<std::size_t>(m_ribbonWidth);
    const std::size_t rowCount = (bytes.size() + ribbonWidth - 1) / ribbonWidth;
    const float widthAvailable = ImGui::GetContentRegionAvail().x;

    // Reserve margins for cursor triangles and coordinate labels.
    const float leftMargin  = Constants::kRibbonLeftMargin;
    const float rightMargin = Constants::kRibbonRightMargin;
    const float pixelSpace  = widthAvailable - leftMargin - rightMargin;

    // Scale pixels to fit available space; use integer scaling when room allows,
    // fractional scaling when the ribbon width exceeds the panel width.
    float pixelScale = pixelSpace / static_cast<float>(ribbonWidth);
    if (pixelScale >= 2.0F) {
        pixelScale = std::floor(pixelScale);
    }
    pixelScale = std::max(0.5F, pixelScale);

    const float contentWidth  = static_cast<float>(ribbonWidth) * pixelScale;
    const float totalWidth    = leftMargin + contentWidth + rightMargin;
    const float contentHeight = std::max(1.0F, static_cast<float>(rowCount) * pixelScale);

    ImGui::TextUnformatted("Bitmap Ribbon");
    ImGui::Text("Rows: %zu", rowCount);
    ImGui::Separator();

    ImGui::BeginChild("RibbonScroll", ImVec2(0.0F, 0.0F), true, ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 contentOrigin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("RibbonCanvas", ImVec2(totalWidth, contentHeight));

    // Pixel area starts after the left margin.
    const float pixelOriginX = contentOrigin.x + leftMargin;

    const float scrollY = ImGui::GetScrollY();
    const float viewHeight = ImGui::GetWindowHeight();
    const std::size_t firstVisibleRow = static_cast<std::size_t>(std::max(0.0F, std::floor(scrollY / pixelScale)));
    const std::size_t lastVisibleRow = std::min(
        rowCount,
        static_cast<std::size_t>(std::ceil((scrollY + viewHeight) / pixelScale)) + 1);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (std::size_t row = firstVisibleRow; row < lastVisibleRow; ++row) {
        const float y0 = contentOrigin.y + static_cast<float>(row) * pixelScale;
        const float y1 = y0 + pixelScale;
        const std::size_t rowStart = row * ribbonWidth;

        for (std::size_t column = 0; column < ribbonWidth; ++column) {
            const std::size_t byteIndex = rowStart + column;
            const std::uint8_t value = byteIndex < bytes.size() ? bytes[byteIndex] : 0;
            const ImU32 color = IM_COL32(Constants::kRibbonByteColorR, value, Constants::kRibbonByteColorB, 255);
            const float x0 = pixelOriginX + static_cast<float>(column) * pixelScale;
            drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + pixelScale, y1), color);
        }
    }

    if (m_transitionMatrix.endOffset() > m_transitionMatrix.startOffset()) {
        const std::size_t startRow = m_transitionMatrix.startOffset() / ribbonWidth;
        const std::size_t endRow   = (m_transitionMatrix.endOffset() - 1) / ribbonWidth;
        const float markerY0 = contentOrigin.y + static_cast<float>(startRow) * pixelScale;
        const float markerY1 = contentOrigin.y + static_cast<float>(endRow + 1) * pixelScale;
        drawList->AddRectFilled(
            ImVec2(pixelOriginX, markerY0),
            ImVec2(pixelOriginX + contentWidth, markerY1),
            Constants::kRibbonHighlightFill);
        drawList->AddRect(
            ImVec2(pixelOriginX, markerY0),
            ImVec2(pixelOriginX + contentWidth, markerY1),
            Constants::kRibbonHighlightBorder,
            0.0F,
            0,
            2.0F);
    }

    // Handle click on ribbon to select byte offset and scroll hex view.
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const float localX = std::clamp(ImGui::GetMousePos().x - pixelOriginX, 0.0F, contentWidth - 1.0F);
        const float localY = std::clamp(ImGui::GetMousePos().y - contentOrigin.y, 0.0F, contentHeight - 1.0F);
        const auto clickCol = static_cast<std::size_t>(localX / pixelScale);
        const auto clickRow = static_cast<std::size_t>(localY / pixelScale);
        const std::size_t clickOffset = clickRow * ribbonWidth + std::min(clickCol, ribbonWidth - 1);
        if (clickOffset < bytes.size()) {
            m_selectedOffset = clickOffset;
            m_seekScrollTarget = clickOffset;
        }
    }

    // Handle drag on ribbon to scrub the analysis window (non-Full-View mode).
    if (!m_fullViewEnabled && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float localY = std::clamp(ImGui::GetMousePos().y - contentOrigin.y, 0.0F, contentHeight - 1.0F);
        const float normalizedY = (contentHeight <= 1.0F) ? 0.0F : (localY / contentHeight);
        const std::size_t centerOffset = static_cast<std::size_t>(normalizedY * static_cast<float>(bytes.size() - 1));
        setWindowFromCenter(centerOffset);
    }

    // Overlay seeking highlight on matching byte positions in the ribbon.
    if (m_seek.seekEnabled && m_seek.valid && !m_seek.result.offsets.empty()) {
        for (const std::size_t offset : m_seek.result.offsets) {
            if (offset >= bytes.size()) {
                continue;
            }
            const std::size_t seekRow = offset / ribbonWidth;
            const std::size_t seekCol = offset % ribbonWidth;
            if (seekRow < firstVisibleRow || seekRow >= lastVisibleRow) {
                continue;
            }
            const float sx0 = pixelOriginX + static_cast<float>(seekCol) * pixelScale;
            const float sy0 = contentOrigin.y + static_cast<float>(seekRow) * pixelScale;
            drawList->AddRectFilled(ImVec2(sx0, sy0), ImVec2(sx0 + pixelScale, sy0 + pixelScale),
                                    Constants::kSeekHighlightFill);
            drawList->AddRect(ImVec2(sx0, sy0), ImVec2(sx0 + pixelScale, sy0 + pixelScale),
                              Constants::kSeekHighlightBorder, 0.0F, 0, 1.0F);
        }
    }

    // Draw cursor pointer triangles for the selected offset.
    if (m_selectedOffset < bytes.size()) {
        const std::size_t curRow = m_selectedOffset / ribbonWidth;
        const std::size_t curCol = m_selectedOffset % ribbonWidth;
        const float cellCenterX = pixelOriginX + (static_cast<float>(curCol) + 0.5F) * pixelScale;
        const float cellCenterY = contentOrigin.y + (static_cast<float>(curRow) + 0.5F) * pixelScale;
        const float triSz = Constants::kRibbonCursorTriSize;
        const ImU32 triCol = Constants::kRibbonCursorColor;

        // Left-edge triangle in the left margin, pointing right toward pixels.
        const float leftTriTipX = pixelOriginX;
        drawList->AddTriangleFilled(
            ImVec2(leftTriTipX - triSz, cellCenterY - triSz),
            ImVec2(leftTriTipX - triSz, cellCenterY + triSz),
            ImVec2(leftTriTipX,         cellCenterY),
            triCol);

        // Right-edge triangle in the right margin, pointing left toward pixels.
        const float rightTriTipX = pixelOriginX + contentWidth;
        drawList->AddTriangleFilled(
            ImVec2(rightTriTipX + triSz, cellCenterY - triSz),
            ImVec2(rightTriTipX + triSz, cellCenterY + triSz),
            ImVec2(rightTriTipX,         cellCenterY),
            triCol);

        // Coordinate label in the right margin, after the triangle.
        char curLabel[24];
        std::snprintf(curLabel, sizeof(curLabel), "%zu:%zu", curRow, curCol);
        const ImVec2 labelSize = ImGui::CalcTextSize(curLabel);
        const float pad = 2.0F;
        const float labelX = rightTriTipX + triSz + pad;
        const float labelY = cellCenterY - labelSize.y * 0.5F;
        drawList->AddRectFilled(
            ImVec2(labelX - pad, labelY - pad),
            ImVec2(labelX + labelSize.x + pad, labelY + labelSize.y + pad),
            Constants::kRibbonCursorLabelBg, 2.0F);
        drawList->AddText(ImVec2(labelX, labelY), Constants::kRibbonCursorLabelText, curLabel);
    }

    ImGui::EndChild();
}

void Application::drawWorkspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    constexpr ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    ImGui::Begin("BinXrayWorkspace", nullptr, windowFlags);
    ImGui::PopStyleVar(2);

    const float spacing     = ImGui::GetStyle().ItemSpacing.x;
    const float totalWidth  = ImGui::GetContentRegionAvail().x;
    float leftWidth   = std::clamp(totalWidth * Constants::kLeftColumnRatio,  Constants::kLeftColumnMin,  Constants::kLeftColumnMax);
    float rightWidth  = std::clamp(totalWidth * Constants::kRightColumnRatio, Constants::kRightColumnMin, Constants::kRightColumnMax);
    float centerWidth = totalWidth - leftWidth - rightWidth - (2.0F * spacing);

    if (centerWidth < Constants::kCenterColumnMin) {
        const float deficit       = Constants::kCenterColumnMin - centerWidth;
        const float leftReduction = std::min(deficit * 0.5F, std::max(0.0F, leftWidth - Constants::kLeftColumnHardMin));
        leftWidth  -= leftReduction;
        rightWidth -= (deficit - leftReduction);
        rightWidth  = std::max(Constants::kRightColumnHardMin, rightWidth);
        centerWidth = totalWidth - leftWidth - rightWidth - (2.0F * spacing);
    }

    ImGui::BeginChild("LeftControls", ImVec2(leftWidth, 0.0F), true);
    drawControlsColumn();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("CenterViews", ImVec2(centerWidth, 0.0F), true);
    drawCenterColumn();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("RightRibbon", ImVec2(0.0F, 0.0F), true);
    drawRibbonColumn();
    ImGui::EndChild();

    ImGui::End();
}

void Application::renderFrame() {
    pollAsyncFileLoad();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open...", nullptr, false, !m_isLoadingFile)) {
                const std::optional<std::wstring> selectedPath = promptOpenFilePath();
                if (selectedPath.has_value()) {
                    startAsyncFileLoad(selectedPath.value());
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                m_running = false;
            }
            ImGui::EndMenu();
        }

        ImGui::Text("Build: %s", BXR_BUILD_VERSION);
        ImGui::EndMainMenuBar();
    }

    drawWorkspace();

    ImGui::Render();

    m_deviceContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
    m_deviceContext->ClearRenderTargetView(m_mainRenderTargetView, Constants::kClearColor);

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_swapChain->Present(1, 0);
}

} // namespace BinXray::UI
