<#
.SYNOPSIS
    Builds the server and runs every check, printing only a short summary.

.DESCRIPTION
    Runs the build with its unit tests, the script check with its flight tests, and the script
    contract check if a luac listing is given. The full build output goes to a log file.

.PARAMETER GameBin
    The game's DarksporeBin folder, which holds the extracted scripts.

.PARAMETER ToolsDir
    Where the build tools are, as for build.ps1.

.PARAMETER Listing
    A luac listing of every game script, for the contract check.

.EXAMPLE
    .\verify.ps1 -GameBin "C:\Program Files (x86)\Steam\steamapps\common\Darkspore\DarksporeBin"
#>
param(
    [Parameter(Mandatory)][string]$GameBin,
    [string]$ToolsDir = (Join-Path $PSScriptRoot '..\..\.tools'),
    [string]$Listing
)

$ErrorActionPreference = 'Stop'
$ToolsDir = [IO.Path]::GetFullPath($ToolsDir)
$server = Join-Path $GameBin 'Server'
$buildLog = Join-Path $ToolsDir 'build-latest.log'
$failed = $false

try {
    & (Join-Path $PSScriptRoot 'build.ps1') -ToolsDir $ToolsDir *>&1 | ForEach-Object { "$_" } | Set-Content -Encoding utf8 $buildLog
} catch {
    $failed = $true
    Write-Host "BUILD FAILED: $_"
}

$summary = Get-Content $buildLog | Select-String -Pattern ' error C\d+| error LNK\d+|test cases:'
$summary | Select-Object -Last 6 | ForEach-Object { Write-Host $_.Line.Trim() }
if ($failed) { Write-Host "Full output: $buildLog"; exit 1 }

try {
    & (Join-Path $PSScriptRoot 'check-scripts.ps1') -GameBin $GameBin *>&1 | Out-Null
} catch {
    $failed = $true
    Write-Host "CHECK FAILED: $_"
}

$report = Get-Content (Join-Path $server 'script-report.txt') -Raw
$combat = Get-Content (Join-Path $server 'script-combat.txt') -Raw
$flight = Get-Content (Join-Path $server 'script-flight.txt')
if ($report -match 'Game ability scripts: (\d+) registered') { $registered = $Matches[1] }
if ($combat -match 'Abilities passed: (\d+ of \d+)') { $passed = $Matches[1] }
$good = @($flight -match ' good').Count
$wrong = @($flight -match ' wrong')
Write-Host "Scripts registered $registered, abilities passed $passed, flight tests $good good, $($wrong.Count) wrong"
$wrong | ForEach-Object { Write-Host $_ }

$errors = @(Get-Content (Join-Path $server 'script-report.txt') | Select-String -Pattern 'LuaThread::').Count
if ($errors -gt 0) { Write-Host "Script thread errors: $errors, see script-report.txt" }

if ($Listing) {
    $python = Get-Command python -ErrorAction SilentlyContinue
    $tool = Join-Path $ToolsDir 'Python313\python.exe'
    if (Test-Path $tool) { $python = $tool } elseif ($python) { $python = $python.Source }
    if ($python) {
        $contract = Join-Path $PSScriptRoot '..\script_contract.py'
        $source = Join-Path $PSScriptRoot '..\..\darkspore_server\source\game'
        & $python -I $contract $Listing (Join-Path $server 'script-api.txt') $source (Join-Path $server 'script-contract.txt')
    } else {
        Write-Host 'No Python found, so the contract check was skipped.'
    }
}

if ($failed) { exit 1 }
