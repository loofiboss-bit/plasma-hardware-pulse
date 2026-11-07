# Loofi Pulse v0.17.0 — UX/UI Modernization & Genuine Hardware Telemetry

This release delivers a comprehensive UX/UI modernization across the Loofi Pulse desktop application and KDE Plasma 6 widget, prioritizing genuine hardware telemetry, Breeze-native visual hierarchy, and an uncluttered system monitor experience.

## Key Changes

### Desktop Application

- **Overview Page Pillars**: Reorganized into 5 high-clarity telemetry cards: CPU, Memory, PSI Pressure, GPU, and Power & Battery.
- **Dual-Line System Activity Chart**: Real-time canvas chart tracking CPU load (%) and temperature (°C) with dynamic axes, time indicators (now, 30s, 60s), and graceful empty states.
- **Genuine Hardware Readings**: Removed all AI-generated placeholder numbers (`64°C`, `28%`, `40°C`, `17.8W`, `97%`) and dummy core arrays in favor of genuine `hwmonSensorList` and per-core telemetry with semantic `—` fallback markers.
- **Dynamic Host Detection**: Replaced static developer machine badges with dynamic detection of OS distribution, kernel version, and chassis/product name via `QSysInfo` and DMI `/sys/class/dmi/id/product_name`.
- **7-Section Navigation**: Restored full navigation sidebar covering Overview, Pressure, Power, Storage, Processes, Flight Recorder, and Settings.
- **Expandable Details**: Preserved accessible `measurementDetailsButton` and `MeasurementDetails` view.

### KDE Plasma 6 Widget

- **Compact & Focused Popup**: Removed the 200+ line raw debug log dump.
- **Consolidated Controls**: Unified single "Mark Incident" action (eliminating duplicate buttons), snooze, snapshot/copy, and KDE System Monitor launch.
- **2x2 Metrics Grid & Live Mini Sparkline**: Clear key telemetry with switchable channels (CPU, Temp, PSI, Power, Battery).
- **Interactive Power Profiles**: Direct one-click profile switching (Performance, Balanced, Power Saver) from the widget.

### Incident History & Diagnostics

- **Grouped Episode Clustering**: `groupedIncidentHistory` clusters repeated identical incidents by rule ID or headline with `[N×]` multiplier badges, aggregated duration, and latest timestamps, eliminating alert fatigue.

## Validation Status

Validated with 84/84 automated tests passing (100%), full RPM/SRPM packaging build verification, and secret scanning.
