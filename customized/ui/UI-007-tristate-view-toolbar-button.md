# UI-007 — Tri-state toolbar button for view layout (task list / editor / split)

| Field | Value |
|---|---|
| Status | Proposed (design note; no code changes) |
| Upstream baseline | ToDoList 9.3.DP7 |
| Scope | `Core/ToDoList/TDLToolbar.*`, `Core/ToDoList/ToDoListWnd.*`, `Core/ToDoList/ToDoList.rc`, `Core/Shared/EnToolBar.*` |
| Related | [index.md](../index.md), [UI-006](UI-006-toolbar-architecture.md), [TED-005](../text-editor/TED-005-comments-display-toggles.md), [TED-001](../text-editor/TED-001-comments-toolbar-in-main-toolbar.md) |

## Question

Can the toolbar have a three-state control for the view layout:

1. maximise task list,
2. maximise comments editor,
3. split view (normal)?

Different icons per state are optional but desirable.

## Findings

- The toolbar is a normal `ToolbarWindow32`. It supports `TBSTYLE_CHECK`,
  `TBSTYLE_GROUP` and per-button image changes, so there is no control-level limitation.
- A toolbar button maps to exactly one command. A "three-state button" is therefore an
  application-level command/state design, not a native tri-state check box.
- `CEnToolBar` already paints checked buttons with a darker background
  (`Core/Shared/EnToolBar.cpp:403-407`), so check-style buttons work with the current
  theming.
- The layout state already exists as `TDCMS_NORMAL`, `TDCMS_MAXTASKLIST`,
  `TDCMS_MAXCOMMENTS` (`Core/ToDoList/TDCEnum.h:882-884`) and is mirrored in
  `CToDoListWnd::m_nMaxState`. Each tasklist control stores its own state; the frame
  refreshes `m_nMaxState` when the active tab changes
  (`Core/ToDoList/ToDoListWnd.cpp:14104`).
- Existing commands:
  - `ID_VIEW_MAXTASKLIST`: toggles normal/task list and calls `SetCheck`
    (`ToDoListWnd.cpp:6569-6587`);
  - `ID_VIEW_MAXCOMMENTS`: toggles normal/editor and calls `SetCheck`
    (`ToDoListWnd.cpp:6589-6607`);
  - `ID_VIEW_UNMAXTASKLISTANDCOMMENTS`: returns to normal, but is labelled "Unmaximise"
    and is disabled when already normal (`ToDoListWnd.cpp:6609-6627`), so it does not fit
    a radio-style third state.
- The data-driven builder currently hardcodes `TBSTYLE_BUTTON` for every command
  (`Core/ToDoList/TDLToolbar.cpp:194-199`); `TDCTOOLBARITEM` has no style field
  (`Core/ToDoList/TDLToolbar.h:12-23`).
- Runtime icon swapping is supported by `CEnToolBar::SetItemImage`
  (`Core/Shared/EnToolBar.cpp:676-684`) using `TBIF_IMAGE`; the custom toolbar uses the
  same mechanism for image changes (`Core/ToDoList/TDLCustomToolbar.cpp:183-192`).
- Toolbar tooltips come from `STRINGTABLE` entries keyed by command ID (see UI-006).

## Option A — three check buttons (recommended)

Three adjacent buttons, each checking itself when its state is active:

- Maximum task list: existing `ID_VIEW_MAXTASKLIST`.
- Split view: new `ID_VIEW_SPLITVIEW` that sets `TDCMS_NORMAL` and is checked in normal
  state. (Do not reuse the "Unmaximise" command.)
- Maximum editor: existing `ID_VIEW_MAXCOMMENTS`.

Required changes:

1. Add a style field to `TDCTOOLBARITEM`, defaulting to `TBSTYLE_BUTTON`, and use it in
   `CTDLToolbarBuilder::Build` instead of the hardcoded style.
2. Add `ID_VIEW_SPLITVIEW` with:
   - a command handler that sets `m_nMaxState = TDCMS_NORMAL`, applies it via
     `GetToDoCtrl().SetMaximizeState(...)`, invalidates and updates the caption (same
     pattern as `OnMaximizeTasklist`);
   - an `ON_UPDATE_COMMAND_UI` handler that enables it when a tasklist is open and calls
     `pCmdUI->SetCheck(m_nMaxState == TDCMS_NORMAL)`;
   - a `STRINGTABLE` tooltip such as `"\nSplit View"`.
3. Add the three spec items:

```cpp
{ ID_VIEW_MAXTASKLIST, _T("210"), TBSTYLE_CHECK | TBSTYLE_GROUP },
{ ID_VIEW_SPLITVIEW,   _T("..."), TBSTYLE_CHECK | TBSTYLE_GROUP },
{ ID_VIEW_MAXCOMMENTS, _T("..."), TBSTYLE_CHECK | TBSTYLE_GROUP },
```

`TBSTYLE_GROUP` is optional: the `ON_UPDATE_COMMAND_UI` checks are authoritative because
MFC re-applies them on idle. `TBSTYLE_CHECK` is required for the checked visual.

No extra icon plumbing is needed; the check mark is the state indicator. This is the least
surprising and most discoverable option and matches radio-button conventions.

## Option B — one cycling button

One button cycles split -> task list -> editor -> split.

Required changes:

1. Add `ID_VIEW_CYCLEVIEW` with a handler that advances `m_nMaxState` and applies it to the
   active tasklist. Reuse the body pattern of `OnMaximizeTasklist` /
   `OnMaximizeComments`.
2. Leave the button as `TBSTYLE_BUTTON` (a check state cannot express three states), or
   mark it checked only when not normal.
3. Optional per-state icons:
   - extend `TDCTOOLBARITEM` with alternate image keys (or a small per-command image table);
   - make the builder copy those extra images into the per-bar image list, otherwise
     `SetItemImage` would reference a missing index;
   - after each state change, call
     `m_toolbarMain.SetItemImage(m_toolbarMain.CommandToIndex(ID_VIEW_CYCLEVIEW), nImage)`;
   - also refresh in `ON_UPDATE_COMMAND_UI`, because switching tabs changes `m_nMaxState`.
4. Tooltip: a static `STRINGTABLE` entry such as `"\nCycle View"` is sufficient. For a
   state-aware tooltip, call `m_tbHelperMain.SetTooltip(ID_VIEW_CYCLEVIEW, text)` when the
   state changes.

This saves toolbar width but hides the available actions and requires icon management.

## Option C — dropdown button

A `TBSTYLE_DROPDOWN` button with three radio menu items (task list, split, editor) is also
possible and is already supported by `CToolbarHelper::SetDropButton`
(`Core/Shared/ToolbarHelper.h:34`). It shows one button with an explicit menu, at the cost of
an extra click and a menu to maintain. Use it only if more view modes are expected later.

## Recommendation

Use **Option A** (three grouped check buttons). It reuses two existing commands, needs no
runtime image swapping, works with the current custom draw/theming, and gives each action
its own tooltip and shortcut. Use Option B only if horizontal space is the priority.

## Risks and constraints

- A single command ID can only have one button in a toolbar. Do not duplicate command IDs
  in the spec; use three commands for three buttons.
- `SetItemImage` needs the target index to exist in the bar's image list. The phase-1
  builder copies only images referenced by the spec, so alternate icons must be included
  explicitly.
- State is per active tasklist. Update handlers must read `m_nMaxState` (refreshed on tab
  switch) rather than caching a frame-only value.
- Add a `STRINGTABLE` tooltip for every new command, otherwise debug builds assert in the
  frame tooltip fallback (see UI-006).
- The "split view" command must be enabled and checked in the normal state, unlike
  `ID_VIEW_UNMAXTASKLISTANDCOMMENTS`.

## Verification

- Build `Core\ToDoList\ToDoList.vcxproj` (Unicode Debug/Win32) after the change.
- Check button states: normal -> split checked; maximise task list -> task-list checked;
  maximise editor -> editor checked; switching tasklist tabs updates the checks.
- Check shortcuts and menu items still work and stay in sync with the toolbar.
- Hover each button and confirm the tooltip text (and shortcut suffix) via
  `TDL_ASSERT_LOG` with no assertions.
- Toggle the main toolbar off/on and switch theme/high-DPI to confirm checked painting.

## Open questions

- Is a third command (`Split View`) acceptable in the menu and shortcut manager, or should
  the cycle command be the only new entry?
- Are three buttons acceptable in the main toolbar, or is width pressure already high?
- Should the split-view state be offered in the View menu as a radio group alongside the
  two maximise commands?
