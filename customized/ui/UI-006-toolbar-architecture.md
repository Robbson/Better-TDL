# UI-006 — Toolbar architecture: command tables, split bars and main-toolbar customization

| Field | Value |
|---|---|
| Status | In progress (phase 1 implemented: data-driven main toolbar; remaining phases proposed) |
| Upstream baseline | ToDoList 9.3.DP7 |
| Scope | `Core/ToDoList/ToDoListWnd.*`, `Core/ToDoList/TDLToolbar.*`, `Core/ToDoList/TDLCustomToolbar.*`, `Core/ToDoList/TDCToolsHelper.*`, `Core/ToDoList/TDCMenuIconMgr.*`, `Core/ToDoList/TDCImageList.*`, `Core/Shared/EnToolBar.*`, `Core/Shared/MenuIconMgr.*`, `Core/ToDoList/ToDoList.rc` |
| Related | [index.md](../index.md), [TED-001](../text-editor/TED-001-comments-toolbar-in-main-toolbar.md), [TED-005](../text-editor/TED-005-comments-display-toggles.md), [UI-007](UI-007-tristate-view-toolbar-button.md), [UI-008](UI-008-compact-filter-bar.md) |

## Goal

Record why the main toolbar was fixed and image-strip based, why the custom toolbar is
not, and how to migrate to a data-driven toolbar model without bitmap editing. The target
is to:

1. make adding or reordering main-toolbar commands a code-table edit, not an `.rc` plus
   bitmap edit;
2. split commands into category bars (for example task tools above the task list, comment
   tools above the editor);
3. optionally let the user customize the main bar with the same machinery the custom bar
   already uses.

This document is the reference for that migration. Phase 1 is implemented; later phases
remain proposed. Phase 1 preserves the existing appearance, order, tooltips and command
behaviour.

## Phase 1 implementation (2026-11-17)

Files added: `Core/ToDoList/TDLToolbar.h/.cpp`.
Files changed: `Core/ToDoList/ToDoListWnd.h/.cpp`,
`Core/ToDoListLib/ToDoListLib.vcxproj(.filters)`, `Core/ToDoList/ToDoList.rc` (comment and
tooltip string entries).

What it does:

- `TDCTOOLBARITEM { UINT nCmdID; LPCTSTR szImageKey; }` and
  `TDCTOOLBARSPEC { name, items, count }` define the bar.
- `MAIN_TOOLBAR_ITEMS` in `TDLToolbar.cpp` is now the source of truth for the main
  toolbar's order, separators and icons. It mirrors the previous `IDR_APP_TOOLBAR` order;
  the `IDR_APP_TOOLBAR` `TOOLBAR` resource is no longer loaded (a comment in `ToDoList.rc`
  says so).
- `CTDLToolbar : CEnToolBar` adds `GetBackgroundColor()` and
  `SetImageList(HIMAGELIST, cx, cy)`, which takes ownership of a list, sets the image and
  button sizes, installs normal/hot lists and rebuilds the disabled list.
- `CTDLToolbarBuilder::Build` loads `CTDCImageList::LoadDefaultImages(TRUE)` (the same
  shared image source as the custom toolbar and menu icons), copies only the icons used by
  the spec into a compact per-bar list and inserts one button per spec item with
  `iBitmap = index in that list`.
- `CToDoListWnd::InitMainToolbar` now calls the builder instead of
  `LoadToolBar(IDR_APP_TOOLBAR)` + positional `SetImage(IDB_APP_TOOLBAR_STD)`.

Consequences for developers:

- Adding an existing command: add one `TDCTOOLBARITEM` line in the required position. Use
  its image name from `CTDCImageList` (`"200".."226"` for `IDB_APP_TOOLBAR_STD`,
  `"250"..` for `IDB_APP_EXTRA_STD`, task icon names, or a file name under `Icons\`).
  No `.rc` or bitmap edit.
- Reordering: move lines.
- Adding a brand-new icon: add the image to the shared image source as before; only then is
  a bitmap/icon edit needed.
- The strip no longer defines toolbar order or button count; it is only an icon source.
  The numeric keys `"200".."226"` still refer to its icon positions, so extending that
  strip appends new names without shifting anything the toolbar table references.
- Tooltips come from the `STRINGTABLE` entry whose ID equals the command ID
  (`CToolbarHelper::GetResourceTip`, `Core/Shared/ToolbarHelper.cpp:587-605`); the text
  after `\n` is shown and the shortcut is appended. Add an entry for every new toolbar
  command. Without one, the helper declines the tooltip, the frame's
  `OnNeedTooltipText` fallback runs, and `CTDCMainMenu::GetDynamicItemTooltip` asserts for
  non-dynamic commands.
- Debug builds assert if a spec item has no image key or the key does not resolve.

Verification for phase 1:

- `msbuild Core\ToDoList_Core.sln /t:Build /p:Configuration="Unicode Debug"
  /p:Platform=Win32 /m` succeeds; `TDLTest` ran 310,419 tests successfully.
- Debug `ToDoList.exe` launched against a temp copy of an example tasklist with
  `TDL_ASSERT_LOG` set: no assertions, toolbar rendered with the expected buttons and the
  Quick Find and comments-format combos in their existing anchors.
- Interactive visual comparison at 100% DPI only; high-DPI and theme-change paths still
  need a manual pass.

## Current architecture (facts)

### Main toolbar (state before phase 1)

The phase 1 implementation above replaced the resource-loaded main toolbar. The facts below
describe the previous design and remain relevant to the custom toolbar comparison and to
the remaining migration phases.

- `CToDoListWnd` derives from plain `CFrameWnd`, not `CFrameWndEx`
  (`Core/ToDoList/ToDoListWnd.h:71`), so no MFC Feature Pack (`CMFCToolBar`, Ribbon,
  visual manager).
- `CTDLToolbar m_toolbarMain` (`ToDoListWnd.h:144`; was `CEnToolBar` before phase 1);
  `CEnToolBar` derives from `CToolBar` (`Core/Shared/EnToolBar.h:19`).
- Created with the classic common-control styles
  `CreateEx(this, TBSTYLE_FLAT | TBSTYLE_WRAPABLE, WS_CHILD | CBRS_ALIGN_TOP | ...)`
  (`ToDoListWnd.cpp:1646`).
- Buttons and order come from the binary `IDR_APP_TOOLBAR` `TOOLBAR` resource
  (`ToDoList.rc:2299`) via `LoadToolBar` (removed in phase 1).
- Icons come from one strip, `IDB_APP_TOOLBAR_STD` (`ToDoList.rc:2217`).
  `CEnToolBar::SetImage` slices it as `bitmapWidth / nonSeparatorButtonCount` and installs
  the whole strip as one image list (`Core/Shared/EnToolBar.cpp:181-198`). Image index is
  therefore position.
- A second, parallel command-ID array in code must match the strip order, with an
  `ASSERT` on the sizes (`Core/ToDoList/TDCMenuIconMgr.cpp:57-96`,
  `Core/Shared/MenuIconMgr.cpp:153-157`).
- The bar hosts real child controls at command anchors:
  - Quick Find combo after `ID_EDIT_FINDTASKS` (`ToDoListWnd.cpp:1662-1683`);
  - comments content-format combo after `ID_COMMENTS_INSERTDATETIME`
    (`ToDoListWnd.cpp:1694-1709`; introduced by [TED-001](../text-editor/TED-001-comments-toolbar-in-main-toolbar.md)).
- Positions of those combos are recomputed by `ReposToolbars` using `CommandToIndex`
  (`ToDoListWnd.cpp:6855-6887`). The combos are children of the toolbar.
- Layout is manual: `ReposToolbars` resizes the main and custom bars, decides whether they
  share a row, and adds the bevel (`ToDoListWnd.cpp:6846-6925`). There is no
  `EnableDocking`/`DockControlBar` anywhere.
- Visibility is a `BOOL` plus `ShowWindow`/`EnableWindow` (`ToDoListWnd.cpp:8322-8341`) and
  an INI setting (`ToDoListWnd.cpp:2682`, `:2981`).
- On theme change in high-DPI mode, the toolbar and both combos are destroyed and
  recreated, and menu icons are cleared because the image lists are DPI/background scaled
  (`ToDoListWnd.cpp:1074-1097`).
- No main-toolbar customization page exists; only the custom toolbar has one
  (`Core/ToDoList/PreferencesDlg.cpp:134`, `:153`, `:371`).

### Custom toolbar

- `CTDLCustomToolbar : CEnToolBar` (`Core/ToDoList/TDLCustomToolbar.h:53`); created the
  same way as the main bar (`ToDoListWnd.cpp:1730-1732`).
- Built programmatically from `TDCCUSTOMTOOLBARBUTTON { UINT nMenuID; CString sImageID; }`
  (`TDLCustomToolbar.h:24-48`). Each button stores a command ID and an image **name**.
- `InitialiseButtons` loads a `CTDCImageList`, resolves each `sImageID` to an image index
  and inserts a `TBBUTTON` with that per-button `iBitmap`
  (`Core/ToDoList/TDLCustomToolbar.cpp:56-98`). No bitmap width/button-count assumption.
- Order is user-configurable via Move Up/Move Down in the preference page
  (`Core/ToDoList/PreferencesUICustomToolbarPage.cpp:198-206`,
  `Core/ToDoList/TDLToolbarButtonListCtrl.cpp:251-315`) and persisted as
  `CustomToolbar\Buttons` in the INI (`PreferencesUICustomToolbarPage.cpp:136-156`).
- Reordering does **not** require bitmap changes: the icon binding travels with the
  command.
- User tools are appended at runtime: `CTDCToolsHelper::AddToolsToToolbar` extracts the
  tool icon, adds it to the bar's image list and inserts a button with the new index
  (`Core/ToDoList/TDCToolsHelper.cpp:351-410`, image add at `:391-398`). It then syncs the
  menu icon manager.
- `CTDCMenuIconMgr::UpdateCustomToolbar` snapshots the command IDs/icons for menu use
  (`Core/ToDoList/TDCMenuIconMgr.cpp:207-231`), and `CMenuIconMgr::AddImages(const CToolBar&)`
  extracts every toolbar icon into the `cmdID -> HICON` map
  (`Core/Shared/MenuIconMgr.cpp:118-149`).
- The preference picker offers the whole shared default image set. The preferences dialog
  loads `CTDCImageList::LoadDefaultImages(TRUE)` for the page
  (`Core/ToDoList/PreferencesDlg.cpp:585`, `:650-651`); the image cell opens
  `CTDLTaskIconDlg` (`Core/ToDoList/TDLToolbarButtonListCtrl.cpp:156-166`), which enumerates
  every image in that list (`Core/ToDoList/TDLTaskIconDlg.cpp:202`, `:351`). This is why
  every main-toolbar icon also appears in the custom-toolbar image picker: it is one shared
  `CTDCImageList`, not a separate catalogue. It is also why image names are positional
  strings within that list rather than command-scoped keys.

### Image source model

`CTDCImageList::LoadImages` builds one ordered list (`Core/ToDoList/TDCImageList.cpp:101-199`):

| Index region | Content | Name form |
|---|---|---|
| 0 | Folder placeholder, replaced by the shell folder icon | `"0"` |
| 1.. | `IDB_TASKICONS_STD`, or user `Icons\TaskIcons.bmp` | `"1"`, `"2"`, … |
| 200.. | `IDB_APP_TOOLBAR_STD` toolbar icons | `"200"`, `"201"`, … |
| 250.. | `IDB_APP_EXTRA_STD` extra icons | `"250"`, `"251"`, … |
| 300.. or file name | User images from `Icons\*.*` | file name |

`GetImageIndex` looks up by lower-cased name (`TDCImageList.cpp:201-212`). The custom
toolbar's default buttons therefore use numeric image names such as `"28"`, `"82"`,
`"258"` (`PreferencesUICustomToolbarPage.cpp:115-131`).

## Planned changes that shape this migration (recorded 2026-11-17)

Two planned UI changes remove the main toolbar's current anchors:

1. **Quick Find and the title/comments filter are merged into the filter bar.**
   `CTDLFilterBar` is already a dialog-based bar above the task list
   (`Core/ToDoList/TDLFilterBar.h:33`, `ToDoListWnd.cpp:6972-6981`) and already has a
   title filter edit and the `FILTER_TITLE` option
   (`TDCEnum.h:716-721`: title only, title + comments, any text) exposed in preferences
   (`PreferencesUIPage.cpp:157`, `ToDoList.rc:447-448`). Quick Find currently only
   navigates (`SelectNextTask`, `ToDoListWnd.cpp:1763-1782`) rather than filtering, so the
   merge must decide whether the combined field filters or navigates, and whether Quick
   Find history (`QuickFind` INI array, `ToDoListWnd.cpp:1684-1687`) is preserved.
2. **The comments content-format combo moves back above the text editor**, reversing that
   part of [TED-001](../text-editor/TED-001-comments-toolbar-in-main-toolbar.md). The three date/time
   **command buttons** are not part of this plan and remain ordinary toolbar commands
   unless decided otherwise.

Consequences: the main toolbar loses both child controls, so it can become a pure
spec-driven command bar without fixed anchor slots. This turns phase 4 below from a design
choice into a prerequisite (or a parallel change), and the `ReposToolbars` combo positioning
code (`ToDoListWnd.cpp:6855-6887`) disappears with it.

## Problem statement

The coupling described in the first point was resolved by phase 1 for the main toolbar; the
remaining points still apply.

- The main toolbar couples three independent lists: RC button order, strip icon order and
  the code command-ID array. Adding one button means keeping all three aligned.
- Only the strip design requires editing a bitmap; the toolbar control supports arbitrary
  per-button image indices, and `CEnToolBar` already exposes `SetItemImage`
  (`EnToolBar.h:56`).
- Today the fixed bar is also the home of two combo boxes, so it cannot be replaced by a
  user-ordered command list without deciding what happens to the anchors. The planned
  relocation of both controls removes this constraint (see above).
- There is no category structure, so task tools and text-editor tools share one strip and
  one visibility flag.

## Recommended design

### 1. Command table

Introduce a single table that is the source of truth for toolbar membership and order:

```cpp
struct TDCTOOLBARITEM
{
	UINT  nCmdID;        // command ID (persisted, must stay stable)
	LPCTSTR szImageKey;  // name resolved by CTDCImageList, or NULL for a separator
};

struct TDCTOOLBARSPEC
{
	LPCTSTR szName;              // "Task", "Comments", "View", "Tools"
	const TDCTOOLBARITEM* pItems;
	int nCount;
};
```

Rules:

- `nCmdID == 0` means separator (matches the custom toolbar convention).
- Image keys should be symbolic (`"app/find"`) or otherwise tied to the command, not to a
  numeric strip position. Keep numeric keys only for backward compatibility with existing
  user preferences.
- Command IDs are persisted in user INI. Changing an ID needs a migration
  (`CTDLCustomToolbar::RemapMenuItemIDs` already models this,
  `TDLCustomToolbar.cpp:209-237`).

### 2. Toolbar builder (implemented)

`CTDLToolbarBuilder::Build` does what `CTDLCustomToolbar::InitialiseButtons` already does:

1. callers create the bar (same `CreateEx` styles) and set the theme colours;
2. load the shared image source, copy the used icons into a compact per-bar list;
3. attach it with `CTDLToolbar::SetImageList` (sets image/button sizes);
4. for each item, `InsertButton` with the item's index in the per-bar list;
5. tooltips via `CToolbarHelper` as before.

This removes `LoadToolBar(IDR_APP_TOOLBAR)` and the positional `SetImage` from
`InitMainToolbar` (the resource-loaded implementation replaced in phase 1). The existing
strip remains the icon **source** through `CTDCImageList`; it no longer defines order.

### 3. Shared image source

Phase 1 shares the *source* (`CTDCImageList::LoadDefaultImages`) and copies the used icons
into each bar's own list. That keeps existing disabled-icon generation and ownership
semantics unchanged.

A later phase can share one `CTDCImageList` instance between bars:

- avoids duplicating icons across bars and the menu icon map;
- keeps command-to-icon resolution in one place;
- lets the developer add an icon once and use it from menu, toolbar and preferences.

Implementation caveat: `CEnToolBar::OnDestroy` deletes `m_ilNormal` and `m_ilDisabled`
(`Core/Shared/EnToolBar.cpp:862-870`). A shared list needs either a non-owning attachment
mode in `CEnToolBar` or a manager that outlives every bar and detaches before destruction.
Do not attach the same `CImageList` to several bars without handling this.

The disabled list is derived per bar today (`EnToolBar.cpp:282-333`). It can be cached per
theme/DPI alongside the normal list, but it must be rebuilt when the theme background
colour or DPI changes; the frame already recreates bars for that case
(`ToDoListWnd.cpp:1074-1097`).

### 4. Category bars

Define one spec per category and create one `CEnToolBar` per visible category:

- `Task`: new/edit/delete/move/filter/sort/expand commands;
- `Comments`: insert date/time and text-formatting commands;
- `View` and `Tools`: lower-frequency commands, user tools, custom bar.

Two placement choices:

| Option | Use when | Notes |
|---|---|---|
| Frame-owned bars positioned by `ReposToolbars` | Default | Keeps wrapping, bevel, theming and DPI recreation in one place; extend `ReposToolbars` to allocate row space above the task list and above the editor |
| Bars parented to the task view / editor pane | Panes become independent | `CEnToolBar` already routes `ON_UPDATE_COMMAND_UI` for non-frame parents (`EnToolBar.cpp:257-266`) |

Do not introduce MFC docking just for this; manual layout is simpler and avoids
`CFrameWnd` docking plumbing.

### 5. Embedded combo boxes (planned removal)

The two combo boxes are being relocated, so the main bar should not need fixed anchor
slots:

- Quick Find merges into `CTDLFilterBar` together with the existing title/comments filter;
- the comments content-format combo returns to a toolbar above the text editor as before
  TED-001.

Until those changes land, keep the current anchors (`CommandToIndex`) and either mark them
non-removable or defer main-bar customization. Do not build a second anchoring mechanism
for a control that is scheduled to move.

This also simplifies the toolbar split: the comments bar above the editor owns the format
combo and the date/time commands, and the filter bar owns text filtering; the task bar
contains only commands.

### 6. User customization

Reuse the custom toolbar machinery rather than building a second mechanism:

- persist an ordered `cmdID` list per bar;
- extend `CPreferencesUICustomToolbarPage`/`CTDLToolbarButtonListCtrl` to select the bar
  being edited;
- add grouping in the picker (spec/category), a search filter and "Reset to defaults";
- validate at startup that every spec command resolves both an icon and a menu entry, and
  assert once instead of the current three-way implicit sync.

## Alternatives considered

| Option | Assessment |
|---|---|
| Keep `CToolBar` + data-driven specs + shared images | **Recommended.** Smallest change, keeps custom theming/DPI code, removes bitmap coupling |
| Migrate to `CMFCToolBar` (Feature Pack) | Gives docking, floating, drag customization and per-user state for free, but needs `CFrameWndEx`, visual-manager integration, resource-ID work and a large regression surface |
| Keep positional strips and document the three-way sync | No technical change; adding tools keeps requiring bitmap edits (rejected) |
| Ribbon (`CMFC...Ribbon`) | Changes the UI paradigm; out of scope |
| WTL/plain Win32 toolbar with shared `HIMAGELIST` | Essentially the recommended pattern, but would discard the existing `CEnToolBar` theming work |

## Why the main toolbar was never customizable

- Historical layering: the main bar is the original resource-loaded MFC toolbar; the custom
  bar was added later and built programmatically around `CTDCImageList`.
- The main bar owns fixed child controls, so arbitrary reordering/removal would break the
  anchor assumptions in `InitMainToolbar`/`ReposToolbars`.
- Not a performance decision. Costs are one `LoadBitmap`, one image-list fill and an
  O(buttons) disabled-icon pass at creation and on theme/DPI change; no benchmark has been
  run, but this is startup-scale work, not per-frame work.
- The real hazard is image-list ownership (see above), not CPU.

## Is the custom-toolbar approach good?

Yes, with fixes:

- Good: command IDs route through normal `CCmdUI`; icons resolve by name; order is data;
  persistence is a simple ordered string; runtime tool insertion already works.
- Fix: numeric image names are unvalidated indices into magic ranges; replace them with
  symbolic keys or derive the icon from the command ID. Add grouping, reset and startup
  validation. Avoid duplicating icons in the menu icon map by sharing the image source.

How other applications do it (for reference, not a change request):

- **MFC Feature Pack**: `CMFCToolBar` + `CMFCToolBarImages` + `CMFCToolBarManager`, shared
  command images, built-in Customize dialog, registry state, optional docking.
- **Win32/WTL**: `TB_ADDBUTTONS` from a table with one shared `HIMAGELIST`; command ID maps
  to icon index.
- **Qt**: `QAction` owns icon/text/shortcut/state; menus and `QToolBar`s are views;
  `saveState`/`restoreState` persist layout.
- **Ribbon/VS shell**: a command table (`.vsct`) defines command, icon, group and
  placement; all surfaces are generated from it.

Principle: define each command once with its icon; generate menus and toolbars from that
definition; persist user layout separately.

## Migration plan (phased)

| Phase | Work | Files | Risk |
|---|---|---|---|
| 0 | Add validation for the current three-way sync; document the strip/count invariant | `TDCMenuIconMgr.cpp`, tests | None |
| 1 | **Done (phase 1):** `TDCTOOLBARITEM`/`TDCTOOLBARSPEC` + `CTDLToolbarBuilder::Build` build the existing main bar from a spec using per-button indices from a `CTDCImageList`; combos and order preserved | `TDLToolbar.h/.cpp` (new), `ToDoListWnd.*`, project files | Medium: layout, tooltips, disabled states |
| 2 | Replace the parallel command arrays with the spec; derive menu icons from the same source | `TDCMenuIconMgr.cpp`, `MenuIconMgr.*` | Medium: menu icons, source-control toggle icons |
| 3 | Split specs into category bars; add View menu toggles and INI visibility flags; extend `ReposToolbars` | `ToDoListWnd.*`, `ToDoList.rc` | Medium-high: layout, DPI, wrapping |
| 4 | Remove the combo anchors: merge Quick Find into `CTDLFilterBar`; move the comments-format combo back above the editor (partly reverts TED-001) | `ToDoListWnd.*`, `TDLFilterBar.*`, `TDLCommentsCtrl.*`, `ToDoCtrl.*` | Medium: filtering semantics, focus/history, editor layout |
| 5 | Unify the preference page across bars; symbolic image keys; reset and validation | `PreferencesUICustomToolbarPage.*`, `TDLCustomToolbar.*` | Medium: user prefs migration |
| 6 | Optional: per-user drag customization for main bars | same | Deferred unless wanted |

Acceptance for phase 1: the main toolbar looks and behaves exactly as today, but its
button order is defined by a table and adding a command requires no bitmap edit.

Phase 4 is an independent UI change and can be done **before** phases 1–3. Doing it first
is preferable: it removes the combo anchors and the `ReposToolbars` combo positioning, so
the later toolbar work only deals with commands.

## Risks

- **Persisted IDs.** User custom-toolbar configuration and any future per-bar configuration
  store command IDs. Renumbering commands requires a remap; keep IDs stable.
- **Image-list ownership.** Sharing lists across bars conflicts with
  `CEnToolBar::OnDestroy` deleting its lists; change the ownership model deliberately.
- **DPI/theme recreation.** Recreating several bars on theme change multiplies the existing
  teardown/rebuild path; keep it idempotent and verify combo re-parenting.
- **Tooltips and command routing.** Tooltips come from the menu via `CToolbarHelper`; every
  spec command must exist in the menu (including dynamic items such as source-control
  check-in/out and new-task variants).
- **Layout.** `ReposToolbars` encodes the current two-bar rules, end-separator width and
  bevel. Splitting bars requires reworking it, not just adding windows.
- **Main-bar bitmap still exists for now.** Until phase 2, keep `app_toolbar_std.bmp` in
  sync; note TED-001's rule that `app_toolbar.bmp` stays 16-colour because
  `CToolBar::LoadToolBar` uses `AfxLoadSysColorBitmap`.

## Verification

- Build: `msbuild Core\ToDoList_Core.sln /t:Build /p:Configuration="Unicode Debug" /p:Platform=Win32 /m`.
- Run with `TDL_ASSERT_LOG=1` and confirm no assertions on startup, theme switch and
  high-DPI restart.
- Add unit coverage in `TDLTest` for spec validation: every command in every spec resolves
  an image key and a menu entry; no duplicate command IDs; separators do not lead, trail or
  duplicate.
- Interactive checks: main bar appearance, tooltips, enabled/checked states, Quick Find and
  comments-format combo positions (until phase 4), custom toolbar ordering, user tools,
  show/hide.
- After phase 4: title/comments filter behaviour (title only, title + comments, any text),
  Quick Find history/autocomplete, focus and Tab order in the filter bar, and the restored
  format toolbar above the editor.
- When category bars exist: resize narrow/wide, wrap, hide each bar, switch theme, restart
  and confirm persisted visibility.

## Open questions

- Should the main toolbar eventually become user-customizable, or only developer-editable
  through the spec table?
- Are symbolic image keys required, or is command-ID-derived icon lookup sufficient?
- For the filter-bar merge: should the combined field filter the list or keep Quick Find's
  navigate-to-next behaviour, and should the `QuickFind` history be reused as an
  autocomplete list for the filter field?
- Is the custom toolbar merged into the category-bar model, or kept as a separate user bar?
