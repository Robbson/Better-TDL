# UI-015: Preferences page grouping and information architecture

Status: **Proposed** (design only)

## Goal

Improve the Preferences dialog's information architecture without changing
behaviour:

- Group the flat `General` page from core settings to advanced/system
  integration settings.
- Make the `User Interface` page consistent: layout first, then navigation,
  then behaviour, then advanced.
- Move settings that are really defaults into the existing Defaults area:
  `Default Task View Visibility` and `Column/Attribute Visibility`.
- Remove naming collisions and ambiguous page names where they affect
  discoverability.

No new index items are required for the visual groups in Proposals 1 and 2. Those
group headings are for scanning only; the tree continues to expose the same
pages and sections. Proposal 3 deliberately moves two existing items into the
Defaults branch.

## Non-goals

- No preference key, default-value, or persistence changes.
- No behavioural changes to the settings themselves.
- No page splitting in this proposal. If grouping is not enough, splitting the
  UI page can be reconsidered separately.

## Current structure

The index is built in `Core/ToDoList/PreferencesDlg.cpp:348-399`. The relevant
parts today are:

```
General
Multiple Users
  File Checking
  Source Control
File Actions
  Loading / Archiving / Switching / Due Notifications
File Actions (more)
  Backup / Saving
User Interface
  General
    Filtering / Sorting / Main Toolbar / Task Comments
    Tasklist Tab Bar / Default Task View Visibility
  Main Toolbar                 (custom toolbar page)
  Column/Attribute Visibility
  Tasklist Attributes
  Fonts and Colours
Tasks
  Time Tracking / Task Time
  Attribute Calculations
  Defaults
    Task Attributes / Inheritable Attributes / Dynamic Droplists
Importing/Exporting
User Defined Tools
Keyboard Shortcuts
```

Problems visible in this structure:

1. `General` is one flat page of unrelated settings, from Home Folder and
   Display Language to system tray, file associations, MRU history, update
   checks and Stickies reminders (`Core/ToDoList/ToDoList.rc:160-218`).
2. The `User Interface > General` page mixes layout, chrome, filtering,
   sorting and comment behaviour in an order that does not reflect either
   importance or frequency of use (`Core/ToDoList/ToDoList.rc:417-483`).
3. `Default Task View Visibility` is indexed under `User Interface > General`,
   even though it is a default applied to new tasklists
   (`PreferencesDlg.h:254`, `CFilteredToDoCtrl::SetDefaultTaskViews`).
4. `Column/Attribute Visibility` is also explicitly a default
   (`PreferencesDlg.h:265`), but is a top-level `User Interface` page.
5. `Main Toolbar` names two different things: the custom toolbar editor page
   and a group inside `User Interface > General` that controls where the
   New Task/New Subtask toolbar buttons place tasks.
6. `Tasklist Attributes` (UI display formatting) and `Task Attributes`
   (defaults for new tasks) are easy to confuse.
7. `Tasklist Attributes` has a large catch-all `Other` group
   (`Core/ToDoList/ToDoList.rc:522-557`) containing task-title settings, icon
   settings, tooltip settings, column layout, list-view behaviour, priority
   display and reminders.
8. "Hide the splitter bar between the task titles and attribute columns" is a
   general layout setting but currently lives in `Tasklist Attributes`
   (`Core/ToDoList/ToDoList.rc:531-532`).

## Proposal 1: regroup the General page

Keep all existing controls and preference keys. Add visual group headings and
reorder the controls from common/core to advanced/system integration.

Suggested target order:

### 1. General
- Home Folder
- Display Language (+ Edit)
- Enable support for Right-to-Left (RTL) user input

### 2. Startup & Window
- Always show ToDoList when it starts up
- Restore previously open tasklists on startup
- Enable delayed loading of tasklists
- Keep ToDoList on top when visible

### 3. System Integration
- Minimise ToDoList to the System Tray (+ tray behaviour, toggle visibility)
- Minimise ToDoList when Escape is pressed
- Global hotkey (+ hotkey field)
- Allow multiple instances of ToDoList
- Enable `.tdl` as a file extension (+ handler status)
- Enable `tdl://` as a URL protocol (+ handler status)
- Minimise ToDoList after a period of inactivity (+ period)

### 4. Files & Maintenance
- Keep track of most recently used (MRU) tasklists (+ Clear MRU)
- Save passwords between sessions for other repositories
- Automatically check for updates on startup

### 5. Confirmations & Reminders
- Display save confirmation dialog on exit
- Display delete confirmation dialog
- Display reminders using Stickies (+ executable path, full task path)
- Only flash the Reminder window for new or reshown reminders

Rationale:

- `Home Folder`, `Display Language` and RTL input are the settings most users
  will look for first.
- Startup and window behaviour is the next most common reason to open the page.
- System integration settings are advanced but coherent as one block.
- MRU/passwords/update checks are maintenance rather than day-to-day use.
- Confirmations and reminders are a separate, easily scanned block.

Implementation note: this needs new `LTEXT` group headings and
`AddGroupLine` calls in `CPreferencesGenPage::OnFirstShow`, plus a resource
reorder. No index entries need to be added, so the headings will not appear in
the tree.

## Proposal 2: make the User Interface page consistent

Use the same principle: layout, then navigation/chrome, then behaviour, then
advanced.

Suggested target order:

### 1. Task View Layout (new heading)
- Position edit fields
- Position comments
- "Stack" edit fields and comments when they occupy the same position
- "Stack" comments above (or left of) edit fields
- Use the following colour theme (+ theme combo)
- Show default task icons in the icon selection dialog
- Hide the splitter bar between the task titles and attribute columns
  (moved from `Tasklist Attributes`)

### 2. Editing & Navigation (new heading)
- Only show menu commands for visible columns/edit fields
- Move the input focus to the tasklist when Enter is pressed

### 3. Tasklist Tabs (existing `IDC_TABBARGROUP`, moved up)
- Show close buttons on tabs
- Hide tab bar when only one tasklist is open
- Stack tab bar items
- Keep tabs ordered by name

### 4. Task Toolbar Placement (renamed `IDC_TOOLBARGROUP`)
- "New Task" button places task
- "New Subtask" button places subtask

### 5. Filtering (existing group)
- unchanged order and contents

### 6. Sorting (existing group)
- unchanged order and contents

### 7. Task Comments (existing group)
- unchanged order and contents

### 8. Advanced (new heading, bottom)
- Use legacy attribute editor (v8.2 style)
- (The hidden `IDC_RIGHTALIGNLABELS` control stays out of the visible layout
  unless it is deliberately re-exposed.)

> Update: UI-017 moved the legacy attribute-editor option into a new `Legacy`
> group below `General`, together with the legacy task-row-height,
> icon-to-title-spacing and header-font options. If the wider reorganisation is
> implemented, keep those legacy options together there.

Rationale:

- Layout is the most tangible part of the page and belongs first.
- Tabs and toolbar placement are chrome/navigation, not task behaviour.
- Filtering, Sorting and Comments are behaviour and form a natural run of
  groups.
- The legacy editor is genuinely advanced and belongs last.

The `Default Task View Visibility` group is removed from this page's index and
handled in Proposal 3.

## Proposal 2b: split the Tasklist Attributes "Other" group

`Tasklist Attributes` currently has three groups: `Date/Time Formatting`,
`Task Completion` and the large catch-all `Other` (`ToDoList.rc:491-557`).
The `Other` group mixes several different concerns. Regroup it by what the
settings affect, not by what is left over.

Suggested target order:

### 1. Dates, Times & Costs
- Hide Start/Due dates for completed tasks
- Display dates and times in ISO format
- Include the weekday when displaying dates
- Round Time Periods to whole numbers
- Display Time Periods in HMS format
- Hide zero Time Periods and Costs
- Append the user's Created By name when pasting Date/Time
- Append the following text when pasting Date/Time (+ text field)

### 2. Task Completion
- Show a partially-complete checkbox for incomplete parent tasks
- Hide % Complete for completed tasks
- Show % Complete as a progress bar
- Hide zero % Complete (0%)
- Set % Complete increment

### 3. Task Titles & Icons (new heading)
- Show checkbox beside task title when completion column is hidden
- Strike-through completed tasks
- Display comments after a task's title
- Display only the first line of comments
- Show parent tasks with a folder icon
- Display File Link images as thumbnails

### 4. Task Tree Layout (new heading)
- Show attribute columns to the right of the task tree
- Display the selected task's path in the header bar
- Always hide parent tasks in List View
- Set focus to tasklist whenever ToDoList is activated
- Limit width of columns to N pixels

### 5. Tooltips & Info Tips (new heading)
- Show task Image Tips
- Show task Info Tips
- Limit length of comments shown to N characters

### 6. Priority, Risk & Reminders (new heading)
- Hide Priority number
- Limit the number of Priority/Risk levels
- Show Reminders as dates and times

Moved out of this page:

- Hide the splitter bar between task titles and attribute columns →
  `User Interface > General > Task View Layout` (see Proposal 2).

Rationale:

- The `Other` group is the main scanning problem: it currently spans title,
  icon, tooltip, column layout, list view, priority and reminder settings.
- Title-affecting completion options (`checkbox beside title`,
  `strike-through`) are grouped with the other title settings, leaving
  `Task Completion` focused on percentage/progress display.
- `Show attribute columns to the right` is arguably also a general layout
  setting; it is kept under `Task Tree Layout` for now because it directly
  concerns attribute columns. Moving it alongside the splitter is an open
  question.

Implementation note: moving the splitter setting across pages is not a pure
resource reorder. `IDC_HIDEPANESPLITBAR` currently has its DDX, member,
getter (`CPreferencesUITasklistPage::GetHidePaneSplitBar`) and tooltip in the
Tasklist Attributes page. Moving it to `CPreferencesUIPage` requires moving
those too, plus updating `CPreferencesDlg::GetHidePaneSplitBar`
(`PreferencesDlg.h:294`). That should be a separate change from reordering
controls within the page.

## Proposal 3: consolidate defaults

Both of these are defaults for new/previously unopened tasklists:

- `Default Task View Visibility` (`PreferencesDlg.h:254`).
- `Column/Attribute Visibility`
  (`PreferencesDlg.h:265`, getter is literally
  `GetDefaultColumnEditFilterVisibility`).

Proposed target tree:

```
Defaults
  Task Attributes
  Inheritable Attributes
  Dynamic Droplists
  Task View Visibility
  Column/Attribute Visibility
```

Recommended end-state:

1. Promote the current `Tasks > Defaults` branch to a top-level `Defaults`
   folder. This is mostly a path-string change:
   - `Tasks>Defaults>Task Attributes` → `Defaults>Task Attributes`
   - `Tasks>Defaults>Inheritable Attributes` → `Defaults>Inheritable Attributes`
   - `Tasks>Defaults>Dynamic Droplists` → `Defaults>Dynamic Droplists`
2. Move `Column/Attribute Visibility` from `User Interface` to `Defaults` and
   keep it as a standalone page (no control moves required).
3. Extract the `Default Task View Visibility` listbox from
   `CPreferencesUIPage` into the visibility page, or into a small dedicated
   Defaults visibility page, and index it as `Defaults > Task View
   Visibility`.
   - The cleanest option is to add a "Task Views" group to
     `CPreferencesUIVisibilityPage` alongside the existing visibility grid, and
     rename that page to "Default Visibility".
   - That puts both "what is visible by default" settings on one page and
     removes the UI page from the defaults business entirely.
4. Drop the redundant "Default" prefix from the index label; under `Defaults`,
   `Task View Visibility` is already clear.

Low-risk staging:

- **Stage A (tree-only):** add `Defaults > Task View Visibility` and
  `Defaults > Column/Attribute Visibility` entries that point at the existing
  controls/sections. This relies on UI-014 section highlighting to land on the
  right group, but leaves the controls physically on their current pages.
- **Stage B (clean layout):** move the listbox/grid controls and update the
  owning page getters so the settings actually live on the Defaults page.

## Proposal 4: naming cleanup

These are optional but address the same discoverability problem:

| Current | Problem | Suggested |
|---|---|---|
| `User Interface > Main Toolbar` | Duplicates the "Main Toolbar" group in UI General | `User Interface > Custom Toolbar` |
| `Main Toolbar` group in UI General | Sounds like the toolbar editor | `Task Toolbar Placement` |
| `User Interface > Tasklist Attributes` | Easily confused with default task attributes | `User Interface > Tasklist Display` |
| `Tasks > Defaults > Task Attributes` | Under a promoted Defaults folder | `Defaults > Task Attributes` is sufficient |
| `File Actions (more)` | Opaque split | `Backup & Saving` or merge into `File Actions` |

Do not change preference keys or historical identifiers; these are display
labels only.

## Interaction with UI-014 section highlighting

- New group headings added through `AddGroupLine` are automatically recognised
  as section boundaries by `CPreferencesPageBase::HighlightSection`
  (`Core/Shared/PreferencesBase.cpp:362-457`).
- Because the new visual groups will not be added to the index, they become
  boundaries but not highlight targets. That is intentional.
- Consequence: if a `Layout` heading is placed at the top of the UI page, the
  page-level `User Interface > General` entry will no longer have ungrouped
  content above the first heading, so it will stop drawing a band. That matches
  the current rule for page-level items with no ungrouped content.
- If a page-level item should still highlight its first group, it must be
  explicitly indexed to that group; do not reintroduce a generic
  "highlight the first group" fallback, for the reasons recorded in UI-014.

## Risks and considerations

- Reordering dialog controls means editing the `.rc` layout and verifying tab
  order, control enable/disable logic and conditional visibility. DDX and
  tooltips are keyed by control ID, so they survive reordering.
- `AddGroupLine` expects real static controls; new headings need resource IDs
  and must not collide with existing ones.
- Promoting `Defaults` to a top-level folder changes the visible tree path and
  the page caption for existing defaults pages. It does not change any saved
  settings.
- Moving the task-view listbox from `CPreferencesUIPage` to another page
  requires moving the DDX, getter plumbing in `PreferencesDlg`, and the
  `IDC_TASKVIEWVISIBILITY` resource block. That is the largest code change in
  this proposal and should be staged separately.
- Moving the splitter setting from `CPreferencesUITasklistPage` to
  `CPreferencesUIPage` likewise requires moving the DDX, member, getter,
  tooltip and the `PreferencesDlg` wrapper; keep it separate from pure
  reordering work.
- Reordering controls within `Tasklist Attributes` is resource-only and does
  not touch preference keys, but the new group headings must be registered in
  `OnFirstShow` with `AddGroupLine`.
- The index Search behaviour depends on page paths. Re-pathing pages should be
  checked against search to ensure each page still appears under its new path.

## Open questions

1. Should `Defaults` be promoted to a top-level branch, or should it remain
   under `Tasks` while receiving the two visibility pages?
2. Is the proposed five-part General page too tall? If so, merge
   `Files & Maintenance` into `System Integration`.
3. Should `Tasklist Display` (`Tasklist Attributes`) also move under
   `Defaults`, since most of its settings are global display defaults?
4. Should the custom toolbar page keep the name `Main Toolbar`, or should the
   UI page group be renamed instead?
5. Should "Show attribute columns to the right of the task tree" move to
   `Task View Layout` together with the splitter setting, since both describe
   the overall title/column layout?
6. Are `Show checkbox beside task title` and `Strike-through completed tasks`
   better left in `Task Completion`, or is grouping them under
   `Task Titles & Icons` clearer?

## Test notes

Not implemented. When implemented, verify:

- Every preference key still loads/saves under its existing key.
- Tooltips still attach to the correct controls after reordering.
- UI-014 group bands land on the intended new headings.
- The splitter setting still loads/saves after moving it to the UI page, and
  the related task-view settings still load/save after regrouping.
- Search finds pages under their new paths.
- `TDLTest` passes and the Preferences dialog has no clipped controls at the
  default font and DPI.
