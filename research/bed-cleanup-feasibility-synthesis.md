# Architectural and material feasibility synthesis: PETG residue cleanup

**Status:** Canonical consolidated research brief; proposed decision, not implementation authorization

**Date:** 2026-07-15

**Decision question:** Is a virtual-plate PETG residue cleanup feature feasible for OrcaSlicer,
and, if physical testing supports it, what product and software boundary minimizes plate damage,
printer risk, and compatibility cost?

## Inputs and method

This document consolidates four independent research inputs into the single canonical brief for
the feature:

1. A repository-aware initial pass covering manual bed painting, adjacent OrcaSlicer interaction
   patterns, printable pickup techniques, and calibrated camera overlays.
2. `dcaf499b-030e-4601-a79c-0a0179b00f4d/pasted-text.txt`, a conservative safety pass.
3. `320f8355-d882-4cf5-a70f-f1e69be2ace4/pasted-text.txt`, a broad product and architecture pass.
4. `bb3d3c4d-5c69-4fd0-b7ef-3adea5611ca1/pasted-text.txt`, a pass emphasizing existing
   full-bed cleaner/test models and same-material pickup.

The partially pasted “Architectural & Material Feasibility Brief” was also considered where
present. New claims that could change the decision were checked against live web sources on
2026-07-15. The current OrcaSlicer source tree was inspected directly for painter-gizmo,
primitive-generation, excluded-region, plate-locking, preflight, and serialization boundaries.
Evidence is classified as primary guidance, community anecdote, engineering inference, or product
proposal where that distinction affects the decision.

## Executive decision

The feature is **materially plausible but not yet product-feasible without controlled physical
testing**. Multiple independent community reports describe printing fresh PETG over thin stuck
PETG and removing the combined piece after cooling [1][2][3]. The most precise precedent found
recommends a three- to four-layer PETG patch, while other reports use a 0.4 mm sheet or a much
thicker block. These are anecdotes, not a validated parameter window.

Existing “full-bed cleaner” models strengthen the prior-art case but not the safety case. At least
one current model uses a two-layer, full-surface print as a combined cleaning and first-layer test
[4]. That proves users already treat printable sheets as plate-maintenance artifacts. It does not
prove that every such model lifts strongly bonded PETG, that full-bed coverage is safe for PETG,
or that “same material” is a universal requirement.

If experiments establish a conservative operating window, implement a dedicated **Plate Cleanup
Wizard**. The wizard is a guarded utility modality; its footprint editor may start with rectangles
and polygons and later add bed-plane painting. It must generate an ordinary `ModelObject` mesh on
a dedicated locked plate and pass that object through normal slicing and Preview. It must not
emit special cleanup G-code, silently override temperature or Z offset, disable printer start
procedures, or claim it can identify the residue material.

The correct sequence is:

1. publish/document the existing primitive-based workaround only as experimental;
2. run a destructive physical test matrix on sacrificial plates;
3. if the predeclared safety gate passes, implement a minimal wizard that creates ordinary
   rectangular/polygonal patches;
4. add freehand bed painting only if irregular residue makes it materially more useful; and
5. defer photo overlays and automatic detection until the manual workflow is proven.

## Reconciliation of the independent passes

| Claim or choice | Combined assessment | Confidence |
| --- | --- | --- |
| No dedicated slicer cleanup-paint feature was found | Retain the qualified wording “not found in searched sources.” Full-bed cleaner/test models do exist, but no direct built-in slicer command matching the proposed localized workflow was found. | Moderate |
| Printing PETG over stuck PETG can work | Supported by several distinct community reports, including both success and failure. No manufacturer or controlled study validates success rate or coating safety. | Moderate plausibility; low safety certainty |
| Same material should be used | Same/similar PETG is the best initial hypothesis because the pickup mechanism depends on bonding to the residue. It can only be user-declared, not detected or truly enforced. PLA is a useful negative control, not a recommended default. | Moderate |
| A fixed two-, three-, or four-layer minimum is safe | The passes cite one layer, two layers, 0.4 mm, three to four layers, and 3 mm blocks. There is no defensible universal default yet. Total thickness and tear resistance should be measured. | Low |
| High nozzle/bed temperatures should be forced | Reject. The 270 C nozzle and 90-100 C bed suggestion is community advice [3], not manufacturer guidance, and it may increase PEI risk. The application must not override the active profile. | High |
| Freezing should be part of the workflow | Reject as a default. Community outcomes conflict, and Prusa explicitly says not to put its textured sheet in a freezer [5]. Start experiments with full ambient cooling and normal flexing. | High |
| The UI should be a wizard rather than a painter | False dichotomy. Use a wizard as the safety and task boundary; use shapes or painting inside it to define footprint. | High |
| Store editable cleanup semantics in 3MF | Not for the first version. Bake ordinary mesh geometry first. Optional editable metadata can be designed later if its value exceeds migration and interoperability cost. | High |
| Photo/lidar detection belongs in the MVP | Reject. Calibration and false-positive risk are independent hard problems and do not make the material process safer. | High |

### Corrections to individual passes

- OrcaSlicer uses wxWidgets, not Qt. The Qt claim in the conservative pass is incorrect.
- “Polymer chain entanglement” or “interdiffusion” is a plausible mechanism for fresh
  PETG-to-residue bonding, but none of the located evidence measures that interface on aged,
  ultrathin residue. Treat it as a hypothesis, not an established explanation.
- A slicer cannot verify that a mark is PETG rather than coating damage, grease, glue, or another
  polymer. A “same-material constraint” can only require user confirmation and filament choice.
- Existing `bed_exclude_area` support does not model all start-G-code purge, wipe, probing, or
  firmware macro motion. Clipping to exclusions is necessary but insufficient.
- `PrePrintChecker` exists, but its current role is primarily print-dialog/device readiness. Do
  not assume it is the natural owner of cleanup geometry validation without tracing its callers.
- Full-bed cleaning/test models are real adjacent prior art [4], but the available descriptions
  emphasize dirt, adhesion, and leveling. Do not automatically reinterpret them as validated
  bonded-PETG removal tools.
- The flexible-sheet patent is relevant to cool-and-flex removal, not to localized sacrificial
  pickup geometry. The cited US application is abandoned, and it is not a freedom-to-operate
  conclusion [6].

## Material feasibility

### What is supported

Textured powder-coated PEI is intended for PETG, and a removable spring-steel sheet is designed
to release prints after cooling and flexing [5][7]. Prusa also warns that the textured coating is
not scratch resistant and says not to use metal spatulas [5]. Bambu's current dual-texture plate
page lists PETG on the textured side without requiring glue, warns that acetone damages PEI, and
recommends cooling before removal [8].

Community evidence establishes a practiced recovery technique:

- print several PETG layers over the affected position and remove after full cooling [3];
- use a small rectangle or block of fresh PETG to capture old PETG [1]; and
- expect that an initial thin attempt may fail while a thicker connected attempt may remove most,
  but not necessarily all, residue [2]. Another walkthrough reports using an approximately
  0.4 mm localized sheet, but remains anecdotal [14].

This is enough to justify experiments. It is not enough to ship defaults.

### Competing interfaces and failure modes

The cleanup patch creates three relevant interfaces:

1. fresh patch to old PETG;
2. fresh patch to exposed PEI around the residue; and
3. old PETG to textured PEI.

Success requires interface 1 and patch cohesion to survive while interface 3 releases, without
interface 2 transferring excessive load into intact coating. The slicer cannot observe or
calculate these strengths. Surface contamination, PETG formulation, residue age, fillers,
texture depth, first-layer compression, temperature, and coating condition can reorder them.

Principal failure modes are:

- the new patch tears and leaves additional film;
- it bonds to clean PEI and enlarges the stuck region;
- old PETG remains while fresh PETG releases from it;
- PEI coating transfers to the patch or chips at its edge;
- a raised fragment catches the nozzle;
- a leader/tab printed on clean PEI becomes the hardest part to release;
- a user mistakes polished, gouged, or missing coating for residue; and
- custom machine start motion contaminates or collides with the marked region.

The tool must therefore classify itself as an experimental pickup aid, not a cleaner, repair
tool, or coating restoration feature.

## Product design

### Chosen product boundary: guarded wizard with ordinary output

Use a **Plate Cleanup Wizard** that creates a dedicated cleanup job. This name describes task
modality, not special slicing semantics.

Proposed flow:

1. **Classify the mark.** Show examples and ask the user to confirm it is thin bonded filament,
   not loose debris, oil/glue, or visibly damaged/lifted coating. Exit to manufacturer care
   guidance for other cases.
2. **Use a dedicated plate.** Create or select an empty plate, then lock it using the existing
   plate-lock mechanism so Arrange/Orient cannot move the registered cleanup objects
   (`PartPlate.hpp:426`, `ArrangeJob.cpp:156-174`). Do not mix cleanup patches with a normal job.
3. **Declare the material.** Require the user to identify the residue material and explicitly
   select the cleanup filament. Recommend the same PETG spool when available, while stating that
   the slicer cannot verify the declaration.
4. **Mark the footprint.** MVP tools: rectangle, polygon, move, resize, erase, and a visible XY
   coordinate readout. Add freehand circle-brush painting only after the simpler workflow is
   physically and ergonomically validated. If painting is added, follow OrcaSlicer's existing
   brush/fill conventions and painted-brim interaction vocabulary [10][11][15].
5. **Generate inspectable geometry.** Convert closed 2D regions into watertight ordinary meshes.
   Expose total thickness, layer-count preview, optional expansion, and optional connection/tab
   experiments. Do not preselect a “safe” thickness before physical results exist.
6. **Validate space.** Clip/reject outside printable geometry and `bed_exclude_area` using the
   existing helpers (`PrintConfig.cpp:11597-11618`) and documented printable-space semantics
   [12]. Display warnings for unmodeled start-G-code, purge, wipe, and probe motion.
7. **Slice normally.** Require normal Preview, focus the user on the first-layer paths, and show
   footprint, material, layer count, temperatures, and estimated time. Do not modify start G-code,
   Z offset, temperatures, flow, calibration, purge behavior, or leveling.
8. **Remove conservatively.** Instruct the user to wait for full cooling, remove the flexible
   sheet, and flex it normally. Do not recommend blades, hot scraping, freezing, or chemical
   solvents.

### Options considered

| Option | Complexity | Safety/compatibility | Decision |
| --- | ---: | --- | --- |
| Documentation plus ordinary cube | Low | Best pre-validation path; placement is manual | Do now |
| Wizard creating rectangle/polygon objects | Low-medium | Clear safety boundary; backward-compatible geometry | First implementation if tests pass |
| Wizard with bed-plane freehand painter | Medium | Best for irregular residue; more geometry/UI edge cases | Second implementation step |
| Photo import with four-point registration | High | Better targeting but adds calibration error and privacy/file-size issues | Defer |
| Automatic camera/lidar mask | Very high | False positives can worsen damage; vendor-specific | Reject for foreseeable MVP |
| Cleanup-specific slicer/G-code entity | High | Bypasses mature object slicing and creates format semantics | Reject |

### Peelability experiments

Treat all peelability features as experimental until the physical gate establishes that they do
not increase coating load:

- round sharp corners to reduce stress concentrations and tearing;
- optionally connect nearby islands with narrow bridges that remain inside printable space and
  never cross excluded regions;
- test a reinforced leader toward a user-selected clear area, while recognizing that the leader
  may adhere more strongly to clean PEI than the pickup region; and
- filter or widen features narrower than the active extrusion width, with every automatic change
  visible before generation.

A tall handle and a solid multi-millimeter slab are poor defaults: both add print time and can
increase removal force without evidence of better residue pickup.

## Architecture decision record

### ADR: Represent cleanup patches as ordinary objects generated by a dedicated wizard

**Status:** Proposed, contingent on physical feasibility gate

**Decision:** Add a GUI utility that captures bed-coordinate footprints and calls a deterministic
geometry generator. The generator returns normal `TriangleMesh` objects positioned on a dedicated
locked plate. The standard slicing pipeline remains unaware of cleanup semantics.

### GUI boundary

Add a `GLGizmoBedCleanup`-style tool derived from `GLGizmoBase`, or extract a small reusable
bed-plane interaction helper if another feature needs it. Reuse visual and input conventions from
the existing painter tools, but do not force the implementation through `GLGizmoPainterBase`:
that base projects strokes onto selected model triangles and assumes mesh-relative paint state
(`GLGizmoPainterBase.cpp:471-592`, `GLGizmoPainterBase.hpp:183-370`).

The closest reusable precedents are:

- `GLGizmosManager` for tool lifecycle;
- common ImGui gizmo layout and undo snapshots;
- `GLGizmoBrimEars` for interactive first-layer markers and snapshot behavior;
- bed raycasters already maintained by `GLCanvas3D` (`GLCanvas3D.cpp:3043`); and
- SVG/primitive paths for turning 2D intent into model geometry.

Orca's documented support- and color-painting tools establish the relevant brush, fill, and
gap-fill vocabulary [10][11]; they do not establish a bed-plane storage model.

### Geometry boundary

Place pure conversion logic in a small testable core unit, conceptually:

```text
BedCleanupPatchInput
  - ExPolygons footprint in bed coordinates
  - total height
  - optional connection/tab parameters

BedCleanupPatchGenerator
  - normalize/union polygons
  - reject or repair sub-extrusion-width features
  - clip against allowed bed polygons
  - triangulate top/bottom and construct side walls
  - return manifold TriangleMesh objects plus diagnostics
```

Keep material, temperature, Z offset, and printer policy out of this generator. Those remain
ordinary object/process selections and validation messages.

### Persistence boundary

For the first version, serialize only the resulting ordinary mesh and transformation. 3MF is
designed for full-fidelity mesh transfer and extensibility [9], but a new private semantic is not
needed to prove the workflow. Older OrcaSlicer/PrusaSlicer versions should see a normal printable
object.

Editable strokes, photo transforms, acknowledgement state, and cleanup-specific object metadata
are deferred. If later added, the baked mesh remains the compatibility fallback; private metadata
must be optional, versioned, and ignored without changing the printable result.

### Consequences

Benefits:

- no change when the feature is unused;
- no new slicing mode or G-code semantics;
- standard Preview and boundary checks remain active;
- saved projects remain useful to older readers; and
- geometry generation can be unit-tested without wxWidgets.

Costs:

- the first saved version will not retain editable brush strokes;
- normal objects can be edited into unsafe shapes after generation, so Preview remains mandatory;
- locking a whole cleanup plate is coarser than locking individual objects; and
- unmodeled printer start motion remains a warning, not a solved collision proof.

## Physical feasibility gate

Run experiments before choosing defaults or writing the painter. Use replaceable/sacrificial
textured PEI sheets and a preregistered protocol.

### Minimum matrix

- at least two textured-PEI plate products;
- at least two unfilled PETG formulations;
- standardized thin residue specimens plus normal support/skirt remnants;
- cleanup material: same spool, different PETG, and PLA negative control;
- total patch thickness measured in millimeters, with one-, two-, and four-layer equivalents;
- nominal and lower in-range nozzle/bed temperatures from the selected filament/plate guidance;
- nominal first-layer calibration only at first; do not intentionally over-squish;
- plain footprint versus rounded footprint; tabs/connections as a separate experiment; and
- full ambient cooling plus normal off-printer flexing.

Do not include freezer cycling, 270 C/100 C community settings, hot scraping, solvents, or an
hour-long IPA soak in the initial protocol. Those add uncontrolled plate and user-safety risks and
are not necessary to test the core hypothesis.

### Measurements

- residue area removed, estimated from registered before/after macro images;
- patch integrity and tear mode;
- visible coating transfer or color/texture change;
- peel force where a repeatable fixture is available;
- subsequent standardized PLA and PETG adhesion in the treated location; and
- repeat-cycle degradation over multiple cleanup attempts.

Predeclare pass/fail thresholds and sample counts before testing. A single successful plate is
not a shipping gate. Proceed only if a conservative window works across multiple plate and PETG
combinations without observable coating damage or degraded follow-up adhesion.

## Testing if implementation proceeds

### Geometry

- stroke/shape union, holes, erase, expansion, and connection;
- rectangular, circular, concave, custom-origin, and negative-coordinate beds;
- clipping against multiple excluded polygons;
- deterministic manifold extrusion;
- minimum feature handling relative to active extrusion width; and
- tabs that never silently cross exclusions.

### Project and behavior compatibility

- 3MF round trip as ordinary geometry;
- opening the baked mesh in an older build;
- undo/redo, duplicate, delete, plate lock/unlock, and multi-plate behavior;
- no config, 3MF, or G-code change when the wizard is never invoked; and
- Windows, macOS, and Linux UI coordinate consistency.

### Slicing and safety

- Preview reflects exact generated geometry and selected filament;
- no implicit brim, support, sequential mode, or multi-material assignment;
- boundary and exclusion failures are blocking;
- custom start-G-code uncertainty is visible but does not pretend to be statically solved; and
- parameter summaries show actual inherited settings without silently replacing them.

## Final recommendation

Accept the **architecture direction**, not yet the feature implementation:

- A guarded Plate Cleanup Wizard is the right product boundary.
- Shape tools and later painting belong inside that boundary.
- Ordinary generated objects are the right compatibility boundary.
- User-declared same-PETG cleanup is the leading experimental hypothesis.
- Temperature, Z offset, thickness, cooling, and tab defaults must come from controlled tests.
- Photo alignment and automatic detection are separate future projects.

If photo registration is revisited, use a calibrated workspace model rather than a raw image
overlay; LightBurn is a mature precedent for separating lens and workspace alignment, refreshed
captures, and overlay visibility controls [13][16]. A clean-reference comparison such as
OctoPrint-BedReady is adjacent prior art for proposing a future residue mask, but any mask must
remain user-confirmed because lighting, plate graphics, ghosting, and coating damage can resemble
residue [17].

The immediate deliverables should be a reproducible physical protocol and an example 3MF made
with existing primitives. Only after the protocol passes should OrcaSlicer gain new UI code.

## Sources

[1] Bambu Lab Community Forum, “Remove PETG from Textured PEI Plate” —
https://forum.bambulab.com/t/remove-petg-from-textured-pei-plate/80317 — anecdotal localized
fresh-PETG pickup reports.

[2] Reddit, “TIL you can’t just print over 0.06mm of PETG fused with textured plate…” —
https://www.reddit.com/r/3Dprinting/comments/1ucnqxm/til_you_cant_just_print_over_006mm_of_petg_fused/
— anecdotal failure followed by partial success with thicker connected layers.

[3] Prusa Forum, “Removing PETG from Double-sided Textured PEI Powder-coated Spring Steel Sheet” —
https://forum.prusa3d.com/forum/original-prusa-i3-mk3s-mk3-general-discussion-announcements-and-releases/removing-petg-from-double-sided-textured-pei-powder-coated-spring-steel-sheet/
— community three- to four-layer recommendation, high-temperature suggestion, failures, and
coating/freezer cautions on the same thread.

[4] “H2D First Layer Adhesion Test and Bed Cleaning” —
https://makerworld.com/en/models/1271342-bambu-lab-h2d-build-plate-cleaner-and-tester — community
full-bed two-layer cleaning/adhesion-test model; direct page may require MakerWorld access.

[5] Prusa Knowledge Base, “Textured steel sheet” —
https://help.prusa3d.com/article/textured-steel-sheet_196534 — primary PETG compatibility,
cooling, scratch, acetone, freezer, and metal-tool guidance.

[6] Google Patents, US20160332387A1, “A device and method for removing 3d print material from
build plates of 3d printers” — https://patents.google.com/patent/US20160332387A1/en — adjacent
flexible-sheet prior art; US application status shown as abandoned.

[7] Prusa Knowledge Base, “Flexible steel sheets” —
https://help.prusa3d.com/article/flexible-steel-sheets-guidepost_2195 — primary cool, flex, and
heat/cool-cycle removal guidance.

[8] Bambu Lab, “Dual-Texture PEI Plate with Glue” —
https://us.store.bambulab.com/products/bambu-dual-texture-pei-plate-with-glue — first-party
material/temperature table, cooling advice, and acetone warning.

[9] 3MF Consortium, “3MF Core Specification” —
https://3mf.io/spec/ — primary specification and extensibility reference.

[10] OrcaSlicer Wiki, “Support Painting” —
https://www.orcaslicer.com/wiki/print_prepare/prepare_support_painting — primary interaction
precedent.

[11] OrcaSlicer Wiki, “Color Painting” —
https://www.orcaslicer.com/wiki/print_prepare/prepare_color_painting — primary brush, fill, and
gap-fill interaction precedent.

[12] OrcaSlicer Wiki, “Printable Space” —
https://www.orcaslicer.com/wiki/printer_settings/basic%20information/printer_basic_information_printable_space
— primary printable-shape and excluded-region semantics.

[13] LightBurn Documentation, “Camera Alignment” —
https://docs.lightburnsoftware.com/latest/Reference/Cameras/Alignment/ — primary calibrated
workspace-overlay precedent for a deferred photo phase.

[14] Reddit, “How-to - removing PETG chunks embedded into textured PEI plate” —
https://www.reddit.com/r/BambuLab/comments/1maq2aw/howto_removing_petg_chunks_embedded_into_textured/
— anecdotal localized 0.4 mm pickup workflow.

[15] OrcaSlicer Wiki, “Brim” —
https://www.orcaslicer.com/wiki/print_settings/others/bed_adhesion_brim — primary painted-brim
interaction precedent.

[16] LightBurn Documentation, “Camera Control Window” —
https://docs.lightburnsoftware.com/latest/Reference/Cameras/CameraControlWindow/ — primary camera
capture, overlay refresh, and fade-control precedent.

[17] OctoPrint Plugin Repository, “OctoPrint-BedReady” —
https://plugins.octoprint.org/plugins/bedready/ — adjacent clean-reference image-comparison
precedent; not evidence that residue can be classified reliably.
