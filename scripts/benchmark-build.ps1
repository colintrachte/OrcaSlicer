<#
.SYNOPSIS
Runs reproducible OrcaSlicer build-iteration scenarios and collects build manifests.

.EXAMPLE
.\scripts\benchmark-build.ps1 -Scenario NoOp,LeafGui -Generator VisualStudio -Repetitions 3

.EXAMPLE
.\scripts\benchmark-build.ps1 -Scenario Clean -Generator Ninja -ParallelValues 1,2,4
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('Clean', 'NoOp', 'LeafCore', 'LeafGui', 'ConfigLeaf', 'CoreHeader', 'GuiHeader', 'GuiPch', 'LinkOnly', 'InterruptedRecovery', 'Install')]
    [string[]] $Scenario = @('NoOp'),

    [ValidateSet('VisualStudio', 'Ninja')]
    [string] $Generator = 'VisualStudio',

    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string] $Configuration = 'Release',

    [ValidateSet('x64', 'ARM64')]
    [string] $Architecture = 'x64',

    [string] $Target = 'App',

    [ValidateRange(1, 20)]
    [int] $Repetitions = 3,

    [ValidateRange(1, 128)]
    [int[]] $ParallelValues = @(1),

    [string] $BuildDirectory,
    [string] $DependencyBuildDirectory = 'deps/build-vs2026',
    [string] $LeafCorePath = 'src/libslic3r/BoundingBox.cpp',
    [string] $LeafGuiPath = 'src/slic3r/GUI/AboutDialog.cpp',
    [string] $ConfigLeafPath = 'src/libslic3r/Config.cpp',
    [string] $CoreHeaderPath = 'src/libslic3r/PrintConfig.hpp',
    [string] $GuiHeaderPath = 'src/slic3r/GUI/GUI_App.hpp',
    [string] $GuiPchPath = 'src/slic3r/pchheader.hpp',
    [string] $LinkInputPath,
    [switch] $QuietBuildOutput
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

function Normalize-ProcessPathEnvironment {
    $pathEntries = @(& $env:ComSpec /d /c 'set path')
    $hasUpperPath = [bool]($pathEntries | Where-Object { $_ -cmatch '^PATH=' } | Select-Object -First 1)
    $hasMixedPath = [bool]($pathEntries | Where-Object { $_ -cmatch '^Path=' } | Select-Object -First 1)
    if ($hasUpperPath -and $hasMixedPath) {
        Remove-Item Env:PATH -ErrorAction Stop
    }
}

Normalize-ProcessPathEnvironment

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDriver = Join-Path $PSScriptRoot 'build.ps1'
$logDirectory = Join-Path $repoRoot 'logs'
if (-not $BuildDirectory) {
    $generatorSuffix = if ($Generator -eq 'Ninja') { 'ninja' } else { 'vs' }
    $BuildDirectory = "build-benchmark-$generatorSuffix-$($Architecture.ToLowerInvariant())"
}

function Resolve-ScenarioInput([string] $Name) {
    $relative = switch ($Name) {
        'LeafCore'  { $LeafCorePath }
        'LeafGui'   { $LeafGuiPath }
        'ConfigLeaf' { $ConfigLeafPath }
        'CoreHeader' { $CoreHeaderPath }
        'GuiHeader' { $GuiHeaderPath }
        'GuiPch' { $GuiPchPath }
        'LinkOnly'  { $LinkInputPath }
        default     { $null }
    }
    if (-not $relative) { return $null }
    $path = if ([System.IO.Path]::IsPathRooted($relative)) { $relative } else { Join-Path $repoRoot $relative }
    return [System.IO.Path]::GetFullPath($path)
}

function ConvertTo-PowerShellLiteral([string] $Value) {
    return "'$($Value.Replace("'", "''"))'"
}

function Invoke-ScenarioBuild([string] $Name, [int] $Parallel, [string] $InputPath) {
    $absoluteBuildDirectory = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $repoRoot $BuildDirectory }
    $buildState = $(switch ($Name) {
            'Clean' { 'Clean' }
            'InterruptedRecovery' { 'InterruptedRecovery' }
            default { 'Warm' }
        })
    $commandParts = [System.Collections.Generic.List[string]]::new()
    [void] $commandParts.Add("& $(ConvertTo-PowerShellLiteral $buildDriver)")
    [void] $commandParts.Add('-NoPause')
    [void] $commandParts.Add('-SlicerOnly')
    if ($Name -ne 'Install') { [void] $commandParts.Add('-BuildOnly') }
    [void] $commandParts.Add("-Configuration $(ConvertTo-PowerShellLiteral $Configuration)")
    [void] $commandParts.Add("-Architecture $(ConvertTo-PowerShellLiteral $Architecture)")
    [void] $commandParts.Add("-Target $(ConvertTo-PowerShellLiteral $Target)")
    [void] $commandParts.Add("-Parallel $Parallel")
    [void] $commandParts.Add("-BuildDirectory $(ConvertTo-PowerShellLiteral $BuildDirectory)")
    [void] $commandParts.Add("-DependencyBuildDirectory $(ConvertTo-PowerShellLiteral $DependencyBuildDirectory)")
    [void] $commandParts.Add("-BuildState $(ConvertTo-PowerShellLiteral $buildState)")
    if ($Generator -eq 'Ninja') { [void] $commandParts.Add('-Ninja') }
    if ($InputPath) { [void] $commandParts.Add("-ChangedInput $(ConvertTo-PowerShellLiteral $InputPath)") }
    if ($Name -eq 'Clean') {
        [void] $commandParts.Add('-Configure')
        [void] $commandParts.Add('-Fresh')
        [void] $commandParts.Add('-CleanFirst')
    }
    elseif ($Name -eq 'Install') {
        [void] $commandParts.Add('-Configure')
        [void] $commandParts.Add('-Install')
    }
    elseif (-not (Test-Path -LiteralPath (Join-Path $absoluteBuildDirectory 'CMakeCache.txt'))) {
        [void] $commandParts.Add('-Configure')
    }
    else {
        [void] $commandParts.Add('-NoConfigure')
    }

    $started = Get-Date
    $powerShellPath = (Get-Process -Id $PID).Path
    $childCommand = $commandParts -join ' '
    $encodedCommand = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($childCommand))
    $processArguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', $encodedCommand)
    if ($QuietBuildOutput) {
        $consoleStamp = (Get-Date).ToString('yyyyMMdd_HHmmss_fff')
        $standardOutput = Join-Path $logDirectory "benchmark_console_${consoleStamp}.log"
        $standardError = Join-Path $logDirectory "benchmark_console_${consoleStamp}.err.log"
        $process = Start-Process -FilePath $powerShellPath -ArgumentList $processArguments -NoNewWindow -PassThru `
            -RedirectStandardOutput $standardOutput -RedirectStandardError $standardError
        # Start-Process -Wait follows descendants on Windows. MSVC's persistent
        # mspdbsrv.exe would therefore keep the benchmark harness waiting after
        # the build child had exited. Wait only for the direct child process.
        # Touching Handle before the wait makes Windows PowerShell retain the
        # native exit code after the process object refreshes.
        [void] $process.Handle
        $process.WaitForExit()
        $process.Refresh()
        $exitCode = $process.ExitCode
        if ($exitCode -ne 0) {
            Get-Content -LiteralPath $standardOutput, $standardError -ErrorAction SilentlyContinue |
                Select-Object -Last 100 | ForEach-Object { Write-Host $_ }
        }
    }
    else {
        & $powerShellPath @processArguments 2>&1 | ForEach-Object { Write-Host $_.ToString() }
        $exitCode = $LASTEXITCODE
    }
    $manifest = Get-ChildItem -LiteralPath $logDirectory -Filter "build_${Architecture}_${Configuration}_*.json" |
        Where-Object { $_.LastWriteTime -ge $started } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $manifest) { throw "The build driver did not produce a manifest for $Name."
    }
    return [pscustomobject]@{
        scenario = $Name
        parallel = $Parallel
        exit_code = $exitCode
        manifest = $manifest.FullName
        data = Get-Content -LiteralPath $manifest.FullName -Raw | ConvertFrom-Json
    }
}

$results = New-Object System.Collections.Generic.List[object]
foreach ($parallel in $ParallelValues) {
    foreach ($name in $Scenario) {
        $inputPath = Resolve-ScenarioInput $name
        if ($name -eq 'LinkOnly' -and -not $inputPath) {
            throw 'LinkOnly requires -LinkInputPath naming an existing linked library or generated input.'
        }
        if ($inputPath -and -not (Test-Path -LiteralPath $inputPath)) {
            throw "Scenario input was not found: $inputPath"
        }

        for ($iteration = 1; $iteration -le $Repetitions; $iteration++) {
            Write-Host "`n=== $name, parallel $parallel, repetition $iteration/$Repetitions ===" -ForegroundColor Cyan
            $originalTimestamp = if ($inputPath) { (Get-Item -LiteralPath $inputPath).LastWriteTimeUtc } else { $null }
            try {
                if ($inputPath) { (Get-Item -LiteralPath $inputPath).LastWriteTimeUtc = [DateTime]::UtcNow }
                $result = Invoke-ScenarioBuild $name $parallel $inputPath
                $results.Add([pscustomobject][ordered]@{
                    scenario = $name
                    repetition = $iteration
                    parallel = $parallel
                    exit_code = $result.exit_code
                    duration_seconds = $result.data.duration_seconds
                    objects_recompiled = $result.data.objects_recompiled
                    pch_status = $result.data.pch_status
                    manifest = $result.manifest
                })
                if ($result.exit_code -ne 0) { throw "$name failed with exit code $($result.exit_code)." }
            }
            finally {
                if ($inputPath -and $null -ne $originalTimestamp) {
                    (Get-Item -LiteralPath $inputPath).LastWriteTimeUtc = $originalTimestamp
                }
            }
        }
    }
}

$summaryPath = Join-Path $logDirectory "benchmark_$((Get-Date).ToString('yyyyMMdd_HHmmss')).json"
$summaryBuildDirectory = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $repoRoot $BuildDirectory }
[ordered]@{
    source_commit = (& git -C $repoRoot rev-parse HEAD)
    source_dirty = [bool](& git -C $repoRoot status --porcelain)
    generator = $Generator
    configuration = $Configuration
    architecture = $Architecture
    target = $Target
    build_directory = [System.IO.Path]::GetFullPath($summaryBuildDirectory)
    results = $results
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "`nBenchmark summary: $summaryPath" -ForegroundColor Green
