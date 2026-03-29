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

4. Click ribbon pixel area once.
Expected: selected offset changes; hex view scrolls to selected location.

5. Enable seeking in 2D mode, hover matrix plot pixels, click to freeze/unfreeze.
Expected: crosshair and counts update on hover; freeze toggles only from plot area.

6. Click matrix margin area (label margins).
Expected: no freeze toggle and no seek update from margin-only clicks.

7. Select an address in the seek list.
Expected: selected byte updates and hex view scroll target is applied once.

8. Switch to 3D mode and verify seek behavior.
Expected: 2D seek address pane is hidden; 3D interactions remain functional.

9. Test narrow window behavior by aggressively shrinking width.
Expected: no visual breakage; hex + addresses remain usable (stacked fallback when needed).

10. Rotate 3D plot (drag), then enable auto-rotate.
Expected: manual drag works when auto-rotate is off; auto-rotate animates smoothly when on.

11. Toggle 3D background modes (`Black`, `White`, `Custom`) and opacity slider.
Expected: visible contrast changes; point transparency changes immediately.

12. Return to 2D mode and verify normal seeking still works.
Expected: seek behavior recovers cleanly; no stale 3D-only state leakage.

## Pass/Fail Criteria

- Pass: all expected behaviors above are observed without crashes or severe UI glitches.
- Fail: any crash, stuck loading state, non-responsive interaction, or incorrect mode-coupling.
