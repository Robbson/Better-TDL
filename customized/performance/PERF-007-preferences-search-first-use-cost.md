# PERF-007: Preferences search is slow on first use

Status: **In progress** (D implemented; E core implemented and unit-tested; dynamic
coverage and the full parity corpus still to be extended)

## Goal

Remove the multi-second stall the user sees the first time the Preferences search box is
used in a dialog session. Subsequent searches in the same session are already fast; the cost
is concentrated in the first search and is repeated each time the dialog is reopened.

The analysis below is unchanged from the original investigation. Implementation progress is
recorded under Option E.

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

**Likely dominant cost (from observed page ranking, not yet instrumented):** the font-name
combo boxes, not generic page setup. `CFontNameComboBox::FillCombo`
(`Core/3rdParty/FontComboBox.cpp:122-139`) enumerates every installed font via `EnumFonts`.
It runs from `PreSubclassWindow` (`Core/3rdParty/FontComboBox.h:34`), which MFC calls from
`DDX_Control` during `UpdateData(FALSE)` in page creation — so each font combo pays a full
system font enumeration when its page is first created. The callback (`EnumFontProc`,
`:36-53`) creates a `CClientDC` and calls `GetTextExtent` **per enumerated font**, and calls
`FindStringExact` (a linear scan of the combo) per font, making it roughly O(n²) plus a
DC/measure per item. Pages with font combos:

- **Fonts & Colors:** two combos, `m_cbTreeFonts` and `m_cbCommentsFonts`
  (`Core/ToDoList/PreferencesUITasklistColorsPage.h:108`) → **two enumerations**.
- **Import & Export:** one combo, `m_cbFonts` (`Core/ToDoList/PreferencesExportPage.h:45`)
  → **one enumeration**.

That matches the reported ranking exactly: Fonts & Colors is the slowest and Import & Export
is roughly half as slow, while other pages only lag a little. A cached enumeration shared by
all combos (Option D) may therefore remove most of the first-search stall on its own.

Secondary factors that ride on the same first-use path:

- The UI-010 tooltip work calls `InitTooltips()` from each page's `OnInitDialog`, so tooltip
  string lookup is part of page creation too (small relative to the above).
- `AddPagesToTree(TRUE)` rebuilds the tree and then `Resize()` runs, twice on the first
  search (the fallback at `PreferencesDlg.cpp:1042-1043` only if no page matched).

## Where the time goes (expected profile)

Not yet measured. The user's page-by-page observation points at font enumeration as the
largest single item; the plan is to confirm the split with the existing logging/timer
infrastructure (see [PERF-006](PERF-006-tasklist-open-activation-cost.md) §Method) or the
[PERF-003](PERF-003-ui-perf-harness.md) harness before changing code. The expected split:

| Component | Expected share | Evidence |
|---|---|---|
| Font enumeration in `CFontNameComboBox::FillCombo` (2× Fonts & Colors, 1× Import & Export) | **large, likely dominant** | `FontComboBox.cpp:36-53`, `:122-139`, `.h:34`; `PreferencesUITasklistColorsPage.h:108`; `PreferencesExportPage.h:45` |
| Creating the other page windows (`Create` + `OnInitDialog` + DDX + tooltips) | medium | `PreferencesDlg.cpp:1019-1036`, `PropertyPageHost.cpp:217-258` |
| Showing/hiding each page → `OnFirstShow` (shortcuts command tree, visibility group lines) | medium | `PreferencesBase.cpp:331-345`, `PreferencesShortcutsPage.cpp:84-100` |
| Highlight walk over all controls + combo/list items | small-medium | `CtrlTextHighlighter.cpp:150-179`, `:265-285` |
| Tree rebuild twice + `Resize()` | small | `PreferencesDlg.cpp:1039-1045` |

If instrumentation confirms fonts dominate (as the page ranking suggests), Option D is the
primary fix and A/B may become optional; if the highlight walk or `OnFirstShow` still
dominates afterwards, the later options handle the remainder.

**Revised after D was implemented:** caching fonts made page visits much faster but did not
improve the first search, so the font row is large for *individual page activation* but not
the dominant share of the all-pages search. The remaining search cost is in the
create/show/highlight rows; the new `-g` timers will apportion it.

## Proposed fix

### Option A — pre-create pages after the dialog is open (cheapest, recommended first step)

Move the "all pages exist" cost off the user's first search and off the first frame.

**Important:** a single posted message that creates all remaining pages in one loop does
*not* keep the dialog responsive. Posting only delays the stall until after the first paint;
the handler then owns the UI thread for the whole loop, so input and paint messages are not
pumped until it completes. `CHoldRedraw` suppresses flicker but does not yield. The
creation must therefore be **chunked** across message-pump iterations:

1. In `CPreferencesDlg::OnInitDialog` (`PreferencesDlg.cpp:226-246`) post a private message
   after the dialog is shown to start pre-creation, and set an "all pages not yet created"
   flag.
2. Create **one page per iteration**: the private handler creates a single not-yet-created
   page (inside `CHoldRedraw` on the host), then either posts the message again or arms a
   `WM_TIMER` and returns, until no uncreated pages remain. Between chunks the message pump
   runs, so input, painting and reopening/closing the dialog stay responsive. Order pages
   cheapest-first if measurement shows that matters; the shortcuts page is the expensive one.
3. Do not use the explicit show/hide pair. Determine what translation actually needs:
   - If `CPropertyPage::Create` already translates the page (it goes through the normal
     dialog creation path and `CLocalizer`), nothing more is needed.
   - If translation genuinely requires a show, call the localiser directly on the page
     (the pattern used elsewhere with `CLocalizer`) instead of showing it.
4. Make `OnUpdateSearch` keep the "create all" block as a fallback only: if a search arrives
   while pre-creation is incomplete, finish the remaining pages synchronously (the current
   block) and cancel the pending chunk message/timer, so results are always correct.
5. Stop the timer / cancel the posted message in `OnDestroy` and when pre-creation finishes.

Result: the first search normally finds all pages already created and only runs the
highlight walk, and the dialog remains interactive because each chunk is one page's
`Create`. Cost is spread across the message pump rather than paid in one freeze. Creating
hidden pages is materially cheaper than creating **and showing** them because `OnFirstShow`
work is deferred until a page is first shown (see B).

One residual: if the user types a search before chunking completes, the fallback in step 4
still does the remainder synchronously, so the first-fast-search guarantee depends on the
chunks finishing quickly. Keep each chunk to a single page and prefer B so the expensive
per-page content build is not part of it.

### Option B — defer `OnFirstShow` work until a page is actually visited

#### When `OnFirstShow` runs

`OnFirstShow` has no message of its own; it is a one-shot side effect of the first
`WM_SHOWWINDOW` the page window receives. `CPreferencesPageBase::OnShowWindow`
(`Core/Shared/PreferencesBase.cpp:331-340`) does not look at `bShow`; it consumes the
`m_bFirstShow` flag (initialised TRUE at `:130`, reset TRUE only in `OnDestroy` `:172-177`)
and calls `OnFirstShow()` once per page window lifetime.

What actually sends the first `WM_SHOWWINDOW`:

- **Page templates are created hidden.** They are `WS_CHILD` with no `WS_VISIBLE`
  (e.g. `IDD_PREFSHORTCUTS_PAGE`, `Core/ToDoList/ToDoList.rc:247-249`), and
  `EnsurePageCreated` calls `CPropertyPage::Create` without showing
  (`PropertyPageHost.cpp:227-255`). So `OnInitDialog` runs at create time, but
  `OnFirstShow` does **not**: a hidden window gets no `WM_SHOWWINDOW`.
- **Normal visit:** `SetActivePage` calls `ShowWindow(SW_SHOW)` (`PropertyPageHost.cpp:345`)
  → `OnShowWindow(TRUE)` → `OnFirstShow()`.
- **Search path:** the all-pages block calls
  `ShowWindow(SW_SHOWNOACTIVATE)` then `ShowWindow(SW_HIDE)` (`PreferencesDlg.cpp:1053-1054`)
  → the show is the first `WM_SHOWWINDOW` → `OnFirstShow()`. The subsequent hide does not
  call it again because the flag is already FALSE (and the base does not test `bShow`, so a
  first hide of a visible page would also consume it — not the normal path here).
- **Never shown, never run:** a page that is created but never shown keeps
  `m_bFirstShow == TRUE` indefinitely.
- **Once per window, not per dialog:** closing and reopening Preferences destroys the pages
  (`OnDestroy` resets the flag), so the cost returns in each new dialog session.

Consequence for Option B: skipping the show in A means `OnFirstShow` (and the shortcuts
`BuildMenuTree`, `PreferencesShortcutsPage.cpp:84-100`) never runs during pre-creation,
which is the intended saving. But the search-highlight path currently depends on some of
that work: `CPreferencesShortcutsPage::HighlightUIText` rebuilds the command tree itself
after the first show (`:341-348`), so deferring `OnFirstShow` must keep the tree populated
for highlighting, or move that build into the highlight/pre-search path instead.

`CPreferencesUIVisibilityPage::OnFirstShow` (`PreferencesUIVisibilityPage.cpp:68-74`) only
adds group lines and is cheap, so it is not a strong reason for B.

If sparse pages keep expensive `OnFirstShow`, make that work demand-driven per page:

- Optionally split `OnFirstShow` into "cheap layout" (always) and "content build"
  (only when the page is the active one), and have the search never trigger content build.
- The shortcuts page is the main beneficiary; the visibility page is minor.

This is a small refactor of the base hook plus two overrides, and it removes the most
expensive part of "showing all pages".

### Option C — cache per-page search text

If profiling still shows the highlight walk dominates:

- Build a per-page index once (page path + control texts + matching HWNDs), reuse it for
  every search in the dialog session.
- Invalidate on control change, or rebuild at dialog open and accept staleness for the
  session (most matching text is static labels and combo items).
- Keep the `HighlightUIText` virtual in sync: `CPreferencesShortcutsPage::HighlightUIText`
  (`PreferencesShortcutsPage.cpp:341-...`) adds tree highlighting and cannot be replaced by
  a plain control-text index.

### Option D — enumerate system fonts once and cache the result (best first step)

This targets the observed dominant cost and is independent of A/B/C.

Today every `CFontNameComboBox` instance enumerates the whole font list itself at
`PreSubclassWindow` time, and the callback re-measures text with a fresh `CClientDC` and
linearly checks for duplicates for every enumerated font (`FontComboBox.cpp:36-53`,
`:122-139`). Three combos per dialog session therefore mean three full enumerations
(two on Fonts & Colors, one on Import & Export).

Plan:

1. Add a process-wide cache in `Core/3rdParty/FontComboBox.cpp`: the deduplicated font-name
   list plus the precomputed maximum text width. Build it once, lazily, on the first
   `FillCombo`.
2. Build the cache efficiently: use `EnumFontFamiliesEx` with `DEFAULT_CHARSET` (one entry
   per family) instead of `EnumFonts`, create **one** DC for the whole pass, and compute the
   width once per unique name. If behaviour must match `EnumFonts` exactly, keep `EnumFonts`
   but dedupe with a set/hash instead of `FindStringExact`.
3. `FillCombo` then only copies the cached names into the combo
   (`InitStorage` + `AddString`/`SetRedraw(FALSE)`), and sets `m_maxWidth` from the cache.
   No per-font DC, no per-font measure, no O(n²) duplicate check.
4. Invalidate the cache when the installed font set changes. Minimal safe approach:
   clear it in `CPreferencesDlg::OnInitDialog`, so each dialog session re-enumerates at most
   once. More correct: also handle `WM_FONTCHANGE` (the dialog is top-level and receives the
   broadcast) and clear the cache there.

Expected result: Fonts & Colors drops from two enumerations to one (shared with Import &
Export, which becomes free), and the first search no longer pays the per-combo O(n²)
measure loop. If the observed ranking is accurate this alone may make the first search
acceptable without A or B.

The cache is process-wide because `CFontNameComboBox` is also used outside Preferences
(`Core/RTFContentCtrl/RRECToolbar.cpp:149`), so the same saving applies there.

#### Implementation (done)

- `Core/3rdParty/FontComboBox.cpp` — added a file-local `FONTNAMECACHE` holding the
  deduplicated names, a `CMap` for O(1) duplicate rejection, and the precomputed maximum
  width. `EnumFontFamilyProc` collects names only; `BuildFontNameCache` uses a single DC
  (`EnumFontFamiliesEx` + `DEFAULT_CHARSET`) and measures each unique name once.
  `FillCombo` now copies the cached names (`ResetContent` + `SetRedraw`), then applies the
  cached width — no per-font DC, measure or linear duplicate scan.
- `Core/3rdParty/FontComboBox.h` — added `static void InvalidateFontCache()`.
- `Core/ToDoList/PreferencesDlg.cpp/.h` — invalidate the cache in `OnInitDialog` (fresh
  list per dialog session) and on `WM_FONTCHANGE` (`OnFontChange`), so installing/removing
  fonts is picked up.

Effect: Fonts & Colors (two combos) and Import & Export (one combo) now share a single
enumeration per dialog session instead of paying three, and the enumeration itself no
longer does the O(n²) `FindStringExact`/per-font `CClientDC` work.

#### First user result (after D)

- Navigating to **Fonts & Colors** and **Import & Export** is now much faster (measured by
  feel, after reopening Preferences) — so the font enumeration was indeed a large part of
  *page-visit* cost.
- **The first search did not improve measurably.** Font enumeration was therefore not the
  dominant cost of the all-pages search path; the remaining cost is elsewhere in
  create/show/highlight over all pages.

Instrumentation was added to `OnUpdateSearch` (guarded by the existing `-g` logging):
per-page create time, per-page show/hide time, per-page highlight time, plus totals for
`CreateAllPages`, `AddPagesToTree(search)` and `Resize`. Run the Debug exe with `-g`,
open Preferences, search once, close, then read `ToDoList.log` beside the exe. The next
step is to attribute the remaining stall from those numbers.

#### Measured attribution (instrumented `-g` run, Release and Debug)

A term/`"risk level"` search in a fresh dialog, with two pages already created:

| Stage | Debug | Release |
|---|---|---|
| `CPreferencesDlg::OnUpdateSearch` total | ~9.0-9.2 s | ~8.8-9.0 s |
| `CreateAllPages` (15 pages) | ~7.7-7.9 s | ~7.7 s |
| `AddPagesToTree(search)` (highlight walk) | ~1.1-1.4 s | ~1.1-1.2 s |
| `Resize` | 0 ms | 0 ms |

Release is not faster, so this is real control-creation/population cost, not a Debug
artifact. The dominant per-page costs (Release ranges):

| Page | Cost | Main component |
|---|---|---|
| Custom Toolbar (`#274`) | 1.0-1.3 s | `CMenuComboBox::Initialise` 750-830 ms inside `CTDLToolbarButtonListCtrl::InitState` (600+ menu items) |
| Fonts & Colors (`#157`) | 0.9-1.25 s | two `CFontNameComboBox::FillCombo` calls ~235-390 ms each, plus other DDX |
| Task (`#138`) | 0.7-0.86 s | `PageInit::DDX` 530-800 ms |
| Shortcuts (`#142`) | 0.6-0.9 s | first show `BuildMenuTree` 375-735 ms, rebuilt again during highlight |
| TaskDef (`#156`) | 0.45-0.66 s | `PageInit::DDX` 390-530 ms |
| Export (`#158`) | 0.44-0.72 s | `FillCombo` ~390 ms plus DDX |

Key corrections to the earlier hypothesis:

- **Font enumeration is no longer expensive.** `FontCache::Enumerate` is 0-15 ms and the
  whole cache build 16-31 ms. D removed the old per-font `CClientDC`/`GetTextExtent` and
  O(n²) `FindStringExact` cost, which is why *page visits* got faster — but that work was
  never the search's bottleneck once the font combos had been created.
- **Adding the names to the combos is still ~0.6 ms/name** (235-390 ms per font combo).
  Removing `CBS_SORT` and pre-sorting the cached names did **not** change this (measured
  390/312/235 ms versus 391/282/265 ms), so it is not the sort compare; it is the
  per-item `CB_ADDSTRING` cost in this environment.
- The highlight walk is only ~1.1 s total; two pages account for most of it (the Shortcuts
  tree rebuild and one large control page).

**Consequence for the options:** A would move ~7.7 s off the search but only by doing the
same work in ≤1.2 s chunks after the dialog opens, and an immediate search still blocks.
B saves only the show-time portion (~1 s) and risks dynamically-set page text. C saves at
most ~1.1 s. The larger lever is reducing the per-page content-creation cost (menu combo,
shortcut tree, font `AddString`, page DDX), or searching without creating every page.

### Option E — search a pre-built index instead of creating pages (recommended direction)

Yes: a searchable index is the only way to make the *very first* search instant without
pre-creating pages. Build it from text metadata, search it, populate the results tree, and
create only the pages the user actually visits. This also removes the ~1.1 s highlight walk
from the search; each visited page is highlighted when it is created/shown.

Design constraints found in the code:

- **The index cannot be purely build-time if it is to stay correct.** Localization is
  runtime: `CLocalizer` translates text through a dictionary (`Localizer.h:30`,
  `Localizer.cpp`), so a generated English table must still be passed through
  `CLocalizer::TranslateText` at runtime. Preference-dependent text (document paths, home
  folder, handler paths, fonts, languages) only exists after `LoadPreferences`.
- **A pure build-time table also misses runtime-generated control text**: font names,
  time lists, priority levels, attribute names, language names, toolbar menu items,
  shortcut command names. Those are common search targets.
- Dialog/menu resources are already available in the module: `FindResource(...,
  RT_DIALOG)` / `RT_MENU` is used by `CLocalizer::ForceTranslateAllUIElements`
  (`Localizer.cpp:219-282`), so either a generated table or a small runtime template
  reader can enumerate static labels without a window.
- The results tree is page-granular: `AddPageToTree` adds the page plus all of its section
  nodes once the page matches; the search is an OR over the terms, not an AND
  (`CtrlTextHighlighter::TextContainsOneOf`). An index of `page -> set of strings` is
  therefore sufficient; no control-to-node mapping is required.

Recommended shape:

1. Add a `CPreferencesSearchIndex` that holds `page index -> CStringArray` entries and
   answers "which pages match any term".
2. Populate the static part from the page dialog templates (labels, group boxes, buttons,
   check/radio text), translated through `CLocalizer::TranslateText`. Two ways to get the
   static text:
   - **Build-time generator (user's suggestion):** parse `ToDoList.rc` in the build and
     emit a table keyed by `IDD_PREF*_PAGE`. Cheap at runtime and no template parser in the
     app, but adds a build step and must be regenerated when labels change. The runtime
     still translates the entries and appends dynamic ones.
   - **Runtime resource reader:** walk `RT_DIALOG`/`RT_MENU` at first search. No build
     step and never stale, at the cost of ~150 lines of `DLGTEMPLATE` walking (variable-
     length menu/class/title fields).
3. Add dynamic contributions per page through a virtual
   `AddSearchText(CStringArray&) const` on `CPreferencesPageBase`, using the members that
   `LoadPreferences` already filled (paths, home folder, font name), plus generator lists
   (font names from the Option D cache, time list, priority levels/attributes, languages,
   priority schemes, menu/toolbar item text). Start with the pages whose dynamic text
   matters (File, Gen, Colors, Export, UITasklist, Shortcuts, Custom Toolbar) and grow the
   list.
4. In `OnUpdateSearch`, match the index to decide the pages and build the tree without
   calling `EnsurePageCreated`. Keep the existing window-walk search behind a debug switch
   and add a parity check (same page set for a corpus of terms) so missing index entries
   are caught.
5. Highlight lazily: after `SetActivePage` creates/shows a page while a search is active,
   call its `HighlightUIText`. The tree already highlights matching page titles via the
   custom draw. For the Shortcuts page this builds the command tree already filtered (its
   `BuildMenuTree` uses the highlighter's current terms).
6. Rebuild/refresh the index at dialog open (preferences are loaded then) and when the UI
   language changes. The static part is language-dependent only through the translation
   step.

Cost/result: index construction is a few ms for a few thousand strings; matching is a
linear scan well under a millisecond. The first search becomes instant, the tree is
populated from the index, and only the pages the user selects (plus normal browsing) pay
the 0.4-1.3 s page-creation cost — which D already made cheaper.

Risks: false negatives if a dynamic string is not contributed (the parity test is
mandatory), translation-context differences, and keeping a build-time table in sync if
that route is chosen. The runtime-reader variant avoids the sync risk and is the safer
choice for this fork.

#### Implementation (core done)

The runtime-resource-reader variant was chosen:

- **`Core/ToDoList/PreferencesSearchIndex.h/.cpp` (new)** — `page -> CStringArray` index.
  `AddDialogTemplate` walks the page's `RT_DIALOG` template and extracts static item text
  (labels, group boxes, buttons, checks/radios). It handles both standard `DLGTEMPLATE`
  (Unicode strings in this Unicode build) and `DLGTEMPLATEEX`, skips `DS_SETFONT` font
  data and creation data, strips accelerators, and translates each entry through
  `CLocalizer::TranslateText`. `PageMatches` reuses
  `CCtrlTextHighlighter::TextContainsOneOf` so term semantics match the old walk exactly.
- **`Core/Shared/PreferencesBase.h`** — new `virtual void AddSearchText(CStringArray&)
  const`, default no-op, for runtime-generated page text.
- **`Core/ToDoList/PreferencesDlg.*`** — builds the index in `OnInitDialog` (after the
  initial page exists and `LoadPreferences` has run) and rebuilds it if preferences are
  reloaded while the dialog is open. `AddPageToTree` now matches the index and no longer
  calls `HighlightUIText` on uncreated pages. `OnUpdateSearch` no longer creates all pages.
  Lazy highlighting runs after `SetActivePage` succeeds in `OnTreeSelChanged`, so only
  visited pages are highlighted. The Shortcuts page is marked always-match, mirroring its
  existing `HighlightUIText` return of `nNumHighlights + 1`.
- **`Core/3rdParty/FontComboBox.h/.cpp`** — `GetFontNames` exposes the Option D cache so
  the Export and Fonts & Colors pages can contribute every font name without creating a
  combo.
- **Dynamic `AddSearchText` overrides** — Gen (home/stickies/handler paths), File,
  File (more), UI (theme file), Task, Task Calc, Task Def, Task Def (more lists/attributes),
  UITasklist (date/time trail), Tools (tool/command/icon paths), Fonts & Colors (fonts,
  tree/comments font, attribute colours), Export (fonts). Shortcuts is covered by the
  always-match rule.

  The Custom Toolbar page is deliberately **not** indexed beyond its static template text.
  Its menu command names live in a hidden in-place `CMenuComboBox` (created by
  `CTDLToolbarButtonListCtrl::InitState`), so the old window walk could add the page but had
  nothing visible to highlight. Indexing it reproduced a result with no visible match.
  Visible toolbar-button rows are also not indexed yet; doing that properly needs row-level
  highlighting, which the text highlighter does not provide. The Visibility page similarly
  contributes no text because the old walk only read its tree window caption, not its rows.
- **`Core/TDLTest/PreferencesTest.*`** — added
  `CPreferencesSearchIndex::DialogTemplates`, which loads page resources from the built
  `ToDoList.exe` and verifies standard and `DIALOGEX` template parsing, and
  `CPreferencesSearchIndex::Matching`, which covers OR terms, case-insensitivity and the
  always-match page.

Debug safety valve: set `TDL_LEGACY_PREFS_SEARCH=1` in a Debug build to run the old
create-all-pages/window-walk path. This is the seed for the mandatory parity check; the
corpus and automated comparison are still to be added.

Still open:

- Grow the dynamic coverage for remaining generated content (languages, priority/risk
  levels, attribute names in the colours page, menu/toolbar item text) and decide whether
  the list-only pages need row text.
- Add the parity corpus: for a set of terms, compare the index page set with the legacy
  window-walk page set and log any difference.
- Measure the new first-search time with `-g`. The expected result is index build plus
  tree population only; the first selected result still pays one page-creation cost.

## Recommended sequence

1. Implement D first (cache the font enumeration in `CFontNameComboBox`). It is small,
   shared, low-risk and targets the component that the observed page ranking points at.
   Validate the first-search time and the Fonts & Colors / Import & Export pages
   specifically before doing anything else. **(Done — see Option D implementation above.)**
2. If a residual stall remains, instrument (cheap): log timings around the creation loop,
   the per-page show/hide, and the highlight walk in `OnUpdateSearch` to see what is left.
   **(Done. Result: 7.7 s creation, 1.1 s highlight; see the measured attribution above.)**
3. The measurements change the priority. Implement **E** (window-free search index) as the
   primary fix for the first search: match the index, build the tree, and create pages only
   when the user visits a result. Keep the current window-walk search behind a debug switch
   and add the parity check from Option E. This is the only option that makes a first
   search in a fresh process instant without pre-creating or stalling on pages.
   **(Core done — see Option E implementation above. Dynamic coverage and parity corpus
   remain.)**
4. Continue reducing per-page content cost (Custom Toolbar menu combo, Shortcuts tree, font
   combo `AddString`, page DDX) because it remains the cost of the first visited page and of
   normal page browsing. A/B are no longer needed for search; a single-batch pre-creation
   loop is not acceptable.
5. Add C only if highlighting the visited pages is still slow; with E only matching pages
   are highlighted, so this is likely unnecessary.

## Files likely touched

- `Core/3rdParty/FontComboBox.cpp/.h` — cached font list + invalidation (Option D).
- `Core/ToDoList/PreferencesDlg.cpp` — cache invalidation / `WM_FONTCHANGE` (D); index
  matching + tree population without page creation and lazy highlight on page visit (E);
  `OnUpdateSearch`, `OnInitDialog`, `SetActivePage`/`OnTreeSelChanged`.
- `Core/ToDoList/PreferencesSearchIndex.*` (new) — static table or runtime template/menu
  reader plus matching (E).
- `Core/Shared/PreferencesBase.*` — `AddSearchText` virtual (E).
- `Core/ToDoList/Preferences*Page.*` — dynamic `AddSearchText` overrides for the pages whose
  content is generated at runtime (E).
- `Core/ToDoList/PreferencesSearchIndex*` build generator + project pre-build step if the
  build-time static-table route is chosen (E).
- `Core/ToDoList/PreferencesUITasklistColorsPage.*` and `PreferencesExportPage.*` — only if
  the combo needs an explicit "fill from cache" call rather than doing it inside
  `FillCombo`.
- `Core/ToDoList/PreferencesShortcutsPage.cpp` — `OnFirstShow` (`:84-100`) split if B.
- `Core/ToDoList/PreferencesUIVisibilityPage.cpp` — `OnFirstShow` (`:68-74`) split if B.
- `Core/Shared/PreferencesBase.h/.cpp` — base `OnFirstShow` hook (`:331-345`) if B.
- `Core/Shared/PropertyPageHost.cpp/.h` — a light "create hidden" helper if useful.
- `Core/Shared/CtrlTextHighlighter.cpp/.h` — only if C.
- `customized/index.md` — add this PERF-007 row.

## Risks

- Creating all pages may still slow dialog open on very slow machines. This is only
  mitigated if creation is chunked (one page per pumped iteration, see Option A); a single
  synchronous batch freezes the UI thread just as the first search does. Verify that the
  dialog accepts input and repaints between chunks on `Unicode Debug` as well as Release.
- `OnFirstShow` overrides may have side effects (focus, selection, enable state). Splitting
  them must not change what a visit-ed page shows; verify each page manually.
- Any cached search text (C) will go stale if a page fills a combo from another control
  (e.g. language lists, priority/risk level combos). Invalidate those explicitly.
- A cached font list (D) goes stale if fonts are installed or removed while the app runs.
  Invalidate on `WM_FONTCHANGE` and/or at each Preferences open; verify a newly installed
  font appears after reopening Preferences.
- Changing `EnumFonts` to `EnumFontFamiliesEx` (D) must not change the visible font names.
  Keep the existing `EnumFonts` + dedupe path if equivalence is uncertain.
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
- After E, `CPreferencesDlg::BuildSearchIndex` and `AddPagesToTree(search)` are the only
  new timers. Expect tens of milliseconds for index construction and tree population; the
  first selected result then pays one page-creation cost (0.4-1.3 s depending on the page).
- Record before/after in this document when implemented.

## Test notes

- Builds: `Core/ToDoListLib` and `Core/ToDoList` (`Unicode Release`, `Win32`) link
  successfully. The Debug exe link was blocked by a pre-existing running instance, so the
  Release link was used for validation.
- `Core/TDLTest/Unicode_Debug/TDLTest.exe` passes: 310430/310430, including the new
  `CPreferencesSearchIndex::DialogTemplates` (5 checks) and
  `CPreferencesSearchIndex::Matching` (6 checks) tests.
- Manual acceptance still required:
  - Open Preferences and search a term that lives on an unvisited page. Only the selected
    result page should be created; the rest must stay uncreated.
  - Verify search results and highlights for label terms, combo-item terms (font names),
    multi-word terms, path-only terms, the always-match Shortcuts page, and the no-match
    fallback; verify clear (`BTN_CLEAR`).
  - Verify every page still translates after a language change and that each page's controls
    work when first visited (especially the shortcuts tree and visibility list).
  - Compare the index results against `TDL_LEGACY_PREFS_SEARCH=1` for the parity corpus.
  - Verify the Fonts & Colors and Import & Export font lists are still identical to before
    (same names, order, selected font, dropdown width) and that a font installed while the
    app is running appears after invalidation.

## Related

- [PERF-003](PERF-003-ui-perf-harness.md) — UI performance harness usable for a baseline.
- [PERF-006](PERF-006-tasklist-open-activation-cost.md) — same class of fixed UI-creation
  cost, deferred window creation precedent.
- [UI-010](../ui/UI-010-preferences-tooltips.md) — added per-page `InitTooltips` on the same
  creation path.
- [UI-011](../ui/UI-011-preferences-apply-button.md) — also touches page creation and the
  dialog's control-change path; keep the two changes compatible.
- Back to [customized/index.md](../index.md).
