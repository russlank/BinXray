# Bin X-ray Architecture (Phase 1-2)

## Goals

- Keep project organization close to `XpressFormula` for familiarity.
- Establish a maintainable C++/ImGui desktop foundation.
- Keep domain logic separate from UI rendering logic.

## Top-Level Structure

- `src/BinXray`: main executable project
- `src/BinXray.Tests`: lightweight test executable
- `src/vendor/imgui`: vendored Dear ImGui sources
- `scripts`: build helpers for CLI/CI reuse
- `packaging`: WiX v4 installer scaffolding (MSI + Burn bundle)
- `doc`: project docs

## Application Layers

1. `Core`
   - `BinaryDocument` \u2013 in-memory binary file model with async loading.
   - `ByteFormatter` \u2013 lightweight hex-string formatting helpers.
   - `TransitionMatrix` \u2013 256\u00d7256 byte-pair density computation and luminance
     rendering (Binary, Linear, Normalized modes).
   - `TransitionSeeker` \u2013 byte-pair offset scanner; returns matching file
     positions for a selected (from, to) transition coordinate.
   - No UI dependencies; independently testable.

2. `UI`
   - `Application` \u2013 Win32 + D3D11 host, ImGui context owner, three-column
     workspace (left controls, centre views, right ribbon).
   - `HexViewPanel` \u2013 virtualised hex dump (ImGuiListClipper) with seek
     highlight and programmatic scroll-to support.
   - `InspectorPanel` \u2013 byte inspector for the selected offset.
   - `StatusBar` \u2013 status information display.
   - `UIConstants` \u2013 centralised compile-time constants for colours, sizes,
     and layout ratios.
   - `SeekState` struct owns crosshair/highlight interaction state.

3. `Entry`
   - `main.cpp` starts application lifecycle.

## Interactive Features

### Transition-Plot Seeking
Hovering the 256\u00d7256 matrix shows a crosshair and locates all file offsets
with that byte-pair transition.  Results appear in a scrollable address
list beside the hex view.  Clicking an address scrolls the hex view to
that position and updates the bitmap ribbon cursor.

### Bitmap Ribbon Navigation
Clicking a pixel in the bitmap ribbon selects that byte offset, scrolls
the hex view, and moves the red cursor triangles.  Dragging the ribbon
(in windowed mode) additionally scrubs the analysis range.

### Snap-to-Data
When enabled, the seeking crosshair automatically snaps to the nearest
non-empty transition cell within a configurable Chebyshev radius.

## Build & Tooling

- Visual Studio projects (`.vcxproj`) under `src`.
- Solution container (`src/BinXray.slnx`) references app + tests and key docs/scripts.
- VS Code tasks and debug profiles mirror Visual Studio configurations.
- `scripts/invoke-msbuild.ps1` is the canonical CLI build wrapper.
- GitHub Actions release pipeline (`.github/workflows/release-packaging.yml`).
- WiX 6.x packaging via `packaging/build-packages.ps1`.

## Test Strategy

- `BinXray.Tests` runs four suites: `ByteFormatterTests`,
  `BinaryDocumentTests`, `TransitionMatrixTests`, `TransitionSeekerTests`.
- Edge cases covered: empty data, single byte, sub-ranges, boundary
  clamping, maxResults capping, self-transitions, inverted ranges.
- Process exit code reflects suite pass/fail for CI gating.

## Planned Evolution

- Dockable panels, ribbon side toggle, and adaptive range controls.
- Rolling histogram and chunk-acceleration for real-time scrubbing.
- Repetition-aware shading and periodicity strip.
- Structured decoders (header/record templates).
- GPU-accelerated rendering path.
