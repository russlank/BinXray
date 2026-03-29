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
- **Point opacity**: A slider (5 %–100 %) controls alpha transparency of
  scatter dots, making dense clusters readable without hiding shadowed
  points behind opaque neighbours.
- **Canvas background**: Three presets — Black (default), White, Custom.
  The Custom preset exposes an RGB colour picker.  A white background
  makes dark low-intensity points easier to spot.
- **Canvas clipping**: All wireframe edges, axis labels, and scatter dots
  are clipped to the canvas rectangle so nothing overflows into
  surrounding UI panels.
- **Seeking disabled**: The 2D-only seeking controls are greyed out in 3D
  mode.

## Build & Tooling

- Visual Studio projects (`.vcxproj`) under `src`.
- Solution container (`src/BinXray.slnx`) references app + tests and key docs/scripts.
- VS Code tasks and debug profiles mirror Visual Studio configurations.
- `scripts/invoke-msbuild.ps1` is the canonical CLI build wrapper.
- GitHub Actions release pipeline (`.github/workflows/release-packaging.yml`).
- WiX 6.x packaging via `packaging/build-packages.ps1`.

## Performance Optimizations

### Dirty-Index Trigram Computation
`TrigramPlot::compute()` uses a thread-local 64 MB voxel buffer.  Instead
of zeroing all 16 M cells between recomputations, a dirty-index list
tracks which cells were written.  Only those cells are extracted and reset,
making the clear cost proportional to the number of unique trigrams rather
than the full cube size.

### Heat-Map Lookup Table
`getHeatMapLUT()` builds a static 256-entry `ImU32` colour table once.
Both the 2D matrix renderer and the 3D scatter loop index this table
directly, eliminating per-pixel colour interpolation.

### Projection Math Folding
In `draw3DPlot()`, the division by 127.5 is folded into the scale factor
(`invNorm = rawScale / 127.5F`).  Centering and projection are combined
into a single multiply-add per axis per point.

### Bit-Shift Matrix Loop
`drawMatrixPlot()` replaces `i / 256` and `i % 256` with `i >> 8` and
`i & 0xFF` for row/column extraction in the 65 536-element flat loop.

### Post-Loop Max Count
`TransitionMatrix::compute()` finds the maximum count with a single pass
over the 64 K counts array after accumulation, instead of a per-iteration
branch during the main scanning loop.

### Event-Driven Idle Rendering
`Application::run()` checks `needsContinuousRender()` each iteration.
When no animation (auto-rotation), async I/O, or pending matrix
recomputation is active, the loop calls
`MsgWaitForMultipleObjects(INFINITE)` and blocks until the OS delivers an
input or paint message.  This drops CPU and GPU utilisation to near-zero
when the application is idle.

### 3D-Mode-Gated Trigram Recompute
`Application::rebuildMatrixIfDirty()` computes the trigram cube only when
3D mode is active.  In 2D mode, only the transition matrix is recomputed,
which reduces scrub latency and avoids unnecessary 64 MB worker-buffer churn.

### Narrow-Layout Safety and Hit-Testing Guards
The centre column now falls back to stacked hex/address rendering when width
is insufficient for side-by-side panes.  The workspace column allocation
includes a final non-negative-width guard for very narrow windows.
Additionally, matrix and ribbon interactions only trigger from their actual
pixel areas (not surrounding margin zones), preventing accidental
freeze/scrub/select actions.

## Test Strategy

- `BinXray.Tests` runs seven suites: `ByteFormatterTests`,
  `BinaryDocumentTests`, `TransitionMatrixTests`, `TransitionSeekerTests`,
  `TrigramPlotTests`, `CrosshairCoordsTests`, `UILayoutLogicTests`.
- Edge cases covered: missing/empty file loads, single byte, sub-ranges,
  boundary clamping, maxResults capping, self-transitions, inverted
  ranges, repeated trigram accumulation, mapIntensity modes, crosshair
  coordinate semantics, opacity-alpha scaling validation.
- Process exit code reflects suite pass/fail for CI gating.

## Planned Evolution

- Dockable panels, ribbon side toggle, and adaptive range controls.
- Rolling histogram and chunk-acceleration for real-time scrubbing.
- Repetition-aware shading and periodicity strip.
- Structured decoders (header/record templates).
- GPU-accelerated rendering path.
