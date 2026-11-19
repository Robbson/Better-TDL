# GEN-007 — TDL set files (`*.tdlset.txt`)

**Status:** In progress (implemented and built; TDLTest passes; interactive check pending)

## Goal

Open a named, ordered group of `.tdl` documents from a single plain-text file in the
Home Folder view. Compared with opening a folder this provides:

1. a user-defined open order, and
2. overlapping sets, so a shared `.tdl` can belong to several sets.

The file is ordinary text so it can be edited anywhere, but the double suffix lets the
app distinguish it from a normal `.txt` document.

## Format

File name: `<name>.tdlset.txt`.

```text
# Lines starting with # or ; are ignored
..\Projects\General.tdl
..\Projects\Release.tdl
C:\Tasklists\Personal.tdl
```

- One `.tdl` document per line.
- Blank lines and lines beginning with `#` or `;` are ignored.
- Surrounding whitespace and matching single or double quotes are removed.
- Relative paths are resolved against the set file's folder first, then the configured
  Home folder, then the application folder.
- Entries are de-duplicated and opened in file order.
- Non-`.tdl` lines are ignored.

## Implementation

- `Core/ToDoList/TDLHomeFolder.cpp/.h`
  - `IsTasklistSetFile` recognises the compound `.tdlset.txt` suffix.
  - `ReadTasklistSet` parses the file and resolves relative paths.
  - Home scanner marks sets with task icon `85`, type `tdlset.txt`, derives
    `Opened`/`Modified` status from the first open or modified member, uses the same
    light-gray closed text colour as `.tdl` rows, and previews the set's text in the
    comments editor when selected (up to 128 KB).
  - Folder collection expands sets into their member tasklists.
- `Core/ToDoList/TDLSimpleTextContentCtrl.cpp`
  - A full auto-indent pass now always repaints the control, so replacing a longer
    preview with a short set file (or an empty selection) cannot leave stale text
    visible.
- `Core/ToDoList/ToDoListWnd.cpp/.h`
  - Double-click/Enter and **Open** open every member through `OpenTaskLists`, so the
    existing delayed-loading behaviour (PERF-008) applies.
  - **Open & Replace** closes other documents and then opens the set members.
  - **Close** closes any open members of the selected set.
  - **Open as Text** is not offered for set files.
  - Duplicate members across selected sets/folders are removed before opening.

## Files

- `Core/ToDoList/TDLHomeFolder.h/.cpp`
- `Core/ToDoList/ToDoListWnd.h/.cpp`
- `Core/ToDoList/TDLSimpleTextContentCtrl.cpp`
- `customized/general/GEN-005-home-folder-view.md` (cross-reference)
- `customized/index.md`

## Manual checks

- A `*.tdlset.txt` row shows icon `85` and type `tdlset.txt`.
- A closed set row uses the same light-gray text as a closed `.tdl` row.
- Selecting a set shows its text in the comments editor.
- Switching from a longer text preview to a set clears the old text completely.
- Double-clicking or pressing Enter opens all listed `.tdl` files in the listed order.
- With delayed loading enabled, only the first listed document loads immediately.
- **Open & Replace** closes the other documents before opening the set.
- **Close** on a set closes every member that is currently open.
- Status shows `Opened`/`Modified` when any member is open or modified.
- Relative entries resolve from the set file's folder; absolute entries work unchanged.
- Comment and blank lines are ignored and matching quotes are stripped.
- Opening a folder that contains a set includes the set's members.
- **Open as Text** is not shown for set files.

## Risks / limitations

- There is no watcher: editing a set is picked up on the next Home refresh.
- Sets cannot nest; entries must point directly to `.tdl` files.
- A member that no longer exists is reported by the normal tasklist load error path.
- Only the Home view recognises sets; Explorer double-click still uses the `.txt`
  association.
