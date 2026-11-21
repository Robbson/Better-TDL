# UI-023 Status bar caret line/column

## Goal

Show the caret's 1-based line and column in the main status bar while the comments
text editor has focus, and clear it when focus leaves the editor.

## Upstream starting point

Upstream ToDoList shows only the focused control name in the `ID_SB_FOCUS` status
bar pane (`Core/ToDoList/TDLStatusBar.cpp`). The comments editor does not report
selection changes to the main window.

## Implementation

- Added an `ID_SB_CARET` status-bar pane (auto-fit) immediately before the focus
  pane, with `IDS_SB_CARET` (`"Ln %d, Col %d"`) and `IDS_SB_CARET_TIP`.
- `CTDLCommentsCtrl` handles `EN_SELCHANGE` and `EN_SETFOCUS` from the embedded
  rich-edit control, computes the caret position with `EM_EXGETSEL`,
  `EM_EXLINEFROMCHAR` and `EM_LINEINDEX`, and sends `WM_ICC_CARETCHANGE`.
- `CTDLSimpleTextContentCtrl` adds `ENM_SELCHANGE` to its rich-edit event mask;
  without it the editor never sends the selection-change notification.
- `CToDoCtrl` forwards `WM_ICC_CARETCHANGE` to the main window as
  `WM_TDCN_CARETCHANGE`.
- `CToDoListWnd` updates `CTDLStatusBar::UpdateCaretPosition()` and clears the
  pane in `OnFocusChange` when the comments editor is no longer focused.
- `CTDLStatusBar` caches the last line/column so repeated selection notifications
  do not repaint the pane unnecessarily.

## Files

- `Core/Interfaces/IContentControl.h` — new `WM_ICC_CARETCHANGE` notification
- `Core/ToDoList/TDCMsg.h` — new `WM_TDCN_CARETCHANGE` notification
- `Core/ToDoList/TDLCommentsCtrl.h/.cpp` — selection/focus handlers
- `Core/ToDoList/ToDoCtrl.h/.cpp` — forwarding
- `Core/ToDoList/ToDoListWnd.h/.cpp` — status-bar update and clear
- `Core/ToDoList/TDLStatusBar.h/.cpp` — caret pane and formatter
- `Core/ToDoList/resource.h`, `Core/ToDoList/ToDoList.rc` — pane/string IDs

## Behaviour and limits

- Only rich-edit based comments controls report positions; custom content
  controls that do not send `EN_SELCHANGE` are unaffected.
- Line numbers follow the rich edit's line model, which includes display lines
  when word wrap is enabled.
- The pane is blank when the comments editor is not focused.

## Risks

- Adds a new pane to the status bar, which slightly changes the width of the
  stretchy title pane.
- Interface header change is additive and does not bump `ICONTENTCTRL_VERSION`.

## Tests

- `BuildDebug`/MSBuild `Unicode Debug` succeeds.
- `Core\TDLTest\Unicode_Debug\TDLTest.exe`: 310454/310454 passed, including
  `CTDLSimpleTextContentCtrl::CaretChangeNotification`, which verifies that the
  control requests `ENM_SELCHANGE` and emits `EN_SELCHANGE` when the caret moves.
- Interactive check pending: focus the comments editor, move the caret and
  verify `Ln n, Col n` updates; leave the editor and verify the pane clears.
