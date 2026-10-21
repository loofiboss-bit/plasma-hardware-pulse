# Loofi Pulse roadmap

## v0.12.0 — Stabilization for public development

This development update prioritizes trustworthy observations and maintainable operation over new telemetry features:

- Preserve unavailable/stale/warmup states throughout application, widget, history and exports. Select GPU sensors explicitly, assess every GPU, and avoid temperature-only bottleneck claims.
- Preserve existing collector replacement/resume recovery and distinguish live, paused, historical and disconnected presentation across clients. Qualify physical suspend/resume separately.
- Make persistent numerical history bounded, batched, retention-aware and recoverable, with visible failures and privacy tests.
- Keep process inspection read-only and hand process management to KDE System Monitor. Maintain existing session/report formats and navigation/configuration compatibility.
- Align version metadata, document installation and native dependencies, add practical contribution/security entry points, and use least-privilege Fedora 44 CI and development prereleases.

The evidence and remaining qualification gates live in [validation-v0.12.0.md](docs/validation-v0.12.0.md). Public visibility, branch settings and publication are separate actions tracked in [public-readiness.md](docs/public-readiness.md).

## Next development iteration

Use real reports from the public development version to prioritize sensor/driver gaps and desktop interaction defects. Expand tests when a concrete unsupported or incorrect case is found. Do not add new hardware claims based solely on fixtures. Keep diagnostics evidence-based and distinguish kernel limitations from product failures.

## v1.0 — Stable daily-use qualification

A stable release requires recorded Fedora KDE installation/upgrade results, sustained retention and responsiveness measurements, and physical desktop/hardware qualification. Test keyboard navigation, accessible names and a screen reader, large fonts, narrow layouts, notification actions, tray behavior, actual suspend/resume, and relevant GPU/battery/USB-C configurations. Mark untested combinations as unqualified.

Preserve the existing configuration keys, Collector1 compatibility where documented, JSON report schema 2 and `.pulse-session` format 1. Background monitoring, login startup and persistent history remain separate opt-ins. Routine telemetry collection does not gain privilege or process-control capabilities.

## Previous development stages

- v0.11 introduced single-instance behavior, preferences, tray integration, localization and persistent trends. v0.12 corrects reliability and qualification gaps discovered during review.
- v0.10 reorganized application/widget presentation and process/device inspection.
- v0.9 introduced the shared collector, incident investigations, local sessions, per-device telemetry and coordinated notifications.

Historical implementation milestones do not imply that every stated hardware or desktop qualification gate passed.
