# APPROVALS Queue

Items requiring design review or architectural decision before implementation.
Completed items are removed; see CHANGELOG.md for what was done.

---

## [FEATURE/ARCH] Self-hosted Orca Cloud sync (#14318)

**Request:** A local, self-hosted alternative to Orca Cloud's profile-sync service, so users
aren't dependent on a third-party-hosted sync backend.
**Owner's read (2026-07-05):** doesn't use Bambu or their printers and never will, so this
has no personal urgency — but wants the _value to other users_ investigated before deciding,
since there isn't enough information yet to judge that.
**Investigation so far:** `ICloudServiceAgent` (`src/slic3r/Utils/ICloudServiceAgent.hpp`) is
a ~40-method interface, and settings sync is a small slice of it — login/OAuth (PKCE),
device/task subscriptions, model mall + publishing, ratings, camera streaming, and telemetry
are all bundled into the same interface as the sync calls
(`get_user_presets`/`get_setting_list`/`get_setting_list2`/`put_setting`/
`request_setting_id`/`delete_setting`). `OrcaCloudServiceAgent.hpp` even references an
internal "Orca Cloud Sync Protocol Specification" for the sync data structures
specifically (`OrcaCloudServiceAgent.hpp:40`) — meaning the sync sub-protocol may already be
documented well enough to implement against without touching the rest of the interface.
**Why this still isn't bounded:** "self-hosted" could mean (a) just documenting/publishing
that existing sync protocol so anyone can stand up a compatible server, (b) shipping a
minimal reference server (just the sync subset, none of the auth/model-mall/telemetry
surface), or (c) building a full drop-in `ICloudServiceAgent` replacement — those are wildly
different amounts of work and (c) is very likely not worth it. No maintainer or community
signal yet on which of these actually matters to users beyond the original issue.
**Action needed:** decide which of (a)/(b)/(c) above the value-investigation should target,
or whether to survey issue #14318's own comments for what requesters actually meant by
"self-hosted" before scoping further.
