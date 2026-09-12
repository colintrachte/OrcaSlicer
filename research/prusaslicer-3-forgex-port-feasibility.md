# PrusaSlicer 3 + Forge-X feasibility correction

Date: 2026-09-01

## Question

Does supporting the user's Flashforge Adventurer 5M Pro running Forge-X require
porting OrcaSlicer's proprietary Flashforge discovery/upload implementation into
PrusaSlicer 3?

## Finding

No. Forge-X exposes standard Klipper through Moonraker. The user's local Forge-X
slicing reference explicitly configures slicers with:

- host type Moonraker / Klipper via Moonraker;
- URL `http://<printer_ip>:7125`;
- plain `.gcode` upload; and
- Orca's packaged `.gcode.3mf` output disabled.

PrusaSlicer 3 already implements that workflow. Its Moonraker print host:

- tests the connection with `GET /server/info`;
- uploads with `POST /server/files/upload`;
- sends the file under the `gcodes` root; and
- supplies `print=true` when the selected post-upload action is Start Print.

Therefore the core Forge-X send workflow should work through PrusaSlicer's
existing **Klipper (via Moonraker)** physical-printer type without new C++
transport code.

## What still needs migration

1. Create a PrusaSlicer printer preset for the exact AD5M Pro geometry,
   extruder, limits, Klipper flavor, and printable area.
2. Translate the Forge-X start/end G-code placeholders from Orca syntax to
   PrusaSlicer syntax. Forge-X expects `START_PRINT` with nozzle and bed
   temperatures and `END_PRINT`.
3. Keep the exported job as plain G-code. This matches both Forge-X's stated
   limitation and PrusaSlicer's existing Moonraker uploader.
4. Configure the Forge-X MD5 post-processing script if checksum validation is
   desired. Confirm PrusaSlicer invokes the script at the correct stage and that
   the uploaded file contains the appended checksum.
5. Validate generated G-code and first-print behavior on hardware under human
   supervision. A successful upload or build does not validate motion, heating,
   homing, or bed-mesh behavior.

## Discovery nuance

PrusaSlicer has a Bonjour/mDNS discovery dialog, currently oriented around the
`octoprint` service. Whether Forge-X advertises a discoverable Moonraker service
in a form that dialog recognizes is separate from upload compatibility.
Discovery is optional: a manual IP or hostname is sufficient for the existing
Moonraker backend. If automatic discovery is missing, that is a small convenience
gap rather than a printer-support blocker.

## Revised decision

For the Forge-X AD5M Pro, a PrusaSlicer 3 proof of concept is low effort compared
with a proprietary Flashforge port. The most likely path is profile translation,
macro-placeholder translation, and validation—not a new print-host backend.

This materially strengthens the case for PrusaSlicer as the long-term base if
its slicing behavior and new architecture are preferred. Orca remains valuable
as the source of tuned Flashforge presets and as a behavioral reference, but its
Flashforge-specific LAN/IFS code is not required for this Forge-X printer.

## Evidence

- Local Forge-X workflow: `D:\Git\ChimeraForge\reference\forgex\SLICING.md`
- PrusaSlicer Moonraker implementation:
  `D:\Git\PrusaSlicer-1\src\slic3r-shared\src\Slic3r\Biz\PrintHost\PrintHostMoonraker.cpp`
- PrusaSlicer print-host interface:
  `D:\Git\PrusaSlicer-1\src\slic3r-shared\include\Slic3r\Biz\PrintHost\IPrintHost.hpp`
- PrusaSlicer print-host factory:
  `D:\Git\PrusaSlicer-1\src\slic3r-shared\src\Slic3r\Biz\PrintHost\PrintHostFactory.cpp`
- [PrusaSlicer 3.0 preview](https://blog.prusa3d.com/prusaslicer-3-0-preview-built-for-the-future-of-3d-printing_137672/)

