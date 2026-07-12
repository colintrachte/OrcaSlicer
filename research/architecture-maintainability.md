# OrcaSlicer architecture and maintainability assessment

**Status:** Research baseline  
**Date:** 2026-07-11  
**Scope:** Repository structure, compile-time coupling, runtime resources, and an incremental
architecture-evolution path. This is not an authorization for a broad rewrite.

## Executive finding

OrcaSlicer does not literally compile most printer, filament, process, UI, shader, or web
content into the executable. The build installs `resources/` and the preset system loads JSON
at runtime. The maintainability problem is subtler: runtime values depend on a large,
hand-authored C++ schema, GUI interpretation is spread through large C++ units, product and
hardware decisions are often represented by compile-time conditionals or vendor checks, and
the build collapses most domain code into two very large static libraries.

The desired direction is therefore not "move everything to JSON" or "add arbitrary native
plugins." It is to establish a versioned boundary between:

- compiled algorithms and typed hot-path data;
- generated configuration bindings and migrations;
- runtime metadata, capabilities, profiles, and content; and
- application services and GUI presentation.

That boundary should be introduced incrementally, with behavior-preserving generated code and
compatibility fixtures before any setting semantics move.

## Evidence from the current tree

### Runtime resources already exist

The root build defines `SLIC3R_RESOURCES_DIR` and installs the resource tree rather than
embedding it (`CMakeLists.txt:81`, `CMakeLists.txt:911`, `CMakeLists.txt:931`). Runtime profile
loading, inheritance normalization, and compatibility evaluation live primarily in
`src/libslic3r/Preset.cpp` and `src/libslic3r/PresetBundle.cpp`.

The repository contains more than 15,000 files below `resources/`, including more than 70
vendor profile bundles. The unused `cmake/modules/bin2h.cmake` helper is not evidence that the
current resource tree is embedded; no active call site was found during this assessment.

### The configuration schema is centralized in compiled C++

`src/libslic3r/PrintConfig.cpp` is approximately 10,400 lines and contains roughly 877 calls
that register settings. Those registrations combine semantic type and defaults with ranges,
labels, units, GUI hints, CLI behavior, and serialization concerns. Static typed configuration
classes and legacy handlers are also declared in `src/libslic3r/PrintConfig.hpp`.

Consequently, profile values are runtime data, but a new key or a change to much of its
metadata still requires rebuilding the core. A setting may also be interpreted separately by
GUI tabs, preset code, 3MF code, CLI handling, and slicing code, making drift possible.

### Build boundaries are much coarser than domain boundaries

The core is principally built as `libslic3r` plus `libslic3r_cgal`
(`src/libslic3r/CMakeLists.txt:487`, `src/libslic3r/CMakeLists.txt:503`). Most GUI and application
services are accumulated into `libslic3r_gui` (`src/slic3r/CMakeLists.txt:739`) through a large,
manually maintained source list.

The directory layout already suggests narrower domains—configuration, presets, geometry,
formats, FFF/SLA pipelines, G-code, gizmos, and device support—but CMake targets do not enforce
most of those dependency directions. This increases rebuild scope, permits accidental coupling,
and makes isolated tests harder to link.

### Several files act as subsystem intersections

Approximate line counts observed during the assessment include:

| File | Lines | Coupled concerns |
| --- | ---: | --- |
| `src/slic3r/GUI/Plater.cpp` | 16,500 | project/plate state, commands, slicing, rendering, export |
| `src/libslic3r/PrintConfig.cpp` | 10,400 | schema, defaults, UI/CLI metadata, compatibility |
| `src/slic3r/GUI/GLCanvas3D.cpp` | 9,500 | input, scene state, rendering, tool interaction |
| `src/slic3r/GUI/GUI_App.cpp` | 8,400 | composition, startup, policy, services, UI lifecycle |
| `src/libslic3r/Format/bbs_3mf.cpp` | 8,300 | archive structure, serialization, compatibility |
| `src/slic3r/GUI/Tab.cpp` | 7,500 | settings UI, visibility, validation, product behavior |
| `src/libslic3r/GCode.cpp` | 7,500 | orchestration and emitted G-code semantics |
| `src/OrcaSlicer.cpp` | 6,800 | bootstrap, CLI, loading, resource and preset setup |

Size is only a warning signal, but these files also cross ownership boundaries. They should be
reduced by extracting cohesive state and services, not by mechanically splitting at arbitrary
line counts.

### Product policy and hardware support are not consistently capability-driven

The source tree contains many platform, debug, dependency, experiment, and product feature
macros. Platform and dependency switches are legitimate compile-time concerns. Product
visibility, release policy, and printer feature distinctions are better represented through a
typed runtime policy when the implementation is already present in the binary.

Preset logic also contains vendor-specific inference, for example deriving a capability from a
BBL vendor-name comparison in `src/libslic3r/Preset.cpp:971-980`. Explicit printer capabilities
would make hardware support additive and testable without scattering vendor-name knowledge.

## Architectural principles

1. **Compiled capability, runtime policy.** The build decides what implementations are
   available; runtime capability and feature policy decide what is applicable and visible.
2. **One versioned setting identity.** Type, serialization identity, default, migration history,
   and safe presentation metadata originate from one schema.
3. **Generate repetitive C++; do not interpret slicing semantics dynamically.** Generated typed
   bindings preserve performance and compiler checking.
4. **Profiles remain data.** Printer, filament, and process values and inheritance stay in
   validated runtime packages; profiles cannot inject executable code.
5. **Compatibility precedes cleanup.** Existing 3MF files, profiles, defaults, and feature-off
   behavior become fixtures before their implementation moves.
6. **Targets enforce architecture.** A directory name is documentation; a CMake target with a
   narrow public interface is an enforceable boundary.
7. **Extract by responsibility, not file size.** Existing coordinators should delegate to
   services with independently testable state transitions.

## Recommended target structure

```text
Application shell
|- workspace/project service
|- preset and profile service
|- device service
|- slicing orchestration
`- GUI presentation

Configuration platform
|- versioned declarative schema
|- generated typed C++ bindings
|- runtime presentation metadata
|- validation
`- migrations and compatibility fixtures

Slicing engine
|- model and geometry
|- FFF and SLA pipelines
|- toolpath generation
|- G-code generation
`- format adapters

Runtime content
|- signed/versioned package manifests
|- printer capabilities
|- vendor profiles and inheritance
|- assets and localization
`- optional content updates
```

Native plugin loading is deliberately not part of the initial target. A stable C++ ABI across
platforms and toolchains would add substantial security, compatibility, and support cost. A
validated data-package boundary provides most of the desired profile extensibility first.

## Recommended evolution sequence

### 1. Establish compatibility baselines

Inventory settings and every consumer of their metadata. Add fixtures for representative legacy
3MF projects, profile inheritance, defaults, enum serialization, and disabled-feature behavior.
Record the current public schema as a machine-readable snapshot. This turns later refactors from
"looks equivalent" into testable equivalence.

### 2. Create a declarative schema and generate the existing registry

Start with a small, representative setting family. Generate the same `ConfigOptionDef`
registrations and typed declarations currently written by hand. Initially keep the generated
output behaviorally identical and checked by snapshot/differential tests.

The schema should cover stable identity, type, default, bounds, units, scope, serialization
version, deprecation/replacement, and translatable label keys. Algorithmic validation and complex
cross-setting rules may remain named C++ hooks.

### 3. Separate safe runtime metadata

Once generated bindings are stable, allow presentation-only metadata—grouping, help links,
labels, tooltips, and safe visibility predicates—to load from a versioned runtime artifact.
Defaults and semantics affecting slicing reproducibility should remain versioned with the
application until compatibility guarantees are explicit.

### 4. Introduce printer capabilities

Define typed, versioned capabilities such as camera, lidar, multi-material system, remote-print
transport, bed leveling, tool count, and nozzle constraints. Load them from printer/vendor
packages, validate them, and replace vendor/model-name branches one family at a time.

Capabilities describe hardware; protocol implementations and security-sensitive network code
remain compiled.

### 5. Enforce module boundaries with CMake targets

Extract low-coupling targets first: configuration/schema, preset loading, and device core. Then
separate geometry/model, formats, G-code, GUI core, plater, gizmos, and device UI as dependency
direction becomes clear. Add CI checks for forbidden target dependencies rather than relying on
include-path convention.

### 6. Shrink coordinators through service extraction

Candidate `Plater` extractions include project session, plate repository, selection model,
arrangement service, slice-job controller, export controller, undo transactions, and printer
assignment. `GUI_App` should become composition/lifecycle; `GLCanvas3D` should focus on rendering
and input; `Tab` should render schema-backed settings rather than own their semantics.

Each extraction needs focused tests and must preserve command ordering, background-job lifetime,
undo behavior, and shared-state rules.

### 7. Formalize runtime content packages

Add a package manifest with package ID, version, schema compatibility range, content hashes,
dependencies, and optional signature. Extend the existing profile validator to reject unknown
keys, inheritance cycles, missing references, invalid capability declarations, and incompatible
schema versions.

This permits profile/content delivery independently of the application binary without granting
runtime code execution.

### 8. Consolidate runtime feature policy

Create a typed feature-policy service composed from build capabilities, platform capabilities,
release channel, signed runtime policy, user experimental preferences, and active printer
capabilities. Migrate product macros only when both branches can safely coexist in one binary.
Retain compile-time guards for unavailable APIs, optional dependencies, and debug instrumentation.

## Risks and guardrails

- Schema generation can accidentally change defaults or serialization. Require differential
  tests and legacy fixture loading before converting a setting family.
- Runtime metadata can make identical projects look or behave differently across installations.
  Version artifacts and keep slicing-affecting semantics pinned.
- Capability migration can alter feature visibility. For every replaced vendor check, snapshot
  old and new results across representative printer profiles.
- Target splitting can expose hidden initialization-order dependencies. Land one boundary at a
  time and keep link/build verification cross-platform.
- Service extraction around background slicing can introduce races. Document ownership and TBB/
  worker-thread access before moving state.
- Package updates need atomic installation, rollback, validation, and a last-known-good copy.

## Success measures

- Adding a printer that uses existing protocols and algorithms requires only validated runtime
  package changes.
- Adding a setting requires one schema entry plus explicitly named algorithm/UI hooks, not
  synchronized edits across registration tables.
- Profile and schema validation runs without launching the GUI.
- Core configuration/preset tests link without the full GUI or slicing engine.
- Editing one GUI feature does not rebuild all GUI sources.
- Vendor-name checks decline as explicit capability queries increase.
- No regression in legacy 3MF/profile fixtures or default generated G-code.

## Roadmap relationship

The tasks under **Infrastructure / CI → Architecture evolution** in `docs/roadmap.md` implement
this sequence. They are intentionally staged: compatibility baselines and a design record come
before schema generation; capability infrastructure precedes vendor-check migration; target
boundaries precede large coordinator extractions; package validation precedes independent content
updates.
