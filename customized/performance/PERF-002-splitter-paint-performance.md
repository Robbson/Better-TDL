# PERF-002 — Splitter paint performance (attempted, reverted)

| Field | Value |
|---|---|
| Status | In progress — drag-time double buffering implemented; residual rich-edit reflow race |
| Upstream baseline | ToDoList 9.3.DP7 |
| Scope | Outer splitter, host resize/repaint path, tasklist container |
| Default | Drag-time only; no change outside a splitter drag |
| Related | [index.md](../index.md), [../PERFORMANCE.md](../../PERFORMANCE.md) |

## Goal

Reduce the splitter-drag artefacts and forced repaint cost described in
`PERFORMANCE.md` (P0-2 to P0-6) **without** stopping live resize, because live pane
movement is the user's manual check for optimisation progress.

## Upstream starting point

- `Core/3rdParty/SimpleSplitter.cpp`
  - `ResizePaneWindows()` erased the whole splitter with `Invalidate(TRUE)`.
  - `OnMouseMove()` forced a synchronous repaint with `UpdateWindow()`.
  - The window was created without `WS_CLIPCHILDREN` and had no
    `WM_ERASEBKGND` handler.
- `Core/ToDoList/ToDoCtrl.cpp` — `OnSize()` called `UpdateWindow()` after every
  resize; `Create()` used `WS_EX_CONTROLPARENT` only.
- `Core/ToDoList/TabbedToDoCtrl.cpp`
  - `ReposTaskCtrl()` called `UpdateWindow()` after moving the task view.
  - `OnSplitChange()` invalidated the whole tabbed control with bErase = TRUE in
    list view.

The splitter is a transparent overlay: it paints only its bars and relies on the
task view's pixels surviving underneath. Any erase of its client blanks that area.

## Experiments and results

All three iterations were built and manually tested; all were reverted.

### 1. Erase/clip cleanup plus deferred `UpdateWindow()` (P0-2 to P0-6)

Changes tried:

- `WM_ERASEBKGND` returns `TRUE`; `WS_CLIPCHILDREN` added.
- `ResizePaneWindows()` invalidated only bar rectangles with `bErase = FALSE`.
- Removed `UpdateWindow()` from `CSimpleSplitter::OnMouseMove`,
  `CToDoCtrl::OnSize` and `CTabbedToDoCtrl::ReposTaskCtrl`.
- `CTabbedToDoCtrl::OnSplitChange` used `Invalidate(FALSE)`.

Result: **worse artefacts**. During a live drag the task view did not repaint at
all. Each intermediate scrollbar position stayed on screen (about one stale copy
per mouse move) when dragging left to right, and a growing blank area appeared in
the other direction.

Cause: while the mouse button is down, `WM_MOUSEMOVE` floods the message queue and
`WM_PAINT` is starved. The splitter's `Invalidate(TRUE)` had been masking this by
erasing the whole overlay, which hid the stale task-view pixels under a background
fill.

### 2. Restore forced updates and force-update the active view

Changes tried:

- Restored all `UpdateWindow()` calls.
- In `ReposTaskCtrl`, additionally called `UpdateAll()` + `UpdateWindow()` on the
  active tree/list view and on the task view host.
- Kept the erase/clip cleanup.

Result: the stale copies were addressed, but the drag still showed intermediate
states. The user's main concern remained: no double buffering, so partial frames
are visible while the panes and task view repaint.

### 3. `WS_EX_COMPOSITED` on the tasklist container

Change tried:

- `CToDoCtrl::Create()` added `WS_EX_COMPOSITED` to compose the container and all
  descendants into complete frames.

Result: **unacceptable regression**. The whole application became much slower, the
attributes table in the task list vanished, and tree/attribute scrolling went
completely out of sync. Reverted immediately.

## Conclusion

- Deferring `UpdateWindow()` while keeping live resize is not viable; it starves
  repaints.
- `WS_EX_COMPOSITED` on the whole `CToDoCtrl` is not viable for this UI: it breaks
  the attribute pane and scroll synchronisation and is much slower.
- The remaining ideas (per-move `WM_SETREDRAW` batching across splitter and task
  view, or limiting composition to a smaller window) must be evaluated with the
  PERF-003 harness; see the retry protocol below.

## Retry protocol (using PERF-003)

The [PERF-003 harness](PERF-003-ui-perf-harness.md) now reproduces the splitter
artefact automatically. Baseline on the unmodified build (`-ui -steps 3/4/8`):

```text
SplitterDrag[0]: maxDiff≈60k cleanDiff=0 artifactSteps=3/3 moved=1
SplitterDrag[1]: maxDiff≈78k cleanDiff=0 artifactSteps=3/3 moved=1
```

`cleanDiff=0` means two forced repaints are identical, so the diff is genuine stale
pixels. The saved `drag_NN_onscreen.bmp` / `drag_NN_clean.bmp` frames show the task
view's right edge/Tags column still at the previous splitter width.

Any retry must be evaluated in isolation and one change at a time:

1. Command: `Core\TDLTest\Unicode_Debug\TDLTest.exe -ui -steps 8`.
2. Acceptance: `moved=1`, `cleanDiff=0`, `artifactSteps=0`, with `maxDiff` at or
   below the `-tol` threshold.
3. Record `elapsedMs` and `gdi` against the baseline; do not accept an artefact
   win that makes the drag materially slower.
4. Keep the captured BMPs when a step fails; they identify the exact stale region.

Do not retry `WS_EX_COMPOSITED` or unconditional `UpdateWindow()` removal. The
first candidate should target the observed stale strip directly (the task view is
resized but its right edge/columns are not painted before the next mouse move),
for example a per-move batched redraw limited to the splitter, active task view and
its children.

## 2026-11 retry: real layout and the latency split

The harness was extended so the worst-case layout can be reproduced directly,
instead of a synthetic fixture:

- `-legacyattr` uses the legacy (non-grid) attribute editor with a deterministic
  stacked layout (attributes below the task view, comments below the attributes)
  via the isolated `Perf.ini`.
- `-inifile <path>` starts from a real user configuration (layout, columns,
  theming, window state) and only overrides the isolation keys
  (`MultiInstance`, update/source-control checks). With `-inifile` the harness no
  longer resizes the frame, so the drag runs on the user's real DPI-scaled
  layout.
- The split bar is now located from the live pane rectangles (the gap between the
  task view and the neighbouring pane), not from a point outside the task view.
  It supports the **vertical** bar (task list beside comments/attributes), which
  is the bar that shows the artefacts in the legacy layout. A `-uiprobe` dump
  prints every child window, its rect and the window under the bar point.
- Mouse input uses `SetCursorPos` for exact positioning (SendInput's absolute
  scaling drifted a few pixels off the few-pixel-wide bar) and `mouse_event` for
  the button state.
- `-measureevery <n>` captures only every Nth step so a full drag runs fast
  (about 3 s for 40 steps in both directions) while still sampling mid-drag.

Reproduction (debug build, real `ToDoList.ini`, `Examples\biglist.tdl`,
legacy editor, 40 steps, no artificial inter-step delay):

```bat
Core\TDLTest\Unicode_Debug\TDLTest.exe -ui -uisplitter ^
  -inifile "Core\ToDoList\Unicode_Debug\ToDoList.ini" ^
  -tdl "Core\ToDoList\Unicode_Debug\Resources\Examples\biglist.tdl" ^
  -steps 40 -notches 120 -delay 0 -measureevery 5
```

Measured (vertical bar, dragging right then back):

| Settle before capture | maxDiff | artifactSteps | cleanDiff |
|---|---|---|---|
| 50 ms | ~62k / ~77k | 8 / 8 | 0 |
| 150 ms | ~4.7k / ~5.8k | 3 / 2 | 0 |

`cleanDiff=0` in all cases, so the forced-repaint reference is stable and the diff
is genuinely stale on-screen pixels.

### What the split tells us

- The bulk of the artefact (about 90%) disappears when the app is given ~150 ms to
  repaint before the capture. That is **repaint latency under a fast drag**: during
  a drag the queued `WM_MOUSEMOVE` messages starve `WM_PAINT`, so the panes and
  their scrollbars are still painting when the user would see the next frame. A
  larger settle is not a fix; the real fix is not showing partial frames, i.e.
  double buffering of the splitter plus the pane strip it exposes.
- A small residual (~5k pixels, 2-3 samples) remains even after 150 ms, so there is
  also a minor genuine paint omission. The failing frames place it at pane edges
  (the list-view right scrollbar and the comments pane's left edge), i.e. child
  non-client areas repositioned by the resize.

### Attempts that did not work (all reverted)

- Recomputing dirty rects inside `CSimpleSplitter::ResizePaneWindows` and
  force-repainting the panes: no measurable change. The pane sizes are updated
  before `ResizePaneWindows` runs, so the "old" rect had to be cached from the
  previous resize; even with the cached old geometry and
  `RedrawWindow(pane, RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_FRAME)` the
  diff was unchanged.
- Flushing the enclosing `#32770` tasklist dialog `ALLCHILDREN|FRAME` from within
  `WM_MOUSEMOVE`: made it **worse** and destabilised the oracle (`cleanDiff` up to
  ~30k). Synchronous whole-container repaint inside the mouse-move handler
  re-enters painting while input is still draining.

### Recommended next candidate

Add real double buffering for the drag path: render the splitter and the exposed
pane strip into one offscreen surface and blit once per mouse move, so the user
never sees the intermediate panes/scrollbars. This is a larger change than the
invalidation tweaks above and should be gated behind the harness pass above
(`artifactSteps=0` with `-delay 0`), not a longer settle.

## Implemented: drag-time double buffering

`Core/3rdParty/SimpleSplitter.cpp` — `CSimpleSplitter::DoubleBufferPanes()`, called
from `ResizePaneWindows()` only while the splitter holds mouse capture (i.e. during
a drag):

- For each visible pane that owns a task view (`SysTreeView32`/`SysListView32`) or a
  comments rich edit (`RICHEDIT50W`), render the pane and all its children into an
  offscreen bitmap with `WM_PRINT` (`PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND|
  PRF_NONCLIENT`) and blit it to the pane's window DC in a single `BitBlt`.
- The bitmap is sized to the pane's **window** rect and blitted with
  `GetWindowDC`, so the pane's 1px non-client border is covered; a client-only
  blit left a stale 1px line against the split bar.
- The attributes frame is deliberately **not** buffered: it contains many child
  controls and `WM_PRINT` of that subtree is not faithful (it produced more
  difference than it removed). It stays on the normal paint path, which the
  harness confirms is not the source of the artefacts.
- Outside a drag the normal path is unchanged, so there is no cost or behaviour
  change for resizing from layout code, maximising, etc.

This is the "double buffering" the earlier attempts lacked: each mouse move now
shows one complete composed frame instead of several partially repainted ones.

### Measured result

Real debug config + `Examples\biglist.tdl`, legacy editor, vertical bar, 40 steps,
`-delay 0 -measureevery 5`:

| Build | maxDiff | artifactSteps | cleanDiff | elapsed |
|---|---|---|---|---|
| before (baseline) | ~62k / ~77k | 8 / 8 | 0 | ~2.9 s |
| drag-time double buffer | 0 / 0 (frequently) | 0 / 0 | 0 | ~3.3 s |

The default synthetic fixture with the grid editor now passes fully (`0 / 0`,
`artifactSteps=0`). `UIPerf::ListScrollSync` is unaffected (`syncErrors=0`,
`maxDiff=0`), confirming the attributes pane and list scroll sync are intact.

### Residual / risks

- A small intermittent residual (~3k–5k pixels, 1–4 samples) can still appear in
  the **comments** pane when its rich edit has not finished reflowing text to the
  new width before the frame is blitted. It is a rich-edit layout race, not a
  splitter invalidation bug; forcing a `RedrawWindow` before the print destabilised
  the oracle and was reverted.
- `WM_PRINT` of the task view runs once per mouse move; `elapsedMs` is essentially
  unchanged (~3 s for 40 steps), so the extra offscreen render is not a material
  drag slowdown. The bitmaps are created and destroyed per move; if that shows up
  as GDI churn, cache one bitmap per pane keyed by size.

## Test notes

- Each iteration compiled successfully with
  `msbuild Core\ToDoList\ToDoList.vcxproj /p:Configuration="Unicode Debug" /p:Platform=Win32`.
- `TDLTest` post-build still reports the pre-existing `CTDCTaskMatcherTest`
  date/locale failures; unrelated.
- No code from this experiment remains in the working tree.
