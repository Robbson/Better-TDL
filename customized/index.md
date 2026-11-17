# Customization Index

This folder tracks every intentional divergence from upstream ToDoList so changes can be
reviewed, tested and re-applied after upstream updates.

- Upstream baseline: ToDoList **9.3.DP7** (file version `9,2,997,7`), branch `custom` at
  `63fc1aacc`, based on upstream `d3b7a917e`.
- Architecture: [../docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md)
- Data model: [../docs/DATA_MODEL.md](../docs/DATA_MODEL.md)
- Project scope: [../PROJECT_SCOPE.md](../PROJECT_SCOPE.md)

## Folder layout

```text
customized/
  index.md                       this file: conventions, status, change index
  CUST-001-legacy-attribute-editor.md
  CUST-002-<keywords>.md         one file per customization
```

## Conventions

- Give every change an ID: `CUST-001`, `CUST-002`, …
- Name files `CUST-nnn-<one-or-few-keywords>.md`.
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

| ID | Title | Status | Document |
|---|---|---|---|
| CUST-001 | Legacy (v8.2-style) attribute editor, selectable | In progress (Phases 1–3 done; date painting pending) | [CUST-001-legacy-attribute-editor.md](CUST-001-legacy-attribute-editor.md) |
| CUST-002 | Duplicated rows in List View while scrolling | Deferred (all targeted tree experiments reverted) | [CUST-002-list-view-stale-title.md](CUST-002-list-view-stale-title.md) |
| CUST-003 | Splitter paint performance (attempted) | Deferred (three iterations tested; all reverted) | [CUST-003-splitter-paint-performance.md](CUST-003-splitter-paint-performance.md) |
| CUST-004 | Automated UI performance/artefact harness | In progress (implemented/built; interactive baseline pending) | [CUST-004-ui-perf-harness.md](CUST-004-ui-perf-harness.md) |
| CUST-005 | Deferred window creation for delay-loaded tasklists | Done | [CUST-005-delayed-tasklist-window.md](CUST-005-delayed-tasklist-window.md) |
| CUST-006 | No throwaway tasklist at startup | Done | [CUST-006-no-throwaway-tasklist.md](CUST-006-no-throwaway-tasklist.md) |
| CUST-007 | Comments editor controls moved to the main toolbar | Done (date/time buttons stay; format-combo move planned to be reverted — see CUST-019) | [CUST-007-comments-toolbar-in-main-toolbar.md](CUST-007-comments-toolbar-in-main-toolbar.md) |
| CUST-008 | Hide mouse pointer while typing in text controls | Done | [CUST-008-comments-hide-pointer-while-typing.md](CUST-008-comments-hide-pointer-while-typing.md) |
| CUST-009 | Highlight Quick Find matches in the comments editor | Done | [CUST-009-quickfind-comments-highlight.md](CUST-009-quickfind-comments-highlight.md) |
| CUST-010 | Auto-indent (Return and wrapped lines) in the comments editor | In progress (implemented; covered by TDLTest) | [CUST-010-comments-auto-indent.md](CUST-010-comments-auto-indent.md) |
| CUST-011 | IntelliJ-style MRU tasklist switcher for Ctrl+Tab | In progress (implemented/built; manual test pending) | [CUST-011-mru-tasklist-switcher.md](CUST-011-mru-tasklist-switcher.md) |
| CUST-012 | Product name and release label from version resource; About layout | Done (built; manual About check pending) | [CUST-012-branding-and-about.md](CUST-012-branding-and-about.md) |
| CUST-013 | Window menu lists up to 25 tasklists and shows the open count | In progress (implemented/built; manual check pending) | [CUST-013-window-menu-list.md](CUST-013-window-menu-list.md) |
| CUST-014 | Quote-safe values in ToDoList.ini (Quick Find history) | Done | [CUST-014-quoted-ini-values.md](CUST-014-quoted-ini-values.md) |
| CUST-015 | Undo/redo feedback, enablement and saved-state tracking | Done | [CUST-015-undo-redo-feedback.md](CUST-015-undo-redo-feedback.md) |
| CUST-016 | Restore tree folding when a filter is cleared | In progress (implemented/built; manual filter test pending) | [CUST-016-filter-fold-restore.md](CUST-016-filter-fold-restore.md) |
| CUST-017 | Functional UI automation framework (UIA/FlaUI proposal) | Proposed | [CUST-017-ui-automation-framework.md](CUST-017-ui-automation-framework.md) |
| CUST-018 | Comments editor word-wrap and show-all-characters toolbar toggles | In progress (implemented/built; interactive check pending) | [CUST-018-comments-display-toggles.md](CUST-018-comments-display-toggles.md) |
| CUST-019 | Toolbar architecture: command tables, split bars, main-toolbar customization | In progress (phase 1 done: data-driven main toolbar; later phases proposed) | [CUST-019-toolbar-architecture.md](CUST-019-toolbar-architecture.md) |
| CUST-020 | Tri-state view toolbar button (task list / editor / split) | Proposed (design note) | [CUST-020-tristate-view-toolbar-button.md](CUST-020-tristate-view-toolbar-button.md) |
| CUST-021 | Fixed per-tasklist open/activation cost in a running app | Proposed (runtime analysis complete; instrumentation next) | [CUST-021-tasklist-open-activation-cost.md](CUST-021-tasklist-open-activation-cost.md) |

