# Loofi Pulse roadmap

## v0.9.0 — Complete local incident investigation

The implementation brings incident investigation and expanded hardware coverage into one update:

- Shared D-Bus-activated user collector, asynchronous collection, client sampling/retention subscriptions and optional background monitoring.
- Problem markers, incident/range investigation, before/during/after comparison and transfer from widget to application.
- Versioned local session save/open, preserved diagnoses, atomic writes, bounded validation and privacy defaults.
- Full readable process inspection plus bounded historical observations, individual disks and interfaces, multiple GPUs and batteries, USB power and capability explanations.
- Central incident notifications with open actions and snooze, explicit collector connection state and separate login opt-in.

Compatibility includes existing configuration keys, widget choices, page identifiers and JSON report schema 2. Sessions use `.pulse-session` version 1. Recording stays local and in memory unless explicitly saved; process identities are excluded from saved output by default.

### Qualification gates

Implementation does not itself establish release qualification. Keep these evidence classes separate:

- Automated: build, deterministic core/session tests, D-Bus service/client tests, QML checks, AppStream and final diff review.
- Performance: 30-minute retention at 250 ms, 1,000 simulated processes, one outstanding collection job, stable memory after retention fills, and UI event-loop delay p95 at most 100 ms. Record CPU, RSS, collection time, workload and environment; do not infer a pass from a short unit test.
- Packaging: RPM and SRPM build, staged collector/activation verification, clean installed package and upgrade from the previous version with preserved settings.
- Physical desktop/hardware: Plasma/Wayland, notification routing, keyboard and screen-reader use, large fonts, narrow layouts, KDE themes, suspend/resume, GPU vendors, multiple batteries and USB-C power. Mark each untested combination as unqualified.

Publication and installation on the ordinary user system require an explicit request. Neither a generated package nor submitted CI establishes public release success.

## v1.0 — Stable daily-use qualification

Close any remaining v0.9 qualification gaps using reproducible evidence. Document the tested operating system, package version, desktop session, driver and hardware combinations. Refine unavailable-data explanations and accessibility based on observed results. Preserve the existing session/report compatibility and local privacy defaults.

## Product defaults

Fedora KDE Plasma 6 is the primary target. Background monitoring and login startup remain separate opt-ins. The app supports investigation and process inspection; process termination and system administration are outside this update. Use established Qt/KDE components, avoid overlapping collection jobs, and distinguish measured values from estimates and missing data.
