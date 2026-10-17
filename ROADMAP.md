# Loofi Pulse roadmap

## v0.8.0 — Reliable measurements and clear guidance

Implementation scope: consistent responsiveness and power semantics, risk-based colours, source and freshness details, per-device rate baselines, PID/start-time process identity, complete retention, stable diagnosis rule identities, section navigation, grouped settings, compact process rows and bounded panel presets. JSON schema 2 and existing configuration keys remain compatible. Reports exclude process identities by default.

## v0.9 — Incident investigation and local sessions

- Mark a problem now; select an interval and inspect onset, peak and recovery.
- Compare before/during/after and export the selected episode.
- Explicitly save and reopen local sessions; keep ordinary rolling history in memory.
- Transfer the actual widget incident recording to the application.
- Episode notifications with open actions, snooze and coordination to avoid duplicate alerts.
- Test reopening, incomplete files, gaps and preservation of the original incident.

## v0.10 — Hardware coverage and overhead

- Enumerate GPUs with explicit selection and device-bound histories.
- Add per-disk and network views, IOPS and average disk request times.
- Aggregate multiple batteries and recognize USB-C external power sources.
- Maintain episodes incrementally, separate sampling frequencies and collect through one non-overlapping worker job.
- Measure CPU, memory and event-loop delay with long histories and many processes.

## v1.0 — Stable daily use

- Complete accessibility and consistent English UI terminology.
- Document hardware capability boundaries and unavailable interfaces.
- Automate upgrades, RPM installation and installed-artifact verification.
- Pass build, CTest, QML, metadata and packaging checks for each delivery.

## Product defaults

KDE Plasma 6 and Fedora remain primary targets, using C++20, Qt and Kirigami. Recording stays local and in memory unless the user explicitly saves a session. Saved sessions and reports exclude process identities unless explicitly enabled. System changes require an explicit user action. Automated evidence and physical hardware qualification are reported separately. No future milestone is claimed as delivered by v0.8.
