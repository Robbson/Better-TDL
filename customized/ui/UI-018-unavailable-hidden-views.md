# UI-018: Ignore and preserve hidden task views whose extension is not loaded

Status: **Done** (implemented and built; TDLTest passes; interactive preferences check pending)

## Goal

Opening **Tools > Preferences > UI > General** in a Debug build asserted once for every
hidden task view whose UI-extension DLL was not loaded in that run. In this fork the
INI is shared between the core build (`Core/ToDoList/Unicode_Debug`) and the plugin
build (`Plugins/debug`), so the core build regularly sees hidden-view type IDs that
belong to managed UI extensions it does not load. The dialog must show the views it
knows about, keep the unknown entries, and not assert.

## Upstream starting point

- `CPreferencesUIPage::LoadPreferences` reads `[Preferences\ViewVisibility]` and passes
  the saved type IDs to `CTDLTaskViewListBox::SetHiddenViews`
  (`Core/ToDoList/PreferencesUIPage.cpp:276-292`).
- `CTDLTaskViewListBox::BuildList` lists only the built-in list view plus the currently
  loaded UI extensions (`Core/ToDoList/TDLTaskViewListBox.cpp:82-127`).
- `SetHiddenViews` asserted `ASSERT(nFind != -1)` for any saved type ID that was neither
  `LISTVIEW_TYPE` nor a loaded extension (`TDLTaskViewListBox.cpp:172`).
- `GetHiddenViews` only returned the check states of visible list items, so accepting the
  preferences silently discarded the unknown entries (`TDLTaskViewListBox.cpp:189-242`).

## What changed

Files: `Core/ToDoList/TDLTaskViewListBox.h`, `Core/ToDoList/TDLTaskViewListBox.cpp`.

- Added `m_aUnavailableViews` to remember hidden type IDs that the current process cannot
  display.
- `SetHiddenViews` now skips an unknown type ID with a `TRACE` instead of asserting and
  records it in `m_aUnavailableViews` (de-duplicated, case-insensitive).
- `GetHiddenViews` appends `m_aUnavailableViews`, so saving the preferences keeps the
  hidden state for extensions that are installed only in another build.
- Known views still assert if their item data cannot be found, because that indicates a
  real mismatch between the list and the extension manager.

## Behaviour

- No assertion when the saved hidden views include extensions that are not loaded.
- Hidden views for unloaded extensions survive a Preferences OK/Cancel round-trip and
  apply again when the app runs with those extensions present.
- Visible views are unchanged; unknown entries are never reported as visible.

## Risks

- A permanently removed extension leaves a small stale entry in the INI
  (`[Preferences\ViewVisibility]`). It has no visual or runtime effect and can be cleared
  with the preferences import/export or by deleting the section.
- The comparison is case-insensitive, matching `CUIExtensionMgr::FindUIExtension`.

## Test notes

- Build: `Core/ToDoListLib/ToDoListLib.vcxproj` (`Unicode Debug`, `Win32`) succeeded, and
  `Core/TDLTest/TDLTest.vcxproj` rebuilt and ran 310,436/310,436 tests successfully. The
  full solution link of `ToDoList.exe` was blocked because a running instance had the
  executable open; only those instances started by this agent may be stopped.
- Reproduction data used: `Core/ToDoList/Unicode_Debug/ToDoList.ini` had
  `HiddenCount=12`, while only the five native UI extensions
  (`BurndownExt`, `CalendarExt`, `GanttChartExt`, `KanbanBoard`, `WorkloadExt`) were
  loaded; the other seven IDs belong to `Plugins/UIExtension` bridges.
- Manual check: open **Tools > Preferences > UI > General** in a Debug build, confirm no
  assertion appears, and confirm the hidden checkboxes for loaded extensions are still
  correct.
