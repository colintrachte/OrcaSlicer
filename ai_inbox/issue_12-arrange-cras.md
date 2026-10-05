**Task ID:** issue_12476

# Arrange crash on overlapping multi-plate layouts

<!-- saved 2026-10-02 14:42:30 · slop filter: off · removed 0 chars -->

## 1. Problem Analysis & Constraints
Auto-arranging objects across multi-plate layouts crashes when bounding boxes overlap plate boundaries and libnest2d solver encounters invalid polygon orientations.

## 2. Surgical Implementation Plan
1. Validate object bounding polygons before passing to arrange solver in ModelArrange.cpp
2. Clamp candidate layout coordinates to bed bounding polygon
3. Add safety timeout and fallback grid arrange if libnest2d raises divergence exception

## 3. Target Files & Deliverables
- `src/libslic3r/ModelArrange.cpp`

## 4. Acceptance Criteria & Verification
- Auto-arrange on multi-plate layouts arranges items cleanly without crashing
- Objects outside bed boundaries are flagged with warning

**Expected Evidence:** Passing arrange regression test.
