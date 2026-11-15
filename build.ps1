# Builds the plugin and fills dist/ (and a Vortex-ready zip) from a plain PowerShell window.
# Usage: ./build.ps1
$ErrorActionPreference = 'Stop'

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio with the C++ workload not found" }
$tools = Join-Path $env:LOCALAPPDATA 'vcpkg\downloads\tools'
$cmakeBin = Get-ChildItem $tools -Directory -Filter 'cmake-*' | Sort-Object Name -Descending | Select-Object -First 1 |
    ForEach-Object { Get-ChildItem $_.FullName -Recurse -Filter cmake.exe | Select-Object -First 1 } | ForEach-Object DirectoryName
$ninjaDir = Get-ChildItem $tools -Directory -Filter 'ninja-*' | Sort-Object Name -Descending | Select-Object -First 1 | ForEach-Object FullName
if (-not $cmakeBin -or -not $ninjaDir) { throw "CMake or Ninja missing; run: vcpkg fetch cmake; vcpkg fetch ninja" }

Push-Location $PSScriptRoot
try {
    & "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
    $env:VCPKG_ROOT = "$vs\VC\vcpkg"
    $env:PATH = "$cmakeBin;$ninjaDir;$env:PATH"

    cmake --preset release
    if ($LASTEXITCODE) { throw "configure failed" }
    cmake --build --preset release
    if ($LASTEXITCODE) { throw "build failed" }

    $zip = Join-Path $PSScriptRoot 'SkyrimMouseRelease.zip'
    if (Test-Path $zip) { Remove-Item $zip }
    Compress-Archive -Path (Join-Path $PSScriptRoot 'dist\*') -DestinationPath $zip
    Write-Host "Built: $zip"
}
finally {
    Pop-Location
}
