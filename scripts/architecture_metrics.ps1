<#
.SYNOPSIS
Records reproducible local architecture metrics without building or modifying source inputs.

.EXAMPLE
.\scripts\architecture_metrics.ps1 -Repetitions 3
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [string] $BuildDirectory = 'build-benchmark-vs-x64',
    [string] $TestBuildDirectory = 'build-dbginfo',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string] $Configuration = 'Release',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string] $TestConfiguration = 'RelWithDebInfo',
    [ValidateRange(1, 20)]
    [int] $Repetitions = 3,
    [string] $OutputPath,
    [switch] $SkipSlice,
    [switch] $SkipProfileValidation
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-RepoPath([string] $Path) {
    $candidate = if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    return [IO.Path]::GetFullPath($candidate)
}

function Get-Artifact([string] $Name, [string] $Path) {
    $absolutePath = Resolve-RepoPath $Path
    if (-not (Test-Path -LiteralPath $absolutePath -PathType Leaf)) {
        return [pscustomobject][ordered]@{
            name = $Name
            path = $absolutePath
            exists = $false
            bytes = $null
            last_write_utc = $null
        }
    }

    $item = Get-Item -LiteralPath $absolutePath
    return [pscustomobject][ordered]@{
        name = $Name
        path = $item.FullName
        exists = $true
        bytes = $item.Length
        last_write_utc = $item.LastWriteTimeUtc.ToString('o')
    }
}

function Invoke-TimedCommand([string] $Name, [string] $FilePath, [string[]] $Arguments) {
    if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) {
        throw "$Name executable was not found: $FilePath"
    }

    $runs = New-Object System.Collections.Generic.List[object]
    for ($iteration = 1; $iteration -le $Repetitions; ++$iteration) {
        $stopwatch = [Diagnostics.Stopwatch]::StartNew()
        $output = @(& $FilePath @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
        $stopwatch.Stop()

        $runs.Add([pscustomobject][ordered]@{
            repetition = $iteration
            exit_code = $exitCode
            duration_seconds = [math]::Round($stopwatch.Elapsed.TotalSeconds, 6)
            warning_lines = @($output | Where-Object { $_ -match '\[warning\]|warning:' }).Count
            error_lines = @($output | Where-Object { $_ -match '\[error\]|error:' }).Count
            output_tail = @($output | Select-Object -Last 8)
        })
    }

    $durations = @($runs | ForEach-Object { $_.duration_seconds })
    return [pscustomobject][ordered]@{
        name = $Name
        executable = $FilePath
        arguments = $Arguments
        repetitions = $runs
        all_passed = @($runs | Where-Object { $_.exit_code -ne 0 }).Count -eq 0
        mean_seconds = [math]::Round(($durations | Measure-Object -Average).Average, 6)
        min_seconds = ($durations | Measure-Object -Minimum).Minimum
        max_seconds = ($durations | Measure-Object -Maximum).Maximum
    }
}

$absoluteBuildDirectory = Resolve-RepoPath $BuildDirectory
$absoluteTestBuildDirectory = Resolve-RepoPath $TestBuildDirectory
$artifacts = @(
    Get-Artifact 'launcher' (Join-Path $absoluteBuildDirectory "src\$Configuration\orca-slicer.exe")
    Get-Artifact 'application_dll' (Join-Path $absoluteBuildDirectory "src\$Configuration\OrcaSlicer.dll")
    Get-Artifact 'core_static_library' (Join-Path $absoluteBuildDirectory "src\libslic3r\$Configuration\libslic3r.lib")
    Get-Artifact 'gui_static_library' (Join-Path $absoluteBuildDirectory "src\slic3r\$Configuration\libslic3r_gui.lib")
)

$measurements = New-Object System.Collections.Generic.List[object]
if (-not $SkipSlice) {
    $sliceExecutable = Join-Path $absoluteTestBuildDirectory "tests\fff_print\$TestConfiguration\fff_print_tests.exe"
    $sliceSpec = 'A default slice emits perimeter\, infill\, and skirt'
    $measurements.Add((Invoke-TimedCommand 'canonical_default_slice' $sliceExecutable @(
        $sliceSpec, '--reporter', 'compact', '--rng-seed', '12345'
    )))
}

if (-not $SkipProfileValidation) {
    $validatorExecutable = Join-Path $absoluteTestBuildDirectory "src\$TestConfiguration\OrcaSlicer_profile_validator.exe"
    $profileRoot = Join-Path $repoRoot 'resources\profiles'
    $measurements.Add((Invoke-TimedCommand 'profile_validation' $validatorExecutable @(
        '--path', $profileRoot
    )))
}

if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot "logs\architecture_metrics_$((Get-Date).ToString('yyyyMMdd_HHmmss')).json"
}
$absoluteOutputPath = Resolve-RepoPath $OutputPath
$outputDirectory = Split-Path -Parent $absoluteOutputPath
if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory | Out-Null
}

$result = [ordered]@{
    generated_at = (Get-Date).ToUniversalTime().ToString('o')
    source_commit = (& git -C $repoRoot rev-parse HEAD)
    source_dirty = [bool](& git -C $repoRoot status --porcelain)
    platform = [Environment]::OSVersion.VersionString
    build_directory = $absoluteBuildDirectory
    test_build_directory = $absoluteTestBuildDirectory
    configuration = $Configuration
    test_configuration = $TestConfiguration
    artifacts = $artifacts
    measurements = $measurements
}

$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $absoluteOutputPath -Encoding UTF8
Write-Host "Architecture metrics: $absoluteOutputPath" -ForegroundColor Green

if (@($measurements | Where-Object { -not $_.all_passed }).Count -ne 0) {
    exit 1
}
