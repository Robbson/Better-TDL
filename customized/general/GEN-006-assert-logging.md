# GEN-006: Default assertion logging

Status: **In progress** (implemented; TDLTest passes; app link pending because a Debug
instance is running)

## Goal

Make every CRT/MFC assertion in a Debug build leave a durable record without requiring
environment setup, while keeping the stock assertion dialog so interactive runs still
notice a failure.

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
4. After logging, the hook returns `FALSE` so the normal assertion dialog/debug break
   proceeds. When `TDL_ASSERT_LOG` is set to `1` or a path, the previous
   log-and-suppress behaviour is kept for unattended runs.

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
- If the log file cannot be opened, the hook falls back to the dialog, so no assertion
  is silently lost.
- Release builds are unaffected.

## Verification

- `Unicode Debug` compilation of `Core/ToDoList/ToDoList.cpp` succeeds.
- `ToDoListLib` (including `ToDoListWnd.cpp` and the new menu handler) compiles.
- `TDLTest` builds and all 310,419 tests pass.
- The full solution link could not finish because
  `Core/ToDoList/Unicode_Debug/ToDoList.exe` is currently running from an interactive
  session; the linker reported `LNK1168`. That instance was not started by the agent and
  was left running. Re-run the build after closing it.
- Manual checks still to do:
  - Launch a Debug build with `TDL_ASSERT_LOG` unset, trigger an assertion and confirm
    the dialog still appears and `%TEMP%\ToDoList_asserts.log` contains the entry.
  - Repeat with `TDL_ASSERT_LOG=1` and confirm the dialog is suppressed.
  - Set `TDL_ASSERT_LOG=0` and confirm no log entry is written.
