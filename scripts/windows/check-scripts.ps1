<#
.SYNOPSIS
    Fails if fewer game scripts work than before.

.DESCRIPTION
    Runs the built server's script check against an installed game and compares the result
    with darkspore_server\tests\script-baseline.json. Raise the baseline when the numbers improve.

.PARAMETER GameBin
    The game's DarksporeBin folder, which holds the extracted scripts.

.EXAMPLE
    .\check-scripts.ps1 -GameBin "C:\Program Files (x86)\Steam\steamapps\common\Darkspore\DarksporeBin"
#>
param(
    [Parameter(Mandatory)][string]$GameBin
)

$ErrorActionPreference = 'Stop'

$source = (Resolve-Path (Join-Path $PSScriptRoot '..\..\darkspore_server')).Path
$baseline = Get-Content (Join-Path $source 'tests\script-baseline.json') -Raw | ConvertFrom-Json
$server = Join-Path $GameBin 'Server'

$env:NoDefaultCurrentDirectoryInExePath = $null
$process = Start-Process (Join-Path $source 'build\recap_server.exe') -ArgumentList '--check-scripts' `
    -WorkingDirectory $server -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "Script check crashed with exit code $($process.ExitCode)" }

$report = Get-Content (Join-Path $server 'script-report.txt') -Raw
$combat = Get-Content (Join-Path $server 'script-combat.txt') -Raw
if ($report -notmatch 'Game ability scripts: (\d+) registered') { throw 'No registration count in the report' }
$registered = [int]$Matches[1]
if ($combat -notmatch 'Abilities passed: (\d+) of') { throw 'No ability count in the report' }
$passed = [int]$Matches[1]
if ($combat -notmatch 'MATH good') { throw 'Script maths test failed' }

$flight = Get-Content (Join-Path $server 'script-flight.txt')
$flight | ForEach-Object { Write-Host $_ }
if (-not ($flight -match 'FLIGHT') -or ($flight -match ' wrong')) { throw 'Projectile flight test failed' }

Write-Host "Registered $registered (baseline $($baseline.registered))"
Write-Host "Abilities passed $passed (baseline $($baseline.abilitiesPassed))"
if ($registered -lt $baseline.registered -or $passed -lt $baseline.abilitiesPassed) {
    throw 'Script check got worse than the baseline'
}
if ($registered -gt $baseline.registered -or $passed -gt $baseline.abilitiesPassed) {
    Write-Host 'Better than the baseline. Raise it in tests\script-baseline.json.'
}
