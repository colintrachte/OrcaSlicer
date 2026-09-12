<#
.SYNOPSIS
Launches existing OrcaSlicer versions or creates a new Windows source build.

.DESCRIPTION
Detects production installations from the Windows uninstall registry and common
installation locations. Local builds are detected from build directories next
to this script. The graphical launcher can start optimized production builds or
diagnostic development builds with hardware-aware compiler recommendations.

.EXAMPLE
.\helper_scripts\orcaslicer_launcher\launcher.ps1

.EXAMPLE
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command List -Json

.EXAMPLE
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Build -Configuration Release -BuildScope Complete
#>
[CmdletBinding()]
param(
    [ValidateSet('Gui', 'List', 'Recommend', 'Prerequisites', 'CreateShortcut', 'Launch', 'Pin', 'Unpin', 'Build')]
    [string] $Command = 'Gui',

    [switch] $ListOnly,
    [switch] $RecommendationOnly,
    [switch] $PrerequisiteOnly,
    [switch] $CreateShortcut,
    [switch] $EmbeddedViewSmokeTest,
    [switch] $Json,
    [switch] $DryRun,

    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string] $Configuration = 'Release',

    [ValidateSet('x64', 'ARM64')]
    [string] $Architecture,

    [ValidateSet('Complete', 'Application')]
    [string] $BuildScope = 'Complete',

    [ValidateRange(0, 128)]
    [int] $Parallel = 0,

    [ValidateRange(0, 128)]
    [int] $MaxParallel = 0,

    [string] $TargetPath,
    [switch] $Ninja,
    [switch] $Fresh,
    [switch] $CleanFirst,
    [switch] $RunAfterBuild
)

$ErrorActionPreference = 'Stop'
$script:RepoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))

function Get-OrcaBuildRecommendation {
    $logicalProcessors = [Math]::Max(1, [Environment]::ProcessorCount)
    $physicalCores = $null
    $memoryGB = $null

    try {
        $system = Get-CimInstance -ClassName Win32_ComputerSystem -ErrorAction Stop
        if ($system.NumberOfLogicalProcessors) {
            $logicalProcessors = [int]$system.NumberOfLogicalProcessors
        }
        if ($system.TotalPhysicalMemory) {
            $memoryGB = [Math]::Round(([double]$system.TotalPhysicalMemory / 1GB), 1)
        }
    }
    catch {
        # Environment.ProcessorCount is still a useful, machine-specific fallback.
    }

    try {
        $processorCores = @(Get-CimInstance -ClassName Win32_Processor -ErrorAction Stop |
            ForEach-Object { [int]$_.NumberOfCores })
        if ($processorCores.Count) {
            $physicalCores = [int](($processorCores | Measure-Object -Sum).Sum)
        }
    }
    catch {
        # Physical-core count is display-only; recommendations use logical CPUs and RAM.
    }

    # CIM can be restricted by policy even for a desktop user. Kernel32 provides a
    # read-only fallback so the recommendation remains based on the actual machine.
    if (-not $memoryGB) {
        if (-not ('OrcaLauncher.NativeSystemInfo' -as [type])) {
            Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace OrcaLauncher {
    public static class NativeSystemInfo {
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
        private class MemoryStatus {
            public uint Length = (uint)Marshal.SizeOf(typeof(MemoryStatus));
            public uint MemoryLoad;
            public ulong TotalPhysical;
            public ulong AvailablePhysical;
            public ulong TotalPageFile;
            public ulong AvailablePageFile;
            public ulong TotalVirtual;
            public ulong AvailableVirtual;
            public ulong AvailableExtendedVirtual;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GlobalMemoryStatusEx([In, Out] MemoryStatus status);

        [DllImport("kernel32.dll")]
        private static extern uint GetActiveProcessorCount(ushort groupNumber);

        public static ulong TotalPhysicalMemory() {
            var status = new MemoryStatus();
            return GlobalMemoryStatusEx(status) ? status.TotalPhysical : 0;
        }

        public static uint LogicalProcessorCount() {
            const ushort AllProcessorGroups = 0xffff;
            return GetActiveProcessorCount(AllProcessorGroups);
        }
    }
}

'@
        }
        $nativeMemory = [OrcaLauncher.NativeSystemInfo]::TotalPhysicalMemory()
        if ($nativeMemory -gt 0) { $memoryGB = [Math]::Round(([double]$nativeMemory / 1GB), 1) }
        $nativeProcessors = [OrcaLauncher.NativeSystemInfo]::LogicalProcessorCount()
        if ($nativeProcessors -gt 0) { $logicalProcessors = [int]$nativeProcessors }
    }

    $memoryLimitedWorkers = if ($memoryGB) {
        [Math]::Max(1, [Math]::Floor($memoryGB / 8))
    }
    else {
        [Math]::Max(1, [Math]::Min(2, $logicalProcessors))
    }
    $recommendedWorkers = [int][Math]::Max(1, [Math]::Min($logicalProcessors, $memoryLimitedWorkers))
    $architecture = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64' -or
        $env:PROCESSOR_ARCHITEW6432 -eq 'ARM64') { 'ARM64' } else { 'x64' }

    [pscustomobject]@{
        Architecture       = $architecture
        LogicalProcessors  = $logicalProcessors
        PhysicalCores      = $physicalCores
        MemoryGB           = $memoryGB
        RecommendedWorkers = $recommendedWorkers
        MemoryPerWorkerGB  = 8
    }
}

function Get-OrcaPrerequisites {
    $pathEntries = @(
        [Environment]::GetEnvironmentVariable('Path', 'Process'),
        [Environment]::GetEnvironmentVariable('Path', 'User'),
        [Environment]::GetEnvironmentVariable('Path', 'Machine')
    ) -join ';' -split ';' | Where-Object { $_ } | Select-Object -Unique
    $env:Path = $pathEntries -join ';'

    $items = [Collections.Generic.List[object]]::new()
    $vswhereCandidates = @()
    if (${env:ProgramFiles(x86)}) { $vswhereCandidates += Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe' }
    if ($env:ProgramFiles) { $vswhereCandidates += Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe' }
    $vswhere = $vswhereCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    $visualStudioReady = $false
    $visualStudioDetail = 'Install Visual Studio 2026 with Desktop development with C++.'
    if ($vswhere) {
        $installationPath = ((@(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) -join '').Trim())
        $installationVersion = ((@(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion) -join '').Trim())
        if ($installationPath -and $installationVersion) {
            $cmakePath = Join-Path $installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            $majorVersion = [int]($installationVersion.Split('.')[0])
            $visualStudioReady = $majorVersion -eq 18 -and (Test-Path -LiteralPath $cmakePath)
            $visualStudioDetail = if ($visualStudioReady) {
                "Visual Studio $installationVersion with C++ and CMake"
            }
            elseif ($majorVersion -ne 18) {
                "Visual Studio $installationVersion found; Visual Studio 2026 is required."
            }
            else {
                'Add the CMake component in Visual Studio Installer.'
            }
        }
    }
    $items.Add([pscustomobject]@{
        Name = 'Visual Studio C++'
        Ready = $visualStudioReady
        Detail = $visualStudioDetail
        HelpUrl = 'https://learn.microsoft.com/en-us/cpp/overview/acquire-msvc'
    })

    $git = Get-Command git.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    $items.Add([pscustomobject]@{
        Name = 'Git'
        Ready = [bool]$git
        Detail = if ($git) { 'Installed' } else { 'Install Git for Windows.' }
        HelpUrl = 'https://git-scm.com/install/windows'
    })

    $perl = Get-Command perl.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    $items.Add([pscustomobject]@{
        Name = 'Perl'
        Ready = [bool]$perl
        Detail = if ($perl) { 'Installed' } else { 'Install Strawberry Perl.' }
        HelpUrl = 'https://strawberryperl.com/'
    })
    return $items.ToArray()
}

function Get-LauncherSettingsPath {
    $settingsRoot = Join-Path $env:APPDATA 'OrcaSlicer\SourceLauncher'
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $repoBytes = [Text.Encoding]::UTF8.GetBytes($script:RepoRoot.ToLowerInvariant())
        $repoKey = ([BitConverter]::ToString($sha.ComputeHash($repoBytes))).Replace('-', '').Substring(0, 16)
    }
    finally {
        $sha.Dispose()
    }
    return Join-Path $settingsRoot "$repoKey.json"
}

function Get-LauncherSettings {
    $path = Get-LauncherSettingsPath
    if (Test-Path -LiteralPath $path) {
        try {
            $loaded = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
            $defaults = [ordered]@{
                SetupComplete = $false
                AutoLaunchEnabled = $false
                LastTargetPath = $null
                PinnedTargetPath = $null
            }
            foreach ($entry in $defaults.GetEnumerator()) {
                if (-not $loaded.PSObject.Properties[$entry.Key]) {
                    $loaded | Add-Member -NotePropertyName $entry.Key -NotePropertyValue $entry.Value
                }
            }
            return $loaded
        }
        catch { }
    }
    return [pscustomobject]@{
        SetupComplete = $false
        AutoLaunchEnabled = $false
        LastTargetPath = $null
        PinnedTargetPath = $null
    }
}

function Save-LauncherSettings {
    param($Settings)
    $path = Get-LauncherSettingsPath
    $directory = Split-Path -Parent $path
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $Settings | ConvertTo-Json | Set-Content -LiteralPath $path -Encoding UTF8
}

function Install-OrcaLauncherShortcut {
    $entryPoint = Join-Path $script:RepoRoot 'START ORCASLICER.vbs'
    $iconPath = Join-Path $script:RepoRoot 'resources\images\OrcaSlicer.ico'
    $windowsScriptHost = Join-Path $env:SystemRoot 'System32\wscript.exe'
    foreach ($requiredPath in @($entryPoint, $iconPath, $windowsScriptHost)) {
        if (-not (Test-Path -LiteralPath $requiredPath)) {
            throw "Shortcut component not found: $requiredPath"
        }
    }

    $desktop = [Environment]::GetFolderPath('Desktop')
    if (-not $desktop) { throw 'Windows could not locate the Desktop folder.' }
    $shortcutPath = Join-Path $desktop 'OrcaSlicer Source Launcher.lnk'
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = $windowsScriptHost
    $shortcut.Arguments = '"' + $entryPoint + '"'
    $shortcut.WorkingDirectory = $script:RepoRoot
    $shortcut.IconLocation = "$iconPath,0"
    $shortcut.Description = 'Launch or build OrcaSlicer from source'
    $shortcut.WindowStyle = 1
    $shortcut.Save()
    return $shortcutPath
}

function Get-OrcaVersion {
    param([Parameter(Mandatory = $true)][string] $Path)

    try {
        $version = [Diagnostics.FileVersionInfo]::GetVersionInfo($Path).ProductVersion
        if (-not $version) {
            $version = [Diagnostics.FileVersionInfo]::GetVersionInfo($Path).FileVersion
        }
        if ($version) { return $version.Trim() }
    }
    catch {
        # A version is helpful but not required to launch a valid executable.
    }
    return 'Unknown'
}

function Get-ExecutableFromDisplayIcon {
    param([string] $DisplayIcon)

    if (-not $DisplayIcon) { return $null }
    $value = [Environment]::ExpandEnvironmentVariables($DisplayIcon.Trim())
    if ($value -match '^"(?<path>.+?\.exe)"(?:,\s*-?\d+)?$' -or
        $value -match '^(?<path>.+?\.exe)(?:,\s*-?\d+)?$') {
        return $Matches.path
    }
    return $null
}

function Get-OrcaLaunchTargets {
    $repoRoot = $script:RepoRoot
    $targets = [Collections.Generic.List[object]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)

    function Add-Target {
        param(
            [string] $Path,
            [string] $Kind,
            [string] $Configuration,
            [string] $Source
        )

        if (-not $Path) { return }
        $item = Get-Item -LiteralPath $Path -ErrorAction SilentlyContinue
        if (-not $item -or $item.PSIsContainer -or $item.Name -ine 'orca-slicer.exe') { return }
        if (-not $seen.Add($item.FullName)) { return }

        $architecture = if ($item.FullName -match '(?i)(arm64|aarch64)') { 'ARM64' } else { 'x64' }
        $targets.Add([pscustomobject]@{
            Name          = if ($Kind -eq 'Production') { 'OrcaSlicer' } else { "$Configuration build" }
            Kind          = $Kind
            Configuration = $Configuration
            Architecture  = $architecture
            Version       = Get-OrcaVersion -Path $item.FullName
            Updated       = $item.LastWriteTime
            Path          = $item.FullName
            Source        = $Source
        })
    }

    # Local builds produced by scripts\build.ps1 or Visual Studio/CMake directly.
    $buildDirectories = Get-ChildItem -LiteralPath $repoRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^build(?:-|$)' }
    foreach ($directory in $buildDirectories) {
        foreach ($configuration in @('Debug', 'RelWithDebInfo', 'Release')) {
            $localKind = if ($configuration -eq 'Release') { 'Production' } else { 'Development' }
            Add-Target -Path (Join-Path $directory.FullName "src\$configuration\orca-slicer.exe") `
                -Kind $localKind -Configuration $configuration -Source $directory.Name
        }

        $stagedConfiguration = if ($directory.Name -match '(?i)^build-dbg(?:-|$)') {
            'Debug'
        }
        elseif ($directory.Name -match '(?i)^build-dbginfo(?:-|$)') {
            'RelWithDebInfo'
        }
        else {
            'Release'
        }
        $stagedKind = if ($stagedConfiguration -eq 'Release') { 'Production' } else { 'Development' }
        Add-Target -Path (Join-Path $directory.FullName 'OrcaSlicer\orca-slicer.exe') `
            -Kind $stagedKind -Configuration $stagedConfiguration -Source "$($directory.Name) (staged)"
    }

    # Installer registrations are the most reliable source for production copies.
    $uninstallRoots = @(
        'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*'
    )
    foreach ($registration in Get-ItemProperty $uninstallRoots -ErrorAction SilentlyContinue |
        Where-Object { $_.DisplayName -match '^OrcaSlicer(?:\s|$)' }) {
        $registeredPath = Get-ExecutableFromDisplayIcon -DisplayIcon $registration.DisplayIcon
        Add-Target -Path $registeredPath -Kind 'Production' -Configuration 'Installed' -Source 'Windows installer'
        if ($registration.InstallLocation) {
            Add-Target -Path (Join-Path $registration.InstallLocation 'orca-slicer.exe') `
                -Kind 'Production' -Configuration 'Installed' -Source 'Windows installer'
        }
    }

    $commonLocations = @()
    if ($env:ProgramFiles) { $commonLocations += Join-Path $env:ProgramFiles 'OrcaSlicer\orca-slicer.exe' }
    if (${env:ProgramFiles(x86)}) { $commonLocations += Join-Path ${env:ProgramFiles(x86)} 'OrcaSlicer\orca-slicer.exe' }
    if ($env:LOCALAPPDATA) { $commonLocations += Join-Path $env:LOCALAPPDATA 'Programs\OrcaSlicer\orca-slicer.exe' }
    foreach ($path in $commonLocations) {
        Add-Target -Path $path -Kind 'Production' -Configuration 'Installed' -Source 'Standard install location'
    }

    $pathCommand = Get-Command 'orca-slicer.exe' -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($pathCommand) {
        Add-Target -Path $pathCommand.Source -Kind 'Production' -Configuration 'Installed' -Source 'PATH'
    }

    return @($targets | Sort-Object @{ Expression = { $_.Kind -eq 'Development' }; Descending = $true },
        @{ Expression = 'Updated'; Descending = $true })
}

function Get-LocalBuildDirectory {
    param($Target)
    if (-not $Target -or $Target.Configuration -eq 'Installed') { return $null }
    $root = [IO.Path]::GetFullPath($script:RepoRoot).TrimEnd('\', '/')
    $path = [IO.Path]::GetFullPath($Target.Path)
    if (-not $path.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { return $null }
    $relative = $path.Substring($root.Length + 1)
    if ($Target.IsBuildGroup) {
        if ($relative -notmatch '^build(?:-[^\\/]+)?$') { return $null }
        $item = Get-Item -LiteralPath $path -ErrorAction Stop
        if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { return $null }
        return $item.FullName
    }
    if ($relative -notmatch '^(build(?:-[^\\/]+)?)[\\/](?:src[\\/](?:Debug|Release|RelWithDebInfo)|OrcaSlicer)[\\/]orca-slicer\.exe$') { return $null }
    # Each row represents one output directory, not the shared CMake build tree.
    # Check every ancestor below the repository before operating on that output.
    $output = Split-Path -Parent $path
    $ancestor = $output
    while ($ancestor -ne $root) {
        $item = Get-Item -LiteralPath $ancestor -ErrorAction Stop
        if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { return $null }
        $ancestor = Split-Path -Parent $ancestor
    }
    return $output
}

function Get-BuildFileSizes {
    param([string] $Path, [hashtable] $Sizes)
    $directory = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) { return [long]0 }
    [long]$bytes = 0
    foreach ($item in Get-ChildItem -LiteralPath $Path -Force -ErrorAction Stop) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { continue }
        if ($item.PSIsContainer) { $bytes += Get-BuildFileSizes $item.FullName $Sizes }
        else { $bytes += $item.Length }
    }
    $Sizes[$directory.FullName] = $bytes
    return $bytes
}

function Format-BuildSize {
    param($Bytes)
    if ($null -eq $Bytes) { return 'Unavailable' }
    if ($Bytes -ge 1GB) { return ('{0:N2} GiB' -f ($Bytes / 1GB)) }
    if ($Bytes -ge 1MB) { return ('{0:N1} MiB' -f ($Bytes / 1MB)) }
    return ('{0:N0} KiB' -f ($Bytes / 1KB))
}

function Get-OrcaBuildGroups {
    param([object[]] $Targets)
    foreach ($directory in Get-ChildItem -LiteralPath $script:RepoRoot -Directory |
        Where-Object { $_.Name -match '^build(?:-|$)' -and -not ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) }) {
        $prefix = $directory.FullName + [IO.Path]::DirectorySeparatorChar
        $children = @($Targets | Where-Object { $_.Path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) })
        $sizes = @{}
        $bytes = $null
        try { $bytes = Get-BuildFileSizes $directory.FullName $sizes }
        catch { $sizes.Clear() }
        [long]$outputBytes = 0
        foreach ($child in $children) {
            $outputBytes += $sizes[(Split-Path -Parent $child.Path)]
        }
        [pscustomobject]@{
            IsBuildGroup = $true
            Path = $directory.FullName
            Name = $directory.Name
            Configuration = 'Build folder'
            Targets = $children
            Sizes = $sizes
            Bytes = $bytes
            SharedBytes = $(if ($null -ne $bytes) { $bytes - $outputBytes } else { $null })
        }
    }
}

function Remove-LocalBuild {
    param($Target)
    $directory = Get-LocalBuildDirectory $Target
    if (-not $directory) { throw 'Only local build folders in this checkout can be deleted.' }
    if ($script:activeBuildProcess -and -not $script:activeBuildProcess.HasExited) {
        throw 'Wait for the current build to finish before deleting a build.'
    }
    $prefix = $directory + [IO.Path]::DirectorySeparatorChar
    foreach ($process in @(Get-Process -Name 'orca-slicer' -ErrorAction SilentlyContinue)) {
        if (-not $process.Path -or $process.Path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Close OrcaSlicer before deleting this build.'
        }
    }
    # Enumerate one level at a time: staged resources can be junctions into the
    # source tree. Delete each link itself, never enumerate its destination.
    function Remove-BuildDirectoryContents {
        param([string] $Path)
        foreach ($item in Get-ChildItem -LiteralPath $Path -Force -ErrorAction Stop) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                if ($item.PSIsContainer) {
                    [IO.Directory]::Delete($item.FullName, $false)
                }
                else {
                    [IO.File]::Delete($item.FullName)
                }
            }
            elseif ($item.PSIsContainer) {
                Remove-BuildDirectoryContents $item.FullName
            }
            else {
                Remove-Item -LiteralPath $item.FullName -Force -ErrorAction Stop
            }
        }
        [IO.Directory]::Delete($Path, $false)
    }
    Remove-BuildDirectoryContents $directory
    foreach ($property in @('PinnedTargetPath', 'LastTargetPath')) {
        if ($settings.$property -and $settings.$property.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            $settings.$property = $null
        }
    }
    Save-LauncherSettings $settings
}

function Write-ConsoleResult {
    param($Value)
    if ($Json) { $Value | ConvertTo-Json -Depth 6 }
    else { Write-Output $Value }
}

if ($ListOnly) { $Command = 'List' }
if ($RecommendationOnly) { $Command = 'Recommend' }
if ($PrerequisiteOnly) { $Command = 'Prerequisites' }
if ($CreateShortcut) { $Command = 'CreateShortcut' }

$launchTargets = @(Get-OrcaLaunchTargets)
$hardware = Get-OrcaBuildRecommendation
$settings = Get-LauncherSettings

switch ($Command) {
    'List' {
        Write-ConsoleResult $launchTargets
        exit 0
    }
    'Recommend' {
        Write-ConsoleResult $hardware
        exit 0
    }
    'Prerequisites' {
        Write-ConsoleResult @(Get-OrcaPrerequisites)
        exit 0
    }
    'CreateShortcut' {
        $shortcutPath = Join-Path ([Environment]::GetFolderPath('Desktop')) 'OrcaSlicer Source Launcher.lnk'
        if (-not $DryRun) { $shortcutPath = Install-OrcaLauncherShortcut }
        Write-ConsoleResult ([pscustomobject]@{ shortcut = $shortcutPath; dry_run = [bool]$DryRun })
        exit 0
    }
    'Launch' {
        $target = if ($TargetPath) {
            $launchTargets | Where-Object { $_.Path -eq [IO.Path]::GetFullPath($TargetPath) } | Select-Object -First 1
        }
        elseif ($settings.PinnedTargetPath) {
            $launchTargets | Where-Object { $_.Path -eq $settings.PinnedTargetPath } | Select-Object -First 1
        }
        elseif ($settings.LastTargetPath) {
            $launchTargets | Where-Object { $_.Path -eq $settings.LastTargetPath } | Select-Object -First 1
        }
        if (-not $target) {
            $target = $launchTargets | Where-Object { $_.Kind -eq 'Production' } |
                Sort-Object Updated -Descending | Select-Object -First 1
        }
        if (-not $target) { throw 'No launchable OrcaSlicer build was found.' }
        if (-not $DryRun) {
            Start-Process -FilePath $target.Path -WorkingDirectory (Split-Path -Parent $target.Path)
            $settings.SetupComplete = $true
            $settings.LastTargetPath = $target.Path
            Save-LauncherSettings $settings
        }
        Write-ConsoleResult ([pscustomobject]@{ target = $target; dry_run = [bool]$DryRun })
        exit 0
    }
    'Pin' {
        if (-not $TargetPath) { throw 'Pin requires -TargetPath from the List command.' }
        $fullTargetPath = [IO.Path]::GetFullPath($TargetPath)
        $target = $launchTargets | Where-Object { $_.Path -eq $fullTargetPath } | Select-Object -First 1
        if (-not $target) { throw "Target is not a discovered OrcaSlicer build: $fullTargetPath" }
        if (-not $DryRun) {
            $settings.SetupComplete = $true
            $settings.AutoLaunchEnabled = $true
            $settings.PinnedTargetPath = $target.Path
            Save-LauncherSettings $settings
        }
        Write-ConsoleResult ([pscustomobject]@{ target = $target; dry_run = [bool]$DryRun })
        exit 0
    }
    'Unpin' {
        if (-not $DryRun) {
            $settings.PinnedTargetPath = $null
            Save-LauncherSettings $settings
        }
        Write-ConsoleResult ([pscustomobject]@{ pinned = $false; dry_run = [bool]$DryRun })
        exit 0
    }
    'Build' {
        if (-not $Architecture) { $Architecture = $hardware.Architecture }
        if ($MaxParallel -le 0) { $MaxParallel = $hardware.RecommendedWorkers }
        $buildScript = Join-Path $script:RepoRoot 'scripts\build.ps1'
        if (-not (Test-Path -LiteralPath $buildScript)) { throw "Build script not found: $buildScript" }
        $buildParameters = @{
            Configuration = $Configuration
            Architecture = $Architecture
            Parallel = $Parallel
            MaxParallel = $MaxParallel
            NoPause = $true
        }
        if ($BuildScope -eq 'Complete') { $buildParameters.Install = $true }
        else {
            $buildParameters.Target = 'App'
            $buildParameters.BuildOnly = $true
        }
        if ($Ninja) { $buildParameters.Ninja = $true }
        if ($Fresh) { $buildParameters.Fresh = $true }
        if ($CleanFirst) { $buildParameters.CleanFirst = $true }
        if ($RunAfterBuild) { $buildParameters.Run = $true }
        if ($DryRun) {
            Write-ConsoleResult ([pscustomobject]@{
                script = $buildScript
                parameters = $buildParameters
                dry_run = $true
            })
            exit 0
        }
        & $buildScript @buildParameters
        $buildExitCode = $LASTEXITCODE
        if ($null -eq $buildExitCode) { $buildExitCode = 0 }
        exit $buildExitCode
    }
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()
[System.Windows.Forms.Application]::SetUnhandledExceptionMode([System.Windows.Forms.UnhandledExceptionMode]::CatchException)
$script:embeddedView = $null
$script:embeddedBuildRequest = $null
$script:LauncherState = [pscustomobject]@{ EmbeddedBuildRequest = $null }
$script:Theme = @{
    Canvas       = [Drawing.Color]::FromArgb(10, 14, 24)
    Surface      = [Drawing.Color]::FromArgb(20, 27, 43)
    SurfaceLift  = [Drawing.Color]::FromArgb(29, 38, 58)
    SurfaceHover = [Drawing.Color]::FromArgb(37, 49, 72)
    Border       = [Drawing.Color]::FromArgb(55, 69, 94)
    Text         = [Drawing.Color]::FromArgb(242, 247, 255)
    Muted        = [Drawing.Color]::FromArgb(151, 166, 191)
    Accent       = [Drawing.Color]::FromArgb(38, 218, 205)
    AccentDeep   = [Drawing.Color]::FromArgb(11, 148, 153)
    Violet       = [Drawing.Color]::FromArgb(139, 92, 246)
    Blue         = [Drawing.Color]::FromArgb(64, 142, 255)
    Success      = [Drawing.Color]::FromArgb(73, 222, 145)
    Warning      = [Drawing.Color]::FromArgb(255, 190, 79)
    Danger       = [Drawing.Color]::FromArgb(255, 100, 124)
}
$script:Glyph = @{
    Diamond       = [char]0x25C6
    DiamondOutline = [char]0x25C7
    External      = [char]0x2197
    Plus          = [char]0xFF0B
    Refresh       = [char]0x21BB
    Play          = [char]0x25B6
    Status        = [char]0x25CF
    Separator     = [char]0x00B7
}

function New-OrcaRoundedPath {
    param([Drawing.Rectangle] $Rectangle, [int] $Radius)
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $diameter = [Math]::Max(2, $Radius * 2)
    $arc = [Drawing.Rectangle]::new($Rectangle.X, $Rectangle.Y, $diameter, $diameter)
    $path.AddArc($arc, 180, 90)
    $arc.X = $Rectangle.Right - $diameter
    $path.AddArc($arc, 270, 90)
    $arc.Y = $Rectangle.Bottom - $diameter
    $path.AddArc($arc, 0, 90)
    $arc.X = $Rectangle.Left
    $path.AddArc($arc, 90, 90)
    $path.CloseFigure()
    return $path
}

function Set-OrcaRoundedRegion {
    param([System.Windows.Forms.Control] $Control, [int] $Radius = 10)
    $roundedPathFactory = ${function:New-OrcaRoundedPath}
    $applyRegion = {
        if ($Control.Width -lt 2 -or $Control.Height -lt 2) { return }
        $rectangle = [Drawing.Rectangle]::new(0, 0, $Control.Width, $Control.Height)
        $path = & $roundedPathFactory $rectangle $Radius
        $oldRegion = $Control.Region
        $Control.Region = [Drawing.Region]::new($path)
        if ($oldRegion) { $oldRegion.Dispose() }
        $path.Dispose()
    }.GetNewClosure()
    & $applyRegion
    $Control.Add_Resize($applyRegion)
}

function Set-OrcaButtonStyle {
    param(
        [System.Windows.Forms.Button] $Button,
        [ValidateSet('Primary', 'Secondary', 'Ghost', 'Danger')]
        [string] $Variant = 'Secondary',
        [int] $Radius = 9
    )
    $normal = switch ($Variant) {
        'Primary' { $script:Theme.Accent }
        'Danger' { $script:Theme.Danger }
        'Ghost' { $script:Theme.Canvas }
        default { $script:Theme.SurfaceLift }
    }
    $hover = switch ($Variant) {
        'Primary' { [Drawing.Color]::FromArgb(91, 239, 225) }
        'Danger' { [Drawing.Color]::FromArgb(255, 130, 148) }
        'Ghost' { $script:Theme.SurfaceHover }
        default { $script:Theme.SurfaceHover }
    }
    $foreground = if ($Variant -in @('Primary', 'Danger')) {
        [Drawing.Color]::FromArgb(8, 20, 28)
    }
    else { $script:Theme.Text }
    $disabledForeground = $script:Theme.Muted
    $disabledBackground = $script:Theme.Surface
    $Button.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
    $Button.FlatAppearance.BorderSize = if ($Variant -eq 'Ghost') { 1 } else { 0 }
    $Button.FlatAppearance.BorderColor = $script:Theme.Border
    $Button.BackColor = $normal
    $Button.ForeColor = $foreground
    $Button.Cursor = [System.Windows.Forms.Cursors]::Hand
    $Button.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
    $Button.UseVisualStyleBackColor = $false
    Set-OrcaRoundedRegion $Button $Radius
    $Button.Add_MouseEnter(({ if ($Button.Enabled) { $Button.BackColor = $hover } }).GetNewClosure())
    $Button.Add_MouseLeave(({ $Button.BackColor = $normal }).GetNewClosure())
    $Button.Add_EnabledChanged(({
        $Button.ForeColor = if ($Button.Enabled) { $foreground } else { $disabledForeground }
        $Button.BackColor = if ($Button.Enabled) { $normal } else { $disabledBackground }
    }).GetNewClosure())
}

function Set-OrcaInputStyle {
    param([System.Windows.Forms.Control] $Control)
    $Control.BackColor = $script:Theme.SurfaceLift
    $Control.ForeColor = $script:Theme.Text
    $Control.Font = [Drawing.Font]::new('Segoe UI', 9.5)
    if ($Control -is [System.Windows.Forms.ComboBox]) {
        $Control.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
    }
    if ($Control -is [System.Windows.Forms.NumericUpDown]) {
        $Control.BorderStyle = [System.Windows.Forms.BorderStyle]::FixedSingle
    }
}

function Set-OrcaListViewStyle {
    param(
        [System.Windows.Forms.ListView] $Control,
        [ValidateSet('Builds', 'Requirements')]
        [string] $Mode
    )
    $theme = $script:Theme
    $roundedPathFactory = ${function:New-OrcaRoundedPath}
    $Control.BackColor = $theme.Surface
    $Control.ForeColor = $theme.Text
    $Control.BorderStyle = [System.Windows.Forms.BorderStyle]::None
    $Control.GridLines = $false
    $Control.OwnerDraw = $true
    try {
        $doubleBuffered = [System.Windows.Forms.ListView].GetProperty(
            'DoubleBuffered',
            [Reflection.BindingFlags]'NonPublic,Instance'
        )
        $doubleBuffered.SetValue($Control, $true, $null)
    }
    catch { }

    $fitLastColumn = {
        if ($Control.Columns.Count -lt 1) { return }
        $fixedWidth = 0
        for ($columnIndex = 0; $columnIndex -lt $Control.Columns.Count - 1; $columnIndex++) {
            $fixedWidth += $Control.Columns[$columnIndex].Width
        }
        $Control.Columns[$Control.Columns.Count - 1].Width = [Math]::Max(
            180,
            $Control.ClientSize.Width - $fixedWidth
        )
    }.GetNewClosure()
    & $fitLastColumn
    $Control.Add_Resize($fitLastColumn)

    $Control.Add_DrawColumnHeader(({ param($sender, $eventArgs)
        $bounds = $eventArgs.Bounds
        $graphics = $eventArgs.Graphics
        $headerBrush = [Drawing.SolidBrush]::new($theme.SurfaceLift)
        $graphics.FillRectangle($headerBrush, $bounds)
        $headerBrush.Dispose()
        $textBounds = [Drawing.Rectangle]::new($bounds.X + 12, $bounds.Y, $bounds.Width - 16, $bounds.Height)
        [System.Windows.Forms.TextRenderer]::DrawText(
            $graphics,
            $eventArgs.Header.Text.ToUpperInvariant(),
            [Drawing.Font]::new('Segoe UI Semibold', 8),
            $textBounds,
            $theme.Muted,
            [System.Windows.Forms.TextFormatFlags]'VerticalCenter,EndEllipsis'
        )
        $pen = [Drawing.Pen]::new($theme.Border)
        $graphics.DrawLine($pen, $bounds.Left, $bounds.Bottom - 1, $bounds.Right, $bounds.Bottom - 1)
        $pen.Dispose()
    }).GetNewClosure())
    $Control.Add_DrawItem(({ param($sender, $eventArgs)
        if ($Control.View -ne [System.Windows.Forms.View]::Details) { $eventArgs.DrawDefault = $true }
    }).GetNewClosure())
    $Control.Add_DrawSubItem(({ param($sender, $eventArgs)
        $graphics = $eventArgs.Graphics
        $bounds = $eventArgs.Bounds
        $selected = $eventArgs.Item.Selected
        $rowIndex = $eventArgs.ItemIndex
        $background = if ($selected) {
            [Drawing.Color]::FromArgb(41, 67, 83)
        }
        elseif ($rowIndex % 2 -eq 0) { $theme.Surface }
        else { [Drawing.Color]::FromArgb(23, 31, 48) }
        $backgroundBrush = [Drawing.SolidBrush]::new($background)
        $graphics.FillRectangle($backgroundBrush, $bounds)
        $backgroundBrush.Dispose()

        $text = $eventArgs.SubItem.Text
        $textColor = $theme.Text
        $badgeColor = $null
        if ($Mode -eq 'Builds') {
            if ($eventArgs.ColumnIndex -eq 0 -and $text -eq 'Pinned') { $badgeColor = $theme.Warning }
            elseif ($eventArgs.ColumnIndex -eq 1 -and $text -eq 'Production') { $badgeColor = $theme.Success }
            elseif ($eventArgs.ColumnIndex -eq 1 -and $text -eq 'Development') { $badgeColor = $theme.Violet }
        }
        elseif ($Mode -eq 'Requirements' -and $eventArgs.ColumnIndex -eq 0) {
            if ($text -eq 'Ready') { $badgeColor = $theme.Success }
            elseif ($text -eq 'Missing') { $badgeColor = $theme.Danger }
        }

        if ($badgeColor) {
            $badgeWidth = [Math]::Min($bounds.Width - 14, [Math]::Max(54, $text.Length * 7 + 20))
            $badgeBounds = [Drawing.Rectangle]::new($bounds.X + 8, $bounds.Y + 6, $badgeWidth, $bounds.Height - 12)
            $badgePath = & $roundedPathFactory $badgeBounds 8
            $badgeBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(45, $badgeColor))
            $badgePen = [Drawing.Pen]::new($badgeColor)
            $graphics.FillPath($badgeBrush, $badgePath)
            $graphics.DrawPath($badgePen, $badgePath)
            [System.Windows.Forms.TextRenderer]::DrawText(
                $graphics, $text, [Drawing.Font]::new('Segoe UI Semibold', 8), $badgeBounds,
                $badgeColor, [System.Windows.Forms.TextFormatFlags]'HorizontalCenter,VerticalCenter,EndEllipsis'
            )
            $badgeBrush.Dispose()
            $badgePen.Dispose()
            $badgePath.Dispose()
        }
        else {
            $textBounds = [Drawing.Rectangle]::new($bounds.X + 12, $bounds.Y, $bounds.Width - 18, $bounds.Height)
            if ($Mode -eq 'Builds' -and $eventArgs.ColumnIndex -eq 5) { $textColor = $theme.Muted }
            [System.Windows.Forms.TextRenderer]::DrawText(
                $graphics, $text, [Drawing.Font]::new('Segoe UI', 9), $textBounds, $textColor,
                [System.Windows.Forms.TextFormatFlags]'VerticalCenter,EndEllipsis,SingleLine'
            )
        }
    }).GetNewClosure())
}

function Quote-ProcessArgument {
    param([string] $Value)
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '(\\*)"', '$1$1\"' -replace '(\\+)$', '$1$1') + '"'
}

function Get-OrcaPowerShellHost {
    $windowsPowerShell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    if (Test-Path -LiteralPath $windowsPowerShell) { return $windowsPowerShell }
    $currentHost = (Get-Process -Id $PID).Path
    if ($currentHost -and (Test-Path -LiteralPath $currentHost)) { return $currentHost }
    throw 'No compatible PowerShell runtime is available.'
}

function Show-EmbeddedView {
    param([Parameter(Mandatory = $true)][System.Windows.Forms.Form] $View)
    if ($script:embeddedView -and -not $script:embeddedView.IsDisposed) {
        $script:embeddedView.Close()
        $script:embeddedView.Dispose()
    }
    $View.TopLevel = $false
    $View.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::None
    $View.Dock = [System.Windows.Forms.DockStyle]::Fill
    $View.BackColor = $form.BackColor
    $form.Controls.Add($View)
    $View.BringToFront()
    $View.Show()
    $script:embeddedView = $View
}

function Show-PrerequisiteDialog {
    $dialog = [System.Windows.Forms.Form]::new()
    $dialog.Text = 'Build prerequisites'
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false
    $dialog.ClientSize = [Drawing.Size]::new(760, 390)
    $dialog.Font = [Drawing.Font]::new('Segoe UI', 9)
    $dialog.BackColor = $script:Theme.Canvas
    $dialog.ForeColor = $script:Theme.Text

    $headerGlow = [System.Windows.Forms.Panel]::new()
    $headerGlow.Location = [Drawing.Point]::new(0, 0)
    $headerGlow.Size = [Drawing.Size]::new(760, 6)
    $headerGlow.Anchor = 'Top,Left,Right'
    $headerAccent = $script:Theme.Accent
    $headerViolet = $script:Theme.Violet
    $headerGlow.Add_Paint(({ param($sender, $eventArgs)
        $gradient = [Drawing.Drawing2D.LinearGradientBrush]::new(
            $headerGlow.ClientRectangle, $headerAccent, $headerViolet, 0
        )
        $eventArgs.Graphics.FillRectangle($gradient, $headerGlow.ClientRectangle)
        $gradient.Dispose()
    }).GetNewClosure())
    $dialog.Controls.Add($headerGlow)

    $heading = [System.Windows.Forms.Label]::new()
    $heading.AutoSize = $true
    $heading.Font = [Drawing.Font]::new('Segoe UI Semibold', 18)
    $heading.ForeColor = $script:Theme.Text
    $heading.Location = [Drawing.Point]::new(22, 18)
    $heading.Text = 'Required software'
    $dialog.Controls.Add($heading)

    $explanation = [System.Windows.Forms.Label]::new()
    $explanation.Location = [Drawing.Point]::new(24, 57)
    $explanation.Size = [Drawing.Size]::new(715, 42)
    $explanation.ForeColor = $script:Theme.Muted
    $explanation.Text = 'A complete build also builds the included dependencies. Install the software listed below first.'
    $dialog.Controls.Add($explanation)

    $prerequisiteList = [System.Windows.Forms.ListView]::new()
    $prerequisiteList.Location = [Drawing.Point]::new(20, 95)
    $prerequisiteList.Size = [Drawing.Size]::new(716, 205)
    $prerequisiteList.View = 'Details'
    $prerequisiteList.FullRowSelect = $true
    $prerequisiteList.MultiSelect = $false
    $prerequisiteList.HideSelection = $false
    $prerequisiteList.Font = [Drawing.Font]::new('Segoe UI', 9.5)
    $prerequisiteList.Anchor = 'Top,Bottom,Left,Right'
    [void]$prerequisiteList.Columns.Add('Status', 85)
    [void]$prerequisiteList.Columns.Add('Requirement', 235)
    [void]$prerequisiteList.Columns.Add('Details', 385)
    $dialog.Controls.Add($prerequisiteList)
    Set-OrcaListViewStyle $prerequisiteList 'Requirements'
    $requirementRowHeight = [System.Windows.Forms.ImageList]::new()
    $requirementRowHeight.ImageSize = [Drawing.Size]::new(1, 36)
    $prerequisiteList.SmallImageList = $requirementRowHeight

    $resultLabel = [System.Windows.Forms.Label]::new()
    $resultLabel.AutoSize = $true
    $resultLabel.Location = [Drawing.Point]::new(22, 313)
    $resultLabel.Anchor = 'Bottom,Left'
    $resultLabel.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
    $dialog.Controls.Add($resultLabel)

    $getButton = [System.Windows.Forms.Button]::new()
    $getButton.Location = [Drawing.Point]::new(455, 340)
    $getButton.Size = [Drawing.Size]::new(120, 32)
    $getButton.Text = 'Get selected...'
    $getButton.Enabled = $false
    $getButton.Anchor = 'Bottom,Right'
    $dialog.Controls.Add($getButton)
    Set-OrcaButtonStyle $getButton 'Primary'

    $refresh = [System.Windows.Forms.Button]::new()
    $refresh.Location = [Drawing.Point]::new(583, 340)
    $refresh.Size = [Drawing.Size]::new(74, 32)
    $refresh.Text = 'Refresh'
    $refresh.Anchor = 'Bottom,Right'
    $dialog.Controls.Add($refresh)
    Set-OrcaButtonStyle $refresh 'Secondary'

    $close = [System.Windows.Forms.Button]::new()
    $close.Location = [Drawing.Point]::new(665, 340)
    $close.Size = [Drawing.Size]::new(72, 32)
    $close.Text = 'Close'
    $close.Anchor = 'Bottom,Right'
    $dialog.Controls.Add($close)
    Set-OrcaButtonStyle $close 'Ghost'
    $dialog.AcceptButton = $close

    $getPrerequisiteData = ${function:Get-OrcaPrerequisites}
    $readyColor = $script:Theme.Success
    $attentionColor = $script:Theme.Warning
    $updatePrerequisiteList = {
        $prerequisiteList.Items.Clear()
        $requirements = @(& $getPrerequisiteData)
        foreach ($requirement in $requirements) {
            $row = [System.Windows.Forms.ListViewItem]::new($(if ($requirement.Ready) { 'Ready' } else { 'Missing' }))
            [void]$row.SubItems.Add($requirement.Name)
            [void]$row.SubItems.Add($requirement.Detail)
            $row.Tag = $requirement
            [void]$prerequisiteList.Items.Add($row)
        }
        $missingCount = @($requirements | Where-Object { -not $_.Ready }).Count
        $resultLabel.Text = if ($missingCount) { "$missingCount prerequisite(s) still need attention." } else { 'Ready for a complete source build.' }
        $resultLabel.ForeColor = if ($missingCount) { $attentionColor } else { $readyColor }
        if ($prerequisiteList.Items.Count) { $prerequisiteList.Items[0].Selected = $true }
    }.GetNewClosure()

    $prerequisiteList.Add_SelectedIndexChanged(({
        $getButton.Enabled = $prerequisiteList.SelectedItems.Count -gt 0
    }).GetNewClosure())
    $prerequisiteList.Add_DoubleClick(({ $getButton.PerformClick() }).GetNewClosure())
    $getButton.Add_Click(({
        if ($prerequisiteList.SelectedItems.Count -eq 0) { return }
        Start-Process $prerequisiteList.SelectedItems[0].Tag.HelpUrl
    }).GetNewClosure())
    $refresh.Add_Click(({ & $updatePrerequisiteList }).GetNewClosure())
    $close.Add_Click(({ $dialog.Close() }).GetNewClosure())

    & $updatePrerequisiteList
    Show-EmbeddedView $dialog
}

function Show-NewBuildDialog {
    $dialog = [System.Windows.Forms.Form]::new()
    $dialog.Text = 'New source build'
    $dialog.StartPosition = 'CenterParent'
    $dialog.FormBorderStyle = 'FixedDialog'
    $dialog.MaximizeBox = $false
    $dialog.MinimizeBox = $false
    $dialog.ClientSize = [Drawing.Size]::new(650, 545)
    $dialog.Font = [Drawing.Font]::new('Segoe UI', 9)
    $dialog.BackColor = $script:Theme.Canvas
    $dialog.ForeColor = $script:Theme.Text

    $headerGlow = [System.Windows.Forms.Panel]::new()
    $headerGlow.Location = [Drawing.Point]::new(0, 0)
    $headerGlow.Size = [Drawing.Size]::new(650, 6)
    $headerGlow.Anchor = 'Top,Left,Right'
    $headerAccent = $script:Theme.Accent
    $headerViolet = $script:Theme.Violet
    $headerGlow.Add_Paint(({ param($sender, $eventArgs)
        $gradient = [Drawing.Drawing2D.LinearGradientBrush]::new(
            $headerGlow.ClientRectangle, $headerAccent, $headerViolet, 0
        )
        $eventArgs.Graphics.FillRectangle($gradient, $headerGlow.ClientRectangle)
        $gradient.Dispose()
    }).GetNewClosure())
    $dialog.Controls.Add($headerGlow)

    $heading = [System.Windows.Forms.Label]::new()
    $heading.AutoSize = $true
    $heading.Font = [Drawing.Font]::new('Segoe UI Semibold', 18)
    $heading.ForeColor = $script:Theme.Text
    $heading.Location = [Drawing.Point]::new(20, 18)
    $heading.Text = 'Create from source'
    $dialog.Controls.Add($heading)

    $specParts = @("$($hardware.LogicalProcessors) logical processors")
    if ($hardware.PhysicalCores) { $specParts += "$($hardware.PhysicalCores) physical cores" }
    if ($hardware.MemoryGB) { $specParts += "$($hardware.MemoryGB) GB RAM" }
    $specParts += "$($hardware.Architecture) Windows"
    $specs = [System.Windows.Forms.Label]::new()
    $specs.AutoSize = $true
    $specs.ForeColor = $script:Theme.Muted
    $specs.Location = [Drawing.Point]::new(22, 55)
    $specs.Text = 'Detected: ' + ($specParts -join ' | ')
    $dialog.Controls.Add($specs)

    $recommendation = [System.Windows.Forms.Label]::new()
    $recommendation.AutoSize = $true
    $recommendation.ForeColor = $script:Theme.Accent
    $recommendation.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
    $recommendation.Location = [Drawing.Point]::new(22, 78)
    $recommendation.Text = "Recommended compiler workers: $($hardware.RecommendedWorkers) (based on installed RAM)"
    $dialog.Controls.Add($recommendation)

    function Add-FieldLabel([string] $Text, [int] $Y) {
        $label = [System.Windows.Forms.Label]::new()
        $label.AutoSize = $true
        $label.Location = [Drawing.Point]::new(22, $Y)
        $label.Text = $Text.ToUpperInvariant()
        $label.ForeColor = $script:Theme.Muted
        $label.Font = [Drawing.Font]::new('Segoe UI Semibold', 8)
        $dialog.Controls.Add($label)
    }

    Add-FieldLabel 'Build type' 108
    $kind = [System.Windows.Forms.ComboBox]::new()
    $kind.DropDownStyle = 'DropDownList'
    $kind.Location = [Drawing.Point]::new(175, 104)
    $kind.Size = [Drawing.Size]::new(445, 25)
    $kind.Anchor = 'Top,Left,Right'
    [void]$kind.Items.Add('Production - optimized build for normal slicing')
    [void]$kind.Items.Add('Development - diagnostics and easier debugging')
    $kind.SelectedIndex = 0
    $dialog.Controls.Add($kind)
    Set-OrcaInputStyle $kind

    Add-FieldLabel 'Configuration' 143
    $configuration = [System.Windows.Forms.ComboBox]::new()
    $configuration.DropDownStyle = 'DropDownList'
    $configuration.Location = [Drawing.Point]::new(175, 139)
    $configuration.Size = [Drawing.Size]::new(445, 25)
    $configuration.Anchor = 'Top,Left,Right'
    $dialog.Controls.Add($configuration)
    Set-OrcaInputStyle $configuration

    Add-FieldLabel 'Build scope' 178
    $scope = [System.Windows.Forms.ComboBox]::new()
    $scope.DropDownStyle = 'DropDownList'
    $scope.Location = [Drawing.Point]::new(175, 174)
    $scope.Size = [Drawing.Size]::new(445, 25)
    $scope.Anchor = 'Top,Left,Right'
    $dialog.Controls.Add($scope)
    Set-OrcaInputStyle $scope

    Add-FieldLabel 'Architecture' 213
    $architecture = [System.Windows.Forms.ComboBox]::new()
    $architecture.DropDownStyle = 'DropDownList'
    $architecture.Location = [Drawing.Point]::new(175, 209)
    $architecture.Size = [Drawing.Size]::new(445, 25)
    $architecture.Anchor = 'Top,Left,Right'
    foreach ($value in @('x64', 'ARM64')) {
        $display = if ($value -eq $hardware.Architecture) { "$value - recommended for this PC" } else { $value }
        [void]$architecture.Items.Add($display)
    }
    $architecture.SelectedIndex = if ($hardware.Architecture -eq 'ARM64') { 1 } else { 0 }
    $dialog.Controls.Add($architecture)
    Set-OrcaInputStyle $architecture

    Add-FieldLabel 'Build system' 248
    $generator = [System.Windows.Forms.ComboBox]::new()
    $generator.DropDownStyle = 'DropDownList'
    $generator.Location = [Drawing.Point]::new(175, 244)
    $generator.Size = [Drawing.Size]::new(445, 25)
    $generator.Anchor = 'Top,Left,Right'
    [void]$generator.Items.Add('Visual Studio - recommended and supported')
    [void]$generator.Items.Add('Ninja - experimental on the current toolchain')
    $generator.SelectedIndex = 0
    $dialog.Controls.Add($generator)
    Set-OrcaInputStyle $generator

    Add-FieldLabel 'Compiler workers' 283
    $automaticWorkers = [System.Windows.Forms.CheckBox]::new()
    $automaticWorkers.AutoSize = $true
    $automaticWorkers.Location = [Drawing.Point]::new(175, 280)
    $automaticWorkers.Text = "Automatic (recommended, up to $($hardware.RecommendedWorkers))"
    $automaticWorkers.Checked = $true
    $automaticWorkers.ForeColor = $script:Theme.Text
    $dialog.Controls.Add($automaticWorkers)

    $workers = [System.Windows.Forms.NumericUpDown]::new()
    $workers.Location = [Drawing.Point]::new(500, 278)
    $workers.Size = [Drawing.Size]::new(120, 25)
    $workers.Minimum = 1
    $workers.Maximum = 128
    $workers.Value = $hardware.RecommendedWorkers
    $workers.Enabled = $false
    $workers.Anchor = 'Top,Right'
    $dialog.Controls.Add($workers)
    Set-OrcaInputStyle $workers
    $automaticWorkers.Add_CheckedChanged(({
        $workers.Enabled = -not $automaticWorkers.Checked
    }).GetNewClosure())

    $advanced = [System.Windows.Forms.GroupBox]::new()
    $advanced.Location = [Drawing.Point]::new(20, 320)
    $advanced.Size = [Drawing.Size]::new(602, 105)
    $advanced.Text = '  REBUILD OPTIONS  '
    $advanced.Anchor = 'Top,Left,Right'
    $advanced.BackColor = $script:Theme.Surface
    $advanced.ForeColor = $script:Theme.Muted
    $advanced.Font = [Drawing.Font]::new('Segoe UI Semibold', 8)
    $dialog.Controls.Add($advanced)

    $fresh = [System.Windows.Forms.CheckBox]::new()
    $fresh.AutoSize = $true
    $fresh.Location = [Drawing.Point]::new(16, 25)
    $fresh.Text = 'Fresh configure (slower; discard cached CMake configuration)'
    $fresh.ForeColor = $script:Theme.Text
    $fresh.Font = [Drawing.Font]::new('Segoe UI', 9)
    $advanced.Controls.Add($fresh)

    $clean = [System.Windows.Forms.CheckBox]::new()
    $clean.AutoSize = $true
    $clean.Location = [Drawing.Point]::new(16, 50)
    $clean.Text = 'Clean before compiling (slowest; rebuild compiled files)'
    $clean.ForeColor = $script:Theme.Text
    $clean.Font = [Drawing.Font]::new('Segoe UI', 9)
    $advanced.Controls.Add($clean)

    $runAfter = [System.Windows.Forms.CheckBox]::new()
    $runAfter.AutoSize = $true
    $runAfter.Location = [Drawing.Point]::new(16, 75)
    $runAfter.Text = 'Launch OrcaSlicer after a successful build'
    $runAfter.ForeColor = $script:Theme.Text
    $runAfter.Font = [Drawing.Font]::new('Segoe UI', 9)
    $advanced.Controls.Add($runAfter)

    $summary = [System.Windows.Forms.Label]::new()
    $summary.Location = [Drawing.Point]::new(22, 436)
    $summary.Size = [Drawing.Size]::new(598, 42)
    $summary.ForeColor = $script:Theme.Muted
    $summary.Anchor = 'Bottom,Left,Right'
    $dialog.Controls.Add($summary)

    $cancel = [System.Windows.Forms.Button]::new()
    $cancel.Location = [Drawing.Point]::new(436, 493)
    $cancel.Size = [Drawing.Size]::new(88, 32)
    $cancel.Text = 'Cancel'
    $cancel.Anchor = 'Bottom,Right'
    $dialog.Controls.Add($cancel)
    Set-OrcaButtonStyle $cancel 'Ghost'

    $build = [System.Windows.Forms.Button]::new()
    $build.Location = [Drawing.Point]::new(532, 493)
    $build.Size = [Drawing.Size]::new(88, 32)
    $build.Text = 'Build'
    $build.Anchor = 'Bottom,Right'
    $dialog.Controls.Add($build)
    Set-OrcaButtonStyle $build 'Primary'
    $dialog.AcceptButton = $build
    $cancel.Add_Click(({ $dialog.Close() }).GetNewClosure())

    $updateBuildSummary = {
        if (-not $configuration.SelectedItem -or -not $scope.SelectedItem) { return }
        $kindName = if ($kind.SelectedIndex -eq 0) { 'production' } else { 'development' }
        $configName = ($configuration.SelectedItem.ToString() -split ' ')[0]
        $scopeName = if ($scope.SelectedIndex -eq 0) { 'complete source build' } else { 'application-only incremental build' }
        $workerText = if ($automaticWorkers.Checked) { "automatic, capped at $($hardware.RecommendedWorkers)" } else { [int]$workers.Value }
        $summary.Text = "This creates a $kindName $configName $scopeName using $workerText compiler workers. Build output is saved in .\logs."
    }.GetNewClosure()

    $updateBuildChoices = {
        $production = $kind.SelectedIndex -eq 0
        $configuration.Items.Clear()
        if ($production) {
            [void]$configuration.Items.Add('Release - optimized and recommended')
        }
        else {
            [void]$configuration.Items.Add('RelWithDebInfo - recommended for diagnostics')
            [void]$configuration.Items.Add('Debug - faster compilation, largest and slowest-running build')
        }
        $configuration.SelectedIndex = 0

        $scope.Items.Clear()
        [void]$scope.Items.Add('Complete - dependencies, app, translations, and install (recommended for a new build)')
        [void]$scope.Items.Add('Application only - faster; requires compatible dependencies already built')
        $scope.SelectedIndex = 0
        & $updateBuildSummary
    }.GetNewClosure()

    $kind.Add_SelectedIndexChanged(({ & $updateBuildChoices }).GetNewClosure())
    $configuration.Add_SelectedIndexChanged(({ & $updateBuildSummary }).GetNewClosure())
    $scope.Add_SelectedIndexChanged(({ & $updateBuildSummary }).GetNewClosure())
    $automaticWorkers.Add_CheckedChanged(({ & $updateBuildSummary }).GetNewClosure())
    $workers.Add_ValueChanged(({ & $updateBuildSummary }).GetNewClosure())

    $launcherState = $script:LauncherState
    $build.Add_Click(({
        $configName = ($configuration.SelectedItem.ToString() -split ' ')[0]
        $archName = ($architecture.SelectedItem.ToString() -split ' ')[0]
        $arguments = @('-Configuration', $configName, '-Architecture', $archName)
        if ($automaticWorkers.Checked) {
            $arguments += @('-Parallel', '0', '-MaxParallel', $hardware.RecommendedWorkers.ToString())
        }
        else {
            $arguments += @('-Parallel', ([int]$workers.Value).ToString(), '-MaxParallel', ([int]$workers.Value).ToString())
        }
        if ($scope.SelectedIndex -eq 0) {
            $arguments += '-Install'
        }
        else {
            $arguments += @('-Target', 'App', '-BuildOnly')
        }
        if ($generator.SelectedIndex -eq 1) { $arguments += '-Ninja' }
        if ($fresh.Checked) { $arguments += '-Fresh' }
        if ($clean.Checked) { $arguments += '-CleanFirst' }
        if ($runAfter.Checked) { $arguments += '-Run' }

        $dialog.Tag = [pscustomobject]@{
            Kind = if ($kind.SelectedIndex -eq 0) { 'Production' } else { 'Development' }
            Configuration = $configName
            Architecture = $archName
            RequiresPerl = $scope.SelectedIndex -eq 0
            Arguments = $arguments
        }
        $launcherState.EmbeddedBuildRequest = $dialog.Tag
        $dialog.Close()
        $newBuildButton.PerformClick()
    }).GetNewClosure())

    & $updateBuildChoices
    Show-EmbeddedView $dialog
}

$form = [System.Windows.Forms.Form]::new()
$form.Text = 'OrcaSlicer Launcher'
$form.StartPosition = 'CenterScreen'
$form.MinimumSize = [Drawing.Size]::new(1020, 680)
$form.ClientSize = [Drawing.Size]::new(1120, 700)
$form.Font = [Drawing.Font]::new('Segoe UI', 9)
$form.BackColor = $script:Theme.Canvas
$form.ForeColor = $script:Theme.Text
$form.AutoScaleMode = [System.Windows.Forms.AutoScaleMode]::Dpi
$form.KeyPreview = $true
try {
    $iconPath = Join-Path $script:RepoRoot 'resources\images\OrcaSlicer.ico'
    if (Test-Path -LiteralPath $iconPath) { $form.Icon = [Drawing.Icon]::new($iconPath) }
}
catch { }

$hero = [System.Windows.Forms.Panel]::new()
$hero.Location = [Drawing.Point]::new(0, 0)
$hero.Size = [Drawing.Size]::new(1120, 150)
$hero.Anchor = 'Top,Left,Right'
$hero.BackColor = $script:Theme.Surface
$hero.Add_Paint({ param($sender, $eventArgs)
    $graphics = $eventArgs.Graphics
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $gradient = [Drawing.Drawing2D.LinearGradientBrush]::new(
        $hero.ClientRectangle,
        [Drawing.Color]::FromArgb(18, 58, 71),
        [Drawing.Color]::FromArgb(35, 24, 68),
        8
    )
    $graphics.FillRectangle($gradient, $hero.ClientRectangle)
    $gradient.Dispose()

    $cyanOrb = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(35, $script:Theme.Accent))
    $violetOrb = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(42, $script:Theme.Violet))
    $graphics.FillEllipse($cyanOrb, $hero.Width - 330, -125, 330, 330)
    $graphics.FillEllipse($violetOrb, $hero.Width - 120, 35, 180, 180)
    $cyanOrb.Dispose()
    $violetOrb.Dispose()

    $gridPen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(18, 255, 255, 255), 1)
    for ($x = $hero.Width - 420; $x -lt $hero.Width; $x += 28) {
        $graphics.DrawLine($gridPen, $x, 0, $x + 145, $hero.Height)
    }
    $gridPen.Dispose()
})
$form.Controls.Add($hero)

$logo = [System.Windows.Forms.PictureBox]::new()
$logo.Location = [Drawing.Point]::new(28, 26)
$logo.Size = [Drawing.Size]::new(86, 86)
$logo.SizeMode = [System.Windows.Forms.PictureBoxSizeMode]::Zoom
$logo.BackColor = [Drawing.Color]::Transparent
try {
    $logoPath = Join-Path $script:RepoRoot 'resources\images\OrcaSlicer_192px_transparent.png'
    if (Test-Path -LiteralPath $logoPath) { $logo.Image = [Drawing.Image]::FromFile($logoPath) }
}
catch { }
Set-OrcaRoundedRegion $logo 18
$hero.Controls.Add($logo)

$eyebrow = [System.Windows.Forms.Label]::new()
$eyebrow.AutoSize = $true
$eyebrow.Font = [Drawing.Font]::new('Segoe UI Semibold', 8)
$eyebrow.ForeColor = $script:Theme.Accent
$eyebrow.BackColor = [Drawing.Color]::Transparent
$eyebrow.Location = [Drawing.Point]::new(134, 24)
$eyebrow.Text = 'ORCASLICER LAUNCHER'
$hero.Controls.Add($eyebrow)

$title = [System.Windows.Forms.Label]::new()
$title.AutoSize = $true
$title.Font = [Drawing.Font]::new('Segoe UI Semibold', 24)
$title.ForeColor = $script:Theme.Text
$title.BackColor = [Drawing.Color]::Transparent
$title.Location = [Drawing.Point]::new(128, 38)
$title.Text = 'Build and launch OrcaSlicer'
$hero.Controls.Add($title)

$subtitle = [System.Windows.Forms.Label]::new()
$subtitle.AutoSize = $true
$subtitle.ForeColor = $script:Theme.Muted
$subtitle.Font = [Drawing.Font]::new('Segoe UI', 9.5)
$subtitle.BackColor = [Drawing.Color]::Transparent
$subtitle.Location = [Drawing.Point]::new(134, 88)
$subtitle.Text = 'Select an existing build, or build OrcaSlicer from this source code.'
$hero.Controls.Add($subtitle)

$specParts = @("$($hardware.LogicalProcessors) threads")
if ($hardware.MemoryGB) { $specParts += "$($hardware.MemoryGB) GB" }
$specParts += $hardware.Architecture
$machineChip = [System.Windows.Forms.Label]::new()
$machineChip.AutoSize = $false
$machineChip.Location = [Drawing.Point]::new(134, 111)
$machineChip.Size = [Drawing.Size]::new(520, 25)
$machineChip.TextAlign = [Drawing.ContentAlignment]::MiddleCenter
$machineChip.BackColor = [Drawing.Color]::FromArgb(40, 9, 16, 26)
$machineChip.ForeColor = $script:Theme.Accent
$machineChip.Font = [Drawing.Font]::new('Segoe UI Semibold', 8)
$machineChip.Text = (($specParts -join "   $($script:Glyph.Separator)   ").ToUpperInvariant()) + "   $($script:Glyph.Separator)   RECOMMENDED: $($hardware.RecommendedWorkers) COMPILER WORKERS"
Set-OrcaRoundedRegion $machineChip 11
$hero.Controls.Add($machineChip)

$shortcutButton = [System.Windows.Forms.Button]::new()
$shortcutButton.Anchor = 'Top,Right'
$shortcutButton.Location = [Drawing.Point]::new(920, 27)
$shortcutButton.Size = [Drawing.Size]::new(170, 38)
$shortcutButton.Text = "$($script:Glyph.External)  Create shortcut"
$hero.Controls.Add($shortcutButton)
Set-OrcaButtonStyle $shortcutButton 'Ghost'

$heroHint = [System.Windows.Forms.Label]::new()
$heroHint.Anchor = 'Top,Right'
$heroHint.AutoSize = $false
$heroHint.Location = [Drawing.Point]::new(815, 76)
$heroHint.Size = [Drawing.Size]::new(275, 42)
$heroHint.TextAlign = [Drawing.ContentAlignment]::MiddleRight
$heroHint.ForeColor = $script:Theme.Muted
$heroHint.BackColor = [Drawing.Color]::Transparent
$heroHint.Text = "SOURCE FOLDER`r`n$script:RepoRoot"
$hero.Controls.Add($heroHint)
$subtitle.BringToFront()
$heroHint.BringToFront()

$buildCard = [System.Windows.Forms.Panel]::new()
$buildCard.Anchor = 'Top,Bottom,Left,Right'
$buildCard.Location = [Drawing.Point]::new(22, 170)
$buildCard.Size = [Drawing.Size]::new(1076, 356)
$buildCard.BackColor = $script:Theme.Surface
Set-OrcaRoundedRegion $buildCard 14
$buildCard.Add_Paint({ param($sender, $eventArgs)
    $eventArgs.Graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $bounds = [Drawing.Rectangle]::new(0, 0, $buildCard.Width - 1, $buildCard.Height - 1)
    $path = New-OrcaRoundedPath $bounds 14
    $pen = [Drawing.Pen]::new($script:Theme.Border)
    $eventArgs.Graphics.DrawPath($pen, $path)
    $pen.Dispose()
    $path.Dispose()
})
$form.Controls.Add($buildCard)

$buildHeading = [System.Windows.Forms.Label]::new()
$buildHeading.AutoSize = $true
$buildHeading.Font = [Drawing.Font]::new('Segoe UI Semibold', 11)
$buildHeading.ForeColor = $script:Theme.Text
$buildHeading.Location = [Drawing.Point]::new(18, 14)
$buildHeading.Text = 'ORCASLICER BUILDS'
$buildCard.Controls.Add($buildHeading)

$buildCountLabel = [System.Windows.Forms.Label]::new()
$buildCountLabel.Anchor = 'Top,Right'
$buildCountLabel.AutoSize = $false
$buildCountLabel.Location = [Drawing.Point]::new(922, 10)
$buildCountLabel.Size = [Drawing.Size]::new(136, 27)
$buildCountLabel.TextAlign = [Drawing.ContentAlignment]::MiddleCenter
$buildCountLabel.BackColor = $script:Theme.SurfaceLift
$buildCountLabel.ForeColor = $script:Theme.Muted
$buildCountLabel.Font = [Drawing.Font]::new('Segoe UI Semibold', 8)
$buildCountLabel.Text = 'SCANNING...'
Set-OrcaRoundedRegion $buildCountLabel 12
$buildCard.Controls.Add($buildCountLabel)

$deleteButton = [System.Windows.Forms.Button]::new()
$deleteButton.Anchor = 'Top,Right'
$deleteButton.Location = [Drawing.Point]::new(794, 8)
$deleteButton.Size = [Drawing.Size]::new(116, 32)
$deleteButton.Text = 'Delete build'
$deleteButton.Enabled = $false
$buildCard.Controls.Add($deleteButton)
Set-OrcaButtonStyle $deleteButton 'Danger'

$list = [System.Windows.Forms.ListView]::new()
$list.Anchor = 'Top,Bottom,Left,Right'
$list.Location = [Drawing.Point]::new(16, 49)
$list.Size = [Drawing.Size]::new(1044, 291)
$list.View = 'Details'
$list.FullRowSelect = $true
$list.MultiSelect = $false
$list.HideSelection = $false
$list.Font = [Drawing.Font]::new('Segoe UI', 9.5)
[void] $list.Columns.Add('Pinned', 65)
[void] $list.Columns.Add('Type', 100)
[void] $list.Columns.Add('Build / version', 265)
[void] $list.Columns.Add('Version', 90)
[void] $list.Columns.Add('Size (files)', 115)
[void] $list.Columns.Add('Location / shared files', 390)
$list.ShowItemToolTips = $true
$buildCard.Controls.Add($list)
Set-OrcaListViewStyle $list 'Builds'
$buildRowHeight = [System.Windows.Forms.ImageList]::new()
$buildRowHeight.ImageSize = [Drawing.Size]::new(1, 38)
$list.SmallImageList = $buildRowHeight

$statusCard = [System.Windows.Forms.Panel]::new()
$statusCard.Anchor = 'Bottom,Left,Right'
$statusCard.Location = [Drawing.Point]::new(22, 544)
$statusCard.Size = [Drawing.Size]::new(1076, 60)
$statusCard.BackColor = $script:Theme.Surface
Set-OrcaRoundedRegion $statusCard 12
$form.Controls.Add($statusCard)

$statusIndicator = [System.Windows.Forms.Label]::new()
$statusIndicator.AutoSize = $false
$statusIndicator.Location = [Drawing.Point]::new(17, 15)
$statusIndicator.Size = [Drawing.Size]::new(24, 28)
$statusIndicator.TextAlign = [Drawing.ContentAlignment]::MiddleCenter
$statusIndicator.Font = [Drawing.Font]::new('Segoe UI Symbol', 12)
$statusIndicator.ForeColor = $script:Theme.Accent
$statusIndicator.Text = [string]$script:Glyph.Status
$statusCard.Controls.Add($statusIndicator)

$status = [System.Windows.Forms.Label]::new()
$status.Anchor = 'Top,Bottom,Left,Right'
$status.AutoSize = $false
$status.Location = [Drawing.Point]::new(47, 12)
$status.Size = [Drawing.Size]::new(820, 36)
$status.TextAlign = [Drawing.ContentAlignment]::MiddleLeft
$status.ForeColor = $script:Theme.Text
$status.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
$statusCard.Controls.Add($status)

$resolutionButton = [System.Windows.Forms.Button]::new()
$resolutionButton.Anchor = 'Top,Right'
$resolutionButton.Location = [Drawing.Point]::new(895, 12)
$resolutionButton.Size = [Drawing.Size]::new(163, 36)
$resolutionButton.Visible = $false
$statusCard.Controls.Add($resolutionButton)
Set-OrcaButtonStyle $resolutionButton 'Secondary'

$statusProgress = [System.Windows.Forms.Panel]::new()
$statusProgress.Anchor = 'Bottom,Left'
$statusProgress.Location = [Drawing.Point]::new(0, 56)
$statusProgress.Size = [Drawing.Size]::new(0, 4)
$statusProgress.BackColor = $script:Theme.Accent
$statusProgress.Visible = $false
$statusCard.Controls.Add($statusProgress)

$commandBar = [System.Windows.Forms.Panel]::new()
$commandBar.Anchor = 'Bottom,Left,Right'
$commandBar.Location = [Drawing.Point]::new(22, 622)
$commandBar.Size = [Drawing.Size]::new(1076, 48)
$commandBar.BackColor = $script:Theme.Canvas
$form.Controls.Add($commandBar)

$primaryActions = [System.Windows.Forms.FlowLayoutPanel]::new()
$primaryActions.Dock = [System.Windows.Forms.DockStyle]::Left
$primaryActions.Size = [Drawing.Size]::new(570, 48)
$primaryActions.WrapContents = $false
$primaryActions.BackColor = $script:Theme.Canvas
$commandBar.Controls.Add($primaryActions)

$secondaryActions = [System.Windows.Forms.FlowLayoutPanel]::new()
$secondaryActions.Dock = [System.Windows.Forms.DockStyle]::Right
$secondaryActions.Size = [Drawing.Size]::new(500, 48)
$secondaryActions.FlowDirection = [System.Windows.Forms.FlowDirection]::LeftToRight
$secondaryActions.WrapContents = $false
$secondaryActions.BackColor = $script:Theme.Canvas
$commandBar.Controls.Add($secondaryActions)

$script:resolutionAction = $null

function Write-LauncherError {
    param([string] $Context, [Exception] $Exception)
    try {
        $logDirectory = Join-Path $script:RepoRoot 'logs'
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
        $errorLog = Join-Path $logDirectory 'launcher_errors.log'
        $details = "[$(Get-Date -Format o)] $Context`r`n$($Exception.ToString())`r`n"
        Add-Content -LiteralPath $errorLog -Value $details -Encoding UTF8
        return $errorLog
    }
    catch { return $null }
}

function Set-LauncherNotice {
    param(
        [string] $Message,
        [string] $ActionText,
        [scriptblock] $Action
    )
    $status.Text = $Message
    $statusIndicator.ForeColor = if ($Message -match '(?i)(error|failed|stopped|could not|missing)') {
        $script:Theme.Danger
    }
    elseif ($Message -match '(?i)(ready|completed|found|selected|pinned|shortcut)') {
        $script:Theme.Success
    }
    elseif ($Message -match '(?i)(launching|running|build)') {
        $script:Theme.Accent
    }
    else { $script:Theme.Muted }
    if ($Message -match '(?i)in ([123]) seconds') {
        $remaining = [int]$Matches[1]
        $statusProgress.Width = [Math]::Max(1, [int]($statusCard.ClientSize.Width * ($remaining / 3.0)))
        $statusProgress.Visible = $true
    }
    else {
        $statusProgress.Visible = $false
        $statusProgress.Width = 0
    }
    $script:resolutionAction = $Action
    $resolutionButton.Visible = [bool]($ActionText -and $Action)
    if ($resolutionButton.Visible) { $resolutionButton.Text = $ActionText }
}

$resolutionButton.Add_Click({
    if ($script:resolutionAction) { & $script:resolutionAction }
})

$threadExceptionHandler = [System.Threading.ThreadExceptionEventHandler]{
    param($sender, $eventArgs)
    $errorLog = Write-LauncherError 'Unhandled launcher UI event' $eventArgs.Exception
    $openErrorLog = if ($errorLog) {
        { Start-Process -FilePath 'explorer.exe' -ArgumentList "/select,`"$errorLog`"" }.GetNewClosure()
    }
    else { $null }
    Set-LauncherNotice 'The launcher recovered from an error. Details were saved for troubleshooting.' 'Open error log' $openErrorLog
}
[System.Windows.Forms.Application]::add_ThreadException($threadExceptionHandler)

$newBuildButton = [System.Windows.Forms.Button]::new()
$newBuildButton.Size = [Drawing.Size]::new(158, 42)
$newBuildButton.Margin = [System.Windows.Forms.Padding]::new(0, 2, 10, 2)
$newBuildButton.Text = "$($script:Glyph.Plus)  New build"
$primaryActions.Controls.Add($newBuildButton)
Set-OrcaButtonStyle $newBuildButton 'Primary'

$prerequisitesButton = [System.Windows.Forms.Button]::new()
$prerequisitesButton.Size = [Drawing.Size]::new(136, 42)
$prerequisitesButton.Margin = [System.Windows.Forms.Padding]::new(0, 2, 10, 2)
$prerequisitesButton.Text = 'Check requirements'
$primaryActions.Controls.Add($prerequisitesButton)
Set-OrcaButtonStyle $prerequisitesButton 'Secondary'

$autoLaunchPill = [System.Windows.Forms.Panel]::new()
$autoLaunchPill.Size = [Drawing.Size]::new(250, 42)
$autoLaunchPill.Margin = [System.Windows.Forms.Padding]::new(0, 2, 0, 2)
$autoLaunchPill.BackColor = $script:Theme.Surface
Set-OrcaRoundedRegion $autoLaunchPill 10
$primaryActions.Controls.Add($autoLaunchPill)

$autoLaunchCheck = [System.Windows.Forms.CheckBox]::new()
$autoLaunchCheck.AutoSize = $true
$autoLaunchCheck.Location = [Drawing.Point]::new(14, 11)
$autoLaunchCheck.Text = 'Launch automatically after 3 seconds'
$autoLaunchCheck.ForeColor = $script:Theme.Text
$autoLaunchCheck.Font = [Drawing.Font]::new('Segoe UI Semibold', 9)
$autoLaunchCheck.Checked = [bool]($settings.SetupComplete -and $settings.AutoLaunchEnabled)
$autoLaunchPill.Controls.Add($autoLaunchCheck)

$pinButton = [System.Windows.Forms.Button]::new()
$pinButton.Size = [Drawing.Size]::new(92, 42)
$pinButton.Margin = [System.Windows.Forms.Padding]::new(0, 2, 8, 2)
$pinButton.Text = 'Pin selected'
$pinButton.Enabled = $false
$secondaryActions.Controls.Add($pinButton)
Set-OrcaButtonStyle $pinButton 'Secondary'

$logsButton = [System.Windows.Forms.Button]::new()
$logsButton.Size = [Drawing.Size]::new(76, 42)
$logsButton.Margin = [System.Windows.Forms.Padding]::new(0, 2, 8, 2)
$logsButton.Text = 'Open logs'
$secondaryActions.Controls.Add($logsButton)
Set-OrcaButtonStyle $logsButton 'Secondary'

$refreshButton = [System.Windows.Forms.Button]::new()
$refreshButton.Size = [Drawing.Size]::new(86, 42)
$refreshButton.Margin = [System.Windows.Forms.Padding]::new(0, 2, 8, 2)
$refreshButton.Text = "$($script:Glyph.Refresh)  Refresh"
$secondaryActions.Controls.Add($refreshButton)
Set-OrcaButtonStyle $refreshButton 'Secondary'

$folderButton = [System.Windows.Forms.Button]::new()
$folderButton.Size = [Drawing.Size]::new(94, 42)
$folderButton.Margin = [System.Windows.Forms.Padding]::new(0, 2, 8, 2)
$folderButton.Text = 'Open folder'
$folderButton.Enabled = $false
$secondaryActions.Controls.Add($folderButton)
Set-OrcaButtonStyle $folderButton 'Secondary'

$launchButton = [System.Windows.Forms.Button]::new()
$launchButton.Size = [Drawing.Size]::new(113, 42)
$launchButton.Margin = [System.Windows.Forms.Padding]::new(0, 2, 0, 2)
$launchButton.Text = "Launch  $($script:Glyph.Play)"
$launchButton.Enabled = $false
$form.AcceptButton = $launchButton
$secondaryActions.Controls.Add($launchButton)
Set-OrcaButtonStyle $launchButton 'Primary'

function Update-TargetList {
    $list.BeginUpdate()
    try {
        $list.Items.Clear()
        $script:launchTargets = @(Get-OrcaLaunchTargets)
        $script:buildGroups = @(Get-OrcaBuildGroups $script:launchTargets)
        $groupedPaths = @{}
        $displayRows = [Collections.Generic.List[object]]::new()
        foreach ($group in $script:buildGroups) {
            $displayRows.Add($group)
            foreach ($target in $group.Targets) {
                $groupedPaths[$target.Path] = $true
                $displayRows.Add($target)
            }
        }
        foreach ($target in $script:launchTargets) {
            if (-not $groupedPaths.ContainsKey($target.Path)) { $displayRows.Add($target) }
        }
        foreach ($target in $displayRows) {
            if ($target.IsBuildGroup) {
                $group = $target
                $row = [System.Windows.Forms.ListViewItem]::new('')
                [void]$row.SubItems.Add('Build folder')
                [void]$row.SubItems.Add($group.Name)
                [void]$row.SubItems.Add("$($group.Targets.Count) outputs")
                [void]$row.SubItems.Add((Format-BuildSize $group.Bytes))
                [void]$row.SubItems.Add("Shared / other files: $(Format-BuildSize $group.SharedBytes)")
                $row.Tag = $group
                $row.ToolTipText = "$($group.Path)`r`nTotal includes all outputs and shared build files. Linked destinations are excluded. File sizes estimate storage; space reclaimed can differ."
                [void]$list.Items.Add($row)
                continue
            }
            $row = [System.Windows.Forms.ListViewItem]::new($(if ($target.Path -eq $settings.PinnedTargetPath) { 'Pinned' } else { '' }))
            [void] $row.SubItems.Add($target.Kind)
            $label = "$($target.Configuration) ($($target.Architecture))"
            if ($target.Source -like '* (staged)') { $label += ' - staged' }
            if ($groupedPaths.ContainsKey($target.Path)) { $label = '    > ' + $label }
            [void] $row.SubItems.Add($label)
            [void] $row.SubItems.Add($target.Version)
            $size = if ($groupedPaths.ContainsKey($target.Path)) {
                Format-BuildSize $group.Sizes[(Split-Path -Parent $target.Path)]
            } else { 'Not measured' }
            [void] $row.SubItems.Add($size)
            [void] $row.SubItems.Add($target.Path)
            $row.Tag = $target
            $row.ToolTipText = "Updated: $($target.Updated.ToString('g'))`r`nOutput size is included in the build folder total. Shared files remain when only this output is deleted."
            [void] $list.Items.Add($row)
        }
    }
    finally {
        $list.EndUpdate()
    }

    $count = $script:buildGroups.Count + @($script:launchTargets | Where-Object { -not $groupedPaths.ContainsKey($_.Path) }).Count
    $buildCountLabel.Text = if ($count -eq 1) { '1 BUILD FOUND' } else { "$count BUILDS FOUND" }
    $buildCountLabel.ForeColor = if ($count) { $script:Theme.Success } else { $script:Theme.Warning }
    Set-LauncherNotice $(if ($count -eq 1) { '1 build found' } else { "$count builds found" })
    if ($count -eq 0) {
        Set-LauncherNotice 'No builds found. Create a source build or install a production release.' 'Create build' { $newBuildButton.PerformClick() }
    }
    elseif ($list.Items.Count -gt 0) {
        $list.Items[0].Selected = $true
        $list.Items[0].Focused = $true
    }
}

function Start-SelectedTarget {
    if ($list.SelectedItems.Count -eq 0) { return }
    $target = $list.SelectedItems[0].Tag
    if ($target.IsBuildGroup) { return }
    Start-LaunchTarget $target
}

function Start-LaunchTarget {
    param($Target)
    try {
        Start-Process -FilePath $Target.Path -WorkingDirectory (Split-Path -Parent $Target.Path)
        $settings.SetupComplete = $true
        $settings.LastTargetPath = $Target.Path
        $settings.AutoLaunchEnabled = $autoLaunchCheck.Checked
        Save-LauncherSettings $settings
        $form.Close()
    }
    catch {
        [void](Write-LauncherError 'Launch OrcaSlicer' $_.Exception)
        $openTargetFolder = { Start-Process -FilePath 'explorer.exe' -ArgumentList "/select,`"$($Target.Path)`"" }.GetNewClosure()
        Set-LauncherNotice 'OrcaSlicer could not be launched from this build.' 'Open build folder' $openTargetFolder
    }
}

$list.Add_SelectedIndexChanged({
    $hasSelection = $list.SelectedItems.Count -gt 0
    $isGroup = $hasSelection -and $list.SelectedItems[0].Tag.IsBuildGroup
    $launchButton.Enabled = $hasSelection -and -not $isGroup
    $folderButton.Enabled = $hasSelection
    $pinButton.Enabled = $hasSelection -and -not $isGroup
    $deleteButton.Text = if ($isGroup) { 'Delete build' } else { 'Delete output' }
    $deleteButton.Enabled = $hasSelection -and [bool](Get-LocalBuildDirectory $list.SelectedItems[0].Tag)
    $pinButton.Text = if ($hasSelection -and $list.SelectedItems[0].Tag.Path -eq $settings.PinnedTargetPath) {
        'Unpin selected'
    }
    else { 'Pin selected' }
})
$list.Add_DoubleClick({ Start-SelectedTarget })
$list.Add_MouseDown({
    if ($autoLaunchTimer.Enabled) {
        Stop-AutoLaunchCountdown
        Set-LauncherNotice 'Auto-launch paused while you choose a build.'
    }
})
$launchButton.Add_Click({ Start-SelectedTarget })
$deleteButton.Add_Click({
    if ($list.SelectedItems.Count -eq 0) { return }
    Stop-AutoLaunchCountdown
    $target = $list.SelectedItems[0].Tag
    try {
        $directory = Get-LocalBuildDirectory $target
        if (-not $directory) { throw 'Only local build folders can be deleted.' }
        $message = if ($target.IsBuildGroup) {
            $versions = ($target.Targets | ForEach-Object { "$($_.Configuration) ($($_.Architecture)) - $($_.Version)" }) -join "`r`n"
            "Permanently delete the entire build '$($target.Name)'?`r`n`r`n$directory`r`nSize (files): $(Format-BuildSize $target.Bytes)`r`n`r`nThis removes shared build files and every output beneath this entry:`r`n$versions"
        } else {
            "Permanently delete the selected $($target.Configuration) output?`r`n`r`n$directory`r`n`r`nOther listed outputs and shared build files will be kept. You will need to rebuild this output to use it again."
        }
        $answer = [System.Windows.Forms.MessageBox]::Show($form,
            $message,
            'Delete build', [System.Windows.Forms.MessageBoxButtons]::YesNo,
            [System.Windows.Forms.MessageBoxIcon]::Warning, [System.Windows.Forms.MessageBoxDefaultButton]::Button2)
        if ($answer -ne [System.Windows.Forms.DialogResult]::Yes) { return }
        Remove-LocalBuild $target
        Update-TargetList
        Set-LauncherNotice 'Build deleted.'
    }
    catch {
        [void](Write-LauncherError 'Delete local build' $_.Exception)
        Update-TargetList
        Set-LauncherNotice "Could not delete build: $($_.Exception.Message)"
    }
})
$refreshButton.Add_Click({
    Stop-AutoLaunchCountdown
    Update-TargetList
})
$folderButton.Add_Click({
    if ($list.SelectedItems.Count -gt 0) {
        Start-Process -FilePath 'explorer.exe' -ArgumentList "/select,`"$($list.SelectedItems[0].Tag.Path)`""
    }
})

$pinButton.Add_Click({
    if ($list.SelectedItems.Count -eq 0) { return }
    Stop-AutoLaunchCountdown
    $target = $list.SelectedItems[0].Tag
    if ($target.IsBuildGroup) { return }
    if ($target.Path -eq $settings.PinnedTargetPath) {
        $settings.PinnedTargetPath = $null
        Set-LauncherNotice 'Build unpinned. Auto-launch will use the last build or newest production build.'
    }
    else {
        $settings.PinnedTargetPath = $target.Path
        $settings.SetupComplete = $true
        $settings.AutoLaunchEnabled = $true
        $script:suppressAutoLaunchCountdown = $true
        $autoLaunchCheck.Enabled = $true
        $autoLaunchCheck.Checked = $true
        $script:suppressAutoLaunchCountdown = $false
        Set-LauncherNotice "$($target.Configuration) is pinned for auto-launch."
    }
    Save-LauncherSettings $settings
    Update-TargetList
})

$logsButton.Add_Click({
    Stop-AutoLaunchCountdown
    $logDirectory = Join-Path $script:RepoRoot 'logs'
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    Start-Process -FilePath 'explorer.exe' -ArgumentList $logDirectory
})

$autoLaunchTimer = [System.Windows.Forms.Timer]::new()
$autoLaunchTimer.Interval = 1000
$script:autoLaunchRemaining = 0
$script:autoLaunchTarget = $null
$script:suppressAutoLaunchCountdown = $false

function Stop-AutoLaunchCountdown {
    $autoLaunchTimer.Stop()
    $script:autoLaunchRemaining = 0
    $script:autoLaunchTarget = $null
}

function Start-AutoLaunchCountdown {
    Stop-AutoLaunchCountdown
    if (-not $autoLaunchCheck.Checked -or -not $settings.SetupComplete) { return }
    $target = $null
    if ($settings.PinnedTargetPath) {
        $target = $script:launchTargets | Where-Object { $_.Path -eq $settings.PinnedTargetPath } | Select-Object -First 1
    }
    if ($settings.LastTargetPath) {
        if (-not $target) {
            $target = $script:launchTargets | Where-Object { $_.Path -eq $settings.LastTargetPath } | Select-Object -First 1
        }
    }
    if (-not $target) {
        $target = $script:launchTargets | Where-Object { $_.Kind -eq 'Production' } |
            Sort-Object Updated -Descending | Select-Object -First 1
    }
    if (-not $target) {
        $autoLaunchCheck.Checked = $false
        Set-LauncherNotice 'Create a production build to enable auto-launch.' 'Create build' { $newBuildButton.PerformClick() }
        return
    }
    $script:autoLaunchTarget = $target
    $script:autoLaunchRemaining = 3
    Set-LauncherNotice "Launching $($target.Configuration) in 3 seconds..."
    $autoLaunchTimer.Start()
}

$autoLaunchTimer.Add_Tick({
    $script:autoLaunchRemaining--
    if ($script:autoLaunchRemaining -le 0) {
        $autoLaunchTimer.Stop()
        Start-LaunchTarget $script:autoLaunchTarget
    }
    else {
        Set-LauncherNotice "Launching $($script:autoLaunchTarget.Configuration) in $($script:autoLaunchRemaining) seconds..."
    }
})

$autoLaunchCheck.Add_CheckedChanged({
    $settings.AutoLaunchEnabled = $autoLaunchCheck.Checked
    Save-LauncherSettings $settings
    if (-not $autoLaunchCheck.Checked) {
        Stop-AutoLaunchCountdown
        if ($settings.SetupComplete) { Set-LauncherNotice 'Auto-launch is off.' }
    }
    elseif (-not $script:suppressAutoLaunchCountdown) {
        Start-AutoLaunchCountdown
    }
})

$buildTimer = [System.Windows.Forms.Timer]::new()
$buildTimer.Interval = 1500
$buildTimer.Add_Tick({
    if ($script:activeBuildProcess -and $script:activeBuildProcess.HasExited) {
        $exitCode = $script:activeBuildProcess.ExitCode
        $script:activeBuildProcess.Dispose()
        $script:activeBuildProcess = $null
        $buildTimer.Stop()
        $newBuildButton.Enabled = $true
        Update-TargetList
        if ($exitCode -eq 0) {
            $matchingTarget = $script:launchTargets | Where-Object {
                $_.Configuration -eq $script:activeBuildRequest.Configuration -and
                $_.Architecture -eq $script:activeBuildRequest.Architecture -and
                $_.Source -like 'build*'
            } | Sort-Object Updated -Descending | Select-Object -First 1
            if ($matchingTarget) { $settings.LastTargetPath = $matchingTarget.Path }
            if (-not $settings.SetupComplete) {
                $settings.SetupComplete = $true
                $settings.AutoLaunchEnabled = $true
                $script:suppressAutoLaunchCountdown = $true
                $autoLaunchCheck.Checked = $true
                $autoLaunchCheck.Enabled = $true
                $script:suppressAutoLaunchCountdown = $false
            }
            Save-LauncherSettings $settings
            Set-LauncherNotice 'Build completed. Auto-launch is enabled for the next startup.'
        }
        else {
            Set-LauncherNotice "Build stopped with exit code $exitCode." 'Open logs' { $logsButton.PerformClick() }
        }
    }
})

$newBuildButton.Add_Click({
    Stop-AutoLaunchCountdown
    if ($script:LauncherState.EmbeddedBuildRequest) {
        $request = $script:LauncherState.EmbeddedBuildRequest
        $script:LauncherState.EmbeddedBuildRequest = $null
    }
    else {
        Show-NewBuildDialog
        return
    }
    $missingPrerequisites = @(Get-OrcaPrerequisites | Where-Object {
        -not $_.Ready -and ($_.Name -ne 'Perl' -or $request.RequiresPerl)
    })
    if ($missingPrerequisites.Count) {
        Set-LauncherNotice 'This build needs additional tools.' 'Review prerequisites' { Show-PrerequisiteDialog }
        Show-PrerequisiteDialog
        $missingPrerequisites = @(Get-OrcaPrerequisites | Where-Object {
            -not $_.Ready -and ($_.Name -ne 'Perl' -or $request.RequiresPerl)
        })
        if ($missingPrerequisites.Count) { return }
    }
    $buildScript = Join-Path $script:RepoRoot 'scripts\build.ps1'
    if (-not (Test-Path -LiteralPath $buildScript)) {
        Set-LauncherNotice 'The source build script is missing from this checkout.' 'Open source folder' { Start-Process explorer.exe $script:RepoRoot }
        return
    }
    try {
        $argumentText = @('-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $buildScript, '-NoPause') + $request.Arguments |
            ForEach-Object { Quote-ProcessArgument $_ }
        $startInfo = [Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = Get-OrcaPowerShellHost
        $startInfo.Arguments = $argumentText -join ' '
        $startInfo.WorkingDirectory = $script:RepoRoot
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
        $script:activeBuildProcess = [Diagnostics.Process]::Start($startInfo)
        $script:activeBuildRequest = [pscustomobject]@{
            Configuration = $request.Configuration
            Architecture = $request.Architecture
        }
        $newBuildButton.Enabled = $false
        Set-LauncherNotice "$($request.Kind) $($request.Configuration) build is running. Output: .\logs"
        $buildTimer.Start()
    }
    catch {
        [void](Write-LauncherError 'Start source build' $_.Exception)
        Set-LauncherNotice 'The build could not be started.' 'Review prerequisites' { Show-PrerequisiteDialog }
    }
})

$prerequisitesButton.Add_Click({
    Stop-AutoLaunchCountdown
    Show-PrerequisiteDialog
})

$shortcutButton.Add_Click({
    Stop-AutoLaunchCountdown
    try {
        $shortcutPath = Install-OrcaLauncherShortcut
        Set-LauncherNotice "Desktop shortcut ready: $shortcutPath"
        $shortcutButton.Text = 'Shortcut created'
    }
    catch {
        [void](Write-LauncherError 'Create desktop shortcut' $_.Exception)
        Set-LauncherNotice 'The desktop shortcut could not be created.' 'Try again' { $shortcutButton.PerformClick() }
    }
})

$form.Add_Shown({
    if ($settings.SetupComplete) {
        Start-AutoLaunchCountdown
        return
    }

    $localTargets = @($script:launchTargets | Where-Object { $_.Source -like 'build*' })
    if ($localTargets.Count) {
        $preferredTarget = $localTargets | Where-Object { $_.Kind -eq 'Production' } |
            Sort-Object Updated -Descending | Select-Object -First 1
        if (-not $preferredTarget) {
            $preferredTarget = $localTargets | Sort-Object Updated -Descending | Select-Object -First 1
        }
        foreach ($item in $list.Items) {
            if ($item.Tag.Path -eq $preferredTarget.Path) {
                $item.Selected = $true
                $item.Focused = $true
                $item.EnsureVisible()
                break
            }
        }
        $settings.SetupComplete = $true
        $settings.AutoLaunchEnabled = $false
        Save-LauncherSettings $settings
        $autoLaunchCheck.Enabled = $true
        Set-LauncherNotice 'Existing build selected. Launch it, pin it, or enable auto-launch.'
    }
    else {
        $autoLaunchCheck.Enabled = $false
        Set-LauncherNotice 'No local build found.' 'Create build' { $newBuildButton.PerformClick() }
    }
})

Update-TargetList
if ($EmbeddedViewSmokeTest) {
    # Materialize the list so programmatic selection raises native change events.
    $null = $list.Handle
    foreach ($group in $script:buildGroups) {
        $parentRow = @($list.Items | Where-Object { $_.Tag.IsBuildGroup -and $_.Tag.Path -eq $group.Path })
        if ($parentRow.Count -ne 1) { throw 'Expected one parent row per build folder.' }
        $list.SelectedItems.Clear()
        $parentRow[0].Selected = $true
        if ($launchButton.Enabled -or $pinButton.Enabled -or $deleteButton.Text -ne 'Delete build') {
            throw 'Parent build actions do not match the selected scope.'
        }
        foreach ($target in $group.Targets) {
            $childRow = @($list.Items | Where-Object { -not $_.Tag.IsBuildGroup -and $_.Tag.Path -eq $target.Path })
            if ($childRow.Count -ne 1) { throw 'Expected one child row per output.' }
            $list.SelectedItems.Clear()
            $childRow[0].Selected = $true
            if (-not $launchButton.Enabled -or -not $pinButton.Enabled -or $deleteButton.Text -ne 'Delete output') {
                throw 'Output actions do not match the selected scope.'
            }
        }
    }
    $smokeErrorLog = Join-Path $script:RepoRoot 'logs\launcher_errors.log'
    $smokeErrorLength = if (Test-Path -LiteralPath $smokeErrorLog) { (Get-Item -LiteralPath $smokeErrorLog).Length } else { 0 }
    Show-NewBuildDialog
    $cancelButton = $script:embeddedView.Controls | Where-Object {
        $_ -is [System.Windows.Forms.Button] -and $_.Text -eq 'Cancel'
    } | Select-Object -First 1
    if (-not $cancelButton) { throw 'Build wizard Cancel button was not found.' }
    $cancelButton.PerformClick()
    if ($script:embeddedView.Visible) { throw 'Build wizard remained visible after Cancel.' }

    Show-PrerequisiteDialog
    $closeButton = $script:embeddedView.Controls | Where-Object {
        $_ -is [System.Windows.Forms.Button] -and $_.Text -eq 'Close'
    } | Select-Object -First 1
    if (-not $closeButton) { throw 'Prerequisite Close button was not found.' }
    $closeButton.PerformClick()
    if ($script:embeddedView.Visible) { throw 'Prerequisite view remained visible after Close.' }
    $newSmokeErrorLength = if (Test-Path -LiteralPath $smokeErrorLog) { (Get-Item -LiteralPath $smokeErrorLog).Length } else { 0 }
    if ($newSmokeErrorLength -ne $smokeErrorLength) { throw 'Embedded wizard generated a recovered UI error.' }
    Write-Output 'Embedded wizard close actions passed.'
    exit 0
}
[void] $form.ShowDialog()
