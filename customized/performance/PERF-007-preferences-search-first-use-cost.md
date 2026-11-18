# PERF-007: Preferences search is slow on first use

Status: **Proposed** (root cause identified; not measured, not implemented)

## Goal

Remove the multi-second stall the user sees the first time the Preferences search box is
used in a dialog session. Subsequent searches in the same session are already fast; the cost
is concentrated in the first search and is repeated each time the dialog is reopened.

No source changes were made for this analysis.

## Bottom line

The first search is slow because it **forces all 16 preference pages to be created and shown
once**, work that is deliberately deferred until a page is visited. That creation is the
stall, not the text matching itself:

1. Pages are created lazily: on a normal dialog open only the initially selected page
   exists as a window (`Core/Shared/PropertyPageHost.cpp:217-258` `EnsurePageCreated`,
   called from `SetActivePage` `:311-364`).
2. Search cannot match text in controls that do not exist, so the search handler creates
   every remaining page up front, under a wait cursor and a redraw lock, and then
   **shows and hides each page** explicitly (`Core/ToDoList/PreferencesDlg.cpp:1011-1037`,
   the `ShowWindow(SW_SHOWNOACTIVATE)` / `ShowWindow(SW_HIDE)` pair at `:1033-1034`).
   The source comment states this is "to trigger translation as well as ensuring all
   content is ready".
3. Showing a page runs its full first-show initialisation: `CPreferencesPageBase::OnShowWindow`
   calls `OnFirstShow()` once (`Core/Shared/PreferencesBase.cpp:331-345`). For the expensive
   pages this is real work, e.g. `CPreferencesShortcutsPage::OnFirstShow` builds the whole
   command tree (`Core/ToDoList/PreferencesShortcutsPage.cpp:84-100`,
   `m_ctrlCommands.BuildMenuTree()`).
4. Only afterwards does matching run: `AddPagesToTree(TRUE)` →
   `CPreferencesPageBase::HighlightUIText` (`Core/Shared/PreferencesBase.cpp:352-355`) →
   `CCtrlTextHighlighter::HighlightUIText` (`Core/Shared/CtrlTextHighlighter.cpp:36-67`),
   which recursively walks every child window (`FindMatchingCtrls`, `:265-285`) and reads
   text (`TextContainsOneOf`, `:150-179`), enumerating **all** combo and list items via
   `CDialogHelper::GetComboBoxItems` / `GetListBoxItems`
   (`Core/Shared/DialogHelper.cpp:677`, `:967`).

On the second and later searches `CPropertyPageHost::PagesAreAllCreated()`
(`PropertyPageHost.cpp:278-298`) returns TRUE, so the whole creation block is skipped and
only the highlight walk runs. That is why only the first search in a session is slow.

Secondary factors that ride on the same first-use path:

- The UI-010 tooltip work calls `InitTooltips()` from each page's `OnInitDialog`, so tooltip
  string lookup is part of page creation too (small relative to the above).
- `AddPagesToTree(TRUE)` rebuilds the tree and then `Resize()` runs, twice on the first
  search (the fallback at `PreferencesDlg.cpp:1042-1043` only if no page matched).

## Where the time goes (expected profile)

Not yet measured; the plan is to confirm with the existing logging/timer infrastructure
(see [PERF-006](PERF-006-tasklist-open-activation-cost.md) §Method) or the
[PERF-003](PERF-003-ui-perf-harness.md) harness before changing code. The expected split:

| Component | Expected share | Evidence |
|---|---|---|
| Creating 15 page windows (`Create` + `OnInitDialog` + DDX + tooltips) | large | `PreferencesDlg.cpp:1019-1036`, `PropertyPageHost.cpp:217-258` |
| Showing/hiding each page → `OnFirstShow` (shortcuts command tree, visibility list fill, group lines, combo fills) | large | `PreferencesBase.cpp:331-345`, `PreferencesShortcutsPage.cpp:84-100` |
| Highlight walk over all controls + combo/list items | small-medium | `CtrlTextHighlighter.cpp:150-179`, `:265-285` |
| Tree rebuild twice + `Resize()` | small | `PreferencesDlg.cpp:1039-1045` |

If instrumentation shows the highlight walk dominating instead (likely only on the
shortcuts and visibility pages, which have the most items), the caching option below is the
right lever; otherwise the creation/show cost is.

## Proposed fix

### Option A — pre-create pages after the dialog is open (cheapest, recommended first step)

Move the "all pages exist" cost off the user's first search and off the first frame:

1. In `CPreferencesDlg::OnInitDialog` (`PreferencesDlg.cpp:226-246`) post a private message
   after the dialog is shown.
2. In the handler, create all remaining pages inside a `CHoldRedraw`, without the explicit
   show/hide pair. Determine what translation actually needs:
   - If `CPropertyPage::Create` already translates the page (it goes through the normal
     dialog creation path and `CLocalizer`), nothing more is needed.
   - If translation genuinely requires a show, call the localiser directly on the page
     (the pattern used elsewhere with `CLocalizer`) instead of showing it.
3. Make `OnUpdateSearch` keep the "create all" block as a fallback only, so a search issued
   before the posted message runs still works.

Result: the first search finds all pages already created and only runs the highlight walk.
Cost moves to a moment where the user is not waiting on a typed search, and creating hidden
pages is materially cheaper than creating **and showing** them because `OnFirstShow` work is
deferred until a page is first shown.

### Option B — defer `OnFirstShow` work until a page is actually visited

`OnFirstShow` is triggered by `OnShowWindow` (`PreferencesBase.cpp:331-339`). Because the
search shows every page once, all first-show work runs. If sparse pages keep expensive
`OnFirstShow` (command tree, visibility list), make that work demand-driven per page:

- Optionally split `OnFirstShow` into "cheap layout" (always) and "content build"
  (only when the page is the active one), and have the search never trigger content build.
- The shortcuts and visibility pages are the main beneficiaries.

This is a small refactor of two overrides plus the base hook, and it directly removes the
most expensive part of "showing all pages".

### Option C — cache per-page search text

If profiling still shows the highlight walk dominates:

- Build a per-page index once (page path + control texts + matching HWNDs), reuse it for
  every search in the dialog session.
- Invalidate on control change, or rebuild at dialog open and accept staleness for the
  session (most matching text is static labels and combo items).
- Keep the `HighlightUIText` virtual in sync: `CPreferencesShortcutsPage::HighlightUIText`
  (`PreferencesShortcutsPage.cpp:341-...`) adds tree highlighting and cannot be replaced by
  a plain control-text index.

## Recommended sequence

1. Instrument (cheap): log timings around the creation loop, the per-page show/hide, and the
   highlight walk in `OnUpdateSearch` to confirm the split.
2. Implement A + B (likely enough): first search becomes instant, and dialog open stays
   responsive because the work happens hidden after the first paint.
3. Re-measure; add C only if the highlight walk is still significant.

## Files likely touched

- `Core/ToDoList/PreferencesDlg.cpp` — `OnUpdateSearch` (`:981-1049`), `OnInitDialog`
  (`:226-246`), a new post-open handler, `AddPagesToTree` call sites.
- `Core/ToDoList/PreferencesShortcutsPage.cpp` — `OnFirstShow` (`:84-100`) split if B.
- `Core/ToDoList/PreferencesUIVisibilityPage.cpp` — `OnFirstShow` (`:68-74`) split if B.
- `Core/Shared/PreferencesBase.h/.cpp` — base `OnFirstShow` hook (`:331-345`) if B.
- `Core/Shared/PropertyPageHost.cpp/.h` — a light "create hidden" helper if useful.
- `Core/Shared/CtrlTextHighlighter.cpp/.h` — only if C.
- `customized/index.md` — add this PERF-007 row.

## Risks

- Creating all pages up front may still slow dialog open on very slow machines. Keep it
  inside `CHoldRedraw` and behind a posted message so the dialog paints first, and verify on
  `Unicode Debug` (which is slower) as well as Release.
- `OnFirstShow` overrides may have side effects (focus, selection, enable state). Splitting
  them must not change what a visit-ed page shows; verify each page manually.
- Any cached search text (C) will go stale if a page fills a combo from another control
  (e.g. language lists, priority/risk level combos). Invalidate those explicitly.
- Search results must remain identical: multi-word `MatchAll` terms, path-only matches
  (`AddPageToTree`'s `nPath` check, `PreferencesDlg.cpp:411-421`), and the "no match → show
  all pages" fallback (`:1042-1043`).
- The tree is rebuilt and `Resize()` runs on each search; if A/B make search fast, consider
  whether the double `AddPagesToTree` call can be reduced, but do not change tree behaviour.

## Measurement plan

- Reproduce with a stopwatch: open Preferences, immediately search a term that lives on a
  page you have not visited (e.g. "thumbnail", "command tree", "risk level"), time until the
  result/highlight appears. Repeat after reopening the dialog.
- Use the existing `CScopedLogTimer` logging pattern (`-g`) if the dialog path can host one;
  otherwise temporary timers around the three stages above.
- Record before/after in this document when implemented.

## Test notes (to fill in when implemented)

- Build `Core/ToDoList_Core.sln` (`Unicode Debug`, `Win32`).
- Verify open-to-visible time and first-search time both drop and the dialog remains
  responsive while pages pre-create.
- Verify every page still translates after a language change, and that each page's controls
  work when visited for the first time (especially shortcuts tree and visibility list).
- Verify search results and highlights unchanged for label terms, combo-item terms,
  multi-word terms, path-only terms, and no-match fallback; verify clear (`BTN_CLEAR`).

## Related

- [PERF-003](PERF-003-ui-perf-harness.md) — UI performance harness usable for a baseline.
- [PERF-006](PERF-006-tasklist-open-activation-cost.md) — same class of fixed UI-creation
  cost, deferred window creation precedent.
- [UI-010](../ui/UI-010-preferences-tooltips.md) — added per-page `InitTooltips` on the same
  creation path.
- [UI-011](../ui/UI-011-preferences-apply-button.md) — also touches page creation and the
  dialog's control-change path; keep the two changes compatible.
- Back to [customized/index.md](../index.md).
