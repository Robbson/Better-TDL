# GEN-014 Version source and automatic build number

Status: **Done** (implemented; Debug/Release build wiring; version resource verified)

## Goal

Keep the application version in a single committed file, `version.txt`, and derive the
fourth (build) component automatically so that a built binary always shows how far the
working code has moved past the last released version. No counter file is committed and no
manual edit of `ToDoList.rc` is required.

## Upstream starting point

- The version was hard-coded in `Core/ToDoList/ToDoList.rc` (`FILEVERSION`, `PRODUCTVERSION`,
  `FileVersion`, `ProductVersion`).
- `BuildRelease.bat` only printed a reminder to update the version by hand.
- The application reads the version from its own executable resource through
  `FileMisc::GetAppVersion` (`Core/Shared/FileMisc.cpp`), so the resource alone decides
  what the app reports.

## Design

- `version.txt` (repository root) contains the **last released** version with 2–3 components,
  currently `1.0.0`. It is the only place a human edits a version.
- `tools/version.ps1` reads `version.txt` and computes the fourth component as the number of
  commits made since `version.txt` was last changed:
  `git rev-list --count <last-version-txt-commit>..HEAD`.
- The script writes `version.h` into the build intermediate directory, not into the source
  tree. It is only rewritten when its content changes, so incremental builds are unaffected
  until a commit moves the build number.
- `ToDoList.rc` includes `version.h` and uses `TDL_VER_FILEVERSION` /
  `TDL_VER_FILEVERSION_STR` (and the product equivalents).
- A `PreBuildEvent` in `Core/ToDoList/ToDoList.vcxproj` runs the script for both
  configurations (IDE and batch builds), and `ResourceCompile` gets `$(IntDir)` on its
  include path.
- Because `rc.exe` does not track included headers reliably, the script also touches
  `ToDoList.rc` when the generated version changes, forcing a resource recompile.

## Behaviour

- A release commit sets `version.txt` to e.g. `1.0.0`; the build right after that commit
  reports `1.0.0.0`.
- Any later commit increases the reported version (`1.0.0.1`, `1.0.0.2`, …) without touching
  `version.txt`. Seeing such a build number in a binary therefore means the code is ahead of
  the recorded release.
- If `version.txt` already has four components, the fourth is used verbatim and git is not
  consulted.
- Without git (for example an exported source zip), the build component falls back to `0`.
- Builds are reproducible for a given commit; no generated file enters the source tree or
  git status.

## Files touched

- `version.txt` (new)
- `tools/version.ps1` (new)
- `Core/ToDoList/ToDoList.rc`
- `Core/ToDoList/ToDoList.vcxproj`
- `BuildRelease.bat` (reminder now points at `version.txt`)

## Risks

- The build number changes on every commit, so a rebuild after a commit recompiles the
  resource and relinks; builds without new commits stay incremental.
- Detached/shallow clones or a source export cannot compute a commit count and report `0`.
- `version.txt` must be committed for the count to include repository history.

## Test notes

- `tools/version.ps1` run standalone prints the computed version.
- Build the `ToDoList` project and check **Help > About** / the exe `FileVersion`; bump the
  count with a commit and rebuild to confirm the fourth component changes.
