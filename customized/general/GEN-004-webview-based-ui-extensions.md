# GEN-004 — Web-technology extensions: WebView2 feasibility for task views and text editors

| Field | Value |
|---|---|
| Status | Proposed (feasibility study and design; no implementation) |
| Upstream baseline | ToDoList 9.3.DP7 (`custom` @ `63fc1aacc`) |
| Scope | Extension host only: `Core/Interfaces` (`IUIExtension`, `IContentControl`) and one new adapter DLL. Core task views, data model and persistence unchanged. |
| Default | Nothing changes for existing builds, plugins or file formats; an adapter DLL is optional and discovered like any other plugin |
| Related | [index.md](../index.md), [../../docs/UI_DECOUPLING_ANALYSIS.md](../../docs/UI_DECOUPLING_ANALYSIS.md), [GEN-003](GEN-003-ui-automation-framework.md), [TED-001](../text-editor/TED-001-comments-toolbar-in-main-toolbar.md) |

## Questions answered

1. Can a current Edge WebView (Chromium) be used as a task view, or does the app still
   force the legacy IE control?
2. Where does such a view get its data: local files, the plugin DLL, or an internally
   started HTTP server?
3. Can the memory footprint stay low with a light web framework (no Electron/Node)?
4. Can the app's message-based input/notification model be bridged to and from the web
   view, including the reverse direction (web events into the app)?
5. Is the same approach suitable for an alternative comments editor (CodeMirror,
   markdown editors), and is that bridge simpler?

Short answers:

| Question | Answer |
|---|---|
| Edge WebView2 allowed? | Yes. Nothing in the extension architecture requires MSHTML. The IE control is used only by two built-in dialogs, not by the plugin host. A plugin DLL may embed WebView2 itself. |
| Data source | Neither file nor HTTP is required. Tasks are handed to the plugin as an in-memory `ITaskList` (`CTaskFile`), exactly like any other view plugin. Static UI assets come from the plugin's own folder (packaged build) or a dev server in development only. |
| Low memory? | Yes in principle. A web view is ~50–150 MB idle versus 300+ MB for an Electron app, because WebView2 is shared with the OS. Still, a second Chromium process model per tasklist is not free; it must be created lazily, one per tasklist at most, and sized realistically. |
| Messages both directions? | Yes, but not by forwarding `HWND` messages into the browser. Encode the semantics of the existing messages (selection, modify, sort, focus, context menu) as a typed JSON/RPC contract: native→web as push and request/response, web→native as requests that either call the existing `IUIExtensionWindow` methods or post the existing registered `WM_IUI_*` messages back to the tasklist window. |
| Text editor | Yes and it is the easier of the two. The comments editor is already a swappable content plugin (`IContentControl`); its contract is small and value-based, so the bridge is a handful of methods plus one change notification. |

## 1. How extensions are loaded today

Two independent plugin families are relevant:

| Family | Interface | Export | Host discovery | Host context |
|---|---|---|---|---|
| Task view | `IUIExtension` / `IUIExtensionWindow` (`Core/Interfaces/IUIExtension.h:42-131`, `:348-381`) | `CreateUIExtensionInterface` + `GetInterfaceVersion` (`IUIExtension.h:43-46`) | every `*.dll` next to the exe (`Core/Interfaces/UIExtensionMgr.cpp:51-105`) | embedded as a task-view tab in `CTabbedToDoCtrl` |
| Content / comments editor | `IContent` / `IContentControl` (`Core/Interfaces/IContentControl.h:127-187`) | `CreateContentInterface` + `GetInterfaceVersion` (`IContentControl.h:52-55`) | every `*.dll` next to the module (`Core/Interfaces/ContentMgr.cpp:73-120`) | the comments pane of `CToDoCtrl`, chosen per task by comments format |

Both are plain C++ COM-style interfaces (`Release()` + virtuals), versioned by an integer,
and both are "discovered by probing every DLL for the factory export"
(`IsUIExtensionDll` / `IsContentDll`, `IUIExtension.h:92-108`, `IContentControl.h:107-123`).
Neither interface mentions HTML, browsers, URLs or files: the host contract is pure Win32
plus the task model. **The choice of rendering technology inside the DLL is entirely the
plugin's.**

### 1.1 Task view plugin contract in practice

`IUIExtensionWindow` gives the plugin:

- its own child `HWND` (`CreateExtWindow(..., HWND hwndParent)`, `IUIExtension.h:123-125`;
  created in `CTabbedToDoCtrl.cpp:870-935` with `WS_CHILD`);
- task data as an `ITaskList*` on every update: `UpdateTasks(const ITaskList*, IUI_UPDATETYPE)`
  (`IUIExtension.h:358`), typed `IUI_EDIT` / `IUI_NEW` / `IUI_DELETE`
  (`IUIExtension.h:135-144`);
- a declared interest in attributes: `WantTaskUpdate(TDC_ATTRIBUTE)` (`IUIExtension.h:359`);
  the host collects all views' interests and builds a filtered `CTaskFile` for each update
  (`GetAllExtensionViewsWantedAttributes`, `TabbedToDoCtrl.cpp:3856`, and the delivery
  path `UpdateExtensionView`, `TabbedToDoCtrl.cpp:1394-1398`);
- input plumbing: `ProcessMessage(MSG*)`, `FilterToolTipMessage(MSG*)`,
  `DoIdleProcessing()` (`IUIExtension.h:362-364`; called from `TabbedToDoCtrl.cpp:434-446`
  and `:1163-1205`);
- host commands in: `DoAppCommand(IUI_APPCOMMAND, ...)` / `CanDoAppCommand`
  (`IUIExtension.h:366-367`, enum `:146-178`: expand/collapse, sort, focus, select
  next/prev, scroll-to-selected, save-to-image, move task);
- geometry for in-place editing and hit testing: `GetLabelEditRect`, `HitTest`,
  `ShowContextMenu` (`IUIExtension.h:369-371`);
- chrome: `SetUITheme`, `SetTaskFont`, `SetReadOnly` (`IUIExtension.h:373-377`);
- and it sends host commands back as **registered window messages** (`WM_IUI_*`,
  `IUIExtension.h:312-344`), handled by `CTabbedToDoCtrl` (`TabbedToDoCtrl.cpp:180-190`).

Selection in the other direction arrives through the same registered messages:
`WM_IUI_SELECTTASK` with `wParam == 0, lParam = TaskID`, or `wParam = LPDWORD,
lParam = count` for a multi-selection (`IUIExtension.h:312-314`).

### 1.2 The task model handed to plugins

`UpdateTasks` receives a `const ITaskList*`. In practice this is the in-memory `CTaskFile`
used for all tasklist I/O (`Core/ToDoList/TaskFile.h:86`), which implements the versioned
`ITaskList` ladder `ITaskList … ITaskList18` (`Core/Interfaces/ITaskList.h:16-34`).
Whether the view participates in selection is declared separately by the extension via
`SupportsTaskSelection` (`IUIExtension.h:130`). It is a tree of `HTASKITEM` handles with getters and setters for
every attribute, custom attributes and metadata (`ITaskList.h:97-330`). Attributes that
were not requested are omitted and flagged by `SetAvailableAttributes`
(`TaskFile.h:177`, set e.g. `TabbedToDoCtrl.cpp:4005`).

**Nothing about this model is file- or network-bound.** A web view can be fed from it
directly (section 3).

## 2. Browser options inside the DLL

### 2.1 What the app itself uses today (and why it does not constrain plugins)

The application has exactly one browser host: `CWebBrowserCtrl`, a Visual C++ generated
wrapper for the **legacy Microsoft Web Browser control** (`CLSID_8856F961-340A-11D0-A96B-00C04FD705A2`,
i.e. Internet Explorer / MSHTML; `Core/Shared/WebBrowserCtrl.h:15-35`, implementation in
`Core/Shared/WebBrowserCtrl.cpp`). It powers:

- `CTDLBrowserDlg`, used to display HTML help and tasklist HTML export in a window, with
  task-link interception (`Core/Shared/BrowserDlg.cpp:68-100, 135-194`;
  `Core/ToDoList/TDLBrowserDlg.cpp:24-47`);
- IE print / print-preview handling including IE-specific dark-mode fixes
  (`Core/Shared/EnBrowserCtrl.cpp:36, 88-120`; `Core/Shared/DarkMode.cpp:395-410,
  1034-1076`).

The HTML export writes an `X-UA-Compatible IE=edge` meta tag
(`Core/ToDoList/TaskListHtmlExporter.cpp:69`) to force the best available IE document
mode inside that control.

None of this is part of the extension host. `IUIExtension`/`IContentControl` never
mention a browser, and the comments HTML plugin comment in `ToDoCtrl.cpp:1030-1042`
shows the historic plugin already embedded its own browser instance inside a plugin DLL.

### 2.2 Options for a plugin

| Option | Modern engine | Notes | Verdict |
|---|---|---|---|
| **WebView2 (Edge Chromium)** | Yes, evergreen | `ICoreWebView2` COM API; shipped-in Windows 11 and as Evergreen Runtime on Windows 10/8.1/7 (as of 2024 no longer supported on 7/8.1); can also be flown in "fixed version". Airspace problem: it is a HWND-less composition surface (`CoreWebView2Controller` parented to an `HWND`) that stays above parent draws and does not clip to `WS_CLIPCHILDREN` siblings. | **Recommended** |
| Legacy `WebBrowser` control / MSHTML | No (IE11 max) | Same control the app uses; `IWebBrowser2` + `WebBrowser_V1/V2` event sinks; `document.parentWindow.external` for host calls; `window.external` host objects. Works with no runtime install, but ES5-era JS, no modules, no modern framework, no WASM-friendly tooling. | Fallback only |
| CEF (Chromium Embedded) | Yes | Full control over Chromium version; huge binary footprint (tens of MB, ~100 MB unpacked) and per-process model; overkill when the OS already ships Chromium. | Not recommended |
| System `msedge.exe` via `--app` window | Yes | Separate top-level window, no embedding, no shared process with the app; cannot be a child view. | Rejected |

WebView2 is only an option if the plugin may use a runtime that is present on the OS or
installed alongside the app. On Windows 11 it is present; on Windows 10 the Evergreen
Runtime is normally installed by Edge itself but must be verified, and the plugin must
degrade gracefully (show its own "install WebView2" page in a plain window, or fall back
to the IE control) when it is missing. The plugin must not require Administrator rights.

### 2.3 Airspace caveat (applies to both options)

The extension window is an ordinary child HWND embedded in the tasklist view
(`TabbedToDoCtrl.cpp:870-935`, `SetWindowPos(..., m_taskList, ...)` at `:932`, hidden /
shown with the view at `:2467-2468`). Anything the host (MFC) paints over that rectangle —
menus, tooltips, drag images, `CBrowserDlg`-style in-place label editors, splash overlays
— will be drawn **under** the WebView2 surface. Mitigations:

- keep host-drawn overlays out of the view rectangle (menus are separate windows anyway);
- for in-place rename, do **not** rely on `GetLabelEditRect` + a native `CEdit` drawn over
  the view; instead ask the web view to start its own inline editor via the bridge and
  report the result back as a modify request (section 4.3);
- if an overlay is unavoidable, the host can temporarily hide the controller
  (`CoreWebView2Controller::put_IsVisible(FALSE)`) around the overlay's lifetime.

This is the same trade-off any Chromium-in-Win32 embedding has and does not affect
correctness of data flow.

## 3. Where data comes from

Three candidate sources, in order of preference:

| Source | What it is | Use it for |
|---|---|---|
| **In-memory `ITaskList`** | The `CTaskFile` the host already builds for the view (`UpdateTasks`, `TabbedToDoCtrl.cpp:1394-1398`) | The task data. Always. |
| **Plugin folder files** | `file://` or, preferably, a virtual host / `SetVirtualHostNameToFolderMapping` mapping `https://app.tdl/` to the plugin's own `web\` subfolder next to the DLL | Static UI assets (html/js/css/wasm/fonts). No server needed. |
| **Loopback HTTP server** | A `WebView2` `AddWebResourceRequestedFilter` handler or, if a dev server is desired, an `http://127.0.0.1:<port>` server started by the plugin | Development hot-reload only (`vite dev` / `npm run dev`); never required in production. |

Design rule: **the view is stateless with respect to files.** It does not open the `.tdl`;
it receives JSON snapshots/deltas derived from the `ITaskList`, and it writes back only
by asking the host to modify tasks. That keeps the "one `CToDoCtrlData` per tasklist"
invariant and keeps undo, modified flags and notifications intact
(`AGENTS.md` core invariants 1, 4).

The bridge can use either WebView2 posting of JSON strings
(`WebMessageReceived` + `PostWebMessageAsJson`) or WebView2's `AddHostObjectToScript`
for a small `{postMessage, request, log}` host object; the former is simpler and versioned
by us, the latter is closer to the old `window.external` model and therefore easier to
port from an IE-based prototype.

## 4. Message bridge

The premise in the question is right: most interaction is message-driven. But two kinds
of messages must be distinguished.

### 4.1 Messages that should NOT be forwarded literally

- **Synchronous `SendMessage` semantics.** `WM_IUI_*` and `WM_TDCN_*` carry pointers
  (`IUIExtension.h:312-344`, `TDCMsg.h:12-60`), and the host expects an immediate return
  value (`CanDoAppCommand`, `GetLabelEditRect`, `HitTest`, `SaveTaskViewToImage` into a
  caller buffer). A web view is asynchronous; there is no safe way to block the UI thread
  on a JS reply. **Convert request/response pairs to async with an id.**
- **Keyboard and mouse `MSG`s.** `ProcessMessage(MSG*)` (`IUIExtension.h:362`) is for
  accelerator/tooltip/drag-drop inspection of messages destined for the plugin's own
  children. It is *not* an input path into the browser: WebView2 consumes input inside its
  own process/renderer and does not need the host for it. The extension should return
  `FALSE` from `ProcessMessage` except for `WM_KEYDOWN` shortcuts the host must see first
  (e.g. app accelerators while the view has focus), which are then handled by WebView2's
  `AcceleratorKeyPressed` event and reported back as commands.
- **Focus.** Focus is an `HWND` concept. `IUI_SETFOCUS` (`IUIExtension.h:156`) becomes "JS
  focus the active grid/editor"; keyboard input then flows through the WebView2 child
  automatically because the extension window *is* the `HWND` that receives it.

### 4.2 Proposed protocol (`tdl-bridge/1`, JSON over WebView2 web messages)

Native → web (**push**, fire and forget):

| Event | Payload | Trigger in the host |
|---|---|---|
| `tasks.snapshot` | `{tasks:[…], attributes:[…], readonly, theme, font, columns, sort}` | `UpdateTasks(IUI_NEW)` or first `IUI_EDIT` after `bNeedFullTaskUpdate` |
| `tasks.patch` | `{changed:[{id, attribute, value}], updateType}` | `UpdateTasks(IUI_EDIT)` (the host already filters by `WantTaskUpdate`) |
| `tasks.deleted` | `{ids:[…]}` | `UpdateTasks(IUI_DELETE)` |
| `selection` | `{ids:[…], anchor}` | host selection changed |
| `theme` / `font` / `readonly` | same shapes | `SetUITheme`, `SetTaskFont`, `SetReadOnly` |
| `edit.start` / `edit.cancel` | `{id, attribute}` | host wants an in-place edit (replaces `GetLabelEditRect`) |
| `contextmenu` | `{x, y, hitTest}` | `ShowContextMenu` |
| `image.request` | `{kind, width, height}` | `IUI_SAVETOIMAGE` |
| `reply` | `{id, ok, value}` | answers a `req` from the web (see below) |

Web → native (**requests** answered asynchronously, plus a few one-way commands):

| Request | Host action |
|---|---|
| `select` `{ids}` | `pExtWnd->SelectTasks(ids, n)` (`IUIExtension.h:355-356`) |
| `modify` `{attribute, value, taskIds, append}` | `PostMessage(WM_IUI_MODIFYSELECTEDTASK, n, &IUITASKMOD[])` (`IUIExtension.h:317`, struct `:202-228`) |
| `move` `{taskIds, parentId, afterId, copy}` | `WM_IUI_MOVESELECTEDTASK` (`IUIExtension.h:320`, struct `:232-240`) |
| `sort` `{attribute, ascending}` or `{multiSort}` | `WM_IUI_SORTCHANGE` / `IUI_MULTISORT` app command (`IUIExtension.h:329, :163`) |
| `appCommand` `{cmd, data}` | used for expand/collapse/scroll/focus (`IUIExtension.h:146-178`) only if the host permits it; otherwise the plugin handles it locally |
| `editTitle` / `editIcon` | `WM_IUI_EDITSELECTEDTASKTITLE` / `…ICON` (`IUIExtension.h:323-326`) |
| `addTask` `{parentId, title}` | `PrepareNewTask(ITaskList*)` (`IUIExtension.h:360`) or the host's own add command |
| `timetrack` `{id, start}` | host notice `WM_TDCN_TIMETRACK` is native→native; web side uses a host request |
| `openFile` / `openUrl` | `WM_IUI_SHOWFILELINK` (`IUIExtension.h:335`) or `ShellExecuteEx` from the plugin |
| `contextMenu` `{taskId, x, y}` | the host builds the standard task context menu at that point |

Notes:

- `modify` and `move` must **not** edit data directly even if a `ITaskList*` (non-const)
  is available; routing them as `WM_IUI_MODIFY*`/`MOVE*` keeps undo, modified flags and
  the `WantTaskUpdate` fan-out to the tree/list intact (host handlers
  `TabbedToDoCtrl.cpp:180-190`).
- The host answers `CanDoAppCommand` / `HitTest` issues synchronously where it needs to
  (context menu availability). Implement `CanDoAppCommand` conservatively (return the
  union of what the current snapshot supports) and let the web side grey out from the
  last `appCommandSet` push instead of asking interactively.
- Validation: attribute values are decoded natively from JSON by the plugin and mapped
  to `TDC_ATTRIBUTE`; custom attributes must reject unknown IDs (see
  `ToDoCtrlDataUtils` custom attribute handling). The web side never sees raw `TODOITEM`
  pointers.

### 4.3 What the web view owns and does not own

| Owned by the web view | Owned by the native app |
|---|---|
| Rendering, layout, scroll, sorting *presentation* | The data model (`CToDoCtrlData`) |
| Focused row/column within the view | The selection that other views observe |
| Inline editors *inside* the view (title, attribute cells) | Undo stack, modified flags, filters, persistence |
| Tooltips, hover, animations, drag *initiation* overlays | Drag/drop *drop* onto the tree/list (`HandleDrop` is native) |

The "in-place rename" behaviour of the native views (`GetLabelEditRect` is used by the
host to position its own `CEdit`, `TabbedToDoCtrl.cpp:2637-2640`, `6954-6989`) is the one
place where the current contract assumes a native editor over the view. For the web view
the contract becomes: host sends `edit.start`, web view opens its own editor, and on
commit sends `modify {attribute: TDCA_TITLE…}`. `GetLabelEditRect` can return `FALSE`
or the view's rectangle.

## 5. Memory footprint

Electron and WebView2 both embed Chromium; the difference is what you ship and what is
shared.

| Model | Per-instance idle RAM (typical, order of magnitude) | Installed bytes |
|---|---|---|
| Electron app | ~150–250 MB app process tree alone (browser + GPU + renderer + Node in main) | ~120–200 MB app bundle |
| WebView2 evergreen | ~60–150 MB for the same page (browser + renderer + GPU), **shared** `msedgewebview2.exe` binaries already on disk | 0 extra MB in the app bundle (runtime is OS/Edge-installed); fixed-version ≈ 130 MB |
| Legacy IE control (MSHTML in-proc) | ~15–40 MB, in-process | 0 extra |

Consequences for this app:

- **Do not create WebView2 in `CreateExtWindow` eagerly if the view is not the active or
  a visible tab.** The host already creates extension windows lazily (on first
  activation, `CreateExtensionWindow`), but a tasklist with a web view tab will keep one
  Chromium process model alive while the tasklist is open. Create the web view when the
  tab first becomes visible, and `Close()` the `CoreWebView2` after an idle timeout while
  keeping the outer HWND.
- **One web view per tasklist, never per task.** The view is a presentation of the
  tasklist, not of a task.
- A **low-memory web framework** helps at the margin (it is renderer-side JS heap, tens
  of MB for a dense grid, not the process baseline): a small reactive library (Preact,
  Solid, Svelte, or plain DOM + `requestAnimationFrame` virtual list) instead of React
  is reasonable for a 100k-row task list because it makes virtualised rows cheaper. The
  process model, not the framework, dominates — so "no Electron, no Node" is worth
  roughly 100–200 MB per install and tens of MB per instance, which is the meaningful
  win; the JS heap itself will not be near zero.
- The legacy IE control would be the only way to get a "few MB" view. That is the
  real trade-off: IE control ⇒ tiny footprint, ES5-era UI, no modern editors; WebView2
  ⇒ modern UI and tooling, Chromium-sized footprint.

Target for a first prototype: a read-only tree/grid for `biglist.tdl` under ~150 MB
working set for the whole app with one web view open, and no measurable loss versus the
native list for the existing PERF-003 harness paths (the web view replaces a path that
the harness does not drive).

## 6. Alternative comments editor (the easier case)

The comments pane is already a content-plugin host. `ToDoCtrl` picks a `CContentCtrl`
by comments format via `CContentMgr::FindContent(cfComments)`
(`ToDoCtrl.cpp:954-968`, `ContentMgr.cpp:221-247`), loads a task's custom comments
through `SetContent`, and listens for `WM_ICC_CONTENTCHANGE` (`IContentControl.h:28`).

The full `IContentControl` surface (`IContentControl.h:152-187`) is small and
value-based:

| Method | Web-editor implementation |
|---|---|
| `GetContent` / `SetContent` (opaque bytes) | text/HTML or a small binary of your own; text format round-trips through `GetTypeID` |
| `GetTextContent` / `SetTextContent` / `InsertTextContent` | editor document text — for markdown, the source text |
| `FindReplaceAll` | editor search API |
| `Undo` / `Redo` | editor history (`Ctrl+Z` mapped to CodeMirror's undo) |
| `Enable`, `SetReadOnly`, `SetUITheme`, `SetContentFont` | CSS variables / editor theme |
| `ProcessMessage`, `FilterToolTipMessage`, `DoIdleProcessing` | mostly `FALSE`; let the editor's own keys through |
| `GetSpellCheckInterface` | return `NULL` (the host's `ISpellCheck` remains valid for other controls) |
| `SavePreferences` / `LoadPreferences` | editor settings (tab size, wrap) under the plugin's own key |

Direction of traffic:

- **native → web**: `SetContent`/`SetTextContent` push `editor.set`; theme/font/readonly
  push settings; selection changes in the task tree push a new document.
- **web → native**: one message, `editor.changed {dirty}`, is posted as
  `WM_ICC_CONTENTCHANGE` to the host window; `GetTextContent` is then read
  synchronously by the host as it already does. Markdown-specific things (e.g. task
  links) use the existing `WM_ICC_TASKLINK`/`WM_ICC_GETATTRIBUTELIST` contract so links
  and attribute insertion behave like the built-in editor.

Why it is simpler than a task view:

- no selection protocol with the tree, no sort, no undo integration at the task level,
  no `ITaskList` snapshot — the editor owns a string;
- the host already tolerates the `IsUpdatingFormat`/re-entrancy dance it needed for the
  HTML plugin (`ToDoCtrl.cpp:1030-1060`), and a WebView2 editor is asynchronous by
  nature, which that path already expects;
- the airspace problem is irrelevant because the comments pane is a leaf control;
- a markdown-first editor can keep the **stored** format as plain text or HTML while the
  **editing** experience is markdown — but note that switching the stored format for
  existing tasklists is a format-compatibility question (comments format IDs are stored
  per tasklist) and should remain an opt-in, per-tasklist property. Without that, only
  the editing surface changes and files stay byte-compatible.

This is the recommended **first implementation** of web technology in the app: it
proves the WebView2 + bridge + packaging story with a tiny contract and near-zero risk to
task semantics.

## 7. Proposed steps

1. **Bridge module** (`Core/Shared/WebView2Host` or inside the plugin): wrap
   `CoreWebView2Environment`/`Controller`, `PostWebMessageAsJson`, `WebMessageReceived`,
   `AddHostObjectToScript` if needed, and a `{req,id}` correlation layer. Keep it free of
   MFC types; it talks to a plain `IMessageSink` so it can be unit-tested.
2. **Content-plugin prototype** (`TED`-adjacent, but a new DLL, not a comment-editor
   change): CodeMirror 6 or a markdown editor behind `IContentControl`; format ID and
   asset folder under the plugin; store plain text; register `WM_ICC_*` links.
3. **View-plugin prototype** (`UI`-adjacent, new DLL): read-only virtualised tree exported
   from `ITaskList` snapshots, then selection, then in-place modify via `WM_IUI_MODIFY*`,
   then sort and context menu. Measure against `biglist.tdl` with PERF-003 fixtures.
4. **Packaging**: `web\` assets next to the DLL, virtual host mapping (no server), optional
   WebView2 runtime check with a user-visible fallback. Do not bundle a fixed-version
   runtime by default.
5. **Fallback** if WebView2 is unacceptable: the same bridge over the existing
   `CWebBrowserCtrl` (`Core/Shared/WebBrowserCtrl.h`) with `window.external` host calls and
   `DISPID` event sinks (`TDLBrowserDlg.cpp:24-28` shows the event-sink pattern). The
   protocol and the plugin-side state machine stay identical; only the transport changes.

## 8. Risks

| Risk | Mitigation |
|---|---|
| WebView2 Runtime missing (older Windows 10, LTSC without Edge) | Detect at plugin load (`GetAvailableCoreWebView2BrowserVersionString`); show a plugin-owned install prompt; otherwise fall back to the IE control or hide the view |
| Airspace (menus/tooltips/overlays clipped) | Keep host overlays out; use in-view editors; toggle `IsVisible` around unavoidable overlays |
| Async bridge vs. synchronous host expectations | Explicit req/reply with ids; `CanDoAppCommand` answered conservatively; no blocking waits on the UI thread |
| Second undo/notification path from a plugin | All edits go through `WM_IUI_MODIFY*`/`MOVE*`, never direct data writes |
| Memory of a Chromium process tree per tasklist | Create on first show, `Close()` after idle, one per tasklist, light framework, measure |
| DPI and theming differences (dark mode, custom DPI) | Push `UITHEME` and the task font through the bridge; test at 100/150/200 % |
| Clock: dynamic `WebView2Loader` import + `CoreWebView2` COM headers in a 32-bit MFC DLL | `LoadLibrary` the loader DLL on demand; keep the loader in the plugin folder, not next to `ToDoList.exe` (plugin discovery scans every DLL) |
| Accessibility | WebView2 exposes a UIA tree of the page; a web view may actually be **more** accessible than the callback-text `CListCtrl` noted in [GEN-003](GEN-003-ui-automation-framework.md#accessibility-probe-findings-2026-11-16) |

## 9. Verification

- Prototype loads on a Windows 11 machine with no extra install and on a Windows 10
  machine without WebView2 Runtime (fallback path).
- Comments editor: open/edit/save/reopen a tasklist with the web editor; text identical
  to the built-in editor; `TDLTest` `CTDLSimpleTextContentCtrlTest` unaffected (different
  `GetTypeID`).
- View: `biglist.tdl` scroll/select/edit/sort round-trips; `.tdl` byte-equal to the native
  list after the same edit sequence (same undo entries).
- No new HTTP listener in production builds (verify with `netstat`).
- Working-set measurement for app + web view before/after, recorded here.
