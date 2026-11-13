# CUST-013: Extended Window menu list and document count

## Goal

Show up to 25 open tasklists in the **Window** menu instead of 16. When more than 25
tasklists are open, append a disabled `...` item to indicate that the list is truncated.
Show the number of open tasklists in the menu title, for example `Window (4)`.

## Upstream starting point

- `CToDoListWnd::OnUpdateWindow()` (`Core/ToDoList/ToDoListWnd.cpp`) capped the list at
  16 entries using `ID_WINDOW1` … `ID_WINDOW16`.
- `OnWindow()` mapped command IDs directly as `nCmdID - ID_WINDOW1`.
- The `IDR_MAINFRAME` Window popup contained 16 grayed `Window N` placeholders in
  `Core/ToDoList/ToDoList.rc`.
- `CTDCMainMenu::IsDynamicItem()` and `GetDynamicItemTooltip()`
  (`Core/ToDoList/TDCMainMenu.cpp`) handled only the 16-item range.

## What changed

Files: `Core/ToDoList/ToDoListWnd.h/.cpp`, `Core/ToDoList/TDCMainMenu.cpp`,
`Core/ToDoList/ToDoList.rc`, `Core/ToDoList/resource.h`.

1. Added `ID_WINDOW17` … `ID_WINDOW25` (34800–34808) and `ID_WINDOW_MOREWINDOWS` (34809).
   The original `ID_WINDOW1` … `ID_WINDOW16` values are unchanged so existing keyboard
   shortcuts remain valid.
2. `CToDoListWnd` keeps an ordered `WINDOW_MENU_IDS[]` array and maps command IDs back to
   tasklist indexes through `GetWindowMenuIndex()`. Both the original and the new command
   ranges route to `OnWindow()`.
3. `OnUpdateWindow()` deletes and rebuilds all 25 entries, and appends a disabled `...`
   item when the tasklist count exceeds 25.
4. `UpdateWindowMenuTitle()` reads the current (localized) Window menu text, strips any
   previous ` (n)` suffix containing only digits, and appends the current tasklist count.
   It is called from the existing caption idle task and from menu initialization, and is
   cached so the menu bar is only redrawn when the count changes.
5. `CTDCMainMenu` recognizes the extra command range for dynamic-item detection and
   tooltips.

## Behaviour

- The Window menu lists the first 25 open tasklists.
- If more than 25 tasklists are open, a grayed `...` item follows the list.
- The Window menu title shows the current number of open tasklists, e.g. `Window (27)`,
  including tasklists that are delay-loaded but not yet activated.
- The menu title is updated when tasklists are opened, closed, closed in bulk, or when the
  menubar is reloaded (for example after a UI language change).

## Risks

- The `...` item is an indicator only; it is not a command that opens a full list. The
  tasklist tab bar remains the way to reach tasklists beyond the first 25.
- `ID_WINDOW17` … `ID_WINDOW25` use a new resource-ID block because the space immediately
  after `ID_WINDOW16` is partly occupied by unrelated commands.
- The count suffix is appended literally; localized Window menu titles that already end
  with a numeric parenthesized suffix would be replaced.

## Test notes

- Built `Core/ToDoList_Core.sln` (`Unicode Debug`, `Win32`) successfully.
- `TDLTest` ran; the 24 `CTDCTaskMatcherTest::IndividualRules` text-array failures are
  pre-existing and unrelated to this change.
- Manual check: open enough tasklists to exceed 25 and confirm the extended list, the `...`
  marker, and the title count.
