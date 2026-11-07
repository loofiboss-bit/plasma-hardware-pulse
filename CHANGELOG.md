# Changelog

## 0.18.0 — Dedicated Hardware Pages, Logical Navigation & Multi-Language Hub (2026-11-07)

### Added
- **Dedicated Hardware Pages**:
  - `GpuPage.qml`: Dedicated GPU load gauge, core clock, boost frequency, board power (W), temperature, and VRAM allocation bar.
  - `BatteryPage.qml`: Power state badge (AC/Battery), charge rate (W), time-to-full/empty, health, and device table for BAT0 and HID++/Bluetooth peripherals.
  - `SensorsPage.qml`: Active cooling fans with animated RPM indicators, complete `hwmon` sensor domain matrix, and thermal velocity.
  - `ProfilesPage.qml`: 3 hero interactive cards (Power-saver, Balanced, Performance) hooked into `power-profiles-daemon`, active profile holds, and causal rules.
- **Logical Sidebar Architecture**: Restructured left sidebar navigation into `SYSTEM`, `HARDWARE & DEVICES`, `CONTROL`, and `FLIGHT RECORDER`, removing the ambiguous "Diagnostics" label.
- **Pinned Settings Action**: Fastened `Settings` at the bottom above the system info badge with a dedicated divider line.
- **Application Language Selection**: Configurable language support under Settings (English default, Swedish, and Follow System OS locale).
- **Expanded About & Documentation Hub**: Restored complete documentation cards, direct GitHub documentation links, issue tracker, license, and shortcuts guide in Settings.
- **High-Resolution Branding & User Guide**: Brand-new SVG banner (`assets/banner.svg`) and complete user guide (`docs/user-guide.md`) with live app screenshots.

### Changed
- Standardized UI string identifiers and source text to English while preserving complete Swedish catalog coverage (`po/sv`).
- Fixed QML layout geometry in `SettingsPage.qml` to ensure smooth full-height scrolling.

## 0.17.0 — UX/UI Modernization & Genuine Hardware Telemetry (2026-11-07)

### Added
- **5 Core Telemetry Pillars**: Clear hierarchy on Overview page for CPU, Memory, PSI, GPU, and Power & Battery.
- **Dynamic Dual-Line Activity Chart**: Clean real-time Canvas chart for CPU load and temperature with dynamic min/max y-axis markers and graceful empty states.
- **Dynamic Host System Detection**: `PulseTelemetryController` dynamically detects distribution name, kernel version, and chassis/product name via `QSysInfo` and DMI `/sys/class/dmi/id/product_name`.
- **Grouped Incident History**: Causal diagnosis episodes are clustered by rule ID or headline with `[N×]` occurrence multipliers, duration sums, and latest timestamps in both Flight Recorder and widget.
- **Compact Plasma 6 Widget Popup**: Focused popup layout with status pill, 2x2 key metrics grid, switchable mini sparkline channels, and direct power profile selector.

### Changed
- Replaced all AI-generated placeholder data and fallback numbers (`64°C`, `28%`, `40°C`, `17.8W`, `97%`) with real sensor readings and semantic `—` placeholders.
- Replaced hardcoded "HP EliteBook 840 G8" hardware badge with real host telemetry.
- Consolidated duplicate incident controls in widget popup to a single action.
- Restored complete 7-section navigation in `PulseSidebar`.

## 0.16.0 — UX/UI Simplification & KDE-Native Overhaul (2026-11-07)

### Added
- **Refined Navigation Sidebar**: 9-section sidebar (`Översikt`, `Prestanda`, `Effekt & Kylning`, `Grafik (GPU)`, `Batteri`, `Sensorer`, `Historik & Logg`, `Profiler`, `Inställningar`) and host system identity badge (Fedora Linux, KDE Plasma, Kernel, hardware model).
- **Streamlined Overview Dashboard**:
  - Live real-time dual-line chart (`Systembelastning (realtid)`) for CPU and GPU temperatures.
  - 5-metric overview cards for CPU, GPU, Power, Battery, and Cooling.
  - Two-column bottom layout with per-core CPU tiles, hardware temperature sensors, interactive power profile switcher, and individual battery monitors.
- **Flight Recorder Status Bar**: Bottom status line showing recording status, sampling frequency, and timestamps.
- **True Linux PSI Metrics**: Direct exposure of memory, CPU, and I/O pressure stall percentages (`some avg10 / full avg10`) replacing synthetic scores.

### Changed
- Replaced misleading "Warming up" status label with "Initializing...".
- Eliminated redundant process listing from Memory view in favor of dedicated Processes view.
- Removed duplicate modal settings dialog in favor of dedicated `SettingsPage`.
- Removed `PulseCard` hover-lift animations, shadows, and gradient overlays in favor of clean Breeze surfaces.
- Updated Swedish translations for 100% complete localized coverage.

## 0.15.0 — UI/UX Redesign & Visual Elevation (2026-11-06)

### Added

- **Modernized Branding & Identity**: New application logo featuring an ECG pulse wave forming an "L" shape (`assets/logo.svg` and `assets/logo-symbolic.svg`), aligned with KDE Human Interface Guidelines.
- **Shared Component Library (`com.github.loofi.hardwarepulse`)**:
  - `PulseTheme`: Central design tokens for typography, spacing, border radiuses, animation curves, and semantic palette colors.
  - `PulseCard`: Elevated surfaces with subtle border strokes, hover lift, and state-aware glow accents.
  - `StatusPill`: Severity-coded status pills with dot indicators.
  - `PulseSparkline`: GPU-accelerated bezier curve trendlines using QtQuick.Shapes with fill gradients.
  - `PulseAreaChart`: Interactive multi-series area chart with threshold lines, time scrubbing, and customizable point markers.
  - `PulseRingGauge`: Circular gauge with round caps, gradient coloring, and alert pulse animations.
  - `MetricCard`: Information-dense metric tile with real-time value, unit, status pill, and embedded trendline.
  - `HealthHero`: 6-domain health ring hero with diagnostic verdict and quick-filter chips.
  - `FindingRow`: Diagnostic anomaly row with severity badges and direct jump-to navigation.
  - `PulsePageHeader`: Consistent header strip with 5/15/30-minute time window selector and incident capture.
  - `DeviceCard`: Modular device status card for NVMe, storage partitions, batteries, and network interfaces.
- **Responsive Navigation Sidebar**: Collapsible navigation rail with smooth transitions (240px expanded to 68px collapsed on narrow viewports or toggle), categorized section headers, live severity badges, and full keyboard accessibility.
- **Redesigned Overview Page**: Features the 6-domain `HealthHero` ring, a 2x3 responsive `MetricCard` grid with live trendlines, and a severity-sorted diagnostic findings card.
- **Standardized Detail Pages**: `PowerThermalPage`, `PressureMemoryPage`, and `StorageNetworkPage` upgraded with `PulsePageHeader`, hero `PulseAreaChart` time series, and elevated device cards.
- **Refined Process & Flight Recorder Views**: Elevated search/sort toolbar, improved process table contrast, standardized header strip, and incident timeline markers.
- **Plasma 6 Widget Enhancements**: Integrated `PulseRingGauge` and `PulseSparkline` in panel representations, elevated popup cards with `PulseCard` surfaces.
- **Complete Swedish Localization**: 100% translation coverage across all newly introduced UI components, headers, metrics, and diagnostics.

### Changed

- Transitioned all legacy inline canvas charts to GPU-accelerated QtQuick.Shapes path rendering.
- Replaced hardcoded dimensions and layout margins with centralized `PulseTheme` tokens.

### Compatibility

- Preserves `.pulse-session` format 1, JSON report schema 2, Collector1 D-Bus API, and existing configuration keys.
- Fully backwards-compatible with existing Plasma 6 panel configurations.

## 0.14.0 — Diagnostic Intelligence & Reliability (2026-11-03)

### Added

- Structured diagnostic assessments distinguish observations, suspected causes, and confirmed problems, with explicit missing evidence and actionable recommendations.
- Flight Recorder incident summaries include wall-clock timestamps, evidence, uncertainty, and before/during/after coverage details.
- Kernel event exports include real journal timestamps and incident correlation identifiers while retaining report schema 2.
- Collector clients can declare UI activity so inactive desktop windows no longer force the fastest shared polling interval; retention requests remain honored.

### Fixed

- Do not infer CPU throttling from high load and a power-saver profile, or sustained capping from a single PL1 sample.
- Do not report NetworkManager signal percentage as measured dBm or infer Wi-Fi throughput degradation without throughput evidence.
- Keep asynchronous SSID, signal, frequency, and bitrate readings associated with their own adapter when several Wi-Fi devices are active.
- Expire unavailable or stale Wi-Fi and power-profile observations instead of carrying them into new diagnoses.
- Fetch incident kernel logs asynchronously; correlate them using wall-clock sample timestamps rather than monotonic uptime.
- Redact network identifiers and additional address forms from default report exports.
- Respect KDE's reduced-motion setting and stop decorative animations on hidden pages.

### Compatibility

- Keep `.pulse-session` format 1, JSON report schema 2, Collector1, existing settings, and existing diagnostic rule identifiers. New session evidence fields are optional.
- Routine collection remains local and uses the current user permissions. Background monitoring, login startup, and persistent history remain separate opt-ins.

Automated and manual qualification results are recorded in [docs/validation-v0.14.0.md](docs/validation-v0.14.0.md).

## 0.13.0 — Comprehensive Evolution (2026-10-25)

### Added

- **Power Profiles Daemon (PPD) Integration**: Interactive quick switcher pill in header and widget, active profile holds inspection (`net.hadess.PowerProfiles`), and causal diagnosis rule `power.profile-throttling` (Bottleneck severity) when power-saver caps performance on AC power.
- **Hardware Link Quality & Wi-Fi Telemetry**: Active SSID, frequency band (2.4/5/6 GHz), signal strength (% and dBm), bitrate, and Ethernet duplex mode via sysfs and NetworkManager D-Bus; causal rule `network.wifi-link-degradation`.
- **Memory Compression & Cgroup v2 Attribution**: ZRAM capacity, compression ratio, space-saved statistics, and cgroup v2 slice memory breakdown (`user.slice`, `system.slice`, `machine.slice`) with causal rule `memory.zram-exhaustion`.
- **Smart Flight Recorder & Interactive HTML Reports**: Automated incident bookmarks with 60s cooldown, SQLite v3 schema extensions (`batteryHealth`, `zramRatio`, `wifiSignal`), and self-contained interactive HTML incident report generation with embedded SVG trend charts.
- **Modern KDE 6 Glass & Accent Glows UI/UX**: Translucent glass surfaces, dynamic border glows, sparkline scrub inspection lines, animated rotating cooling fan indicators, and pulsating Plasmoid alert glows.
- **Complete Swedish Localization**: Full `sv` translations across all v13 telemetry channels, causal diagnosis rules, and technical evidence strings.
- Multi-fan telemetry discovery and RPM progress indicators across all detected sysfs hwmon fans.
- NVMe controller discovery via `/sys/class/nvme/nvme*`, decoding critical warning bitmasks and composite temperature.
- Causal diagnosis rule `cpu.single-core-saturation` for identifying asymmetric thread bottlenecking under moderate overall CPU load.
- Causal diagnosis rules `storage.nvme-critical-warning` (Severity 3, critical hardware alert) and `storage.nvme-thermal-threshold` (Severity 2, elevated thermal risk).
- Dedicated desktop notification support for hardware-critical events (`[Event/hardwareCritical]`) via KNotification.
- Privacy-redacted kernel log correlation (`±15s` window around incidents) from `journalctl`/`dmesg` with IP, MAC, and path masking.
- Interactive side-by-side incident comparison in Flight Recorder with delta metrics calculation (peak temperature, average CPU load, worst responsiveness, culprit process).
- Three selectable panel representation styles in Plasmoid: Text Pill (0), Mini Sparkline (1), and Ring Gauge (2).
- Enriched Markdown, HTML and JSON report exports including correlated kernel events, NVMe health observations, and incident comparison deltas.

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
