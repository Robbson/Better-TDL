# GEN-013: Safe INI migration for compatibility issues

Status: **Done (built; migration and backup verified with legacy 6.8.10 and Better TDL 1.0.0.0 configs)**

## Goal

When an existing `ToDoList.ini` contains values that are not compatible with the current
build, migrate them once instead of asserting or silently ignoring them, and leave a copy
of the original file. The migration must also distinguish **Better TDL** (`1.0.0.0`) from
the original ToDoList version line (`6.x`-`9.x`) so that version checks do not apply
original-ToDoList upgrades to Better TDL configs (or vice versa).

## Upstream starting point

- `CToDoListApp::UpgradePreferences()` only had two version gates
  (`< 7.2.10` and `< 7.999.999.7`) that assume the original ToDoList version numbering.
  Better TDL writes `[AppVer] Version=1.0.0.0`, which compares as older than `7.2.10`, so
  those gates would run on every Better TDL start.
- `[AppVer]` recorded only `Version`, not the product name, so a config could not be
  attributed to Better TDL or ToDoList.
- Saved Find Tasks rules persist the numeric `TDC_ATTRIBUTE` in
  `[FindTasks\Searches\<name>\RuleN] Attribute=...`. Commit `2c41ca089`
  (`TDCA_ATTRIBUTECOUNT` -> `TDCA_DUMMY`/`TDCA_LAST_REALATTRIBUTE`) reset the enum and moved
  every pseudo attribute down by one. Legacy rules such as `Attribute=42`
  (previously `TDCA_ANYTEXTATTRIBUTE`) then decoded to `TDCA_DEPRECATED_1`, whose
  `SEARCHPARAM::GetAttributeType()` is `FT_NONE`. That triggered
  `ASSERT(nFindType != FT_NONE)` in `CTDCSearchParamHelper::DecodeAttribute()` and the
  follow-up `VERIFY(LoadSearch(...))` in `CTDLFindTasksDlg::GetSearchIncludesCompletedTasks()`,
  and the rule was dropped in Release builds.

## What changed

### 1. Product-aware preference versioning

Files: `Core/Interfaces/Preferences.cpp`, `Core/ToDoList/ToDoList.cpp`.

- `CPreferences::SaveInternal()` and the registry `CPreferences::Release()` now also write
  `[AppVer] Name=FileMisc::GetAppProductName()` (`Preferences.cpp:349`, `Preferences.cpp:206`).
- `InitInstance()` reads `[AppVer] Name` alongside `Version` (`ToDoList.cpp:1078`) and passes
  it to `UpgradePreferences`.
- `UpgradePreferences` treats the previous config as Better TDL when:
  - a stored product name matches the current product name, or
  - no name is stored and the version is below `2.0` (Better TDL uses `1.x`; the original
    ToDoList uses `6.x`-`9.x`).

  The original upstream migrations are then applied only when the config is **not** Better
  TDL (`ToDoList.cpp:1350-1354`, `ToDoList.cpp:1392`).

### 2. Legacy search-rule attribute migration

Files: `Core/ToDoList/TDCSearchParamHelper.cpp/.h`, `Core/ToDoList/ToDoList.cpp`.

- New `CTDCSearchParamHelper::MapLegacyAttributeID()` (`TDCSearchParamHelper.cpp:177`)
  leaves real and custom attributes alone, keeps any attribute that still decodes, and for
  an invalid pseudo-attribute ID tries `id - 1` and `id - 2` to find the valid legacy
  value (`42 -> 41` = `TDCA_ANYTEXTATTRIBUTE`).
- `DecodeAttribute()` applies the mapping before looking up the find type, so rules in a
  config that has not yet been migrated no longer assert and still load
  (`TDCSearchParamHelper.cpp:118`).
- `MigrateLegacySearchRuleAttributes()` (`ToDoList.cpp:1279`) enumerates
  `FindTasks\Searches\...\RuleN`, applies the same mapping and writes the corrected numeric
  ID back once. It runs from `UpgradePreferences` for both product lines
  (`ToDoList.cpp:1436`) and logs the number of repaired rules.

### 3. Original-config backup

Before the first write, the migration copies the on-disk INI to:

```text
<inifile>.pre-<product>-<version>.bak
```

Examples:

```text
ToDoList.ini.pre-ToDoList-6.8.10.0.bak
ToDoList.ini.pre-Better_TDL-1.0.0.0.bak
```

The existing `ini.Backup\ToDoList.<date>.ini` rotation in
`CPreferences::SaveInternal()` (`Preferences.cpp:393`) remains unchanged; the explicit
`.pre-*` copy makes the pre-migration state easy to find and is not overwritten.

## Risks / limits

- Only **numeric attribute IDs in saved search rules** are migrated persistently. Other
  persisted pseudo-attribute IDs, such as `EditVisibility`/`AttribState` entries, are not
  rewritten. Ambiguous legacy IDs that are still *valid* but mean a different attribute
  (for example pre-2021 `TDCA_PATH` vs current `TDCA_SUBTASKDONE`) cannot be detected
  generically; a full fix needs a stored schema version or name-based persistence.
- The `id - 1`/`id - 2` fallback runs only for IDs that do not decode at all, so current
  valid rules are never changed.
- `[AppVer] Name` is only written from this build onwards. Existing configs without a name
  are classified by version (`< 2.0` = Better TDL).
- Color preferences are user settings, not a compatibility shim; the migration does not
  change `TextColorOption`, `ColorAttribute` or `ColorTaskBackground`.

## Tests

- Debug build: `msbuild Core\ToDoList_Core.sln /p:Configuration="Unicode Debug" /p:Platform=Win32`.
- Legacy `ToDoList_Classic.ini` (`6.8.10.0`, two saved rules with `Attribute=42`):
  - both rules became `Attribute="41"`,
  - `ToDoList.ini.pre-ToDoList-6.8.10.0.bak` was created with the original 75,296 bytes,
  - `TextColorOption=2` and `ColorTaskBackground=0` were preserved,
  - zero assertions (`%TEMP%\ToDoList_asserts.log`).
- Better TDL `ToDoList.ini` (`1.0.0.0`, same legacy rules):
  - both rules became `Attribute="41"`,
  - `ToDoList.ini.pre-Better_TDL-1.0.0.0.bak` was created with the original 240,088 bytes,
  - `[AppVer] Name=Better TDL` was added,
  - zero assertions, and the original-ToDoList version gates were skipped.

## Related

- [GEN-010](GEN-010-shared-ini-session-state.md) - shared INI session state.
- [GEN-012](GEN-012-single-instance-window-matching.md) - another product-name-sensitive
  compatibility fix.
