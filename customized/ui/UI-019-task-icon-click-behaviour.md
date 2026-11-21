# UI-019 — Task icon click behaviour

## Goal

Make the single-click behaviour of a task's icon configurable. Upstream opens the icon
selection dialog when the icon beside a task title is clicked. This option allows users
who prefer the title behaviour to turn that off:

- **On (default)** — a single click on the task icon opens the icon selection dialog and
  shows the hand cursor over the icon.
- **Off** — the icon behaves like the task title text: a single click selects the task,
  a double click folds or unfolds a parent task, and no hand cursor is shown over the
  icon.

The icon column (when `TDCC_ICON` is displayed as its own column) is unaffected: that is
the normal column-edit path and still opens the icon selection dialog.

## Behaviour

- Preferences: **User Interface > General > Click task icons to open the icon selection
  dialog** — default on.
- Persisted in the existing `[Preferences]` section of `ToDoList.ini` as
  `ClickTaskIconEditsIcon=0|1`.
- Switching the option and pressing OK updates open tasklists immediately; no restart.
- Applies to both the Task Tree and the flat List View title pane, including the cursor.

## Implementation

The preference is delivered to each tasklist as the `TDCS_EDITICONONCLICK` style via
`CTDCToDoCtrlPreferenceHelper::PopulateStyles`, so existing preference plumbing and
per-tasklist style updates are reused.

Click and cursor handling then checks the style:

- `CTDLTaskTreeCtrl::HandleClientColumnClick` only maps `TVHT_ONITEMICON` to
  `TDCC_ICON` when the style is enabled; `CTDLTaskTreeCtrl::OnSetCursor` only shows the
  hand cursor under the same condition. When disabled, the click falls through to the
  tree's default handling, which selects the task.
- `CTDLTaskListCtrl::HandleClientColumnClick` and `CTDLTaskListCtrl::OnSetCursor` apply
  the same style check to `LVHT_ONITEMICON`.
- The tree's existing double-click branch, which folds/unfolds parent tasks, runs before
  the single-click icon handling and is unchanged.

The new control was inserted in the General group and the controls below it were moved
down by 14 dialog units to make room.

## Files

- `Core/ToDoList/TDCEnum.h` — new `TDCS_EDITICONONCLICK` style.
- `Core/ToDoList/TDLTaskTreeCtrl.cpp` — click and cursor conditions.
- `Core/ToDoList/TDLTaskListCtrl.cpp` — click and cursor conditions.
- `Core/ToDoList/PreferencesUIPage.h` / `.cpp` — option, persistence and tooltip.
- `Core/ToDoList/PreferencesDlg.h` — forwarding getter.
- `Core/ToDoList/TDCToDoCtrlPreferenceHelper.cpp` — apply the preference as a style.
- `Core/ToDoList/ToDoList.rc`, `Core/ToDoList/resource.h` — checkbox, tooltip and IDs.

## Verification

- `msbuild Core\ToDoList_Core.sln /t:Build /p:Configuration="Unicode Debug"
  /p:Platform=Win32 /m` succeeded with 0 errors.
- `Core\TDLTest\Unicode_Debug\TDLTest.exe` passed `310436/310436` tests.
- Interactive verification of the cursor and the two click modes is left to the user.
