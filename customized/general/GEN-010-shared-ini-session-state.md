# GEN-010 — Sharing session state through a common ToDoList.ini

**Status:** Proposed (analysis complete; remaining portability gaps identified, no code change yet)

Related: [GEN-005](GEN-005-home-folder-view.md) (home folder and home-relative session/MRU).

## Goal

Establish whether one `ToDoList.ini` can be used by the same user across several
machines (synced or copied), what already works after the home-folder change, and which
stored values still pin the file to one machine.

This is an analysis document. It records the current state and the concrete gaps so a
follow-up change can make the profile intentional rather than incidental.

## Verdict

The home/session/MRU part is now portable **provided each machine has its own home folder
configured**. The INI as a whole is not yet machine-independent: several preference
values are still written as raw absolute paths, and session restore is a single global
list rather than per-machine state.

Consequences:

- Opening, restoring and switching tasklists inside the home folder works across
  machines.
- Files outside the home folder only travel when they happen to be relativizable to the
  application folder on the same drive.
- Tools, sounds, themes, spell-checker engines, file-dialog history and several
  export/import folders still point at the first machine's paths. They usually fail
  softly, but a path that also exists on the second machine can silently select the
  wrong executable, sound, theme or output folder.
- Two live instances/machines writing the same INI remain unsafe because the whole file
  is rewritten and the last flush wins
  ([APPLICATION_STATE.md](../../docs/APPLICATION_STATE.md), lines 380-381).

## What is already portable

### Home folder location is per machine

`[HomeFolders]` uses the sanitized computer name as the key and the absolute folder as
the value (`Core/ToDoList/TDLHomeFolder.cpp:88-129`). Each machine writes and reads only
its own entry, so one shared INI can carry different home folders.

### Session and MRU are stored relative

- `CToDoListWnd::SaveSettings()` writes `[Settings] LastFileN` and `LastActiveFile`
  through `CTDLHomeFolder::GetStoredPath` (`Core/ToDoList/ToDoListWnd.cpp:2654-2708`).
- `[MRU]` is written through `CEnRecentFileList::WriteList` with the home folder
  (`ToDoListWnd.cpp:2725`, `Core/Shared/EnRecentFileList.cpp:108-131`).
- Restore resolves relative entries against the home folder first, then the application
  folder (`ToDoListWnd.cpp:2740-2776`, `TDLHomeFolder.cpp:164-185`;
  MRU read at `EnRecentFileList.cpp:132-169`, `ToDoListWnd.cpp:3655-3657`).

A file under the home folder is therefore stored as `.\name.tdl` and resolves to each
machine's own home folder. Storage-backed tasklists are separate: they are encoded with
`TSM_TASKLISTINFO` and have their local path cleared before saving
(`ToDoListWnd.cpp:2679-2696`).

### Per-tasklist state does not embed a folder

`CPreferences::KeyFromFile` defaults to the file name only
(`Core/Interfaces/Preferences.cpp:939-952`), and all per-tasklist state uses that form
(`ToDoCtrl.cpp:8775-8790`, `TDLCommentsCtrl.cpp:703`, `ToDoCtrlReminders.cpp:568`).
`[FileStates\...]` therefore carries no drive or folder and is shared cleanly between
machines.

Known trade-off: two different tasklists with the same file name share one state entry.
That is pre-existing and not caused by sharing.

### Some paths are already made relative on load

- Export folder and save-export stylesheet: `PreferencesFile2Page.cpp:283-291`.
- Due-task stylesheet: `PreferencesFilePage.cpp:207-217`.
- Language file: stored relative (`PreferencesGenPage.cpp:356`).
- Spell-checker dictionary list and active dictionary: `SpellCheckDlg.cpp:350-359`.
- Backup folder: relative only when the "relative to application" mode is selected
  (`PreferencesFile2Page.cpp:216-229`, `:242`, `:297`).

## Remaining machine-specific values

| Setting | Where stored | Handling today | Effect on a second machine |
|---|---|---|---|
| `[Tools\ToolN] Path`, `IconPath`, `Cmdline` | `PreferencesToolPage.cpp:1073-1077` | Raw strings, no relativization | Tool or icon missing; a same-named path can launch the wrong program |
| `PathToStickies` | `PreferencesGenPage.cpp:357` | Raw string | Stickies integration points at the wrong install |
| `UIThemeFile` | `PreferencesUIPage.cpp:322` | Default relative to `Resources\Themes` (`:234`, `:350`); user-picked file can be absolute | Theme not found or wrong theme |
| `[SpellChecker] EnginePath` | `SpellCheckDlg.cpp:572` | Raw string (dictionaries are relativized) | Spell checker DLL not found or wrong DLL |
| Reminder sounds: `[Reminders] SoundFile`, `ReminderSound`, per-tasklist sound | `PreferencesTaskPage.cpp:332`, `PreferencesTaskDefPage.cpp:301`, `TDCReminder.cpp:250-272` | Raw string; default is `%WINDIR%\media\chimes.wav` | Wrong or missing sound |
| `[OpenFileDialog]`/`[SaveFileDialog]` last folder per dialog title | `EnFileDialog.cpp:153-155`, `:192-194` | Raw absolute folder | Dialog opens in a non-existent or unrelated folder |
| Import/export/print/transform/analysis folders | `TDLImportDialog.cpp:179,436`; `TDLExportDlg.cpp:579-583`; `TDLPrintDialog.cpp:362`; `TDLTransformDialog.cpp:73`; `TDLAnalyseLoggedTimeDlg.cpp:277` | Raw absolute folders | Wrong default output/input location |
| Remote last upload/download folders | `Core/Shared/RemoteFile.cpp:434,617,668,738` | Raw server/folder strings | Wrong local or remote default |
| Backup folder configured as a full path | `PreferencesFile2Page.cpp:242,297` | Absolute unless relative-to-app mode is chosen | Backups may be written elsewhere, or fail |
| `[Pos]`, fonts, DPI and monitor layout | `ToDoListWnd.cpp:2657-2673`, UI pages | Raw pixel/size values | Off-screen or odd-sized window; fonts substituted |

None of these corrupt task data, because task data lives in the `.tdl` document. The
risk is operational: wrong tool, wrong sound/theme, wrong backup or export destination.

## Behavioural caveats beyond paths

1. **Session restore is global, not per-machine.** `[HomeFolders]` is per machine but
   `[Settings] LastFileN`/`LastActiveFile` are one list. If two machines' home folders
   contain different files, the same relative entry can resolve to a different or missing
   file. This is fine when the home folder contents are kept in sync; otherwise the
   session should be keyed per machine as well.
2. **Relativization stops at drive boundaries.** `FileMisc::GetRelativePath` returns the
   original absolute path when `PathRelativePathTo` cannot relate the two locations
   (`Core/Shared/FileMisc.cpp:2610-2624`). `GetStoredPath` only prefers the home folder
   when the file is inside it, then falls back to the application folder
   (`TDLHomeFolder.cpp:147-161`). A tasklist on another drive, outside the home folder,
   stays absolute.
3. **Application-folder-relative entries depend on identical install layouts.** Files
   outside the home folder but on the app drive are stored relative to the application
   folder. Machines that install ToDoList in different folders will not resolve them.
4. **First run on a new machine has no home folder entry.** Until the home folder is
   configured there, relative session/MRU entries fall back to the application folder and
   may not resolve. This is expected but worth documenting for users.
5. **Concurrent writers clobber each other.** Do not run two instances against the same
   live INI, and prefer sync-while-closed over a live network share
   ([APPLICATION_STATE.md](../../docs/APPLICATION_STATE.md), lines 380-381).
6. **Machine keys are only the computer name.** A renamed machine loses its
   `[HomeFolders]` entry; two machines with the same name would collide. The assumption
   is one user with uniquely named machines.

## Recommendation

If a genuinely portable profile is wanted, add one shared helper for path-valued
preferences:

- Store a path relative to the home folder when it is inside it, otherwise relative to
  the application folder when on the same drive, otherwise leave it absolute.
- Resolve on load in the reverse order: home folder, then application folder, then the
  stored absolute path.
- Apply it to every path-valued preference in the table above, including command lines
  that embed paths.
- Keep genuinely machine-specific state (home folder, window position, sounds, tools)
  either out of the shared profile or under a machine-keyed section such as
  `[HomeFolders]`.

The existing `CTDLHomeFolder::GetStoredPath`/`ResolveStoredPath` pair is the natural
starting point; it currently only knows about session/MRU data.

## Files and evidence

- `Core/ToDoList/TDLHomeFolder.cpp/.h` — machine key, home-folder lookup, stored/resolved
  paths.
- `Core/ToDoList/ToDoListWnd.cpp` — session save/restore and MRU wiring.
- `Core/Shared/EnRecentFileList.cpp/.h` — home-relative MRU storage.
- `Core/Interfaces/Preferences.cpp` — `KeyFromFile` filename-only section keys.
- `Core/ToDoList/ToDoCtrl.cpp`, `TDLCommentsCtrl.cpp`, `ToDoCtrlReminders.cpp` —
  per-tasklist `FileStates` keys.
- Preference writers listed in the table above.
- [APPLICATION_STATE.md](../../docs/APPLICATION_STATE.md) — backend selection, write
  timing and concurrent-writer warning.

## Manual checks

If the recommendation is implemented:

- Configure different home-folder paths on two machines, sync the INI, and confirm the
  session and MRU restore the correct files on both.
- Store an external tool, reminder sound and theme inside the home folder; confirm both
  machines resolve them after sync.
- Store a tasklist outside the home folder on another drive; confirm it remains absolute
  and is documented as non-portable rather than silently lost.
- Confirm no `[HomeFolders]` entry from one machine is overwritten by the other.
- Confirm closing both apps and syncing does not lose unrelated preferences (whole-file
  rewrite, last flush wins).
