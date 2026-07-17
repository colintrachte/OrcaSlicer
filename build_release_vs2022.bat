@echo off
REM Compatibility alias. The canonical driver detects the installed Visual Studio version.
call "%~dp0build_release_vs.bat" %*
exit /b %ERRORLEVEL%
