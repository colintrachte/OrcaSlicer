# OrcaSlicer architecture and maintainability assessment

**Status:** Revised research baseline

**Date:** 2026-07-12

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

## Operating assumptions and non-goals

This is a personal fork, but it is not a hard fork. `TRIAGE_POLICY.md` explicitly treats
conflict-free upstream improvements as desirable and records an `upstream` remote and periodic
sync workflow. Continued upstream intake is therefore an architectural constraint even without
external-maintainer governance.

The work should optimize for the owner's custom FDM, Flashforge, and future toolchanger needs.
It should not build organization-scale infrastructure merely because that would be conventional
in a large team. In particular, this effort does **not** aim to:

- eliminate legitimate platform, dependency, or debug compile-time guards;
- replace wxWidgets or make the GUI framework swappable;
- redesign undo/redo into event sourcing or introduce an actor framework;
- introduce arbitrary executable plugins or a stable third-party C++ ABI;
- migrate all 877 settings before a real feature needs them;
- make every runtime artifact hot-reloadable; or
- split the code into the maximum possible number of libraries.

Each milestone needs an independent payoff and an exit ramp. If a pilot does not reduce the
number of synchronized edits, improve validation, or unblock in-scope hardware, stop rather than
scale it up.

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
8. **Remain bisectable.** Every commit builds and passes the relevant baseline; a step requiring
   a flag day is too large.
9. **Separate facts, policy, and packaging.** Printer capabilities describe hardware facts;
   feature policy decides behavior; vendor packages only deliver versioned data.
10. **Preserve the upstream seam.** Prefer additive sidecar files, adapters, and generated
    comparisons over rewriting high-churn upstream files until an atomic cutover is justified.

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
|- versioned package manifests and integrity hashes
|- printer capabilities
|- vendor profiles and inheritance
|- assets and localization
`- optional content updates
```

Native plugin loading is deliberately not part of the initial target. A stable C++ ABI across
platforms and toolchains would add substantial security, compatibility, and support cost. A
validated data-package boundary provides most of the desired profile extensibility first.

## Upstream merge strategy

Replacing upstream's hand-written `PrintConfig.cpp` with a locally generated file too early
would turn routine upstream merges into schema archaeology. The migration must therefore use a
shadow path:

1. Keep upstream-owned registration code active.
2. Add the schema, generator, and generated representation in new, clearly local files.
3. Compare generated and active registries in tests or a validator command.
4. Convert one low-risk setting family only after equivalence is demonstrated.
5. Keep a small reconciliation script/report that identifies upstream settings absent from the
   schema and schema entries whose upstream registration changed.
6. Cut over atomically only if the reduced local maintenance exceeds the recurring merge cost.

Upstream merges remain normal throughout the shadow period. Generated output is reproducible and
never hand-edited. Schema commits should be separate from semantic changes so conflicts can be
resolved by re-importing the upstream definition rather than interpreting mixed refactors. If
upstream churn makes the pilot persistently expensive, retain the schema as validation and
documentation instead of making it authoritative.

## Validation strategy

The current tree already has Catch2-based `libslic3r` tests and 3MF read/round-trip coverage in
`tests/libslic3r/test_3mf.cpp`; Catch2 benchmarking support is present, but a project-specific
architecture baseline has not yet been established. Capturing that baseline is real work and
must be incremental—not a promise to hand-author exhaustive fixtures for 877 settings.

Use a boundary-oriented test pyramid:

| Layer | Purpose | Initial smallest slice |
| --- | --- | --- |
| Schema unit tests | Types, defaults, bounds, IDs, deprecation, hook lookup | One generated setting family |
| Differential tests | Compare active hand-written and shadow-generated registries | Exact normalized metadata dump |
| Preset integration tests | Import, inheritance, unknown keys, compatibility reasons | Representative system/user chain |
| Format fixtures | Old 3MF open, in-memory migration, round trip, approved deltas | A small versioned corpus, not every project |
| G-code characterization | Detect slicing behavior drift | Canonical models/profiles for affected settings only |
| Headless service tests | State transitions without wxWidgets widgets | First extracted Plater responsibility |
| GUI smoke tests | Event wiring and lifecycle | Targeted manual/scripted checks; screenshots only for rendering work |
| Fuzz/property tests | Malformed schema/config and migration invariants | Validator and deserializer after the schema pilot |

Every phase has a hard no-unapproved-drift gate: normalized defaults, enum encodings, relevant
profile/3MF round trips, and affected canonical G-code remain unchanged unless the commit records
and tests an intentional delta. Old data is detected on load, migrated in memory, and written in
the current format only on an explicit save. Destructive migrations require a recoverable backup
or atomic replacement strategy; reading old formats must not mutate the original file.

Performance, build time, and binary size should first be recorded as trend metrics on a stable
machine/configuration rather than brittle universal CI thresholds. A material regression requires
explanation before landing. Schema bindings should be compiled into a library or implementation
unit by default; exposing an 877-setting template-heavy header to many translation units is not
acceptable without measurements. Target splitting must measure clean and representative
incremental builds because finer libraries can worsen compilation through duplicated headers or
PCH loss.

## Recommended evolution sequence

### 1. Establish compatibility baselines

Inventory settings and every consumer of their metadata as its own deliverable: registration,
typed accessor, GUI/editor, CLI, preset/inheritance, import/export/3MF, validation, and slicing
use. Add fixtures for representative legacy
3MF projects, profile inheritance, defaults, enum serialization, and disabled-feature behavior.
Record the current public schema as a machine-readable snapshot. This turns later refactors from
"looks equivalent" into testable equivalence.

The first user/developer-visible payoff is a validator/debug command that exports the normalized
registry and explains invalid or unknown profile keys. Do not block this milestone on exhaustive
coverage; prioritize settings used by owned hardware and representative types.

### 2. Create a declarative schema and generate the existing registry

Start with a small, representative setting family. Generate the same `ConfigOptionDef`
registrations and typed declarations currently written by hand. Initially keep the generated
output behaviorally identical and checked by snapshot/differential tests.

The schema should cover stable identity, type, default, bounds, units, scope, serialization
version, deprecation/replacement, and translatable label keys. Algorithmic validation and complex
cross-setting rules remain named C++ hooks behind one narrow interface that receives a typed
configuration view and structured diagnostics.

The schema format and generator are an ADR decision, not an implicit detail. Prefer a boring,
widely supported data format over a custom DSL; the ADR must compare at least JSON, YAML, and
TOML against deterministic parsing, comments, source locations, dependency cost, gettext
extraction, and CMake/Python availability. The generator needs golden-output tests, deterministic
ordering, actionable source-location errors, and a `--check` mode suitable for CI. Generation and
schema redesign are separate changes: first reproduce the existing registry, then propose cleanup.

The schema does not automatically solve the multiple-consumer problem. For each migrated field,
the consumer inventory must record whether GUI, CLI, preset/3MF serialization, validation, and
slicing obtain data from the generated descriptor, a generated typed accessor, or an explicit C++
hook. Any remaining duplicate table is tracked rather than assumed away.

### 3. Separate safe runtime metadata

Once generated bindings are stable, allow presentation-only metadata—grouping, help links,
labels, tooltips, and safe visibility predicates—to load from a versioned runtime artifact.
Defaults and semantics affecting slicing reproducibility should remain versioned with the
application until compatibility guarantees are explicit.

Integrate with the existing gettext `.po`/`.mo` workflow: schema/runtime strings use stable
message IDs, are included in extraction, preserve translator context, and are checked for missing
translations. Do not invent a parallel localization system. Initially load metadata at startup;
hot reload is deferred until atomic reload, validation failure, and UI-state behavior are designed.

### 4. Introduce printer capabilities

Define typed, versioned capabilities such as camera, lidar, multi-material system, remote-print
transport, bed leveling, tool count, and nozzle constraints. Load them from printer/vendor
packages, validate them, and replace vendor/model-name branches one family at a time.

Capabilities describe hardware; protocol implementations and security-sensitive network code
remain compiled.

Because this directly supports custom and future toolchanging hardware, a capability pilot may
start after the compatibility baseline in parallel with—not depend on—the broad schema rollout.
The pilot must keep hardware facts free of UI policy expressions. Device protocols expose a
transport-agnostic print-job interface so plater/slicing code does not depend on MQTT, OctoPrint,
Moonraker, or Flashforge details; redesigning network sandboxing or certificate policy is outside
this maintainability effort unless a concrete security requirement demands it.

### 5. Enforce module boundaries with CMake targets

Extract low-coupling targets first: configuration/schema, preset loading, and device core. Then
separate geometry/model, formats, G-code, GUI core, plater, gizmos, and device UI as dependency
direction becomes clear. Add CI checks for forbidden target dependencies rather than relying on
include-path convention.

Begin with a pilot/read-only boundary and declare its allowed incoming/outgoing dependencies.
Move write paths only after hidden initialization and mutation flows are understood. A target
split is successful only if focused core tests link without wxWidgets, forbidden edges decline,
and clean/incremental build trends and binary size remain acceptable; C++20 modules are not a
prerequisite and should not be bundled into this work.

### 6. Shrink coordinators through service extraction

Candidate `Plater` extractions include project session, plate repository, selection model,
arrangement service, slice-job controller, export controller, undo transactions, and printer
assignment. `GUI_App` should become composition/lifecycle; `GLCanvas3D` should focus on rendering
and input; `Tab` should render schema-backed settings rather than own their semantics.

Each extraction needs focused tests and must preserve command ordering, background-job lifetime,
undo behavior, and shared-state rules.

Extracted services keep domain state and commands free of wxWidgets types where practical, but
framework replacement is not a goal. Every service declares one of three thread-affinity models:
main-thread-only, immutable/thread-safe, or serialized through the existing job/event queue.
Cross-thread mutation without an explicit handoff is forbidden. The first extraction must use the
existing undo transaction boundary; choosing a new command/event-sourced undo architecture is a
separate decision, not incidental refactoring.

### 7. Formalize runtime content packages

Add a package manifest with package ID, version, schema compatibility range, dependencies, and
mandatory content hashes. Extend the existing profile validator to reject unknown
keys, inheritance cycles, missing references, invalid capability declarations, and incompatible
schema versions.

This permits profile/content delivery independently of the application binary without granting
runtime code execution.

For a personal/local package source, signatures add little beyond hashes and a trusted transport;
defer them. If remote packages from multiple authors are later accepted, signing becomes mandatory
for that channel and requires a separate trust/key-rotation/revocation design. Packages initially
load at startup. Hot update requires staged validation, atomic activation, rollback, and a
last-known-good copy before it is enabled.

### 8. Consolidate runtime feature policy

Create a typed feature-policy service composed from build capabilities, platform capabilities,
release channel, signed runtime policy, user experimental preferences, and active printer
capabilities. Migrate product macros only when both branches can safely coexist in one binary.
Retain compile-time guards for unavailable APIs, optional dependencies, and debug instrumentation.

The inventory classifies every macro as platform availability, optional dependency, debug/
instrumentation, algorithm experiment, or product policy. Only the final category is a default
runtime-policy migration candidate; the goal is not zero preprocessor conditionals.

## Incremental delivery gates and exit ramps

| Milestone | Smallest useful result | Gate to continue | Valid stopping point |
| --- | --- | --- | --- |
| Baseline | Registry export/profile diagnostics | Representative fixtures are stable | Keep validator only |
| Schema pilot | One family generated in shadow mode | Exact equivalence and acceptable build cost | Use schema as audit/docs only |
| Capability pilot | One owned-hardware branch uses typed facts | Visibility/behavior parity | Keep capability API narrow |
| Runtime UI metadata | One page uses stable gettext IDs | Same UI/default/serialization behavior | Keep remaining metadata compiled |
| Target pilot | Config tests link without GUI | No forbidden edge or build regression | Stop before broad splitting |
| Service pilot | One headless stateful responsibility | Thread/undo characterization passes | Do not decompose other coordinators |
| Package pilot | Local package validates and rolls back | Atomic failure recovery demonstrated | Keep packages startup-only/local |
| Feature policy pilot | One product switch migrated | On/off equivalence across platforms | Retain other macros |

No long-running migration branch is required. Shadow artifacts land in small commits, remain
inactive until proven, and every commit is buildable. A pilot that cannot demonstrate value after
one bounded family should be retired rather than defended as sunk cost.

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
- Upstream changes can invalidate schema mappings or reintroduce vendor checks. Run the
  reconciliation report after every upstream merge and keep local structural patches small.
- Generated code can increase compile time, binary size, and onboarding cost. Measure the pilot,
  keep bindings out of widely included headers, and retain a documented manual fallback.
- Runtime metadata can break translations. Treat gettext extraction and stable message IDs as
  compatibility surfaces.
- A solo maintainer can create tests that merely encode a mistaken assumption. Prefer normalized
  comparisons against the active implementation and real legacy artifacts over hand-written
  expected values wherever possible.

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

Track a lightweight migration dashboard in the research/configuration-contract output:

- settings still hand-registered versus shadow/generated;
- settings requiring edits in more than two ownership locations;
- vendor/model-name behavior branches remaining;
- GUI modules directly encoding configuration semantics;
- forbidden target dependency edges;
- clean and representative incremental build time, binary size, and canonical slice-time trends;
- upstream merges requiring manual reconciliation in schema/configuration files; and
- median files changed to add an in-scope printer capability or setting.

Success is not zero large files, zero macros, or a perfectly layered architecture. It is fewer
synchronized edits and merge conflicts, earlier detection of compatibility drift, and faster
support for the hardware this fork actually uses.

## Roadmap relationship

The tasks under **Infrastructure / CI → Architecture evolution** in `docs/roadmap.md` implement
this sequence. They are intentionally staged: compatibility baselines and a design record come
before schema generation; capability infrastructure precedes vendor-check migration; target
boundaries precede large coordinator extractions; package validation precedes independent content
updates. The roadmap should be adjusted as pilots produce evidence; this document defines gates,
not a commitment to complete every phase.
