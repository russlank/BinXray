# Bin X-ray Quality and Testing Guide

## Scope

This document defines the practical quality gates used for local work and CI.
It complements `run-guide.md` (how to build/run) and `architecture.md`
(system structure and design rationale).

## Automated Test Suites

The solution currently runs six suites from `BinXray.Tests`:

- `ByteFormatterTests`
- `BinaryDocumentTests`
- `TransitionMatrixTests`
- `TransitionSeekerTests`
- `TrigramPlotTests`
- `CrosshairCoordsTests`

### What They Cover

- Formatting correctness for bytes and offsets.
- Binary document loading success and failure paths (including missing and empty files).
- Transition matrix range clamping, intensity mapping, and inverted-range safety.
- Transition-seek offset scanning, max-result capping, and range edge cases.
- Trigram accumulation correctness, intensity mapping, and recompute reset behavior.
- Hex-selection-to-plot coordinate mapping for both 2D and 3D crosshair modes.

## Local Verification Steps

1. Build the solution in `Debug|x64`.
2. Run `BinXray.Tests.exe` and verify all suites pass.
3. Launch `BinXray.exe`.
4. Smoke-test these flows manually:
   - Open a large binary file.
   - Scrub via ribbon in non-Full-View mode and verify matrix updates.
   - Enable seeking, hover matrix, click-to-freeze/unfreeze, navigate via address list.
   - Toggle 3D mode, rotate/auto-rotate, and verify seek UI remains 2D-only.
   - Switch between grayscale and heat map; validate visual responsiveness.
   - Shrink window width aggressively and verify hex + address panels remain usable.
   - Click matrix/ribbon margin areas and verify no unintended freeze/select/scrub.

## Recommended Next Quality Additions

1. Golden-image parity tests against legacy BinView outputs (`BXR-010`).
2. A small integration harness for UI-state transitions (seek ↔ 3D mode, range updates).
3. Performance regression checks for scrub latency and trigram recompute time.
4. Memory-mapped I/O path benchmarking for very large files.
