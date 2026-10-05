**Task ID:** issue_10524

# Crash on startup when Pango/freetype2 finds no fonts

<!-- saved 2026-10-02 14:42:30 · slop filter: off · removed 0 chars -->

## 1. Problem Analysis & Constraints
In containerized, minimal Linux, or unusual Windows environments lacking standard desktop fonts, Pango/freetype2 font initialization returns a null font description, triggering an immediate crash during splash screen text rendering.

## 2. Surgical Implementation Plan
1. Audit GUI_App.cpp splash screen and 3D text font loader initialization
2. Add fallback to generic system monospace/sans font if fontconfig/freetype2 query returns null
3. Log warning instead of aborting when custom font family is missing

## 3. Target Files & Deliverables
- `src/slic3r/GUI/GUI_App.cpp`

## 4. Acceptance Criteria & Verification
- OrcaSlicer starts up successfully even when default UI font is unavailable
- Splash screen initializes without assertion failure

**Expected Evidence:** Successful startup log in headless/mocked font environment.
