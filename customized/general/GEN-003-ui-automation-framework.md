# GEN-003 — Functional UI automation framework (proposed)

| Field | Value |
|---|---|
| Status | Proposed (design only; no implementation) |
| Upstream baseline | ToDoList 9.3.DP7 |
| Scope | New test project only; production code unchanged unless test hooks/accessibility are added |
| Default | Not part of the normal `TDLTest` run or build |
| Related | [index.md](../index.md), [PERF-003](../performance/PERF-003-ui-perf-harness.md), [../docs/TESTING.md](../../docs/TESTING.md) §2.4, §4.1 |

## Goal

Add automated **functional** UI tests that drive the real `ToDoList.exe` through complete
user flows (open tasklist, add/edit/delete tasks, switch views, filter, undo/redo,
dialogs, save/close/reopen), asserting both the UI state and the persisted `.tdl`.

`CUIPerfTest` (PERF-003) remains what it is: an opt-in splitter/scroll rendering and
performance harness that moves the real mouse and compares pixels. It is not the
foundation for functional UI tests.

## Why UI Automation (UIA) is the recommended approach

- The task views are real common controls, not custom-drawn windows: the tree host creates
  a `CTreeCtrl` (`Core/ToDoList/TDLTaskTreeCtrl.cpp:112`), the list host creates a
  `CListCtrl` (`Core/ToDoList/TDLTaskListCtrl.cpp:126`), both inside a `WS_EX_CONTROLPARENT`
  host (`Core/ToDoList/TDLTaskCtrlBase.cpp:287`). Windows supplies MSAA/UIA providers for
  `SysTreeView32`/`SysListView32`, including item text supplied through
  `TVN_GETDISPINFO`/`LVN_GETDISPINFO`.
- Dialogs are standard `#32770` windows with control IDs, which UIA exposes as
  `AutomationId`; the existing harness already locates them by class/ID
  (`Core/TDLTest/UIPerfTest.cpp:560-640`).
- Commands are ordinary menu commands. `CUIPerfTest` already dispatches them via
  `WM_COMMAND` (`UIPerfTest.cpp:1610`), so flows can use UIA patterns first and
  `WM_COMMAND`/window messages as targeted fallbacks.
- Automatic UI tests require **no recompilation for coverage** and no production change on
  day one; the out-of-process driver also survives app crashes and can enforce timeouts.

Expected gaps, confirmed by the probe below: the splitter and some custom-drawn regions
have no UIA element (reuse PERF-003 geometry/`SendInput` logic for those), task-list rows
use callback text and expose blank names, and only the first column of the owner-draw
attribute grid is named.

## Decision

Use **FlaUI (UIA3) in a separate C#/.NET test project** (for example
`Core/TDLUITest/` with xUnit/NUnit), running out-of-process against a Debug build of
`ToDoList.exe`.

| Option | Assessment |
|---|---|
| **FlaUI + UIA3** | Recommended. Maintained, typed API, UIA + input + screenshots, works with Win32/MSAA providers |
| Raw UIA COM in C++ | Possible if a .NET test project is unacceptable; significantly more boilerplate |
| WinAppDriver / Appium Windows driver | Workable but adds a server/driver layer and maintenance risk; only if the project standardises on Appium |
| pywinauto | Good for diagnostics and one-off scripts; less structured for a CI suite |
| AutoIt / AutoHotkey | Window/pixel level and brittle; manual tooling only |
| Extend `CUIPerfTest` | Keep for rendering/perf artefacts; wrong foundation for functional coverage |

Rationale for out-of-process: modal dialogs, crashes, hangs and timers are isolated from
the test runner, and the same binary can be exercised in a plain desktop session.
FlaUI handles 32-bit targets from a 64-bit test process.

## Proposed phases

### Phase 0 — Accessibility spike (probe run 2026-11-16)

Run Accessibility Insights (or a small FlaUI dump) against a fixture tasklist and record:

- whether task tree/list rows expose names, selection and columns;
- which panes host accessible elements and which are blind (splitter, custom hosts);
- whether dialogs and the menu/toolbar expose stable `AutomationId`s;
- whether the localized UI affects names (tests must prefer IDs over text).

The probe confirmed the tree, toolbars, dialogs and comments editor; the task list and
attribute grid have the limitations listed under "Accessibility probe findings". If task
rows or attribute values must become readable through UIA, decide between narrow test
hooks and implementing accessibility in the shared list controls (a separate
product-facing customization).

### Phase 1 — Skeleton

- New test project with launch/teardown helpers.
- Reuse the PERF-003 fixture and isolation model: per-run temp folder, generated/copied
  `.tdl`, isolated `.ini` (`UIPerfTest.cpp:1280-1375`), launch with `/i <ini>` and the
  tasklist path, wait for the visible frame by process id (`UIPerfTest.cpp:699-713`).
- One smoke flow: open a fixture, verify the task count, switch tree/list, close.
- Failure artifacts: UIA tree dump, screenshot, app/INI paths, and `%TEMP%` assert log.

### Phase 2 — Functional flows

Cover the state paths that the in-process suite cannot reach:

- add/edit/delete tasks and verify after save/close/reopen (`CTaskFile`/XML round-trip);
- filter, view switching, maximize/restore, column show/hide;
- undo/redo and saved-state feedback (UI-004);
- dialogs (Preferences pages, Add Task, Find Tasks) including validation and Cancel;
- tasklist lifecycle: delayed-window close (PERF-004), MRU switcher (UI-002), Window
  menu (UI-003).

Every test asserts UI state **and** persistence where a file write is involved.

### Phase 3 — Determinism hooks

Only if Phase 0/2 shows they are needed, add a test-only switch (for example `/test`) that
disables first-run wizards, reminders, tray icons, animations and update checks, and
optionally fixes "today". Keep the hook documented and inert without the switch.

## Comments editor behaviour (TED-004 and future editing work)

The plain-text comments editor is `CTDLSimpleTextContentCtrl`, a RichEdit created with
`IDC_COMMENTS` (`Core/ToDoList/ToDoCtrl.cpp:577`; auto-indent lives in
`TDLSimpleTextContentCtrl.cpp`). It is a good fit for a **hybrid** strategy, because
UIA can drive and read the editor but does not expose paragraph formatting directly.

### 1. Logic tests (fast, in `TDLTest`)

Extract the file-local `GetAutoIndentPrefix()`/bullet-prefix rules into a small testable
helper and assert them without a UI: spaces, tabs, bullets, empty bullets, indented
bullets, multi-line selections, and the `BULLET_PREFIXES` table. These are the rules most
likely to regress and need no desktop.

### 2. UIA end-to-end tests (the real editing path)

Drive real keystrokes (`SendInput`/FlaUI `Keyboard.Type`), because auto-indent runs from
`ProcessMessage(WM_KEYDOWN/VK_RETURN)` and synthetic `EM_REPLACESEL` would bypass it:

- focus the comments editor (by `AutomationId`/`IDC_COMMENTS` or `RichEdit50W` under the
  tasklist container) and type a known prefix + text;
- press Enter and assert the caret/selection and resulting text via UIA **TextPattern**
  (range text, selection ranges);
- wait for the 50 ms `TIMER_AUTOINDENT` coalescing before asserting paragraph geometry;
- exercise the edge cases from TED-004: bullet continuation, empty bullet, no-text line,
  Shift/Ctrl/Alt+Enter unchanged, paste, font change, multi-line selection + Enter,
  undo/redo (the formatting pass must not create undo entries), and save/close/reopen
  (stored text must keep the spaces; the visual offset is re-applied on load).

The expected results are written as text-with-caret/selection tables in
[TED-004](../text-editor/TED-004-comments-auto-indent.md#behaviour-specification); the same rows drive
the unit and UIA layers, so the contract is defined once.

RichEdit exposes TextPattern text/selection and, on current Windows, range bounding
rectangles. Use the bounding rectangles to verify wrapped-line alignment without pixels:
compare the x of the first character of a wrapped display line with the x of the first
non-whitespace character of its paragraph. If a provider does not return rectangles,
fall back to a test-only read of `EM_GETPARAFORMAT` (`dxOffset`) or a narrow screenshot
check; do not make screenshot comparison the primary assertion.

Probe result (2026-11-16): the comments editor is `RICHEDIT50W` with
`AutomationId=1003`, and exposes `TextPattern` (document text, selection ranges and
bounding rectangles) plus `ValuePattern`. This makes the TED-004 typing/caret/alignment
tests viable without a test hook.

### 3. What not to automate first

IME composition, spell-check popups, and drag/drop text are timing- and environment-heavy;
document them as manual checks unless a deterministic hook is added.

## Component harness (desktop analogue of isolated component tests)

Web component tests mount a component with its real renderer and controlled inputs. The
desktop/MFC equivalent is a small test host that creates the **production `CWnd` class**
inside a test frame, pumps messages, supplies the parent contract, and inspects state
in-process. There is no DOM or Storybook, but for leaf controls this is feasible and can be
faster and more precise than UIA.

Feasibility in this repo:

- `TDLTest` is already an MFC app (`CWinApp theApp`, `AfxWinInit`; `TDLTest.cpp:48`,
  `:131`) and links the production library (`TDLTest.vcxproj:226-239`), so it can host real
  windows in the test process.
- In-process assertions can use `GetTextRange()`, `GetSel()` and `EM_GETPARAFORMAT`, so the
  UIA text/selection/geometry limitations do not apply to component tests.
- `CTDLSimpleTextContentCtrl`/`CTDLCommentsCtrl` is a good first host: create it in a
  fixed-size frame, set the font/DPI, answer the `WM_ICC_*` parent messages with a minimal
  stub (or reuse `CTDLCommentsCtrl`), then drive keys and assert the TED-004 contract.

Caveats:

- `ToDoList.rc` is compiled into `ToDoList.exe`, not `ToDoListLib`; dialogs, toolbars and
  string tables are therefore not in `TDLTest.exe` (which has only `TDLTest.rc`). Load the
  app resources with `LoadLibraryEx(..., LOAD_LIBRARY_AS_DATAFILE)` + `AfxSetResourceHandle`,
  or add the `.rc` to the test target. Controls that do not load app resources (such as the
  plain comments edit field) can run without this.
- Controls depend on MFC globals and services: `CWinApp`, preferences, theme, clipboard,
  spell-check/content interfaces, plugins. The host must provide them; a drifting stub
  silently changes behaviour, so keep the host contract minimal and documented.
- Whole views (`CToDoCtrl`, tree/list, filter, undo) are not leaf components; isolating
  them means rebuilding the app shell. Keep those at app level (UIA).

Use a component harness for the comments editor, new dialogs/pages and standalone controls.
Use app-level UIA for tasklist lifecycle, command routing, filtering and persistence.

## Accessibility probe findings (2026-11-16)

A UIA dump of a fixture tasklist (`biglist.tdl`, isolated INI) produced:

| Control | Result |
|---|---|
| Task tree (`SysTreeView32`) | Items expose names/properties; usable as-is |
| Task list (`SysListView32`, id 101) | `GridPattern` reports 20 rows x 103 columns, but every row/cell name is empty |
| Attribute grid (`SysListView32`, id 387) | `GridPattern` reports 40 rows x 2 columns; row names are the attribute labels, the value cell is an empty `Text` element |
| Comments editor (`RICHEDIT50W`, id 1003) | `TextPattern` text, selection and bounding rectangles; `ValuePattern` |
| Toolbars, dialogs, menus | Standard controls with IDs; readable via UIA |

Causes:

- The task list inserts items with `LPSTR_TEXTCALLBACK` and supplies text only through
  `LVN_GETDISPINFO` (`Core/ToDoList/TDLTaskListCtrl.cpp:436-442`, `:958-982`). The
  ListView UIA/MSAA provider does not resolve callback text here, so names are blank.
- The attribute grid is a report-view `CEnListCtrl`, which is forced owner-draw
  (`Core/Shared/EnListCtrl.cpp:1101-1107`). The provider exposes the first column but not
  subitem text, so the value column is empty.

Consequences for tests:

- Do not identify task rows or attribute values by UIA `Name`. Use `GridPattern` row/column
  indices plus bounding rectangles to locate and click cells, and verify content through
  the model: save/close/reopen the `.tdl` (or read the inline editor's value while editing).
- If names/values must be readable through UIA, implement MSAA/`IAccessible` (or a UIA
  provider) for `CEnListCtrl`/`CInputListCtrl` and optionally the task list so first-column
  callback text and subitems are returned. That is a product-facing accessibility change
  and should be its own customization, not a test-only hook.
- A read-only test hook (registered window message returning cell text) is the cheaper
  fallback if only the test suite needs the values.


## Environment and CI

- Requires an interactive, unlocked desktop. Use a self-hosted Windows agent with
  auto-logon and a scheduled task/`psexec -i` in the active session; screen lock breaks
  `SendInput`/`BitBlt` and screenshots.
- Fix DPI and resolution for the run; do not run UI tests in parallel.
- Prefer explicit UIA waits over `Sleep`; every test gets a timeout.
- Set `TDL_ASSERT_LOG=1` so MFC asserts are logged instead of blocking on a dialog.
- Keep the project out of `BuildDebug.bat`; run it via `dotnet test` (or a small `.bat`)
  after the Debug build.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Custom controls expose nothing through UIA | Phase 0 spike; fall back to `WM_COMMAND`/geometry, or implement accessibility separately |
| Flaky timing (timers, reminders, async load) | Deterministic hooks, isolated INI, explicit waits, single-threaded runs |
| Localized names break selectors | Select by control ID/`AutomationId`, never by visible text |
| Modal dialogs block the driver | UIA dialog handling with timeouts; kill only the launched process |
| Maintenance drift in a second language/toolchain | Keep tests thin over stable IDs; reuse existing fixture generators |
| Test-only switches leaking into production behaviour | Gate strictly on the command-line flag; document in `docs/APPLICATION_STATE.md` if added |

## Verification

- Phase 0 spike result recorded here.
- Phase 1 smoke test passes repeatedly (at least 10 consecutive runs) on the target VM.
- Phase 2 flows pass against the Debug build and produce clear artifacts on failure.
- Normal `TDLTest`, `BuildDebug.bat` and the PERF-003 harness are unaffected.
