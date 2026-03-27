# Bin X-ray Architecture (Phase 1)

## Goals

- Keep project organization close to `XpressFormula` for familiarity.
- Establish a maintainable C++/ImGui desktop foundation.
- Keep domain logic separate from UI rendering logic.

## Top-Level Structure

- `src/BinXray`: main executable project
- `src/BinXray.Tests`: lightweight test executable
- `src/vendor/imgui`: vendored Dear ImGui sources
- `scripts`: build helpers for CLI/CI reuse
- `packaging`: installer scaffolding
- `doc`: project docs

## Application Layers

1. `Core`
   - Binary data model and formatting helpers.
   - Transition matrix computation and luminance rendering.
   - Transition seeker (byte-pair offset scanner for seeking).
   - No UI dependencies.

2. `UI`
   - Win32 + D3D11 host (`Application`).
   - ImGui panel composition (`HexViewPanel`, `InspectorPanel`, `StatusBar`).
   - `SeekState` struct for crosshair/highlight interaction state.

3. `Entry`
   - `main.cpp` starts application lifecycle.

## Build & Tooling

- Visual Studio projects (`.vcxproj`) under `src`.
- Solution container (`src/BinXray.slnx`) references app + tests and key docs/scripts.
- VS Code tasks and debug profiles mirror Visual Studio configurations.
- `scripts/invoke-msbuild.ps1` is the canonical CLI build wrapper.

## Planned Evolution

- Add binary file loading workflow + MRU.
- Add structured decoders (headers/records templates).
- Add search/seek, entropy, and diff views.
- Add richer automated tests for parsers and decoders.
