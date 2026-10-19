# Loofi Pulse v0.11.0

Loofi Pulse v0.11.0 is the major "daily-driver" upgrade for Linux performance monitoring on Fedora 44 / KDE Plasma 6. This release hardens system lifecycle resilience, introduces a unified preferences hub, adds long-term historical metrics with privacy guarantees, provides system tray integration and safe process control, and brings full native Swedish localization.

## Highlights in v0.11.0

- **Single-Instance Application & D-Bus Activation**: Seamlessly brings existing windows forward using `KDBusService::Unique` when launched again, parsing secondary command line arguments (`--page`, `--section`, `--file`) and routing focus dynamically.
- **Collector Reconnection with Exponential Backoff**: Eliminates permanent disconnect failures. If `loofi-pulse-service` exits or restarts, clients automatically enter an exponential backoff reconnect loop (1s to 30s) with jitter.
- **Systemd-Logind Sleep & Resume Protection**: Listens to systemd-logind `PrepareForSleep` D-Bus signals. On wakeup, rate baselines are cleanly re-initialized and explicit telemetry gap markers prevent invalid spikes from sleep intervals.
- **Preferences & Settings Hub (`SettingsPage.qml`)**:
  - Unified configuration for telemetry sampling intervals (250 ms to 5 s) and in-memory retention (1 to 30 min).
  - Background monitoring and login autostart toggles.
  - Alert notification controls: severity thresholds (Warnings, Critical, All, Muted), quiet hours scheduler (HH:mm to HH:mm), and per-domain alert muting.
  - Temperature unit preference (°C vs. °F) affecting all labels and thresholds across the application and widget.
  - Standard KDE About and Keyboard Shortcuts dialogs.
- **Persistent Long-Term History & Trends (`TrendsView.qml`)**:
  - Opt-in SQLite database using write-ahead logging (WAL) storing 1-minute numerical performance aggregates (min, avg, max).
  - Configurable retention periods (1, 7, 14, 30 days) with automatic vacuuming and storage metrics.
  - **Zero Process Identity Guarantee**: Process names, command lines, window titles, and user PIDs are NEVER persisted or stored.
  - Interactive multi-range trends oscilloscope (1h, 6h, 24h, 7d, 30d).
- **System Tray & Process Management**:
  - Status-adaptive system tray icon (`loofi-pulse-status-nominal`, `warning`, `critical`) with rich multi-line telemetry tooltips.
  - System tray menu with quick incident marking, 15-minute notification snoozing, pause/resume, and quit actions.
  - Optional minimize-to-tray on window close with first-run explanatory notification balloon.
  - Safe process termination from the process table: gated by UID check (`getuid()`), session/desktop blacklist (`systemd`, `kwin`, `plasmashell`, `pipewire`), and `/proc/<pid>/stat` start-time recycling prevention.
  - On-demand per-process disk I/O metrics (Read / Write MiB from `/proc/<pid>/io`).
- **Internationalization (ki18n)**:
  - Full native Swedish localization (`po/sv/*.po`) across application and applet domains.
  - Standard `Messages.sh` catalog extraction script.
  - Localized `.desktop` and AppStream metadata.
- **Accessibility & Polish**:
  - Clean Tab keyboard navigation removing `activeFocusOnTab` from static containers.
  - Responsive `FontMetrics` column sizing in the process table.
  - Collapsible hardware cards for missing battery/GPU setups.
  - Automated accessibility audit test in `test_qml_ui`.

## Qualification & Verification

- **100% Passing Tests**: All 65 automated tests in `ctest` passed cleanly.
- **Zero Lint Errors**: 0 warnings or errors in `qmllint` across all QML files.
- **Valid AppStream Metadata**: Validated with `appstreamcli validate --no-net`.
- **Packaging**: Updated RPM spec file, AppStream metainfo, and packaging metadata for version 0.11.0.

## Install / Upgrade on Fedora

Download the x86_64 RPM and SHA256SUMS, verify its checksum, then install with:
```bash
sudo dnf install ./plasma-hardware-pulse-0.11.0-1.fc44.x86_64.rpm
```
Restart any running `loofi-pulse` or `loofi-pulse-service` process and reload the Plasma widget.

# Loofi Pulse v0.10.0

Loofi Pulse v0.10.0 brings a comprehensive UI/UX overhaul and visual refinement to both the desktop application and the KDE Plasma 6 widget. The interface transitions from plain lists and wrapping toolbars into a modern, column-aligned, structured, and easy-to-read performance studio.

## Highlights in v0.10.0

- **Design System & Tokens**: Introduced modern `PulseTheme` tokens including nested surfaces (`cardNestedBg`), table headers (`tableHeaderBg`), row-hover states (`tableRowHoverBg`), and standard card radius (10px) / inner radius (7px).
- **Custom PulseButton Component**: Replaced stark unstyled square buttons across all pages with custom-styled, theme-aware, rounded `PulseButton` items featuring hover feedback, 3D top-edge highlights, and semantic primary accents.
- **Segmented Top Navigation**: Modern macOS/Plasma 6-grade segmented tab bar inside a recessed container track, featuring an active bottom accent indicator line and adaptive short labels on narrow viewports. Redundant page header title strips removed for a sleek and compact layout.
- **Processes Inspector Table**: Completely transformed from raw text concatenations into a structured, column-aligned process table (`PROCESS`, `PID`, `USER`, `CPU %`, `RESIDENT RAM`, `STATE`) with visual mini CPU load bars, semantic state badges (`D` disk wait in high-contrast red/amber), active process count indicators, and a dedicated inspector card with a direct System Monitor shortcut.
- **Storage & Network Cards**: Replaced monolithic GroupBoxes with modern per-device cards for disks (Read/Write gauges, IOPS, latency) and network interfaces (directional transfer rates `↓ Receive` / `↑ Send`, error/drop badges), plus a clean 2-column capability matrix.
- **Flight Recorder Incident Workflow**: Clustered history controls into three clear functional groups (Playback, Incident Actions, Sessions & Export), enhanced timeline interaction, and sleek Before/During/After comparative analysis tiles.
- **Overview & Diagnostics**: Redesigned "Why this status?" into an interactive inspector card with rule badge, evidence checkmarks, signal chips, and direct recommendation pills.
- **Plasma 6 Widget Refinement**: Unified two-row compact action bar, unclipped domain card titles (`CPU & SoC`, `Memory & PSI`, `GPU & Sensors`, `Power & Battery`), and vertically aligned panel status indicators.

## Qualification & Verification

- **100% Passing Tests**: All 52 automated tests in `ctest` passed cleanly.
- **Backward Compatibility**: Preserved all configuration keys, D-Bus interfaces, and `.pulse-session` format compatibility.
- **Packaging**: Updated RPM spec file, AppStream metainfo, and packaging metadata for version 0.10.0.

## Install / Upgrade on Fedora

Download the x86_64 RPM and SHA256SUMS, verify its checksum, then install with:
```bash
sudo dnf install ./plasma-hardware-pulse-0.10.0-1.fc44.x86_64.rpm
```
Restart any running `loofi-pulse` or `loofi-pulse-service` process and reload the Plasma widget.
