# UI-021 — Tasklist tab tooltip lists due-status reasons

## Goal

The coloured triangle at the top-left of an open tasklist tab is a due-status tag: red for
overdue tasks, another colour for tasks due today. Upstream only exposes that a tasklist is
due; it does not say which tasks caused it. That is a real problem when the offending task
is collapsed under a parent, hidden by the current filter, or when no Due column is visible:
the tab is marked but the task cannot be located in the view.

This change appends the actual triggering tasks to the tab tooltip, with their due dates:

```text
C:\Tasks\Project.tdl

Overdue tasks (3):
  - Send invoice (19/11/2026)
  - Review spec (20/11/2026)
  - Call supplier (21/11/2026)
```

At most ten tasks are listed; any remainder is summarised as `(+N more)`.

## Upstream starting point

- The tab tag is drawn by `CTabCtrlEx::DrawTabTag` (`Core/Shared/TabCtrlEx.cpp`) from the
  colour returned by `TCN_GETTAGCOLOR`, handled in
  `CToDoListWnd::OnTabCtrlGetTagColor` (`Core/ToDoList/ToDoListWnd.cpp`).
- The status itself is `CToDoCtrlMgr::RefreshDueItemStatus`
  (`Core/ToDoList/ToDoCtrlMgr.cpp`): `TDCM_PAST` if `HasOverdueTasks()`, else `TDCM_TODAY`
  if `HasDueTodayTasks()`, else none.
- Those checks run over the **complete data model** via `CTDCTaskCalculator`
  (`Core/ToDoList/ToDoCtrlDataUtils.cpp`), independently of visible columns, collapsed
  state, and the active filter. `CToDoCtrlMgr::GetTabItemTooltip` already produced the
  text shown for a tab (`TTN_NEEDTEXT` in `CToDoListWnd::OnNeedTooltipText`).

Two upstream paths that look like they should find the trigger often do not:

- **Tools > Show Tasks Due > By Today** applies the *Only show due tasks for person*
  preference (`PrepareNewDueTaskNotification`, `TDCGETTASKS::sAllocTo`) and exports only
  the task name plus visible-column attributes, so it can return an empty report.
- **Ctrl+F Find** walks the task-tree presentation (`CToDoCtrlFind::FindTasks`), so tasks
  hidden by the active filter are not searched.

### Delay-loaded tasklists

A tasklist can be opened without loading its tasks. In that state
`CToDoListWnd::DelayOpenTaskList` sets the tab status from the **earliest due date cached in
the file header** (`CToDoCtrl::DelayLoad` → `CTaskFile::GetEarliestTaskDueDate`), not from a
live scan of tasks — there is no task data to scan yet. This is why such a tab can show the
red tag while the Tools report, Find and any task-based enumeration show nothing, and why
the first version of this change (which skipped unloaded tabs) produced an empty tooltip.

The status is now stored together with that cached date, and the tooltip falls back to it
with an explicit note that the tasklist is not yet loaded. Once the tab is opened the live
task list is shown instead.

#### Phantom tag from a zero header date

The investigation also found why some delay-loaded tabs were tagged with no due tasks
anywhere: files with no due tasks can contain `EARLIESTDUEDATE="0.00000000"`.
`CDateHelper::IsDateSet` treats a valid OLE date of 0 (30 Dec 1899) as set, so
`DelayOpenTaskList` compared it with today and marked the tab `TDCM_PAST`. The Tools report
and Find then (correctly) show nothing because no task is actually due.

Fixed in three places:

- `CTaskFile::GetEarliestTaskDueDate` treats a non-positive date as unset and returns
  `FALSE`, so `DelayLoad`/`OffsetDates` never use the 1899 epoch.
- `CTDCTaskCalculator::GetStartDueDate` normalises a non-positive task due/start date to
  null, so a task stored with `DUEDATE="0.00000000"` cannot mark the tab overdue. The
  tooltip fallback also refuses to display an epoch date.
- `CToDoCtrl::DelayLoad` now applies the project name and custom attributes whenever the
  header parses, independently of whether an earliest due date exists (previously a
  zero-date header also lost the project name).

#### Single-line tooltip clipping

The tab tooltip control had no maximum tip width set, so although the text returned by
`TTN_NEEDTEXT` contained newlines, the popup was sized for one line and only the file path
was visible. `CToDoListWnd::InitTabCtrl` now calls
`CToolTipCtrlEx::EnableMultilineTips(m_tabCtrl.GetToolTips()->GetSafeHwnd())`
(`TTM_SETMAXTIPWIDTH`), and the full task list is displayed.

#### Caption separation

`CToDoListWnd::UpdateCaption` reuses `GetTabItemTooltip` for the bracketed status shown in
the window caption. The due reasons are now appended only when the text is actually used
for a tooltip (`bIncDueReasons`), so the caption stays single-line.

## Implementation

1. `CTDCTaskCalculator::GetDueTaskIDs(BOOL bOverdue, CDWordArray&, int nMaxCount)` walks the
   full `CToDoCtrlData` structure (reference nodes are skipped by `GET_SUBTASK` and cycles
   are guarded with the existing `CDWordSet` mechanism). For each task it reuses
   `GetTaskDueDate`, so the same `TDCS_USEEARLIESTDUEDATE` / `TDCS_USELATESTDUEDATE` and
   `TDCS_NODUEDATEISDUETODAYORSTART` rules apply. Overdue means the due date is before now
   (date-only dates are promoted to end-of-day, as in `IsTaskDue`); due-today means
   `CDateHelper::IsToday`.
2. `CToDoCtrl` exposes the new helper and `GetTaskDueDate(DWORD)`.
3. `CToDoCtrlMgr::AppendDueTaskReason` appends the header, task titles and formatted dates
   to the existing tooltip. It runs for any tab whose status is `TDCM_PAST` or
   `TDCM_TODAY`. Loaded tasklists list live tasks; delay-loaded tabs show the cached
   earliest due date from the file header plus a "not yet loaded" note. `TDCITEM` now
   stores that date, set by `DelayOpenTaskList`/`RefreshDueItemStatus`.
4. Five new localised strings: `IDS_TABTIP_DUEOVERDUE`, `IDS_TABTIP_DUETODAY`,
   `IDS_TABTIP_DUEMORE`, `IDS_TABTIP_DUEEARLIEST`, `IDS_TABTIP_DUENOTLOADED`.

## Files

- `Core/ToDoList/ToDoCtrlDataUtils.h/.cpp` — due-task enumeration.
- `Core/ToDoList/ToDoCtrl.h` — public accessors.
- `Core/ToDoList/ToDoCtrlMgr.h/.cpp` — tooltip append, caption separation and
  `datehelper.h` include.
- `Core/ToDoList/ToDoCtrl.cpp`, `Core/ToDoList/TaskFile.cpp` — zero header date handling.
- `Core/ToDoList/ToDoCtrlDataUtils.cpp` — zero task-date normalisation.
- `Core/ToDoList/ToDoListWnd.cpp` — caption requests without due reasons and multiline tab
  tooltip setup.
- `Core/ToDoList/resource.h`, `Core/ToDoList/ToDoList.rc` — five string resources.

## Risks

- The tooltip is computed on hover only, so the extra full-structure scan is not on a hot
  path. The overdue status scan already runs once per minute for every loaded tasklist.
- For delay-loaded tabs the cached header date can be stale (the file may have changed
  since it was last saved). It is labelled as the file's earliest due date and disappears
  once the tasklist is opened and re-evaluated.
- Enumeration uses each task's own effective due date. With the "use latest due date"
  style the aggregate `HasOverdueTasks` test uses the earliest date in a subtree; in a
  highly unusual mixed hierarchy this can differ, in which case the tooltip falls back to
  the earliest due date.
- Reference tasks are skipped, matching the existing calculator recursion.
- The tooltip grows by up to eleven lines; `TTS_NOPREFIX`/`TTS_ALWAYSTIP` already allow
  multi-line text.

## Verification

- `msbuild Core\ToDoList_Core.sln /t:Build /p:Configuration="Unicode Debug"
  /p:Platform=Win32 /m` succeeded with 0 errors.
- `Core\TDLTest\Unicode_Debug\TDLTest.exe` passed `310436/310436` tests.
- Manual check: hover a tab with the due tag and confirm the tooltip lists the same tasks
  that carry an overdue/due-today date, including ones under collapsed parents and ones
  hidden by the current filter; confirm the tag disappears and the list is gone after
  clearing the dates.
- Manual check with a delay-loaded session (`Introduction_Vietnamese.tdl`): hover without
  opening the tab and confirm the tooltip shows the cached earliest due date plus the
  "not yet loaded" note; open the tab and confirm the tooltip then lists the actual tasks.
- Verified by reading the live tooltip window contents (`WM_GETTEXT`) and geometry against
  temporary lists:
  - loaded `Introduction_Vietnamese.tdl` → `Overdue tasks (17): …`, tooltip popup grows to
    215 px for 14 lines (previously 20 px, i.e. only the file path was visible);
  - delay-loaded `Introduction_Vietnamese.tdl` → `Earliest due date: 06.09.2022` +
    "not yet loaded" note;
  - delay-loaded `Inventar.tdl` (`EARLIESTDUEDATE="0.00000000"`) → only
    "Not yet loaded (path)", i.e. no phantom 1899 date and no tag;
  - a copy of `IT - AI.tdl` with `DUEDATE="0.00000000"` injected into its first task →
    tooltip remains the single path line, no "Earliest due date: 30.12.1899" and no tag;
  - window caption remains `Sample Tasklist VietNam [path] - Better TDL` with no due
    reasons appended.
