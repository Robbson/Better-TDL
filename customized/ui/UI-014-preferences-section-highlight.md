# UI-014: Preferences index section highlighting

Status: **In progress** (implemented and built; interactive check pending)

## Goal

Make the Preferences dialog index useful for pages that contain several named
groups. Selecting `User Interface > General > Filtering` (or `Sorting`, `Main
Toolbar`, etc.) should scroll the page to that group and visibly mark the whole
group, not just the heading.

## Upstream starting point

- Each index leaf can carry a section control ID when it is added to the tree
  (`Core/ToDoList/PreferencesDlg.cpp:339-390`). The node-to-control map is
  populated in `AddPageToTree` and consumed in `OnTreeSelChanged`.
- `OnTreeSelChanged` already tries to scroll to the section control
  (`Core/ToDoList/PreferencesDlg.cpp:487-517`), but there is no highlighting.
- Group headings are plain `LTEXT` labels plus a drawn line (`AddGroupLine`),
  not `GROUPBOX` controls, so there are no built-in group bounds to paint.
- `CScrollingPropertyPageHost::ScrollTo` read its scroll position with
  `SCROLLINFO::cbSize` left at zero, so `GetScrollInfo` failed and the position
  was clamped to zero (`Core/Shared/ScrollingPropertyPageHost.cpp:286-310`).
  This made every section anchor a no-op.

## What changed

Files: `Core/Shared/ScrollingPropertyPageHost.cpp`,
`Core/Shared/CtrlTextHighlighter.h/.cpp`,
`Core/Shared/PreferencesBase.h/.cpp`,
`Core/ToDoList/PreferencesDlg.cpp`.

1. Fixed the section scroll:
   - `ScrollTo` and `OnVScroll` now initialise `SCROLLINFO::cbSize` before
     calling `GetScrollInfo`, so the host can actually move the page to the
     section control.
2. Added a reusable band highlight to `CPreferencesPageBase`:
   - Pages record every `AddGroupLine` control ID in `m_aGroupLineIDs`.
   - `HighlightSection(UINT nIDSection, COLORREF crHighlight)` locates the
     selected heading, finds the next registered heading by vertical position
     (or the page bottom for the last group), builds a band rect, collects the
     page's direct children that intersect it, and asks the highlighter to
     paint the band behind those controls.
   - A non-positive section ID means the ungrouped content at the top of the
     page, so page-level items such as `User Interface > General` highlight the
     controls above the first group heading. If there is no such content the
     band is cleared: highlighting just the first group would be misleading
     when the page's other groups have no index entries of their own.
   - `CCtrlTextHighlighter` gained `HighlightCtrls()`: search highlighting
     paints per control, while the new mode fills one band rect in
     `WM_ERASEBKGND` and returns the band brush for static controls inside it.
     The two state sets are independent, so a search result (yellow) can sit on
     top of a section band (blue).
3. Corrected two index items that were mapped to the top of their page instead
   of to their group heading:
   - `Multiple Users > File Checking` now targets `IDC_CHECKGROUP`.
   - `Tasks > Defaults > Inheritable Attributes` now targets
     `IDC_INHERITGROUP`.
4. `OnTreeSelChanged` now distinguishes three kinds of index item:
   - **Group leaf** (`nIDSection > 0`): highlight its band, including while a
     search is active.
   - **Page-level leaf** (`IDC_TOPOFPAGE`, which is negative): highlight the
     ungrouped content above the first heading only when the page node also has
     group children in the index, and only when no search is active. Standalone
     pages with groups but no group entries (for example `Tasklist Attributes`
     and `Fonts and Colours`) get no highlight because the other groups cannot
     be reached from the index.
   - **Folder** (no section, `nIDSection == 0`): clear any band so that folders
     are visibly distinct from their first child.
   The section band uses a pale blue tint (`RGB(224, 239, 252)`) in light mode
   and a muted dark blue (`RGB(44, 64, 92)`) in dark mode, so it reads as a
   location hint rather than a search match.

## Behaviour

- Clicking a group index item scrolls to that group and paints a pale blue band
  behind the heading and all controls up to the next heading.
- Clicking another group moves both the scroll position and the band.
- Clicking a folder selects its first child page but clears the band, so a
  folder never looks highlighted.
- Clicking a page-level item such as `User Interface > General` scrolls to the
  top and highlights the ungrouped content above the first group heading. This
  only happens when that page has group children in the index; standalone pages
  with groups but no group entries are left unhighlighted.
- This also covers the first item of a group folder: `Multiple Users > File
  Checking` and `Tasks > Defaults > Inheritable Attributes` now target their
  actual group headings.
- During a search, the filtered index still works: selecting a group scrolls to
  that group and draws the blue band while the yellow search matches remain
  visible, including when a match sits inside the selected group. Page-level
  items scroll to the first search match.

## Risks

- Group extents are derived from control vertical order, which is reliable for
  the current top-down preference page layouts but would need revisiting if a
  page places unrelated controls side by side at different heights.
- Owner-drawn or composite controls paint over the band with their own
  background; the band remains visible around them.
- The band is cleared when another page/group is selected, but it is not timed
  out. This keeps the implementation timer-free.

## Test notes

- Built `Core/ToDoListLib/ToDoListLib.vcxproj` and
  `Core/ToDoList/ToDoList.vcxproj` (`Unicode Debug`, `Win32`) successfully.
- `Core/TDLTest/Unicode_Debug/TDLTest.exe` passed all 310,419 tests.
- Interactive check of the band and search-over-band rendering pending.
