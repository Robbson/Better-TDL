#Requires -Version 7.0
<#
.SYNOPSIS
    Sample the stacks of a running ToDoList activation and resolve them via ToDoList.map.

.DESCRIPTION
    WPR/xperf need elevation; on machines where only an unelevated session is available
    this sampler answers the same question cheaply. It suspend-samples every thread of
    the target process twice per second, walks each stack (frame-pointer walk), resolves
    the addresses against the linker map of the matching binary and prints the app-side
    frames of the busiest threads.

    Nothing is started or stopped by this script: it attaches to an already running
    ToDoList and only reads its memory (PROCESS_VM_READ | QUERY_INFORMATION).

    Requires:
      - ToDoList built with /MAP (Unicode_Release\ToDoList.map; the Visual Studio project
        already emits it),
      - the x86 dbghelp.dll is NOT needed; resolution uses the map file only.

.PARAMETER Seconds
    Sampling duration.

.PARAMETER ProcessPath
    Substring used to pick the ToDoList instance (default Unicode_Release).

.PARAMETER Top
    Number of threads to print (by CPU time).

.EXAMPLE
    # start ToDoList on a test document, pick a delayed tab from the Window menu, then:
    tools\perf006-stack-sample.ps1 -Seconds 10
#>
[CmdletBinding()]
param(
    [int]$Seconds = 10,
    [string]$ProcessPath = 'Unicode_Release',
    [int]$Top = 3
)

$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$map = Join-Path $repo 'Core\ToDoList\Unicode_Release\ToDoList.map'
if (-not (Test-Path $map)) { throw "missing map file: $map (link with /MAP)" }

if (-not ('TdlSampler' -as [type]))
{
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class TdlSampler
{
    public const uint THREAD_SUSPEND_RESUME = 0x0002;
    public const uint THREAD_GET_CONTEXT = 0x0008;
    public const uint THREAD_QUERY_INFORMATION = 0x0040;
    public const uint PROCESS_QUERY_INFORMATION = 0x0400;
    public const uint PROCESS_VM_READ = 0x0010;
    public const uint CONTEXT_i386 = 0x00010000;
    public const uint CONTEXT_CONTROL = CONTEXT_i386 | 0x0001;
    public const uint CONTEXT_INTEGER = CONTEXT_i386 | 0x0002;
    public const uint CONTEXT_SEGMENTS = CONTEXT_i386 | 0x0004;
    public const uint CONTEXT_CONTROL_INTEGER = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    [StructLayout(LayoutKind.Sequential)]
    struct FLOATSAVE
    {
        public uint ControlWord, StatusWord, TagWord, ErrorOffset, ErrorSelector, DataOffset, DataSelector;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 80)] public byte[] RegisterArea;
        public uint Cr0NpxState;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct CONTEXT
    {
        public uint ContextFlags, Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
        public FLOATSAVE FloatSave;
        public uint SegGs, SegFs, SegEs, SegDs, Edi, Esi, Ebx, Edx, Ecx, Eax, Ebp, Eip, SegCs, EFlags, Esp, SegSs;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 512)] public byte[] ExtendedRegisters;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr OpenThread(uint access, bool inherit, uint tid);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern uint SuspendThread(IntPtr h);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern int ResumeThread(IntPtr h);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool GetThreadContext(IntPtr h, IntPtr ctx);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, IntPtr buf, IntPtr size, out IntPtr read);
    [DllImport("kernel32.dll")]
    static extern bool CloseHandle(IntPtr h);

    static IntPtr gProc = IntPtr.Zero;

    public static bool Open(uint pid)
    {
        gProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid);
        return gProc != IntPtr.Zero;
    }

    public static void Close()
    {
        if (gProc != IntPtr.Zero) { CloseHandle(gProc); gProc = IntPtr.Zero; }
    }

    static uint Read32(uint addr)
    {
        IntPtr buf = Marshal.AllocHGlobal(4);
        try
        {
            IntPtr read;
            if (!ReadProcessMemory(gProc, (IntPtr)(long)addr, buf, (IntPtr)4, out read)) return 0;
            return (uint)Marshal.ReadInt32(buf);
        }
        finally { Marshal.FreeHGlobal(buf); }
    }

    // frame-pointer walk: [ebp] = saved ebp, [ebp+4] = return address
    public static string WalkThread(uint tid)
    {
        IntPtr hT = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, false, tid);
        if (hT == IntPtr.Zero) return null;
        try
        {
            if (SuspendThread(hT) == 0xFFFFFFFF) return null;
            IntPtr ctx = Marshal.AllocHGlobal(716);
            try
            {
                for (int b = 0; b < 716; b++) Marshal.WriteByte(ctx, b, 0);
                Marshal.WriteInt32(ctx, 0, (int)CONTEXT_CONTROL_INTEGER);
                if (!GetThreadContext(hT, ctx)) return null;
                uint ebp = (uint)Marshal.ReadInt32(ctx, 180);
                uint esp = (uint)Marshal.ReadInt32(ctx, 196);
                uint eip = (uint)Marshal.ReadInt32(ctx, 184);
                IntPtr ret = Marshal.AllocHGlobal(4);
                try
                {
                    IntPtr read;
                    ReadProcessMemory(gProc, (IntPtr)(long)eip, ret, (IntPtr)4, out read);
                }
                finally { Marshal.FreeHGlobal(ret); }

                StringBuilder sb = new StringBuilder();
                sb.AppendFormat("0x{0:X8}", eip);
                for (int i = 0; i < 32 && ebp > esp && ebp < 0x7FF00000; i++)
                {
                    uint retAddr = Read32(ebp + 4);
                    uint next = Read32(ebp);
                    if (retAddr == 0) break;
                    sb.Append(' ').AppendFormat("0x{0:X8}", retAddr);
                    if (next <= ebp) break;
                    ebp = next;
                }
                return sb.ToString();
            }
            finally
            {
                Marshal.FreeHGlobal(ctx);
                ResumeThread(hT);
                ResumeThread(hT);
            }
        }
        finally { CloseHandle(hT); }
    }
}
"@
}

function Read-MapSymbols([string]$path)
{
    $rows = New-Object System.Collections.Generic.List[object]
    foreach ($line in [IO.File]::ReadLines($path))
    {
        if ($line -match '^ 0001:[0-9a-f]{8}\s+(\S+)\s+(0[0-9a-f]{7})\s')
        {
            $rows.Add([pscustomobject]@{ addr = [Convert]::ToUInt64($Matches[2], 16); name = $Matches[1] })
        }
    }
    $rows.Sort([System.Collections.Generic.Comparer[object]]::Create({ param($a, $b) $a.addr.CompareTo($b.addr) }))
    return ,$rows
}

function Resolve-Address([uint64]$value, $symbols)
{
    $lo = 0; $hi = $symbols.Count - 1; $best = -1
    while ($lo -le $hi)
    {
        $mid = [int](($lo + $hi) / 2)
        if ($symbols[$mid].addr -le $value) { $best = $mid; $lo = $mid + 1 } else { $hi = $mid - 1 }
    }
    if ($best -lt 0) { return $null }
    $sym = $symbols[$best]
    # addresses that fall into an unrelated module (MFC, user32, ...) are not ours
    if (($value - $sym.addr) -gt 0x20000) { return $null }
    return ('{0}+0x{1:X}' -f $sym.name, ($value - $sym.addr))
}

$target = $null
foreach ($p in (Get-Process ToDoList -ErrorAction SilentlyContinue))
{
    try { if ($p.Path -like ('*' + $ProcessPath + '*')) { $target = $p; break } } catch {}
}
if (-not $target) { throw ("no ToDoList running from a path containing '" + $ProcessPath + "'") }

Write-Host ('sampling pid ' + $target.Id + ' (' + $target.Path + ') for ' + $Seconds + ' s')
if (-not [TdlSampler]::Open([uint32]$target.Id)) { throw 'OpenProcess failed (run elevated?)' }

$symbols = Read-MapSymbols $map
Write-Host ('map symbols: ' + $symbols.Count)

$samples = New-Object System.Collections.Generic.List[string]
try
{
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline)
    {
        $proc = Get-Process -Id $target.Id -ErrorAction SilentlyContinue
        if (-not $proc) { break }
        foreach ($t in $proc.Threads)
        {
            $chain = [TdlSampler]::WalkThread([uint32]$t.Id)
            if ($chain) { $samples.Add($chain) }
        }
        Start-Sleep -Milliseconds 50
    }
}
finally
{
    [TdlSampler]::Close()
}

$appFrames = foreach ($chain in $samples)
{
    $resolved = foreach ($a in ($chain -split ' '))
    {
        $r = Resolve-Address ([Convert]::ToUInt64($a.Replace('0x', ''), 16)) $symbols
        if ($null -ne $r) { $r }
    }
    if ($resolved) { $resolved -join ' < ' }
}

Write-Host ''
Write-Host ('samples: ' + $samples.Count + ', with app frames: ' + @($appFrames).Count)
Write-Host ''
Write-Host 'Most frequent app-side stack prefixes:'
$appFrames | Group-Object | Sort-Object Count -Descending | Select-Object -First $Top | ForEach-Object {
    Write-Host ('  x' + $_.Count + '  ' + $_.Name)
}
