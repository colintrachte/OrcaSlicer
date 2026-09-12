# OrcaSlicer launcher backend

Ordinary Windows users should double-click `START ORCASLICER.vbs` in the repository root.
These files are the backend and legacy developer helpers.

The graphical list groups each local build folder into one parent entry, with
Release, Debug, RelWithDebInfo, and staged outputs listed beneath it. Select an
output to launch or pin it. Build folders without runnable outputs remain visible
for cleanup.

The parent size includes all outputs plus shared/other build files. Child sizes
are already included in that total. Sizes sum file lengths, excluding junctions
and symbolic-link destinations; they estimate storage rather than exact space
reclaimed (filesystem allocation, compression, and hard links can differ).
Refresh recalculates sizes; unreadable folders show `Unavailable`.

**Delete output** removes only the selected output directory. **Delete build**
on a parent removes its entire build folder, including shared files and every
output listed beneath it. Both require confirmation. Installed releases cannot
be deleted here.

Run the isolated discovery, grouping, size, and deletion regression checks with:
`powershell.exe -NoProfile -File helper_scripts/orcaslicer_launcher/test-delete-build.ps1`.

The PowerShell backend is also a deterministic console interface:

```powershell
# Machine-readable discovery and preflight
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command List -Json
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Recommend -Json
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Prerequisites -Json

# Build and launch
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Build `
    -Configuration Release -BuildScope Complete -DryRun -Json
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Build `
    -Configuration Release -BuildScope Complete
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Launch

# Pin an exact Path returned by List
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Pin -TargetPath '<orca-slicer.exe path>'
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Unpin

# Recreate the desktop shortcut for this clone
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command CreateShortcut
```

`launcher-console.bat` exposes the same PowerShell parameters for `cmd.exe`. `run.ps1` and
`run.bat` are compatibility helpers for launching a specific build configuration.
