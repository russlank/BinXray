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
Expected: ribbon column width changes immediately and remains clamped to limits.

7. Hover `Block Size` / `Ribbon Width` controls and use mouse wheel.
Expected: values step up/down from wheel input (`Shift` accelerates); block-size wheel updates active window length.

8. Hover highlighted ribbon window edge and use mouse wheel.
Expected: hovered edge moves in steps without triggering scrub; analysis window size updates immediately.

9. Hold `Ctrl` and use mouse wheel over ribbon pixels.
Expected: `Block Size` changes from wheel input while ribbon is hovered.

10. Hold `Ctrl+Shift` and use mouse wheel over ribbon pixels.
Expected: `Ribbon Width` changes from wheel input while ribbon is hovered.

11. Click ribbon pixel area once.
Expected: selected offset changes; hex view scrolls to selected location.

12. Enable seeking in 2D mode, hover matrix plot pixels, click to freeze/unfreeze.
Expected: crosshair and counts update on hover; freeze toggles only from plot area.

13. Click matrix margin area (label margins).
Expected: no freeze toggle and no seek update from margin-only clicks.

14. Select an address in the seek list.
Expected: selected byte updates and hex view scroll target is applied once.

15. Switch to 3D mode and verify seek behavior.
Expected: 2D seek address pane is hidden; 3D interactions remain functional.

16. Test narrow window behavior by aggressively shrinking width.
Expected: no visual breakage; hex + addresses remain usable (stacked fallback when needed).

17. Rotate 3D plot (drag), then enable auto-rotate.
Expected: manual drag works when auto-rotate is off; auto-rotate animates smoothly when on.

18. Toggle 3D background modes (`Black`, `White`, `Custom`) and opacity slider.
Expected: visible contrast changes; point transparency changes immediately.

19. Return to 2D mode and verify normal seeking still works.
Expected: seek behavior recovers cleanly; no stale 3D-only state leakage.

## Pass/Fail Criteria

- Pass: all expected behaviors above are observed without crashes or severe UI glitches.
- Fail: any crash, stuck loading state, non-responsive interaction, or incorrect mode-coupling.
