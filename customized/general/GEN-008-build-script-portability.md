# GEN-008 — Portable build and packaging scripts

**Status:** In progress (implemented; plugin Release build verified; end-to-end zip pending)

## Goal

Make the root batch scripts usable on a normal developer machine. Upstream scripts
hard-code tool locations such as `D:\Tools\ProjectZip.exe`,
`D:\tools\rcedit-x64.exe`, `C:\Program Files (x86)\MSBuild\14.0\...` and the VC6
`msdev.exe`. None of these exist on this machine, and the hard-coded MSBuild is older than
the project toolset (`v143`).

## Upstream starting point

Upstream `BuildDebug.bat`, `BuildRelease.bat`, `BuildReleaseZip.bat`, `BuildCodeZip.bat`,
`CleanDebug.bat`, `CleanRelease.bat` and `CopyResToBuildFolders.bat` each embed absolute
tool paths and assume a fixed sibling-folder layout.

## Changes

- New `FindTools.bat`, called by the other scripts, resolves:
  - `MSBUILD` from the environment, `PATH`, `vswhere.exe` or VS 2022/2019 install folders.
  - `SEVENZIP` from the environment, `PATH` or `Program Files\7-Zip`.
  - `RCEDIT` from the environment, `PATH`, `<repo>\tools` or the legacy `D:\tools`.
  - `PROJECTZIP` from the environment, `PATH`, `<repo>\tools` or the legacy `D:\Tools`.
  - `MSDEV` from the environment, `PATH` or the VC6 install folder (optional).
  - Sibling paths and the archive version folder, overridable through
    `TDL_RESOURCES`, `TDL_TESTREPO`, `TDL_SYMBOLS`, `TDL_DOWNLOADS`, `TDL_PREV` and
    `TDL_PREV_VERSION`.
- Found tools are stored in the quoted form the scripts already use.
- Null checks changed from `IF NOT EXIST %TOOL%` to `IF NOT DEFINED`, because `EXIST`
  with an unset variable produces a syntax error.
- `BuildReleaseZip.bat` copies `ConvertRTFToHTML.exe` from its own output folder
  (`Core\ConvertRTFToHTML\Unicode_Release`) into the release output, because the project
  does not post-build copy it there.
- `BuildReleaseZip.bat` runs a preflight over its file list before versioning and zipping.
  If any binary is missing it prints the list and tells the user to run `BuildRelease.bat`
  instead of opening a log full of rcedit "Unable to load file" errors.
- `BuildRelease.bat` and `BuildDebug.bat` build the plugin solution with
  `/p:PlatformToolset=v143` because the plugin projects still declare the VS2015 `v140`
  toolset (which requires Windows SDK 8.1).
- When the .NET Framework 4.0 reference assemblies are not installed, `FindTools.bat`
  selects the newest installed targeting pack (4.8 here) and the scripts pass it as
  `/p:TargetFrameworkVersion=...`. `TDL_DOTNET_TFM` overrides the choice.
- `Plugins/3rdParty/MSHtmlEditorControl/HtmlEditorControl.resx` used shortcut values
  (`Ctrl+Z`, `Del`) that the modern `KeysConverter` cannot parse during ResX compilation.
  They are now `Control+Z` and `Delete`.
- `BuildRelease.bat` uses the VC6 workspace only when `MSDEV` is found; otherwise it builds
  `Core\ToDoList_Core.sln` with MSBuild (`Unicode Release|Win32`). Error matching accepts
  both the MSDev `") : error"` and MSBuild `"): error"` forms.
- `BuildReleaseZip.bat` uses `%SystemRoot%\SysWOW64` (falling back to `System32`) instead
  of the literal `C:\Windows` path for the bundled VC runtime DLLs.
- Missing output folders (`ToDoList_Downloads\Latest`, `ToDoList_Prev\<version>`,
  `ToDoList_Symbols`) are created before use.
- `BuildReleaseZip.bat` prints the final release/downloads/symbols archive paths and
  pauses at the end, so a double-clicked window no longer closes before the result can be
  read. The copy and move into the sibling folders are now error-checked and pause with
  the source/target paths when they fail.

## Files

- `FindTools.bat` (new)
- `BuildDebug.bat`, `BuildRelease.bat`, `BuildReleaseZip.bat`, `BuildCodeZip.bat`,
  `CleanDebug.bat`, `CleanRelease.bat`, `CopyResToBuildFolders.bat`
- `Plugins/3rdParty/MSHtmlEditorControl/HtmlEditorControl.resx` (ResX key values)
- `docs/BUILDING.md` (new), linked from `README.md`, `PROJECT_SCOPE.md` and `AGENTS.md`

## Risks

- Environment variables win over discovery, so a stale `MSBUILD` or `SEVENZIP` in the
  caller's environment takes precedence by design.
- If VC6 is installed, `BuildRelease.bat` still prefers `msdev.exe`; unset `MSDEV` to force
  the MSBuild path.
- `BuildCodeZip.bat` needs ProjectZip; `BuildRelease.bat` skips the source zips with a
  message when it is not available.
- `TDL_PREV_VERSION` defaults to `9.3` and must be updated for a new release line.

## Test notes

- `FindTools.bat` was called from `cmd` and resolved MSBuild
  (`VS2022 Community`), `7z.exe` and `rcedit-x64.exe`; `PROJECTZIP` and `MSDEV` stayed
  unset on this machine, which the scripts now report cleanly.
- `BuildReleaseZip.bat` preflight was exercised against the current partial build: it
  copied `ConvertRTFToHTML.exe` and listed the missing plugin DLLs before any versioning.
- The Plugins solution now builds with `Release|Mixed Platforms` using the v143 and v4.8
  overrides, and the release output contains all 93 files the zip script requires.
- The final copy/move block was exercised against a temp tree: both destinations are
  created, success is not misreported as an error, and the summary prints the exact paths.
- Full `BuildDebug.bat`, `BuildRelease.bat` and `BuildReleaseZip.bat` runs are still to be
  done on a clean checkout.
