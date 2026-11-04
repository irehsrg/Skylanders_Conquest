# Runs an Unreal editor Python script headlessly via the pythonscript commandlet and
# echoes the LogPython lines from the log.
# Usage:  .\Scripts\RunEditorPython.ps1 -Script Scripts\CreateHitFlashMaterial.py
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [string]$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)

$ProjectDir = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $ProjectDir "Skylanders_Conquest.uproject"
if (-not (Test-Path $Project)) {
    $found = Get-ChildItem -Path $ProjectDir -Filter *.uproject | Select-Object -First 1
    if ($null -eq $found) { Write-Error "No .uproject in $ProjectDir"; exit 1 }
    $Project = $found.FullName
}

if ([System.IO.Path]::IsPathRooted($Script)) { $ScriptPath = $Script }
else { $ScriptPath = Join-Path $ProjectDir $Script }
if (-not (Test-Path $ScriptPath)) { Write-Error "Script not found: $ScriptPath"; exit 1 }
$ScriptPath = (Resolve-Path $ScriptPath).Path

if (-not (Test-Path $Editor)) { Write-Error "Editor not found: $Editor"; exit 1 }

$LogFile = Join-Path $ProjectDir "Saved\Logs\editorpy.log"
if (Test-Path $LogFile) { Remove-Item $LogFile -Force -Confirm:$false }

$argList = @(
    "`"$Project`"",
    "-run=pythonscript",
    "-script=`"$ScriptPath`"",
    "-unattended", "-nosplash", "-nullrhi", "-NoLiveCoding",
    "-log=editorpy.log"
)
Write-Host "Running: $Editor $($argList -join ' ')"
$proc = Start-Process -FilePath $Editor -ArgumentList $argList -Wait -PassThru -NoNewWindow
$code = $proc.ExitCode
Write-Host "Exit code: $code"

if (Test-Path $LogFile) {
    Get-Content $LogFile | Where-Object { $_ -match "LogPython" } | ForEach-Object { Write-Host $_.Trim() }
} else {
    Write-Warning "Log not found: $LogFile"
}
exit $code
