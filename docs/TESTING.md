# Testing and Coverage

> Companion to [ARCHITECTURE.md](ARCHITECTURE.md) and [APPLICATION_STATE.md](APPLICATION_STATE.md).
> This document describes the existing test assets under `Core/TDLTest`, how they are
> classified, how they are run, what they do and do not verify, and whether code
> coverage can be measured.

---

## 1. Execution model

There is exactly one test project in the core solution: `Core/TDLTest/TDLTest.vcxproj`
(`Core/ToDoList_Core.sln`). It is a console MFC application that links the production
static library `Core/ToDoListLib` (`TDLTest.vcxproj:226-239`), so tests run production
code in-process rather than reimplementing it.

The harness is custom, not a framework such as GoogleTest/Catch:

- `CTDLTestBase` holds the assertion helpers (`ExpectEQ`, `ExpectTrue`, …), scoped test
  and subtest helpers, and per-test result counters (`Core/TDLTest/TDLTestBase.h:60-362`).
- `DoTests()` instantiates the suites in a fixed order (`Core/TDLTest/TDLTest.cpp:175-224`).
- Assertion failures are printed and counted in `TESTRESULT`; the executable still returns
  0 (`Core/TDLTest/TDLTest.cpp:170`, `226-230`).
- Debug builds install a CRT assert report hook that appends to
  `%TEMP%\TDLTest_asserts.log` (or `%TDL_ASSERT_LOG%`) instead of blocking on a dialog
  (`Core/TDLTest/TDLTest.cpp:57-120`).

The Debug project's post-build step runs `Unicode_Debug\TDLTest.exe` automatically
(`Core/TDLTest/TDLTest.vcxproj:154-160`); Release only copies the exe
(`TDLTest.vcxproj:101-106`).

Run commands:

```bat
:: normal suite (GUI harness skipped)
Core\TDLTest\Unicode_Debug\TDLTest.exe

:: include timings/benchmarks that are otherwise skipped
Core\TDLTest\Unicode_Debug\TDLTest.exe -p

:: UI e2e/perf harness (interactive desktop; moves the real mouse)
Core\TDLTest\Unicode_Debug\TDLTest.exe -ui
Core\TDLTest\Unicode_Debug\TDLTest.exe -ui -uiscroll
Core\TDLTest\Unicode_Debug\TDLTest.exe -ui -uisplitter
```

Current local result (Debug build, normal run on 2026-11-16): **84 subtests in 23
top-level suites, 310,297 PASS/FAIL checks, 0 failures, about 7 seconds**. The count is
high because several suites assert inside large loops (for example date offsets and
recurrence), not because there are 310k distinct scenarios.

---

## 2. What the tests are

### 2.1 Unit tests (pure/helper and domain logic)

These construct production objects and assert on return values, without launching the
app or a window:

| Suite | Focus |
|---|---|
| `CDateHelperTest` | date compare/decode/offset/make/truncate |
| `CMiscTest` | string, array, formatting and prefix/suffix helpers |
| `CTimeHelperTest` | time parsing/formatting |
| `CWorkingWeekTest` / `CWorkingDayTest` | working-day/week duration maths |
| `CPreferencesTest` | quoted string round-trip through an isolated temp INI |
| `CRemapMenuIDsTest` | menu-ID remapping for filters, shortcuts and toolbar |
| `CSEARCHPARAMSTest` | search-parameter serialization/matching |
| `CTDCDEPENDENCYTest` | dependency string format/parse |
| `CTDCRECURRENCETest` | regularity/recurrence calculation |
| `CTDCTaskCalculatorTest` | calculated task attributes (dates, priority, risk, cost, times, percent) |
| `CTDCTaskMatcherTest` | filter rule matching, individual and combined |
| `CTDCTaskTimeLogTest` | time-log load/save/format |
| `CToDoCtrlTaskLinkTest` | `tdl://` task link/dependency format and parse |
| `CFileMiscTest` | path split/extension/virtual-store helpers, backup culling |

### 2.2 Component/integration tests (several production classes together, real files)

| Suite | Focus |
|---|---|
| `CTaskFileTest` | `CTaskFile` attribute merge modes and custom comments; `-p` adds save/load benchmarks |
| `CTDCCUSTOMATTRIBUTECALCULATIONTest` | custom-attribute calculation over `CToDoCtrlData` |
| `CToDoCtrlDataTest` | data model creation, undo/save point, recurring-date adjustment, priority/risk/custom-date offsets; `-p` adds benchmarks |
| `CToDoCtrlMgr::CreateBackup` (inside `CTDCFileBackupTest`) | tasklist backup/culling |
| `CWorkingWeek::CalculateDuration` | registered but currently runs no checks |

These write to temporary files or a relative `backup` folder and read them back. They
still run in the test process; nothing is installed, no tasklist is opened by the real
app, and no command is dispatched through `CToDoListWnd`.

### 2.3 Performance/benchmark-only suites (no assertions)

These print timings only when `-p` is passed:

- `CXmlFileTest` (`Core/TDLTest/XmlFileTest.cpp:42-59`)
- `CTDLTaskCtrlBaseTest` (column recalculation, `Core/TDLTest/TDLTaskCtrlBaseTest.cpp:88+`)
- the performance sections of `CTaskFileTest`, `CPreferencesTest` and `CToDoCtrlDataTest`.

They are not pass/fail tests; they are measurements. `-p` also makes the normal run slower
and should be treated separately from the functional suite.

### 2.4 UI/e2e harness

`CUIPerfTest` is different in kind. It is the only test that exercises the real
application:

- launches `ToDoList.exe` with an isolated temp INI and generated `.tdl` fixture
  (`Core/TDLTest/UIPerfTest.cpp:699-713`, `1280-1375`);
- finds the frame/child windows with `EnumWindows`;
- switches views and maximises via `WM_COMMAND`;
- drives the real mouse/keyboard with `SendInput`;
- captures screen pixels with `BitBlt`, forces a clean repaint and compares frames to
  detect stale/intermediate painting;
- checks list-view top-index sync in fast-scroll mode.

It covers two scenarios only: splitter drag and list-scroll sync. It is opt-in (`-ui`),
requires an interactive desktop, and is described in
[customized/CUST-004-ui-perf-harness.md](../customized/CUST-004-ui-perf-harness.md).

Verdict:

- The main `TDLTest` suite is **unit tests plus in-process component/integration tests**,
  with a few benchmark-only suites.
- `CUIPerfTest` is a narrow **end-to-end/UI acceptance and performance harness**.
- There is **no end-to-end functional suite** for open/save/close, persistence, dialogs,
  import/export plugins, reminders or general command handling.
- `CImportExportMgrTest` is a stub: its `Run()` returns an empty result and performs no
  checks (`Core/TDLTest/ImportExportMgrTest.cpp:31-34`).

---

## 3. Test-run limitations

- **No discovery/runner integration.** Adding a suite means editing `DoTests()` and the
  `.vcxproj`; there is no per-test filter except the UI flags (`-ui`, `-uiscroll`,
  `-uisplitter`, `-uifix`).
- **Exit code is always 0.** A failing suite does not fail the process. Debug builds emit
  a CRT assert and the assert hook may swallow it, so CI must parse the printed summary or
  the assert log instead of trusting `%ERRORLEVEL%`.
- **Release cannot assert failures.** `ASSERT` is a no-op in Release; only the printed
  output carries the result.
- **Working-directory and temp-file effects.** Some suites create relative files
  (`TestTasklist_*.tdl`, `backup\`) or use `GetTempFilePath`; they are not hermetic across
  concurrent runs in the same directory.
- **The UI harness is not CI-ready.** It takes over the mouse, needs a foreground desktop,
  can be corrupted by occlusion/always-on-top windows, and intentionally has no CI mode.

---

## 4. Can coverage be calculated?

Locally, yes: `Coverage.ps1` at the repository root builds the tests, runs them under
OpenCppCoverage and renders the report (§4.1). There is still no CI job, gating threshold
or committed baseline. VS 2022 **Community** is installed; Community lacks Visual Studio's
native code-coverage feature, which is why the PDB-based fork below is used.

This works because the in-process suites need no recompilation for coverage: the
`Unicode Debug` build already produces full PDBs for `TDLTest.exe` and `ToDoListLib`
(`TDLTest.vcxproj:139-148`), so a PDB-based collector can attribute executed lines.

Tool options considered:

| Tool | Fit here | Notes |
|---|---|---|
| Microsoft native code coverage / `Microsoft.CodeCoverage.Console` | Best native support, but requires VS 2022 **Enterprise** and `/PROFILE` for the new C++ instrumentation | Command-line collection for console apps is documented, but the tool is not present in the installed Community edition |
| [Fine Code Coverage](https://github.com/FortuneN/FineCodeCoverage) (FCC) | Not usable for `TDLTest` as-is | FCC is a Visual Studio **visualization/orchestration** extension, not a native C++ collector. It reacts to **Test Explorer** and delegates to OpenCover/Coverlet or Microsoft Code Coverage. Its README says C++ is supported through MS Code Coverage but "has not been properly tested with C++ projects" beyond a simple Google Test class, and branch coverage for C++ has been reported as 0. `TDLTest` is a custom console runner with no VS test adapter, so FCC would never start it. Supporting FCC would first require wrapping the harness in a C++ test adapter or converting it to Google Test/Boost.Test. |
| [OpenCppCoverage maintained fork](https://github.com/oussamamk/OpenCppCoverage) (`oussamamk`, v0.9.9.1+) | **Best free ready-to-use fit** | Active continuation of the archived upstream, explicitly presented as such by the fork. Ships GitHub **releases with installers** for x86, x64 and ARM64 (0.9.9.0 and 0.9.9.1). Same CLI, PDB-based, no recompilation, child processes, HTML + Cobertura, coverage aggregation. v0.9.9.1 adds `--branch` branch coverage (Cobertura `branch-rate`/`condition-coverage`, x64/ARM64, debug builds only). Rebuilt on the v143/VS2022 toolset. It is command-line only: there is no Test Explorer/test-adapter integration, which does not matter here because `TDLTest` is a custom console runner. |
| [vcov](https://github.com/wissem01chiha/vcov) (another OpenCppCoverage fork) | Falls back to **source-only** | Same PDB-based model, but no GitHub **releases** or downloadable binaries. Builds from source with CMake 3.20-3.31 + vcpkg (Boost 1.83+, Poco, Protobuf, GTest 1.17+, CTemplate) and the DIA SDK; `vcpkg.json` is a manifest, not a port. Prefer the `oussamamk` releases above. |
| [Archived OpenCppCoverage upstream](https://github.com/OpenCppCoverage/OpenCppCoverage) | Still-available fallback | The archived upstream still has downloadable 0.9.9.x binaries, same CLI and PDB-based model; line coverage only. |
| Commercial native coverage (BullseyeCoverage, CTC++) | Reliable if licensed | Supports MSVC/PDB builds, but adds a paid dependency. |

### 4.1 Running it

```powershell
pwsh -File .\Coverage.ps1                    # build, run, collect, render
pwsh -File .\Coverage.ps1 -NoBuild -Open     # reuse the current Debug build, open report
pwsh -File .\Coverage.ps1 -Branch            # add Cobertura branch data
pwsh -File .\Coverage.ps1 -TargetArgs '-p'   # optional TDLTest flags
```

The script locates the **x86** OpenCppCoverage install (PATH or default install directory)
and ReportGenerator (PATH or the dotnet global-tools folder), and writes the git-ignored
`coverage.xml`, `coverage-html\`, `coverage-report\` and `LastCoverageResults.log`. It
resolves the collector's drive-root source mapping itself, so the rendered report does not
silently drop files.

`-Branch` requires fork 0.9.9.1+; the x86 release notes mark branch runtime validation as
pending, while x64/ARM64 are validated. `-TargetArgs '-ui'` also enables
`--cover_children`, which measures only the startup/splitter/scroll paths the UI harness
drives; instrumentation distorts its timing numbers.

A local Debug run on 2026-11-16 produced **10.9% line coverage (10,246 of 93,783 lines)**,
consistent with the UI-heavy distribution described in §2.

For in-editor highlighting, open the repository in VS Code with the **Coverage Gutters**
extension and set `"coverage-gutters.coverageFileNames": ["coverage.xml"]`. Visual Studio
2022 extensions cannot consume this CLI output: Fine Code Coverage is bound to Test
Explorer. If the project standardises on VS 2022 Enterprise, the native alternative is
`/PROFILE` + `Microsoft.CodeCoverage.Console collect`.

Interpretation notes:

- Treat the result as **line coverage per directory**, not one global percentage across
  `Core/ToDoList` (~133k lines). Most `ToDoListWnd`/`ToDoCtrl` UI, command, dialog and
  plugin code is not reached by the suite, so a low overall number is expected and is not
  by itself a regression.
- Exclude `Core/3rdParty`, `Plugins`, generated resource code and system headers.
- Run performance suites (`-p`) separately from functional coverage; their timings are
  not meaningful under instrumentation.
- Line coverage is the dependable output. Branch coverage is available with `--branch`
  (fork 0.9.9.1+) and exports Cobertura `branch-rate`/`condition-coverage`; it needs a
  debug-style build, does not attribute jump-table `switch` statements, and is only
  validated on x64/ARM64 debuggees. VS Enterprise native coverage is primarily
  line/block based.

---

## 5. Practical next steps

- Give `TDLTest` a non-zero exit code when `res.nNumError` is non-zero, and print a
  machine-readable result line; this makes CI and coverage gating possible without parsing
  human text.
- Add a `-p`/coverage split so the functional run stays fast and deterministic.
- Replace the `CImportExportMgrTest` stub with real import/export tests (plugin discovery
  is exe-directory based, so use temp fixtures).
- Add end-to-end coverage for the state paths documented in
  [APPLICATION_STATE.md](APPLICATION_STATE.md): close tasklist → exit → reopen same path,
  delay-loaded tab close, and FileStates round-trip.
- Record and track a coverage baseline (the current local run is 10.9% lines) and gate on
  it in CI once `TDLTest` returns a meaningful exit code.

---

## 6. Evidence index

| Topic | Source |
|---|---|
| Test project and production-library link | `Core/TDLTest/TDLTest.vcxproj:226-239` |
| Harness and assertion helpers | `Core/TDLTest/TDLTestBase.h:60-362` |
| Suite registration and UI flag | `Core/TDLTest/TDLTest.cpp:175-224` |
| Assert logging and exit code | `Core/TDLTest/TDLTest.cpp:57-120`, `124-171`, `226-230` |
| Debug auto-run post-build | `Core/TDLTest/TDLTest.vcxproj:154-160` |
| No-op import/export suite | `Core/TDLTest/ImportExportMgrTest.cpp:31-34` |
| Performance-only suites | `Core/TDLTest/XmlFileTest.cpp:42-59`, `Core/TDLTest/TDLTaskCtrlBaseTest.cpp:79-90` |
| UI e2e harness | `Core/TDLTest/UIPerfTest.cpp:699-713`, `1013-1020`, `1106-1139`, `1600-1850`; [CUST-004](../customized/CUST-004-ui-perf-harness.md) |
| Debug symbols/PDB | `Core/TDLTest/TDLTest.vcxproj:139-148`, `Core/ToDoListLib/ToDoListLib.vcxproj:67` |
| Test documentation in build instructions | `AGENTS.md` (Build and test), `docs/ARCHITECTURE.md:102` |
