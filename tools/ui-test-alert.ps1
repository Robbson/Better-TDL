<#
.SYNOPSIS
    Play a distinctive audible alert before a mouse-moving UI test.

.DESCRIPTION
    The TDLTest -ui harness moves the real mouse and takes over the desktop, so
    it conflicts with anything the user is doing. Run this alert first and give
    the user a few seconds to release the mouse and keyboard.

    Uses only built-in Windows facilities (System.Media.SoundPlayer and
    [console]::Beep); no external CLI or audio file is required. Falls back to
    beeps if the standard WAV is missing or playback fails.

.PARAMETER Seconds
    Countdown seconds after the alert, before the caller starts the test.

.PARAMETER Sound
    WAV file to play. Defaults to the standard Windows alarm.

.EXAMPLE
    pwsh -File .\tools\ui-test-alert.ps1

.EXAMPLE
    pwsh -File .\tools\ui-test-alert.ps1 -Seconds 8
#>
param(
    [int]$Seconds = 5,
    [string]$Sound
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $Sound) {
    $Sound = Join-Path $env:SystemRoot 'Media\Alarm10.wav'
}

function Play-Wav([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }
    try {
        $player = New-Object System.Media.SoundPlayer $Path
        $player.PlaySync()
        return $true
    } catch {
        return $false
    }
}

function Invoke-Beeps {
    [console]::Beep(880, 150)
    Start-Sleep -Milliseconds 120
    [console]::Beep(1175, 150)
    Start-Sleep -Milliseconds 120
    [console]::Beep(1568, 300)
}

Write-Host "UI TEST ALERT: the mouse will be taken over in $Seconds second(s)." -ForegroundColor Black -BackgroundColor Yellow

if (-not (Play-Wav $Sound)) {
    Invoke-Beeps
}

if ($Seconds -gt 0) {
    for ($i = $Seconds; $i -ge 1; $i--) {
        Write-Host "  starting UI test in $i..." -ForegroundColor Yellow
        Start-Sleep -Seconds 1
    }
}

Write-Host "Starting UI test now." -ForegroundColor Yellow
