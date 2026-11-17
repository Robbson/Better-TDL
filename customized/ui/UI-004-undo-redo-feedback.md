# UI-004: Undo/redo feedback, enablement and saved-state tracking

Status: **Done** (implemented, built, unit-tested)

## Goal

Make undo/redo predictable:

- the menu and toolbar say what will be undone or redone;
- Undo and Redo are enabled only when their respective stack can act;
- redo no longer performs an undo while focus is in an edit field;
- the modified (`*`) marker clears when undo/redo returns to the loaded or saved state;
- comments-editor undo is queried through the rich edit's real undo/redo state instead of
  a one-way dirty flag.

## Upstream starting point

- `CToDoCtrl::CanUndoLastAction()` ignored `bUndo` for the comments editor and for
  `WC_EDIT` controls (`Core/ToDoList/ToDoCtrl.cpp:9919` upstream).
- `CToDoCtrl::UndoLastAction()` always called `pEdit->Undo()` in the edit branch,
  including for Redo requests (`Core/ToDoList/ToDoCtrl.cpp:9942` upstream).
- Comments enablement used `m_nCommentsState != CS_CLEAN`, which never returns to
  `CS_CLEAN` after the first edit.
- `CToDoCtrl::SetModified()` was a plain boolean with no relationship to undo position;
  undo/redo always set it to `TRUE` (`Core/ToDoList/ToDoCtrl.cpp:10013` upstream).
- `OnUpdateEditUndoRedo()` only called `Enable()` (`Core/ToDoList/ToDoListWnd.cpp:13990`
  upstream), and the menu/toolbar only had static "Undo"/"Redo" text.
- `CContentCtrl` exposed `Undo()`/`Redo()` but no way to query availability.

## What changed

Files:

- `Core/ToDoList/ToDoCtrlUndo.h/.cpp`
- `Core/ToDoList/ToDoCtrlData.h/.cpp`
- `Core/ToDoList/ToDoCtrl.h/.cpp`
- `Core/ToDoList/ToDoListWnd.h/.cpp`
- `Core/ToDoList/TDLCommentsCtrl.h`
- `Core/ToDoList/TDLSimpleTextContentCtrl.h/.cpp`
- `Core/Interfaces/ContentCtrl.h/.cpp`
- `Core/Shared/UrlRichEditCtrl.cpp`
- `Core/ToDoList/resource.h`, `Core/ToDoList/ToDoList.rc`
- `Core/TDLTest/ToDoCtrlDataTest.h/.cpp`

1. **Per-action serial and save point.**
   Each `TDCUNDOACTION` receives a `dwSerial` when it is created. `CToDoCtrlUndo` records
   the serial of the action on top of the stack at load/save time and exposes
   `SetSavePoint()` / `IsAtSavePoint()`. Undo/redo moves actions between the two stacks
   without changing their serials, so returning to the saved top action reports clean.
   Extending the action that contains the save point invalidates the save point because
   the saved content is no longer reachable.
2. **Modified state follows the save point.**
   `CToDoCtrl::UndoLastAction()` still emits the normal modification notifications, but
   then sets `m_bModified` from `m_data.IsAtUndoActionSavePoint()`. `SetModified(FALSE)`
   records a new save point, so save, load, and "treat as clean" paths stay consistent.
3. **Directional enablement.**
   `CanUndoLastAction(bUndo)` now queries the comments editor's real `CanUndo`/`CanRedo`
   (falling back to the document stack) and never reports Redo from a plain `CEdit`
   (which only has undo). `UndoLastAction()` only forwards Undo to a focused edit, so Redo
   cannot accidentally undo text.
4. **Action descriptions.**
   `TDCUNDOACTION::GetAttributeIDs()` extracts the attributes stored in undo elements.
   `CToDoCtrl::GetUndoActionDescription()` builds "Edit Priority", "Delete 3 tasks",
   "Move task", etc. using the existing attribute labels. `OnUpdateEditUndoRedo()` sets the
   menu text and updates the main-toolbar tooltip through `CToolbarHelper`, appending the
   shortcut as before. The cached undo/redo tip text is cleared whenever the main toolbar
   is (re)initialised so a theme/language toolbar rebuild cannot leave a stale tip.
5. **Comments capacity queries.**
   `CContentCtrl::CanUndo()` / `CanRedo()` query `EM_CANUNDO` / `EM_CANREDO` on the content
   control window; `CTDLCommentsCtrl` forwards them.
6. **Text-edit descriptions.**
   `CanUndoLocalAction()` centralises the focused-control rules. When the active undo/redo
   belongs to the comments editor or a focused edit field, the menu/tooltip reports
   "Edit text" instead of showing the document's previous action.
7. **Link formatting and auto-indent no longer create phantom undo steps.**
   `CUrlRichEditCtrl::OnSetText()` clears the undo buffer after `ParseAndFormatText(TRUE)`.
   `CTDLSimpleTextContentCtrl` also clears the buffer after the auto-indent pass that
   follows a programmatic `WM_SETTEXT`; a real user edit cancels that pending clear so the
   user's typing history is preserved. Clicking into the editor therefore no longer shows
   an "Edit text" undo for a change that never happened.

## Behaviour

- Edit menu and toolbar show e.g. "Undo: Edit Priority", "Redo: Delete 3 tasks" or
  "Undo: Edit text" for the focused text control.
- Toolbar tooltip shows the same action plus the shortcut.
- Undo is disabled when the focused editor and the document stack are both exhausted.
- Redo is disabled when the redo stack is empty, including after redoing everything.
- Redo while editing a field no longer undoes the field text.
- Undoing every change back to the loaded/saved state clears the tab's `*`.
- Redoing forward to the saved state clears it again.

## Risks and limits

- The save point is positional, not content-based. If an action is extended after saving,
  the document is treated as modified until the next explicit save, which is correct but
  means it cannot return to "clean" by undoing that extended action.
- The description is intentionally coarse: it names the action type, affected attribute
  or task count, not the exact before/after value.
- Content plugins are assumed to be rich-edit based for `EM_CANUNDO`/`EM_CANREDO`; the
  built-in text content control and `RTFContentCtrl` both are.

## Tests

- Added `CToDoCtrlUndo::SavePoint` to `CToDoCtrlDataTest`:
  save-point set/reset, undo/redo round trips, attribute extraction, extension
  invalidation and `ResetAll`.
- Added `CToDoCtrlData::MoveUndoRedo`: move → undo → redo → undo restores the original
  parent and position, confirming the move stack is not split or duplicated.
- `Core\TDLTest\Unicode_Debug\TDLTest.exe` passes `310297/310297`.
- Core `Unicode Debug` build succeeds.

## Known remaining issue

The reported need for a second click after move → undo → redo could not be reproduced at
the data/undo layer (`MoveUndoRedo` passes), so it is a view/focus or command-routing
issue rather than a duplicated move action.
