**Task ID:** issue_10756

# Crash in wxWebView when switching to Klipper Device Tab

<!-- saved 2026-10-02 14:42:30 · slop filter: off · removed 0 chars -->

## 1. Problem Analysis & Constraints
On Windows/Linux, navigating to the Device tab for Klipper/Mainsail printers causes an immediate application crash if wxWebView fails initialization or accesses a null parent frame during rapid tab switching.

## 2. Surgical Implementation Plan
1. Inspect src/slic3r/GUI/DeviceCore/DevManager.cpp and DevicePanel.cpp
2. Add null-check guards on wxWebView backend handle before dispatching navigation calls
3. Implement graceful fallback error banner if embedded browser engine fails to initialize
4. Verify fix by tab switching between Prepare, Preview, and Device repeatedly

## 3. Target Files & Deliverables
- `src/slic3r/GUI/DeviceCore/DevManager.cpp`
- `src/slic3r/GUI/DeviceCore/DevManager.h`

## 4. Acceptance Criteria & Verification
- Switching to Device tab with invalid/unreachable Klipper IP displays connection error instead of crashing
- Rapid tab switching does not trigger segfault

**Expected Evidence:** Runtime execution log showing graceful connection error handling.
