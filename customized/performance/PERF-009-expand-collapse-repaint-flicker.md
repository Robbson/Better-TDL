# PERF-009: Flicker when expanding or collapsing a task

Status: **In progress** (implemented and built; `TDLTest` passes; interactive check pending)

## Goal

Expanding a parent in the Task Tree showed a sequence of partial frames: the
newly exposed area appeared empty, then the row background/grid was filled, then
the text followed. The longer the child list, the more visible the effect was.
Collapsing showed the same problem in reverse.

This is a repaint-ordering issue, not the scrolling artefact from
[PERF-001](PERF-001-list-view-stale-title.md).

## Upstream starting point

The Task Tree is a synced `SysTreeView32` (title pane) plus a separate
attribute `CListCtrl` (`m_lcColumns`), kept in step by `CTreeListSyncer`
(see [../docs/ARCHITECTURE.md](../../docs/ARCHITECTURE.md) §4.2.1). A
user-driven expand is handled in `CTreeListSyncer::HandleItemExpanded`
(`Core/Shared/TreeListSyncer.cpp:2060`):

1. `RefreshSize()` ran while both panes were still redraw-enabled.
2. The tree was explicitly invalidated and forced to paint with
   `::InvalidateRect(hwndTree, ...)` + `::UpdateWindow(hwndTree)`, with the
   comment "make sure the tree is redrawn first before resyncing the list".
3. Only then was the parent redraw hold taken (`CHoldRedraw hr(GetHwnd())`) and
   the attribute list populated with the new rows.

`CTreeListSyncer::WindowProc` forwards `WM_SETREDRAW` to both panes while
resync is enabled (`Core/Shared/TreeListSyncer.cpp:1571`), so the later hold did
cover the list. But the tree had already been painted, and `RefreshSize()` also
ran before the hold. The newly expanded title rows therefore reached the screen
against an attribute pane that did not yet contain the new rows, and the
attribute rows were inserted/painted separately. That is the visible
empty → background/grid → text sequence.

The same split applied to collapse: the tree was repainted first and then the
attribute rows were deleted.

## What changed

Files:

- `Core/Shared/TreeListSyncer.cpp` — `WindowProc` (`TVN_ITEMEXPANDING`) and
  `HandleItemExpanded`.
- `Core/Shared/TreeListSyncer.h` — `m_pExpandHold` member.

The whole operation is now one redraw batch:

- `TVN_ITEMEXPANDING` takes a redraw hold as soon as the expansion starts
  (stored in `CTreeListSyncer::m_pExpandHold`). `WM_SETREDRAW FALSE` is
  forwarded to both panes while resync is still enabled, so the tree cannot
  paint its new rows during the native expansion.
- `HandleItemExpanded` keeps that hold for the duration of the attribute-list
  update. `EnableResync(FALSE)` and `RefreshSize()` run inside it, so no
  intermediate `UpdateWindow`/`UpdateAll` can flush a half-updated frame.
- The attribute list is expanded or collapsed inside the same hold.
- `EnableResync(TRUE, hwndTree)` still runs before the hold is released; the
  hold is then deleted, and both panes are explicitly invalidated so they are
  repainted together with the final layout.

The forced tree repaint was removed; the title and attribute panes now paint
from the same final model state.

Programmatic expansion (`CTDLTaskTreeCtrl::ExpandItem`,
`ExpandTasks`) already took its own `CHoldRedraw`/`CTLSHoldResync` before
changing the tree, so it is unaffected.

## Behaviour

- Expanding or collapsing from the mouse or keyboard now leaves the previous
  frame on screen until both panes have been updated, instead of showing the
  tree's new rows before the attribute rows exist.
- The final layout, scroll positions, selection and column widths are
  unchanged: `EnableResync(TRUE, hwndTree)` still posts the resync that fixes
  scroll/selection state after the batch.
- `CTreeListSyncer` is shared, so other tree/list consumers (Gantt, Workload,
  Calendar) get the same batched repaint. Their data flows are unchanged.

## Verification

- Unicode Debug solution builds; `TDLTest` reports **310436/310436 tests
  pass**.
- Interactive check still required: expand and collapse parents with long child
  lists in the Task Tree (mouse and keyboard) and confirm the empty-space and
  background-before-text phases are gone. Also check a panel that does not use
  the Task Tree (for example Gantt) for regressions.
- [PERF-003](PERF-003-ui-perf-harness.md) has no expand scenario yet; adding
  one would make this repeatable in the same way as the splitter/scroll tests.

## Risks and follow-ups

- The tree is now repainted on release rather than synchronously before the
  list update. If a future change makes the list population slow for very large
  child sets, the user will see the old (collapsed) frame for longer instead of
  partial new rows. That is preferred to flicker but should be measured with a
  large fixture.
- `RefreshSize()` now runs with resync disabled; the resync posted by
  `EnableResync(TRUE, ...)` performs the scroll/selection fix-up. If a future
  change makes `RefreshSize()` depend on an immediate resync, that ordering must
  be revisited.
- Do not reintroduce an `UpdateWindow` of either pane between the hold and the
  list update; that recreates the split frame this change removes.
