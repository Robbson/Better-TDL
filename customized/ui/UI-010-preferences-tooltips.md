# UI-010: Preferences dialog setting tooltips

Status: **Done** (implemented and built; interactive check pending)

## Goal

Show a tooltip with an explanatory description for each setting on every page of the
Preferences dialog. The descriptions come from the ToDoList website preference help
(sections 1–14), mapped to the matching control on each page. Settings that the website
documents only as `TODO` initially showed a literal `TODO: description missing` marker;
those 24 gaps have since been replaced with descriptions derived from the relevant control
labels and implementation.

## Upstream starting point

- `CPreferencesPageBase` (`Core/Shared/PreferencesBase.h/.cpp`) hosts every Preferences
  page. It has a `m_mgrPrompts` empty-edit prompt manager but no tooltip support.
- Pages each implement `OnInitDialog()` (except `CPreferencesTaskDefPage`, which relies on
  the base).
- Elsewhere the app shows tooltips via `CToolTipCtrlEx::SetToolInfo` from an
  `OnToolHitTest` override combined with `EnableToolTips()` (for example
  `CTDLFilterDlg::OnToolHitTest`, `Core/ToDoList/TDLFilterDlg.cpp`).

## What changed

Files: `Core/Shared/PreferencesBase.h/.cpp`, all `Core/ToDoList/Preferences*Page.h/.cpp`,
`Core/ToDoList/ToDoList.rc`, `Core/ToDoList/resource.h`.

1. `CPreferencesPageBase` gained a generic tooltip facility:
   - `AddTooltip(UINT nIDCtrl, UINT nIDTip)` registers a translated string (`CEnString`) for
     a control ID and flags the page as having tooltips.
   - `virtual void InitTooltips()` is called from the base `OnInitDialog()` right after the
     controls exist. Each page overrides it to register its control-to-string mappings.
   - `OnToolHitTest()` resolves the control under the mouse (walking up the child chain via
     `ChildWindowFromPointEx(..., CWP_ALL)` so disabled/composite controls still show a tip)
     and returns a multiline `CToolTipCtrlEx` tool info.
   - Tips are word-wrapped to a maximum of 60 characters per line (`WrapTooltipText` using
     `Misc::SplitLines`) and rendered as multiline tooltips, so long descriptions do not
     produce a single extremely wide tooltip.
   - `EnableTooltips()` calls `CWnd::EnableToolTips` only when at least one tooltip was
     registered, so pages with no mappings are unaffected.
2. Added `IDS_PREFTIP_*` strings to `Core/ToDoList/resource.h` (starting at 51718, after the
   existing `IDS_UNDOACTION_*` block) and a `STRINGTABLE` block to `Core/ToDoList/ToDoList.rc`.
   The strings live in resources so they remain localizable through the normal
   translation mechanism.
3. Each page's `InitTooltips()` maps its control IDs to `IDS_PREFTIP_*` IDs following the
   order of the website sections:
   - Section 1 General → `CPreferencesGenPage`
   - Section 2 Multiuser → `CPreferencesMultiUserPage`
   - Section 3 File actions → `CPreferencesFilePage` + `CPreferencesFile2Page`
   - Section 4 UI General → `CPreferencesUIPage`
   - Section 5 Custom toolbar → `CPreferencesUICustomToolbarPage`
   - Section 6 Visibility → `CPreferencesUIVisibilityPage`
   - Section 7 Task attributes → `CPreferencesUITasklistPage`
   - Section 8 Fonts & colours → `CPreferencesUITasklistColorsPage`
   - Section 9 Time tracking → `CPreferencesTaskPage`
   - Section 10 Calculations → `CPreferencesTaskCalcPage`
   - Section 11 Default attributes → `CPreferencesTaskDefPage` + `CPreferencesTaskDef2Page`
   - Section 12 Import/Export → `CPreferencesExportPage`
   - Section 13 User-defined tools → `CPreferencesToolPage`
   - Section 14 Keyboard shortcuts → `CPreferencesShortcutsPage`

## Behaviour

- Hovering a setting on any Preferences page shows its website description as a tooltip.
- Descriptions with an extra sentence (secondary options) are folded into the main tip where
  the doc gives them as sub-bullets (for example RTL input, comment sub-options, calculation
  sub-options).
- Settings that were missing a website description now use written descriptions based on
  the control labels and the code paths they affect; see "Follow-up" below.
- Settings that the website does not document at all get no tooltip and are left unchanged.

## Follow-up: filling the `TODO` descriptions

The following 24 strings were replaced with descriptions (all in
`Core/ToDoList/ToDoList.rc`):

- General: saved storage passwords.
- User Interface: comments position, stacked comments, automatic resort, column-header
  sorting, pasted web links.
- Task attributes: date/time paste user and trailing text, mixed completion checkbox,
  hidden zero percent, percent increment, hidden priority number, priority/risk level
  limit, reminders as dates, hidden pane splitter, auto focus, column width limit, hidden
  list parents, file-link thumbnails.
- Fonts & colours: task-list font for Reminders and Find Tasks, priority colour schemes,
  group-header background colour, no task title colouring.

The unused generic `IDS_PREFTIP_TODO` fallback now reads "No description is available for
this option."

## Risks

- The website text is English; the `STRINGTABLE` entries are the localizable source, so
  translations can be added later without touching code.
- Toolbar buttons that are custom-drawn (custom toolbar page, user-definition tools page)
  are not individual controls and are not covered by the control-ID mechanism.
- Cosmetic discord between the older website wording and newer UI labels is expected.

## Source marking

The file `E:\Dev\opensource\windows\TDL\Preferences.txt` is the source of the descriptions.
Every line that was incorporated into a tooltip (setting headers, used sub-bullets and used
`TODO` markers) is prefixed with `|` so the mapping can be audited. The file's original
UTF-16 LE encoding is preserved.

## Test notes

- Built `Core/ToDoList_Core.sln` (`Unicode Debug`, `Win32`) successfully with no errors.
- Interactive hover check pending.
