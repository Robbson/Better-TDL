# Task Views ↔ Data Source Coupling — Alternative Sources and a Filesystem "Vault" View

> Independent analysis document. It does not replace [ARCHITECTURE.md](ARCHITECTURE.md) or
> [UI_DECOUPLING_ANALYSIS.md](UI_DECOUPLING_ANALYSIS.md).
>
> UI_DECOUPLING_ANALYSIS.md asks how much business logic is entangled with MFC windows and
> what a frontend replacement costs. This document asks a different question: **how tightly
> do the task views bind to the document data model, and can a view be fed from a different
> data source — for example a filesystem tree — without a major rework?**
>
> Scope: `Core/ToDoList`, `Core/Interfaces`, `Core/Shared` where relevant.

---

## 1. Where the views actually bind to the document

The views do not know about `CToDoCtrl`, the filesystem or `CTaskFile`. They receive the
document's parts as constructor references:

- `CTDLTaskCtrlBase(const CTDCImageList&, const CToDoCtrlData&, const CTDCStyleMap&,
  const TDCAUTOLISTDATA&, const CTDCColumnIDMap&, const CTDCCustomAttribDefinitionArray&,
  const CContentMgr&)` (`TDLTaskCtrlBase.h:74-82`), stored in `m_data`, `m_styles`,
  `m_tld`, `m_aCustomAttribDefs`, `m_mgrContent` (`TDLTaskCtrlBase.h:238-248`).
- Tree and flat list pass the same parts through (`TDLTaskTreeCtrl.h:42-50`,
  `TDLTaskListCtrl.h:51-59`).
- Across all task-view files the views touch only **22 distinct `CToDoCtrlData`
  accessors** (~30 call sites), dominated by `HasStyle`, `GetTrueTask`, `IsTaskReference`,
  `LocateTask` and a handful of formatting helpers. The data API they use
  (`ToDoCtrlData.h:61-160`) is read-only and value-based (`DWORD` task IDs,
  `TODOITEM*`/`TODOSTRUCTURE*`).
- The views never call `CToDoCtrl` methods; they notify their parent with
  `WM_TDCN_*` messages, which any host window can consume or ignore.
- Sorting/filtering/formatting/calculation are *not* behind the view either:
  `CTDCTaskComparer`, `CTDCTaskCalculator`, `CTDCTaskFormatter` and
  `CTDCTaskColumnSizer` all take `CToDoCtrlData` directly
  (`ToDoCtrlDataUtils.h:50,166,317,490`).

So the coupling is "concrete type" coupling, not "whole application" coupling.

---

## 2. A virtual document is already an intended shape

`CToDoCtrlData` is a self-contained in-memory document: a task map plus a hierarchy
(`ToDoCtrlData.h:43`, `:249-251`) built from a `CTaskFile` by
`BuildDataModel` (`ToDoCtrlData.h:59`). `CToDoCtrl::LoadTasks(const CTaskFile&)` is public
and accepts a tasklist that was never saved (`ToDoCtrl.h:732`); it calls
`SetAllTasks` → `BuildDataModel` → `RebuildTree`
(`ToDoCtrl.cpp:4664-4770`, `:7543-7557`). A `CTaskFile` can be populated in memory via
`ITaskList::NewTask`/`SetTaskTitle`/`SetTaskColor`/`SetTaskIcon`
(`ITaskList.h:115,160,168,325`) and `CTaskFile::SetCustomAttributeDefs`
(`TaskFile.h:159`).

A folder tree therefore maps cleanly onto the existing structures:

| Vault concept | Model mapping |
|---|---|
| Folder / file node | task (`TODOITEM` + `TODOSTRUCTURE` node) |
| Folder nesting | `TODOSTRUCTURE` parent/child |
| File name | task title |
| Size, modified, type, extension, open state | custom attributes / custom columns (`TDCC_CUSTOMCOLUMN_FIRST..LAST`, `TDCEnum.h:335-336`; types at `IEnums.h:222-231`) |
| "Open tasklist" badge | `TODOITEM.color` (`ToDoitem.h:279`) or `sIcon` (`ToDoitem.h:252`) |
| Read-only | tasklist style `TDCS_READONLY`, set with `CToDoCtrl::SetReadonly` (`ToDoCtrl.h:150-151`, `ToDoCtrl.cpp:4034-4040`) |

Read-only is already meaningful at the view level: inline editing, column click
handling and drag/drop consult `IsReadOnly()` (`TDLTaskCtrlBase.cpp:4675`,
`TDLTaskTreeCtrl.cpp:733,930,1455`). Selection, sorting, expanding, filtering and
colouring remain available, which is exactly the interaction a vault view needs.

---

## 3. Integration options, cheapest first

1. **Synthetic read-only document hosted by `CToDoCtrl` (recommended).**
   Build a `CTaskFile` from the folder tree, call `LoadTasks`, then `SetReadonly(TRUE)`.
   Give the control `SetAlternatePreferencesKey` (`ToDoCtrl.h:457`) so its column,
   expansion and sort state does not collide with the "Default" key used when no file
   path exists (`ToDoCtrl.cpp:8618-8640`). Clicking a `.tdl` node is just a callback into
   `CToDoListWnd::OpenTaskList` (`ToDoListWnd.h:714`).
   Core work is a folder scanner + tasklist builder. Shell work is limited to marking the
   tab as virtual/no-file: save prompts, auto-save, MRU, session restore and source
   control all key off `HasFilePath()`/`GetFilePath()`
   (`ToDoCtrlMgr.cpp:305-376,494-541`) and must short-circuit for this document.
2. **Standalone task view.** Instantiate `CTDLTaskTreeCtrl`/`CTDLTaskListCtrl` directly
   around a `CToDoCtrlData` plus style map, custom-column definitions, image list and
   content manager, in a window you own. No shell changes, but you supply the column
   header UI, selection plumbing and the activation gesture. This is the cleanest
   isolation if the vault is a separate window rather than a tasklist tab.
3. **Storage plugin.** Implement `ITasklistStorage::RetrieveTasklist`
   (`ITaskListStorage.h:133`) to return the synthetic tasklist. This reuses the existing
   "tasklist origin" abstraction (`TSM_TASKLISTINFO`) and keeps core untouched, but adds a
   DLL boundary and still lands in a normal `CToDoCtrl` document.

All three still use the real `CToDoCtrlData`; none needs an abstraction over the data
model.

---

## 4. When a real refactor would be required

If the goal becomes "the task grid is a general hierarchical grid over arbitrary data"
with a data-source interface (lazy children, change notifications, no `TODOITEM`), then
yes, that is a substantial change: introduce a document/view contract and port
`CTDLTaskCtrlBase` plus the comparer/calculator/formatter/sizer, because those bind to
`CToDoCtrlData` by type. That is on the scale of the UI rewrite described in
[UI_DECOUPLING_ANALYSIS.md](UI_DECOUPLING_ANALYSIS.md) §7, not a small adapter. For the
vault use case it is unnecessary.

---

## 5. Practical caveats

- **No change notifications from the model.** The vault document is a snapshot; a
  filesystem watcher must rebuild or patch it, then `LoadTasks`/`InvalidateTask` and
  re-apply per-node colours. There is no incremental virtual-row API in the view.
- **Custom attributes are document-scoped.** Metadata columns only exist when the
  generated `CTaskFile` defines them (`TaskFile.h:159`) and the user makes the columns
  visible; adding hard-coded columns would mean touching the fixed `TDC_COLUMN` enum and
  the formatting/export paths.
- **Task IDs must be stable and unique.** The model addresses rows by `DWORD` ID, and
  selection/sort state is ID-based. Derive IDs deterministically from paths (with a
  collision check) rather than from enumeration order.
- **The vault root is a document, not a folder.** Opening/closing tasklists elsewhere in
  the app must update the vault's row colours; that is app-level coordination, not a
  model feature.

---

## 6. Evidence index

| Claim | Reference |
|---|---|
| Views take document parts by reference | `TDLTaskCtrlBase.h:74-82,238-248`; `TDLTaskTreeCtrl.h:42-50`; `TDLTaskListCtrl.h:51-59` |
| Views use a narrow read-only data API | `ToDoCtrlData.h:61-160`; 22 distinct `m_data.<method>` names in `TDLTaskCtrlBase.cpp`, `TDLTaskTreeCtrl.cpp`, `TDLTaskListCtrl.cpp`, `TDLTaskAttributeListCtrl.cpp` |
| Semantics helpers take `CToDoCtrlData` | `ToDoCtrlDataUtils.h:50,166,317,490` |
| In-memory tasklist is a first-class input | `ToDoCtrl.h:732`; `ToDoCtrl.cpp:4664,7543-7557`; `ToDoCtrlData.h:59` |
| In-memory construction API | `ITaskList.h:115,160,168,325,363`; `TaskFile.h:159` |
| Read-only is a runtime style honoured by the view | `ToDoCtrl.h:150-151`; `ToDoCtrl.cpp:4034-4040`; `TDLTaskCtrlBase.cpp:4675`; `TDLTaskTreeCtrl.cpp:733,930,1455` |
| Per-item colour/icon and custom columns | `ToDoitem.h:252,279,289`; `TDCEnum.h:329,335-336`; `IEnums.h:222-231` |
| Custom window state key | `ToDoCtrl.h:457`; `ToDoCtrl.cpp:8618-8640` |
| File-open entry point for activation | `ToDoListWnd.h:714` |
| Shell assumes file paths in several places | `ToDoCtrlMgr.cpp:305-376,494-541` |
| Storage-plugin seam | `ITaskListStorage.h:133`; `TaskListStorageMgr.h:16` |

---

## 7. Implementation note

The recommended option was implemented as
[GEN-005](../customized/general/GEN-005-home-folder-view.md): a synthetic read-only
`CTaskFile` is loaded into a normal `CToDoCtrl` through
`CToDoCtrl::LoadVirtualTasks`, with the home folder stored per machine in
`[HomeFolders]`, file metadata in custom attributes, open tasklists coloured via
`TODOITEM.color`, and session/MRU paths stored relative to the home folder. No
data-source abstraction was added.

