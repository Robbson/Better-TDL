# UI-017 — Legacy tasklist display options

## Goal

Let the user choose between the current tasklist presentation and the corresponding
6.8.10 presentation for row height and icon-to-title spacing, without giving up
per-monitor DPI awareness or the task font zoom added by the current UI. The comment
brackets are a separate option on the Tasklist Attributes page.

At 100% DPI with the default `Arial 10` task font the two modes differ by one pixel:
the current task rows are 20 px high, the legacy rows are 19 px (measured on the running
6.8.10 and current builds). At other DPI/font sizes the difference follows the font
metric, so both modes remain DPI- and zoom-aware.

## Why the difference exists

- 6.8.10 used a single tree view for the tasklist. Its row height was the native tree
  height (19 px), with `CToDoCtrl::UpdateItemHeight()` only applying a small minimum
  (`ToDoCtrl.cpp:681`–`721` in the old tree).
- The current tasklist is a tree plus a report-mode list for the attribute columns,
  synchronised by `CTreeListSyncer`. For the same font and 16 px item icons the native
  list-view row is one pixel taller than the native tree-view row. `InitItemHeights()`
  raises the tree to the list height, so the task rows become 20 px.
- Legacy mode makes the tree the height source again: the tree keeps its native
  font-driven height and both panes are set to the same compact height. The attribute
  columns are owner-drawn (`LVS_OWNERDRAWFIXED`) so their item height can be set
  explicitly, which a report list view cannot do through its font or image lists.

### When the new calculation appeared

Yes — the calculation arrived with the 7.x tree/list tasklist. In the 7.1 source
(`E:\Dev\opensource\windows\TDL\_old\ToDoList_7.1`):

- `ToDoList/TDLTaskCtrlBase.cpp:92` already inherits `CTreeListSyncer`, and
  `:186`–`:204` already creates the report-mode `m_lcColumns` list and syncs it with the
  task tree/list.
- `Shared/TreeListSyncer.cpp:616`–`:666` already contains today's logic:
  `nItemHeight = max(nOtherItemHeight, nPrimaryItemHeight)`, then raises the tree to that
  height.
- The 6.8.10 `UpdateItemHeight`/`CalcMinItemImageHeight` functions are gone in 7.1.

A native check on this machine confirms the root cause: a report-mode list view with the
same `Arial -13` font is **20 px** high even with no image list at all, while a tree view
is 18–19 px. So `max(...)` picked 20 px in 7.1 too; the later commit `6c0d9b897`
(2019-12-22) only added the task icon image list to the columns list to stop the height
changing when label icons are hidden. It did not create the difference.

## Behaviour

- Preferences:
  - **User Interface > Legacy > Use legacy task row height (v6.8 style)** — default off.
  - **User Interface > Legacy > Use legacy icon-to-title spacing (v6.8 style)** — default
    off. Adds a fixed 2 px between the task icon and title (easy to tune later).
  - **User Interface > Legacy > Use legacy header font (v6.8 style)** — default off.
    Draws the tasklist column headers with the application's UI font (6.8 used the
    parent control's font), left-aligns the header text, and makes
    header-only/empty columns smaller.
  - **User Interface > Tasklist Attributes > Display comments in brackets (v6.8 style)**
    — default on, below *Display comments after a task's title*.
- Persisted in the existing `[Preferences]` section of `ToDoList.ini` as
  `LegacyRowHeight=0|1`, `LegacyHorizontalPadding=0|1`, `LegacyHeaderFont=0|1` and
  `LegacyCommentBrackets=0|1`.
- Switching any option and pressing OK updates open tasklists immediately; no restart.
- Font zoom and DPI changes recalculate row heights in both modes, because the height is
  re-derived from the current task font on every `WM_SETFONT`/image-list change.
- The legacy row-height mode also keeps the old rule that a row must be at least one
  pixel taller than the list item icons, so small fonts do not clip icons.
- Both the title tree and the attribute columns use the same row height in each mode,
  so selection bands and gridlines stay aligned across the splitter.
- The trailing comment text is always top-aligned to the old label position instead of
  being vertically centred in the (now shorter) row. Whether it is wrapped in `[...]`
  is controlled by the separate Tasklist Attributes option.
- The flat **List View** tab is not affected by the row-height option: it is a report
  list with no tree metric to follow, so its rows remain at the current height in both
  modes. See the List View section below for the technical reason and options.

## Preferences layout

The UI page has a visible **General** group at the top and a visible **Legacy** group
immediately before **Filtering**, containing the four legacy options (attribute editor,
row height, icon-to-title spacing and header font). The Tasklist Attributes page has the
comment brackets option directly below *Display comments after a task's title*.

The preferences index now also has a **User Interface > General > Legacy** entry that
scrolls directly to the Legacy group, and the page-level **General** entry now targets
the General group so that it is highlighted when selected.

## Files

- `Core/Shared/TreeListSyncer.h` / `.cpp` — legacy height mode and the virtual
  `InitItemHeights` hook.
- `Core/ToDoList/TDLTaskCtrlBase.h` / `.cpp` — owner-drawn attribute columns,
  `WM_MEASUREITEM` height control, native-height probe, re-measure nudge, legacy
  icon-to-title padding, trailing-comment brackets and top alignment.
- `Core/ToDoList/ToDoCtrl.h` / `.cpp` — task-tree setters.
- `Core/ToDoList/TabbedToDoCtrl.h` / `.cpp` — apply to the List View control too.
- `Core/ToDoList/PreferencesUIPage.h` / `.cpp` — Legacy group options, persistence and
  tooltips.
- `Core/ToDoList/PreferencesUITasklistPage.h` / `.cpp` — comment-brackets option,
  persistence, tooltip and enable state.
- `Core/ToDoList/PreferencesDlg.h` — forwarding getters.
- `Core/ToDoList/TDCToDoCtrlPreferenceHelper.cpp` — apply the preferences to tasklists.
- `Core/ToDoList/ToDoList.rc`, `Core/ToDoList/resource.h` — General/Legacy groups, the
  four legacy options, the comment-brackets option and tooltips.

## Verification

- `msbuild Core\ToDoList_Core.sln /t:Build /p:Configuration="Unicode Debug"
  /p:Platform=Win32 /m` succeeded. TDLTest was run by the solution build:
  310436/310436 tests passed.
- Launching the Debug build with a temporary INI and the `London Underground` example:
  - `LegacyRowHeight=0` → task tree item height 20 px and attribute columns 20 px.
  - `LegacyRowHeight=1` → task tree item height 19 px and attribute columns 19 px,
    matching the live 6.8.10 instance.
- Clicking the new checkbox in **User Interface > Legacy** and pressing OK changed the
  open tasklist from 20/20 to 19/19 px and persisted `LegacyRowHeight=1`; clicking it off
  restored 20/20 px and `LegacyRowHeight=0`.
- A screenshot check in legacy mode confirmed that the title text, attribute text, icons,
  gridlines and selection band still render correctly and stay aligned across the
  splitter.
- With `LegacyHorizontalPadding=1` the title text shifted 2 px further from the task
  icon; with `LegacyCommentBrackets=1` the trailing comment rendered as `[comment]` and
  with it off as `comment`.
- With `LegacyHeaderFont=1` the tasklist headers changed from `Arial -13` to
  `Tahoma -11` (the UI font) and an empty column whose width was header-driven shrank
  from 36 px to 33 px.
- The UI page layout was checked live: **General**, **Legacy** (four options) and
  **Filtering** appear in that vertical order, with the same inter-group spacing as the
  other sections. The Tasklist Attributes page shows the brackets option directly below
  *Display comments after a task's title* and disables it with the parent option.
- The final header-alignment and General-highlight changes were compiled only (both
  configurations); runtime verification is left to the user.

## List View row height — limitation and recommendation

The legacy row-height option currently applies to the **tree view only**. The List View
title pane is a native report `CListCtrl` created by
`CTDLTaskListCtrl::CreateTasksWnd()` (`TDLTaskListCtrl.cpp:126`–`134`) and relies on
Windows to draw:

- the 16 px task icon from `LVSIL_SMALL` (`SetTasksImageList()`, `TDLTaskListCtrl.cpp:145`–`155`)
- the checkbox from `LVSIL_STATE` plus the `LVIS_STATEIMAGEMASK` callback mask

A report list has no `TreeView_SetItemHeight()` equivalent. Its row height is the native
report-list metric (20 px at `Arial -13`) and the image/state lists can only *increase*
it. The attribute columns are owner-drawn, so they could be forced to 19 px, but the
title pane would remain at 20 px and the rows would misalign across the splitter. That
is why the whole List View was left unchanged rather than changing only half of it.

The header font and left-alignment options do apply to both views because they only
reconfigure the header controls.

A native experiment with an owner-draw-fixed report list confirmed that the task icon
and checkbox are **not** drawn once `LVS_OWNERDRAWFIXED` is used; the application would
have to paint them itself.

### Options if exact List View parity is needed

1. **Owner-drawn title list (recommended if parity is required)**
   - Create `m_lcTasks` with `LVS_OWNERDRAWFIXED` and return the target height from
     `WM_MEASUREITEM` (19 legacy / 20 normal), as the attribute columns now do.
   - Owner-draw drops the native icon/checkbox, so draw them in custom draw using
     `m_ilTaskIcons.Draw()`, `m_ilCheckboxes.Draw()`/`DrawColumnCheckBox()` and the
     existing `OnPostPaintTaskTitle()` text/comment logic.
   - Must validate: checkbox hit-testing and state notifications, icon placement,
     selection band, comments, font zoom/DPI, and columns-left/columns-right layouts.
   - Risk: medium-high; the normal List View drawing path changes unless the
     owner-draw mode is creation-time and can be kept legacy-only.
2. **One-pixel smaller title-list font (not recommended)**
   - May lower the native row to 19 px but also makes List View task titles smaller
     than tree titles, and the result is font/DPI dependent.
3. **Smaller task-icon image list (not recommended)**
   - Would clip or scale the 16 px task icons.
4. **Status quo**
   - List View remains 20 px in both modes; the tree remains 19/20 px.

Recommendation: defer the List View change. If it is implemented, make it a separate
customization item (for example UI-018) and validate it independently. 6.8.10 had no
List View, so this is a new-view consistency issue rather than a regression against the
old application.

## Related

- UI-001 (legacy attribute editor) — option now lives in the Legacy group.
- UI-015 (preferences grouping) — the General and Legacy sections here are a first,
  minimal step; the wider reorganisation remains proposed.
