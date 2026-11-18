# UI-008 — Filter bar: integrated text filter and Quick Find

| Field | Value |
|---|---|
| Status | Proposed (revised scope: single combined control first; compact layout later) |
| Upstream baseline | ToDoList 9.3.DP7 |
| Scope | `Core/ToDoList/TDLFilterBar.*`, `Core/ToDoList/ToDoListWnd.*`, `Core/ToDoList/TDLQuickFindComboBox.*`, `Core/ToDoList/FilteredToDoCtrl.*`, `Core/ToDoList/ToDoCtrl.*`, `Core/ToDoList/TDCStruct.*`, `Core/ToDoList/ToDoList.rc` |
| Related | [index.md](../index.md), [UI-006](UI-006-toolbar-architecture.md) (phase 4), [UI-005](UI-005-filter-fold-restore.md), [TED-003](../text-editor/TED-003-quickfind-comments-highlight.md), [GEN-002](../general/GEN-002-quoted-ini-values.md) |

## Goal (revised)

Focus the first implementation on the highest-value change and avoid a large filter-bar
rewrite:

1. **Combine the title/comments filter and Quick Find in one control.**
2. Make that control the **first control** of the filter bar. All other controls follow to
   the right in their existing order and with their existing labels.
3. Keep the current user-chosen filter set from preferences/visibility. Do **not** add
   dynamic `+`/`-` buttons or change the label layout in the first implementation.
4. Preserve the option to separate filtering from searching later: a **second**
   filter/search field, opened from the first control's context menu.
5. Later, optionally let a text field be configured as filter or search-only (whether the
   result shrinks the tree), again from the context menu.
6. Treat advanced filters as out of scope for the first implementation. They keep their
   current behaviour; no new layering or special support.
7. The compact layout with left-side labels and on-demand attribute filters remains a
   later phase, not part of the first implementation.

The intent is to reuse the existing filter and Quick Find machinery as far as possible and
avoid building a separate result list or preview widget.

## Verdict

This is a better first step than the compact/dynamic-filter design. It delivers the
important improvement (one predictable text control that filters and searches) without
touching the riskiest areas: label layout, visibility configuration, dynamic controls and
advanced filters.

Key conclusions:

- **One control, two established behaviours.** The control writes
  `TDCFILTER::sTitle` for filtering and uses the existing `SelectNextTask()` /
  `FindInComments()` path for searching. It does not need a new filter model for the first
  version.
- **A second text field is naturally search-only.** Because Quick Find already walks the
  *visible* task tree, a search-only field opened after the primary filter automatically
  searches only the prefiltered tasks. That gives the two-step workflow without adding a
  second filter rule.
- **Do not support multiple filtering text fields yet.** Two fields that both shrink the
  tree require ordered AND rules and a change to the filter model. A search-only second
  field avoids that.
- **Do not build a results preview.** The filtered tree, the selected task in the comments
  editor, and the existing status-bar counts already provide the feedback. A popup
  result list would be a separate feature.
- **Advanced filters are not part of this work.** They remain available exactly as today
  through the existing `Show` combo and View menu, and continue to disable the simple text
  filter as they do now.

## Scope of the first implementation

| Included now | Deferred |
|---|---|
| Replace the title edit with a composite text filter/search control | Left-side labels |
| Make it the first control in the filter bar | Dynamic `+`/`-` attribute filters |
| Keep the existing filters, labels and `Show`/`Options` controls | Synchronising the shown set with visibility state |
| Filter + next/previous + comments highlight | Second optional text/search field |
| Text scope context menu | Per-field "filter results" toggle |
| Debounced filtering and unclosed-quote handling | Advanced-filter support/layering |
| Route toolbar Quick Find commands to the control | Results preview/popup list |

## What "advanced filter" means (out of scope)

An advanced filter is the named search built in the Find Tasks dialog and represented by
`TDCADVANCEDFILTER { CString sName; SEARCHPARAMS params; }`
(`Core/ToDoList/TDCStruct.h:587-610`). It can contain arbitrary search rules, dates, custom
attributes and boolean combinations. It is not a column and not one of the filter-bar
attribute controls.

- Saved advanced filters appear in the `Show` combo as `FS_ADVANCED` entries and in the
  View menu (`Core/ToDoList/TDCMainMenu.cpp:871`, `:930-957`).
- Activating one replaces the simple filter: `CTDCFilter::SetAdvancedFilter()` resets the
  simple `m_filter` to `FS_ADVANCED`
  (`Core/ToDoList/TDCFilter.cpp:164-179`). The simple text filter and an advanced filter
  are therefore mutually exclusive today.
- The advanced filter is saved with the per-tasklist filter state
  (`TDCFilter.cpp:491-517`).

The first implementation does not change this. When an advanced filter (or the Selected
filter) is active, the combined control follows the current rule: the attribute filters,
including the title text, are disabled (`TDLFilterBar.cpp:768-787`,
`ToDoListWnd.cpp:12817-12836`). Making text filter on top of an advanced filter would be a
separate feature.

## Current implementation (facts)

### The bar and its layout

- `CTDLFilterBar : CDialog, CDialogHelper`, created from `IDD_FILTER_BAR`
  (`Core/ToDoList/TDLFilterBar.h:33`, `TDLFilterBar.cpp:67-70`).
- The dialog template pre-creates every standard control and label; the title edit is at
  `IDC_TITLEFILTERTEXT`, the text filter label at `IDC_TITLEFILTERLABEL`, the filter
  selector at `IDC_SHOWFILTERCOMBO` and the options combo at `IDC_OPTIONFILTERCOMBO`
  (`Core/ToDoList/ToDoList.rc:709-746`).
- `FILTERCTRLS` is a fixed table mapping control ID → label ID → `TDC_ATTRIBUTE`
  (`TDLFilterBar.cpp:38-60`). `GetControls()` returns the standard controls in that fixed
  order, then dynamically created custom-attribute controls, then the options combo
  (`TDLFilterBar.cpp:962-990`).
- DDX reads and writes the whole `TDCFILTER` from those controls
  (`TDLFilterBar.cpp:97-167`). Date controls have buddy controls that appear only for the
  `FD_USER` and `FD_NEXTNDAYS` modes (`TDLFilterBar.cpp:854-872`, `391-414`).
- `ReposControls()` places labels above controls, gives every control a fixed `CTRLLEN` of
  75 DLU and wraps when `nXPosDLU + CTRLLEN > nWidthDLU`
  (`TDLFilterBar.cpp:32-36`, `816-960`). All calculations are in DLU until the final move.
- Combo boxes get an extra DPI-scaled 200 px on the bottom when moved, for the dropdown
  (`TDLFilterBar.cpp:905-913`).
- `UpdateDropListData()` destroys and recreates a combo when its `CBS_SORT` state changes
  (`TDLFilterBar.cpp:599-619`), so visibility/z-order/focus must be re-applied.
- Custom-attribute filter controls are built dynamically by
  `CTDCCustomFilterAttributeUIHelper::RebuildControls()` for enabled definitions with
  `TDCCAF_FILTER` (`Core/ToDoList/TDCCustomFilterAttributeUIHelper.cpp:300-402`).

### Visibility and persistence

- Visibility is the tasklist's visible filter-field set. `WantShowFilter()` asks
  `m_mapVisibility.Has(nType)`, copied from the active tasklist's
  `GetVisibleFilterFields()` (`TDLFilterBar.cpp:544-560`, `768-787`).
- The supported standard filter attributes are a fixed subset of `TDC_ATTRIBUTE`
  (`Core/ToDoList/TDCStruct.cpp:3327-3363`). Custom attributes are not in this list; they
  are shown whenever their definition is enabled and has `TDCCAF_FILTER`.
- The visibility state is persisted in the per-tasklist preferences by default:
  `CToDoCtrl::SaveAttributeVisibility(CPreferences&)` →
  `TDCCOLEDITFILTERVISIBILITY::Save()` under `[FileStates\<key>]`
  (`Core/ToDoList/ToDoCtrl.cpp:8693-8696`, `TDCStruct.cpp:3299-3320`;
  `docs/APPLICATION_STATE.md:20`).
- It can optionally be embedded in the `.tdl` file as `ATTRIBVIS/FILTER` when
  `TDCS_SAVEUIVISINTASKLIST` is set (`ToDoCtrl.cpp:8687-8691`,
  `TaskFile.cpp:1024-1057`, `Core/Interfaces/TasklistSchemaDef.h:45-49`).
- The filter itself is per tasklist and is saved as `...\Filter` by
  `CFilteredToDoCtrl::SaveState()`/`LoadState()`
  (`Core/ToDoList/FilteredToDoCtrl.cpp:233-246`).
- The "filter bar visible" flag is global: `[Settings]ShowFilterBar`
  (`Core/ToDoList/ToDoListWnd.cpp:2679`, `:2970`, toggled at `:12705-12722`).

### Frame integration and sizing

- The frame owns the bar by value and calls it for creation and preference propagation
  (`ToDoListWnd.cpp:1554-1576`), layout (`:6964-6981`), height comparison/resize
  (`:8985-9002`), filter application (`:12779-12844`) and display data refreshes
  (`:8956-8983`).
- `ReposFilterBar()` asks for `CalcHeight(width)` and advances the tasklist area
  (`ToDoListWnd.cpp:6964-6981`). Any height change must be followed by a frame `Resize()`.
- The filter is frame-level but applies to the active tasklist; `DoChangeFilter()` calls
  `SetFilter`/`SetAdvancedFilter`/`ClearFilter` on the active `CFilteredToDoCtrl`
  (`ToDoListWnd.cpp:12793-12844`; `docs/ARCHITECTURE.md:270-274`).

### Quick Find today

- Quick Find is a separate combo in the main toolbar after `ID_EDIT_FINDTASKS`
  (`ToDoListWnd.cpp:1661-1691`), repositioned from the toolbar command index
  (`:6854-6870`).
- It is an `CAutoComboBox` with delete/add-to-start/autocomplete and prev/next buttons
  (`Core/ToDoList/TDLQuickFindComboBox.h:19-50`). History is stored in `[QuickFind]`
  (`ToDoListWnd.cpp:1684-1689`, `:2696-2699`).
- Typing calls `ProcessQuickFindTextChange()`, stores `m_sQuickFind` and calls
  `CFilteredToDoCtrl::SelectNextTask(..., TDC_SELECTNEXTINCLCURRENT)`
  (`ToDoListWnd.cpp:1874-1908`). Next/previous are separate commands
  (`ToDoListWnd.cpp:1765-1815`).
- `SelectNextTask()` builds one `FOP_INCLUDES` rule with `TDCA_ANYTEXTATTRIBUTE`, then
  walks the task tree via `FindNextTask()`
  (`Core/ToDoList/ToDoCtrl.cpp:8827-8898`, `Core/ToDoList/ToDoCtrlFind.cpp:259-276`).
  It therefore visits only tasks present in the filtered presentation.
- Comments highlighting runs after a 50 ms one-shot timer so the comments control has the
  newly selected task; `FindInComments()` selects the match
  (`ToDoListWnd.cpp:1910-1924`, `:7456-7459`;
  [TED-003](../text-editor/TED-003-quickfind-comments-highlight.md)).
- The title filter has three scopes (`TDCEnum.h:716-721`): title only, title or comments,
  any text attribute. The filter maps them to `TDCA_TASKNAME`,
  `TDCA_TASKNAMEORCOMMENTS` and `TDCA_ANYTEXTATTRIBUTE`
  (`Core/ToDoList/TDCFilter.cpp:429-446`). Quick Find always uses
  `TDCA_ANYTEXTATTRIBUTE`, so filter scope and search scope can differ.
- The search parser keeps quoted phrases together but does not report an unclosed quote
  (`Core/Shared/Misc.cpp:2292-2340`).
- The existing title edit applies on Enter or the update button, not on every keystroke
  (`TDLFilterBar.cpp:452-464`).

## Design for the first implementation

### 1. One composite control, first in the bar

Add `CTDLTextFilterSearchCtrl` and use it in place of `m_eTitleFilter`:

- an edit field with autocomplete/history;
- previous/next buttons;
- a scope button with a context menu;
- a clear affordance if cheap;
- notifications: text changed, apply now, find next, find previous, scope changed,
  add second field (later).

The control does not know the tasklist. The bar/frame applies the filter; a frame-owned
`CTDLQuickFindController` performs selection and comments highlighting. This mirrors the
separation already implicit in `ProcessQuickFindTextChange()` but removes the direct
dependency on `m_cbQuickFind`.

First-version layout:

- Move the composite to the first position in `FILTERCTRLS`, before `IDC_SHOWFILTERCOMBO`.
- Keep all other controls in their current order and keep their labels.
- Give the composite a larger width than a normal combo (about two normal fields) so the
  edit, prev/next and scope button fit.
- Keep `Options` last, as `GetControls()` already does.
- Do not change the label-above layout yet.
- Preserve `CanPasteText()` and the Enter handling for the new control
  (`TDLFilterBar.cpp:452-464`, `1284-1287`).

**DDX impact**

The composite cannot use `DDX_Text` on `m_eTitleFilter` anymore. The bar should:

- read the composite text into `m_filter.sTitle` on apply;
- write `m_filter.sTitle` back into the composite in `DoDataExchange(FALSE)`;
- keep the rest of the `TDCFILTER` DDX unchanged.

This is local to `CTDLFilterBar`; `DoChangeFilter()` still receives a normal `TDCFILTER`.

### 2. Default behaviour: filter + search

The primary control defaults to both:

- **Filter**: non-empty text is copied to `TDCFILTER::sTitle` and the frame applies it like
  today. Empty text clears only the title part, preserving other filter fields.
- **Search**: after the filter has been applied, select the first/next matching task and
  schedule the comments highlight. This reuses `SelectNextTask()` and `FindInComments()`.
- **Next/Previous**: buttons on the control move through the visible (already filtered)
  results and highlight comments. Wrap to first/last like Quick Find today.
- **Scope**: the control's context menu changes what the text matches. The first version
  should at least expose the existing three scopes; extending them is a separate decision.
  The scope must be used by both filtering and search. Today Quick Find always uses
  `TDCA_ANYTEXTATTRIBUTE`, while the filter can be title-only
  (`TDCFilter.cpp:429-446`, `ToDoCtrl.cpp:8827-8829`); use one scope for both to avoid
  selecting tasks whose comments match a title-only filter.
- **No results**: do not show the modal message while typing. The filtered tree is the
  feedback. For explicit next/previous with no match, keep the existing status/behaviour
  or show a non-modal message.

Limit the first version to the existing filter/search semantics. Do not add a separate
result list.

### 3. Debounce and quoted text

The control should not rebuild on every keystroke:

- **Unquoted text**: apply after about 250-300 ms of idle input.
- **Quoted text**: if the number of `"` characters is odd (an unclosed phrase), do not
  apply. Apply immediately when the closing quote makes the count even.
- **Enter**: apply immediately.
- **Cleared text**: apply immediately (clear the title part).
- **After applying**: select the first/next match and schedule the comments highlight once.
- Navigation buttons act immediately.

The parser does not report an unclosed quote (`Misc.cpp:2292-2340`), so add a small helper
(for example `HasUnclosedSearchQuote(text)`) or extend the parser. Keep the same quote
semantics as the existing parser (no escape syntax; odd quote count means open).

### 4. Optional second text/search field (later)

A second field addresses the "filter first, then search separately" workflow:

- Opened from the first control's context menu ("Add search field").
- Placed to the right of the first control; it has its own prev/next buttons and text.
- **Default: search-only.** It does not modify `TDCFILTER::sTitle` and does not shrink the
  tree. It navigates within the already-filtered visible tasks, which is exactly what
  Quick Find already does (`ToDoCtrl.cpp:8858-8897`, `ToDoCtrlFind.cpp:259-276`).
- This preserves the existing single-title-filter model. No ordered multiple filter rules
  are needed.
- F3/Shift+F3 and the toolbar Quick Find commands should target the focused or last-used
  text field. If only one exists, that one is the target.
- The second field can share the `[QuickFind]` history or have its own; sharing is simpler
  and reuses existing INI data.

If a later requirement really needs two **filtering** text fields, the model needs an
ordered list of text rules (AND semantics) and persistence for their order. That is a
separate feature; do not implement it by concatenating strings or by letting the two fields
fight over `sTitle`.

### 5. Optional filter-vs-search toggle (later)

Add a context-menu option such as "Filter results" to each text field:

- **Checked (filter)**: the field writes `sTitle` and shrinks the tree.
- **Unchecked (search-only)**: the field navigates the current visible tasks and
  highlights; it does not write `sTitle`.

This is useful mainly for the second field, so it can behave like a pure search. Keep the
primary field checked by default.

State questions to settle before implementing:

- If the primary field is switched to search-only, what happens to the existing
  `sTitle` filter? Recommended: leave it unchanged and show that the field is search-only;
  or clear it explicitly. Silent partial clearing is dangerous.
- Is the toggle per document or global? Recommended: per document for the primary field,
  because the filter value is per document; per field for additional fields.
- The toggle must be visible (icon/tooltip), not only hidden in a context menu, otherwise
  the user cannot tell why the tree is or is not shrinking.

### 6. Existing controls and advanced filters

- `Show`, `Options`, date/time, priority/risk, user lists and custom attributes continue to
  behave as today. The only order change is that the text control comes first.
- Advanced filters remain selected through the existing `Show` combo and View menu. No new
  integration.
- When `FS_ADVANCED` or `FS_SELECTED` is active, the text control follows the current
  disabled/hidden rule (`TDLFilterBar.cpp:768-787`) unless the later search-only mode is
  deliberately used to navigate that result. Do not change this in the first version.

### 7. Results count / preview

Do not add a popup result list in this work. It would require a new list/tree
implementation and new selection/scroll coordination. The existing filtered tree, the
selected task, the comments highlight and the status-bar counts are the feedback.

If a count is considered later, an "X matching tasks" status-bar text is much cheaper than
a popup and reuses the filtered presentation. This is a possible follow-up, not a
requirement.

## Later phase: compact layout and on-demand filters

The original compact design remains a valid later phase, but it is not part of the first
implementation:

- labels to the left of controls;
- one-row height calculation and wrapping;
- text control first at double width;
- `+`/`-` attribute filters;
- reuse the tasklist's existing visible-filter-field state as the single source of truth;
- `Show`/`Options` behind an overflow if desired.

When that phase is taken up, keep the earlier conclusions:

- The visibility list is editable only in `TDLSA_ANY`; the first `+`/`-` must convert
  `TDLSA_ALL`/`TDLSA_ASCOLUMN` to `TDLSA_ANY` while preserving the derived set
  (`TDCStruct.cpp:3190-3196`, `3271-3295`, `3349-3361`).
- `+`/`-` update the active tasklist's visibility state, then refresh the bar and resize
  the frame. Reuse the Select Columns flow (`ToDoListWnd.cpp:12915-12960`); do not create a
  second filter list.
- Removal should be a label/context-menu action or a per-filter `-`; decide whether it
  clears the filter value.
- Custom attributes are outside the standard visibility map; keep the current
  always-when-enabled behaviour initially, or add stable-ID visibility later.

## Technical challenges and risks

### 1. One control, two state owners (high)

The control's text belongs to the per-tasklist filter when it filters, but to the global
Quick Find state when it searches. With only the default filter+search behaviour this is
simple (`sTitle` owns the text). Adding the filter/search toggle later creates ambiguity:
switching modes, switching tasklists and clearing text must not silently destroy a filter
or leave a stale search. Define the ownership rules before adding the toggle.

### 2. Filter + search ordering and timing (high)

Typing currently triggers Quick Find selection on each edit change
(`ToDoListWnd.cpp:1817-1820`, `1874-1908`), while the title filter applies only on
Enter/update (`TDLFilterBar.cpp:452-464`). Combining them needs a clear sequence: debounce,
apply filter, then select/highlight. The comments highlight is already deferred by 50 ms
(`ToDoListWnd.cpp:1918-1924`), showing the ordering sensitivity.

### 3. Filter scope vs. search scope (high)

Quick Find always searches `TDCA_ANYTEXTATTRIBUTE` (`ToDoCtrl.cpp:8827-8829`), while the
filter can be title-only or title/comments (`TDCFilter.cpp:429-446`). Without a scope-aware
`SelectNextTask` overload, a title-only filter can select a task because of a comments
match. Add the overload (the internal `CToDoCtrl` version already accepts an attribute
scope, `ToDoCtrl.h:733`; the public `CFilteredToDoCtrl` version does not,
`FilteredToDoCtrl.h:36`) and use the same scope for filtering, navigation and highlight.

### 4. Two filtering text fields would need a model change (high, if attempted)

`TDCFILTER` has one `sTitle`. Two filtering fields require ordered AND rules and their own
persistence. The recommendation is to make the second field search-only. If two filters
are truly needed later, extend the filter model deliberately; do not concatenate strings.

### 5. Scope enum extension and persistence (medium-high)

`FILTER_TITLE` is persisted in preferences and per-tasklist filter state
(`TDCEnum.h:716-721`, `TDCFilter.cpp:637`, `:704`). New values must be appended. The label,
filter dialog, preference page, filter rules and search scope must all understand them.
The current refresh path overwrites the per-tasklist `nTitleOption` from the global
preference (`ToDoListWnd.cpp:8970`, `TDLFilterBar.cpp:571`); decide whether the scope button
changes a per-document scope (stop overwriting) or the global default.

### 6. Layout change for the composite control (medium-high)

Even without the full compact layout, the first control needs more width than the fixed
75 DLU used by `ReposControls()`, and it contains multiple sub-controls (edit, prev/next,
scope). The layout engine needs a per-control/preferred-width exception and correct tab
order and paint exclusion. The existing labels-above geometry remains, so this is much
smaller than the left-label rewrite, but it is not zero.

### 7. Replacing the title edit and DDX (medium)

`m_eTitleFilter` is used by `DDX_Text`, `PreTranslateMessage`, `CanPasteText` and the
prompt manager (`TDLFilterBar.cpp:97-167`, `452-464`, `1004-1008`, `1284-1287`). Replacing
it with a composite requires manual get/set of `m_filter.sTitle` and preserving the Enter,
paste and placeholder behaviour. Do not leave two sources of truth.

### 8. Quick Find command and focus plumbing (medium)

`ID_EDIT_QUICKFIND/NEXT/PREV` currently focus or depend on the toolbar combo
(`ToDoListWnd.cpp:1754-1815`); paste and focus handling also special-case `m_cbQuickFind`
(`ToDoListWnd.cpp:2022-2041`, `13823-13826`). After the merge these must target the active
bar control, and the commands must decide what to do when the bar is hidden. The toolbar
combo can remain temporarily as an alias while the new control is validated, then be
removed for UI-006 phase 4.

### 9. Advanced/Selected modes remain unchanged (medium)

The text control is disabled/hidden in `FS_SELECTED` and `FS_ADVANCED` today
(`TDLFilterBar.cpp:768-787`). Keeping that behaviour is correct for this scope, but the
control is now prominent and users may expect it to work in every mode. Explain via
tooltip/disabled state. Do not attempt layering in this phase.

### 10. No results preview (medium)

There is no plan to show a result list or count beyond the filtered tree and status bar.
Users who expect a search-results preview will not get one. The filter-first workflow
compensates by making the result set visible in the tree. If needed, add a status-bar match
count later.

### 11. Verification coverage (medium)

There is no `TDLTest` coverage for the filter bar or Quick Find. Add unit tests for the
quote/debounce decision, scope mapping, and `sTitle` get/set round-tripping. Use the
interactive harness for focus, resize and tasklist switching.

## Alternatives considered

| Option | Assessment |
|---|---|
| One combined filter+search control first | **Recommended first step.** Smallest change that delivers the main benefit |
| Separate filter control and separate search control | Preserves conceptual clarity but leaves two text entry points and does not deliver the combined workflow |
| Two filtering text fields | Requires ordered AND rules and a filter-model change; defer |
| Second field as search-only | **Recommended later extension.** Reuses the current single-filter model |
| Popup results list/preview | New treelist, selection and scrolling work; out of scope |
| Compact layout with `+`/`-` first | Larger risk; do it after the combined control is proven |
| Compact-bar active-filter list separate from visibility | Rejected: duplicates state and persistence |
| Support advanced filters in the combined control | Requires a layered search-params model; explicitly out of scope |

## Phased migration plan

| Phase | Work | Primary files | Risk |
|---|---|---|---|
| 0 | Extract `CTDLQuickFindController` (text, history, next/prev, highlight timer) and route the existing toolbar commands through it. No visual change. | `ToDoListWnd.*`, new controller | Low |
| 1 | Add scope-aware `SelectNextTask` and `CTDLTextFilterSearchCtrl`. Replace the title edit, move the control first, keep labels and all other controls. Default filter+search, debounce and unclosed-quote rule. | `FilteredToDoCtrl.*`, `ToDoCtrl.*`, `TDLFilterBar.*`, `ToDoListWnd.*`, `TDCEnum.h`, `TDCFilter.cpp` | Medium-high: layout/DDX/focus/timing |
| 2 | Route toolbar Quick Find and F3/Shift+F3 to the new control; keep or retire the toolbar combo. | `ToDoListWnd.*`, `TDLQuickFindComboBox.*` | Medium: focus/commands/history |
| 3 | Add the optional second text/search field from the context menu. Default search-only; active-field routing for next/prev. | `TDLFilterBar.*`, controller | Medium: focus, history, tasklist switching |
| 4 | Add the per-field "Filter results" toggle, with clear state rules and indicators. | bar/controller, preferences | High: filter vs search state ownership |
| 5 | Optional compact layout: left labels, measured widths, `+`/`-`, sync with visible-filter state, Show/Options overflow. | bar/layout helper, frame | High: layout/DPI/sync |
| 6 | Optional advanced-filter support, only if a layered model is designed. | filter model, Find Tasks, filter bar | Very high; explicitly deferred |

Phases 0-2 already deliver the main improvement and can ship without the second field or
any compact-layout work.

## Verification

- Build `Core\ToDoList_Core.sln` in `Unicode Debug` and `Unicode Release`.
- Run `Core\TDLTest\Unicode_Debug\TDLTest.exe`.
- Add unit tests for:
  - quote/debounce decisions (unclosed quote, closing quote, Enter, cleared text);
  - scope mapping from `FILTER_TITLE` to `TDC_ATTRIBUTE`;
  - `m_filter.sTitle` round-tripping through the composite control.
- Interactive checks with a temp copy of a tasklist:
  - the control is first; all other filters and labels are where they were;
  - typing filters after the debounce; an unclosed quote does not filter; closing it does;
  - Enter applies immediately; clearing restores the unfiltered view;
  - next/previous navigate only the filtered results and highlight comments;
  - title-only scope does not navigate to comment-only matches;
  - existing attribute filters, `Show`, `Options` and custom filters still work;
  - `Selected` and advanced filters keep the current disabled/hidden behaviour;
  - switching tasklists shows the right filter text and does not leak search state;
  - restart persistence of filter text, scope and Quick Find history;
  - high DPI, narrow window, focus/Tab order, paste into the control and toolbar Quick
    Find commands;
  - no assertions in `%TEMP%\ToDoList_asserts.log`.

## Decisions still needed

1. Exact scope set in the control's context menu. Start with the existing three scopes, or
   add comments-only/any-attribute immediately?
2. Is scope per document (using `TDCFILTER::nTitleOption`) or global (the existing
   preference)? If per document, remove the refresh overwrite.
3. Should the toolbar Quick Find combo remain as a fallback after the bar control ships,
   or be removed immediately for UI-006 phase 4?
4. For the second field: should it always be search-only, or expose the "Filter results"
   toggle from the start? The document recommends search-only first.
5. Should the second field have its own scope, or share the primary field's scope?
6. Should the second field share the `[QuickFind]` history or have separate history?
7. When the primary "Filter results" toggle is later unchecked, should an existing
   `sTitle` filter be left in place, cleared, or preserved and shown as inactive?
8. Is a status-bar match count wanted later, or is the filtered tree sufficient feedback?

## Non-goals

- Advanced-filter support/layering in the combined control.
- Two filtering text fields and ordered text-filter rules.
- Dynamic `+`/`-` attribute filters and left-side labels in the first implementation.
- A search-results preview/popup list.
- Changing `.tdl` file format or filter visibility persistence.
- Changing filter/search matching semantics beyond the debounce and unclosed-quote rules.
