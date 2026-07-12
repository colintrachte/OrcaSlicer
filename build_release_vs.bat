@echo off
setlocal EnableExtensions
set "ORCA_BUILD_LAUNCHER=1"
set "PAUSE_ON_ERROR=1"
for %%A in (%*) do if /I "%%~A"=="-NoPause" set "PAUSE_ON_ERROR=0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" %* -NoPause
set "BUILD_EXIT_CODE=%ERRORLEVEL%"
if not "%BUILD_EXIT_CODE%"=="0" if "%PAUSE_ON_ERROR%"=="1" (
    echo.
    echo Build failed with exit code %BUILD_EXIT_CODE%. Press any key to close...
    pause >nul
)
endlocal & exit /b %BUILD_EXIT_CODE%
