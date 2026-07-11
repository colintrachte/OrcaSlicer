@echo off
REM Launch the locally-built OrcaSlicer (RelWithDebInfo) with the repo as CWD so
REM it finds resources/. To look for the resin printer: open the Printer dropdown
REM (top-left) and check for "- default SLA -". Selecting it should switch the app
REM into SLA mode (resin process/material tabs + the support-point and hollow gizmos).
cd /d "D:\Git\OrcaSlicer"
start "OrcaSlicer" "D:\Git\OrcaSlicer\build-dbginfo\src\RelWithDebInfo\orca-slicer.exe" %*
