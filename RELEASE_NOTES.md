# Loofi Pulse v0.12.0 — Development stabilization

This candidate prepares Loofi Pulse for public development. It focuses on reliable evidence, local data handling and a smaller process-inspection scope. It is a **development prerelease**, with physical Fedora KDE qualification still tracked separately.

## Changes

- Assess all available GPU devices, select and identify the temperature sensor consistently, and avoid reporting temperature alone as a graphics bottleneck.
- Preserve measured package power above 250 W and treat malformed/missing PSI averages as unavailable.
- Make live, paused, disconnected and saved-session presentation consistent across the application, tray and widget.
- Correct persistent-history size reclamation, same-minute resume, sample deduplication and incident calendar-time storage; back up schema migrations and preserve unknown-time legacy incidents. Expose storage/read failures without silently substituting empty data. Process identities remain excluded from this store.
- Align trend series by timestamps, show collection gaps and load incidents for the same requested interval; reject stale replies and offer retry after failed reads.
- Route tray and application quit through the same save/discard/cancel flow.
- Keep process inspection read-only. Process termination controls/API are removed; KDE System Monitor is the next action for process management.
- Share temperature-unit preferences across clients and correct unavailable values and temperature differences.
- Translate application/widget text and accessibility names into Swedish, validate quiet hours, restore notification controls and preserve readable page navigation in narrow windows.
- Add contribution and issue templates, an evidence-based validation record, Fedora 44 validation scripts, full-history secret scanning and a separate restricted development-release workflow.

## Compatibility and privacy

Existing configuration and page identifiers, report schema 2 and `.pulse-session` format 1 remain supported. Saved sessions retain their recorded diagnoses. The process-termination API is intentionally removed; Pulse is an investigation tool. Background monitoring, login startup and persistent history remain opt-ins. Session/report process details stay excluded unless explicitly enabled.

## Validation status

See [the v0.12 validation record](docs/validation-v0.12.0.md) for exact commands, revision, results and pending items. A passing portable/offscreen suite or RPM build does not establish physical desktop, accessibility, vendor-driver, suspend/resume or installed-upgrade qualification. Do not treat pending checks as passed.

## Installation

Use the matching Fedora RPM plus its checksums when published; see [README.md](README.md#install-on-fedora-kde). The widget bundle requires the matching installed native QML plugin and collector. Repository readiness does not itself create a tag, release, COPR build or KDE Store publication.
