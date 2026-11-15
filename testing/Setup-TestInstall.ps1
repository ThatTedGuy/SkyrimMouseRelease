# Builds a vanilla Skyrim test install for one game version, separate from the main (modded) install.
# Usage: ./testing/Setup-TestInstall.ps1 -Version 1.5.97     (or 1.7.104)
# Run it once to get the Steam console commands, again after the downloads finish to move the files
# into place, and again any time to check what the install is still missing. See testing/README.md.
param(
    [Parameter(Mandatory)][ValidateSet('1.5.97', '1.7.104')][string]$Version,
    [string]$Root = 'C:\SkyrimTest'
)
$ErrorActionPreference = 'Stop'

# Steam depots of Skyrim Special Edition (app 489830): 489831 and 489832 hold the game data,
# 489833 the executable. 1.7.104 IDs are the ones the main install had on 2026-11-15
# (steamapps/appmanifest_489830.acf); 1.5.97 IDs are the community-published ones.
$manifests = @{
    '1.5.97'  = [ordered]@{ '489831' = '7848722008564294070'; '489832' = '8702665189575304780'; '489833' = '2289561010626853674' }
    '1.7.104' = [ordered]@{ '489831' = '4940892828028256588'; '489832' = '5728778377666085157'; '489833' = '4886117324142477814' }
}[$Version]

$steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
if (-not $steam) { $steam = "${env:ProgramFiles(x86)}\Steam" }
$content = Join-Path $steam 'steamapps\content\app_489830'
$target = Join-Path $Root $Version
$exe = Join-Path $target 'SkyrimSE.exe'

function Get-ExeVersion([string]$Path) {
    if (-not (Test-Path $Path)) { return $null }
    # The version text, not the numeric fields: the 1.5.97 exe's numeric fields say 1.0.0.
    if ((Get-Item $Path).VersionInfo.FileVersion -match '^(\d+)\.(\d+)\.(\d+)') {
        return '{0}.{1}.{2}' -f $Matches[1], $Matches[2], $Matches[3]
    }
    return $null
}

# 1. Game files: move the finished depot downloads into the test folder.
if ((Get-ExeVersion $exe) -ne $Version) {
    $missing = @($manifests.Keys | Where-Object { -not (Test-Path (Join-Path $content "depot_$_")) })
    if ($missing.Count -gt 0) {
        Write-Host "Game files for $Version are not downloaded yet. In the Steam console that just opened, run:"
        foreach ($depot in $manifests.Keys) { Write-Host "  download_depot 489830 $depot $($manifests[$depot])" }
        Write-Host "Each one prints 'Depot download complete' with a folder when done (about 15 GB in all)."
        Write-Host "Download one game version at a time: both use the same folders under $content."
        Write-Host "Then run this script again."
        Start-Process 'steam://open/console'
        exit 1
    }
    $downloaded = Get-ExeVersion (Join-Path $content 'depot_489833\SkyrimSE.exe')
    if ($downloaded -ne $Version) {
        throw "The downloaded SkyrimSE.exe is $downloaded, not $Version. Delete $content and download again."
    }
    New-Item -ItemType Directory -Force $target | Out-Null
    foreach ($depot in $manifests.Keys) {
        Write-Host "Moving depot $depot into $target"
        robocopy (Join-Path $content "depot_$depot") $target /E /MOVE /NFL /NDL /NJH /NJS /NP | Out-Null
        if ($LASTEXITCODE -ge 8) { throw "robocopy failed on depot $depot (exit $LASTEXITCODE)" }
    }
    if ((Get-ExeVersion $exe) -ne $Version) { throw "SkyrimSE.exe in $target is not $Version after the move" }
}
# Lets the game start from this folder without Steam relaunching the main install instead.
Set-Content -Path (Join-Path $target 'steam_appid.txt') -Value '489830' -NoNewline
Write-Host "Game $Version : OK ($target)"

# 2. SKSE and Address Library: downloaded by hand, checked here.
$underscored = $Version -replace '\.', '_'
$dashed = $Version -replace '\.', '-'
$ok = $true
if (-not (Test-Path (Join-Path $target "skse64_$underscored.dll")) -or -not (Test-Path (Join-Path $target 'skse64_loader.exe'))) {
    $ok = $false
    $which = if ($Version -eq '1.5.97') { 'SKSE 2.0.20 (the build for 1.5.97)' } else { "the SKSE build for $Version" }
    Write-Host "SKSE: missing. Get $which from https://skse.silverlock.org/ and copy everything inside its skse64_... folder into $target (the src folder is not needed)"
}
else { Write-Host 'SKSE: OK' }

$plugins = Join-Path $target 'Data\SKSE\Plugins'
if (-not (Get-ChildItem $plugins -Filter "*-$dashed-0.bin" -ErrorAction SilentlyContinue)) {
    $ok = $false
    $file = 'Address Library All in One (1.7.104.0)'
    Write-Host "Address Library: missing. Get '$file' from https://www.nexusmods.com/skyrimspecialedition/mods/32444?tab=files"
    Write-Host "  and put its .bin file for $Version into $plugins"
}
else { Write-Host 'Address Library: OK' }

if ($ok) { Write-Host "Ready. Run: ./testing/Run-Test.ps1 -Version $Version"; exit 0 } else { exit 1 }
