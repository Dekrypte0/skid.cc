#Requires -Version 5.1
<#
.SYNOPSIS
    Builds skeet-ui for Windows x64.

.DESCRIPTION
    Locates MSBuild (vswhere first, known Visual Studio paths second), stops a
    running demo so the linker can replace the exe, then builds the project.

.EXAMPLE
    .\build.ps1

.EXAMPLE
    .\build.ps1 -Configuration Debug -Run

.EXAMPLE
    .\build.ps1 -Clean
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [switch]$Clean,

    [switch]$Run
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $MyInvocation.MyCommand.Definition
$project = Join-Path $root 'skeet-ui.vcxproj'
$exe = Join-Path $root "bin\x64\$Configuration\skeet-ui.exe"

if (-not (Test-Path -LiteralPath $project)) {
    throw "project not found: $project"
}

function Find-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
            -find 'MSBuild\**\Bin\MSBuild.exe' 2>$null
        if ($found) {
            return ($found | Select-Object -First 1)
        }
    }

    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\17\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\17\Professional\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\17\Enterprise\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe'
    )
    foreach ($path in $candidates) {
        if (Test-Path -LiteralPath $path) {
            return $path
        }
    }

    $onPath = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($onPath) {
        return $onPath.Source
    }

    throw 'MSBuild.exe not found. Install Visual Studio with the C++ desktop workload or run from a Developer PowerShell.'
}

$msbuild = Find-MSBuild
Write-Host "msbuild     : $msbuild"
Write-Host "configuration: $Configuration|x64"

Get-Process -Name 'skeet-ui' -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 250

$arguments = @(
    $project,
    "/p:Configuration=$Configuration",
    '/p:Platform=x64',
    '/m',
    '/v:m',
    '/nologo'
)
if ($Clean) {
    $arguments += '/t:Rebuild'
}

& $msbuild @arguments
if ($LASTEXITCODE -ne 0) {
    throw "build failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path -LiteralPath $exe)) {
    throw "build finished but the executable is missing: $exe"
}

$size = (Get-Item -LiteralPath $exe).Length
Write-Host ''
Write-Host ("built       : {0} ({1:N0} bytes)" -f $exe, $size)

if ($Run) {
    Write-Host 'starting    : skeet-ui'
    Start-Process -FilePath $exe | Out-Null
}
