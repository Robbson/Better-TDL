# GEN-009 — Plugin Manager view

**Status:** Proposed (design only; no implementation)

## Goal

Add a **Tools → Plugin Manager** command that opens a read-only virtual tasklist
listing every plugin DLL present in the application folder. Each row shows the
plugin title, enabled state, interface version, extension type, origin and file
name; the comments editor shows a longer description when one is available.
The `Enabled` column is the user-facing control that decides whether the plugin
is loaded on the next launch.

This is a counterpart to [GEN-005](GEN-005-home-folder-view.md): the Home view
proves that a synthetic `CTaskFile` can be loaded into a normal `CToDoCtrl`
without a new document format, and the same approach is used here.

## Upstream starting point

Upstream has no plugin management UI. Every plugin DLL in the executable folder
is discovered and loaded unconditionally the first time its manager is used:

- `CUIExtensionMgr::Initialize` (`Core/Interfaces/UIExtensionMgr.cpp:37-90`)
- `CContentMgr::Initialize` (`Core/Interfaces/ContentMgr.cpp:59-119`)
- `CImportExportMgr::Initialize` (`Core/Interfaces/ImportExportMgr.cpp:62-109`)
- `CTasklistStorageMgr::Initialize` (`Core/Interfaces/TasklistStorageMgr.cpp:209-248`)

All four scan `<exe folder>\*.dll` with `CFileFind`, skip directories and do not
recurse into subfolders. Each scan calls `LoadLibrary` on every file to test for
a factory export; a recognised plugin is deliberately left loaded and then
loaded a second time by the `Create...Interface` helper
(`Core/Interfaces/IUIExtension.h:54-108`, `IContentControl.h:107-123`,
`IImportExport.h:90-148`, `ITaskListStorage.h:49-98`). The two startup-relevant
managers are content (`CToDoListWnd::OnCreate` at
`Core/ToDoList/ToDoListWnd.cpp:1256`) and UI extensions (first tasklist created,
`CTabbedToDoCtrl.cpp:243-244`). Import/export and storage remain lazy until
their commands are used.

`EnableTDLExtension` in Preferences → General is unrelated: it registers the
`.tdl` file association (`PreferencesGenPage.cpp:299`,
`ToDoListWnd.cpp:3674`).

## Findings that constrain the design

### Metadata must not require loading the DLL

The current interface contracts expose only:

| Interface | Metadata |
|---|---|
| `IUIExtension` | menu text, icon, type ID, interface version |
| `IContent` | type ID, type description, icon, interface version |
| `IImportTasklist` / `IExportTasklist` | type ID, menu text, file extensions, version |
| `ITasklistStorage` | type ID, menu text, icon, version |

None of these expose author, origin or a long description, and all require
`LoadLibrary`. Probing a disabled plugin that way would run its `DllMain` and
defeat the purpose of disabling it.

The catalog therefore has to read metadata **passively from the file**:

- **Interface type**: parse the PE export directory for
  `CreateUIExtensionInterface`, `CreateContentInterface`,
  `CreateImportInterface`, `CreateExportInterface`,
  `CreateTasklistStorageInterface`. A DLL may export more than one, so the
  `Extension Type` cell can be a combined value.
- **Version**: the `VERSIONINFO` resource (`ProductVersion` / `FileVersion`).
  The shipped plugins already carry one (`Core/CalendarExt/CalendarExt.rc:93-120`,
  `Core/KanbanBoard/KanbanBoard.rc:30-55`); the contract version is only
  available by calling `GetInterfaceVersion`, so show the file version in the
  column and keep the ABI version as a cached diagnostic.
- **Title**: `FileDescription` from `VERSIONINFO`; for enabled plugins the live
  `GetMenuText()` value can be preferred and cached.
- **Long description**: `Comments` from `VERSIONINFO`, optionally overridden by
  a sidecar manifest (see below).
- **Origin**: not encoded in the binaries. Use a shipped manifest of core
  plugin file names (the `Core/*Ext`/`Core/KanbanBoard` projects); anything else
  is `External`. Do not infer origin from the file name at runtime.
- **Loaded**: optional; `CreateToolhelp32Snapshot(TH32CS_SNAPMODULE)` reports
  what is actually mapped in the process, so "present" and "loaded" can differ.

### A disabled plugin still needs a row

When the user disables a DLL and restarts, the app must still list it without
loading it. The catalog therefore needs a persisted metadata cache keyed by
file name plus size/mtime (or a hash). On a cache miss the row can still be
built from `VERSIONINFO` and the PE export table. A "Probe" action that calls
the real interface is optional and only allowed for enabled plugins.

### Unloading is unsafe

Interfaces are handed to live views and dialogs and are never `FreeLibrary`-ed
after creation. There is no partial manager refresh path. Version 1 therefore
defines toggles as **apply on restart**; the view shows which rows need a
restart. Live enable/disable is a later phase and would require closing all
dependent views, releasing the manager and re-initialising it.

### The virtual-view special cases are spread out

`IsHomeFolderView()` is checked in roughly thirty places in
`Core/ToDoList/ToDoListWnd.cpp` (for example `1618`, `2161`, `2680`, `3039`,
`4750`, `10372`, `14763`). A second virtual view must either update each site or
introduce a shared predicate such as `IsVirtualView()` /
`IsReadonlyVirtualView()` and migrate the Home checks to it. The latter is
preferred and should be part of the implementation, not an afterthought.

## Design

### Catalog

New `CTDLPluginCatalog` (suggested `Core/ToDoList/TDLPluginCatalog.h/.cpp`):

- Enumerates `<exe folder>\*.dll` once, non-recursively (same scope as the four
  managers today).
- For each file, records: file name, full path, size, last-write time, PE export
  names, `VERSIONINFO` fields, derived interface types, enabled state and cached
  display metadata.
- Classifies rows as `UI Extension`, `Content`, `Import/Export`, `Storage` or
  `Unknown`; `Unknown` DLLs are listed but cannot be enabled.
- Is the single source of truth for the four managers. This replaces four
  independent directory scans with one and gives the view a consistent snapshot.

### Preferences

Stored in `ToDoList.ini` under a new section, following the quote-safe conventions
of [GEN-002](GEN-002-quoted-ini-values.md). Identity is the DLL file name;
size and mtime are stored alongside so a replaced DLL can be detected:

```ini
[Plugins]
DisabledCount=2
Disabled0=EisenhowerUIExtensionBridge.dll
Disabled1=OldImporter.dll
```

If a disabled file changes (size/mtime differ), re-enable it and log a notice;
otherwise a fixed plugin would stay disabled silently. Enablement is intended to
be per installation. If machine-specific state is required, reuse the machine-key
pattern from [GEN-005](GEN-005-home-folder-view.md).

### Managers consult the catalog

Each manager's `Initialize()` obtains the catalog, skips entries whose enabled
state is false, and asks it for the already-classified type instead of calling
`Is...Dll` on every file. The catalog itself must not call `LoadLibrary` for
files it classifies; only enabled plugins reach `Create...Interface`.

Ordering: the catalog is built before any manager initializes. The content
manager still initializes early from `OnCreate`; the UI-extension manager from
the first tasklist. Both are after preference loading, so the disabled list is
available.

### Virtual document

`CTDLPluginCatalog::BuildTaskFile` produces a flat `CTaskFile` like
`CTDLHomeFolder::BuildTaskFile`:

| Column | Model |
|---|---|
| Title | `FileDescription`, or live `GetMenuText()` for enabled plugins |
| Enabled | `TDCCA_BOOL` custom attribute `Plugin.Enabled`; renders as a checkbox |
| Version | custom string attribute `Plugin.Version` |
| Type | custom string attribute `Plugin.Type` (`UI Extension`, `Content`, `Import/Export`, `Storage`, combinations) |
| Origin | custom string attribute `Plugin.Origin` (`Core`, `External`) |
| File Name | custom string attribute `Plugin.FileName` |
| Loaded | optional `TDCCA_BOOL` custom attribute `Plugin.Loaded` |
| Long description | task comments |

`TDCCA_BOOL` columns already render as checkboxes
(`Core/ToDoList/TDLTaskCtrlBase.cpp:3372-3379`) and are directly editable
(`TDLTaskCtrlBase.cpp:4817-4818`), so no new column type is needed. The
`Plugin.Enabled` edit must be intercepted: persist the preference, revert the
in-memory virtual value if necessary, and mark the row as restart-required. The
view otherwise stays read-only (`SetReadonly(TRUE)`, drag/drop disabled) like
Home. If intercepting the edit is fragile, expose **Enable/Disable** in the
context menu and treat a click on the checkbox as the same command.

The view uses its own alternate preferences key (`FileStates\PluginManager`) so
column order, widths and sorting do not collide with Home or real tasklists.
`ReloadState()` is called once after the initial column set, matching
`InitHomeFolderView` (`ToDoListWnd.cpp:3007-3028`).

### Tools menu and lifecycle

- New command `ID_TOOLS_PLUGINMANAGER`, appended after the existing Tools items
  (do not renumber existing IDs); handler mirrors `OnFileHome`
  (`ToDoListWnd.cpp:2939-3005`): find an existing view, refresh it, otherwise
  create and add it as a normal `CFilteredToDoCtrl` with no file path.
- Menu text and any notice strings are added to `ToDoList.rc` / `resource.h`.
- The view is excluded from session restore and from save prompts, and it is not
  registered with the time tracker or reminders. This requires the shared
  virtual-view predicate described above.
- Refresh is explicit (command again) and lazy after a toggle. There is no
  filesystem watcher; the catalog snapshot is rebuilt when the view opens.

## Phases

| Phase | Scope | Risk |
|---|---|---|
| 1 | `CTDLPluginCatalog` with passive PE/`VERSIONINFO` parsing; Tools → Plugin Manager read-only view; columns and comments | Low; no loading changes |
| 2 | Persisted `DisabledPlugins`; all four managers filter before loading; Enabled checkbox with restart-required semantics | Medium; touches plugin startup |
| 3 | Live enable; optional live disable by closing dependent views and re-initialising managers | High; interface lifetime |

Phase 1 alone delivers the inventory and is independently useful.

## Files (expected)

- `Core/ToDoList/TDLPluginCatalog.h/.cpp` — scan, PE export parsing,
  `VERSIONINFO` reading, metadata cache, `BuildTaskFile`.
- `Core/ToDoList/ToDoListWnd.cpp/.h` — command, virtual-view lifecycle,
  refresh, shared virtual-view predicate.
- `Core/ToDoList/ToDoCtrl.h/.cpp` — `IsPluginManagerView` (or the generalized
  predicate).
- `Core/Interfaces/*Mgr.cpp` — skip disabled entries and use the catalog.
- `Core/Interfaces/IPluginInfo.h` (optional) — shared catalog types if the
  managers need to consume them from `Core/Interfaces`.
- `Core/ToDoList/ToDoList.rc`, `resource.h` — command, menu item, strings.
- `Core/ToDoListLib/ToDoListLib.vcxproj(.filters)` — new sources.

No change to `TaskFile`, `TasklistSchemaDef`, `TDC_ATTRIBUTE` or `TDC_COLUMN`
order: the plugin columns are app-owned custom attributes like Home's.

## Manual checks

- Tools → Plugin Manager opens a single tab with one row per DLL in the exe
  folder, including DLLs that are currently disabled.
- A disabled plugin's row appears after restart without the DLL being loaded
  (verify with Process Explorer / module list).
- Title, version, type, origin and file name match the file's `VERSIONINFO` and
  exports; an `Unknown` DLL is listed but cannot be enabled.
- The Enabled checkbox writes `[Plugins]` and shows a restart-required state.
- Restarting honours the disabled list; re-enabling restores the plugin and its
  UI entry points.
- Replacing a disabled DLL (different size/mtime) re-enables it and logs why.
- Home and Plugin Manager can be open at the same time, have independent column
  configuration, and neither prompts to save on close.
- Closing the app with the Plugin Manager open does not prompt to save and does
  not assert.
- Existing startup remains unchanged when no `[Plugins]` section exists.

## Risks / limitations

- Origin is a curated classification, not intrinsic metadata; a renamed core
  DLL would be reported as external unless the manifest is updated.
- A long description is only as good as the `VERSIONINFO` `Comments` field or
  an optional sidecar manifest; many third-party plugins will have little text.
- DLLs can export more than one factory; the view is per file, so type is a
  combined value.
- Disabling cannot rescue a plugin that crashes during `DllMain` before the user
  can reach the UI; a command-line safe mode or last-startup-crash detection is
  out of scope for this change.
- The catalog must parse untrusted PE files. Parsing must be bounds-checked and
  must never execute code; malformed files are reported as `Unknown`.
