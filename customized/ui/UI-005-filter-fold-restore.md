# UI-005: Restore tree folding when a filter is cleared

Status: **In progress** (implemented and compiled; manual filter test pending)

## Goal

Keep the user's tree expansion state across filter activation. Today, applying a filter
rebuilds the tree from the filtered model, and clearing the filter rebuilds again from the
filtered tree. Parents that were absent while filtered have no saved expansion, so they
come back collapsed.

## Upstream starting point

- `CToDoCtrl::RebuildTree()` preserves expansion by capturing the tree being replaced and
  restoring it after the rebuild (`Core/ToDoList/ToDoCtrl.cpp:7568`, `:7595`).
- `CFilteredToDoCtrl::RefreshTreeFilter()` calls `RebuildTree()` for every filter change
  (`Core/ToDoList/FilteredToDoCtrl.cpp:567`).
- `CFilteredToDoCtrl::SetFilter/ClearFilter/ToggleFilter` change `m_filter` and then call
  `RefreshFilter()` (`Core/ToDoList/FilteredToDoCtrl.cpp:390`).
- Tree expansion is available as task IDs via
  `CTDLTaskTreeCtrl::GetExpandedTasks()` / `ExpandTasks()` (`Core/ToDoList/TDLTaskTreeCtrl.cpp:236`,
  `:267`).

## What changed

Files: `Core/ToDoList/FilteredToDoCtrl.h/.cpp`, `Core/ToDoList/ToDoCtrl.h/.cpp`.

1. `CFilteredToDoCtrl` keeps `m_aUnfilteredExpanded`, `m_bHaveUnfilteredExpanded` and
   `m_bRestoreUnfilteredExpanded`.
2. When a filter is activated (none → filter), `CaptureUnfilteredExpansion()` snapshots
   the expanded task IDs from the current, unfiltered tree.
3. When the filter is toggled off or cleared, the next rebuild uses the snapshot instead
   of the filtered tree's expansion.
4. `CToDoCtrl::RebuildTree()` was split into `RebuildTree()` and
   `RebuildTreeWithExpansion()`, both delegating to `RebuildTreeInternal()`. The internal
   version accepts an optional expansion array. A fresh tree is built collapsed, so
   expanding the snapshot restores the exact pre-filter layout without a second rebuild or
   collapse pass.
5. Filter-to-filter changes keep the original snapshot; the snapshot is overwritten the
   next time a filter is activated from an unfiltered tree.

## Behaviour

- Applying a filter does not destroy the unfiltered folding state.
- Toggling the filter off (`ToggleFilter`) or clearing it restores the expansion that was
  active before the filter was applied.
- Internal transient toggles (select task link, select history, archive) also restore the
  unfiltered layout before re-applying the filter.

## Risks and limits

- The snapshot is in-session only. If a tasklist is closed and reopened with a filter
  active, the unfiltered snapshot is not persisted, so the saved expansion may be the
  filtered one.
- If a filter is restored from preferences at startup and no unfiltered tree has existed
  yet, there is no snapshot; clearing that filter falls back to the previous behaviour.
- Delayed-load tasklists that set a filter before the tree window exists cannot capture a
  snapshot.

## Tests

- Core `Unicode Debug` build (ToDoListLib / TDLTest).
- Manual check: expand/collapse a mix of parents, apply a filter, clear or toggle it off,
  confirm the original layout returns.
