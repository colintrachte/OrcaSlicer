**Task ID:** issue_11641

# Crash when closing Preferences dialog without parent window

<!-- saved 2026-10-02 14:42:30 · slop filter: off · removed 0 chars -->

## 1. Problem Analysis & Constraints
When opening the Preferences dialog from specific modal contexts or tray icons, closing or pressing ESC attempts to call EndModal on a parent window that has already been destroyed or is null, causing an unhandled access violation.

## 2. Surgical Implementation Plan
1. Inspect PreferencesDialog.cpp close handler
2. Check wxGetApp().GetTopWindow() and ensure parent pointer validity before calling window focus restores
3. Ensure dialog destroys itself cleanly using Destroy() instead of Close() on orphaned parents

## 3. Target Files & Deliverables
- `src/slic3r/GUI/PreferencesDialog.cpp`

## 4. Acceptance Criteria & Verification
- Opening and closing Preferences from main menu, hotkey, and modal context closes cleanly
- Zero crashes on ESC key press

**Expected Evidence:** Clean exit log and verification in debug build.
