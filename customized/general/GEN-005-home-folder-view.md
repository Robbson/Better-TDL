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
| Last modified | built-in `TDCA_LASTMODDATE` (Unix `time64_t`, converted from `FILETIME`) |
| Size | custom attribute `HomeFolder.Size` (integer, bytes); blank for folders |
| Type | custom attribute `HomeFolder.Type` (extension) |
| Status | custom attribute `HomeFolder.Status`: `Opened`, `Modified` or blank when closed |
| Content preview | text files up to 128 KB are loaded into the task comments |
| Path | custom attribute `HomeFolder.Path`; blank for home entries, reserved for future external files |
| Absolute path | disabled custom attribute `HomeFolder.FullPath` (internal, no column) |
| `.tdl` icon | `tdl` (open) or `tdlclosed` (pale, not open), mapped to `IDR_MAINFRAME` |
| text icon | task icon `93` for `.txt`, `.md`, `.log`, `.ini`, `.csv`, `.json`, `.xml`, `.yml`, `.yaml` |

Visible columns are fixed to Title, Last Modified, Size, Type, Status and Path. `Size` is
an integer and has no value for folder rows. The Status column is sortable and shows
`Opened`, `Modified` or nothing for closed files. The internal absolute path is stored in
a **disabled** custom attribute so it does not appear as a column or editor field. `.tdl`
rows use the application icon; closed tasklists use a 50%-alpha pale copy, while open
tasklists use the normal icon. Text files use task icon 93 and display their content in
the comments control (up to 128 KB); BOM/UTF-8 is auto-detected, and files that fail UTF-8
conversion are retried as ANSI. Top-level rows remain bold because the tree renderer
always bolds tasks whose parent is the root (`TDLTaskCtrlBase.cpp:2368`); this is existing
upstream behaviour, not a Home setting.

Row colouring is text-only and mirrors the MRU tasklist switcher, except that closed
files use a lighter gray (`RGB(150,150,150)`) so they remain distinguishable from bold
open titles:

| State | Text colour |
|---|---|
| Closed file | `RGB(150,150,150)` |
| Open, unmodified tasklist | default window text |
| Open, modified and unsaved tasklist | `COLOR_HOTLIGHT` |
| Folder | default window text |

`CToDoListWnd::OnFileHome` creates or refreshes a normal `CFilteredToDoCtrl` with no file
path, loads the generated `CTaskFile` through the new public
`CToDoCtrl::LoadVirtualTasks`, calls `SetReadonly(TRUE)`, clears the column configuration
down to the fixed set above, forces text-only per-task colours, and adds it as a tab. The
tab is marked with `CToDoCtrl::IsHomeFolderView()`; `IsPristine()` returns false for it so
it can coexist with an empty untitled tasklist, and it is excluded from session save and
from time-tracker/reminder registration. Drag/drop is explicitly disabled
(`CToDoCtrl::SetDragDropEnabled(FALSE)`) because reordering a generated snapshot could
not be persisted, and the OLE drag handlers return early for read-only tasklists so an
external drag cannot assert. The Home view is restricted to Task Tree and List View;
attempts to activate any other task view are blocked in
`CTabbedToDoCtrl::OnPreTabViewChange` and show a notice.

The filter bar hides the Show and Options combos while Home is active
(`CTDLFilterBar::SetHomeFolderMode`) and leaves the title/date/attribute filters
available. Because Home is not registered with the time tracker, idle-time time-tracker
updates are skipped for it, which also avoids the assertion that previously occurred when
filtering.

The virtual Home document uses its own `FileStates\HomeFolder` preference key, so an
active filter would otherwise be restored on the next creation and make the view look
empty. A newly created Home document therefore calls `ClearFilter()` once after loading;
the filter bar remains usable for the current session.

Home status is refreshed lazily. Opening, closing or modifying a tasklist sets
`m_bHomeFolderNeedsRefresh`; if Home is not active it is rebuilt when the user next
selects the Home tab. This avoids reloading the virtual document while it is hidden and
keeps multiple-open and edited-document states correct.

Home is omitted from the Window menu (commands still map by manager index); the tab
remains because it is the only way to navigate back to the virtual document.

### Activation

Double-clicking or pressing **Enter** on a read-only task with a `HomeFolder.FullPath`
value sends the new `WM_TDCN_ACTIVATETASK` notification. The shell opens `.tdl` files
with the existing `CToDoListWnd::OpenTaskList` path and refreshes the home view; other
files are launched through `FileMisc::Run`. The Home task context menu is replaced with
**Open**, **Open & Replace**, **Close** (enabled when any selected `.tdl` is already open)
and **Show in Explorer**; Open handles all selected items, and opening a folder
recursively opens every `.tdl` beneath it. Open & Replace first closes every open
non-home tasklist (prompting to save modified documents) and then opens the selection.
`Tools → Select in Explorer` also uses the selected Home item.

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
- `Core/ToDoList/TDLTaskTreeCtrl.cpp` — read-only double-click and Enter activation.
- `Core/ToDoList/TDCImageList.cpp/.h` — 32-bit alpha application icon for `.tdl` files.
- `Core/Shared/FileMisc.cpp/.h` — `IsPathInFolder`; Unix-based `GetFileLastModified`.
- `Core/Shared/EnRecentFileList.cpp/.h` — optional home-relative MRU storage.
- `Core/ToDoListLib/ToDoListLib.vcxproj(.filters)` — new source files.

## Manual checks

- Toolbar Home button opens a Home tab containing the folder tree.
- A `.tdl` row opens the tasklist as a document and changes from grey (closed) to the
  default text colour (open) or hotlight (modified, unsaved).
- Attempting to switch Home to Graphs/Calendar/Gantt/Kanban/Workload shows a notice and
  stays in the task view.
- While Home is active, the filter bar shows no Show or Options combos; entering a title
  filter does not assert and the field remains editable.
- Idle processing with Home active produces no time-tracker assertion.
- Dragging a Home row does not start a drag operation and does not assert.
- Last-modified dates match the filesystem dates (not 1601-based values).
- `.tdl` rows show a smooth 50%-alpha application icon when closed and the normal icon
  when open; text files show icon 93 and their content appears in the comments control.
- ANSI text files with non-ASCII characters display correctly (UTF-8 first, ANSI retry).
- Right-clicking a Home row shows Open, Open & Replace, Close and Show in Explorer; Open
  handles a multiple selection and recursively opens all `.tdl` files in a selected
  folder.
- Opening/closing/editing tasklists updates the Home Status column when Home is next
  shown; multiple opens are reflected too.
- Home is absent from the Window menu but remains available as a tab.
- Pressing Enter on a selected Home row opens it like double-click.
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
