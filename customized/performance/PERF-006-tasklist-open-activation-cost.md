# PERF-006: Fixed per-tasklist cost when opening or activating a document

Status: **Proposed** (runtime analysis complete; instrumentation and fix not started)

## Goal

Loading a document in an already running ToDoList takes roughly 2-3 s, and an 8 KB
tasklist costs the same as a 1.7 MB tasklist. This note records the measured breakdown,
the code paths responsible, and the next steps. It is the per-document counterpart to
the startup work in [PERF-004](PERF-004-delayed-tasklist-window.md) and
[PERF-005](PERF-005-no-throwaway-tasklist.md); `PERFORMANCE.md` P0-5 covers the
multi-document startup floor, not opening a single document while the app is running.

No source changes were made for this analysis.

## Bottom line

The cost is **fixed UI work per tasklist tab**, not file I/O:

- reading and DOM-parsing a 1.7 MB file costs ~60 ms in Release;
- creating the tasklist window costs ~0.8-1.0 s;
- applying runtime preferences costs ~0.9 s;
- loading even a tiny tasklist into the UI costs ~0.8-1.3 s, dominated by restoring
  the saved tree/sort/column state;
- showing and painting the new tab costs another ~0.9 s plus app bookkeeping.

The UI thread is serial; additional cores do not help. The fix has to remove repeated
layout/paint and redundant control updates, not optimise parsing or CPU throughput.

## Method

Fixture: the existing `Core\ToDoList\Unicode_Debug\ToDoList.ini` session (8 sample
documents, `EnableDelayedLoading=1`, active document `Modern Dancing.tdl`). The Release
run used a copy of the Release ini with the same kind of session (9 documents including
`biglist.tdl`). Relative paths were resolved with the build folder as working directory.

The delayed tabs were activated with the Window menu command equivalent:
`SendMessageTimeout(hwnd, WM_COMMAND, ID_WINDOWn, ..., 60 s)`. This blocks until MFC has
finished the handler, including selection, showing and painting, so the wall time is what
the user perceives when picking the document from the Window menu. The same path runs
when a delay-loaded tab is first selected by any other means.

Logging was enabled with `-g`; the existing `CScopedLogTimer`s in `CreateToDoCtrl`,
`CToDoCtrl::Load`, `LoadTasks`, `AddToDoCtrl` and `OpenTaskList` were read from
`ToDoList.log`.

Caveat: every log line is an open/append/close of the log file, so the absolute wall
times are inflated by roughly 0.5-0.9 s for one activation (~15 log lines). Relative
comparisons and the sub-timers are still valid. The measurements are single runs per
scenario; treat them as ±10%.

Hardware was the user's Zen 5 machine. Builds were `Unicode Release` and `Unicode Debug`
(v143), Win32.

## Measurements

### Release

| File | Size | Wall (`-g`) | `CreateToDoCtrl` | `LoadTasks` | of which Restore state | Read + DOM | Untimed remainder |
|---|---:|---:|---:|---:|---:|---:|---:|
| `Evidence Board.tdl` | 7.7 KB | 3.9 s | 828 ms | 1156 ms | 422 ms | 47 ms | ~1.86 s |
| `London Underground.tdl` | 10.8 KB | 3.7 s | 1000 ms | 829 ms | 328 ms | 46 ms | ~1.85 s |
| `biglist.tdl` | 1.7 MB | 4.2 s | 1047 ms | 1250 ms | 672 ms | **62 ms** | ~1.80 s |

"Read + DOM" is `CToDoCtrl::Load(before LoadTasks`; it includes `CTaskFile::Load` and the
whole-file XML parse. The untimed remainder is `Wall - CreateToDoCtrl - LoadTasks`; it
contains `UpdateToDoCtrlPreferences`, showing/painting the tab and the post-load
bookkeeping in `SelectToDoCtrl`.

In the same Release process the startup document reported
`CreateToDoCtrl(Prefs) took 875 ms`. Delay-loaded tabs skip preferences during creation
(`CreateToDoCtrl(Prefs) took 31 ms`) and apply the same helper later, so ~875 ms of the
untimed remainder is the preference application. That leaves ~0.9 s for show, paint and
bookkeeping.

The startup active document measured `OpenTaskList() took 3125 ms`
(CreateToDoCtrl 1532 including Prefs 875, Load 968, and a 500 ms post block that
includes AddToDoCtrl 468). A delay-loaded activation does not pay `AddToDoCtrl` for a
new tab, which is why its total is lower.

### Debug

| File | Size | Wall (`-g`) | `CreateToDoCtrl` | `LoadTasks` | of which Restore state | Read + DOM | Untimed remainder |
|---|---:|---:|---:|---:|---:|---:|---:|
| `Evidence Board.tdl` | 8.7 KB | 5.0 s | 969 ms | 1953 ms | 907 ms | 46 ms | ~2.06 s |
| `biglist.tdl` | 1.7 MB | 5.2 s | 891 ms | 1703 ms | 969 ms | 156 ms | ~2.61 s |
| `Dancing.tdl` | 494 KB | 5.0 s | 1062 ms | 1922 ms | 1266 ms | 47 ms | ~1.97 s |

Debug roughly doubles the UI cost and remains file-size independent. Any fix should be
measured in Release; Debug is useful only for assertions and stack traces.

## Code path

For a delay-loaded tab, activation is `SelectToDoCtrl` ->
`VerifyTaskListOpen` -> `EnsureToDoCtrlWindow` -> `OpenTaskList(&tdc, ...)`:

- `Core/ToDoList/ToDoListWnd.cpp:9438-9449` - `VerifyTaskListOpen` creates the window and
  then loads.
- `Core/ToDoList/ToDoListWnd.cpp:8708-8726` - `EnsureToDoCtrlWindow` calls
  `CreateToDoCtrl(pTDC, TRUE, TRUE)` and sizes the control.
- `Core/ToDoList/ToDoListWnd.cpp:8623-8706` - `CreateToDoCtrl` calls
  `CToDoCtrl::Create`, sets font/styles/theme, and skips preferences when
  `bSkipPrefs=TRUE`.
- `Core/ToDoList/ToDoCtrl.cpp:543-606` - `CToDoCtrl::Create`/`OnInitDialog` constructs
  the tree/list view, comments control, attribute editor, splitters, tab view and
  timers, then calls `UpdateControls`.
- `Core/ToDoList/ToDoListWnd.cpp:5063-5072` - `OpenTaskList` applies the skipped runtime
  preferences and then calls `pTDC->Load`.
- `Core/ToDoList/TDCToDoCtrlPreferenceHelper.cpp:70-168` - preference application:
  styles, column/edit visibility, layout, attribute-editor mode, auto-list data, colours,
  calculation settings and `NotifyEndPreferencesUpdate`.
- `Core/ToDoList/ToDoListWnd.cpp:9533-9561` - `SelectToDoCtrl` moves and shows the new
  control, then calls `Invalidate(TRUE)` and `UpdateWindow()`.
- `Core/ToDoList/ToDoCtrl.cpp:715-722` - `CToDoCtrl::OnSize` calls `UpdateWindow()` on
  every size change, so layout can force synchronous full repaints.
- `Core/ToDoList/ToDoCtrl.cpp:4558-4627` - `CToDoCtrl::Load` reads/parses the file and
  calls `LoadTasks`.
- `Core/ToDoList/ToDoCtrl.cpp:4664-4828` - `LoadTasks` loads globals, custom attributes,
  split positions, attribute visibility and the task tree; `Restore state` is
  `LoadTasksState`.
- `Core/ToDoList/ToDoCtrl.cpp:8724-8733` - `LoadTasksState` restores the task tree state
  and calls `RecalcUntrackedColumnWidths` when not rebuilding.
- `Core/ToDoList/TDLTaskTreeCtrl.cpp:2230-2243` - tree `LoadState` expands saved items
  and restores the selected item.
- `Core/ToDoList/TDLTaskCtrlBase.cpp:1443-1474` - column order/width/tracked state,
  split position, sort state and `PostResize`.
- `Core/ToDoList/TDLTaskCtrlBase.cpp:1318-1418` - `RecalcUntrackedColumnWidths` scans the
  visible tasks and measures text for untracked columns.

## Hypotheses for the fixed cost

In descending expected value. These need measurement, not more code reading.

1. **Repeated synchronous layout/paint during a preference update.** `ModifyStyles`
   ends with `Invalidate(TRUE)`, `UpdateControls()` and a conditional `Resize()`
   (`Core/ToDoList/ToDoCtrl.cpp:4040-4071`). `SetAttributeEditorMode` calls
   `UpdateControls(TRUE)` and `Resize()` (`ToDoCtrl.cpp:735-751`).
   `SetLayoutPositions` resizes again (`ToDoCtrl.cpp:724-733`), and
   `NotifyEndPreferencesUpdate` invalidates the whole control and updates the comments
   control (`ToDoCtrl.cpp:4134-4152`). On a fresh, hidden control this can mean several
   full layouts before the tab is ever shown.
2. **Comments/font work.** `SetCommentsFont` re-reads and re-sends the content to the
   content control (`ToDoCtrl.cpp:645-667`). With the RTF content plugin this is not
   free even for an empty tasklist.
3. **Attribute editor (re)creation and update.** `SetAttributeEditorMode` can replace
   the editor and then repopulate auto-list data, completion status and default values
   (`ToDoCtrl.cpp:735-751`).
4. **Tree-state restore.** `ExpandTasks` plus `RecalcUntrackedColumnWidths` in
   `LoadTasksState` (`ToDoCtrl.cpp:8724-8733`); the latter scans every visible task and
   measures text (`TDLTaskCtrlBase.cpp:1318-1418`). It is called during load and may be
   repeated by idle processing.
5. **Show/paint and bookkeeping.** `SelectToDoCtrl` forces `Invalidate(TRUE)` +
   `UpdateWindow()` (`ToDoListWnd.cpp:9556-9561`), `CToDoCtrl::OnSize` forces another
   update, and the selection path refreshes filter bar, status bar, auto-list data and
   due-task notification (`ToDoListWnd.cpp:9563-9578`).
6. **Preference getter cost.** `UpdateToDoCtrlPrefs` performs ~100 `prefs.Get*` calls
   and sets ~100 style/colour/config values. If any getter or setter does repeated
   registry/ini or control work, the cost is linear in preferences, not tasks.

Findings that are explicitly *not* the cause: file size, XML parsing, disk read,
plugin scans (`CContentMgr::Initialize`/`CUIExtensionMgr::Initialize` are once per
process), and the number of tasks.

## Next steps

### 1. Add targeted timers (next action)

Follow the existing "PERMANENT LOGGING" convention and add `CScopedLogTimer`s around:

- `OpenTaskList(&tdc)`: preferences, `Load`, post-load bookkeeping
  (`Core/ToDoList/ToDoListWnd.cpp:5063-5131`).
- `CTDCToDoCtrlPreferenceHelper::UpdateToDoCtrlPrefs`:
  `NotifyBeginPreferencesUpdate`, `ModifyStyles`, column visibility, layout,
  attribute-editor mode, auto-list data, colours, `NotifyEndPreferencesUpdate`
  (`Core/ToDoList/TDCToDoCtrlPreferenceHelper.cpp:70-168`).
- The font block in `CTDCToDoCtrlPreferenceHelper::UpdateToDoCtrl`
  (`TDCToDoCtrlPreferenceHelper.cpp:27-68`), especially `SetCommentsFont`.
- `SelectToDoCtrl`: move, show, `Invalidate+UpdateWindow`, and the idle/bookkeeping
  block (`Core/ToDoList/ToDoListWnd.cpp:9533-9578`).
- `CToDoCtrl::OnInitDialog`: each child-control creation and `UpdateControls`
  (`Core/ToDoList/ToDoCtrl.cpp:564-606`).
- `LoadTasksState`, `ExpandTasks`, `RecalcUntrackedColumnWidths`
  (`ToDoCtrl.cpp:8724-8733`, `TDLTaskCtrlBase.cpp:1318-1418`).

To avoid the `-g` per-line file overhead while measuring, prefer an accumulated QPC
report printed once at the end of the activation, or `OutputDebugString`, instead of
adding a log line per block.

### 2. Re-run the Window-menu measurement

Use the Release build and the same 8-document session; activate one small and one large
delayed tab. Acceptance for this step is a complete attribution with no untimed gap
larger than ~100 ms. Keep the raw log with the result.

### 3. Use a sampler only for the remaining opaque block

If preferences or activation remains fragmented after the timers, take one CPU sample of
a single activation. The required tools are already installed; no Enterprise edition is
needed:

- `C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\wpr.exe` and
  `wpa.exe`;
- VS 2022 Community Performance Profiler/DiagnosticsHub;
- `Core\ToDoList\Unicode_Release\ToDoList.pdb`.

WPR/WPA recipe:

```bat
set _NT_SYMBOL_PATH=srv*C:\symbols*https://msdl.microsoft.com/download/symbols
wpr.exe -start CPU -filemode
:: activate one delayed tab via the Window menu
wpr.exe -stop %TEMP%\tdl-load.etl
```

Open the trace in WPA, load the local PDB, select `CPU Usage (Sampled)`, filter to
`ToDoList.exe`, and inspect inclusive/exclusive time on the UI thread. Add
`-start FileIO -start DiskIO -start Registry` if the UI thread is waiting rather than
running. The same data answers whether the UI thread is CPU-bound or blocked on I/O.

### 4. Structural fixes to evaluate

Do not start with micro-optimisation. The likely win is batching and removing redundant
layout/paint:

- Batch preference application: defer `Resize`, `Invalidate`, `UpdateControls` and
  idle-column recalculation until `NotifyEndPreferencesUpdate`, then apply once.
- Apply preferences before the control has a visible window where ordering permits (the
  current delayed path already creates the window before applying them).
- Avoid `UpdateWindow()` inside `CToDoCtrl::OnSize` for hidden or not-yet-shown controls;
  let the message loop coalesce paints.
- Collapse the multiple layouts in `CreateToDoCtrl`/`EnsureToDoCtrlWindow`/
  `SelectToDoCtrl` into one final size.
- Defer `LoadTasksState`/`ExpandTasks`/column-width recalculation until after the tree is
  visible and the first paint is complete.
- Check whether `SetCommentsFont` and `SetAttributeEditorMode` need to run when the
  control is new and empty, or whether their effect can be applied once at creation.
- Add a cheap guard to `ModifyStyles`/preference updates to skip no-op updates and avoid
  repeated full invalidation.
- Cache formatted column widths/text if `RecalcUntrackedColumnWidths` proves hot (see
  `PERFORMANCE.md` P1-3).

Acceptance target: activating or opening an 8 KB tasklist in Release should be under
~1 s total, with no more than one layout and one full paint pass per activation, while
opening a 1.7 MB tasklist should differ only by the actual parse/build time.

## Risks and caveats

- The wall times include `-g` logging overhead and the full activation/paint path, not
  just file loading. Without logging, the observed 3.7-4.2 s Release figures are
  consistent with the reported 2-3 s.
- The preference cost depends on the ini: fonts, theme, info tips, UI extensions,
  visibility and calculation settings all change `UpdateToDoCtrlPrefs`.
- `CScopedLogTimer` still formats its scope string when logging is disabled
  (`Core/Shared/ScopedTimer.cpp:94-115`); do not add permanent timers to per-row or
  per-cell paths for this work.
- Debug exaggeration is large and misleading; all acceptance measurements must be
  Release.
- Do not regress the PERF-004 invariant: delay-loaded tabs intentionally have no window
  until first activation. Any fix must keep `GetSafeHwnd()`/`IsLoaded()` guards valid.
- Do not change parsing, file format or plugin contracts for this work.

## Related documents

- [PERFORMANCE.md](../../PERFORMANCE.md) - overall performance analysis; P0-5 startup and
  remaining startup work.
- [PERF-004-delayed-tasklist-window.md](PERF-004-delayed-tasklist-window.md) - the
  delay-loaded object/window split that this note builds on.
- [PERF-005-no-throwaway-tasklist.md](PERF-005-no-throwaway-tasklist.md) - removed the
  throwaway startup tasklist.
- [PERF-003-ui-perf-harness.md](PERF-003-ui-perf-harness.md) - existing interactive UI
  harness; not used here because it covers splitter/scroll, not document activation.
