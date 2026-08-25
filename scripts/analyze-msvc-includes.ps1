<#
.SYNOPSIS
Builds a weighted MSVC transitive-include invalidation graph.

.DESCRIPTION
CL.read.*.tlog records the full tracked dependency set for each compiler input. Optional MSVC
/Bt+ logs add measured frontend/backend compiler time per translation unit. The result reports
both consumer counts and the observed compile time invalidated by each tracked header. Optional
/showIncludes logs add compiler-observed direct include edges when captured with one compiler
worker so output from translation units cannot interleave.

.EXAMPLE
.\scripts\analyze-msvc-includes.ps1 -BuildDirectory build-vs2026

.EXAMPLE
.\scripts\analyze-msvc-includes.ps1 -BuildDirectory build-benchmark-vs-x64 `
    -TimingLog logs\slicer_x64_Release_20260728_160551.log -HeaderRoot src

.EXAMPLE
.\scripts\analyze-msvc-includes.ps1 -BuildDirectory build-benchmark-vs-x64 `
    -ShowIncludesLog logs\slicer_x64_Release_20260731_073224.log -HeaderRoot src
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [string] $BuildDirectory,

    [string[]] $TimingLog = @(),

    [string[]] $ShowIncludesLog = @(),

    [string] $BudgetPath,

    [string] $HeaderRoot,

    [string] $OutputPath,

    [ValidateRange(1, 1000)]
    [int] $Top = 100
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-RepositoryPath([string] $Path) {
    $absolute = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    return [System.IO.Path]::GetFullPath($absolute)
}

function ConvertTo-PathKey([string] $Path) {
    return ([System.IO.Path]::GetFullPath($Path)).TrimEnd('\', '/').ToLowerInvariant()
}

function ConvertTo-DisplayPath([string] $PathKey) {
    $displayPath = $PathKey
    $repoKey = ConvertTo-PathKey $repoRoot
    if ($displayPath -eq $repoKey) { return '.' }
    if ($displayPath.StartsWith($repoKey + '\') -or $displayPath.StartsWith($repoKey + '/')) {
        $displayPath = $displayPath.Substring($repoKey.Length).TrimStart('\', '/')
    }
    return $displayPath.Replace('\', '/')
}

$absoluteBuildDirectory = Resolve-RepositoryPath $BuildDirectory
if (-not (Test-Path -LiteralPath $absoluteBuildDirectory)) {
    throw "Build directory was not found: $absoluteBuildDirectory"
}

$absoluteHeaderRoot = if ($HeaderRoot) { Resolve-RepositoryPath $HeaderRoot } else { $null }
if ($absoluteHeaderRoot -and -not (Test-Path -LiteralPath $absoluteHeaderRoot)) {
    throw "Header root was not found: $absoluteHeaderRoot"
}
$headerRootKey = if ($absoluteHeaderRoot) { ConvertTo-PathKey $absoluteHeaderRoot } else { $null }

$absoluteTimingLogs = @($TimingLog | ForEach-Object {
    $path = Resolve-RepositoryPath $_
    if (-not (Test-Path -LiteralPath $path)) { throw "Timing log was not found: $path" }
    $path
})

$absoluteShowIncludesLogs = @($ShowIncludesLog | ForEach-Object {
    $path = Resolve-RepositoryPath $_
    if (-not (Test-Path -LiteralPath $path)) { throw "Show-includes log was not found: $path" }
    $path
})

$absoluteBudgetPath = if ($BudgetPath) {
    $path = Resolve-RepositoryPath $BudgetPath
    if (-not (Test-Path -LiteralPath $path)) { throw "Header budget file was not found: $path" }
    $path
}
else { $null }

if (-not $OutputPath) {
    $logDirectory = Join-Path $repoRoot 'logs'
    New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
    $OutputPath = Join-Path $logDirectory "includes_$((Get-Date).ToString('yyyyMMdd_HHmmss')).json"
}
else {
    $OutputPath = Resolve-RepositoryPath $OutputPath
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

$tlogs = @(Get-ChildItem -LiteralPath $absoluteBuildDirectory -Filter 'CL.read.*.tlog' -Recurse -File -ErrorAction SilentlyContinue)
if (-not $tlogs.Count) { throw "No CL.read.*.tlog files were found below $absoluteBuildDirectory" }

$consumerMap = @{}
$knownSources = New-Object System.Collections.Generic.HashSet[string]([StringComparer]::OrdinalIgnoreCase)
$edgeCount = 0
foreach ($tlog in $tlogs) {
    $sources = @()
    foreach ($rawLine in Get-Content -LiteralPath $tlog.FullName) {
        $line = $rawLine.Trim()
        if (-not $line) { continue }
        if ($line[0] -eq '^') {
            $sources = @($line.Substring(1).Split('|') |
                Where-Object { $_ -match '(?i)\.(c|cc|cpp|cxx)$' } |
                ForEach-Object { ConvertTo-PathKey $_ })
            foreach ($source in $sources) { [void]$knownSources.Add($source) }
            continue
        }
        if (-not $sources.Count -or $line -notmatch '(?i)\.(h|hh|hpp|hxx|inl)$') { continue }
        $header = ConvertTo-PathKey $line
        if ($headerRootKey -and -not ($header -eq $headerRootKey -or $header.StartsWith($headerRootKey + '\') -or $header.StartsWith($headerRootKey + '/'))) {
            continue
        }
        if (-not $consumerMap.ContainsKey($header)) {
            $consumerMap[$header] = New-Object System.Collections.Generic.HashSet[string]([StringComparer]::OrdinalIgnoreCase)
        }
        foreach ($source in $sources) {
            if ($consumerMap[$header].Add($source)) { $edgeCount++ }
        }
    }
}

# /showIncludes emits a source marker followed by an indented include tree. Capture with one
# compiler worker; parallel compiler output may interleave translation-unit trees.
$sourceKeysByLeaf = @{}
foreach ($sourceKey in $knownSources) {
    $leaf = [System.IO.Path]::GetFileName($sourceKey).ToLowerInvariant()
    if (-not $sourceKeysByLeaf.ContainsKey($leaf)) {
        $sourceKeysByLeaf[$leaf] = New-Object System.Collections.Generic.List[string]
    }
    $sourceKeysByLeaf[$leaf].Add($sourceKey)
}

$directIncluderMap = @{}
$directSourceConsumerMap = @{}
$directEdgeSet = New-Object System.Collections.Generic.HashSet[string]([StringComparer]::OrdinalIgnoreCase)
$directEvidenceSources = New-Object System.Collections.Generic.HashSet[string]([StringComparer]::OrdinalIgnoreCase)
$showIncludesDetails = New-Object System.Collections.Generic.List[object]
foreach ($showIncludesPath in $absoluteShowIncludesLogs) {
    $lines = @(Get-Content -LiteralPath $showIncludesPath)
    $buildBoundaries = @()
    for ($lineIndex = 0; $lineIndex -lt $lines.Count; $lineIndex++) {
        if ($lines[$lineIndex] -match '^>\s+.*cmake(?:\.exe)?\s+--build\s+') {
            $buildBoundaries += $lineIndex
        }
    }
    $selectedStart = if ($buildBoundaries.Count) { $buildBoundaries[-1] } else { 0 }
    $currentSource = $null
    $includeStack = @{}
    $records = 0
    $ambiguousSourceMarkers = 0
    $capturedSources = New-Object System.Collections.Generic.HashSet[string]([StringComparer]::OrdinalIgnoreCase)

    for ($lineIndex = $selectedStart; $lineIndex -lt $lines.Count; $lineIndex++) {
        $line = $lines[$lineIndex]
        if ($line -match '^\s{2}(?<leaf>[^\\/:]+\.(?:c|cc|cpp|cxx))\s*$') {
            $leaf = $Matches.leaf.ToLowerInvariant()
            if ($sourceKeysByLeaf.ContainsKey($leaf) -and $sourceKeysByLeaf[$leaf].Count -eq 1) {
                $currentSource = $sourceKeysByLeaf[$leaf][0]
                [void]$capturedSources.Add($currentSource)
                [void]$directEvidenceSources.Add($currentSource)
            }
            else {
                $currentSource = $null
                if ($sourceKeysByLeaf.ContainsKey($leaf)) { $ambiguousSourceMarkers++ }
            }
            $includeStack = @{}
            continue
        }
        if (-not $currentSource -or $line -notmatch '^\s*Note: including file:(?<indent>\s+)(?<path>.+?)\s*$') {
            continue
        }

        $depth = $Matches.indent.Length
        $includedPath = ConvertTo-PathKey $Matches.path
        $includerPath = if ($depth -eq 1) {
            $currentSource
        }
        elseif ($includeStack.ContainsKey($depth - 1)) {
            $includeStack[$depth - 1]
        }
        else { $null }
        $includeStack[$depth] = $includedPath
        foreach ($staleDepth in @($includeStack.Keys | Where-Object { $_ -gt $depth })) {
            $includeStack.Remove($staleDepth)
        }
        if (-not $includerPath) { continue }

        $edgeKey = $includerPath + '|' + $includedPath
        if (-not $directEdgeSet.Add($edgeKey)) { continue }
        $records++
        if (-not $directIncluderMap.ContainsKey($includedPath)) {
            $directIncluderMap[$includedPath] = New-Object System.Collections.Generic.HashSet[string]([StringComparer]::OrdinalIgnoreCase)
        }
        [void]$directIncluderMap[$includedPath].Add($includerPath)
        if ($depth -eq 1) {
            if (-not $directSourceConsumerMap.ContainsKey($includedPath)) {
                $directSourceConsumerMap[$includedPath] = New-Object System.Collections.Generic.HashSet[string]([StringComparer]::OrdinalIgnoreCase)
            }
            [void]$directSourceConsumerMap[$includedPath].Add($currentSource)
        }
    }

    [void]$showIncludesDetails.Add([pscustomobject][ordered]@{
        path = $showIncludesPath
        detected_build_attempts = $(if ($buildBoundaries.Count) { $buildBoundaries.Count } else { 1 })
        selected_build_attempt = $(if ($buildBoundaries.Count) { $buildBoundaries.Count } else { 1 })
        captured_sources = $capturedSources.Count
        direct_edges = $records
        ambiguous_source_markers = $ambiguousSourceMarkers
    })
}

# /Bt+ emits one line per compiler stage, for example:
# time(...\c1xx.dll)=3.70688s < ... > BB [D:\repo\source.cpp]
# Sum all stages per source within one log, then average complete per-log samples.
$timingSamples = @{}
$timingStageRecords = 0
$timingLogDetails = New-Object System.Collections.Generic.List[object]
foreach ($timingPath in $absoluteTimingLogs) {
    $lines = @(Get-Content -LiteralPath $timingPath)
    $buildBoundaries = @()
    for ($lineIndex = 0; $lineIndex -lt $lines.Count; $lineIndex++) {
        if ($lines[$lineIndex] -match '^>\s+.*cmake(?:\.exe)?\s+--build\s+') {
            $buildBoundaries += $lineIndex
        }
    }
    $selectedStart = if ($buildBoundaries.Count) { $buildBoundaries[-1] } else { 0 }
    $perLogTotals = @{}
    $perLogStageRecords = 0
    for ($lineIndex = $selectedStart; $lineIndex -lt $lines.Count; $lineIndex++) {
        $line = $lines[$lineIndex]
        if ($line -notmatch '(?i)time\([^\r\n]+\)=(?<seconds>[0-9]+(?:\.[0-9]+)?)s.*\[(?<source>[A-Za-z]:\\[^\]]+\.(?:c|cc|cpp|cxx))\]\s*$') {
            continue
        }
        $sourceKey = ConvertTo-PathKey $Matches.source
        $seconds = [double]::Parse($Matches.seconds, [Globalization.CultureInfo]::InvariantCulture)
        if (-not $perLogTotals.ContainsKey($sourceKey)) { $perLogTotals[$sourceKey] = 0.0 }
        $perLogTotals[$sourceKey] += $seconds
        $timingStageRecords++
        $perLogStageRecords++
    }
    foreach ($entry in $perLogTotals.GetEnumerator()) {
        if (-not $timingSamples.ContainsKey($entry.Key)) {
            $timingSamples[$entry.Key] = New-Object System.Collections.Generic.List[double]
        }
        $timingSamples[$entry.Key].Add([double]$entry.Value)
    }
    [void]$timingLogDetails.Add([pscustomobject][ordered]@{
        path = $timingPath
        detected_build_attempts = $(if ($buildBoundaries.Count) { $buildBoundaries.Count } else { 1 })
        selected_build_attempt = $(if ($buildBoundaries.Count) { $buildBoundaries.Count } else { 1 })
        ignored_prior_attempts = [math]::Max(0, $buildBoundaries.Count - 1)
        timing_stage_records = $perLogStageRecords
        timed_sources = $perLogTotals.Count
    })
}

$timingMeans = @{}
$timedSources = @($timingSamples.GetEnumerator() | ForEach-Object {
    $mean = ($_.Value | Measure-Object -Average).Average
    $timingMeans[$_.Key] = [double]$mean
    [pscustomobject][ordered]@{
        source = ConvertTo-DisplayPath $_.Key
        samples = $_.Value.Count
        mean_compile_seconds = [math]::Round($mean, 5)
        min_compile_seconds = [math]::Round(($_.Value | Measure-Object -Minimum).Minimum, 5)
        max_compile_seconds = [math]::Round(($_.Value | Measure-Object -Maximum).Maximum, 5)
    }
} | Sort-Object @{ Expression = 'mean_compile_seconds'; Descending = $true }, source)

$allHeaders = @(foreach ($headerEntry in $consumerMap.GetEnumerator()) {
    $consumerKeys = @($headerEntry.Value | Sort-Object)
    $timedConsumerKeys = @(foreach ($consumerKey in $consumerKeys) {
        if ($timingMeans.ContainsKey([string]$consumerKey)) { $consumerKey }
    })
    $displayConsumers = @($consumerKeys | ForEach-Object { ConvertTo-DisplayPath $_ })
    $pchConsumers = @($displayConsumers | Where-Object { $_ -match '(?i)(^|/)cmakefiles/.+\.dir/(?:.*/)?cmake_pch\.(c|cc|cpp|cxx)$' })
    [object[]]$directIncluders = @()
    if ($directIncluderMap.ContainsKey($headerEntry.Key)) {
        $directIncluders = @($directIncluderMap[$headerEntry.Key] | ForEach-Object { ConvertTo-DisplayPath $_ } | Sort-Object)
    }
    [object[]]$directSourceConsumers = @()
    if ($directSourceConsumerMap.ContainsKey($headerEntry.Key)) {
        $directSourceConsumers = @($directSourceConsumerMap[$headerEntry.Key] | ForEach-Object { ConvertTo-DisplayPath $_ } | Sort-Object)
    }
    $coreConsumerCount = @($displayConsumers | Where-Object { $_ -like 'src/libslic3r/*' }).Count
    $guiConsumerCount = @($displayConsumers | Where-Object { $_ -like 'src/slic3r/*' }).Count
    $appConsumerCount = @($displayConsumers | Where-Object { $_ -like 'src/orcaslicer*' }).Count
    $generatedConsumerCount = @($displayConsumers | Where-Object { $_ -like 'build-*' }).Count
    $otherConsumerCount = $displayConsumers.Count - $coreConsumerCount - $guiConsumerCount - $appConsumerCount - $generatedConsumerCount
    $repositoryDomainCount = @($coreConsumerCount, $guiConsumerCount, $appConsumerCount, $otherConsumerCount | Where-Object { $_ -gt 0 }).Count
    $measuredSeconds = if ($absoluteTimingLogs.Count) {
        ($timedConsumerKeys | ForEach-Object { $timingMeans[[string]$_] } | Measure-Object -Sum).Sum
    }
    else { $null }
    [pscustomobject][ordered]@{
        header = ConvertTo-DisplayPath $headerEntry.Key
        transitive_consumers = $consumerKeys.Count
        direct_includer_count = if ($absoluteShowIncludesLogs.Count) { @($directIncluders).Count } else { $null }
        direct_includers = if ($absoluteShowIncludesLogs.Count) { $directIncluders } else { $null }
        direct_source_consumer_count = if ($absoluteShowIncludesLogs.Count) { @($directSourceConsumers).Count } else { $null }
        direct_source_consumers = if ($absoluteShowIncludesLogs.Count) { $directSourceConsumers } else { $null }
        timed_consumers = $timedConsumerKeys.Count
        timing_coverage_pct = if ($consumerKeys.Count -and $absoluteTimingLogs.Count) {
            [math]::Round(100.0 * $timedConsumerKeys.Count / $consumerKeys.Count, 2)
        }
        else { $null }
        measured_invalidation_seconds = if ($null -ne $measuredSeconds) { [math]::Round([double]$measuredSeconds, 5) } else { $null }
        measured_invalidation_minutes = if ($null -ne $measuredSeconds) { [math]::Round([double]$measuredSeconds / 60.0, 3) } else { $null }
        pch_amplified = [bool]$pchConsumers.Count
        pch_consumers = $pchConsumers
        crosses_repository_domains = ($repositoryDomainCount -gt 1)
        consumer_domains = [ordered]@{
            core = $coreConsumerCount
            gui = $guiConsumerCount
            app = $appConsumerCount
            generated_pch = $generatedConsumerCount
            other = $otherConsumerCount
        }
        consumers = $displayConsumers
    }
})

$headers = if ($absoluteTimingLogs.Count) {
    @($allHeaders | Sort-Object @{ Expression = 'measured_invalidation_seconds'; Descending = $true },
        @{ Expression = 'transitive_consumers'; Descending = $true }, header | Select-Object -First $Top)
}
else {
    @($allHeaders | Sort-Object @{ Expression = 'transitive_consumers'; Descending = $true }, header | Select-Object -First $Top)
}

for ($rankIndex = 0; $rankIndex -lt $headers.Count; $rankIndex++) {
    $headers[$rankIndex] | Add-Member -NotePropertyName rank -NotePropertyValue ($rankIndex + 1)
}

$budgetFailed = $false
$budgetResult = $null
if ($absoluteBudgetPath) {
    $budgetDocument = Get-Content -LiteralPath $absoluteBudgetPath -Raw | ConvertFrom-Json
    $budgetChecks = New-Object System.Collections.Generic.List[object]
    foreach ($budget in @($budgetDocument.headers)) {
        $budgetHeader = ([string]$budget.header).Replace('\', '/').ToLowerInvariant()
        $actual = $allHeaders | Where-Object { $_.header -eq $budgetHeader } | Select-Object -First 1
        $violations = New-Object System.Collections.Generic.List[string]
        if (-not $actual) {
            [void]$violations.Add('header was not present in the tracked dependency graph')
        }
        else {
            if ($budget.PSObject.Properties.Name -contains 'max_transitive_consumers') {
                if ($actual.transitive_consumers -gt [int]$budget.max_transitive_consumers) {
                    [void]$violations.Add("transitive consumers $($actual.transitive_consumers) exceed $($budget.max_transitive_consumers)")
                }
            }
            if ($budget.PSObject.Properties.Name -contains 'max_direct_includers') {
                if (-not $absoluteShowIncludesLogs.Count) {
                    [void]$violations.Add('direct includer budget requires -ShowIncludesLog evidence')
                }
                elseif ($actual.direct_includer_count -gt [int]$budget.max_direct_includers) {
                    [void]$violations.Add("direct includers $($actual.direct_includer_count) exceed $($budget.max_direct_includers)")
                }
            }
            if ($budget.PSObject.Properties.Name -contains 'max_direct_source_consumers') {
                if (-not $absoluteShowIncludesLogs.Count) {
                    [void]$violations.Add('direct source-consumer budget requires -ShowIncludesLog evidence')
                }
                elseif ($actual.direct_source_consumer_count -gt [int]$budget.max_direct_source_consumers) {
                    [void]$violations.Add("direct source consumers $($actual.direct_source_consumer_count) exceed $($budget.max_direct_source_consumers)")
                }
            }
        }
        if ($violations.Count) { $budgetFailed = $true }
        [void]$budgetChecks.Add([pscustomobject][ordered]@{
            header = $budgetHeader
            status = $(if ($violations.Count) { 'failed' } else { 'passed' })
            violations = @($violations)
            actual_transitive_consumers = $(if ($actual) { $actual.transitive_consumers } else { $null })
            max_transitive_consumers = $(if ($budget.PSObject.Properties.Name -contains 'max_transitive_consumers') { $budget.max_transitive_consumers } else { $null })
            actual_direct_includers = $(if ($actual) { $actual.direct_includer_count } else { $null })
            max_direct_includers = $(if ($budget.PSObject.Properties.Name -contains 'max_direct_includers') { $budget.max_direct_includers } else { $null })
            actual_direct_source_consumers = $(if ($actual) { $actual.direct_source_consumer_count } else { $null })
            max_direct_source_consumers = $(if ($budget.PSObject.Properties.Name -contains 'max_direct_source_consumers') { $budget.max_direct_source_consumers } else { $null })
        })
    }
    $budgetResult = [ordered]@{
        path = $absoluteBudgetPath
        status = $(if ($budgetFailed) { 'failed' } else { 'passed' })
        checks = $budgetChecks
    }
}

$semantics = if ($absoluteTimingLogs.Count -and $absoluteShowIncludesLogs.Count) {
    'MSVC tracked transitive dependencies weighted by mean observed /Bt+ frontend/backend time, plus compiler-observed /showIncludes direct edges within the captured source scope. Measured invalidation time is the sum over timed consumers; coverage below 100% is not extrapolated.'
}
elseif ($absoluteTimingLogs.Count) {
    'MSVC tracked transitive dependencies weighted by mean observed /Bt+ frontend/backend time. Measured invalidation time is the sum over timed consumers; coverage below 100% is not extrapolated.'
}
elseif ($absoluteShowIncludesLogs.Count) {
    'MSVC tracked transitive dependencies plus compiler-observed /showIncludes direct edges within the captured source scope; compile-cost weights are unavailable without -TimingLog containing /Bt+ output.'
}
else {
    'MSVC tracked transitive dependencies; direct include edges and compile-cost weights are unavailable without -TimingLog containing /Bt+ output.'
}

[ordered]@{
    generated_at = (Get-Date).ToString('o')
    source_commit = (& git -C $repoRoot rev-parse HEAD)
    source_dirty = [bool](& git -C $repoRoot status --porcelain)
    build_directory = $absoluteBuildDirectory
    header_root = $absoluteHeaderRoot
    tracking_logs = $tlogs.Count
    tracked_headers = $consumerMap.Count
    tracked_edges = $edgeCount
    timing_logs = $absoluteTimingLogs
    show_includes_logs = $absoluteShowIncludesLogs
    show_includes_log_details = $showIncludesDetails
    direct_evidence_source_count = $directEvidenceSources.Count
    direct_edge_count = $directEdgeSet.Count
    timing_log_details = $timingLogDetails
    timing_stage_records = $timingStageRecords
    timed_source_count = $timingMeans.Count
    semantics = $semantics
    budget = $budgetResult
    timed_sources = $timedSources
    headers = $headers
} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $OutputPath -Encoding UTF8

Write-Host "Include dependency summary: $OutputPath" -ForegroundColor Green
if ($budgetFailed) {
    Write-Host "One or more header dependency budgets were exceeded. See $OutputPath." -ForegroundColor Red
    exit 2
}
