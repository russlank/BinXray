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
     positions for a selected (from, to) transition coordinate.   - `TrigramPlot` – 256³ byte-trigram density accumulator for the 3D
     scatter plot.  Uses a thread-local 64 MB voxel buffer for efficient
     recomputation during scrubbing.   - No UI dependencies; independently testable.

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
### Heat-Map Colour Mode
The transition plot can render in greyscale (default) or a heat-map
gradient (blue→cyan→green→yellow→red).  Coordinate margins ensure
labels never overlap the plot area.
### Bitmap Ribbon Navigation
Clicking a pixel in the bitmap ribbon selects that byte offset, scrolls
the hex view, and moves the red cursor triangles.  Dragging the ribbon
(in windowed mode) additionally scrubs the analysis range.

### Snap-to-Data
When enabled, the seeking crosshair automatically snaps to the nearest
non-empty transition cell within a configurable Chebyshev radius.

### 3D Trigram Scatter Plot
When *3D Mode* is enabled, the centre column replaces the 2D transition
matrix with an interactive 256³ byte-trigram scatter plot.  Each non-empty
voxel is drawn as a small filled rectangle via ImDrawList orthographic
projection with yaw/pitch rotation.

- **Mouse-drag rotation**: Click-drag rotates the view (yaw around Y, pitch
  around X).  Pitch is clamped to ±1.5 rad.
- **Auto-rotation**: A checkbox enables continuous yaw advance at a
  configurable speed (0.05“5.0 deg/frame) with an adjustable elevation
  slider (−89° to 89°).  Manual drag is disabled during auto-rotation.
- **Double-click reset**: Restores default yaw, pitch, and elevation.
- **Heat-map / greyscale**: The same colour toggle applies to 3D points.
- **Seeking disabled**: The 2D-only seeking controls are greyed out in 3D
  mode.

## Build & Tooling

- Visual Studio projects (`.vcxproj`) under `src`.
- Solution container (`src/BinXray.slnx`) references app + tests and key docs/scripts.
- VS Code tasks and debug profiles mirror Visual Studio configurations.
- `scripts/invoke-msbuild.ps1` is the canonical CLI build wrapper.
- GitHub Actions release pipeline (`.github/workflows/release-packaging.yml`).
- WiX 6.x packaging via `packaging/build-packages.ps1`.

## Test Strategy

- `BinXray.Tests` runs five suites: `ByteFormatterTests`,
  `BinaryDocumentTests`, `TransitionMatrixTests`, `TransitionSeekerTests`,
  `TrigramPlotTests`.
- Edge cases covered: empty data, single byte, sub-ranges, boundary
  clamping, maxResults capping, self-transitions, inverted ranges,
  repeated trigram accumulation, mapIntensity modes.
- Process exit code reflects suite pass/fail for CI gating.

## Planned Evolution

- Dockable panels, ribbon side toggle, and adaptive range controls.
- Rolling histogram and chunk-acceleration for real-time scrubbing.
- Repetition-aware shading and periodicity strip.
- Structured decoders (header/record templates).
- GPU-accelerated rendering path.
