#Requires -Version 7.0
<#
.SYNOPSIS
    PERF-006 step 3: profile one Window-menu activation of a delay-loaded tasklist.

.DESCRIPTION
    Captures a single tasklist activation with Windows Performance Recorder (CPU
    sampling + context switches) and produces an ETL plus a frame-resolved summary.

    Measurement discipline (see customized/performance/PERF-006-...md):
      - the app is started WITHOUT -g: logging is line buffered through the file system
        and inflates the activation by hundreds of ms, more in an elevated process on a
        slower disk;
      - the profiled instance runs on an isolated ini outside the repo, with only the
        three session documents and without stale per-file UI state (FileStates), so the
        numbers are comparable between runs;
      - the instance is brought to the foreground before the activation, because a
        background window measures roughly 30% slower;
      - WPR is started immediately before the activation and stopped immediately after
        it, the app is closed right away, so neither the app nor the recorder stays open;
      - only processes this script starts are stopped; other ToDoList instances
        (e.g. the 6.8.10 notes instance) are never touched.

    Requires elevation: unelevated WPR fails with 0xc5585011 "Failed to enable the
    policy to profile system performance", so the script relaunches itself via UAC.

.PARAMETER Label
    Output folder suffix, default a timestamp. Output:
    %TEMP%\tdl-perf006-<Label>\.

.PARAMETER DocumentIndex
    0-based position in the session: 0 = biglist.tdl (large, default),
    1 = Evidence Board.tdl (small).

.PARAMETER UseLogging
    Also start ToDoList with -g. Only for debugging: it adds a log write per scope.

.PARAMETER KeepEtl
    Keep the (large) ETL; by default it is deleted after the CSV export.

.EXAMPLE
    tools\perf006-activation-profile.ps1
.EXAMPLE
    tools\perf006-activation-profile.ps1 -DocumentIndex 1 -Label small
#>
[CmdletBinding()]
param(
    [string]$Label = '',
    [ValidateRange(0, 1)][int]$DocumentIndex = 0,
    [switch]$UseLogging,
    [switch]$KeepEtl
)

$ErrorActionPreference = 'Stop'

$repo     = Split-Path -Parent $PSScriptRoot
$exe      = Join-Path $repo 'Core\ToDoList\Unicode_Release\ToDoList.exe'
$srcIni   = Join-Path $repo 'Core\ToDoList\Unicode_Release\ToDoList.ini'
$fixtures = Join-Path $repo 'Core\ToDoList\Unicode_Release\Resources\Examples'
$wpt      = 'C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit'
$wpr      = Join-Path $wpt 'wpr.exe'
$xperf    = Join-Path $wpt 'xperf.exe'

# ---------------------------------------------------------------- elevation ----
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin)
{
    if (-not $Label) { $Label = 'perf006-' + (Get-Date -Format 'HHmmss') }
    $work = Join-Path ([IO.Path]::GetTempPath()) ('tdl-' + $Label)
    New-Item -ItemType Directory -Path $work -Force | Out-Null
    $outerLog = Join-Path $work 'run.log'

    $inner = '& "' + $PSCommandPath + '" -Label "' + $Label + '" -DocumentIndex ' + $DocumentIndex +
             $(if ($UseLogging) { ' -UseLogging' } else { '' }) +
             $(if ($KeepEtl) { ' -KeepEtl' } else { '' })
    $cmd = 'try { ' + $inner + ' *> "' + $outerLog + '" } catch { ''FAILED: '' + ($_ | Out-String) *> "' + $outerLog + '" }'

    Write-Host 'WPR needs elevation; relaunching elevated (one UAC prompt)...'
    $enc = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($cmd))
    $p = Start-Process -FilePath 'pwsh.exe' -ArgumentList @('-NoProfile', '-EncodedCommand', $enc) -Verb RunAs -PassThru -Wait

    if (Test-Path $outerLog) { Get-Content $outerLog | ForEach-Object { Write-Host $_ } }
    Write-Host ('Output folder: ' + $work)
    exit $p.ExitCode
}

if (-not $Label) { $Label = 'perf006-' + (Get-Date -Format 'HHmmss') }
$work = Join-Path ([IO.Path]::GetTempPath()) ('tdl-' + $Label)
New-Item -ItemType Directory -Path $work -Force | Out-Null

# ---------------------------------------------------------------- preflight ----
foreach ($tool in @($exe, $srcIni, $wpr, $xperf))
{
    if (-not (Test-Path $tool)) { throw "missing: $tool (build Unicode Release and install the Windows Performance Toolkit)" }
}

$docs = @(
    (Join-Path $fixtures 'biglist.tdl'),
    (Join-Path $fixtures 'Evidence Board.tdl'),
    (Join-Path $fixtures 'Dancing.tdl')
)
foreach ($doc in $docs) { if (-not (Test-Path $doc)) { throw "missing fixture: $doc" } }

$wprStatus = (& $wpr -status) -join "`n"
if ($wprStatus -notmatch 'not recording')
{
    # an interrupted earlier run leaves the recorder behind
    Write-Host 'WPR was still recording (leftover from an interrupted run); stopping it...' -ForegroundColor Yellow
    & $wpr -cancel | Out-Null
    $wprStatus = (& $wpr -status) -join "`n"
    if ($wprStatus -notmatch 'not recording') { throw "WPR is recording and cannot be stopped.`n$wprStatus" }
}

# a leftover test instance would hold the exe; close only the ones running this binary
foreach ($q in @(Get-Process ToDoList -ErrorAction SilentlyContinue | Where-Object {
            try { $_.Path -eq $exe } catch { $false } }))
{
    Write-Host ('closing leftover test instance pid ' + $q.Id)
    try { Stop-Process -Id $q.Id -Force } catch {}
}
Start-Sleep -Milliseconds 500

# ---------------------------------------------------------------- isolated ini
$ini = Join-Path $work 'ToDoList.ini'
$lines = Get-Content $srcIni
$session = [ordered]@{
    LastActiveFile = '"' + $docs[2] + '"'
    LastFile0      = '"' + $docs[0] + '"'
    LastFile1      = '"' + $docs[1] + '"'
    LastFile2      = '"' + $docs[2] + '"'
    NumLastFiles   = '3'
}
foreach ($key in $session.Keys)
{
    $pattern = '^' + [regex]::Escape($key) + '=.*$'
    if ($lines -match $pattern) { $lines = $lines -replace $pattern, ($key + '=' + $session[$key]) }
    else { $lines += ($key + '=' + $session[$key]) }
}
# only the three documents above may be restored
$lines = $lines | Where-Object { $_ -notmatch '^LastFile([3-9]|[1-9][0-9])\s*=' }
# stale per-file UI state would dominate the activation; use ToDoList's defaults
$lines = $lines | Where-Object { $_ -notmatch '^\[FileStates\\' }
Set-Content -LiteralPath $ini -Value $lines -Encoding Unicode

# ---------------------------------------------------------------- win32 glue --
if (-not ('TdlPerf006' -as [type]))
{
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class TdlPerf006
{
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")]
    public static extern bool BringWindowToTop(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int cmd);
    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("kernel32.dll")]
    public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")]
    public static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll")]
    public static extern bool SetFocus(IntPtr hWnd);
    public const uint WM_COMMAND = 0x0111;

    public static bool ForceForeground(IntPtr hwnd)
    {
        ShowWindow(hwnd, 9);
        SetWindowPos(hwnd, IntPtr.Zero, 0, 0, 0, 0, 0x0001 | 0x0002 | 0x0040);
        uint fpid;
        uint fthread = GetWindowThreadProcessId(GetForegroundWindow(), out fpid);
        uint mythread = GetCurrentThreadId();
        AttachThreadInput(mythread, fthread, true);
        try
        {
            BringWindowToTop(hwnd);
            SetForegroundWindow(hwnd);
            SetFocus(hwnd);
        }
        finally { AttachThreadInput(mythread, fthread, false); }
        return GetForegroundWindow() == hwnd;
    }
}
"@
}

function Close-TestApp($proc)
{
    if (-not $proc) { return }
    $p = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
    if (-not $p) { return }
    $p.CloseMainWindow() | Out-Null
    if (-not $p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force }
    Write-Host ('closed test ToDoList pid ' + $p.Id)
}

# ---------------------------------------------------------------- profile -----
$etl = Join-Path $work ($Label + '.etl')
$csv = Join-Path $work ($Label + '.csv')
$cmd = 33006 + $DocumentIndex
$docName = Split-Path $docs[$DocumentIndex] -Leaf

$argList = @('-i', ('"' + $ini + '"'))
if ($UseLogging) { $argList = @('-g') + $argList }

$app = Start-Process -FilePath $exe -ArgumentList ($argList -join ' ') -WorkingDirectory (Split-Path $exe) -PassThru
Write-Host ('ToDoList pid: ' + $app.Id + (if ($UseLogging) { ' (with -g)' } else { '' }))

try
{
    $deadline = (Get-Date).AddSeconds(60)
    $hwnd = [IntPtr]::Zero
    while ((Get-Date) -lt $deadline)
    {
        $cur = Get-Process -Id $app.Id -ErrorAction SilentlyContinue
        if (-not $cur) { throw 'ToDoList exited during startup' }
        if ($cur.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $cur.MainWindowHandle; break }
        Start-Sleep -Milliseconds 100
    }
    if ($hwnd -eq [IntPtr]::Zero) { throw 'ToDoList main window did not appear' }

    Start-Sleep -Seconds 8      # active document loads, delayed tabs stay unloaded
    $fg = [TdlPerf006]::ForceForeground($hwnd)
    Write-Host ('foreground: ' + $fg)
    Start-Sleep -Seconds 1

    $sw = [Diagnostics.Stopwatch]::StartNew()
    & $wpr -start CPU -filemode | Out-Null
    if ($LASTEXITCODE -ne 0) { throw ('wpr -start failed with ' + $LASTEXITCODE) }
    try
    {
        Start-Sleep -Seconds 2
        Write-Host ('activating ' + $docName + ' (WM_COMMAND ' + $cmd + ')')
        [TdlPerf006]::PostMessage($hwnd, [TdlPerf006]::WM_COMMAND, [IntPtr]$cmd, [IntPtr]::Zero) | Out-Null
        $activationStart = $sw.Elapsed.TotalMilliseconds
        Start-Sleep -Seconds 8  # one activation is well under 10 s
    }
    finally
    {
        & $wpr -stop $etl | Out-Null
        Write-Host ('profiled ' + [int]$sw.Elapsed.TotalMilliseconds + ' ms window')
    }
}
finally
{
    Close-TestApp $app
}

if (-not (Test-Path $etl)) { throw ('ETL not produced: ' + $etl) }

# ---------------------------------------------------------------- export ------
Write-Host 'exporting CPU samples (xperf -a dumper)...'
& $xperf -i $etl -o $csv -symbols -a dumper 2>$null | Out-Null

if (Test-Path $csv)
{
    Write-Host ('csv: ' + $csv)
}
else
{
    Write-Host 'xperf export failed; open the ETL in WPA instead' -ForegroundColor Yellow
}

if (-not $KeepEtl -and (Test-Path $etl)) { Remove-Item $etl -Force }
elseif (Test-Path $etl) { Write-Host ('etl: ' + $etl) }

Write-Host ''
Write-Host ('Output folder: ' + $work)
