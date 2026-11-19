# Building and Packaging

How to build the core app and plugins, run the tests and create the release zips with the
root batch scripts. The scripts contain no machine-specific install paths: they locate every
external tool at run time so the repository can live anywhere.

## 1. Requirements

| Tool | Needed for | Where to get it |
|---|---|---|
| Windows 10/11 (x64) | everything | — |
| Visual Studio 2022, workload **Desktop development with C++** | MSBuild, v143 toolset, MFC, C++/CLI plugin bridges | <https://visualstudio.microsoft.com/downloads/> |
| .NET Framework targeting pack | C# / managed plugin projects | Installed with Visual Studio; 4.8 is used when the 4.0 pack is absent |
| 7-Zip (`7z.exe`) | `BuildReleaseZip.bat` | <https://www.7-zip.org/download.html> |
| rcedit (`rcedit-x64.exe`) | `BuildReleaseZip.bat` version stamping | <https://github.com/electron/rcedit/releases> |
| ProjectZip (`ProjectZip.exe`) | `BuildCodeZip.bat` source zips (optional) | Legacy AbstractSpoon utility, <https://www.abstractspoon.com/>; `BuildRelease.bat` skips the source zips when it is missing |
| Visual C++ 6.0 (`msdev.exe`) | legacy core build only (optional) | Not required; see below |

Visual Studio 2015's MSBuild (`14.0`) is no longer required. The project files use toolset
`v143`, and `BuildRelease.bat` falls back to `MSBuild` on `Core\ToDoList_Core.sln` when
`msdev.exe` is not installed.

The plugin projects still declare `v140` and .NET Framework `v4.0`. The build scripts pass
`/p:PlatformToolset=v143` and, when the v4.0 reference assemblies are missing, the newest
installed `TargetFrameworkVersion` (for example `v4.8`). Set `TDL_DOTNET_TFM` before calling
a script to force a specific framework.

## 2. Tool discovery

`FindTools.bat` in the repository root is called by the other scripts. For each tool it
uses the first match from:

1. the environment variable already being set by the caller,
2. `PATH`,
3. well-known install locations (`vswhere.exe`, Program Files, `<repo>\tools`).

The resulting variables are quoted paths, ready to use as `%MSBUILD%`, `%SEVENZIP%`, etc.

| Variable | Tool | Default lookup |
|---|---|---|
| `MSBUILD` | `MSBuild.exe` | `PATH`, `vswhere`, VS 2022/2019 install folders |
| `MSDEV` | `msdev.exe` | `PATH`, VC6 install folder (optional) |
| `SEVENZIP` | `7z.exe` | `PATH`, `Program Files\7-Zip` |
| `RCEDIT` | `rcedit-x64.exe` | `PATH`, `<repo>\tools`, `D:\tools` |
| `PROJECTZIP` | `ProjectZip.exe` | `PATH`, `<repo>\tools`, `D:\Tools` |

To use a specific copy, set the variable before starting the script, for example:

```bat
set RCEDIT=D:\bin\rcedit-x64.exe
BuildReleaseZip.bat
```

## 3. Sibling folders

The release scripts read resources and write archives into sibling repositories next to
this one. The defaults match the usual checkout layout and can be overridden.

| Variable | Default | Contents |
|---|---|---|
| `TDL_RESOURCES` | `..\ToDoList_Resources` | runtime resources copied into the zip |
| `TDL_TESTREPO` | `..\ToDoList_Test` | sample tasklists and checklists |
| `TDL_SYMBOLS` | `..\ToDoList_Symbols` | symbol zip per version |
| `TDL_DOWNLOADS` | `..\ToDoList_Downloads` | `Latest\` release zip |
| `TDL_PREV` | `..\ToDoList_Prev` | archived builds and source zips |
| `TDL_PREV_VERSION` | `9.3` | version subfolder under `TDL_PREV` |

Output folders are created when missing.

## 4. Scripts

Run them from the repository root (they also work from any working directory because each
script `PUSHD`es to its own folder):

```bat
BuildDebug.bat        :: core Debug + TDLTest + Plugins Debug
BuildRelease.bat      :: core Release + TDLTest + Plugins Release, then both zip scripts
BuildReleaseZip.bat   :: version, symbols and binary zip only (needs existing Release build)
BuildCodeZip.bat      :: source zips only
CleanDebug.bat        :: MSBuild clean, Debug configurations
CleanRelease.bat      :: MSBuild clean, Release configurations
CopyResToBuildFolders.bat :: copy Resources and sample tasklists into build folders
```

`BuildReleaseZip.bat` reads the version from
`Core\ToDoList\Unicode_Release\ToDoList.exe`, so build Release before running it. It also
checks that every binary it is about to package exists and stops with a list of missing
files (typically because the plugin solution has not been built). Close any running copy of
`ToDoList.exe` from this build first, otherwise `rcedit` and `7z` fail on locked files.
`ConvertRTFToHTML.exe` is copied in from its own output folder automatically.

Individual build without the scripts:

```bat
msbuild Core\ToDoList_Core.sln /t:Build /p:Configuration="Unicode Release" /p:Platform=Win32 /m
```

## 5. Outputs

| Artifact | Location |
|---|---|
| `ToDoList.exe` / DLLs | `Core\ToDoList\Unicode_Release` |
| Binary release zip | `ToDoList_Downloads\Latest\todolist_exe_.zip` |
| Archived copy | `ToDoList_Prev\<TDL_PREV_VERSION>\ToDoList_exe.<version>._.zip` |
| Symbols | `ToDoList_Symbols\<version>.zip` |
| Source zips | `ToDoList_Prev\<TDL_PREV_VERSION>\ToDoList_src_Core.*.zip`, `..._Plugins.*.zip` |

## 6. Notes

- The scripts are interactive: they pause on errors and `BuildRelease.bat` pauses before
  zipping so the version number can be checked.
- `BuildRelease.bat` uses the VC6 workspace only if `MSDEV` is found; otherwise it builds
  `ToDoList_Core.sln` with MSBuild.
- `BuildDebug.bat`, `BuildRelease.bat`, `CleanDebug.bat` and `CleanRelease.bat` use
  `vswhere` to find MSBuild, so they do not need a Developer Command Prompt.
- The plugin projects still declare `v140` and .NET `v4.0`; the release/debug scripts
  override them with `v143` and the newest installed targeting pack. Set
  `TDL_DOTNET_TFM`, or pass `/p:TargetFrameworkVersion=...` to your own MSBuild call, to
  choose a different framework.
