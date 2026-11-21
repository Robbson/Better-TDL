# GEN-006: Default assertion logging

Status: **Done** (built; TDLTest passes). Assertion dialogs are now suppressed by
default; the visible summary is documented in [UI-022](../ui/UI-022-assert-menu.md).

## Goal

Make every CRT/MFC assertion in a Debug build leave a durable record without requiring
environment setup, and keep repeated assertions from blocking interactive runs.

## Upstream starting point

Upstream shows the MFC assertion dialog and writes nothing to disk, so the message, file
and line can only be read manually from the dialog (which has no copy function).

The fork previously added `TDL_ASSERT_LOG` support (see
[UI-001](../ui/UI-001-legacy-attribute-editor.md)): a `_CrtSetReportHook2` hook in
`Core/ToDoList/ToDoList.cpp` logged the assertion and stack trace, but only when the
variable was set, and it also suppressed the dialog.

## What changed

Files: `Core/ToDoList/ToDoList.cpp`.

1. `InitAssertLogging()` now always installs the report hook in Debug builds. Only
   `TDL_ASSERT_LOG=0` restores stock behaviour (dialog only, no log).
2. The default log remains `%TEMP%\ToDoList_asserts.log`; `TDL_ASSERT_LOG=<path>`
   redirects it.
3. `TDLAssertReportHook()` appends timestamp, process id, the assertion message and a
   `dbghelp`-resolved stack trace on every assertion.
4. After logging, the hook returns `TRUE` with a zero return value so the dialog and
   debug break are suppressed. Assertions are surfaced by the red `Asserts (N)` menu
   (see [UI-022](../ui/UI-022-assert-menu.md)).

`TDLTest` already logs by default to `%TEMP%\TDLTest_asserts.log` and is unchanged.

## Tools menu access

`Tools > View Assertion Log File` opens the same file in the default text editor,
immediately below `View Debug Log File`. It uses
`CToDoListApp::GetAssertLogPath()` (`ToDoList.cpp`) so it honours `TDL_ASSERT_LOG`,
including a custom path. The item is enabled only in Debug builds and only while the log
file exists; Release builds leave the menu entry greyed out.

## Risks

- Every assertion now writes to `%TEMP%`; repeated assertions grow the log until it is
  deleted. The file is plain text and only written in Debug builds.
- The first assertion initialises `dbghelp` symbols, adding a small one-off delay.
- If the log file cannot be opened, the assertion is still recorded in the `Asserts`
  menu but has no stack trace on disk.
- Suppressing dialogs means a Debug developer must watch the `Asserts` menu instead of
  being interrupted. `TDL_ASSERT_LOG=0` restores the stock dialog.
- Release builds are unaffected.

## Verification

- `Unicode Debug` solution builds (`ToDoList.exe` and `TDLTest.exe` link).
- `TDLTest` builds and all 310,436 tests pass.
- The Debug post-build step runs the unit test suite with no failures.
- Manual checks:
  - Launch a Debug build with `TDL_ASSERT_LOG` unset, trigger an assertion and confirm
    no dialog appears, the red `Asserts` menu appears before `Debug`, and the log file
    contains the entry.
  - Set `TDL_ASSERT_LOG=0` and confirm the dialog returns and no menu appears.
