<#
.SYNOPSIS
Launches a locally built OrcaSlicer from any working directory.

.EXAMPLE
.\run.ps1

.EXAMPLE
.\run.ps1 -Configuration RelWithDebInfo -Wait -- --single-instance
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [Alias('BuildType')]
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo')]
    [string] $Configuration = 'Release',

    [ValidateSet('x64', 'ARM64')]
    [string] $Architecture,

    [string] $BuildDirectory,
    [switch] $Wait,
    [switch] $ResolveOnly,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $ApplicationArguments
)

$ErrorActionPreference = 'Stop'

if (-not $Architecture) {
    $Architecture = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64' -or
        $env:PROCESSOR_ARCHITEW6432 -eq 'ARM64') { 'ARM64' } else { 'x64' }
}

$baseName = switch ($Configuration) {
    'Debug'          { 'build-dbg' }
    'RelWithDebInfo' { 'build-dbginfo' }
    default          { 'build' }
}
if ($Architecture -eq 'ARM64') { $baseName += '-arm64' }

if ($BuildDirectory) {
    if (-not [IO.Path]::IsPathRooted($BuildDirectory)) {
        $BuildDirectory = Join-Path $PSScriptRoot $BuildDirectory
    }
    $buildDirectories = @(Get-Item -LiteralPath $BuildDirectory -ErrorAction SilentlyContinue)
}
else {
    # Visual Studio version and Ninja suffixes are added by scripts/build.ps1.
    # Match the complete directory name so Release never selects build-dbginfo.
    $namePattern = '^' + [regex]::Escape($baseName) + '(?:-vs\d+|-ninja)?$'
    $buildDirectories = @(Get-ChildItem -LiteralPath $PSScriptRoot -Directory |
        Where-Object { $_.Name -match $namePattern })
}

$candidates = foreach ($directory in $buildDirectories) {
    foreach ($relativePath in @(
        'OrcaSlicer\orca-slicer.exe',
        "src\$Configuration\orca-slicer.exe"
    )) {
        $item = Get-Item -LiteralPath (Join-Path $directory.FullName $relativePath) -ErrorAction SilentlyContinue
        if ($item) {
            [pscustomobject]@{
                Executable = $item
                Installed = $relativePath.StartsWith('OrcaSlicer\')
            }
        }
    }
}

$selected = $candidates |
    Sort-Object @{ Expression = 'Installed'; Descending = $true },
                @{ Expression = { $_.Executable.LastWriteTimeUtc }; Descending = $true } |
    Select-Object -First 1

if (-not $selected) {
    $scope = if ($BuildDirectory) { $BuildDirectory } else { "$baseName, $baseName-vs*, or $baseName-ninja" }
    throw "No $Architecture $Configuration OrcaSlicer executable was found in $scope. Build it with: .\scripts\build.ps1 -Configuration $Configuration -Architecture $Architecture"
}

$executable = $selected.Executable.FullName
if ($ResolveOnly) {
    Write-Output $executable
    exit 0
}

Write-Host "Launching $executable"
if ($Wait) {
    Push-Location $selected.Executable.DirectoryName
    try {
        & $executable @ApplicationArguments
        exit $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
}

$startParameters = @{
    FilePath = $executable
    WorkingDirectory = $selected.Executable.DirectoryName
}
if ($ApplicationArguments) { $startParameters.ArgumentList = $ApplicationArguments }
Start-Process @startParameters
