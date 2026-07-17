<#
.SYNOPSIS
Creates a shortcut to a locally built OrcaSlicer that can be pinned to the taskbar.

.EXAMPLE
.\scripts\create-shortcut.ps1

.EXAMPLE
.\scripts\create-shortcut.ps1 -Configuration RelWithDebInfo
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo')]
    [string] $Configuration = 'Release',

    [ValidateSet('x64', 'ARM64')]
    [string] $Architecture,

    [string] $BuildDirectory,
    [string] $OutputPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$launcher = Join-Path $repoRoot 'run.ps1'
$resolveParameters = @{
    ResolveOnly = $true
    Configuration = $Configuration
}
if ($Architecture) { $resolveParameters.Architecture = $Architecture }
if ($BuildDirectory) { $resolveParameters.BuildDirectory = $BuildDirectory }

$executable = (& $launcher @resolveParameters | Select-Object -Last 1)
if (-not $executable -or -not (Test-Path -LiteralPath $executable)) {
    throw 'The OrcaSlicer launcher did not resolve a valid executable.'
}

if (-not $OutputPath) {
    $desktop = [Environment]::GetFolderPath('DesktopDirectory')
    $OutputPath = Join-Path $desktop 'OrcaSlicer (Local Build).lnk'
}
elseif (-not [IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path (Get-Location) $OutputPath
}
if ([IO.Path]::GetExtension($OutputPath) -ne '.lnk') { $OutputPath += '.lnk' }

if ($PSCmdlet.ShouldProcess($OutputPath, "Create shortcut to $executable")) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($OutputPath)
    $shortcut.TargetPath = $executable
    $shortcut.WorkingDirectory = Split-Path -Parent $executable
    $shortcut.IconLocation = "$executable,0"
    $shortcut.Description = "Local OrcaSlicer $Configuration build"
    $shortcut.Save()
    Write-Host "Created $OutputPath" -ForegroundColor Green
    Write-Host 'Right-click the shortcut and choose Pin to taskbar.'
}
