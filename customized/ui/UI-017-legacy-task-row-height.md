# UI-017 — Legacy task row height

## Goal

Let the user choose between the current task row height and the slightly more compact
height of ToDoList 6.8.10, without giving up per-monitor DPI awareness or the task font
zoom added by the current UI.

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

- Preference: **User Interface > General > Use legacy task row height (v6.8 style)**.
- Default: off (current height).
- Persisted in the existing `[Preferences]` section of `ToDoList.ini` as
  `LegacyRowHeight=0|1`.
- Switching the option and pressing OK updates open tasklists immediately; no restart.
- Font zoom and DPI changes recalculate both modes, because the height is re-derived
  from the current task font on every `WM_SETFONT`/image-list change.
- The legacy mode also keeps the old rule that a row must be at least one pixel taller
  than the list item icons, so small fonts do not clip icons.
- Both the title tree and the attribute columns use the same row height in each mode,
  so selection bands and gridlines stay aligned across the splitter.
- The flat **List View** tab is not affected: it is a report list with no tree metric to
  follow, so its rows remain at the current height in both modes.

## Preferences layout

The UI page now has a visible **General** group at the top. The new option and the
existing legacy attribute-editor option (UI-001) are placed there together. The rest of
the page was shifted down by 45 dialog units and the template height was increased
accordingly; the preferences host scrolls the page as before.

The tree/display label for the page was already `User Interface>General`, so the new
group also makes the visible layout match the preferences index.

## Files

- `Core/Shared/TreeListSyncer.h` / `.cpp` — legacy height mode and the virtual
  `InitItemHeights` hook.
- `Core/ToDoList/TDLTaskCtrlBase.h` / `.cpp` — owner-drawn attribute columns,
  `WM_MEASUREITEM` height control, native-height probe and re-measure nudge.
- `Core/ToDoList/ToDoCtrl.h` / `.cpp` — task-tree setter.
- `Core/ToDoList/TabbedToDoCtrl.h` / `.cpp` — apply to the List View control too.
- `Core/ToDoList/PreferencesUIPage.h` / `.cpp` — checkbox, persistence, tooltip.
- `Core/ToDoList/PreferencesDlg.h` — forwarding getter.
- `Core/ToDoList/TDCToDoCtrlPreferenceHelper.cpp` — apply the preference to tasklists.
- `Core/ToDoList/ToDoList.rc`, `Core/ToDoList/resource.h` — General group, both options,
  tooltip.

## Verification

- `msbuild Core\ToDoList_Core.sln /t:Build /p:Configuration="Unicode Debug"
  /p:Platform=Win32 /m` succeeded. TDLTest was run by the solution build:
  310436/310436 tests passed.
- Launching the Debug build with a temporary INI and the `London Underground` example:
  - `LegacyRowHeight=0` → task tree item height 20 px and attribute columns 20 px.
  - `LegacyRowHeight=1` → task tree item height 19 px and attribute columns 19 px,
    matching the live 6.8.10 instance.
- Clicking the new checkbox in **User Interface > General** and pressing OK changed the
  open tasklist from 20/20 to 19/19 px and persisted `LegacyRowHeight=1`; clicking it off
  restored 20/20 px and `LegacyRowHeight=0`.
- A screenshot check in legacy mode confirmed that the title text, attribute text, icons,
  gridlines and selection band still render correctly and stay aligned across the
  splitter.

## Related

- UI-001 (legacy attribute editor) — option moved next to this one.
- UI-015 (preferences grouping) — the General section here is a first, minimal step; the
  wider reorganisation remains proposed.
