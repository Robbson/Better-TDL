<#
.SYNOPSIS
    Collect OpenCppCoverage line coverage for TDLTest and render an HTML report.

.DESCRIPTION
    Builds Core/TDLTest (Unicode Debug|Win32) unless -NoBuild is passed, runs
    TDLTest.exe under OpenCppCoverage, writes coverage.xml and coverage-html\,
    then renders coverage-report\ with ReportGenerator when available.

    Requires the x86 OpenCppCoverage build on PATH or in its default install
    directory (this repo builds Win32 only). ReportGenerator is optional.

    Generated artefacts are git-ignored.

.PARAMETER NoBuild
    Skip the MSBuild step and use the existing TDLTest.exe.

.PARAMETER Branch
    Pass --branch to OpenCppCoverage (Cobertura branch data). Runtime validation
    is documented only for x64/ARM64 debuggees, not the x86 build used here.

.PARAMETER Open
    Open the generated report when the run finishes.

.PARAMETER TargetArgs
    Extra arguments forwarded to TDLTest.exe, for example '-p' or '-ui'.

.EXAMPLE
    pwsh -File .\Coverage.ps1

.EXAMPLE
    pwsh -File .\Coverage.ps1 -NoBuild -Open

.EXAMPLE
    pwsh -File .\Coverage.ps1 -TargetArgs '-p'
#>
[CmdletBinding()]
param(
    [switch]$NoBuild,
    [switch]$Branch,
    [switch]$Open,
    [string[]]$TargetArgs = @(),
    [string]$OpenCppCoveragePath,
    [string]$ReportGeneratorPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = $PSScriptRoot
$exe = Join-Path $repo 'Core\TDLTest\Unicode_Debug\TDLTest.exe'
$coverageXml = Join-Path $repo 'coverage.xml'
$coverageHtml = Join-Path $repo 'coverage-html'
$coverageReport = Join-Path $repo 'coverage-report'

function Find-MSBuild {
    $cmd = Get-Command msbuild.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' 2>$null
        if ($LASTEXITCODE -eq 0 -and $found) { return ($found | Select-Object -First 1) }
    }

    throw 'MSBuild not found. Install Visual Studio 2022 with the C++ workload, or pass -NoBuild.'
}

function Find-OpenCppCoverage {
    if ($OpenCppCoveragePath) {
        if (-not (Test-Path -LiteralPath $OpenCppCoveragePath)) {
            throw "OpenCppCoverage.exe not found at '$OpenCppCoveragePath'."
        }
        return (Resolve-Path -LiteralPath $OpenCppCoveragePath).Path
    }

    $cmd = Get-Command OpenCppCoverage.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        (Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'OpenCppCoverage\OpenCppCoverage.exe'),
        (Join-Path ([Environment]::GetFolderPath('ProgramFiles')) 'OpenCppCoverage\OpenCppCoverage.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\OpenCppCoverage\OpenCppCoverage.exe')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }

    throw 'OpenCppCoverage.exe not found. Install the x86 release from https://github.com/oussamamk/OpenCppCoverage/releases or pass -OpenCppCoveragePath.'
}

function Find-ReportGenerator {
    if ($ReportGeneratorPath) {
        if (-not (Test-Path -LiteralPath $ReportGeneratorPath)) {
            throw "reportgenerator.exe not found at '$ReportGeneratorPath'."
        }
        return (Resolve-Path -LiteralPath $ReportGeneratorPath).Path
    }

    $cmd = Get-Command reportgenerator.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidate = Join-Path $env:USERPROFILE '.dotnet\tools\reportgenerator.exe'
    if (Test-Path -LiteralPath $candidate) { return $candidate }

    return $null
}

Push-Location $repo
try {
    if (-not $NoBuild) {
        $msbuild = Find-MSBuild
        Write-Host '==> Building TDLTest (Unicode Debug|Win32)'
        & $msbuild 'Core\TDLTest\TDLTest.vcxproj' /t:Build '/p:Configuration=Unicode Debug' /p:Platform=Win32 /m /v:minimal
        if ($LASTEXITCODE -ne 0) { throw "TDLTest build failed (exit $LASTEXITCODE)." }
    }

    if (-not (Test-Path -LiteralPath $exe)) {
        throw "TDLTest.exe not found at '$exe'. Build it, or run without -NoBuild."
    }

    $occ = Find-OpenCppCoverage
    if ($occ -like '*\Program Files\OpenCppCoverage\*') {
        Write-Warning 'Using the x64 OpenCppCoverage build; the Win32 test executable needs the x86 release.'
    }

    if (Test-Path -LiteralPath $coverageHtml) { Remove-Item -LiteralPath $coverageHtml -Recurse -Force }
    if (Test-Path -LiteralPath $coverageXml) { Remove-Item -LiteralPath $coverageXml -Force }

    $occArgs = @(
        '--sources', (Join-Path $repo 'Core\ToDoList'),
        '--sources', (Join-Path $repo 'Core\Shared'),
        '--sources', (Join-Path $repo 'Core\Interfaces'),
        '--excluded_sources', (Join-Path $repo 'Core\3rdParty'),
        '--excluded_sources', (Join-Path $repo 'Plugins'),
        '--export_type', "html:$coverageHtml",
        '--export_type', "cobertura:$coverageXml"
    )
    if ($Branch) { $occArgs += '--branch' }
    if ($TargetArgs -contains '-ui') { $occArgs += '--cover_children' }
    $occArgs += '--', $exe
    $occArgs += $TargetArgs

    Write-Host '==> Running TDLTest under OpenCppCoverage'
    & $occ @occArgs
    if ($LASTEXITCODE -ne 0) { throw "OpenCppCoverage failed (exit $LASTEXITCODE)." }
    if (-not (Test-Path -LiteralPath $coverageXml)) { throw 'Coverage XML was not produced.' }

    [xml]$coverage = Get-Content -LiteralPath $coverageXml
    $sourceRoots = @(@($coverage.coverage.sources.source) | Where-Object { $_ })
    if ($sourceRoots.Count -gt 0) {
        $sourceDirs = $sourceRoots -join ';'
    } else {
        $sourceDirs = [IO.Path]::GetPathRoot($repo)
    }

    $reportTool = Find-ReportGenerator
    if (-not $reportTool) {
        Write-Warning 'ReportGenerator not found; skipping coverage-report\ (coverage-html\ is still available).'
        Write-Host '    Install it with: dotnet tool install -g dotnet-reportgenerator-globaltool'
    } else {
        Write-Host '==> Rendering report with ReportGenerator'
        & $reportTool "-reports:$coverageXml" "-targetdir:$coverageReport" '-reporttypes:Html_Dark;HtmlSummary' "-sourcedirs:$sourceDirs" -verbosity:Warning
        if ($LASTEXITCODE -ne 0) { throw "ReportGenerator failed (exit $LASTEXITCODE)." }
    }

    Write-Host ''
    Write-Host 'Coverage artefacts:'
    Write-Host "    XML:    $coverageXml"
    Write-Host "    HTML:   $(Join-Path $coverageHtml 'index.html')"
    if ($reportTool) {
        $reportIndex = Join-Path $coverageReport 'index.html'
        Write-Host "    Report: $reportIndex"
        if ($Open) { Start-Process $reportIndex }
    } elseif ($Open) {
        Start-Process (Join-Path $coverageHtml 'index.html')
    }
}
finally {
    Pop-Location
}
