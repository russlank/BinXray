# Bin X-ray Manual UI Smoke Test

## Purpose

Quickly validate end-to-end UI behavior after code changes, especially
interaction wiring between controls, plots, ribbon, and hex view.

## Preconditions

1. Build `Debug|x64`.
2. Launch `BinXray.exe`.
3. Have at least one binary file available (small and medium-sized).

## Script

1. Open a file from `File > Open...`.
Expected: load starts, UI remains responsive, bytes/file info update when complete.

2. Toggle `Scale`, `Normalize`, and `Heat Map`.
Expected: matrix/trigram appearance changes immediately, no stale redraw.

3. Disable `Full View` and drag on ribbon pixel area.
Expected: analysis window moves with drag; matrix updates continuously.

4. Drag the highlighted ribbon window's top and bottom edges.
Expected: window size changes directly from the ribbon; `Block Size` value follows; drag does not fall through to ribbon scrub.

5. Adjust the `Block Size` gauge under the numeric control.
Expected: analysis window length updates immediately via mouse drag.

6. Adjust the `Ribbon Width` gauge under the numeric control.
Expected: ribbon column width changes immediately and remains clamped to limits (`4..8192`).

7. Hover `Block Size` / `Ribbon Width` controls and use mouse wheel.
Expected: values step up/down from wheel input (`Shift` accelerates); block-size wheel updates active window length.

8. Hover highlighted ribbon window edge and use mouse wheel.
Expected: hovered edge moves in steps without triggering scrub; analysis window size updates immediately.

9. Hold `Ctrl` and use mouse wheel over ribbon pixels.
Expected: `Block Size` changes from wheel input while ribbon is hovered.

10. Hold `Ctrl+Shift` and use mouse wheel over ribbon pixels.
Expected: `Ribbon Width` changes from wheel input while ribbon is hovered.

11. Set `Ribbon Width` to a very large value (for example `4096` or `8192`).
Expected: ribbon cells remain readable (minimum 1 px); horizontal scrolling appears so off-screen ribbon content and cursor markers can be reached.

12. Enable `Auto-Slide` in controls with `Repeat` disabled and set moderate speed.
Expected: active ribbon window advances automatically in row-proportional steps and stops at end-of-data.

13. Enable `Repeat` while `Auto-Slide` is on.
Expected: after reaching end-of-data, window wraps to the beginning and continues sliding.

14. Enable `3D Mode` + `Auto-Rotate` while `Auto-Slide` remains enabled.
Expected: auto-rotation and auto-slide run in parallel without blocking each other.

15. Click ribbon pixel area once.
Expected: selected offset changes; hex view scrolls to selected location.

16. Enable seeking in 2D mode, hover matrix plot pixels, click to freeze/unfreeze.
Expected: crosshair and counts update on hover; freeze toggles only from plot area.

17. Click matrix margin area (label margins).
Expected: no freeze toggle and no seek update from margin-only clicks.

18. Select an address in the seek list.
Expected: selected byte updates and hex view scroll target is applied once.

19. Switch to 3D mode and verify seek behavior.
Expected: 2D seek address pane is hidden; 3D interactions remain functional.

20. Test narrow window behavior by aggressively shrinking width.
Expected: no visual breakage; hex + addresses remain usable (stacked fallback when needed).

21. Rotate 3D plot (drag), then enable auto-rotate.
Expected: manual drag works when auto-rotate is off; auto-rotate animates smoothly when on.

22. Toggle 3D background modes (`Black`, `White`, `Custom`) and opacity slider.
Expected: visible contrast changes; point transparency changes immediately.

23. Return to 2D mode and verify normal seeking still works.
Expected: seek behavior recovers cleanly; no stale 3D-only state leakage.

## Pass/Fail Criteria

- Pass: all expected behaviors above are observed without crashes or severe UI glitches.
- Fail: any crash, stuck loading state, non-responsive interaction, or incorrect mode-coupling.
