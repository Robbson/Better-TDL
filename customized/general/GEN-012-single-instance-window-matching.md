# GEN-012: Single-instance detection must not match foreign windows

Status: **Done**

## Goal

Stop the application from exiting silently at startup when any unrelated window has a
caption ending with the product name (for example a file manager showing a folder called
`Better TDL`). With the default single-instance setting the app treated that window as a
running instance, handed it the startup options and quit.

## Upstream starting point

`CToDoListWnd::FindOtherInstance()` (`ToDoListWnd.cpp`) enumerated top-level windows and
accepted every window whose caption ended with the product name:

```cpp
static CString COPYRIGHT(GetAppCopyright());
...
if (Misc::HasSuffix(COPYRIGHT, sCaption))
```

Upstream provides no explicit single-instance flag, so when the setting `MultiInstance`
is `0`/absent (`CToDoListApp::ProcessStartupOptions`, `ToDoList.cpp:477-505`) the new
process forwards its startup options to the "other instance" and returns `TRUE`, which
makes `InitInstance()` quit with exit code 0.

This was harmless while the product name was `ToDoList`, because a window whose title
ends in `ToDoList` is almost always a ToDoList frame. After [GEN-001](GEN-001-branding-and-about.md)
renamed the product to `Better TDL`, any window titled with a folder of the same name
matches. Reproduced on 2026-11-21 with the unpacked bundle in `E:\Office\Better TDL`
while a file-manager window titled `Better TDL` was open:

```text
CToDoListApp::ProcessStartupOptions found 1 other instances
CToDoListApp::ProcessStartupOptions found no other instance with '' already loaded
```

and the process exited with code 0 without showing a window.

## What changed

File: `Core/ToDoList/ToDoListWnd.cpp` (`FindOtherInstance`).

After the caption suffix check, the window is accepted only if either:

1. its class is `ToDoListFrame` (the frame class registered by
   `CToDoListWnd::Create`), or
2. its owning process image name matches our own executable (queried with
   `FileMisc::GetWindowModuleFilePath` + `FileMisc::IsSamePath(..., TRUE)`).

Otherwise enumeration continues. The class check works across elevation because
`GetClassName` needs no process access, while the module-name check keeps hand-off
working if a historical build used a different frame class.

## Risks

- If a future build renames the frame class and the other instance runs elevated (so its
  module path cannot be read), hand-off to it is skipped and a second instance starts.
  The class name has been stable upstream for many versions.
- No behaviour change for genuine ToDoList windows: both checks accept them.

## Verification

- Reproduced the silent exit with the shipped `ToDoList.ini` (`MultiInstance=0`) and a
  file-manager window titled `Better TDL`.
- `Unicode Debug` solution build succeeds.
- Manual check: with the same ini and window open, the app now opens its main window
  instead of exiting, and a second launch still hands off to the first instance.
