# Lists the RomFS `archives/` folder from the Switch dump, so we can pick which
# paks to pull instead of dumping blind.
#
# Two ways in, tried in order:
#
#   1. SD CARD IN A READER  - the dump lands in \nxdt_rw_poc\ (pre-release path,
#      NOT \switch\nxdumptool\). Just a normal directory listing.
#   2. MTP over DBI         - run DBI on the Switch and it appears in This PC as
#      a portable device. PowerShell's Get-PSDrive will NOT see it; MTP devices
#      are only reachable through the Shell COM namespace (17 == My Computer).
#
#   powershell -ExecutionPolicy Bypass -File Scripts\ListSwitchArchives.ps1
#
# Writes Saved\SwitchArchives.txt -- send that back and it becomes a shopping list.

$ErrorActionPreference = 'Continue'
$out = Join-Path (Split-Path -Parent $PSScriptRoot) 'Saved\SwitchArchives.txt'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $out) | Out-Null

function Save-Listing($names, $how) {
    $names | Sort-Object -Unique | Set-Content -Path $out -Encoding utf8
    Write-Host ""
    Write-Host "$($names.Count) entries via $how"
    Write-Host "-> $out"
}

# ---- 1. SD card mounted as a drive letter -------------------------------
foreach ($d in (Get-PSDrive -PSProvider FileSystem)) {
    foreach ($rel in @('nxdt_rw_poc', 'switch\nxdumptool', '')) {
        $probe = if ($rel) { Join-Path $d.Root $rel } else { $d.Root }
        if (-not (Test-Path $probe)) { continue }
        $arch = Get-ChildItem -Path $probe -Recurse -Directory -Filter 'archives' `
                              -Depth 6 -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($arch) {
            Write-Host "found on disk: $($arch.FullName)"
            $names = Get-ChildItem -Path $arch.FullName -File -ErrorAction SilentlyContinue |
                     ForEach-Object { '{0}`t{1}' -f $_.Name, $_.Length }
            Save-Listing $names "SD card at $($arch.FullName)"
            exit 0
        }
    }
}

# ---- 2. MTP portable device (DBI) ---------------------------------------
Write-Host "no SD card path found; looking for an MTP device..."
$shell = New-Object -ComObject Shell.Application
$myComputer = $shell.NameSpace(17)
if (-not $myComputer) { Write-Host "could not open the shell namespace"; exit 1 }

$devices = @($myComputer.Items() | Where-Object { $_.IsFolder -and $_.Path -notmatch '^[A-Z]:' })
if (-not $devices) {
    Write-Host ""
    Write-Host "No portable device found. Either put the SD card in a reader, or"
    Write-Host "start DBI on the Switch and confirm it shows up under This PC."
    exit 1
}

# The dump sits 8 levels down, deeper than a casual recursive search will reach:
#   SD Card\nxdt_rw_poc\NCA FS\User\Extracted\<title>\Program #0\1\archives
# Walk it directly, matching the two variable segments by pattern.
function Get-ByName($folder, $name) {
    return (@($folder.Items()) | Where-Object { $_.Name -eq $name } | Select-Object -First 1)
}

function Find-Archives($device) {
    $f = @($device.Items()) | Where-Object { $_.Name -like '*SD Card*' } | Select-Object -First 1
    if (-not $f) { return $null }
    $f = $f.GetFolder
    foreach ($seg in @('nxdt_rw_poc', 'NCA FS', 'User', 'Extracted')) {
        $next = Get-ByName $f $seg
        if (-not $next) { Write-Host "  missing segment: $seg"; return $null }
        $f = $next.GetFolder
    }
    # <title> and the NCA index are named per-dump, so take the first of each.
    $title = @($f.Items()) | Select-Object -First 1
    if (-not $title) { return $null }
    Write-Host "  title: $($title.Name)"
    $prog = @($title.GetFolder.Items()) | Where-Object { $_.Name -like 'Program*' } | Select-Object -First 1
    if (-not $prog) { return $null }
    $idx = @($prog.GetFolder.Items()) | Select-Object -First 1
    if (-not $idx) { return $null }
    $arch = Get-ByName $idx.GetFolder 'archives'
    if (-not $arch) { return $null }
    return $arch.GetFolder
}

foreach ($dev in $devices) {
    Write-Host "  scanning device: $($dev.Name)"
    $found = Find-Archives $dev.GetFolder
    if ($found) {
        $names = @($found.Items() | ForEach-Object { $_.Name })
        Save-Listing $names "MTP device $($dev.Name)"
        exit 0
    }
}

Write-Host "Found a device but no 'archives' folder inside it."
exit 1
