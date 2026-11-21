# UI-022: Assertion summary menu instead of assertion dialogs

Status: **Done** (built in Debug and Release; runtime-verified with temporary asserts)

## Goal

Stop repeated Debug assertions from blocking the application with hundreds of modal
dialogs. Collect the assertion locations instead and expose them through a red
`Asserts (N)` menu that is added dynamically to the main menu bar, immediately to the
left of `Debug`.

Selecting the menu shows one entry per distinct assertion source, most frequent first:

```text
ToDoCtrlDataUtils.cpp(261) [20x]
TDLTaskViewListBox.cpp(172) [3x]
```

## Upstream starting point

Upstream `ASSERT`/`VERIFY` failures go through the CRT report hook and end in the
standard assertion dialog. The fork already logged every assertion to
`%TEMP%\ToDoList_asserts.log` (see [GEN-006](../general/GEN-006-assert-logging.md)) but
still showed the dialog unless `TDL_ASSERT_LOG` was explicitly set.

## What changed

Files:

- `Core/ToDoList/ToDoList.h`, `Core/ToDoList/ToDoList.cpp`
- `Core/ToDoList/TDCMainMenu.h`, `Core/ToDoList/TDCMainMenu.cpp`
- `Core/ToDoList/ToDoListWnd.h`, `Core/ToDoList/ToDoListWnd.cpp`

Debug builds now:

1. Record each assertion source in an in-memory list keyed by `file(line)`, parsed from
   the CRT hook message, and increment a total count. A critical section protects the
   list because assertions can occur on any thread.
2. Suppress the assertion dialog and debug break. `TDL_ASSERT_LOG=0` still restores the
   stock dialog (and disables logging/menu collection).
3. Post a coalesced `WM_TDL_ASSERTREPORTED` message to the main window. The window
   rebuilds the menu without flooding the message queue when an assertion repeats in a
   tight loop.
4. Insert a right-justified owner-drawn `Asserts (N)` popup at `AM_DEBUG`, so it appears
   immediately left of `Debug`. The item is drawn in red (`RGB(200, 0, 0)`), with the
   normal highlight text while selected.
5. Populate the popup on every update, sorted by descending count. Each entry uses
   `ID_TOOLS_VIEWASSERTLOG`, so clicking an entry opens the assertion log file.
6. Recreate the menu after the menu bar is reloaded (for example after a preference
   change) and show any assertions already collected at that point.

The assertion source strings are runtime data; translation is disabled for the submenu.
Release builds compile the menu update as a no-op and never add the item.

## Risks

- The owner-drawn top-level item owns its own painting; it therefore does not pick up
  menu-bar theme gradients automatically. It uses `COLOR_MENUBAR` /
  `COLOR_HIGHLIGHT` and the system menu font.
- The popup can become very tall if hundreds of distinct assertion sites occur. Windows
  scrolls large menus; the list is not capped.
- Assertions that happen before the main window exists are collected but only shown when
  the menu bar is first initialised.
- `TDL_ASSERT_LOG=0` keeps the previous dialog-based workflow.

## Tests

- `Unicode Debug` solution builds and links `ToDoList.exe`.
- `Unicode Release` solution builds.
- `TDLTest` builds and all 310,436 tests pass after the change.
- Runtime check (temporary assertions removed afterwards):
  - Four temporary assertions (three at one line, one at another) produced no dialog.
  - The menu bar showed a red `Asserts (4)` immediately left of `Debug`.
  - Its submenu showed `ToDoListWnd.cpp(3385) [3x]` then
    `ToDoListWnd.cpp(3386) [1x]`, highest count first.
  - A build without assertions had no `Asserts` menu.
- Still to check manually: `TDL_ASSERT_LOG=0` restores the stock dialog with no
  `Asserts` menu, and clicking an entry opens the assertion log.
