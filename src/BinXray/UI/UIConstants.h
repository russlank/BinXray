// SPDX-License-Identifier: MIT
#pragma once

#include "imgui.h"

#include <cstddef>
#include <cstdint>

namespace BinXray::UI::Constants {

// ── Window ───────────────────────────────────────────────────────────────────
constexpr int kWindowInitialX      = 100;
constexpr int kWindowInitialY      = 100;
constexpr int kWindowInitialWidth  = 1600;
constexpr int kWindowInitialHeight = 900;

// ── Background clear colour ───────────────────────────────────────────────────
constexpr float kClearColor[4] = {0.08F, 0.10F, 0.14F, 1.0F};

// ── Layout columns ────────────────────────────────────────────────────────────
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
constexpr float kMatrixPlotMinSize     = 64.0F;
constexpr float kMatrixPlotMinHeight   = 280.0F;
constexpr float kMatrixPlotHeightRatio = 0.58F;
constexpr ImU32 kMatrixBorderColor     = IM_COL32(200, 200, 200, 255);

// ── Ribbon ────────────────────────────────────────────────────────────────────
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

// ── Hex view ──────────────────────────────────────────────────────────────────
constexpr std::size_t kHexBytesPerRow          = 16;
constexpr std::size_t kHexMaxVisibleBytes      = 4096;
constexpr ImU32       kHexSelectedColor        = IM_COL32( 55, 120, 220, 255);
constexpr ImU32       kHexSelectedHoveredColor = IM_COL32( 75, 140, 240, 255);

// ── Controls ──────────────────────────────────────────────────────────────────
constexpr ImVec4 kErrorTextColor   = {0.95F, 0.40F, 0.40F, 1.0F};
constexpr int    kBlockSizeDefault = 10240;
constexpr int    kBlockSizeMin     =     2;
constexpr int    kBlockSizeStep    =   256;
constexpr int    kBlockSizeFastStep =  2048;

// ── Seeking overlay ───────────────────────────────────────────────────────────
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

} // namespace BinXray::UI::Constants
