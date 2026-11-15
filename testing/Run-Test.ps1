# Runs one test session in a vanilla test install without touching the main (modded) setup.
# Usage: ./testing/Run-Test.ps1 -Version 1.5.97     (or 1.7.104)
#        ./testing/Run-Test.ps1 -RestoreOnly         (if a run was interrupted)
#
# Every Skyrim SE/AE install shares two profile folders: Documents\My Games\Skyrim Special Edition
# (INIs, saves, SKSE logs) and AppData\Local\Skyrim Special Edition (Plugins.txt, loadorder.txt).
# This script renames both out of the way, lets the test game make fresh ones, and when the game
# exits, moves the test ones to <Root>\runs\ and renames the originals back. Nothing is copied or
# deleted. See testing/README.md.
param(
    [ValidateSet('1.5.97', '1.7.104')][string]$Version,
    [string]$Root = 'C:\SkyrimTest',
    [switch]$RestoreOnly,
    [switch]$VerboseLog,  # bVerboseLog=1 for this run
    [switch]$PluginOff    # bReleaseMouse=0 for this run, to compare with the plugin's behavior
)
$ErrorActionPreference = 'Stop'

$profileName = 'Skyrim Special Edition'
$holdName = "$profileName.MouseReleaseHold"
$profileBases = [ordered]@{
    'My Games' = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games'
    'AppData'  = $env:LOCALAPPDATA
}

function Restore-Profiles([string]$RunDir) {
    foreach ($label in $profileBases.Keys) {
        $base = $profileBases[$label]
        $live = Join-Path $base $profileName
        $held = Join-Path $base $holdName
        if (-not (Test-Path $held)) { continue }
        if (Test-Path $live) {
            New-Item -ItemType Directory -Force $RunDir | Out-Null
            Move-Item $live (Join-Path $RunDir $label)
        }
        Rename-Item $held $profileName
        Write-Host "Restored $live"
    }
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if ($RestoreOnly) {
    Restore-Profiles (Join-Path $Root "runs\interrupted-$stamp")
    exit 0
}
if (-not $Version) { throw 'Give -Version 1.5.97 or -Version 1.7.104 (or -RestoreOnly)' }

$install = Join-Path $Root $Version
$repo = Split-Path $PSScriptRoot -Parent
$runDir = Join-Path $Root "runs\$Version-$stamp"

# Checks before touching anything.
if (Get-Process SkyrimSE -ErrorAction SilentlyContinue) { throw 'Skyrim is running. Close it first.' }
if (Get-Process Vortex -ErrorAction SilentlyContinue) { throw 'Vortex is running. Close it first: it rewrites Plugins.txt when it sees changes.' }
foreach ($base in $profileBases.Values) {
    if (Test-Path (Join-Path $base $holdName)) { throw "A previous run wasn't restored. Run: ./testing/Run-Test.ps1 -RestoreOnly" }
}
& (Join-Path $PSScriptRoot 'Setup-TestInstall.ps1') -Version $Version -Root $Root | Out-Null
if ($LASTEXITCODE) { throw "The $Version install isn't ready. Run: ./testing/Setup-TestInstall.ps1 -Version $Version" }
$built = Join-Path $repo 'dist\SKSE\Plugins'
if (-not (Test-Path (Join-Path $built 'SkyrimMouseRelease.dll'))) { throw 'No built plugin in dist/. Run ./build.ps1 first.' }

# Newest build into the test install, every run.
Copy-Item (Join-Path $built '*') (Join-Path $install 'Data\SKSE\Plugins') -Force
Write-Host "Installed $((Get-Item (Join-Path $built 'SkyrimMouseRelease.dll')).LastWriteTime) build"
$ini = Join-Path $install 'Data\SKSE\Plugins\SkyrimMouseRelease.ini'
$lines = Get-Content $ini
if ($VerboseLog) { $lines = $lines -replace '^bVerboseLog=.*', 'bVerboseLog=1'; Write-Host 'Detailed log on' }
if ($PluginOff) { $lines = $lines -replace '^bReleaseMouse=.*', 'bReleaseMouse=0'; Write-Host 'Plugin OFF for this run' }
Set-Content $ini $lines

# Hold the main setup's profile folders.
$renamed = @()
try {
    foreach ($base in $profileBases.Values) {
        $live = Join-Path $base $profileName
        if (Test-Path $live) { Rename-Item $live $holdName; $renamed += $base }
    }
}
catch {
    foreach ($base in $renamed) { Rename-Item (Join-Path $base $holdName) $profileName }
    throw "Couldn't move the profile folders aside (is something using them?): $_"
}

try {
    # Borderless window at the main screen's size, so the mouse can leave it. The game fills in
    # everything else with defaults.
    Add-Type -AssemblyName System.Windows.Forms
    $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $testProfile = Join-Path $profileBases['My Games'] $profileName
    New-Item -ItemType Directory -Force $testProfile | Out-Null
    Set-Content (Join-Path $testProfile 'SkyrimPrefs.ini') @"
[Display]
bFull Screen=0
bBorderless=1
iSize W=$($screen.Width)
iSize H=$($screen.Height)
"@

    Write-Host "Starting Skyrim $Version. This window waits until the game closes."
    Start-Process (Join-Path $install 'skse64_loader.exe') -WorkingDirectory $install
    $game = $null
    for ($i = 0; $i -lt 120 -and -not $game; $i++) {
        Start-Sleep -Milliseconds 500
        $game = Get-Process SkyrimSE -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($install, 'OrdinalIgnoreCase') }
    }
    if (-not $game) { Write-Host 'The game did not start within a minute.' }
    else { $game | Wait-Process }
}
finally {
    Restore-Profiles $runDir
}

$log = Join-Path $runDir 'My Games\SKSE\SkyrimMouseRelease.log'
if (Test-Path $log) {
    Write-Host "Plugin log: $log"
    $problems = Select-String -Path $log -Pattern '\] \[(W|E|C)\] '
    if ($problems) { Write-Host "$($problems.Count) warning or error lines in the log" } else { Write-Host 'No warnings or errors in the log' }
}
else { Write-Host "No plugin log in $runDir; the plugin didn't load. Check $runDir\My Games\SKSE\skse64.log" }
