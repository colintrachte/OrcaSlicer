<#
.SYNOPSIS
    Build OrcaSlicer on Windows, auto-installing any missing prerequisites first.

.DESCRIPTION
    A build script that self-heals missing dependencies: it checks for the build
    prerequisites (Visual Studio 2022 C++ toolchain, Strawberry Perl, Git) and
    installs whatever is absent, then builds the third-party dependencies (once)
    and the OrcaSlicer app. Idempotent: anything already present is skipped, so
    it doubles as your normal build command. Safe to re-run.

    Prerequisites are installed with winget. Visual Studio is the only large
    download; everything else is small.

.PARAMETER Config
    Build configuration: Release, RelWithDebInfo (default), or Debug.
    RelWithDebInfo gives a runnable app plus debug symbols.

.PARAMETER Jobs
    Max parallel compiler processes for the app build. Defaults to a value sized
    to available RAM (~1 per 4 GB) because each RelWithDebInfo PCH needs ~2 GB and
    unbounded parallelism OOMs (MSVC C3859/C1076).

.PARAMETER SkipDeps
    Skip the (slow, one-time) third-party dependency build. Use once deps exist.

.PARAMETER SkipApp
    Install prerequisites and build deps only; don't build the app.

.PARAMETER Run
    Launch the built OrcaSlicer.exe when the build succeeds.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\setup_windows.ps1
.EXAMPLE
    .\setup_windows.ps1 -Config Release -SkipDeps -Run
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'RelWithDebInfo', 'Debug')]
    [string]$Config = 'RelWithDebInfo',
    [int]$Jobs = 0,
    [switch]$SkipDeps,
    [switch]$SkipApp,
    [switch]$Run
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $MyInvocation.MyCommand.Path

function Info($m)  { Write-Host "[setup] $m"         -ForegroundColor Cyan }
function Ok($m)    { Write-Host "[ ok  ] $m"         -ForegroundColor Green }
function Warn($m)  { Write-Host "[warn ] $m"         -ForegroundColor Yellow }
function Die($m)   { Write-Host "[fail ] $m"         -ForegroundColor Red; exit 1 }

# --- 0. winget ------------------------------------------------------------
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Die "winget not found. Install 'App Installer' from the Microsoft Store, then re-run."
}

function Install-Winget($id, $name) {
    Info "Installing $name ($id) ..."
    winget install --id $id --accept-source-agreements --accept-package-agreements --disable-interactivity --silent
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne -1978335189) {  # -1978335189 = already installed
        Warn "winget returned $LASTEXITCODE for $id (may already be installed)."
    }
}

# --- 1. Visual Studio 2022 with the C++ desktop workload ------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
function Get-VsInstall {
    if (-not (Test-Path $vswhere)) { return $null }
    # Pin to VS 2022 (v17) — the repo's CMake generator is "Visual Studio 17 2022",
    # so vcvars/cmake/generator must all come from the same 2022 instance even if a
    # newer VS is also installed.
    & $vswhere -version '[17.0,18.0)' -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null | Select-Object -First 1
}

$vs = Get-VsInstall
if (-not $vs) {
    Warn "Visual Studio 2022 with the C++ toolset was not found."
    Install-Winget 'Microsoft.VisualStudio.2022.Community' 'Visual Studio 2022 Community'
    Info "Ensuring the 'Desktop development with C++' workload is installed ..."
    winget install --id Microsoft.VisualStudio.2022.Community --accept-source-agreements --accept-package-agreements --disable-interactivity --silent `
        --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"
    $vs = Get-VsInstall
    if (-not $vs) { Die "Visual Studio C++ toolset still not detected. Install 'Desktop development with C++' via the VS Installer, then re-run." }
}
Ok "Visual Studio: $vs"

$vcvars   = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$cmakeBin = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
if (-not (Test-Path $vcvars))   { Die "vcvars64.bat not found under $vs" }
if (-not (Test-Path (Join-Path $cmakeBin 'cmake.exe'))) { Die "VS-bundled cmake not found; install the 'C++ CMake tools for Windows' component." }

# --- 2. Strawberry Perl (needed to build the OpenSSL dependency) ----------
$perlExe = 'C:\Strawberry\perl\bin\perl.exe'
if (-not (Test-Path $perlExe) -and -not (Get-Command perl -ErrorAction SilentlyContinue)) {
    Install-Winget 'StrawberryPerl.StrawberryPerl' 'Strawberry Perl'
}
$perlBin = if (Test-Path $perlExe) { 'C:\Strawberry\perl\bin' } else { Split-Path (Get-Command perl).Source }
Ok "Perl: $perlBin"

# --- 3. Git ---------------------------------------------------------------
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Install-Winget 'Git.Git' 'Git' }
Ok "Git: $((Get-Command git).Source)"

# --- 4. Pre-flight: known dependency-recipe fixes this repo needs ----------
$boostCmake = Get-Content (Join-Path $repo 'deps\Boost\Boost.cmake') -Raw
if ($boostCmake -match 'boost-1\.(8[5-9]|9\d|\d{3})') {
    Warn "deps/Boost/Boost.cmake pins Boost >= 1.85 but the source targets 1.84."
    Warn "Expect io_service / copy_option / Boost.Process v1 / MD5 errors. See research/build-environment.md."
}
$curlCmake = Get-Content (Join-Path $repo 'deps\CURL\CURL.cmake') -Raw
if ($curlCmake -notmatch 'CURL_USE_LIBPSL') {
    Warn "deps/CURL/CURL.cmake is missing -DCURL_USE_LIBPSL:BOOL=OFF; CURL 8.20 will fail to configure."
    Warn "Add it to the CURL flags. See research/build-environment.md."
}

# --- Helper: run a command inside the VC dev environment ------------------
function Invoke-VsEnv([string]$Command) {
    $prefix = "call `"$vcvars`" >nul 2>&1 && set `"PATH=$cmakeBin;$perlBin;%PATH%`" && set CMAKE_POLICY_VERSION_MINIMUM=3.5 && "
    & cmd /c ($prefix + $Command)
    if ($LASTEXITCODE -ne 0) { Die "Command failed (exit $LASTEXITCODE): $Command" }
}

$depsPrefix = (Join-Path $repo 'deps\build\OrcaSlicer_dep\usr\local') -replace '\\', '/'

# --- 5. Third-party dependencies (build from source, one-time) ------------
$destdir = Join-Path $repo 'deps\build\OrcaSlicer_dep\usr\local'
if ($SkipDeps) {
    Info "Skipping dependency build (-SkipDeps)."
} elseif (Test-Path (Join-Path $destdir 'lib')) {
    Ok "Dependencies already built at deps\build (use -SkipDeps to force skip, or delete deps\build to rebuild)."
} else {
    Info "Building third-party dependencies (this takes ~30-60 min the first time) ..."
    Invoke-VsEnv "cd /d `"$repo\deps`" && (if not exist build mkdir build) && cd build && cmake .. -G `"Visual Studio 17 2022`" -A x64 -DCMAKE_BUILD_TYPE=Release && cmake --build . --config Release --target deps -- -m"
    Ok "Dependencies built."
}

# --- 6. The app -----------------------------------------------------------
if ($SkipApp) { Info "Skipping app build (-SkipApp). Done."; exit 0 }

switch ($Config) {
    'Debug'          { $bdir = 'build-dbg' }
    'RelWithDebInfo' { $bdir = 'build-dbginfo' }
    default          { $bdir = 'build' }
}

if ($Jobs -le 0) {
    $ramGB = [int]((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB)
    $cores = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
    $Jobs  = [Math]::Max(2, [Math]::Min($cores, [int]($ramGB / 4)))
}
Info "Building OrcaSlicer ($Config) in $bdir with CL_MPCount=$Jobs ..."

Invoke-VsEnv "cd /d `"$repo`" && (if not exist $bdir mkdir $bdir) && cd $bdir && cmake .. -G `"Visual Studio 17 2022`" -A x64 -DORCA_TOOLS=ON -DCMAKE_PREFIX_PATH=`"$depsPrefix`" -DCMAKE_BUILD_TYPE=$Config && cmake --build . --config $Config --target ALL_BUILD -- -m -p:CL_MPCount=$Jobs"

# Localization + resource install (best-effort; needs gettext for .mo files)
Info "Running gettext + install step ..."
Invoke-VsEnv "cd /d `"$repo`" && call scripts\run_gettext.bat && cd $bdir && cmake --build . --target install --config $Config"

# The app is orca-slicer.exe (a thin launcher next to the ~145 MB OrcaSlicer.dll).
$exe = Join-Path $repo "$bdir\src\$Config\orca-slicer.exe"
if (Test-Path $exe) { Ok "Build complete: $exe" } else { Warn "Build finished but orca-slicer.exe not found at $exe (check the build output)." }

# --- 7. Optional run ------------------------------------------------------
if ($Run -and (Test-Path $exe)) {
    Info "Launching OrcaSlicer ..."
    & $exe
}
