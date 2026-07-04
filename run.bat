@REM OrcaSlicer launch script for Windows
@echo off

set BUILD_TYPE=Release
if "%1"=="debug"    set BUILD_TYPE=Debug
if "%1"=="dbg"      set BUILD_TYPE=Debug
if "%1"=="debuginfo" set BUILD_TYPE=RelWithDebInfo

if "%BUILD_TYPE%"=="Debug" (
    set BUILD_DIR=build-dbg
) else if "%BUILD_TYPE%"=="RelWithDebInfo" (
    set BUILD_DIR=build-dbginfo
) else (
    set BUILD_DIR=build
)

set EXE=%CD%\%BUILD_DIR%\OrcaSlicer\OrcaSlicer.exe

if not exist "%EXE%" (
    echo ERROR: OrcaSlicer binary not found at %EXE%
    echo Build the project first with: build_release_vs2022.bat
    exit /b 1
)

echo Launching %EXE%
start "" "%EXE%"
