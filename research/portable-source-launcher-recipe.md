# Portable source launcher recipe

**Status:** Implemented reference design and reusable project playbook

**Date:** 2026-08-29

**Reference implementation:** OrcaSlicer Windows source launcher

**Audience:** Maintainers building a fast desktop workflow for technical users

## Executive recipe

A good source launcher turns this:

> clone repository -> discover tools -> open a terminal -> learn build commands -> wait -> find the
> executable -> remember which build is useful

into this:

> clone repository -> click one unmistakable file -> follow one window -> build or launch

The reusable design has four layers:

```text
one obvious, dependency-light entry point
                 |
                 v
single-window launcher UI <----> deterministic console interface
                 |                         |
                 +------------+------------+
                              v
                  authoritative project build driver
                              |
                 +------------+------------+
                 v                         v
          discovered builds       logs and per-clone state
```

The launcher is not a second build system. It discovers the machine, translates a small set of
human choices into the repository's existing build driver, starts that work without showing a
console, discovers the result, and remembers what the user chose.

The most important product rule is that the interface should spend as little of the user's time as
possible. It should make the common path obvious, derive recommendations from the current machine,
keep required guidance inside one window, and expose the same operations through a scriptable
backend for maintainers and automation.

## Product contract

These requirements came from the launcher design and should be treated as acceptance criteria in
another project.

### Entry and startup

- A fresh checkout has one unmistakable file for an ordinary user to click.
- The primary entry point does not flash a console window.
- Secondary launcher scripts live in a clearly named helper directory.
- The launcher uses as few prerequisites as practical and can bootstrap its own runtime when that
  runtime is absent.
- Paths are derived from the checkout, executable, or selected target. A committed launcher never
  contains a path to one developer's clone.

### One-window experience

- The application has one top-level launcher window.
- Wizards, prerequisites, notifications, progress, errors, and suggested resolutions stay inside
  that window.
- Do not use message boxes for ordinary status or recoverable errors.
- External installers, official help pages, file browsers, and the application being launched may
  open separately when the user asks for them.
- The user should not need substantial reading or typing to understand the primary actions.

### Visual and language design

- A technical utility does not need to look like an unstyled system dialog. Give it a coherent
  visual system: deliberate color, spacing, hierarchy, project graphics, state colors, and clear
  primary actions.
- Use the project's real icon or logo. A graphical header, restrained gradient, cards, status
  badges, and progress treatment can make the launcher visually distinct without changing how it
  works.
- Apply the same theme to embedded build and prerequisite views. A polished main screen followed by
  a default platform dialog is an incomplete design.
- Preserve readable contrast, ordinary keyboard behavior, window resizing, and recognizable
  controls. Visual novelty must not make the tool harder to operate.
- Write for technical users in plain English. State what a control does and show useful values such
  as the source folder, logical processor count, installed RAM, recommended compiler workers, log
  location, and discovered build count.
- Avoid both extremes: promotional slogans and vague product language on one side, cryptic labels
  and unexplained shorthand on the other.
- Prefer labels such as **Build and launch**, **Source folder**, **Recommended compiler workers**,
  **Check requirements**, **Pin selected**, and **Launch automatically after 3 seconds**.
- Use symbols only as secondary visual cues. The text must remain clear if a symbol is missing,
  misrendered, or ignored by assistive technology.

### Builds and machine-aware recommendations

- The launcher can create production and development builds from source.
- It exposes the few settings that materially affect compile time or output: configuration, build
  scope, architecture, build system, concurrency, and rebuild options.
- Recommended values are calculated at runtime from the current computer. They are never values
  copied from the maintainer's workstation.
- Recommendations are visibly marked but remain overridable.
- The GUI delegates to the project's authoritative build driver instead of duplicating build
  logic.

### Prerequisites

- The launcher detects the prerequisites needed by the selected build, not merely a single global
  list.
- It automatically refreshes environment discovery after an install.
- It installs safe prerequisites when practical or provides one-click links to authoritative
  installers and concise in-window instructions.
- Project dependencies that the existing build system already compiles should remain part of that
  build rather than becoming a separate manual ritual.

### Launch selection

- The launcher normally starts the last build the user launched after a three-second delay.
- An obvious checkbox disables automatic launch.
- A pinned build overrides the last-used choice.
- If neither exists, the newest production build is the default.
- A development build is never silently preferred merely because it is newer than production.
- First-time setup never auto-launches. If an existing local build is found, select it and let the
  user decide. If no build is found, offer the build workflow without forcing it open.
- After the first successful source build, setup becomes complete and auto-launch may be enabled
  for the next launcher startup.

### Maintainability

- Every useful GUI operation also has a deterministic console command.
- Discovery and recommendation commands support machine-readable output.
- Mutating commands support dry runs where useful.
- Errors are logged with technical detail while the UI shows a short explanation and a relevant
  recovery action.
- Existing build, packaging, and CI entry points remain in place when other systems depend on their
  exact locations. Decluttering must not break canonical workflows.

## Reference file layout

This pattern gives users one choice while keeping the implementation accessible to maintainers:

```text
PROJECT ROOT/
|-- START PROJECT.vbs                 # obvious, console-free Windows entry point
|-- helper_scripts/
|   `-- project_launcher/
|       |-- launcher.ps1              # UI and console command surface
|       |-- launcher-console.bat      # optional cmd.exe adapter
|       |-- run.ps1                   # compatibility helper, if still needed
|       |-- run.bat                   # compatibility helper, if still needed
|       `-- README.md                 # backend command reference
|-- scripts/
|   `-- build.ps1                     # authoritative project build driver
|-- resources/
|   `-- project.ico                   # shortcut/application identity
`-- research/
    `-- portable-source-launcher-recipe.md
```

The exact languages may change, but the responsibilities should not. Keep the root entry point
tiny, keep UI policy and command translation in the launcher backend, and keep compilation in the
existing build driver.

## Startup and self-bootstrap sequence

On Windows, a small VBScript entry point is a practical way to start a graphical PowerShell
launcher without displaying a console. The reference sequence is:

1. Resolve the repository from the entry point's own location.
2. Look for the most widely available compatible runtime first.
3. Look for an already installed newer runtime second.
4. If no compatible runtime exists and WinGet is available, install the runtime silently with
   noninteractive agreement flags.
5. Retry runtime discovery.
6. If automatic bootstrap is impossible, open the official setup page. Do not present a stack
   trace or a wall of setup prose.
7. Start the launcher backend with a hidden host window. The launcher UI itself remains visible.

For OrcaSlicer, [`START ORCASLICER.vbs`](../START%20ORCASLICER.vbs) follows this sequence with
Windows PowerShell, PowerShell 7, the PowerShell application alias, WinGet, and Microsoft's
official installation page.

This is intentionally a modest bootstrap policy. Bundling a large private runtime makes a checkout
heavier and creates an update obligation. Depending on a maintainer-specific or editor-specific
runtime makes the launcher nonportable. Prefer an operating-system component, then an existing
installed runtime, then a standard package manager, and finally an official manual path.

## Launcher state model

The launcher should behave as a small state machine rather than a pile of startup popups.

| State | What the window does | Auto-launch |
| --- | --- | --- |
| First run, local build exists | Select newest local production build; otherwise newest local build; explain available actions in the status area | Off |
| First run, no local build | Show the normal launcher and offer **Create build** in the status area | Off |
| Setup complete, valid pinned build | Select and count down the pinned build | On if enabled |
| Setup complete, no pin, valid last-used build | Select and count down the last-used build | On if enabled |
| Setup complete, no remembered valid build | Select newest production build | On if enabled and a target exists |
| Remembered target disappeared | Fall back through the same priority order and make the missing choice recoverable | Never launch a nonexistent path |
| Build running | Keep the launcher responsive; show quiet background progress and log access | Paused |
| Build succeeded | Refresh discovery, remember the matching build, complete setup | Ready for next startup |
| Build failed | Preserve the window and offer **Open logs** or a prerequisite action | Off for that attempt |

Any meaningful user interaction should stop the current countdown. A countdown that fights the
user while they are trying to select a build is not a convenience.

### Per-clone persistence

Preferences should not leak between two clones of the same repository. Store a small settings
record under the user's application-data directory and key the filename or namespace with a stable
hash of the normalized repository path.

The minimal schema is:

```json
{
  "SetupComplete": false,
  "AutoLaunchEnabled": false,
  "LastTargetPath": null,
  "PinnedTargetPath": null
}
```

`PinnedTargetPath` and `LastTargetPath` are separate concepts. Launching a one-off diagnostic build
must not silently replace the user's pinned daily build.

The OrcaSlicer implementation stores this record in
`%APPDATA%\OrcaSlicer\SourceLauncher\<repository-path-hash>.json`.

## Build discovery and classification

Discovery should return normalized records rather than raw paths. A useful record contains:

```json
{
  "Name": "OrcaSlicer",
  "Kind": "Production",
  "Configuration": "Release",
  "Architecture": "x64",
  "Version": "2.x",
  "Updated": "timestamp",
  "Path": "absolute executable path",
  "Source": "build directory or installer"
}
```

Search only well-defined project outputs and supported installation sources. Deduplicate by
normalized executable path. Validate that a candidate is a file with the expected executable name
before offering it.

Classification is semantic:

- Optimized release output is **Production**.
- Debug and diagnostic-symbol configurations are **Development**.
- Installed release copies are **Production**.

Do not label every local build as development. The fact that an executable came from source says
nothing about whether it is a production configuration.

## Build options without an expert-only interface

Expose decisions in the user's language and translate them internally:

| User-facing choice | Typical build-driver mapping | Compile-time effect |
| --- | --- | --- |
| Production | `Release` | Fastest runtime; normal daily build |
| Development | `RelWithDebInfo` or `Debug` | More diagnostics; often slower or larger |
| Complete | Build dependencies, application, and staged output | Slowest initial path; most self-contained |
| Application only | Reuse prepared dependencies and build the app target | Faster iteration |
| Automatic workers | Pass machine-derived bounded concurrency | Safe default |
| Custom workers | Pass the user's explicit ceiling | Expert override |
| Fresh configure | Discard cached configuration | Slower; resolves stale configuration |
| Clean first | Recompile existing outputs | Slowest; resolves stale objects |
| Run after build | Launch only after success | Convenience, not compilation policy |

Mark the recommended configuration, architecture, build system, and worker count directly in their
labels. Put the short consequence next to the choice. Avoid a separate page of documentation that
the user must read before they can proceed.

Do not compress these labels into unexplained jargon merely to make the interface look technical.
For example, **Recommended compiler workers: 3 (based on installed RAM)** is clearer than
**P3 smart limit** or **RAM ceiling**. Likewise, **Source folder** is clearer than **checkout** for a
path label, even though the implementation is a Git checkout.

## Dynamic hardware recommendations

The recommendation function must inspect the machine on every computer where the launcher runs.
For C++ builds, the main inputs are:

- logical processor count for the hard CPU ceiling;
- installed physical memory for the memory ceiling;
- physical core count for useful display context;
- operating-system/process architecture for the default target architecture.

A conservative initial concurrency formula is:

```text
memory_limited_workers = max(1, floor(installed_memory_GB / memory_per_worker_GB))
recommended_workers    = max(1, min(logical_processors, memory_limited_workers))
```

The OrcaSlicer reference uses 8 GB per worker because its C++ translation units and precompiled
headers can be memory intensive. That constant is a **project policy**, not a universal hardware
fact. Another project should derive its own value from build measurements and keep it configurable
in one place.

Hardware queries can fail under restricted policies. The reference launcher first uses Windows
management data, then falls back to read-only kernel APIs for installed memory and active logical
processors. If memory still cannot be determined, it chooses a small CPU-derived fallback instead
of pretending to know the machine.

For projects with enough history, improve this policy by recording successful and resource-failed
worker counts per build identity. Hardware detection gives the first recommendation; measured local
outcomes can safely refine later recommendations.

## Prerequisite assistance

Prerequisites should be described as capabilities, not merely executable names. For each item,
return:

```text
name | ready | concise detail | authoritative help/install URL
```

Before each refresh, merge current process, user, and machine search paths. Installers commonly
update the persistent environment without updating an already-running process.

Filter the checklist by the selected build. In the reference implementation:

- Visual Studio 2026 with the C++ toolchain and CMake is required for the supported Windows build.
- Git is required.
- Perl is required for a complete dependency build but not an application-only rebuild.
- Bundled source dependencies are compiled by the repository's existing complete-build path.

Those are OrcaSlicer facts, not reusable defaults. A Rust project might check Rustup and a linker;
an Electron project might check Node and the selected package manager. The reusable rule is to
detect capabilities, explain only what is missing, and route installation through official sources.

## Quiet background execution

Hiding the launcher's initial console is only half the job. Every child build process must also be
created with console-free process settings. In the reference PowerShell implementation this means:

```text
UseShellExecute = false
CreateNoWindow  = true
WindowStyle     = Hidden
```

The build driver receives a noninteractive/no-pause flag. Timestamped logs receive the detailed
output, while a UI timer watches for process completion and updates the single window.

Do not redirect away the only copy of an error. Quiet execution requires better logging and
recovery, not less evidence.

## In-window error handling and resolution

Use two levels of error reporting:

1. Write technical context, exception details, and timestamps to a launcher error log.
2. Replace the status area with a short human explanation and one relevant action.

Examples:

| Failure | In-window message | Resolution action |
| --- | --- | --- |
| Missing build driver | Source build script is missing from this checkout | Open source folder |
| Missing selected prerequisite | This build needs additional tools | Review prerequisites |
| Build exits unsuccessfully | Build stopped with exit code N | Open logs |
| Shortcut creation fails | Desktop shortcut could not be created | Try again |
| Unexpected UI exception | Launcher hit an unexpected problem | Open error log |

The GUI should register its global UI exception handler before creating controls. In WinForms,
changing the unhandled-exception mode after controls exist can itself fail. Recovery handlers also
must not make tests pass falsely: smoke tests should assert that the error log did not grow while
the tested action ran.

### Embedded-view lifetime pitfall

Converting a modal dialog into a child view changes variable lifetime. In PowerShell WinForms, an
event handler that refers to local controls after the view-construction function returns may later
see null values. Modal code can hide this bug because the function remains active while the dialog
is open.

The OrcaSlicer Cancel failure came from exactly this transition. The fix was to give every embedded
event handler a closure and to capture update helpers as scriptblocks. Functions needed across the
dynamic boundary are captured explicitly. The regression smoke test now opens and closes both the
build and prerequisite views, checks visibility, and verifies that no recovered UI exception was
silently logged.

The reusable lesson is broader than PowerShell: when a wizard becomes asynchronous or embedded,
re-audit ownership and lifetime for every callback.

## Shortcut strategy

A committed Windows shortcut is normally wrong because a `.lnk` stores machine-specific target
paths. Instead, let the launcher create it on demand:

1. Resolve the current clone's root.
2. Resolve the operating system's Desktop known folder, including redirected or OneDrive desktops.
3. Create a shortcut whose target is the console-free system host.
4. Pass the current clone's entry point as its argument.
5. Use the project's real `.ico` resource.
6. Set the working directory to the clone root.

The resulting shortcut can be renamed, moved, or pinned through Windows in the normal way. Recreate
it from the launcher after moving or recloning the project.

## Console contract

The console interface is not a second-class debugging escape hatch. It is the deterministic
contract that makes the launcher testable and lets scripts, maintainers, and coding agents drive
the same behavior as the GUI.

Recommended verbs are:

| Verb | Purpose | JSON | Dry run |
| --- | --- | --- | --- |
| `List` | Discover launchable builds | Yes | Not needed |
| `Recommend` | Report hardware and recommended settings | Yes | Not needed |
| `Prerequisites` | Report capability readiness | Yes | Not needed |
| `CreateShortcut` | Create the per-clone desktop shortcut | Yes | Yes |
| `Launch` | Apply pin/last-used/production priority and run | Yes | Yes |
| `Pin` / `Unpin` | Manage the exact preferred build | Yes | Yes |
| `Build` | Translate build choices into the authoritative build driver | Yes for plan/dry run | Yes |

OrcaSlicer examples:

```powershell
# Read-only discovery
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command List -Json
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Recommend -Json
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Prerequisites -Json

# Inspect a build plan without changing anything
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Build `
    -Configuration Release -BuildScope Complete -DryRun -Json

# Build, launch, and pin
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Build `
    -Configuration Release -BuildScope Complete
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Launch
.\helper_scripts\orcaslicer_launcher\launcher.ps1 -Command Pin `
    -TargetPath '<path returned by List>'
```

The GUI and console paths must call the same discovery, recommendation, settings, prerequisite, and
command-translation functions. Otherwise they will drift.

## OrcaSlicer reference implementation

The recipe is currently embodied in these files:

- [`START ORCASLICER.vbs`](../START%20ORCASLICER.vbs) is the one obvious root entry point and
  launcher-runtime bootstrap.
- [`helper_scripts/orcaslicer_launcher/launcher.ps1`](../helper_scripts/orcaslicer_launcher/launcher.ps1)
  contains the single-window UI and console commands.
- [`helper_scripts/orcaslicer_launcher/README.md`](../helper_scripts/orcaslicer_launcher/README.md)
  is the short backend command reference.
- [`scripts/build.ps1`](../scripts/build.ps1) remains the authoritative build implementation.
- [`scripts/create-shortcut.ps1`](../scripts/create-shortcut.ps1) and the relocated compatibility
  launch helpers route through the new helper layout.
- [`README.md`](../README.md) tells a newly cloned Windows user exactly what to double-click.
- [`resources/images/OrcaSlicer.ico`](../resources/images/OrcaSlicer.ico) supplies the shortcut icon.

The implementation supports production/development builds, Release/RelWithDebInfo/Debug,
complete/application-only scope, x64/ARM64, Visual Studio/experimental Ninja, automatic/custom
workers, fresh configuration, clean rebuild, and run-after-build. Automatic concurrency is capped
by detected processors and RAM and is passed to the build driver as an explicit maximum.

The graphical implementation uses a dark high-contrast canvas, a cyan-to-violet header treatment,
the actual OrcaSlicer logo, rounded cards and controls, color-coded Production/Development/Pinned
and prerequisite states, an auto-launch progress indicator, and owner-drawn build tables. The same
theme is applied to the embedded build and prerequisite views. Labels remain literal and technical;
the header reports the source folder and detected hardware instead of displaying a slogan.

At the time of this document, the launcher also creates
`OrcaSlicer Source Launcher.lnk` on the current user's resolved Desktop. The shortcut targets the
current clone through `wscript.exe` and uses the OrcaSlicer icon. The generated shortcut is a local
artifact; it is deliberately not a committed repository file.

Upstream platform build scripts remain at the repository root where CI, packaging, and upstream
documentation expect them. The launcher-specific files were consolidated without renaming or
moving those authoritative interfaces.

## Verification recipe

Validate the smallest authoritative layer first, then the integrated behavior.

### Static checks

- Parse the backend with every supported PowerShell runtime.
- Check all changed files for whitespace and patch errors.
- Confirm the root contains only one new user-facing Windows launcher-like file.
- Search documentation and scripts for stale paths after moving helpers.
- Confirm no committed launcher file contains a developer-specific absolute clone path.

### Read-only behavior

- Run build discovery in JSON mode and validate required fields.
- Run hardware recommendation in JSON mode and compare it with the current machine.
- Simulate restricted hardware-query access and confirm the fallback remains machine-derived.
- Run prerequisite discovery in JSON mode.
- Verify Release output is Production and diagnostic configurations are Development.

### Mutation planning

- Dry-run complete and application-only builds.
- Dry-run launch selection with a pin, with only last-used state, and with neither.
- Dry-run shortcut creation and validate the resolved path.

### UI regression checks

- Start through the root entry point and confirm no console flashes.
- Confirm only one top-level launcher window exists.
- Open and cancel the build view.
- Open and close prerequisites.
- Confirm neither action appends an unexpected UI error.
- Confirm first run with an existing build does not open a wizard or auto-launch.
- Confirm first run without a build offers an in-window action.
- Confirm the three-second countdown stops on user interaction.
- Confirm build child processes do not create visible consoles.
- Render the main, build, and prerequisite views and inspect them at the default and minimum window
  sizes. Check clipping, column fill, contrast, selection, disabled controls, and long paths.
- Run the UI under the oldest supported scripting runtime. Confirm non-ASCII symbols render
  correctly or construct optional glyphs at runtime instead of depending on source-file decoding.
- Review every visible string once for promotional language and once for unexplained shorthand.

### End-to-end checks

- Create the shortcut and inspect its target, arguments, working directory, and icon.
- Run a real development build and a real production build on a clean clone.
- Confirm the built executable is discovered and classified correctly.
- Confirm first successful build completes setup and prepares auto-launch for the next startup.
- Move or clone the repository elsewhere and recreate the shortcut there.
- Test prerequisite recovery on a clean Windows environment or disposable virtual machine.

For the OrcaSlicer implementation, parser checks, JSON discovery/recommendation/prerequisite
commands, build and launch dry runs, the hidden VBScript bootstrap, relocated helper resolution,
shortcut metadata, and the embedded-view close smoke test have passed. Patch whitespace checks also
pass. A full clean production compilation and a clean-machine bootstrap were not run as part of
this documentation pass and remain the highest-value end-to-end validation.

## Failure lessons

The following approaches looked convenient but violated the product contract or caused real
failures:

- **Batch file as the primary entry point:** it flashes a console. Use a graphical, dependency-light
  host for the user-facing entry.
- **A shortcut tied to a private/editor runtime:** it works only on the developer's machine. Bootstrap
  a stable system runtime instead.
- **A committed machine-specific shortcut:** its absolute target becomes stale. Generate the
  shortcut from the current clone.
- **Automatic first-run wizard:** it interrupts users and is wrong when a build already exists.
  Keep the main window visible and offer the next action inline.
- **Modal dialogs converted mechanically to embedded views:** callback lifetimes change and can
  produce null-control failures. Capture state deliberately and test cancellation.
- **Message boxes for recoverable events:** they fragment the workflow and demand dismissal. Use a
  persistent status/resolution area.
- **Global settings shared by every clone:** one checkout's remembered paths contaminate another.
  Namespace state by repository identity.
- **Newest executable wins:** a fresh debug build can unexpectedly replace the normal production
  build. Apply pin, last-used, then newest-production priority.
- **Every local output labeled development:** Release-from-source is still production.
- **Hardcoded worker count:** it encodes one PC and may be unsafe or wasteful elsewhere. Inspect the
  machine and bound by both CPU and memory.
- **Assuming one hardware API always works:** enterprise policy can deny management queries. Provide
  a lower-level read-only fallback and an honest conservative default.
- **Quiet process with missing diagnostics:** hiding the console also hides the only clue. Preserve
  timestamped logs and expose a direct recovery action.
- **Exception recovery without error-sensitive tests:** a global handler can make broken UI tests
  appear successful. Assert that recovery logs remain unchanged during smoke tests.
- **Decluttering canonical upstream scripts:** moving files referenced by CI or packaging breaks the
  project. Consolidate only launcher-specific helpers unless all consumers are migrated together.
- **Leaving the platform-default visual hierarchy untouched:** a functional launcher can still be
  slow to scan when every control has equal weight. Use project graphics, state colors, cards, and
  a clear primary action.
- **Styling only the first screen:** default-looking embedded views make the interface feel patched
  together. Theme every state in the same top-level window.
- **Replacing plain wording with slogans:** visual polish does not justify promotional copy.
  Technical users need the current state, values, paths, and actions.
- **Replacing slogans with jargon:** terse labels such as `P3`, `smart limit`, `flow`, or `checkout`
  can be less clear than ordinary technical English. Prefer literal descriptions even when they
  take slightly more space.
- **Embedding decorative Unicode directly in a legacy script:** older runtimes may decode a UTF-8
  source file differently and display mojibake. Keep symbols optional, construct them from code
  points when necessary, and retain a complete text label.
- **Fixed table columns inside a resizable view:** owner-drawn headers can leave bright unpainted
  gaps or clip paths. Recompute the final column width when the view changes size.

## Porting checklist

Use this sequence when applying the recipe to another project:

1. Identify the authoritative build and run commands. Do not design the GUI first.
2. Inventory root scripts and determine which names and locations are external contracts.
3. Define production and development output classifications.
4. Define the target discovery record and search locations.
5. Define the minimal prerequisite capabilities for each build scope.
6. Measure memory-heavy compilation and choose an initial memory-per-worker policy.
7. Implement shared discovery, recommendation, prerequisite, settings, and command-translation
   functions.
8. Expose those functions as JSON-capable console verbs with dry runs.
9. Define a compact visual system using the project's actual graphics, accessible colors, spacing,
   state badges, and one obvious primary action.
10. Write literal technical labels, then audit them separately for slogans and unexplained jargon.
11. Build the one-window UI on top of the same backend functions and apply the visual system to
    every embedded view.
12. Add hidden child-process execution, logs, and in-window recovery actions.
13. Add per-clone settings and the pin/last-used/newest-production priority.
14. Add the three-second countdown, disabled for first-time setup.
15. Add a tiny console-free bootstrap and on-demand branded shortcut creation.
16. Consolidate launcher-only helpers and update references without disturbing canonical scripts.
17. Run static, console, UI-lifetime, rendered-layout, language, shortcut, clean-clone, and
    clean-machine verification.

Before declaring the port complete, replace every OrcaSlicer-specific policy in this document:
toolchain version, dependency list, executable names, build configurations, output directories,
memory-per-worker estimate, application-data namespace, icon, build driver, and official setup
links. The architecture is reusable; those values are not.

## Completion definition

The launcher is complete when a person can clone the project on a supported computer, identify the
correct file without reading a script inventory, start it without seeing a console, understand the
current state in one window, obtain missing prerequisites, create a recommended source build,
launch it, recreate a branded desktop shortcut, and later reach their preferred build after a
three-second cancellable delay. The interface should look intentionally designed, use the project's
graphics, and communicate in plain technical English without slogans or cryptic shorthand.

The same completion test should also be possible through deterministic console commands. That
parallel contract is what keeps a friendly launcher maintainable rather than turning it into an
opaque second build system.
