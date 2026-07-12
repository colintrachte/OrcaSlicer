<#
.SYNOPSIS
Compatibility wrapper for the canonical Windows build script.

.DESCRIPTION
New automation should call scripts\build.ps1 directly. This wrapper preserves
the options accepted by the earlier root-level PowerShell build script.
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'RelWithDebInfo', 'Debug')]
    [string] $Config = 'RelWithDebInfo',
    [int] $Jobs = 0,
    [switch] $SkipDeps,
    [switch] $SkipApp,
    [switch] $Run,
    [switch] $PreflightOnly,
    [switch] $NoPause
)

$driver = Join-Path $PSScriptRoot 'scripts\build.ps1'
$arguments = @{ Configuration = $Config }
if ($Jobs -gt 0) { $arguments.Parallel = $Jobs }
if ($SkipDeps) { $arguments.SkipDependencies = $true }
if ($SkipApp) { $arguments.DependenciesOnly = $true }
if ($Run) { $arguments.Run = $true }
if ($PreflightOnly) { $arguments.PreflightOnly = $true }
if ($NoPause) { $arguments.NoPause = $true }

& $driver @arguments
exit $LASTEXITCODE
