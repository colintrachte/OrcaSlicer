@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "WP=%CD%"

@REM epoch for elapsed time, locale independent
for /f %%i in ('powershell -NoProfile -Command "[int64](Get-Date -UFormat %%s)" 2^>nul') do set "_START_EPOCH=%%i"
if not defined _START_EPOCH set "_START_EPOCH=0"

@REM ---------- args ----------
set "arch=x64"
set "debug=OFF"
set "debuginfo=OFF"
set "USE_NINJA=0"
@REM MSVC /MP is uncapped by default (one cl.exe per core). On boxes with a high
@REM core-to-RAM ratio (e.g. 32 cores / 32GB here), a RelWithDebInfo PCH compile
@REM needs ~2GB per cl.exe, so unbounded /MP OOMs (C3859/C1076). Cap it; override
@REM with `set MP_CAP=N` before calling this script if your box has more headroom.
if not defined MP_CAP set "MP_CAP=8"
set "DO_PACK=0"
set "DO_DEPS_ONLY=0"
set "DO_SLICER_ONLY=0"

if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "arch=ARM64"
if /I "%PROCESSOR_ARCHITEW6432%"=="ARM64" set "arch=ARM64"

for %%a in (%*) do (
    if /I "%%a"=="x64" set "arch=x64"
    if /I "%%a"=="arm64" set "arch=ARM64"
    if /I "%%a"=="debug" set "debug=ON"
    if /I "%%a"=="debuginfo" set "debuginfo=ON"
    if "%%a"=="-x" set "USE_NINJA=1"
    if /I "%%a"=="pack" set "DO_PACK=1"
    if /I "%%a"=="deps" set "DO_DEPS_ONLY=1"
    if /I "%%a"=="slicer" set "DO_SLICER_ONLY=1"
)

if "%debug%"=="ON" (
    set "build_type=Debug"
    set "build_dir=build-dbg"
) else if "%debuginfo%"=="ON" (
    set "build_type=RelWithDebInfo"
    set "build_dir=build-dbginfo"
) else (
    set "build_type=Release"
    set "build_dir=build"
)
if /I "%arch%"=="ARM64" set "build_dir=%build_dir%-arm64"

@REM normalize arch for CMake -A
set "CMAKE_ARCH=x64"
if /I "%arch%"=="ARM64" set "CMAKE_ARCH=ARM64"

@REM ---------- logging ----------
echo [DEBUG] Setting up log directories...
for /f "usebackq delims=" %%t in (`powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"`) do set "LOG_STAMP=%%t"
set "LOG_DIR=%WP%\logs"
set "LOG_DEPS=%LOG_DIR%\deps_%arch%_%build_type%_%LOG_STAMP%.log"
set "LOG_SLICER=%LOG_DIR%\slicer_%arch%_%build_type%_%LOG_STAMP%.log"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo build type %build_type%, arch %arch%, cmake arch %CMAKE_ARCH%

@REM ---------- generator detection ----------
if "%USE_NINJA%"=="1" (
    set "VS_VERSION=Ninja"
    set "CMAKE_GENERATOR=Ninja Multi-Config"
    echo Using %CMAKE_GENERATOR%
    goto :generator_ready
)

echo [DEBUG] Detecting Visual Studio version...
call :detect_vs
if not defined CMAKE_GENERATOR (
    echo Error: Could not detect Visual Studio installation
    exit /b 1
)
echo Detected Visual Studio %VS_VERSION% -^> "%CMAKE_GENERATOR%"

:generator_ready

call :resolve_cmake
echo Using CMake: %CMAKE_EXE%

if "%DO_PACK%"=="1" goto :do_pack

set "SIG_FLAG="
if defined ORCA_UPDATER_SIG_KEY set "SIG_FLAG=-DORCA_UPDATER_SIG_KEY=%ORCA_UPDATER_SIG_KEY%"
set "DEPS_SRC=%WP%\deps"
set "DEPS_BIN=%WP%\deps\%build_dir%"
set "SLICER_SRC=%WP%"
set "SLICER_BIN=%WP%\%build_dir%"

if "%DO_SLICER_ONLY%"=="1" goto :slicer

@REM ---------- deps ----------
echo.
echo [1/2] Configuring deps -^> %LOG_DEPS%
> "%LOG_DEPS%" echo === deps %DATE% %TIME% arch=%arch% type=%build_type% gen=%CMAKE_GENERATOR% ===
if "%USE_NINJA%"=="1" (
    "%CMAKE_EXE%" -S "%DEPS_SRC%" -B "%DEPS_BIN%" -G "%CMAKE_GENERATOR%" %SIG_FLAG% >>"%LOG_DEPS%" 2>&1
) else (
    "%CMAKE_EXE%" -S "%DEPS_SRC%" -B "%DEPS_BIN%" -G "%CMAKE_GENERATOR%" -A %CMAKE_ARCH% -DCMAKE_BUILD_TYPE=%build_type% %SIG_FLAG% >>"%LOG_DEPS%" 2>&1
)
if errorlevel 1 goto :log_error

echo [2/2] Building deps - log: %LOG_DEPS%
if "%USE_NINJA%"=="1" (
    "%CMAKE_EXE%" --build "%DEPS_BIN%" --config %build_type% --target deps >>"%LOG_DEPS%" 2>&1
) else (
    "%CMAKE_EXE%" --build "%DEPS_BIN%" --config %build_type% --target deps -- -m -p:CL_MPCount=%MP_CAP% >>"%LOG_DEPS%" 2>&1
)
if errorlevel 1 goto :log_error

if "%DO_DEPS_ONLY%"=="1" goto :done

:slicer
echo.
echo [1/3] Configuring slicer -^> %LOG_SLICER%
> "%LOG_SLICER%" echo === slicer %DATE% %TIME% arch=%arch% type=%build_type% gen=%CMAKE_GENERATOR% ===
if "%USE_NINJA%"=="1" (
    "%CMAKE_EXE%" -S "%SLICER_SRC%" -B "%SLICER_BIN%" -G "%CMAKE_GENERATOR%" -DORCA_TOOLS=ON %SIG_FLAG% >>"%LOG_SLICER%" 2>&1
) else (
    "%CMAKE_EXE%" -S "%SLICER_SRC%" -B "%SLICER_BIN%" -G "%CMAKE_GENERATOR%" -A %CMAKE_ARCH% -DORCA_TOOLS=ON %SIG_FLAG% -DCMAKE_BUILD_TYPE=%build_type% >>"%LOG_SLICER%" 2>&1
)
if errorlevel 1 goto :log_error

echo [2/3] Building slicer
if "%USE_NINJA%"=="1" (
    "%CMAKE_EXE%" --build "%SLICER_BIN%" --config %build_type% >>"%LOG_SLICER%" 2>&1
) else (
    "%CMAKE_EXE%" --build "%SLICER_BIN%" --config %build_type% --target ALL_BUILD -- -m -p:CL_MPCount=%MP_CAP% >>"%LOG_SLICER%" 2>&1
)
if errorlevel 1 goto :log_error

echo [3/3] gettext and install
if not exist "%WP%\scripts\run_gettext.bat" (
    echo Error: %WP%\scripts\run_gettext.bat not found!
    goto :log_error
)
call "%WP%\scripts\run_gettext.bat" >>"%LOG_SLICER%" 2>&1
if errorlevel 1 (
    echo run_gettext failed, see %LOG_SLICER%
    goto :log_error
)
"%CMAKE_EXE%" --build "%SLICER_BIN%" --config %build_type% --target install >>"%LOG_SLICER%" 2>&1
if errorlevel 1 goto :log_error

goto :done

:do_pack
echo [DEBUG] Starting pack routine...
set "PACK_BUILD_DIR=build"
if /I "%arch%"=="ARM64" set "PACK_BUILD_DIR=build-arm64"
set "PACK_BIN=%WP%\deps\%PACK_BUILD_DIR%"
set "LOG_PACK=%LOG_DIR%\pack_%arch%_%LOG_STAMP%.log"
> "%LOG_PACK%" echo Pack %DATE% %TIME% arch=%arch% vs=%VS_VERSION%
if not exist "%PACK_BIN%\OrcaSlicer_dep" (
    echo Error: %PACK_BIN%\OrcaSlicer_dep not found
    echo See %LOG_PACK%
    exit /b 1
)
for /f "usebackq delims=" %%d in (`powershell -NoProfile -Command "Get-Date -Format yyyyMMdd"`) do set "build_date=%%d"
echo Packing OrcaSlicer_dep_win-%arch%_%build_date%_vs%VS_VERSION%.zip
pushd "%PACK_BIN%"
if not exist "%WP%\tools\7z.exe" (
    echo Error: 7z.exe tool missing from %WP%\tools\
    popd
    exit /b 1
)
"%WP%\tools\7z.exe" a "OrcaSlicer_dep_win-%arch%_%build_date%_vs%VS_VERSION%.zip" OrcaSlicer_dep >>"%LOG_PACK%" 2>&1
popd
goto :done

:detect_vs
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_YEAR="
set "VS_INSTALL_VERSION="
set "VS_INSTALL_PATH="
set "VS_MAJOR="
set "CMAKE_GENERATOR="
set "VS_VERSION="

if not exist "%VSWHERE%" goto :skip_vswhere
for /f "usebackq delims=" %%y in (`"%VSWHERE%" -latest -property catalog_productLineVersion 2^>nul`) do set "VS_YEAR=%%y"
for /f "usebackq delims=" %%v in (`"%VSWHERE%" -latest -property installationVersion 2^>nul`) do set "VS_INSTALL_VERSION=%%v"
for /f "usebackq delims=" %%p in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VS_INSTALL_PATH=%%p"
if defined VS_INSTALL_VERSION (
    for /f "tokens=1 delims=." %%m in ("%VS_INSTALL_VERSION%") do set "VS_MAJOR=%%m"
)
@REM catalog_productLineVersion is a real year (e.g. "2019"/"2022") on older
@REM VS releases, but newer installers report the raw product-line number
@REM (e.g. "18") instead - discard it there so the VS_MAJOR fallback below
@REM can map it to the correct year instead of producing "Visual Studio 18 18".
if defined VS_YEAR (
    if not "%VS_YEAR:~0,2%"=="20" set "VS_YEAR="
)
:skip_vswhere

@REM fallback to msbuild if vswhere gave nothing
if defined VS_MAJOR goto :skip_msbuild
@REM Safe execution of piped commands without parenthetical blocks
where msbuild >nul 2>&1
if errorlevel 1 goto :skip_msbuild
for /f "tokens=1 delims=." %%a in ('msbuild -version 2^>^&1 ^| findstr /R "[0-9][0-9]*\.[0-9][0-9]*\.[0-9]"') do (
    set "VS_MAJOR=%%a"
)
:skip_msbuild

@REM if we have year from vswhere, use it, otherwise map major
if not defined VS_YEAR (
    if "%VS_MAJOR%"=="16" set "VS_YEAR=2019"
    if "%VS_MAJOR%"=="17" set "VS_YEAR=2022"
    if "%VS_MAJOR%"=="18" set "VS_YEAR=2026"
)

if defined VS_MAJOR if defined VS_YEAR (
    set "VS_VERSION=%VS_YEAR%"
    set "CMAKE_GENERATOR=Visual Studio %VS_MAJOR% %VS_YEAR%"
    goto :eof
)
if defined VS_MAJOR (
    set "VS_VERSION=%VS_MAJOR%"
    set "CMAKE_GENERATOR=Visual Studio %VS_MAJOR%"
    goto :eof
)
goto :eof

@REM Visual Studio ships its own CMake under the IDE install (kept current
@REM with each VS release), which is often newer than whatever "cmake" a
@REM user has on PATH. Prefer it when present so new VS releases (whose
@REM generator name isn't recognized by older standalone CMake) still work.
:resolve_cmake
set "CMAKE_EXE=cmake"
if not defined VS_INSTALL_PATH (
    if not defined VSWHERE set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "%VSWHERE%" (
        for /f "usebackq delims=" %%p in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VS_INSTALL_PATH=%%p"
    )
)
if defined VS_INSTALL_PATH if exist "%VS_INSTALL_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "CMAKE_EXE=%VS_INSTALL_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
goto :eof

:log_error
echo.
echo Build FAILED
echo   deps log   : %LOG_DEPS%
echo   slicer log : %LOG_SLICER%
exit /b 1

:done
echo [DEBUG] Calculating total build time...
for /f "delims=" %%e in ('powershell -NoProfile -Command "$s=%_START_EPOCH%; $e=[int64](Get-Date -UFormat %%s); $d=$e-$s; if($d -lt 0){$d=0}; $h=[int]($d/3600); $m=[int](($d -mod 3600)/60); $s=$d -mod 60; \"{0}h {1}m {2}s\" -f $h,$m,$s"') do set "ELAPSED_STR=%%e"
echo.
echo Build completed in %ELAPSED_STR%
echo Logs:
echo   %LOG_DEPS%
echo   %LOG_SLICER%
endlocal
exit /b 0
pause