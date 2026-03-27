// SPDX-License-Identifier: MIT

#include "Application.h"

#include "UIConstants.h"
#include "Version.h"

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <commdlg.h>
#include <d3d11.h>
#include <dxgi.h>
#include <utility>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace BinXray::UI {

namespace {
Application* g_applicationInstance = nullptr;
constexpr wchar_t kWindowClass[] = L"BinXrayWindowClass";
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
      m_fullViewEnabled(true),
      m_blockSize(Constants::kBlockSizeDefault),
      m_windowStartOffset(0),
      m_windowEndOffset(0),
      m_matrixDirty(true),
      m_isLoadingFile(false),
      m_loadingPath(),
      m_lastLoadError(),
      m_ribbonWidth(Constants::kRibbonWidthDefault),
      m_hexViewPanel() {
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

    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEXW),
        CS_CLASSDC,
        Application::WndProc,
        0L,
        0L,
        hInstance,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        kWindowClass,
        nullptr
    };
    ::RegisterClassExW(&wc);

    m_hWnd = ::CreateWindowW(
        wc.lpszClassName,
        L"Bin X-ray",
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
    m_matrixDirty = false;
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

    controlsChanged = ImGui::Checkbox("Full View", &m_fullViewEnabled) || controlsChanged;
    if (ImGui::InputInt("Block Size", &m_blockSize, Constants::kBlockSizeStep, Constants::kBlockSizeFastStep)) {
        m_blockSize = std::max(Constants::kBlockSizeMin, m_blockSize);
        controlsChanged = true;
    }

    ImGui::Separator();
    if (ImGui::InputInt("Ribbon Width", &m_ribbonWidth, Constants::kRibbonWidthStep, Constants::kRibbonWidthFastStep)) {
        m_ribbonWidth = std::clamp(m_ribbonWidth, Constants::kRibbonWidthMin, Constants::kRibbonWidthMax);
    }

    if (controlsChanged) {
        refreshRangeAfterDocumentChange();
    }

    const auto& bytes = m_document.bytes();
    ImGui::Separator();
    ImGui::Text("Bytes: %zu", bytes.size());
    if (!m_document.sourcePath().empty()) {
        ImGui::TextWrapped("File: %ls", m_document.sourcePath().c_str());
    } else {
        ImGui::TextUnformatted("File: sample dataset");
    }

    ImGui::Text("Range: 0x%zX - 0x%zX", m_transitionMatrix.startOffset(), m_transitionMatrix.endOffset());
    ImGui::Text("Max Density: %u", m_transitionMatrix.maxCount());

    if (!bytes.empty()) {
        m_selectedOffset = std::min(m_selectedOffset, bytes.size() - 1);
        const std::uint8_t value = bytes[m_selectedOffset];
        ImGui::Separator();
        ImGui::Text("Selected Offset: 0x%zX", m_selectedOffset);
        ImGui::Text("Selected Byte: %u", static_cast<unsigned int>(value));
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
    const float plotSize = std::max(Constants::kMatrixPlotMinSize, std::min(available.x, available.y));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("TransitionPlotCanvas", ImVec2(plotSize, plotSize));

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float cellSize = plotSize / static_cast<float>(Core::TransitionMatrix::kDimension);
    for (std::size_t i = 0; i < Core::TransitionMatrix::kCellCount; ++i) {
        const std::uint8_t intensity = m_matrixLuminance[i];
        if (intensity == 0) {
            continue;
        }
        const std::size_t row    = i / Core::TransitionMatrix::kDimension;
        const std::size_t column = i % Core::TransitionMatrix::kDimension;
        const ImU32  color = IM_COL32(intensity, intensity, intensity, 255);
        const float  x0    = origin.x + static_cast<float>(column) * cellSize;
        const float  y0    = origin.y + static_cast<float>(row)    * cellSize;
        drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + cellSize, y0 + cellSize), color);
    }

    drawList->AddRect(origin, ImVec2(origin.x + plotSize, origin.y + plotSize), Constants::kMatrixBorderColor, 0.0F, 0, 1.0F);
}

void Application::drawCenterColumn() {
    rebuildMatrixIfDirty();

    const float totalHeight = ImGui::GetContentRegionAvail().y;
    const float plotHeight  = std::max(Constants::kMatrixPlotMinHeight, totalHeight * Constants::kMatrixPlotHeightRatio);

    ImGui::BeginChild("MatrixView", ImVec2(0.0F, plotHeight), true);
    drawMatrixPlot();
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::BeginChild("HexView", ImVec2(0.0F, 0.0F), true);
    ImGui::TextUnformatted("Hex View");
    ImGui::Separator();
    m_hexViewPanel.drawEmbedded(
        m_document,
        m_selectedOffset,
        m_transitionMatrix.startOffset(),
        m_transitionMatrix.endOffset());
    ImGui::EndChild();
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
    const float pixelScale   = std::max(1.0F, std::floor((widthAvailable - Constants::kRibbonScrollMargin) / static_cast<float>(ribbonWidth)));
    const float contentWidth = static_cast<float>(ribbonWidth) * pixelScale;
    const float contentHeight = std::max(1.0F, static_cast<float>(rowCount) * pixelScale);

    ImGui::TextUnformatted("Bitmap Ribbon");
    ImGui::Text("Rows: %zu", rowCount);
    ImGui::Separator();

    ImGui::BeginChild("RibbonScroll", ImVec2(0.0F, 0.0F), true, ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 contentOrigin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("RibbonCanvas", ImVec2(contentWidth, contentHeight));

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
            const float x0 = contentOrigin.x + static_cast<float>(column) * pixelScale;
            drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + pixelScale, y1), color);
        }
    }

    if (m_transitionMatrix.endOffset() > m_transitionMatrix.startOffset()) {
        const std::size_t startRow = m_transitionMatrix.startOffset() / ribbonWidth;
        const std::size_t endRow   = (m_transitionMatrix.endOffset() - 1) / ribbonWidth;
        const float markerY0 = contentOrigin.y + static_cast<float>(startRow) * pixelScale;
        const float markerY1 = contentOrigin.y + static_cast<float>(endRow + 1) * pixelScale;
        drawList->AddRectFilled(
            ImVec2(contentOrigin.x, markerY0),
            ImVec2(contentOrigin.x + contentWidth, markerY1),
            Constants::kRibbonHighlightFill);
        drawList->AddRect(
            ImVec2(contentOrigin.x, markerY0),
            ImVec2(contentOrigin.x + contentWidth, markerY1),
            Constants::kRibbonHighlightBorder,
            0.0F,
            0,
            2.0F);
    }

    if (!m_fullViewEnabled && ImGui::IsItemHovered() && (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
        const float localY = std::clamp(ImGui::GetMousePos().y - contentOrigin.y, 0.0F, contentHeight - 1.0F);
        const float normalizedY = (contentHeight <= 1.0F) ? 0.0F : (localY / contentHeight);
        const std::size_t centerOffset = static_cast<std::size_t>(normalizedY * static_cast<float>(bytes.size() - 1));
        m_selectedOffset = centerOffset;
        setWindowFromCenter(centerOffset);
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
