<#
.SYNOPSIS
Builds OrcaSlicer and its Windows dependencies with live, timestamped logging.

.EXAMPLE
.\scripts\build.ps1 -Configuration Release -Architecture x64

.EXAMPLE
.\scripts\build.ps1 -SlicerOnly -Parallel 8

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

    [ValidateRange(0, 128)]
    [int] $Parallel = 0,

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

function Write-Section([string] $Message) {
    Write-Host "`n=== $Message ===" -ForegroundColor Cyan
}

function Invoke-LoggedCommand {
    param(
        [Parameter(Mandatory = $true)][string] $Phase,
        [Parameter(Mandatory = $true)][string] $FilePath,
        [Parameter(Mandatory = $true)][string[]] $Arguments,
        [Parameter(Mandatory = $true)][string] $LogPath,
        [string] $WorkingDirectory = $script:RepoRoot
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
            Write-Host $line
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

    $warningPattern = '(?i)\bwarning\b'
    $errorPattern = '(?i)(fatal error|error\s+(C|LNK|MSB)\d+|MSB\d+:\s*error|CMake Error|FAILED:|exited with code|PowerShell build driver error)'
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
    return '{0}h {1}m {2}s' -f [int]$Duration.TotalHours, $Duration.Minutes, $Duration.Seconds
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
    $year = switch ($major) { 16 { '2019' } 17 { '2022' } 18 { '2026' } default { $major.ToString() } }
    $cmake = Join-Path $installPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (-not (Test-Path -LiteralPath $cmake)) { $cmake = 'cmake.exe' }

    return [pscustomobject]@{
        InstallPath = $installPath
        InstallVersion = $installVersion
        Major = $major
        Year = $year
        Generator = "Visual Studio $major $year"
        CMake = $cmake
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
        # RelWithDebInfo PCH compilation can consume roughly 2 GB per cl.exe.
        # Four GB per worker leaves headroom for linking, the OS, and the IDE.
        return [math]::Max(2, [math]::Min($processors, [math]::Floor($memoryGB / 4)))
    }
    catch {
        return 8
    }
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
if (-not $Architecture) {
    $Architecture = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64' -or $env:PROCESSOR_ARCHITEW6432 -eq 'ARM64') { 'ARM64' } else { 'x64' }
}
if ($env:MP_CAP) {
    $parsedCap = 0
    if ([int]::TryParse($env:MP_CAP, [ref]$parsedCap) -and $parsedCap -gt 0) { $Parallel = $parsedCap }
}
if ($Parallel -le 0) { $Parallel = Get-RecommendedParallelism }

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

try {
    Write-Section 'Preflight'
    if ($Ninja) {
        $generator = 'Ninja Multi-Config'
        $vsYear = 'Ninja'
        $cmake = (Get-Command cmake.exe -ErrorAction Stop).Source
        if (-not (Get-Command ninja.exe -ErrorAction SilentlyContinue)) { throw 'ninja.exe was not found on PATH.' }
    }
    else {
        $vs = Resolve-VisualStudio
        $generator = $vs.Generator
        $vsYear = $vs.Year
        $cmake = $vs.CMake
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
    $depsBuild = Join-Path $depsSource $baseBuildDirectory
    $slicerBuild = Join-Path $script:RepoRoot $baseBuildDirectory
    $gettext = Join-Path $script:RepoRoot 'scripts\run_gettext.bat'
    $sevenZip = Join-Path $script:RepoRoot 'tools\7z.exe'
    $dependencyStamp = Join-Path $depsBuild '.orca-deps-complete.json'
    $signatureArgument = if ($env:ORCA_UPDATER_SIG_KEY) { @("-DORCA_UPDATER_SIG_KEY=$($env:ORCA_UPDATER_SIG_KEY)") } else { @() }

    Write-Host "Configuration : $Configuration"
    Write-Host "Architecture  : $Architecture"
    Write-Host "Generator     : $generator"
    Write-Host "Build folder  : $baseBuildDirectory"
    Write-Host "Parallel cap  : $Parallel"
    Write-Host "CMake         : $cmake"

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
            $fresh = Get-FreshArgument $depsBuild $generator $(if ($Ninja) { '' } else { $Architecture })
            $args = @($fresh) + @('-S', $depsSource, '-B', $depsBuild, '-G', $generator)
            if (-not $Ninja) { $args += @('-A', $Architecture, "-DCMAKE_BUILD_TYPE=$Configuration") }
            $args += $signatureArgument
            Assert-CommandSucceeded (Invoke-LoggedCommand 'Configure dependencies' $cmake $args $depsLog) 'Configure dependencies'

            Write-Section 'Build dependencies'
            $args = @('--build', $depsBuild, '--config', $Configuration, '--target', 'deps')
            if (-not $Ninja) { $args += @('--', '-m', "-p:CL_MPCount=$Parallel") }
            Assert-CommandSucceeded (Invoke-LoggedCommand 'Build dependencies' $cmake $args $depsLog) 'Build dependencies'
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
            Write-Section 'Configure OrcaSlicer'
            $script:LogFiles += $slicerLog
            Set-Content -LiteralPath $slicerLog -Value "=== OrcaSlicer $(Get-Date -Format o) ==="
            $fresh = Get-FreshArgument $slicerBuild $generator $(if ($Ninja) { '' } else { $Architecture })
            $args = @($fresh) + @('-S', $script:RepoRoot, '-B', $slicerBuild, '-G', $generator, '-DORCA_TOOLS=ON')
            if (-not $Ninja) { $args += @('-A', $Architecture, "-DCMAKE_BUILD_TYPE=$Configuration") }
            $args += $signatureArgument
            Assert-CommandSucceeded (Invoke-LoggedCommand 'Configure OrcaSlicer' $cmake $args $slicerLog) 'Configure OrcaSlicer'

            Write-Section 'Build OrcaSlicer'
            $args = @('--build', $slicerBuild, '--config', $Configuration)
            if (-not $Ninja) { $args += @('--target', 'ALL_BUILD', '--', '-m', "-p:CL_MPCount=$Parallel") }
            Assert-CommandSucceeded (Invoke-LoggedCommand 'Build OrcaSlicer' $cmake $args $slicerLog) 'Build OrcaSlicer'

            if (-not (Test-Path -LiteralPath $gettext)) { throw "Gettext helper was not found: $gettext" }
            Write-Section 'Compile translations'
            Assert-CommandSucceeded (Invoke-LoggedCommand 'Gettext' 'cmd.exe' @('/d', '/c', $gettext) $slicerLog) 'Gettext'

            Write-Section 'Install OrcaSlicer'
            Assert-CommandSucceeded (Invoke-LoggedCommand 'Install' $cmake @('--build', $slicerBuild, '--config', $Configuration, '--target', 'install') $slicerLog) 'Install'

            if ($Run) {
                $executableCandidates = @(
                    (Join-Path $slicerBuild 'OrcaSlicer\OrcaSlicer.exe'),
                    (Join-Path $slicerBuild "src\$Configuration\orca-slicer.exe")
                )
                $executable = $executableCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
                if (-not $executable) { throw "Build succeeded, but OrcaSlicer.exe was not found under $slicerBuild" }
                Write-Host "Launching $executable"
                Start-Process -FilePath $executable
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
    $manifest = [ordered]@{
        started = $script:StartTime.ToString('o')
        duration_seconds = [math]::Round($duration.TotalSeconds, 3)
        exit_code = $script:ExitCode
        failed_phase = $script:FailedPhase
        configuration = $Configuration
        architecture = $Architecture
        generator = if (Get-Variable generator -ErrorAction SilentlyContinue) { $generator } else { $null }
        build_directory = if (Get-Variable baseBuildDirectory -ErrorAction SilentlyContinue) { $baseBuildDirectory } else { $null }
        parallel = $Parallel
        git_commit = $gitCommit
        git_dirty = $gitDirty
        warnings = $diagnostics.Warnings
        errors = $diagnostics.Errors
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
