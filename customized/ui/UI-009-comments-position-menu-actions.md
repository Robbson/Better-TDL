# UI-009: Menu actions for the comments position

Status: **Done** (implemented, built and tests pass; interactive check pending)

## Goal

Make the three comments-panel positions that exist as a preference
(`Preferences > User Interface > General > Position comments`) available as main-menu
commands. Both the keyboard-shortcut editor and the custom toolbar build their command
lists from the main menu, so a menu command is the only way to expose the positions to
those features.

## Upstream starting point

- The position is stored as the `Preferences\VertComments` integer, loaded and written by
  `CPreferencesUIPage` (`Core/ToDoList/PreferencesUIPage.cpp:149`, `:211`).
- `PUIP_LOCATION` (`Core/ToDoList/TDCEnum.h:1022`) defines `PUIP_LOCATEBOTTOM`,
  `PUIP_LOCATERIGHT` and `PUIP_LOCATELEFT`, matching `TDC_UILOCATION`.
- Applying the preference to a tasklist happens through
  `CTDCToDoCtrlPreferenceHelper::UpdateToDoCtrl()`
  (`Core/ToDoList/TDCToDoCtrlPreferenceHelper.cpp:104`).
- The shortcut tree (`Core/ToDoList/TDLShortcutsTreeListCtrl.cpp:130`) and the custom
  toolbar menu-item list (`Core/ToDoList/TDLToolbarButtonListCtrl.cpp:109`) both
  enumerate `IDR_MAINFRAME`, so new menu commands appear there automatically.

## What changed

Files: `Core/ToDoList/ToDoList.rc`, `Core/ToDoList/resource.h`,
`Core/ToDoList/ToDoListWnd.h/.cpp`, `Core/ToDoList/PreferencesUIPage.h/.cpp`,
`Core/ToDoList/PreferencesDlg.h/.cpp`.

1. Added `ID_VIEW_COMMENTSPOS_BOTTOM` (34789), `ID_VIEW_COMMENTSPOS_RIGHT` (34790) and
   `ID_VIEW_COMMENTSPOS_LEFT` (34791).
2. Added a `Comments Position` submenu to the **View** menu with `Below Tasklist`,
   `Right of Tasklist` and `Left of Tasklist`.
3. `CToDoListWnd` handles the commands by updating the stored preference through new
   `CPreferencesUIPage::SetCommentsPos()` / `CPreferencesDlg::SetCommentsPos()` helpers,
   saving the INI/registry store, marking every tasklist for a preference update and
   refreshing the active tasklist. The update handlers show the active position as a
   radio check.

## Behaviour

- The menu commands change the same value as the preference combo and persist it, so the
  choice survives a restart and applies to newly opened tasklists.
- All open tasklists switch to the new position; the active one updates immediately and
  the others when they are next activated (existing lazy-update path).
- The commands are available in the keyboard-shortcut editor and the custom toolbar
  button list without further wiring.

## Risks

- The commands change a global preference, exactly like the preferences dialog, rather
  than only the active tasklist.
- The new commands have no main-toolbar icon; the custom toolbar uses the fallback icon
  unless the user picks one.
- `SetCommentsPosition()` writes the whole UI preference page, mirroring the existing
  `SetAutoHideTabbar()` pattern.

## Test notes

- Built `Core/ToDoList_Core.sln` (`Unicode Debug`, `Win32`). The changed projects compile;
  linking the real `ToDoList.exe` was blocked by an already running instance locking the
  file, so the link was verified by building a temporary copy of the project against the
  same objects.
- `TDLTest` ran: 310,419/310,419 tests succeeded.
- Manual check: apply each of the three positions from the View menu and confirm the
  layout changes and the radio check follows the active position.
- Reopen `Preferences > User Interface > General` and confirm `Position comments` shows
  the last value selected from the menu.
- Assign a shortcut and add a custom-toolbar button to each new command and verify both
  invoke the action.
