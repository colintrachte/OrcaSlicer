<#
.SYNOPSIS
Builds OrcaSlicer with target-selectable developer loops and timestamped diagnostics.

.EXAMPLE
.\scripts\build.ps1 -Configuration Release -Architecture x64

.EXAMPLE
.\scripts\build.ps1 -SlicerOnly -Parallel 8

.EXAMPLE
.\scripts\build.ps1 -Target App -BuildOnly -Parallel 0 -MaxParallel 6

.EXAMPLE
.\scripts\build.ps1 -Target Gui -NoConfigure -DiagnoseRebuild

.EXAMPLE
.\scripts\build.ps1 -PreflightOnly
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string] $Configuration = 'Release',

    [ValidateSet('x64', 'ARM64')]
    [string] $Architecture,

    [Alias('x')]
    [switch] $Ninja,

    [switch] $DependenciesOnly,
    [switch] $SlicerOnly,
    [Alias('SkipDeps')]
    [switch] $SkipDependencies,
    [switch] $Pack,
    [switch] $Run,
    [switch] $PreflightOnly,
    [switch] $NoPause,

    [string] $Target,
    [switch] $BuildOnly,
    [switch] $Install,
    [switch] $Configure,
    [switch] $NoConfigure,
    [switch] $Fresh,
    [switch] $DiagnoseRebuild,
    [switch] $SafeBuild,
    [switch] $CleanFirst,

    [ValidateSet('Auto', 'Clean', 'Warm', 'InterruptedRecovery')]
    [string] $BuildState = 'Auto',

    [string] $ChangedInput,

    [string] $BuildDirectory,

    [string] $DependencyBuildDirectory,

    [ValidateRange(0, 128)]
    [int] $Parallel = 0,

    [ValidateRange(1, 128)]
    [int] $MaxParallel = 6,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $LegacyArguments
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$script:StartTime = Get-Date
$script:PhaseTimes = [ordered]@{}
$script:CurrentPhase = $null
$script:FailedPhase = $null
$script:ExitCode = 0
$script:LogFiles = @()
$script:BuildAttempts = @()
$script:BinaryLog = $null
$script:RebuildSummary = $null

function Write-Section([string] $Message) {
    Write-Host "`n=== $Message ===" -ForegroundColor Cyan
}

function Invoke-LoggedCommand {
    param(
        [Parameter(Mandatory = $true)][string] $Phase,
        [Parameter(Mandatory = $true)][string] $FilePath,
        [Parameter(Mandatory = $true)][string[]] $Arguments,
        [Parameter(Mandatory = $true)][string] $LogPath,
        [string] $WorkingDirectory = $script:RepoRoot,
        [bool] $SuppressConsoleOutput = $false
    )

    $started = Get-Date
    $script:CurrentPhase = $Phase
    $display = $FilePath + ' ' + (($Arguments | ForEach-Object {
        if ($_ -match '\s') { '"' + $_ + '"' } else { $_ }
    }) -join ' ')
    Add-Content -LiteralPath $LogPath -Value "`r`n> $display"
    Write-Host "[$Phase] $display" -ForegroundColor DarkGray

    Push-Location $WorkingDirectory
    $previousErrorActionPreference = $ErrorActionPreference
    $writer = $null
    try {
        # Windows PowerShell wraps native stderr as ErrorRecord objects. With the
        # script-wide Stop policy, harmless compiler/CMake warnings would otherwise
        # terminate the pipeline before the native exit code can be inspected.
        $ErrorActionPreference = 'Continue'
        $utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
        $writer = New-Object System.IO.StreamWriter($LogPath, $true, $utf8WithoutBom)
        & $FilePath @Arguments 2>&1 | ForEach-Object {
            $line = $_.ToString()
            $writer.WriteLine($line)
            if (-not $SuppressConsoleOutput) { Write-Host $line }
        }
        $code = $LASTEXITCODE
    }
    finally {
        if ($writer) { $writer.Dispose() }
        $ErrorActionPreference = $previousErrorActionPreference
        Pop-Location
    }

    $script:PhaseTimes[$Phase] = (Get-Date) - $started
    if ($null -eq $code) { $code = 0 }
    return [int]$code
}

function Assert-CommandSucceeded([int] $Code, [string] $Phase) {
    if ($Code -ne 0) {
        throw "$Phase failed with exit code $Code."
    }
}

function Get-CachedCMakeValue([string] $BuildDirectory, [string] $Name) {
    $cache = Join-Path $BuildDirectory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cache)) { return $null }
    $match = Select-String -LiteralPath $cache -Pattern ("^{0}:INTERNAL=(.*)$" -f [regex]::Escape($Name)) | Select-Object -First 1
    if ($match) { return $match.Matches[0].Groups[1].Value }
    return $null
}

function Get-FreshArgument([string] $BuildDirectory, [string] $Generator, [string] $Platform) {
    $cachedGenerator = Get-CachedCMakeValue $BuildDirectory 'CMAKE_GENERATOR'
    $cachedPlatform = Get-CachedCMakeValue $BuildDirectory 'CMAKE_GENERATOR_PLATFORM'
    if ($cachedGenerator -and $cachedGenerator -ne $Generator) {
        Write-Host "Refreshing incompatible CMake cache: '$cachedGenerator' -> '$Generator'"
        return @('--fresh')
    }
    if ($Platform -and $cachedPlatform -and $cachedPlatform -ne $Platform) {
        Write-Host "Refreshing incompatible CMake platform: '$cachedPlatform' -> '$Platform'"
        return @('--fresh')
    }
    return @()
}

function Export-Diagnostics {
    param([string[]] $Logs, [string] $WarningsPath, [string] $ErrorsPath)

    $warningPattern = '(?i)(\bwarning\s+(C|LNK|MSB)\d+|:\s*warning\b|CMake Warning)'
    $errorPattern = '(?i)(fatal error|error\s+(C|D|LNK|MSB)\d+|MSB\d+:\s*error|CMake Error|FAILED:|exited with code|PowerShell build driver error)'
    $warnings = New-Object System.Collections.Generic.List[string]
    $errors = New-Object System.Collections.Generic.List[string]

    foreach ($log in $Logs) {
        if (-not (Test-Path -LiteralPath $log)) { continue }
        foreach ($line in Get-Content -LiteralPath $log) {
            if ($line -match $errorPattern) { $errors.Add($line) }
            elseif ($line -match $warningPattern) { $warnings.Add($line) }
        }
    }

    [System.IO.File]::WriteAllLines($WarningsPath, $warnings, [System.Text.Encoding]::UTF8)
    [System.IO.File]::WriteAllLines($ErrorsPath, $errors, [System.Text.Encoding]::UTF8)
    return [pscustomobject]@{ Warnings = $warnings.Count; Errors = $errors.Count }
}

function Format-Duration([TimeSpan] $Duration) {
    return '{0}h {1}m {2}s' -f [math]::Floor($Duration.TotalHours), $Duration.Minutes, $Duration.Seconds
}

function Normalize-ProcessPathEnvironment {
    # Windows environment blocks may contain both PATH and Path. .NET treats those
    # names case-insensitively and MSBuild then crashes while constructing the
    # environment for cl.exe. Keep the conventional Path entry when both exist.
    $pathEntries = @(& $env:ComSpec /d /c 'set path')
    $hasUpperPath = [bool]($pathEntries | Where-Object { $_ -cmatch '^PATH=' } | Select-Object -First 1)
    $hasMixedPath = [bool]($pathEntries | Where-Object { $_ -cmatch '^Path=' } | Select-Object -First 1)
    if ($hasUpperPath -and $hasMixedPath) {
        Remove-Item Env:PATH -ErrorAction Stop
        Write-Host 'Normalized duplicate PATH/Path process environment entries.'
    }
}

function Resolve-VisualStudio {
    $vswhereCandidates = @(@(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) })
    if (-not $vswhereCandidates) { throw 'Visual Studio Installer (vswhere.exe) was not found.' }

    $vswhere = $vswhereCandidates[0]
    $installPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
    $installVersion = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion).Trim()
    if (-not $installPath -or -not $installVersion) { throw 'No Visual Studio installation with the C++ toolchain was found.' }

    $major = [int]($installVersion.Split('.')[0])
    if ($major -ne 18) {
        throw "Visual Studio 2026 with the C++ toolchain is required; latest detected installation is $installVersion at $installPath."
    }
    $year = switch ($major) { 16 { '2019' } 17 { '2022' } 18 { '2026' } default { $major.ToString() } }
    $cmake = Join-Path $installPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (-not (Test-Path -LiteralPath $cmake)) {
        throw "Visual Studio 2026's bundled CMake was not found: $cmake"
    }
    $msbuild = Join-Path $installPath 'MSBuild\Current\Bin\MSBuild.exe'
    if (-not (Test-Path -LiteralPath $msbuild)) {
        throw "Visual Studio 2026's MSBuild was not found: $msbuild"
    }
    $ninja = Join-Path $installPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'

    return [pscustomobject]@{
        InstallPath = $installPath
        InstallVersion = $installVersion
        Major = $major
        Year = $year
        Generator = "Visual Studio $major $year"
        CMake = $cmake
        MSBuild = $msbuild
        Ninja = $ninja
    }
}

function Initialize-VisualStudioEnvironment {
    param([string] $InstallPath, [string] $TargetArchitecture)

    $vcvars = Join-Path $InstallPath 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path -LiteralPath $vcvars)) { throw "Visual Studio environment script was not found: $vcvars" }
    $vcvarsArchitecture = if ($TargetArchitecture -eq 'ARM64') { 'x64_arm64' } else { 'x64' }
    $command = "call `"$vcvars`" $vcvarsArchitecture >nul && set"
    $environment = & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw "vcvarsall.bat failed for $vcvarsArchitecture with exit code $LASTEXITCODE." }
    foreach ($entry in $environment) {
        $separator = $entry.IndexOf('=')
        if ($separator -gt 0) {
            [Environment]::SetEnvironmentVariable($entry.Substring(0, $separator), $entry.Substring($separator + 1), 'Process')
        }
    }
}

function Get-RecommendedParallelism {
    try {
        $system = Get-CimInstance Win32_ComputerSystem
        $memoryGB = [math]::Floor($system.TotalPhysicalMemory / 1GB)
        $processors = [int]$system.NumberOfLogicalProcessors
        # GUI PCH compilation can consume several GB of committed memory per cl.exe.
        # Eight GB per worker leaves headroom for linking, the OS, and the IDE,
        # including systems configured with a small paging file.
        return [math]::Max(1, [math]::Min($processors, [math]::Floor($memoryGB / 8)))
    }
    catch {
        return 2
    }
}

function Resolve-BuildTarget([string] $RequestedTarget, [bool] $TargetIsWindows) {
    # Parameter deliberately not named $IsWindows: that collides with PowerShell's own
    # read-only automatic variable of the same name and fails at call time with
    # "Cannot overwrite variable IsWindows because it is read-only or constant."
    if (-not $RequestedTarget) { return $null }
    switch ($RequestedTarget.ToLowerInvariant()) {
        'core' { return 'libslic3r' }
        'gui'  { return 'libslic3r_gui' }
        'app'  { return $(if ($TargetIsWindows) { 'OrcaSlicer_app_gui' } else { 'OrcaSlicer' }) }
        default { return $RequestedTarget }
    }
}

function Get-ParallelHistory([string] $Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return @() }
    try {
        $parsed = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
        foreach ($entry in @($parsed)) {
            if ($entry.PSObject.Properties['key']) {
                Write-Output $entry
            }
            elseif ($entry.PSObject.Properties['value']) {
                foreach ($nested in @($entry.value)) {
                    if ($nested.PSObject.Properties['key']) { Write-Output $nested }
                }
            }
        }
    }
    catch {
        Write-Warning "Ignoring invalid parallel history: $Path"
        return @()
    }
}

function ConvertTo-ParallelHistoryTimestamp($Value) {
    try { return [DateTimeOffset]$Value }
    catch { return [DateTimeOffset]::MinValue }
}

function Get-LearnedParallelCeiling {
    param([string] $Path, [string] $Key, [int] $AutomaticCeiling)

    $matching = @(Get-ParallelHistory $Path | Where-Object { $_.key -eq $Key } |
        Sort-Object { ConvertTo-ParallelHistoryTimestamp $_.timestamp })
    if (-not $matching.Count) { return [math]::Max(1, $AutomaticCeiling) }

    # A resource failure is evidence about the conditions of that run, not a lifetime ban on the
    # ceiling. Three later successes at the same ceiling rehabilitate it. This prevents a single
    # stale D8040/PCH failure from forcing all future builds to one compiler process even after the
    # controlled benchmark and subsequent builds repeatedly prove the higher ceiling stable.
    $recoverySuccesses = 3
    $unresolvedUnsafe = New-Object System.Collections.Generic.List[int]
    foreach ($failure in @($matching | Where-Object { $_.memory_failure })) {
        $failureTime = ConvertTo-ParallelHistoryTimestamp $failure.timestamp
        $laterSuccessCount = @($matching | Where-Object {
            $_.success -and [int]$_.parallel -eq [int]$failure.parallel -and
            (ConvertTo-ParallelHistoryTimestamp $_.timestamp) -gt $failureTime
        }).Count
        if ($laterSuccessCount -lt $recoverySuccesses) {
            [void]$unresolvedUnsafe.Add([int]$failure.parallel)
        }
    }

    if ($unresolvedUnsafe.Count) {
        $AutomaticCeiling = [math]::Min(
            $AutomaticCeiling,
            ([int]($unresolvedUnsafe | Measure-Object -Minimum).Minimum) - 1)
    }

    # Once history exists, do not jump from a merely hardware-derived estimate to an untested
    # ceiling. Use the highest ceiling demonstrated by at least three successful builds. Explicit
    # -Parallel remains available for a controlled higher-ceiling benchmark.
    $proven = @($matching | Where-Object { $_.success } | Group-Object { [int]$_.parallel } |
        Where-Object { $_.Count -ge $recoverySuccesses } | ForEach-Object { [int]$_.Name })
    if ($proven.Count) {
        $AutomaticCeiling = [math]::Min($AutomaticCeiling, [int]($proven | Measure-Object -Maximum).Maximum)
    }

    return [math]::Max(1, $AutomaticCeiling)
}

function Add-ParallelHistory {
    param(
        [string] $Path,
        [string] $Key,
        [int] $Ceiling,
        [bool] $Success,
        [bool] $MemoryFailure,
        [string] $TargetName
    )

    $history = New-Object System.Collections.Generic.List[object]
    foreach ($entry in @(Get-ParallelHistory $Path)) { [void]$history.Add($entry) }
    [void]$history.Add([pscustomobject][ordered]@{
        timestamp = (Get-Date).ToString('o')
        key = $Key
        target = $TargetName
        parallel = $Ceiling
        success = $Success
        memory_failure = $MemoryFailure
    })
    # This is learned build telemetry, not an unbounded event log.
    $historyArray = if ($history.Count -gt 200) { @($history | Select-Object -Last 200) } else { @($history.ToArray()) }
    ConvertTo-Json -InputObject $historyArray -Depth 4 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Get-ProvenParallelCeilingBelow {
    param([string] $Path, [string] $Key, [int] $CurrentCeiling)

    $recoverySuccesses = 3
    $provenBelow = @(Get-ParallelHistory $Path | Where-Object {
        $_.key -eq $Key -and $_.success -and [int]$_.parallel -lt $CurrentCeiling
    } | Group-Object { [int]$_.parallel } | Where-Object {
        $_.Count -ge $recoverySuccesses
    } | ForEach-Object { [int]$_.Name })
    if (-not $provenBelow.Count) { return $null }
    return [int]($provenBelow | Measure-Object -Maximum).Maximum
}

function Test-CompilerMemoryFailure([string] $LogPath, [long] $StartOffset = 0) {
    if (-not (Test-Path -LiteralPath $LogPath)) { return $false }

    $patterns = @(
        '(?i)\b(C1060|C1076|C3859|D8040|LNK1102)\b',
        '(?i)(out of (heap space|memory)|compiler.*out of memory|not enough memory|insufficient system resources)',
        '(?i)(paging file is too small|commit limit|system returned code 1455|STATUS_NO_MEMORY|0xC0000017|0xC000012D)',
        '(?i)(std::bad_alloc|bad allocation)'
    )

    # Retries append to the same phase log. Only inspect output from this attempt;
    # otherwise a memory error from attempt one can misclassify an unrelated error
    # from a later attempt and trigger progressively smaller, misleading retries.
    $stream = [System.IO.File]::Open($LogPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    $reader = $null
    try {
        [void]$stream.Seek([math]::Min([math]::Max(0, $StartOffset), $stream.Length), [System.IO.SeekOrigin]::Begin)
        $reader = New-Object System.IO.StreamReader($stream, [System.Text.Encoding]::UTF8, $true, 4096, $true)
        while (-not $reader.EndOfStream) {
            if ($reader.ReadLine() -match ($patterns -join '|')) { return $true }
        }
        return $false
    }
    finally {
        if ($reader) { $reader.Dispose() }
        $stream.Dispose()
    }
}

function Invoke-BoundedBuild {
    param(
        [string] $Phase,
        [string] $CMakePath,
        [string[]] $BaseArguments,
        [string] $LogPath,
        [string] $WorkingDirectory,
        [int] $CompilerCeiling,
        [bool] $UsesMSBuild,
        [string] $HistoryPath,
        [string] $HistoryKey,
        [string] $TargetName,
        [string] $BinaryLogPath
    )

    $ceiling = $CompilerCeiling
    $attempt = 0
    while ($true) {
        $attempt++
        $attemptPhase = if ($attempt -eq 1) { $Phase } else { "$Phase retry" }
        $arguments = @($BaseArguments)
        if ($UsesMSBuild) {
            # A single MSBuild project worker prevents /m:P and /MP:C from
            # multiplying into P x C compiler processes.
            $arguments += @('--parallel', '1', '--', "/p:CL_MPCount=$ceiling", '/nr:false', '/v:minimal')
            if ($DiagnoseRebuild) { $arguments[-1] = '/v:diagnostic' }
            if ($BinaryLogPath) { $arguments += "/bl:$BinaryLogPath" }
        }
        else {
            $arguments += @('--parallel', $ceiling.ToString())
        }

        if ($DiagnoseRebuild -and $UsesMSBuild) {
            Write-Host "MSBuild diagnostic output is being captured in $LogPath"
        }
        $attemptLogOffset = if (Test-Path -LiteralPath $LogPath) { (Get-Item -LiteralPath $LogPath).Length } else { 0 }
        $code = Invoke-LoggedCommand $attemptPhase $CMakePath $arguments $LogPath $WorkingDirectory ($DiagnoseRebuild -and $UsesMSBuild)
        $memoryFailure = ($code -ne 0) -and (Test-CompilerMemoryFailure $LogPath $attemptLogOffset)
        $script:BuildAttempts += [pscustomobject][ordered]@{
            phase = $Phase
            attempt = $attempt
            target = $TargetName
            compiler_ceiling = $ceiling
            msbuild_ceiling = $(if ($UsesMSBuild) { 1 } else { $null })
            exit_code = $code
            memory_failure = $memoryFailure
        }
        Add-ParallelHistory $HistoryPath $HistoryKey $ceiling ($code -eq 0) $memoryFailure $TargetName

        if ($code -eq 0) { return 0 }
        if (-not $memoryFailure -or $ceiling -le 1) { return $code }

        $provenFallback = Get-ProvenParallelCeilingBelow $HistoryPath $HistoryKey $ceiling
        $ceiling = if ($null -ne $provenFallback) {
            $provenFallback
        }
        else {
            [math]::Max(1, [math]::Floor($ceiling / 2))
        }
        Write-Warning "$Phase exhausted compiler memory; retrying with compiler parallelism $ceiling."
    }
}

function Export-RebuildSummary([string] $LogPath, [string] $SummaryPath) {
    $patterns = @(
        '(?i)not up[- ]to[- ]date',
        '(?i)out of date',
        '(?i)will be built because',
        '(?i)source compilation required',
        '(?i)input file .* is newer than output file',
        '(?i)tracking log.*(missing|invalid|not found|unavailable|incomplete|does not exist)'
    )
    $matches = @(Select-String -LiteralPath $LogPath -Pattern $patterns | ForEach-Object { $_.Line.Trim() } | Select-Object -Unique)
    [System.IO.File]::WriteAllLines($SummaryPath, $matches, [System.Text.Encoding]::UTF8)
    return $matches.Count
}

function Get-BuildCommandFingerprint([string] $BuildDirectory, [string] $ResolvedTarget) {
    if (-not (Test-Path -LiteralPath $BuildDirectory)) { return $null }
    $candidates = @()
    if ($ResolvedTarget) {
        $project = Get-ChildItem -LiteralPath $BuildDirectory -Filter "$ResolvedTarget.vcxproj" -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($project) { $candidates += $project.FullName }
    }
    $cache = Join-Path $BuildDirectory 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cache) { $candidates += $cache }
    if (-not $candidates.Count) { return $null }
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        foreach ($candidate in $candidates) {
            $bytes = [System.IO.File]::ReadAllBytes($candidate)
            [void]$sha.TransformBlock($bytes, 0, $bytes.Length, $bytes, 0)
        }
        [void]$sha.TransformFinalBlock([byte[]]@(), 0, 0)
        return ([BitConverter]::ToString($sha.Hash)).Replace('-', '').ToLowerInvariant()
    }
    finally { $sha.Dispose() }
}

function Get-CachedCMakeEntry([string] $BuildDirectory, [string] $Name) {
    $cache = Join-Path $BuildDirectory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cache)) { return $null }
    $match = Select-String -LiteralPath $cache -Pattern ("^{0}:[^=]+=(.*)$" -f [regex]::Escape($Name)) | Select-Object -First 1
    if ($match) { return $match.Matches[0].Groups[1].Value }
    return $null
}

function Get-CMakeCompilerEntry([string] $BuildDirectory, [string] $Name) {
    $compilerFile = Get-ChildItem -LiteralPath (Join-Path $BuildDirectory 'CMakeFiles') -Filter 'CMakeCXXCompiler.cmake' -Recurse -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $compilerFile) { return $null }
    $match = Select-String -LiteralPath $compilerFile.FullName -Pattern ('^set\({0}\s+"?([^"\)]+)' -f [regex]::Escape($Name)) | Select-Object -First 1
    if ($match) { return $match.Matches[0].Groups[1].Value }
    return $null
}

function Get-RecompiledSourceCount([string] $LogPath) {
    if (-not (Test-Path -LiteralPath $LogPath)) { return $null }
    $sources = New-Object System.Collections.Generic.HashSet[string]([StringComparer]::OrdinalIgnoreCase)
    foreach ($line in Get-Content -LiteralPath $LogPath) {
        if ($line -match '^\s*(?<source>[^\s]+\.(c|cc|cpp|cxx))\s*$') {
            [void]$sources.Add($Matches.source)
        }
        elseif ($line -match '(?i)Building CXX object .*[/\\](?<source>[^/\\]+\.(c|cc|cpp|cxx))\.obj') {
            [void]$sources.Add($Matches.source)
        }
    }
    return $sources.Count
}

function Test-PCHRegenerated([string] $LogPath) {
    if (-not (Test-Path -LiteralPath $LogPath)) { return $false }
    foreach ($line in Get-Content -LiteralPath $LogPath) {
        if ($line -match '(?i)^\s*cmake_pch\.(c|cc|cpp|cxx)\s*$' -or $line -match '(?i)Building CXX object .*cmake_pch\.(c|cc|cpp|cxx)\.obj') {
            return $true
        }
    }
    return $false
}

function Test-DependencyStamp {
    param([string] $StampPath, [string] $ExpectedGenerator, [string] $ExpectedArchitecture, [string] $ExpectedConfiguration)
    if (-not (Test-Path -LiteralPath $StampPath)) { return $false }
    try {
        $stamp = Get-Content -LiteralPath $StampPath -Raw | ConvertFrom-Json
        return $stamp.completed -and
            $stamp.generator -eq $ExpectedGenerator -and
            $stamp.architecture -eq $ExpectedArchitecture -and
            $stamp.configuration -eq $ExpectedConfiguration
    }
    catch { return $false }
}

# Preserve the old batch-file arguments: debug, debuginfo, x64, arm64, -x,
# deps, slicer, and pack.
foreach ($argument in $LegacyArguments) {
    switch ($argument.ToLowerInvariant()) {
        'debug'     { $Configuration = 'Debug' }
        'debuginfo' { $Configuration = 'RelWithDebInfo' }
        'x64'       { $Architecture = 'x64' }
        'arm64'     { $Architecture = 'ARM64' }
        'deps'      { $DependenciesOnly = $true }
        'slicer'    { $SlicerOnly = $true }
        'pack'      { $Pack = $true }
        '-x'        { $Ninja = $true }
        default     { throw "Unknown argument: $argument" }
    }
}

if ($DependenciesOnly -and $SlicerOnly) { throw 'DependenciesOnly and SlicerOnly cannot be used together.' }
if ($Configure -and $NoConfigure) { throw 'Configure and NoConfigure cannot be used together.' }
if ($Fresh -and $NoConfigure) { throw 'Fresh and NoConfigure cannot be used together.' }
if ($BuildOnly -and $Install) { throw 'BuildOnly and Install cannot be used together.' }
if ($Target -and $DependenciesOnly) { throw 'Target cannot be combined with DependenciesOnly.' }
if ($Target) { $SlicerOnly = $true }
if ($Fresh) { $Configure = $true }
if (-not $Architecture) {
    $Architecture = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64' -or $env:PROCESSOR_ARCHITEW6432 -eq 'ARM64') { 'ARM64' } else { 'x64' }
}
if ($env:MP_CAP) {
    $parsedCap = 0
    if ([int]::TryParse($env:MP_CAP, [ref]$parsedCap) -and $parsedCap -gt 0) { $Parallel = $parsedCap }
}
$parallelWasAutomatic = ($Parallel -le 0)
if ($SafeBuild) {
    $Parallel = 1
    $parallelWasAutomatic = $false
}

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$logDirectory = Join-Path $script:RepoRoot 'logs'
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$depsLog = Join-Path $logDirectory "deps_${Architecture}_${Configuration}_${stamp}.log"
$slicerLog = Join-Path $logDirectory "slicer_${Architecture}_${Configuration}_${stamp}.log"
$packLog = Join-Path $logDirectory "pack_${Architecture}_${stamp}.log"
$warningsLog = Join-Path $logDirectory "warnings_${Architecture}_${Configuration}_${stamp}.log"
$errorsLog = Join-Path $logDirectory "errors_${Architecture}_${Configuration}_${stamp}.log"
$manifestPath = Join-Path $logDirectory "build_${Architecture}_${Configuration}_${stamp}.json"
$parallelHistoryPath = Join-Path $logDirectory 'build_parallel_history.json'
$binaryLogPath = Join-Path $logDirectory "msbuild_${Architecture}_${Configuration}_${stamp}.binlog"
$rebuildSummaryPath = Join-Path $logDirectory "rebuild_${Architecture}_${Configuration}_${stamp}.txt"

try {
    Write-Section 'Preflight'
    Normalize-ProcessPathEnvironment
    if ($Ninja) {
        $vs = Resolve-VisualStudio
        Initialize-VisualStudioEnvironment $vs.InstallPath $Architecture
        $generator = 'Ninja Multi-Config'
        $vsYear = 'Ninja'
        $cmake = $vs.CMake
        $msbuild = $null
        $ninjaExe = $vs.Ninja
        if (-not (Test-Path -LiteralPath $ninjaExe)) { throw "Visual Studio's bundled Ninja was not found: $ninjaExe" }
        Write-Host "Visual Studio $($vs.Year): $($vs.InstallVersion)"
    }
    else {
        $vs = Resolve-VisualStudio
        $generator = $vs.Generator
        $vsYear = $vs.Year
        $cmake = $vs.CMake
        $msbuild = $vs.MSBuild
        Initialize-VisualStudioEnvironment $vs.InstallPath $Architecture
        Write-Host "Visual Studio $($vs.Year): $($vs.InstallVersion)"
    }
    if (-not (Get-Command $cmake -ErrorAction SilentlyContinue)) { throw "CMake was not found: $cmake" }
    if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) { throw 'Git was not found on PATH.' }
    if (-not $SlicerOnly -and -not $SkipDependencies -and -not $Pack) {
        if (-not (Get-Command perl.exe -ErrorAction SilentlyContinue)) {
            throw 'Perl was not found on PATH. Install Strawberry Perl to build OpenSSL and the other dependencies.'
        }
        Write-Host "Perl          : $((Get-Command perl.exe).Source)"
    }

    $baseBuildDirectory = switch ($Configuration) {
        'Debug'          { 'build-dbg' }
        'RelWithDebInfo' { 'build-dbginfo' }
        default          { 'build' }
    }
    if ($Architecture -eq 'ARM64') { $baseBuildDirectory += '-arm64' }
    if (-not $Ninja -and $vsYear -ne '2022') { $baseBuildDirectory += "-vs$vsYear" }
    if ($Ninja) { $baseBuildDirectory += '-ninja' }

    $depsSource = Join-Path $script:RepoRoot 'deps'
    if ($DependencyBuildDirectory) {
        $depsBuild = if ([System.IO.Path]::IsPathRooted($DependencyBuildDirectory)) { $DependencyBuildDirectory } else { Join-Path $script:RepoRoot $DependencyBuildDirectory }
        $depsBuild = [System.IO.Path]::GetFullPath($depsBuild)
    }
    else {
        $depsBuild = Join-Path $depsSource $baseBuildDirectory
    }
    if ($BuildDirectory) {
        $slicerBuild = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $script:RepoRoot $BuildDirectory }
        $slicerBuild = [System.IO.Path]::GetFullPath($slicerBuild)
    }
    else {
        $slicerBuild = Join-Path $script:RepoRoot $baseBuildDirectory
    }
    $gettext = Join-Path $script:RepoRoot 'scripts\run_gettext.bat'
    $sevenZip = Join-Path $script:RepoRoot 'tools\7z.exe'
    $dependencyStamp = Join-Path $depsBuild '.orca-deps-complete.json'
    $signatureArgument = if ($env:ORCA_UPDATER_SIG_KEY) { @("-DORCA_UPDATER_SIG_KEY=$($env:ORCA_UPDATER_SIG_KEY)") } else { @() }
    $resolvedTarget = Resolve-BuildTarget $Target $true
    $developerMode = [bool]($Target -or $BuildOnly -or $Configure -or $NoConfigure -or $Fresh)
    $legacyFullWorkflow = -not $developerMode -and -not $Install
    $targetLabel = if ($resolvedTarget) { $resolvedTarget } else { 'ALL_BUILD' }
    $toolsetLabel = if ($Ninja) { "$($vs.InstallVersion)|$(& $cmake --version | Select-Object -First 1)|ninja $(& $ninjaExe --version)" } else { $vs.InstallVersion }
    $parallelHistoryKey = "$targetLabel|$toolsetLabel|$Architecture|$Configuration|$generator"
    if ($parallelWasAutomatic) {
        $automaticCeiling = [math]::Min((Get-RecommendedParallelism), $MaxParallel)
        $Parallel = Get-LearnedParallelCeiling $parallelHistoryPath $parallelHistoryKey $automaticCeiling
    }
    elseif ($Parallel -le 0) {
        $Parallel = 1
    }

    if ($Run -and $resolvedTarget -and $resolvedTarget -notin @('OrcaSlicer', 'OrcaSlicer_app_gui')) {
        throw "Run requires the App alias or an OrcaSlicer application target; resolved target is '$resolvedTarget'."
    }

    Write-Host "Configuration : $Configuration"
    Write-Host "Architecture  : $Architecture"
    Write-Host "Generator     : $generator"
    Write-Host "Build folder  : $slicerBuild"
    Write-Host "Target        : $targetLabel"
    Write-Host "Compiler cap  : $Parallel$(if ($parallelWasAutomatic) { ' (automatic)' } else { '' })"
    if (-not $Ninja) { Write-Host 'MSBuild cap   : 1' }
    Write-Host "CMake         : $cmake"
    if ($Ninja) { Write-Host "Ninja         : $ninjaExe" }

    if ($PreflightOnly) {
        Write-Host 'Preflight completed; no build was requested.' -ForegroundColor Green
    }
    elseif ($Pack) {
        Write-Section 'Package dependencies'
        $packageRoot = Join-Path $depsBuild 'OrcaSlicer_dep'
        if (-not (Test-Path -LiteralPath $packageRoot)) { throw "Dependency package directory was not found: $packageRoot" }
        if (-not (Test-Path -LiteralPath $sevenZip)) { throw "7z.exe was not found: $sevenZip" }
        $archive = "OrcaSlicer_dep_win-${Architecture}_$(Get-Date -Format yyyyMMdd)_vs${vsYear}.zip"
        $script:LogFiles += $packLog
        Assert-CommandSucceeded (Invoke-LoggedCommand 'Package' $sevenZip @('a', $archive, 'OrcaSlicer_dep') $packLog $depsBuild) 'Package'
    }
    else {
        if ($SkipDependencies) {
            if (-not (Test-DependencyStamp $dependencyStamp $generator $Architecture $Configuration)) {
                throw "Cannot skip dependencies: a matching completion stamp was not found at $dependencyStamp"
            }
            Write-Host "Skipping dependencies; verified completion stamp: $dependencyStamp" -ForegroundColor Green
        }
        elseif (-not $SlicerOnly) {
            Write-Section 'Configure dependencies'
            $script:LogFiles += $depsLog
            Set-Content -LiteralPath $depsLog -Value "=== dependencies $(Get-Date -Format o) ==="
            $freshArguments = Get-FreshArgument $depsBuild $generator $(if ($Ninja) { '' } else { $Architecture })
            $args = @($freshArguments) + @('-S', $depsSource, '-B', $depsBuild, '-G', $generator)
            if ($Ninja) {
                $args += "-DCMAKE_MAKE_PROGRAM=$ninjaExe"
            }
            if (-not $Ninja) { $args += @('-A', $Architecture, "-DCMAKE_BUILD_TYPE=$Configuration") }
            $args += $signatureArgument
            Assert-CommandSucceeded (Invoke-LoggedCommand 'Configure dependencies' $cmake $args $depsLog) 'Configure dependencies'

            Write-Section 'Build dependencies'
            $args = @('--build', $depsBuild, '--config', $Configuration, '--target', 'deps')
            $depsHistoryKey = "deps|$toolsetLabel|$Architecture|$Configuration|$generator"
            Assert-CommandSucceeded (Invoke-BoundedBuild 'Build dependencies' $cmake $args $depsLog $depsSource $Parallel (-not $Ninja) $parallelHistoryPath $depsHistoryKey 'deps' $null) 'Build dependencies'
            [ordered]@{
                completed = $true
                completed_at = (Get-Date).ToString('o')
                generator = $generator
                architecture = $Architecture
                configuration = $Configuration
                git_commit = (& git.exe -C $script:RepoRoot rev-parse HEAD)
            } | ConvertTo-Json | Set-Content -LiteralPath $dependencyStamp -Encoding UTF8
        }

        if (-not $DependenciesOnly) {
            $script:LogFiles += $slicerLog
            Set-Content -LiteralPath $slicerLog -Value "=== OrcaSlicer $(Get-Date -Format o) ==="
            $cachePath = Join-Path $slicerBuild 'CMakeCache.txt'
            if ($NoConfigure -and -not (Test-Path -LiteralPath $cachePath)) {
                throw "NoConfigure requires an existing CMake cache: $cachePath"
            }
            if ($NoConfigure) {
                $cachedGenerator = Get-CachedCMakeValue $slicerBuild 'CMAKE_GENERATOR'
                $cachedPlatform = Get-CachedCMakeValue $slicerBuild 'CMAKE_GENERATOR_PLATFORM'
                if ($cachedGenerator -ne $generator) {
                    throw "NoConfigure build tree uses generator '$cachedGenerator', not '$generator'."
                }
                if (-not $Ninja -and $cachedPlatform -and $cachedPlatform -ne $Architecture) {
                    throw "NoConfigure build tree uses platform '$cachedPlatform', not '$Architecture'."
                }
            }
            $shouldConfigure = $Configure -or $legacyFullWorkflow -or -not (Test-Path -LiteralPath $cachePath)
            if ($NoConfigure) { $shouldConfigure = $false }
            if ($shouldConfigure) {
                Write-Section 'Configure OrcaSlicer'
                $freshArguments = if ($Fresh) { @('--fresh') } else { @(Get-FreshArgument $slicerBuild $generator $(if ($Ninja) { '' } else { $Architecture })) }
                $buildTools = $legacyFullWorkflow -or $Install
                $args = @($freshArguments) + @('-S', $script:RepoRoot, '-B', $slicerBuild, '-G', $generator, "-DORCA_TOOLS=$(if ($buildTools) { 'ON' } else { 'OFF' })", "-DDEP_BUILD_DIR=$depsBuild")
                if ($Ninja) {
                    $args += "-DCMAKE_MAKE_PROGRAM=$ninjaExe"
                }
                if (-not $Ninja) { $args += @('-A', $Architecture, "-DCMAKE_BUILD_TYPE=$Configuration") }
                $args += $signatureArgument
                Assert-CommandSucceeded (Invoke-LoggedCommand 'Configure OrcaSlicer' $cmake $args $slicerLog) 'Configure OrcaSlicer'
            }
            else {
                Write-Host 'Configure     : skipped; generated build system will regenerate if its CMake inputs changed.'
            }

            Write-Section 'Build OrcaSlicer'
            $args = @('--build', $slicerBuild, '--config', $Configuration)
            if ($resolvedTarget) { $args += @('--target', $resolvedTarget) }
            if ($CleanFirst) { $args += '--clean-first' }
            $activeBinaryLog = if ($DiagnoseRebuild -and -not $Ninja) { $binaryLogPath } else { $null }
            if ($activeBinaryLog) { $script:BinaryLog = $activeBinaryLog }
            Assert-CommandSucceeded (Invoke-BoundedBuild 'Build OrcaSlicer' $cmake $args $slicerLog $script:RepoRoot $Parallel (-not $Ninja) $parallelHistoryPath $parallelHistoryKey $targetLabel $activeBinaryLog) 'Build OrcaSlicer'

            if ($legacyFullWorkflow -or $Install) {
                if (-not (Test-Path -LiteralPath $gettext)) { throw "Gettext helper was not found: $gettext" }
                Write-Section 'Compile translations'
                Assert-CommandSucceeded (Invoke-LoggedCommand 'Gettext' 'cmd.exe' @('/d', '/c', $gettext) $slicerLog) 'Gettext'

                Write-Section 'Install OrcaSlicer'
                $args = @('--build', $slicerBuild, '--config', $Configuration, '--target', 'install')
                $installHistoryKey = "install|$toolsetLabel|$Architecture|$Configuration|$generator"
                Assert-CommandSucceeded (Invoke-BoundedBuild 'Install' $cmake $args $slicerLog $script:RepoRoot $Parallel (-not $Ninja) $parallelHistoryPath $installHistoryKey 'install' $null) 'Install'
            }

            if ($DiagnoseRebuild) {
                $reasonCount = Export-RebuildSummary $slicerLog $rebuildSummaryPath
                $script:RebuildSummary = $rebuildSummaryPath
                Write-Host "Rebuild reasons: $reasonCount unique diagnostic lines -> $rebuildSummaryPath"
            }

            if ($Run) {
                $launcher = Join-Path $script:RepoRoot 'helper_scripts\orcaslicer_launcher\run.ps1'
                & $launcher -Configuration $Configuration -Architecture $Architecture -BuildDirectory $slicerBuild
            }
        }
    }
}
catch {
    $script:FailedPhase = $script:CurrentPhase
    $nativeExitCode = Get-Variable LASTEXITCODE -ValueOnly -ErrorAction SilentlyContinue
    $script:ExitCode = if ($nativeExitCode -and $nativeExitCode -gt 0) { [int]$nativeExitCode } else { 1 }
    if ($script:LogFiles.Count) {
        Add-Content -LiteralPath $script:LogFiles[-1] -Value "PowerShell build driver error: $($_.Exception.Message)"
    }
    Write-Host "`nBuild FAILED: $($_.Exception.Message)" -ForegroundColor Red
}
finally {
    $diagnostics = Export-Diagnostics $script:LogFiles $warningsLog $errorsLog
    $duration = (Get-Date) - $script:StartTime
    $gitCommit = (& git -C $script:RepoRoot rev-parse HEAD 2>$null)
    $gitDirty = [bool](& git -C $script:RepoRoot status --porcelain 2>$null)
    $manifestBuildDirectory = if (Get-Variable slicerBuild -ErrorAction SilentlyContinue) { $slicerBuild } else { $null }
    $manifestTarget = if (Get-Variable targetLabel -ErrorAction SilentlyContinue) { $targetLabel } else { $Target }
    $compilerFingerprint = if ($manifestBuildDirectory) { Get-BuildCommandFingerprint $manifestBuildDirectory $(if (Get-Variable resolvedTarget -ErrorAction SilentlyContinue) { $resolvedTarget } else { $null }) } else { $null }
    $recompiledObjects = if ($DiagnoseRebuild) { $null } elseif (Test-Path -LiteralPath $slicerLog) { Get-RecompiledSourceCount $slicerLog } else { $null }
    $pchStatus = if (Test-PCHRegenerated $slicerLog) { 'regenerated' } else { 'not_observed' }
    $outcome = if ($script:ExitCode -ne 0) { 'failure' } elseif (@($script:BuildAttempts | Where-Object { $_.attempt -gt 1 }).Count) { 'success_after_retry' } else { 'success' }
    $manifest = [ordered]@{
        started = $script:StartTime.ToString('o')
        duration_seconds = [math]::Round($duration.TotalSeconds, 3)
        exit_code = $script:ExitCode
        outcome = $outcome
        failed_phase = $script:FailedPhase
        configuration = $Configuration
        architecture = $Architecture
        generator = if (Get-Variable generator -ErrorAction SilentlyContinue) { $generator } else { $null }
        compiler = if ($manifestBuildDirectory) { Get-CMakeCompilerEntry $manifestBuildDirectory 'CMAKE_CXX_COMPILER' } else { $null }
        compiler_version = if ($manifestBuildDirectory) { Get-CMakeCompilerEntry $manifestBuildDirectory 'CMAKE_CXX_COMPILER_VERSION' } else { $null }
        toolset = if (Get-Variable toolsetLabel -ErrorAction SilentlyContinue) { $toolsetLabel } else { $null }
        build_directory = $manifestBuildDirectory
        requested_target = $Target
        resolved_target = $manifestTarget
        build_state = $(if ($BuildState -eq 'Auto') { 'unspecified' } else { $BuildState })
        changed_input = $ChangedInput
        configure_requested = [bool]$Configure
        configure_skipped = if (Get-Variable shouldConfigure -ErrorAction SilentlyContinue) { -not $shouldConfigure } else { $null }
        install_requested = [bool](-not $PreflightOnly -and ($Install -or $(if (Get-Variable legacyFullWorkflow -ErrorAction SilentlyContinue) { $legacyFullWorkflow } else { $false })))
        msbuild_parallel_ceiling = $(if ((Get-Variable generator -ErrorAction SilentlyContinue) -and -not $Ninja) { 1 } else { $null })
        compiler_parallel_ceiling = $Parallel
        parallel_was_automatic = $parallelWasAutomatic
        max_parallel = $MaxParallel
        build_attempts = $script:BuildAttempts
        compiler_command_fingerprint = $compilerFingerprint
        compiler_flags = if ($manifestBuildDirectory) { Get-CachedCMakeEntry $manifestBuildDirectory 'CMAKE_CXX_FLAGS' } else { $null }
        executable_linker_flags = if ($manifestBuildDirectory) { Get-CachedCMakeEntry $manifestBuildDirectory 'CMAKE_EXE_LINKER_FLAGS' } else { $null }
        objects_recompiled = $recompiledObjects
        object_count_source = $(if ($DiagnoseRebuild) { 'unavailable under MSBuild diagnostic verbosity; use the binary log' } else { 'build-log conservative parse' })
        pch_status = $pchStatus
        peak_compiler_processes = $null
        telemetry_note = 'Peak process and memory telemetry are not yet sampled; null means unavailable, not zero.'
        git_commit = $gitCommit
        git_dirty = $gitDirty
        warnings = $diagnostics.Warnings
        errors = $diagnostics.Errors
        binary_log = $script:BinaryLog
        rebuild_summary = $script:RebuildSummary
        phases = @($script:PhaseTimes.GetEnumerator() | ForEach-Object {
            [ordered]@{ name = $_.Key; seconds = [math]::Round($_.Value.TotalSeconds, 3) }
        })
        logs = $script:LogFiles
    }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

    Write-Section 'Build summary'
    Write-Host "Result   : $(if ($script:ExitCode -eq 0) { 'SUCCESS' } else { 'FAILED' })"
    Write-Host "Duration : $(Format-Duration $duration)"
    Write-Host "Warnings : $($diagnostics.Warnings) -> $warningsLog"
    Write-Host "Errors   : $($diagnostics.Errors) -> $errorsLog"
    Write-Host "Manifest : $manifestPath"
    foreach ($log in $script:LogFiles) { Write-Host "Log      : $log" }

    if ($script:ExitCode -ne 0 -and (Test-Path -LiteralPath $errorsLog)) {
        $errorTail = @(Get-Content -LiteralPath $errorsLog -Tail 20)
        if ($errorTail.Count) {
            Write-Host "`nLast errors:" -ForegroundColor Red
            $errorTail | ForEach-Object { Write-Host $_ -ForegroundColor Red }
        }
    }
    if ($script:ExitCode -ne 0 -and -not $NoPause) {
        Write-Host "`nPress any key to close this window..."
        & $env:ComSpec /d /c pause | Out-Null
    }
}

exit $script:ExitCode
