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
