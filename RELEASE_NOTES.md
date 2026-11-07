# Loofi Pulse v0.16.0 — UX/UI Simplification & KDE-Native Overhaul

This is a development candidate for Fedora KDE Plasma 6. It introduces a comprehensive visual redesign and UX simplification across the desktop application and Plasma widget, focusing on a clean, professional KDE-native interface without visual clutter, redundant modal dialogs, or excessive animations.

## Changes

### User Interface & Navigation

- **Sidebar Navigation (`PulseSidebar`)**: Fixed 210px navigation sidebar with 9 clearly structured destinations (`Översikt`, `Prestanda`, `Effekt & Kylning`, `Grafik (GPU)`, `Batteri`, `Sensorer`, `Historik & Logg`, `Profiler`, `Inställningar`), active section indicator capsules, and host system information badge (`Fedora Linux 45`, `KDE Plasma 6.7.5`, `Kärna: 7.2.15`, host hardware model).
- **Redesigned Overview Dashboard (`OverviewPage`)**:
  - **Real-Time Dual-Line Chart**: Interactive `Systembelastning (realtid)` time-series chart rendering CPU and GPU temperature curves with a 5-minute rolling window and clean coordinate axes.
  - **5-Metric Summary Row**: High-contrast, responsive metric tiles for CPU (`°C` and `% belastning`), GPU (`°C` and `% belastning`), Effekt (`W` and PL1/PL2 thresholds), Batteri (`%` and power/charging state), and Kylning (`Fläkthastighet`).
  - **Two-Column Hardware Layout**:
    - *Left Column*: Dedicated 4-core tiles (CPU 0–3) displaying live temperatures, load percentages, and progress bars, alongside dedicated hardware sensor gauges for CPU Package, NVMe SSD, and Wi-Fi.
    - *Right Column*: Interactive system power profile selector (`Power-saver`, `Balanced`, `Performance` with active checkmark) and detailed per-battery status list (`BAT0` and connected peripherals).
- **Header & Footer Status Bars**: Sleek application title bar and bottom status strip displaying Flight Recorder status, active view, sampling frequency, and timestamp.

### Plasma 6 Widget & Components

- **Clean Surfaces**: Refined `PulseCard` surfaces and consistent spacing using KDE Breeze design tokens.
- **True Linux PSI Telemetry**: Direct exposure of memory, CPU, and I/O pressure stall percentages (`some avg10 / full avg10`) replacing synthetic scoring.
- **Swedish Localization**: 100% complete localized Swedish translations across all application pages, hardware labels, and widget views.

## Validation Status

Automated validation verified via `scripts/validate.sh` with 83/83 tests passing (100%), including QML UI tests, accessibility focus audit, AppStream metadata validation, and secret scanning.
