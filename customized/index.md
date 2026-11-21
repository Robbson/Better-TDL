# Customization Index

This folder tracks every intentional divergence from upstream ToDoList so changes can be
reviewed, tested and re-applied after upstream updates.

- Upstream baseline: ToDoList **9.3.DP7** (file version `9,2,997,7`), branch `custom` at
  `63fc1aacc`, based on upstream `d3b7a917e`.
- Architecture: [../docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md)
- Data model: [../docs/DATA_MODEL.md](../docs/DATA_MODEL.md)
- Project scope: [../PROJECT_SCOPE.md](../PROJECT_SCOPE.md)

## Folder layout

Documents are grouped by the area they affect. Each category has its own ID prefix and
its own number sequence.

```text
customized/
  index.md                        this file: conventions, status and change index
  general/     GEN-001 ...        cross-cutting behaviour, configuration, branding, tooling
  performance/ PERF-001 ...       startup, open/activation cost, rendering, responsiveness
  text-editor/ TED-001 ...        comments editor, text controls, editing behaviour
  ui/          UI-001 ...         windows, menus, toolbars, views, navigation, dialogs
```

## Where a new document goes

Pick exactly one category by the primary user-visible effect this change has:

| Category | Prefix | Folder | Use for |
|---|---|---|---|
| General | `GEN` | `general/` | Cross-cutting or non-visual behaviour: configuration/INI handling, branding and version resources, build/test tooling, automation frameworks that are not tied to one UI area. If a change spans several categories and none dominates, use `GEN`. |
| Performance | `PERF` | `performance/` | Changes whose main purpose is speed, startup cost, latency, memory or rendering performance. A performance-motivated fix that also touches the UI still belongs here. |
| Text editor | `TED` | `text-editor/` | The comments editor and other text-editing controls: toolbar buttons that act on text, typing behaviour, formatting, Quick Find highlighting, display toggles. |
| UI | `UI` | `ui/` | Windows, frames, menus, toolbars as UI structure, views, list/tree presentation, navigation, dialogs and theme. |

Rules of thumb:

- Judge by the **reason for the change**, not by the files touched. Fixing a repaint bug
  because it is slow is `PERF`; adding a toolbar button for text formatting is `TED`.
- A customization that changes only the comments editor is `TED`, even if its buttons live
  in the main toolbar.
- Do not create a new category without updating this file and `AGENTS.md`; keep the folder
  name the kebab-case form of the category and the prefix short and unique.

## Conventions

- Give every change an ID in the form `<PREFIX>-nnn`: `GEN-001`, `PERF-001`, `TED-001`,
  `UI-001`, …
- Number per category, starting at `001`. Never reuse or renumber an existing ID; a new
  change takes the next free number in its category.
- Name files `<PREFIX>-nnn-<one-or-few-keywords>.md` and place them in the matching
  category folder, e.g. `performance/PERF-006-tasklist-open-activation-cost.md`.
- Cross-reference other changes by ID and relative link, e.g.
  `[PERF-004](performance/PERF-004-delayed-tasklist-window.md)` or, across categories,
  `[TED-001](text-editor/TED-001-comments-toolbar-in-main-toolbar.md)`. Link back to
  this index as `../index.md`.
- Record: goal, upstream starting point, files touched, settings, risks, test notes.
- Keep custom code in clearly marked blocks/files so future upstream merges remain
  possible.
- Do not rewrite upstream files wholesale when a contained adapter or new class is
  sufficient.
- Update the index and the change file when a change is designed, implemented, revised or
  abandoned.

## Status legend

| Status | Meaning |
|---|---|
| Proposed | Analysed and designed; not implemented |
| In progress | Implementation started |
| Done | Implemented, built and tested |
| Deferred | Deliberately postponed or rejected |

## Change index

### General (`GEN`)

| ID | Title | Status | Document |
|---|---|---|---|
| GEN-001 | Product name and release label from version resource; About layout | Done (built and manually verified) | [GEN-001-branding-and-about.md](general/GEN-001-branding-and-about.md) |
| GEN-002 | Quote-safe values in ToDoList.ini (Quick Find history) | Done | [GEN-002-quoted-ini-values.md](general/GEN-002-quoted-ini-values.md) |
| GEN-003 | Functional UI automation framework (UIA/FlaUI proposal) | Proposed | [GEN-003-ui-automation-framework.md](general/GEN-003-ui-automation-framework.md) |
| GEN-004 | Web-technology extensions: WebView2 for task views and text editors | Proposed (feasibility study; no implementation) | [GEN-004-webview-based-ui-extensions.md](general/GEN-004-webview-based-ui-extensions.md) |
| GEN-005 | Home Folder view over a filesystem tree | Done (built; TDLTest passes; basic flow manually verified) | [GEN-005-home-folder-view.md](general/GEN-005-home-folder-view.md) |
| GEN-006 | Default assertion logging in Debug builds | In progress (implemented; TDLTest passes; app link pending) | [GEN-006-assert-logging.md](general/GEN-006-assert-logging.md) |
| GEN-007 | TDL set files (`*.tdlset.txt`) in the Home view | In progress (implemented/built; TDLTest passes; interactive check pending) | [GEN-007-tdl-set-files.md](general/GEN-007-tdl-set-files.md) |
| GEN-008 | Portable build and packaging scripts | In progress (implemented; plugin Release build verified; end-to-end zip pending) | [GEN-008-build-script-portability.md](general/GEN-008-build-script-portability.md) |
| GEN-009 | Plugin Manager view over plugin DLLs in the app folder | Proposed (design only) | [GEN-009-plugin-manager-view.md](general/GEN-009-plugin-manager-view.md) |
| GEN-010 | Sharing session state through a common ToDoList.ini | Proposed (analysis; portability gaps identified) | [GEN-010-shared-ini-session-state.md](general/GEN-010-shared-ini-session-state.md) |
| GEN-011 | Compact tasklist backups and per-task history | Proposed (design and measurements; no implementation) | [GEN-011-compact-tasklist-history.md](general/GEN-011-compact-tasklist-history.md) |
| GEN-012 | Single-instance detection must not match foreign windows | Done (built; silent-exit case manually verified) | [GEN-012-single-instance-window-matching.md](general/GEN-012-single-instance-window-matching.md) |

### Performance (`PERF`)

| ID | Title | Status | Document |
|---|---|---|---|
| PERF-001 | Duplicated rows in List View while scrolling | Deferred (all targeted tree experiments reverted) | [PERF-001-list-view-stale-title.md](performance/PERF-001-list-view-stale-title.md) |
| PERF-002 | Splitter paint performance (attempted) | Deferred (three iterations tested; all reverted) | [PERF-002-splitter-paint-performance.md](performance/PERF-002-splitter-paint-performance.md) |
| PERF-003 | Automated UI performance/artefact harness | In progress (implemented/built; interactive baseline pending) | [PERF-003-ui-perf-harness.md](performance/PERF-003-ui-perf-harness.md) |
| PERF-004 | Deferred window creation for delay-loaded tasklists | Done | [PERF-004-delayed-tasklist-window.md](performance/PERF-004-delayed-tasklist-window.md) |
| PERF-005 | No throwaway tasklist at startup | Done | [PERF-005-no-throwaway-tasklist.md](performance/PERF-005-no-throwaway-tasklist.md) |
| PERF-006 | Fixed per-tasklist open/activation cost in a running app | Measured (steps 1-2: instrumentation + attribution done; fix pending) | [PERF-006-tasklist-open-activation-cost.md](performance/PERF-006-tasklist-open-activation-cost.md) |
| PERF-007 | Preferences search slow on first use | In progress (D font cache done; E search index core done and unit-tested; dynamic coverage/parity corpus pending) | [PERF-007-preferences-search-first-use-cost.md](performance/PERF-007-preferences-search-first-use-cost.md) |
| PERF-008 | Delayed loading for multi-document menu and Home opens | Done (built; TDLTest passes; interactive check pending) | [PERF-008-delay-load-multi-document-opens.md](performance/PERF-008-delay-load-multi-document-opens.md) |

### Text editor (`TED`)

| ID | Title | Status | Document |
|---|---|---|---|
| TED-001 | Comments editor controls moved to the main toolbar | Done (date/time buttons stay; format-combo move planned to be reverted — see UI-006) | [TED-001-comments-toolbar-in-main-toolbar.md](text-editor/TED-001-comments-toolbar-in-main-toolbar.md) |
| TED-002 | Hide mouse pointer while typing in text controls | Done | [TED-002-comments-hide-pointer-while-typing.md](text-editor/TED-002-comments-hide-pointer-while-typing.md) |
| TED-003 | Highlight Quick Find matches in the comments editor | Done | [TED-003-quickfind-comments-highlight.md](text-editor/TED-003-quickfind-comments-highlight.md) |
| TED-004 | Auto-indent (Return and wrapped lines) in the comments editor | In progress (implemented; covered by TDLTest) | [TED-004-comments-auto-indent.md](text-editor/TED-004-comments-auto-indent.md) |
| TED-005 | Comments editor word-wrap and show-all-characters toolbar toggles | In progress (implemented/built; interactive check pending) | [TED-005-comments-display-toggles.md](text-editor/TED-005-comments-display-toggles.md) |

### UI (`UI`)

| ID | Title | Status | Document |
|---|---|---|---|
| UI-001 | Legacy (v8.2-style) attribute editor, selectable | Done (phases 1–3 implemented; date-field issues resolved) | [UI-001-legacy-attribute-editor.md](ui/UI-001-legacy-attribute-editor.md) |
| UI-002 | IntelliJ-style MRU tasklist switcher for Ctrl+Tab | Done (built and manually verified) | [UI-002-mru-tasklist-switcher.md](ui/UI-002-mru-tasklist-switcher.md) |
| UI-003 | Window menu lists up to 25 tasklists and shows the open count | Done (built and manually verified) | [UI-003-window-menu-list.md](ui/UI-003-window-menu-list.md) |
| UI-004 | Undo/redo feedback, enablement and saved-state tracking | Done | [UI-004-undo-redo-feedback.md](ui/UI-004-undo-redo-feedback.md) |
| UI-005 | Restore tree folding when a filter is cleared | Done (built and manually verified; multiple-snapshot support deferred) | [UI-005-filter-fold-restore.md](ui/UI-005-filter-fold-restore.md) |
| UI-006 | Toolbar architecture: command tables, split bars, main-toolbar customization | In progress (phase 1 done: data-driven main toolbar; later phases proposed) | [UI-006-toolbar-architecture.md](ui/UI-006-toolbar-architecture.md) |
| UI-007 | Tri-state view toolbar button (task list / editor / split) | Proposed (design note) | [UI-007-tristate-view-toolbar-button.md](ui/UI-007-tristate-view-toolbar-button.md) |
| UI-008 | Filter bar: integrated text filter and Quick Find | Proposed (revised scope: combined control first) | [UI-008-compact-filter-bar.md](ui/UI-008-compact-filter-bar.md) |
| UI-009 | Menu actions for the comments position | Done (built; tests pass; interactive check pending) | [UI-009-comments-position-menu-actions.md](ui/UI-009-comments-position-menu-actions.md) |
| UI-010 | Preferences dialog setting tooltips | Done (built; interactive check pending) | [UI-010-preferences-tooltips.md](ui/UI-010-preferences-tooltips.md) |
| UI-011 | Preferences dialog Apply button with Cancel rollback | Proposed | [UI-011-preferences-apply-button.md](ui/UI-011-preferences-apply-button.md) |
| UI-012 | Tooltip and wider hit band for the attribute/title splitter | Done (built; TDLTest passes; widened-band interactive check pending) | [UI-012-attribute-splitter-tooltip.md](ui/UI-012-attribute-splitter-tooltip.md) |
| UI-013 | Remove the debug dead-space marker beside the horizontal scrollbar | Done (built; TDLTest passes; visual check pending) | [UI-013-debug-scrollbar-dead-space.md](ui/UI-013-debug-scrollbar-dead-space.md) |
| UI-014 | Preferences index section highlighting | In progress (implemented and built; interactive check pending) | [UI-014-preferences-section-highlight.md](ui/UI-014-preferences-section-highlight.md) |
| UI-015 | Preferences page grouping and information architecture | Proposed | [UI-015-preferences-reorganization.md](ui/UI-015-preferences-reorganization.md) |
| UI-016 | Custom toolbar default configuration | Done (built; TDLTest passes) | [UI-016-default-custom-toolbar.md](ui/UI-016-default-custom-toolbar.md) |
| UI-017 | Legacy tasklist display options (row height, icon spacing, header font, comment brackets) | Done (built; TDLTest passes; List View row-height limitation documented) | [UI-017-legacy-task-row-height.md](ui/UI-017-legacy-task-row-height.md) |
