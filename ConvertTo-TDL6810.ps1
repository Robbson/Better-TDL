#Requires -Version 7.0
<#
.SYNOPSIS
    Converts ToDoList .tdl files from the current format to the 6.8.10 format.

.DESCRIPTION
    Uses yq to rewrite the XML into the vocabulary understood by ToDoList 6.8.10:

      - sets FILEFORMAT to 10 (the 6.8.10 file format)
      - renames the root CUSTOMATTRIBDEF elements back to CUSTOMATTRIB
      - drops custom attribute definitions whose data type is newer than 6.8.10
        understands (types 6..9: file link, time period, fraction, calculation)
      - maps the newer recurrence attribute names back to the 6.8.10 names:
          RECURFREQ      -> REGULARITY
          RECURSPECIFIC1 -> REGPECIFIC1
          RECURSPECIFIC2 -> REGPECIFIC2

    Everything else is left in place. 6.8.10 ignores unknown elements and
    attributes, so newer task fields are simply not shown by the old version.

    The output is written as UTF-16 LE with a BOM, matching what 6.8.10 saves.
    By default each input file produces a sibling '<name>.6.8.10.tdl' file.

.PARAMETER Path
    One or more .tdl files or folders. Folders are searched recursively for
    *.tdl files. Already converted '<name>.6.8.10.tdl' files are skipped.

.PARAMETER OutputFolder
    Optional folder for the converted files. When omitted, each output is
    written next to its input.

.PARAMETER InPlace
    Overwrite the input files. A '<name>.tdl.before-6.8.10.bak' backup is
    created first if it does not already exist.

.EXAMPLE
    .\ConvertTo-TDL6810.ps1 '.\MyTasks.tdl'

.EXAMPLE
    .\ConvertTo-TDL6810.ps1 '.\Tasklists' -OutputFolder '.\Tasklists-6810'

.EXAMPLE
    .\ConvertTo-TDL6810.ps1 '.\MyTasks.tdl' -InPlace
#>
[CmdletBinding()]
param(
	[Parameter(Mandatory, Position = 0, ValueFromRemainingArguments)]
	[string[]]$Path,

	[string]$OutputFolder,

	[switch]$InPlace
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($InPlace -and $OutputFolder)
{
	throw 'Use either -InPlace or -OutputFolder, not both.'
}

$yq = (Get-Command yq -ErrorAction Stop).Source
$yqExpr = @'
.TODOLIST."+@FILEFORMAT" = "10"
| .TODOLIST."+@APPVER" = "6.8.10"
| .TODOLIST |= with_entries(if .key == "CUSTOMATTRIBDEF" then .key = "CUSTOMATTRIB" else . end)
| (.TODOLIST.CUSTOMATTRIB // [] | if type == "!!seq" then . else [.] end) as $defs
| ($defs | map(select((((."+@TYPE" // "0") | tonumber) % 256) <= 5))) as $kept
| .TODOLIST.CUSTOMATTRIB = $kept
| if ($kept | length) == 0 then del(.TODOLIST.CUSTOMATTRIB) else . end
| (.. | select(type == "!!map") | select(has("+@RECURFREQ"))) |= (del(."+@REGULARITY", ."+@REGPECIFIC1", ."+@REGPECIFIC2") | with_entries(if .key == "+@RECURFREQ" then .key = "+@REGULARITY" elif .key == "+@RECURSPECIFIC1" then .key = "+@REGPECIFIC1" elif .key == "+@RECURSPECIFIC2" then .key = "+@REGPECIFIC2" else . end))
'@

function Read-TdlText
{
	param([Parameter(Mandatory)][string]$File)

	$bytes = [System.IO.File]::ReadAllBytes($File)

	if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
	{
		return [System.Text.Encoding]::UTF8.GetString($bytes, 3, $bytes.Length - 3)
	}

	if ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE)
	{
		return [System.Text.Encoding]::Unicode.GetString($bytes, 2, $bytes.Length - 2)
	}

	if ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFE -and $bytes[1] -eq 0xFF)
	{
		return [System.Text.Encoding]::BigEndianUnicode.GetString($bytes, 2, $bytes.Length - 2)
	}

	if ($bytes.Length -ge 4 -and $bytes[0] -eq 0x3C -and $bytes[1] -eq 0x00 -and $bytes[2] -eq 0x3F -and $bytes[3] -eq 0x00)
	{
		return [System.Text.Encoding]::Unicode.GetString($bytes)
	}

	# yq handles UTF-8 only, so normalise any other BOM-less input as UTF-8
	return [System.Text.Encoding]::UTF8.GetString($bytes)
}

function Get-TdlFiles
{
	param([string[]]$InputPath)

	$files = [System.Collections.Generic.List[System.IO.FileInfo]]::new()

	foreach ($item in $InputPath)
	{
		$resolved = Get-Item -LiteralPath $item -ErrorAction Stop

		if ($resolved.PSIsContainer)
		{
			Get-ChildItem -LiteralPath $resolved.FullName -Recurse -File -Filter '*.tdl' |
				ForEach-Object { $files.Add($_) }
		}
		else
		{
			$files.Add($resolved)
		}
	}

	return $files |
		Where-Object { $_.Extension -ieq '.tdl' } |
		Where-Object { $_.Name -notmatch '\.6\.8\.10\.tdl$' } |
		Sort-Object FullName -Unique
}

function Convert-TdlFile
{
	param(
		[Parameter(Mandatory)][System.IO.FileInfo]$File,
		[Parameter(Mandatory)][string]$ExpressionFile,
		[Parameter(Mandatory)][string]$YqPath,
		[Parameter(Mandatory)][string]$Destination
	)

	$tempIn = [System.IO.Path]::GetTempFileName()

	try
	{
		$text = Read-TdlText -File $File.FullName

		if ($text -notmatch '<TODOLIST\b')
		{
			throw 'not a ToDoList XML file'
		}

		# yq inspects the XML declaration, so present the UTF-8 copy as UTF-8
		if ($text -match '^\s*<\?xml\b[^>]*\?>')
		{
			$text = [regex]::Replace($text, '^\s*<\?xml\b[^>]*\?>', '<?xml version="1.0" encoding="utf-8"?>')
		}
		else
		{
			$text = "<?xml version=""1.0"" encoding=""utf-8""?>`r`n$text"
		}

		[System.IO.File]::WriteAllText($tempIn, $text, [System.Text.UTF8Encoding]::new($false))

		$prevOutEncoding = [Console]::OutputEncoding

		try
		{
			[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
			$output = & $YqPath --from-file $ExpressionFile -p xml -o xml $tempIn
			$exitCode = $LASTEXITCODE
		}
		finally
		{
			[Console]::OutputEncoding = $prevOutEncoding
		}

		if ($exitCode -ne 0)
		{
			throw "yq failed with exit code $exitCode"
		}

		$converted = ($output -join "`n")

		if ($converted -notmatch '<TODOLIST\b')
		{
			throw 'yq produced no TODOLIST output'
		}

		$converted = [regex]::Replace($converted, '^\s*<\?xml\b[^>]*\?>', '<?xml version="1.0" encoding="utf-16"?>')

		$destDir = Split-Path -Parent $Destination

		if ($destDir -and -not (Test-Path -LiteralPath $destDir))
		{
			New-Item -ItemType Directory -Path $destDir -Force | Out-Null
		}

		[System.IO.File]::WriteAllText($Destination, $converted, [System.Text.Encoding]::Unicode)
	}
	finally
	{
		Remove-Item -LiteralPath $tempIn -Force -ErrorAction SilentlyContinue
	}
}

$files = @(Get-TdlFiles -InputPath $Path)

if ($files.Count -eq 0)
{
	Write-Warning 'No .tdl files found.'
	return
}

$tempExpr = [System.IO.Path]::GetTempFileName()
$succeeded = 0
$failed = 0

try
{
	[System.IO.File]::WriteAllText($tempExpr, $yqExpr, [System.Text.UTF8Encoding]::new($false))

	foreach ($file in $files)
	{
		if ($InPlace)
		{
			$backup = "$($file.FullName).before-6.8.10.bak"

			if (-not (Test-Path -LiteralPath $backup))
			{
				Copy-Item -LiteralPath $file.FullName -Destination $backup -Force
			}

			$destination = $file.FullName
		}
		else
		{
			$outDir = if ($OutputFolder) { $OutputFolder } else { $file.DirectoryName }
			$destination = Join-Path $outDir ($file.BaseName + '.6.8.10.tdl')
		}

		try
		{
			Convert-TdlFile -File $file -ExpressionFile $tempExpr -YqPath $yq -Destination $destination
			Write-Host "Converted: $($file.FullName) -> $destination"
			$succeeded++
		}
		catch
		{
			Write-Error "Failed: $($file.FullName): $($_.Exception.Message)" -ErrorAction Continue
			$failed++
		}
	}
}
finally
{
	Remove-Item -LiteralPath $tempExpr -Force -ErrorAction SilentlyContinue
}

Write-Host "Done: $succeeded converted, $failed failed."

if ($failed -gt 0)
{
	exit 1
}
