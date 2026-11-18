# GEN-005 — Home Folder view

**Status:** Done (built; TDLTest passes; basic open/activate/close flow manually verified)

## Goal

Give the application a browser-style "home" location: a read-only tree of a
user-selected folder, showing sub-folders and files. `.tdl` rows open as tasklists;
other files open with their default application. Opened tasklists are highlighted.
The home folder is configurable in Preferences and stored per machine so a shared
`ToDoList.ini` can be used across machines.

This implements option 1 from
[../../docs/DATA_SOURCE_COUPLING_ANALYSIS.md](../../docs/DATA_SOURCE_COUPLING_ANALYSIS.md)
§3: a synthetic read-only `CTaskFile` loaded into a normal `CToDoCtrl`. No data-model
abstraction or view rewrite was required.

## Upstream starting point

Upstream has no equivalent. The feature is additive and uses existing documented
extension points.

## Implementation

### Home folder setting

- New toolbar command `ID_FILE_HOME` (34810), button after the navigation forward
  arrow in `MAIN_TOOLBAR_ITEMS` using task icon `86` (`TDLToolbar.cpp`); the File menu
  also has "Open Home Folder".
- Preferences → General has a "Home Folder" row immediately before Display Language,
  using `CFileEdit(FES_FOLDERS)` (same control as the backup folder picker).
- Stored in `ToDoList.ini` as:

  ```ini
  [HomeFolders]
  ROBIN-PC="C:\Users\Robin\Documents\Home"
  ```

  The key is the computer name sanitised to `[A-Za-z0-9_.-]`; the value is the folder.
  Later sessions on another machine use that machine's key.

### Virtual document

`CTDLHomeFolder::BuildTaskFile` scans the folder recursively (folders first, then files,
alphabetical, hidden/system/reparse-point entries skipped) and fills a `CTaskFile`:

| Vault concept | Model |
|---|---|
| Folder / file | task (`TODOITEM` + `TODOSTRUCTURE`) |
| Name | task title, stable sequential `DWORD` task ID (not shown) |
| Last modified | built-in `TDCA_LASTMODDATE` |
| Size | custom attribute `HomeFolder.Size` (integer, bytes); blank for folders |
| Type | custom attribute `HomeFolder.Type` (extension) |
| Path | custom attribute `HomeFolder.Path`; blank for home entries, reserved for future external files |
| Absolute path | disabled custom attribute `HomeFolder.FullPath` (internal, no column) |

Visible columns are fixed to Title, Last Modified, Size, Type and Path. `Size` is an
integer and has no value for folder rows. The internal absolute path is stored in a
**disabled** custom attribute so it does not appear as a column or editor field.

Row colouring is text-only and mirrors the MRU tasklist switcher:

| State | Text colour |
|---|---|
| Closed file | `COLOR_GRAYTEXT` |
| Open, unmodified tasklist | default window text |
| Open, modified and unsaved tasklist | `COLOR_HOTLIGHT` |
| Folder | default window text |

`CToDoListWnd::OnFileHome` creates or refreshes a normal `CFilteredToDoCtrl` with no file
path, loads the generated `CTaskFile` through the new public
`CToDoCtrl::LoadVirtualTasks`, calls `SetReadonly(TRUE)`, clears the column configuration
down to the fixed set above, forces text-only per-task colours, and adds it as a tab. The
tab is marked with `CToDoCtrl::IsHomeFolderView()`; `IsPristine()` returns false for it so
it can coexist with an empty untitled tasklist, and it is excluded from session save and
from time-tracker/reminder registration. The Home view is restricted to Task Tree and
List View; attempts to activate any other task view are blocked in
`CTabbedToDoCtrl::OnPreTabViewChange` and show a notice.

### Activation

Double-clicking a read-only task with a `HomeFolder.FullPath` value sends the new
`WM_TDCN_ACTIVATETASK` notification. The shell opens `.tdl` files with the existing
`CToDoListWnd::OpenTaskList` path and refreshes the home view; other files are launched
through `FileMisc::Run`.

### Relative paths

When a tasklist is inside the home folder, session (`[Settings] LastFileN`,
`LastActiveFile`) and MRU (`[MRU]`) entries are stored relative to it, e.g.
`.\sample.tdl`. On load, relative paths are resolved against the home folder when the
file exists there, otherwise against the application folder (preserving old sessions).

## Files

- `Core/ToDoList/TDLHomeFolder.h/.cpp` — setting access, path helpers, folder scanner.
- `Core/ToDoList/ToDoListWnd.cpp/.h` — command, virtual-document lifecycle, activation,
  session/MRU relative paths.
- `Core/ToDoList/ToDoCtrl.cpp/.h` — `IsHomeFolderView`, `LoadVirtualTasks`,
  `GetTaskCustomAttributeData`, pristine override.
- `Core/ToDoList/PreferencesGenPage.cpp/.h` — Home Folder preference and browse control.
- `Core/ToDoList/TDLToolbar.cpp` — main-toolbar button.
- `Core/ToDoList/ToDoList.rc`, `resource.h` — command, dialog row, menu, tooltip string.
- `Core/ToDoList/TDCMsg.h` — `WM_TDCN_ACTIVATETASK`, `WM_TDCN_VIEWUNSUPPORTED`,
  home-folder attribute IDs.
- `Core/ToDoList/TDLTaskTreeCtrl.cpp` — read-only double-click activation.
- `Core/Shared/FileMisc.cpp/.h` — `IsPathInFolder`.
- `Core/Shared/EnRecentFileList.cpp/.h` — optional home-relative MRU storage.
- `Core/ToDoListLib/ToDoListLib.vcxproj(.filters)` — new source files.

## Manual checks

- Toolbar Home button opens a Home tab containing the folder tree.
- A `.tdl` row opens the tasklist as a document and changes from grey (closed) to the
  default text colour (open) or hotlight (modified, unsaved).
- Attempting to switch Home to Graphs/Calendar/Gantt/Kanban/Workload shows a notice and
  stays in the task view.
- `[HomeFolders]`, `LastFileN`, `LastActiveFile` and `MRU` use the machine key and
  home-relative paths.
- Preferences shows the configured folder and closes/saves without assertions.
- Closing the app with Home open does not prompt to save and does not assert.

## Risks / limitations

- The home view has its own preferences key (`FileStates\HomeFolder`) and a fixed column
  set, so it does not share column configuration with real tasklists.
- The view is a snapshot. Filesystem changes are picked up when Home is invoked again
  or after opening a tasklist from Home; there is no filesystem watcher.
- The scanner skips hidden/system entries and reparse points (avoids `.git`, loops).
- The home document is not restored by session restore; only real tasklists are.
- The internal absolute path is a disabled custom attribute rather than task metadata
  because the tasklist import path does not import task metadata.
