@echo off
setlocal
if /I "%~1"=="debug" goto debug
if /I "%~1"=="dbg" goto debug
if /I "%~1"=="debuginfo" goto debuginfo
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" %*
exit /b %ERRORLEVEL%

:debug
shift
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" -Configuration Debug %*
exit /b %ERRORLEVEL%

:debuginfo
shift
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" -Configuration RelWithDebInfo %*
exit /b %ERRORLEVEL%
