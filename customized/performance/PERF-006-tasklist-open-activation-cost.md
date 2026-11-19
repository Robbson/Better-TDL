# PERF-006: Fixed per-tasklist cost when opening or activating a document

Status: **Measured** (steps 1-2 complete: buffered instrumentation added and a
Release Window-menu measurement attributed; structural fix not started)

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

> This first measurement was superseded by the buffered instrumentation in
> [Step 1-2 results](#step-1-2-results-instrumentation-and-release-attribution), which
> removes the per-line log overhead and attributes every phase. Keep this section as
> the record of the original observation.

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

## Step 1-2 results (instrumentation and Release attribution)

### Instrumentation

The existing `CScopedLogTimer`s write one line per scope and each write reopens the
log file, which was the main reason the earlier `-g` numbers were inflated by roughly
0.5-0.9 s per activation. To measure the cost without perturbing it, a buffered
`CTDCPerfTrace` was added in `Core/ToDoList/TDCPerfTrace.h`:

- no-op unless `FileMisc::IsLoggingEnabled()`;
- uses `QueryPerformanceCounter`;
- collects every scope in memory and writes the whole activation as a single log
  append when the outermost scope ends (one file open per activation, not per line).

It is a drop-in for the `CScopedLogTimer` subset used on this path
(`LogStart`/`LogTimeElapsed`). Instrumented scopes:

- `CToDoListWnd::VerifyTaskListOpen`
- `CToDoListWnd::OpenTaskList(CFilteredToDoCtrl*, ...)`
- `CToDoListWnd::SelectToDoCtrl(int, ...)` including `Show`, `Invalidate/UpdateWindow`
  and the post-selection bookkeeping
- `CToDoListWnd::CreateToDoCtrl`, `EnsureToDoCtrlWindow`,
  `UpdateToDoCtrlPreferences`, `RefreshFilterBarControls`
- `CTDCToDoCtrlPreferenceHelper::UpdateToDoCtrl` (fonts) and
  `UpdateToDoCtrlPrefs` (per-phase checkpoints)
- `CToDoCtrl::Create`, `OnInitDialog`, `SetCommentsFont`, `ModifyStyles`,
  `SetLayoutPositions`, `SetAttributeEditorMode`, `NotifyEndPreferencesUpdate`,
  `Load`, `LoadTasks`, `LoadSplitPos`, `ExpandTasks`
- `CTabbedToDoCtrl::LoadTasksState` (list view and tree)
- `CTDLTaskCtrlBase::LoadState`, `RecalcUntrackedColumnWidths`
- `CTDLTaskTreeCtrl::ExpandTasks`

The trace is permanent (same convention as the existing `PERMANENT LOGGING` blocks) and
costs one `IsLoggingEnabled()` check per scope when logging is off.

### Measurement method

- Release `Win32`, `-g`, isolated temp copy of the Release ini.
- `[Settings]` reduced to three documents: `biglist.tdl` (1.66 MB),
  `Evidence Board.tdl` (7.6 KB) and `Dancing.tdl` (461 KB, active). Delay-loading
  enabled, so the first two are only loaded on first activation.
- Activation driven by `PostMessage(WM_COMMAND, ID_WINDOWn)` from a helper process;
  the ini is regenerated before every run because ToDoList rewrites the session on
  close.
- Wall time stops when the activation's root trace line appears in the log
  (`CToDoListWnd::SelectToDoCtrl(n) took ...`), i.e. when the handler has returned
  and the buffered trace has flushed.
- The window is explicitly brought to the foreground before measuring. Foreground
  state changes the result by ~30%; see the focus-state control below.

The first runs after a rebuild appeared slower, but the dominant variable turned out to be
whether the test window was **foreground**. A controlled pair on the same warm binary and
fresh session gave:

| Mode | Evidence Board (wall / app CPU) | biglist (wall / app CPU) |
|---|---:|---:|
| Background (behind VS Code) | 3739 / 2797 ms | 3795 / 3125 ms |
| Foreground | 2503 / 1938 ms | 2571 / 2109 ms |

Foreground is ~30% faster in both wall and app CPU time. Follow-up controls:

- **Visible but unfocused** (window raised topmost without activation): 3697 / 4576 ms -
  still slow. So it is not occlusion or DWM visibility.
- **Background at High process priority**: 3490 / 3635 ms - still slow. So it is not the
  process priority class.
- `GetProcessInformation(ProcessPowerThrottling)` reports `throttled=False` in all modes,
  so it is not EcoQoS either.

Representative scopes (Evidence Board / biglist) shrink roughly uniformly:

| Scope | Background | Foreground |
|---|---:|---:|
| `OnInitDialog(Attributes)` | 305 / 264 | 240 / 226 |
| `SetAttributeEditorMode(SetEditorMode)` | 483 / 485 | 342 / 317 |
| `SetAttributeEditorMode(UpdateControls)` | 592 / 343 | 200 / 251 |
| `SetAttributeEditorMode(Resize)` | 119 / 70 | 55 / 50 |
| `UpdateToDoCtrlPrefs(RiskLevels)` | 145 / 139 | 85 / 82 |
| `LoadState(Column order)` (both views) | 292 / 338 | 203 / 196 |
| `LoadTasks(Restore state)` | 325 / 680 | 227 / 392 |
| `RefreshFilterBarControls(FilterControls)` | 480 / 284 | 302 / 192 |
| `SelectToDoCtrl(Show)` | 19 / 136 | 14 / 76 |
| `SelectToDoCtrl(Invalidate/Update)` | 67 / 118 | 60 / 72 |

The wall and app CPU reductions are proportional and the logged sub-scopes shrink
uniformly. That points to an OS-level foreground scheduling/frequency effect rather than
a different code path. It must be controlled in every future measurement. The numbers
below are foreground (the figure the user actually sees when picking a document from a
visible, active window). Background measurements are inflated by roughly a third.

### Wall times (foreground run)

These are from a foreground activation (the user had brought the window to the front).
Background runs are ~30% slower; see the focus-state control above.

| File | Size | Wall | `CreateToDoCtrl` | Preferences | `Load` | show/paint | bookkeeping (filter bar) |
|---|---:|---:|---:|---:|---:|---:|---:|
| `Evidence Board.tdl` | 7.6 KB | 2387 ms | 645 ms | 801 ms | 494 ms | 73 ms | 340 ms |
| `biglist.tdl` | 1.66 MB | 2745 ms | 689 ms | 810 ms | 860 ms | 155 ms | 194 ms |

The fixed UI cost is ~2.2-2.3 s; the 1.66 MB file adds only ~360 ms over the 7.6 KB
file. This confirms the original conclusion: the problem is fixed per-tab UI work, not
I/O or parsing.

This detailed run measured 2387/2745 ms; the controlled foreground pair above measured
2503/2571 ms. That is run-to-run variance; the phase proportions are stable.

### Small file breakdown (`Evidence Board.tdl`, 7.6 KB, 2387 ms)

| Phase | ms | Detail |
|---|---:|---|
| Create window | 645 | `CToDoCtrl::Create` 580: `OnInitDialog` 407 (tree 115, comments 18, **attributes 243**, `UpdateControls` 31), dialog template/other 173; global styles 55 |
| Preferences | 801 | `ModifyStyles` 34, layout 54, **`SetAttributeEditorMode` 614** (set editor 329, repopulate 0.4, `UpdateControls` 205, `Resize` 79), `SetNumPriorityRiskLevels` 88, fonts 5 |
| `Load` | 494 | file read/parse 2; `LoadTasks` 492: globals 49, custom attrib 9, **split positions 130** (layout 74, attributes list 56), build tree 48, **restore state 229** (list view 116 incl. column order 110; tree 113 incl. column order 98), rest 20 |
| Post-load `OpenTaskList` | 8 | readonly/archive/MRU/time-tracker updates |
| Show / paint | 73 | `ShowWindow` 16, `Invalidate+UpdateWindow` 57 |
| Bookkeeping | 340 | almost entirely `RefreshFilterBarControls` (327; advanced filters 12, filter controls 327) |

`LoadTasksState` used to be the largest single `LoadTasks` line; it is now attributed:
the attribute-editor list view also restores its own column state (`TaskList`) before
the tree (`TaskTree`).

### Large file breakdown (`biglist.tdl`, 1.66 MB, 2745 ms)

| Phase | ms | Detail |
|---|---:|---|
| Create window | 689 | `CToDoCtrl::Create` 644: `OnInitDialog` 437 (tree 126, comments 18, attributes 256, `UpdateControls` 36), dialog template/other 205; styles 39 |
| Preferences | 810 | `ModifyStyles` 27, layout 50, **`SetAttributeEditorMode` 634** (set editor 352, `UpdateControls` 230, `Resize` 52), `SetNumPriorityRiskLevels` 90, fonts 5 |
| `Load` | 860 | file read/parse 33; `LoadTasks` 827: split positions 161, attribute visibility 53, build tree 111, **restore state 436** (list 122, tree 307 incl. column order 100 and `ExpandTasks` 196), rest 52 |
| Post-load | 13 | |
| Show / paint | 155 | `ShowWindow` 78, `Invalidate+UpdateWindow` 77 |
| Bookkeeping | 194 | filter bar refresh |

Only the `Load` phase and `ExpandTasks` grow with file size. Everything else is a
fixed per-tab cost.

### Attribution completeness

Every phase is now timed; there is no unnamed gap over 100 ms except the part of
`CToDoCtrl::Create` outside `OnInitDialog` (~170-250 ms), which is the MFC runtime
dialog creation/template path (`CRuntimeDlg::Create`). It is inside the timed
`CToDoCtrl::Create` scope and is the same for both file sizes. The remaining sub-blocks
over 100 ms are named calls: `SetAttributeEditorMode`, `SetNumPriorityRiskLevels`,
`CTDLTaskCtrlBase::LoadState(Column order)`, `ExpandTasks`, and
`RefreshFilterBarControls(FilterControls)`.

### Verdict on the original hypotheses

1. **Repeated synchronous layout/paint**: partly confirmed, but not where expected.
   `ModifyStyles` invalidation/update is <4 ms on the fresh hidden control. The real
   layout cost is inside `SetAttributeEditorMode`: `UpdateControls(TRUE)` 205-230 ms
   plus `Resize()` 52-79 ms, on top of editor creation.
2. **Comments/font work**: refuted. `SetCommentsFont` is <1 ms; tree font 4-11 ms.
3. **Attribute editor creation/update**: confirmed and dominant. `OnInitDialog`
   attributes control 243-256 ms plus `SetAttributeEditorMode` 614-1034 ms
   (`SetEditorMode` 329-557 ms) = ~0.9-1.3 s per activation.
4. **Tree-state restore**: confirmed, mostly fixed. Column-order restore costs
   ~100-190 ms per view (list and tree) regardless of size; `ExpandTasks` scales with
   saved expanded items (0-196 ms).
5. **Show/paint/bookkeeping**: paint is small (57-155 ms total); the bookkeeping cost
   is the filter bar refresh (194-340 ms), not idle tasks or due notifications.
6. **Preference getter cost**: not the issue in aggregate (all other setters are a few
   ms); `SetNumPriorityRiskLevels` is an unexpected ~90-145 ms.

The dispatch to `CTabbedToDoCtrl::LoadTasksState` explains an apparent 200+ ms
"unattributed" gap between `Build tree` and `LoadTasksState`: the list view restores
its own column state before the base class is entered.

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

These were the pre-measurement hypotheses. The measured verdicts are in
[Step 1-2 results](#step-1-2-results-instrumentation-and-release-attribution); the
short version is that the attribute editor dominates, the filter bar is second, and
comments/font work is negligible.

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

### 1. Add targeted timers — **Done**

Implemented as buffered `CTDCPerfTrace` scopes (see
[Step 1-2 results](#step-1-2-results-instrumentation-and-release-attribution)),
covering every path listed below plus the filter bar and the list-view state restore
that the original list missed. One buffered log write per activation replaces the
per-line `CScopedLogTimer` writes on this path.

### 2. Re-run the Window-menu measurement — **Done**

Release run with a fresh delay-loaded session, one small and one large tab. Both
activations are fully attributed; see the tables in
[Step 1-2 results](#step-1-2-results-instrumentation-and-release-attribution). The only
unnamed block over 100 ms is the MFC runtime dialog creation inside
`CToDoCtrl::Create` (~170-250 ms).

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

Do not start with micro-optimisation. The measured order of attack is:

1. **Attribute editor (largest, ~0.9-1.3 s).**
   - `OnInitDialog` creates the attribute editor unconditionally (243-256 ms even on a
     fresh, empty document).
   - `SetAttributeEditorMode` re-creates the editor on every activation
     (329-557 ms) and then calls `UpdateControls(TRUE)` (205-230 ms) and `Resize()`
     (52-79 ms). For a new control the mode should be established at creation, and the
     update/resize should be batched/coalesced.
2. **Filter bar refresh (~0.2-0.5 s).** `RefreshFilterBarControls(FilterControls)`
   rebuilds the controls after every selection. Check whether it can skip unchanged
   attributes or defer until the first paint.
3. **Column-order restore (~0.2 s fixed).** `CTDLTaskCtrlBase::LoadState` spends
   100-190 ms in `SetColumnOrder` for each of the list and tree views. Check for
   per-column reconfiguration/repaint and batch/reorder once.
4. **Priority risk levels (~0.09-0.145 s).** `SetNumPriorityRiskLevels` always
   repopulates the attribute editor even when the value has not changed; add a no-op
   guard.
5. **Layout/split state (~0.13-0.23 s).** `LoadSplitPos` restores layout and attribute
   list state before the control is shown; evaluate applying it at creation or
   deferring the recalculations.
6. **Post-load tree state.** `ExpandTasks` scales with saved expanded items
   (196 ms for 51 expanded items in `biglist`); defer it until after the first paint if
   it does not affect visible content.
7. **Dialog creation (~0.17-0.25 s).** The MFC runtime dialog template path is a
   residual fixed cost; investigate only after the items above.

General techniques that still apply:

- Batch preference application: defer `Resize`, `Invalidate`, `UpdateControls` and
  idle-column recalculation until `NotifyEndPreferencesUpdate`, then apply once.
- Avoid `UpdateWindow()` inside `CToDoCtrl::OnSize` for hidden or not-yet-shown controls;
  let the message loop coalesce paints.
- Collapse the multiple layouts in `CreateToDoCtrl`/`EnsureToDoCtrlWindow`/
  `SelectToDoCtrl` into one final size.
- Add cheap no-op guards to preference setters that rebuild controls
  (`SetNumPriorityRiskLevels`, `SetAttributeEditorMode`, `ModifyStyles`).
- Cache formatted column widths/text if `RecalcUntrackedColumnWidths` proves hot; in the
  measured runs it was 10-50 ms and not a priority.

Acceptance target: activating or opening an 8 KB tasklist in Release should be under
~1 s total, with no more than one layout and one full paint pass per activation, while
opening a 1.7 MB tasklist should differ only by the actual parse/build time.

## Risks and caveats

- The wall times still include `-g` and the full activation/paint path, but the
  buffered trace removes the per-line file overhead. Foreground activations measure
  2.5-2.8 s; the same runs in the background measure 3.5-4.6 s.
- **Foreground state is a measurement confound.** The same binary and session are ~30%
  slower in wall time and app CPU time when the window is not foreground, even when it
  is fully visible and at High priority. Pin the window foreground for all acceptance
  runs, or label the numbers as background.
- Single runs vary by roughly ±10%; repeat a measurement when a phase differs from the
  recorded range.
- The preference cost depends on the ini: fonts, theme, info tips, UI extensions,
  visibility and calculation settings all change `UpdateToDoCtrlPrefs`.
- `CScopedLogTimer` still formats its scope string when logging is disabled
  (`Core/Shared/ScopedTimer.cpp:94-115`); do not add permanent timers to per-row or
  per-cell paths for this work. `CTDCPerfTrace` only checks `IsLoggingEnabled()`.
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
