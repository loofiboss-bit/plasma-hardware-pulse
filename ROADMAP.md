# Loofi Pulse roadmap

## v0.13.0 — Telemetry, Diagnostics & Flight Recorder Enhancements

This development release adds expanded hardware telemetry, causal diagnostic verdicts, and rich incident analysis:

- Multi-fan discovery with RPM progress bars across detected hwmon fan controllers.
- NVMe controller telemetry with critical warning bitmask decoding (spare capacity, reliability, read-only mode, backup device) and composite temperature.
- Causal diagnosis verdicts for single-core saturation (`cpu.single-core-saturation`) and differentiated NVMe critical warnings (`storage.nvme-critical-warning` / `storage.nvme-thermal-threshold`) with dedicated `KNotification` alerts.
- Privacy-redacted kernel log correlation (`±15s` window around incidents) from `journalctl`/`dmesg` with automatic masking of IP, MAC, and home directory paths.
- Interactive side-by-side incident comparison in Flight Recorder with deltas for temperature, CPU load, responsiveness, and culprit processes.
- Enriched Markdown/JSON report exports including correlated kernel logs, NVMe health, and comparison analysis.
- Three Plasmoid panel display styles: Text Pill (0), Mini Sparkline (1), and Ring Gauge (2).
- Overview domain card gradient sparklines and direct action buttons.

The evidence and qualification gates live in [validation-v0.13.0.md](docs/validation-v0.13.0.md).

## Next development iteration

- Power profiles daemon integration (power-profiles-daemon / tuneD) with active switching and policy correlation.
- Detailed Wi-Fi link quality, signal strength, and network interface duplex telemetry.
- Long-term trend correlation against power profile history in SQLite persistent store.

## v1.0 — Stable daily-use qualification

A stable release requires recorded Fedora KDE installation/upgrade results, sustained retention and responsiveness measurements, and physical desktop/hardware qualification. Test keyboard navigation, accessible names and a screen reader, large fonts, narrow layouts, notification actions, tray behavior, actual suspend/resume, and relevant GPU/battery/USB-C configurations. Mark untested combinations as unqualified.

Preserve the existing configuration keys, Collector1 compatibility where documented, JSON report schema 2 and `.pulse-session` format 1. Background monitoring, login startup and persistent history remain separate opt-ins. Routine telemetry collection does not gain privilege or process-control capabilities.

## Previous development stages

- v0.12 stabilized measurements, GPU selection, collector lifecycle and bounded persistent history.
- v0.11 introduced single-instance behavior, preferences, tray integration, localization and persistent trends.
- v0.10 reorganized application/widget presentation and process/device inspection.
- v0.9 introduced the shared collector, incident investigations, local sessions, per-device telemetry and coordinated notifications.
