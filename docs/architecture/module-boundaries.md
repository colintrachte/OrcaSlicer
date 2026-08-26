# ADR: Staged read-only configuration boundary

**Status:** Accepted for staged implementation; compiled target held at gate
**Date:** 2026-07-31 (scope corrected 2026-08-25: this is the Phase 4 pilot, not Phase 6 — see
`research/orcaslicer-improvement-plan.md` Workstream B3/C3, sequenced under "Phase 4 - First
enforceable boundaries")
**Decision scope:** Phase 4 `slic3r_config` pilot
**Validation scope:** Windows x64 Release; macOS/Linux validation deferred by user

## Context

`libslic3r` is one static target containing `Config.cpp`, `PrintConfig.cpp`, `AppConfig.cpp`,
`Preset.cpp`, slicing algorithms, format readers, and geometry. The single
`tests/libslic3r/libslic3r_tests` executable links the whole target, so configuration behavior has
no focused compile or link boundary even though these sources have no direct wxWidgets includes.

The existing boundaries are not layered:

- `Config.hpp` defines generic options, definitions, dynamic/static configuration, file loading,
  serialization hooks, and mutation APIs in one public header.
- `Config.cpp` implements both value/registry mechanics and INI, JSON, and G-code I/O. Its object
  has unresolved calls to `CNumericLocalesSetter`, `float_to_string_decimal_point`,
  `is_gcode_file`, `is_json_file`, and `header_slic3r_generated`, currently supplied by
  `LocalesUtils.cpp` and `Utils.cpp` in `libslic3r`.
- `Config.cpp` includes `Preset.hpp` and `PrintConfig.hpp`, although it contains no textual use of
  either name. Those reverse-facing includes make the intended dependency direction unclear.
- `PrintConfig.hpp` derives the print-specific schema from the generic configuration types.
  `PrintConfig.cpp` combines registry construction, defaults, validation, normalization, legacy
  conversion, and mutation in a translation unit of more than 11,000 lines.
- `AppConfig` and `Preset` are persistence/mutation consumers and must stay above the read-only
  boundary.

The weighted Windows dependency capture found 106 tracked `PrintConfig.hpp` consumers with 32.297
serial compiler-minutes of measured invalidation. Both `Config.hpp` and `PrintConfig.hpp` also enter
forced PCHs, so PCH tracking hides most direct lexical consumers. The direct-edge counts from the
240-source `GUI_App.hpp` capture are not repository-wide evidence and are not used as target-split
budgets.

## Decision

Use a staged seam. Do not move `Config.cpp` directly into a new static archive yet.

1. Establish a controlled leaf benchmark for `Config.cpp` and a focused compatibility test list.
2. Replace reverse includes only where a smaller dependency can carry the symbols actually used.
3. Separate generic option/registry mechanics from config file I/O and application utilities only
   where object-symbol evidence proves the dependency.
4. Create `slic3r_config` only when its compiled objects link a focused test executable without
   `libslic3r`, `libslic3r_gui`, wxWidgets, `Preset`, or `AppConfig`.
5. Keep print-schema registration above the generic core initially. Reconsider a read-only
   print-registry target only after registry fixtures make defaults and enum encoding observable.

This is a compilation firewall pilot, not a schema rewrite. Public serialization formats, option
keys, initialization order, defaults, enum values, legacy substitutions, and disabled-feature
behavior remain unchanged.

## Options considered

| Option | Focused consumer | Dependency direction | Risk | Decision |
| --- | --- | --- | --- | --- |
| Move `Config.cpp` into a static target now | No; it still needs symbols from `libslic3r` | Creates an archive cycle or leaks the monolith | High | Reject |
| Wrap existing files in an object library | Build grouping only; tests still need the monolith | Does not narrow headers or links | Medium | Reject |
| Split generic mechanics from I/O/utilities, then create a target | Yes, if the focused link closes | Enforces core → external libraries only | Medium | Adopt as pilot |
| Move generic config, print registry, presets, and migration together | Large target, no narrow read-only seam | Mixes read and mutation paths | Very high | Reject |

## Pilot budgets

All comparisons use the Windows Visual Studio Release/x64 tree and the bounded p2 compiler ceiling.

| Dimension | Required gate |
| --- | --- |
| Public surface | No new project-header dependency from the generic public interface; any new interface must be smaller than `Config.hpp` |
| Forbidden dependencies | `slic3r_config` has zero GUI/wxWidgets, `Preset`, `AppConfig`, and `PrintConfig` source or link dependencies |
| Focused link | A config-focused Catch2 executable links without `libslic3r` or `libslic3r_gui` |
| Focused behavior | Generic option creation, strict deserialization, equality/diff, nullable handling, CLI parsing, and cereal round-trip tests pass |
| Incremental build | A config implementation edit builds only the config target, focused test, and necessary final consumers; it recompiles zero GUI translation units |
| Leaf timing | Three-run p2 mean must not regress by more than 5% from the controlled pre-split `Config.cpp` leaf baseline |
| Clean build | One isolated p2 application build must not regress by more than 5% from the equivalent retained-patch baseline |
| Linked artifacts | `OrcaSlicer.dll` must not grow by more than 0.1%; duplicate config symbols are an automatic failure |
| Compatibility | Existing config tests plus registry/default/enum fixtures pass with no format or initialization changes |

## Consequences

- The next implementation is smaller than a target split and can fail safely at compile/test gates.
- A new target is delayed until it creates a real focused consumer rather than rearranging files.
- Config file I/O may remain in a separate adapter linked above the generic core.
- Print registry extraction remains blocked on explicit fixtures for keys, defaults, enum encoding,
  and feature-gated definitions.
- Cross-platform source rules still apply, but the current execution records only Windows evidence.

## First seam result

The pre-seam `Config.cpp` application leaf loop averaged 32.121 seconds over three Windows p2 runs
(30.676-34.981), rebuilding one object per run (`logs/benchmark_20260731_103330.json`). Removing
both `Preset.hpp` and `PrintConfig.hpp` failed immediately with 34 compile errors: config I/O uses
preset JSON-key macros and the print-schema `SupportMaterialStyle` enum. This proved the reverse
dependencies were real, despite not appearing as qualified names in the earlier text search.

The retained bounded seam moves the 39 shared preset JSON-key macros, with identical names and
values, into `PresetJsonKeys.hpp`. `Preset.hpp` re-exports that header for compatibility, while
`Config.cpp` includes it directly and no longer includes the full preset subsystem.
`PrintConfig.hpp` remains explicit because its enum is still required.

The first integration build rebuilt 275 objects and completed in 981.279 seconds after an earlier
900-second capped attempt. Three settled post-seam `Config.cpp` leaf runs averaged 32.111 seconds
(30.983-34.364), a 0.03% improvement versus baseline, with one object and one existing warning per
run (`logs/benchmark_20260731_110822.json` and `logs/benchmark_20260731_110910.json`). The timing
gate passes as neutral.

A freshly linked Windows RelWithDebInfo `libslic3r_tests` target passed all 19 selected config,
serialization, preset identity, rename, legacy-bundle, validation, and AppConfig tests. A
deterministic macro comparison found 39 before and 39 after, with zero missing, added, or changed
definitions. The final Visual Studio 2026 no-op application build completed in 3.049 seconds with
zero objects rebuilt, and `OrcaSlicer.dll` remained exactly 128,203,776 bytes, so the linked-artifact
no-growth check passes.

The compiled `slic3r_config` target remains held. `Config.cpp` still calls symbols owned by
`LocalesUtils.cpp` and `Utils.cpp`, still uses a print-schema enum, and still combines generic
mechanics with file I/O. Moving it wholesale would violate the forbidden-edge and focused-link
budgets. The next candidate must first isolate an object set whose unresolved symbols close without
linking back to `libslic3r`.

## Object-symbol evidence for the config-I/O seam (2026-08-25)

A direct symbol search of `Config.cpp` (2,136 lines) found every remaining reverse dependency on
`LocalesUtils.cpp`, `Utils.cpp`, and `PrintConfig.hpp`'s `SupportMaterialStyle` enum confined to
exactly 5 `ConfigBase` methods — nothing in the option/registry mechanics (`add`, `option`, `set`
for non-`double` types, `diff`, `equals`, `apply`, and the rest) touches any of them:

| Method | Lines | Dependency used |
| --- | --- | --- |
| `ConfigBase::set(key, double, create)` | 564-~572 | `float_to_string_decimal_point` (one branch of a 4-way type switch only) |
| `ConfigBase::load_string_map` | 803-818 | `CNumericLocalesSetter` |
| `ConfigBase::load(file, rule)` (dispatch overload) | 822-836 | `is_gcode_file`, `is_json_file` |
| `ConfigBase::load_from_json` | 848-1152 | `CNumericLocalesSetter`, `ConfigOptionEnum<SupportMaterialStyle>` (legacy `support_style` migration) |
| `ConfigBase::load(file, rule)` (second overload, INI path) | 1213-1228 | inherits the same dependency chain as the dispatch overload it calls |
| `ConfigBase::save` | 1585-1595 | `header_slic3r_generated` |

This closes action item 4's proposal: the seam is real and the object set is these 6 function
spans (`load_from_json` is one of them, and by far the largest — 305 lines of legacy-key
migration logic, not simple I/O plumbing). Moving them into a new `ConfigIO.cpp` in the same
`libslic3r` target (no new compiled target yet, per this ADR's staged decision) would let the
remaining ~1,800 lines of `Config.cpp` core mechanics drop the `LocalesUtils.hpp`, `Utils.hpp`,
and `PrintConfig.hpp` includes entirely.

`load_from_json`'s size is the reason this is recorded as evidence rather than executed tonight:
it is 305 lines of legacy-substitution business logic touching many option keys, not a mechanical
cut-paste candidate, and Executive Decision 1 in the plan requires characterization/compatibility
tests before moving code like this, not just object-symbol evidence that it's separable. The
actual extraction (action item 5) should be its own gated session: extract, rebuild, run the full
`[Config]`/`[Registry]`/`[Preset]` selectors plus the 67-vendor validator, and only then run the
formal leaf/clean-build timing benchmarks this ADR's pilot budget table requires.

## Action items

1. [x] Add and run a `ConfigLeaf` benchmark scenario.
2. [x] Capture the focused config/preset compatibility cases and current test result.
3. [x] Replace the `Preset.hpp` dependency with the lightweight `PresetJsonKeys.hpp` seam; retain
       the proven `PrintConfig.hpp` dependency.
4. [x] Use object symbols and source ownership to propose the smallest `ConfigCore.cpp` / config-I/O
       seam — see "Object-symbol evidence" above: 6 function spans, ~336 lines, dominated by
       `load_from_json`.
5. [ ] Extract the proposed `ConfigIO.cpp` seam within the existing `libslic3r` target (no new
       compiled target yet), verify compatibility tests and both application targets still build,
       then pilot `slic3r_config` only if the proposed object set closes without a forbidden edge.
