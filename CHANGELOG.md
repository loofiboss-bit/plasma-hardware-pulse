# Changelog

## 0.13.0 — Development candidate (2026-10-25)

### Added

- Multi-fan telemetry discovery and RPM progress indicators across all detected sysfs hwmon fans.
- NVMe controller discovery via `/sys/class/nvme/nvme*`, decoding critical warning bitmasks and composite temperature.
- Causal diagnosis rule `cpu.single-core-saturation` for identifying asymmetric thread bottlenecking under moderate overall CPU load.
- Causal diagnosis rules `storage.nvme-critical-warning` (Severity 3, critical hardware alert) and `storage.nvme-thermal-threshold` (Severity 2, elevated thermal risk).
- Dedicated desktop notification support for hardware-critical events (`[Event/hardwareCritical]`) via KNotification.
- Privacy-redacted kernel log correlation (`±15s` window around incidents) from `journalctl`/`dmesg` with IP, MAC, and path masking.
- Interactive side-by-side incident comparison in Flight Recorder with delta metrics calculation (peak temperature, average CPU load, worst responsiveness, culprit process).
- Three selectable panel representation styles in Plasmoid: Text Pill (0), Mini Sparkline (1), and Ring Gauge (2).
- Enriched Markdown and JSON report exports including correlated kernel events, NVMe health observations, and incident comparison deltas.
- Gradient sparklines and direct action buttons on Overview domain cards.

### Changed

- Updated version metadata to 0.13.0 across application, widget, RPM spec and AppStream metadata.
- Preserved session format 1 and JSON report schema 2 with backward-compatible metadata extensions.

Session format 1, report schema 2 and existing configuration remain supported. Qualification results are recorded in [docs/validation-v0.13.0.md](docs/validation-v0.13.0.md).

## 0.12.0 — Development candidate (2026-10-21)

### Fixed

- Deterministic GPU sensor identity and diagnosis across available GPU devices.
- Temperature-only GPU risk severity, unclipped high RAPL power, and PSI avg60 availability.
- Consistent live, paused, disconnected and saved-session status presentation.
- Persistent-history bounds, same-minute resume, sample deduplication, calendar-time incidents and migration backups.
- Timestamp-aligned trends, visible collection gaps, paired incident queries and retryable read errors.
- Shared save/discard/cancel handling for explicit application and tray quit actions.
- Shared temperature-unit preferences in the application, tray and widget.
- Swedish/English UI translation, accessible page selection, notification controls and quiet-hours validation.

### Changed

- Process inspection is read-only; use KDE System Monitor for process management. Remove the process-termination controls and callable API.
- Prepare Fedora 44 development validation, least-privilege tag releases and checksum-verified full-history secret scanning.
- Add contributor guidance, issue/PR templates and explicit public-launch and qualification records.

Session format 1, report schema 2 and existing configuration remain supported. Qualification results are recorded separately in [docs/validation-v0.12.0.md](docs/validation-v0.12.0.md).

## 0.11.0 — 2026-10-19

Introduced single-instance behavior, settings, tray integration, persistent numerical trends and Swedish localization. These were development features; their presence did not establish complete physical daily-driver qualification.

## 0.10.0 — 2026-10-18

Reorganized application/widget navigation, device cards, process inspection and incident controls.

## 0.9.0 — 2026-10-18

Introduced a shared user-session collector, incident investigation, saved local sessions and expanded per-device inspection. Earlier changes are recorded in Git history and the RPM changelog.
