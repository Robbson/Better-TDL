# PERF-008: Delayed loading for multi-document menu and Home opens

Status: **Done** (built through `ToDoListLib`/`TDLTest`; interactive exe run pending)

## Goal

`[Preferences] EnableDelayedLoading` deferred task data only for documents restored
from the previous session and for the second and later files on the command line.
Opening several tasklists from **File → Open Tasklist** or from the **Home Folder**
view loaded every document in full, so selecting a folder or a multi-selection paid the
full open cost for each tab even though only one tab was going to be shown.

This change applies the existing "first document fully, the rest delayed" rule to those
multi-open paths.

## Upstream starting point

- `CToDoListWnd::OnLoad` (`Core/ToDoList/ToDoListWnd.cpp`) looped over the multi-select
  file dialog and called `OpenTaskList` for each path.
- `CToDoListWnd::OpenHomeSelection` looped over the Home selection and called
  `OpenTaskList` for every `.tdl` it found, including recursively collected files.
- Delayed loading was only entered from `OnPostOnCreate` (session restore) and
  `ProcessStartupOptions` (command line) via `CToDoListWnd::DelayOpenTaskList`
  (see [PERF-004](PERF-004-delayed-tasklist-window.md)).

## What changed

Files:

- `Core/ToDoList/ToDoListWnd.h`
- `Core/ToDoList/ToDoListWnd.cpp`

New helper `CToDoListWnd::OpenTaskLists(const CStringArray&, BOOL)`:

1. Opens the first path with `OpenTaskList` (full load) and remembers its tab index.
2. Delay-opens every later path with `DelayOpenTaskList` when delayed loading is
   enabled.
3. Falls back to a full open for a later path when it is a storage tasklist or already
   has registered reminders, matching the guards used by session restore. Files whose
   earliest due date falls inside `NotifyDueByOnLoad` are still downgraded to a full
   open inside `DelayOpenTaskList`.
4. Reports load errors through `HandleLoadTasklistError` and selects the first opened
   tab, matching `ProcessStartupOptions`.

Call sites:

- `OnLoad` now collects the dialog paths into a `CStringArray` and calls the helper.
- `OpenHomeSelection` now first collects every selected/home-folder `.tdl` path and
  calls the helper once; non-tasklist files are still launched with their shell handler.
  The Home status refresh flag is set when at least one tasklist opened.

Single-document opens (the MRU list, `OpenHomeItem` double-click, drag/drop, archive and
storage openers) are unchanged and still load fully immediately.

## Behaviour

- Opening one document from the menu or Home behaves exactly as before: full load and
  the document is selected.
- Opening several documents loads the first one and selects it; the remaining tabs
  appear immediately with the existing "not loaded" state and load on first activation.
- With delayed loading disabled, or when no later document could be delayed, the
  previous behaviour is preserved: every document loads and the last opened one stays
  selected.
- During a Home multi-selection or folder open, files are no longer opened while the
  selection is being enumerated. Non-`.tdl` selections are still launched in selection
  order.

## Verification

- `TDLTest` (Unicode Debug): **310419/310419 tests pass**. The test harness rebuilt
  `ToDoListLib` and linked against the changed code.
- `ToDoList.exe` could not be relinked in this session because an already running
  `Core\ToDoList\Unicode_Debug\ToDoList.exe` (not started by this work) holds the
  image and plugin DLLs. Plugin copy post-build steps therefore failed with
  `MSB3073`; the compile and library link themselves succeeded.
- Interactive check still required: open several files from the menu and from Home,
  confirm only the first is loaded and later tabs load on activation.

## Risks and follow-ups

- Delay-opened files are added to the MRU only when they are first activated, because
  `OpenTaskList` owns that update. If they are closed without activation they still
  appear in session state but not in the MRU. This matches restored-session behaviour;
  adjust only if MRU completeness is required.
- The first document is always the fully loaded one and the selected tab. Previously
  the last file of a multi-open selection was selected; the new behaviour matches the
  command line and session restore.
- Any new multi-open entry point should call `OpenTaskLists` instead of looping over
  `OpenTaskList` to keep the policy in one place.
