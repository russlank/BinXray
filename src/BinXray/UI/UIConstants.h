// SPDX-License-Identifier: MIT
//
// UIConstants.h
//
// Centralised compile-time constants for the BinXray UI layer.
// Grouping all magic numbers here keeps the rendering code clean
// and makes visual tuning straightforward.
//
#pragma once

#include "imgui.h"

#include <cstddef>
#include <cstdint>

namespace BinXray::UI::Constants {

// ── Window ───────────────────────────────────────────────────────────────────
/// Default position and size for the main application window.
constexpr int kWindowInitialX      = 100;
constexpr int kWindowInitialY      = 100;
constexpr int kWindowInitialWidth  = 1600;
constexpr int kWindowInitialHeight = 900;

// ── Background clear colour ───────────────────────────────────────────────────
/// RGBA float colour used to clear the D3D11 render target each frame.
constexpr float kClearColor[4] = {0.08F, 0.10F, 0.14F, 1.0F};

// ── Layout columns ────────────────────────────────────────────────────────────
/// Three-column workspace: left controls, centre views, right ribbon.
/// Ratios are proportions of viewport width; Min/Max are pixel clamps.
constexpr float kLeftColumnRatio    = 0.22F;
constexpr float kLeftColumnMin      = 220.0F;
constexpr float kLeftColumnMax      = 360.0F;
constexpr float kRightColumnRatio   = 0.18F;
constexpr float kRightColumnMin     = 180.0F;
constexpr float kRightColumnMax     = 320.0F;
constexpr float kCenterColumnMin    = 260.0F;
constexpr float kLeftColumnHardMin  = 180.0F;
constexpr float kRightColumnHardMin = 160.0F;

// ── Matrix plot ───────────────────────────────────────────────────────────────
/// The transition plot is rendered as a square InvisibleButton with this
/// minimum size, occupying a proportion of the centre column height.
constexpr float kMatrixPlotMinSize     = 64.0F;
constexpr float kMatrixPlotMinHeight   = 280.0F;
constexpr float kMatrixPlotHeightRatio = 0.58F;
constexpr ImU32 kMatrixBorderColor     = IM_COL32(200, 200, 200, 255);

// ── Ribbon ────────────────────────────────────────────────────────────────────
/// The bitmap ribbon visualises every byte of the file as a coloured pixel.
/// Default width is 128 columns (matching legacy BinView).
constexpr int            kRibbonWidthDefault    = 128;
constexpr int            kRibbonWidthMin        =   4;
constexpr int            kRibbonWidthMax        = 1024;
constexpr int            kRibbonWidthStep       =   4;
constexpr int            kRibbonWidthFastStep   =  64;
constexpr float          kRibbonScrollMargin    = 20.0F;
constexpr ImU32          kRibbonHighlightFill   = IM_COL32(255, 220,  70,  28);
constexpr ImU32          kRibbonHighlightBorder = IM_COL32(255, 220,  70, 180);
constexpr std::uint8_t   kRibbonByteColorR      = 32;
constexpr std::uint8_t   kRibbonByteColorB      = 64;
/// Red triangle / label colours used to indicate the selected byte in the ribbon.
constexpr ImU32          kRibbonCursorColor      = IM_COL32(220,  50,  50, 230);
constexpr ImU32          kRibbonCursorLabelBg    = IM_COL32(220,  50,  50, 200);
constexpr ImU32          kRibbonCursorLabelText  = IM_COL32(255, 255, 255, 255);
constexpr float          kRibbonCursorTriSize    = 7.0F;

// ── Hex view ──────────────────────────────────────────────────────────────────
/// The hex view is rendered with ImGuiListClipper for virtualized scrolling.
constexpr std::size_t kHexBytesPerRow          = 16;
constexpr std::size_t kHexMaxVisibleBytes      = 4096;  ///< Legacy cap (unused with clipper).
constexpr ImU32       kHexSelectedColor        = IM_COL32( 55, 120, 220, 255);
constexpr ImU32       kHexSelectedHoveredColor = IM_COL32( 75, 140, 240, 255);

// ── Controls ──────────────────────────────────────────────────────────────────
constexpr ImVec4 kErrorTextColor   = {0.95F, 0.40F, 0.40F, 1.0F};
constexpr int    kBlockSizeDefault = 10240;
constexpr int    kBlockSizeMin     =     2;
constexpr int    kBlockSizeStep    =   256;
constexpr int    kBlockSizeFastStep =  2048;

// ── Seeking overlay ───────────────────────────────────────────────────────────
/// Crosshair, coordinate labels, and highlight colours for the seeking feature
/// that links the transition plot to file offsets.
constexpr ImU32  kSeekCrosshairColor       = IM_COL32(255, 200,  50, 200);
constexpr ImU32  kSeekCoordTextColor       = IM_COL32(255, 220, 100, 255);
constexpr ImU32  kSeekCoordBgColor         = IM_COL32( 30,  30,  30, 200);
constexpr ImU32  kSeekHighlightFill        = IM_COL32(255, 180,  40,  45);
constexpr ImU32  kSeekHighlightBorder      = IM_COL32(255, 180,  40, 200);
constexpr ImU32  kHexSeekHighlightColor    = IM_COL32(200, 160,  40, 255);
constexpr ImU32  kHexSeekHighlightHovered  = IM_COL32(220, 180,  60, 255);
constexpr float  kSeekCrosshairThickness   = 1.0F;
constexpr float  kSeekCoordFontScale       = 0.85F;
constexpr std::size_t kSeekMaxAddresses    = 256;
constexpr float  kSeekAddressPanelWidth    = 130.0F;  ///< Width of the address list beside the hex view.
constexpr int    kSeekSnapMaxRadius        = 24;       ///< Chebyshev radius for snap-to-data search.

// ── Controls labels ───────────────────────────────────────────────────────────
/// Warm-tint colour for section headers and field labels in the controls panel,
/// visually separating them from data values which use the default text colour.
constexpr ImVec4 kControlsLabelColor = {1.0F, 0.94F, 0.70F, 1.0F};

// ── Matrix plot margins ───────────────────────────────────────────────────────
/// Space reserved around the transition plot for coordinate labels.
constexpr float kMatrixPlotMarginLeft = 28.0F;
constexpr float kMatrixPlotMarginTop  = 20.0F;

// ── Ribbon margins ────────────────────────────────────────────────────────────
/// Space reserved on left/right of the pixel area for cursor triangles and
/// coordinate labels, ensuring they do not overlap rendered pixels.
constexpr float kRibbonLeftMargin  = 10.0F;
constexpr float kRibbonRightMargin = 70.0F;

// ── 3D Trigram plot ───────────────────────────────────────────────────────────
/// Interactive 3D byte-trigram scatter plot rendered via ImDrawList projection.
constexpr float k3DPlotPointSize       = 2.0F;     ///< Pixel radius of each trigram dot.
constexpr float k3DPlotDefaultYaw      = 0.78F;    ///< Initial yaw   (radians, ~45°).
constexpr float k3DPlotDefaultPitch    = 0.52F;    ///< Initial pitch  (radians, ~30°).
constexpr float k3DPlotRotationSpeed   = 0.005F;   ///< Radians per mouse-pixel drag.
constexpr ImU32 k3DPlotAxisColor       = IM_COL32(120, 120, 120, 180);
constexpr float k3DAutoRotateSpeedDefault = 0.5F;  ///< Default auto-rotation deg/frame.
constexpr float k3DAutoRotateSpeedMin     = 0.05F;
constexpr float k3DAutoRotateSpeedMax     = 5.0F;
constexpr float k3DElevationMin           = -89.0F; ///< Degrees.
constexpr float k3DElevationMax           =  89.0F;

// ── 3D point opacity ─────────────────────────────────────────────────────────
/// Controls transparency of scatter-plot dots so dense regions remain readable.
constexpr float k3DOpacityDefault = 1.0F;
constexpr float k3DOpacityMin     = 0.05F;
constexpr float k3DOpacityMax     = 1.0F;

// ── 3D canvas background ─────────────────────────────────────────────────────
/// Three background presets: Black (0), White (1), Custom (2).
/// The custom preset uses an RGB triplet adjustable via sliders.
constexpr int   k3DBgModeBlack   = 0;
constexpr int   k3DBgModeWhite   = 1;
constexpr int   k3DBgModeCustom  = 2;
constexpr int   k3DBgModeDefault = 0;
constexpr ImU32 k3DBgColorBlack  = IM_COL32(  10,  12,  18, 255);
constexpr ImU32 k3DBgColorWhite  = IM_COL32( 240, 240, 240, 255);
/// Default custom background colour (dark blue-grey, same as Black preset).
constexpr float k3DBgCustomDefault[3] = {0.04F, 0.05F, 0.07F};

} // namespace BinXray::UI::Constants
