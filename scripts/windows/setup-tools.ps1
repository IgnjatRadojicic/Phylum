<#
.SYNOPSIS
    Installs everything needed to build the ReCap server on Windows into one folder.

.DESCRIPTION
    Needs no Visual Studio install. Downloads pinned, hash-checked versions of CMake, Ninja,
    Strawberry Perl and NASM, then Microsoft's compiler and Windows SDK. Nothing is
    installed into Windows and nothing goes outside ToolsDir, except that unpacking the
    SDK uses Windows Installer, which may briefly use the system temp folder.

    The compiler step runs portable-msvc.py by mmozeiko, a community script that downloads
    Microsoft's packages. It only runs with -AcceptMsvcLicense, which accepts the Visual
    Studio license. Read that license before passing it.

.PARAMETER ToolsDir
    Where to put the tools, about 3 GB. Defaults to .tools in the repository.

.EXAMPLE
    .\setup-tools.ps1 -AcceptMsvcLicense

.EXAMPLE
    .\setup-tools.ps1 -ToolsDir D:\ReCapTools -AcceptMsvcLicense
#>
param(
    [string]$ToolsDir = (Join-Path $PSScriptRoot '..\..\.tools'),
    [switch]$AcceptMsvcLicense,
    [string]$Python = 'python'
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = 'Tls12'

$ToolsDir = [IO.Path]::GetFullPath($ToolsDir)

$downloads = Join-Path $ToolsDir 'dl'
$temp = Join-Path $ToolsDir 'tmp'
New-Item -ItemType Directory -Force $downloads, $temp | Out-Null
$env:TEMP = $temp
$env:TMP = $temp

function Get-Verified([string]$Url, [string]$Sha256, [string]$FileName) {
    $file = Join-Path $downloads $FileName
    if (-not ((Test-Path $file) -and (Get-FileHash $file -Algorithm SHA256).Hash -eq $Sha256)) {
        Write-Host "Downloading $FileName"
        Invoke-WebRequest -UseBasicParsing $Url -OutFile $file
        $actual = (Get-FileHash $file -Algorithm SHA256).Hash
        if ($actual -ne $Sha256) {
            Remove-Item $file
            throw "Hash mismatch for ${FileName}: got $actual"
        }
    }
    return $file
}

function Install-Zip([string]$Name, [string]$Url, [string]$Sha256, [string]$Check) {
    $target = Join-Path $ToolsDir $Name
    if (Test-Path (Join-Path $target $Check)) {
        Write-Host "$Name already installed"
        return
    }
    $zip = Get-Verified $Url $Sha256 "$Name.zip"
    Write-Host "Unpacking $Name"
    Expand-Archive $zip -DestinationPath $target -Force
}

Install-Zip 'cmake' 'https://github.com/Kitware/CMake/releases/download/v4.4.5/cmake-4.4.5-windows-x86_64.zip' `
    '326C826486D46A23CF9D5A1C9EE3A567D427BDD02C4A34071937A2A675CFF004' 'cmake-4.4.5-windows-x86_64\bin\cmake.exe'
Install-Zip 'perl' 'https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_54231_64bit/strawberry-perl-5.42.3.1-64bit-portable.zip' `
    '6A081A811781C30ACA51DBC036AFD93092AF91E3297901F02C17043795A10690' 'perl\bin\perl.exe'
Install-Zip 'nasm' 'https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/win64/nasm-2.16.03-win64.zip' `
    '3EE4782247BCB874378D02F7EAB4E294A84D3D15F3F6EE2DE2F47A46AA7226E6' 'nasm-2.16.03\nasm.exe'

Install-Zip 'ninja' 'https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip' `
    '07FC8261B42B20E71D1720B39068C2E14FFCEE6396B76FB7A795FB460B78DC65' 'ninja.exe'

$msvc = Join-Path $ToolsDir 'msvc'
if (Test-Path (Join-Path $msvc 'msvc\setup_x64.bat')) {
    Write-Host 'msvc already installed'
} elseif (-not $AcceptMsvcLicense) {
    Write-Warning 'Skipped the compiler. Rerun with -AcceptMsvcLicense to accept the Visual Studio license and download it.'
} else {
    $script = Get-Verified 'https://gist.githubusercontent.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977/raw/49b58850aa5e5c1e65f090d4651ee76607883e11/portable-msvc.py' `
        '1791CCCCA594FF083CE8D3E0A139E6156D2C312E24332A909E608010F0230A51' 'portable-msvc.py'
    New-Item -ItemType Directory -Force $msvc | Out-Null
    Push-Location $msvc
    try {
        & $Python $script --vs 2022 --accept-license
        if ($LASTEXITCODE -ne 0) { throw "portable-msvc.py failed with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
}

Write-Host "Done. Build with: .\build.ps1 -ToolsDir `"$ToolsDir`""
