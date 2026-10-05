**Task ID:** issue_10434

# Step import crash on degenerate face topology

<!-- saved 2026-10-02 14:42:30 · slop filter: off · removed 0 chars -->

## 1. Problem Analysis & Constraints
Importing complex CAD STEP files with self-intersecting or degenerate B-rep faces causes OpenCASCADE triangulator to throw an unhandled standard::NullObject exception, terminating the slicing process.

## 2. Surgical Implementation Plan
1. Wrap B-rep meshing routine in StepInput.cpp with try-catch for Standard_Failure
2. Skip degenerate face with warning log while triangulating remaining valid geometry
3. Add unit test with known malformed STEP file verifying non-fatal import

## 3. Target Files & Deliverables
- `src/libslic3r/Format/STEP.cpp`

## 4. Acceptance Criteria & Verification
- Malformed STEP file imports valid sub-bodies and displays repair notification
- Slicer does not crash

**Expected Evidence:** STEP import unit test passing with warning logged.
