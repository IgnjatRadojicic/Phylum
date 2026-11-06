<#
.SYNOPSIS
    Builds the ReCap server with the tools from setup-tools.ps1, and optionally installs it.

.PARAMETER GameBin
    The game's DarksporeBin folder. When given, the built server replaces Server\recap_server.exe
    and the old one is kept next to it as recap_server.previous.exe. The server must not be running.

.PARAMETER ToolsDir
    The folder given to setup-tools.ps1. Defaults to .tools in the repository.

.EXAMPLE
    .\build.ps1

.EXAMPLE
    .\build.ps1 -GameBin "C:\Program Files (x86)\Steam\steamapps\common\Darkspore\DarksporeBin"
#>
param(
    [string]$ToolsDir = (Join-Path $PSScriptRoot '..\..\.tools'),
    [string]$GameBin
)

$ErrorActionPreference = 'Stop'

$ToolsDir = [IO.Path]::GetFullPath($ToolsDir)
$source = (Resolve-Path (Join-Path $PSScriptRoot '..\..\darkspore_server')).Path
$setup = Join-Path $ToolsDir 'msvc\msvc\setup_x64.bat'
if (-not (Test-Path $setup)) { throw "No compiler at $setup. Run setup-tools.ps1 first." }

if (-not (Test-Path (Join-Path $ToolsDir 'ninja\ninja.exe'))) { throw 'No Ninja found. Run setup-tools.ps1 again.' }

$temp = Join-Path $ToolsDir 'tmp'
New-Item -ItemType Directory -Force $temp | Out-Null

# A build folder cannot change generator, so start over if it was made by another one.
$buildDir = Join-Path $source 'build'
$cache = Join-Path $buildDir 'CMakeCache.txt'
if ((Test-Path $cache) -and -not (Select-String -Path $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=Ninja$' -Quiet)) {
    Write-Host 'Removing a build folder made by another generator'
    Remove-Item $buildDir -Recurse -Force
}

# The compiler environment only exists inside cmd, so run the whole build there.
$batch = Join-Path $temp 'recap-build.bat'
@"
@echo off
call "$setup"
set NoDefaultCurrentDirectoryInExePath=
set TEMP=$temp
set TMP=$temp
set CPM_SOURCE_CACHE=$ToolsDir\cpm
set CC=cl
set CXX=cl
set PATH=$ToolsDir\cmake\cmake-4.4.5-windows-x86_64\bin;$ToolsDir\ninja;$ToolsDir\perl\perl\bin;$ToolsDir\nasm\nasm-2.16.03;%PATH%
cd /d "$source"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENSSL_TARGET_PLATFORM=VC-WIN64A -DCMAKE_INSTALL_PREFIX=./dist || exit /b 1
cmake --build build --target recap_server recap_tests || exit /b 1
build\recap_tests.exe || exit /b 1
"@ | Set-Content -Encoding ascii $batch

# Windows PowerShell turns any redirected stderr line into an error, and CMake warns on stderr.
# Only the exit code decides whether the build failed.
$ErrorActionPreference = 'Continue'
cmd /c $batch
$buildExitCode = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if ($buildExitCode -ne 0) { throw "Build failed with exit code $buildExitCode" }

$built = Join-Path $source 'build\recap_server.exe'
Write-Host "Built $built"

if ($GameBin) {
    if (Get-Process recap_server -ErrorAction SilentlyContinue) { throw 'recap_server is running. Close it first.' }
    $target = Join-Path $GameBin 'Server\recap_server.exe'
    if (-not (Test-Path $target)) { throw "No ReCap server at $target. Extract the ReCap release into DarksporeBin first." }
    Copy-Item $target (Join-Path $GameBin 'Server\recap_server.previous.exe') -Force
    Copy-Item $built $target -Force
    Write-Host "Installed to $target"
}
