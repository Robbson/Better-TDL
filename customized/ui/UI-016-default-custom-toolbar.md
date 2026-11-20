# UI-016: Custom toolbar default configuration

Status: **Done** (implemented and built; TDLTest passes)

## Goal

When no `ToDoList.ini` exists, or it contains no `[CustomToolbar]` buttons, the app seeds
the custom toolbar with a built-in showcase list. Replace that list with the toolbar
configuration used during development so a fresh install gets the more useful view and
task actions.

## Upstream starting point

- `CPreferencesUICustomToolbarPage::LoadPreferences` builds the buttons from
  `[CustomToolbar] Buttons` (`Core/ToDoList/PreferencesUICustomToolbarPage.cpp:84-119`).
- If the list is empty and `FirstTime` is true, it adds the upstream showcase buttons:
  New, Print Preview, Send Tasks, Flag, Lock, Paste Attributes, Clear Filter, Refresh
  Filter, Analyze Logged Time, Donate and Check For Updates
  (`PreferencesUICustomToolbarPage.cpp:121-140`).
- The default is saved on the next preferences write, after which `FirstTime` is false.

## What changed

File: `Core/ToDoList/PreferencesUICustomToolbarPage.cpp`.

The default button list is now the development `[CustomToolbar]` configuration:

```text
Show Tasklist Tabbar, Show Filterbar
| Comments Bottom, Comments Right
| Flag, Lock, Paste Attributes
| Clear Filter, Refresh Filter
| Analyze Logged Time
```

Mapped to `ID_VIEW_SHOWTASKLISTTABBAR` (image `5`), `ID_VIEW_SHOWFILTERBAR` (`87`),
`ID_VIEW_COMMENTSPOS_BOTTOM` (`6`), `ID_VIEW_COMMENTSPOS_RIGHT` (`7`),
`ID_EDIT_FLAGTASK` (`49`), `ID_EDIT_LOCKTASK` (`100`),
`ID_EDIT_PASTEATTRIBUTES` (`258`), `ID_VIEW_CLEARFILTER` (`62`),
`ID_VIEW_REFRESHFILTER` (`99`) and `ID_TOOLS_ANALYSELOGGEDTIME` (`93`).

The leading separator present in the development INI was dropped: it was left behind when
the preceding buttons were deleted and would only produce an empty gap at the start of a
fresh toolbar.

## Behaviour

- Only affects first-time users or users whose INI has no custom toolbar buttons and no
  `FirstTime=0`. Existing custom toolbars are unchanged.
- The upstream default's New, Print Preview, Send Tasks, Donate and Check For Updates
  buttons are no longer seeded automatically; users can still add them from preferences.

## Risks

- `ID_VIEW_COMMENTSPOS_BOTTOM`/`RIGHT` are view-state actions. They are present in the
  menu regardless of the comments pane, so toolbar enablement matches normal command
  handling.
- If image names `5`, `6`, `7`, `87` change upstream, the buttons fall back to no image
  (existing behaviour for unknown image IDs).

## Test notes

- Build: `Core/ToDoList_Core.sln` (`Unicode Debug`, `Win32`).
- `TDLTest` passes.
- Manual check: start with an INI containing no `[CustomToolbar]` section (or delete it)
  and confirm the seeded toolbar matches the list above.
