# Loofi Pulse v0.18.0 — Dedicated Hardware Pages, Logical Navigation & Multi-Language Hub

This major release elevates Loofi Pulse into an intuitive, modular, and KDE Plasma 6-native hardware monitoring experience. It introduces dedicated hardware subpages, a re-architected navigation sidebar, multi-language support (English primary default with Swedish alternative), an expanded documentation hub, and high-resolution visual branding.

## Key Highlights

### 1. Dedicated Hardware Pages
- **Graphics (GPU) Page (`GpuPage.qml`)**: Real-time GPU load arc gauge, core clock MHz, boost frequency, board power (W), temperature (°C), VRAM allocation bar (used vs total MiB), and multi-GPU adapter switcher.
- **Battery Page (`BatteryPage.qml`)**: AC power state badge, charging/discharging wattage, estimated time to full/empty, battery health percentage, and unified device list for primary system batteries (`BAT0`) and wireless peripherals (Logitech HID++, Bluetooth).
- **Sensors Page (`SensorsPage.qml`)**: Active cooling fan status with animated RPM indicators, complete `hwmon` sensor domain matrix (CPU, GPU, NVMe composite, Wi-Fi, motherboard), and thermal velocity tracking.
- **Power Profiles Page (`ProfilesPage.qml`)**: Hero interactive profile cards (`Power-saver`, `Balanced`, `Performance`) hooked into `power-profiles-daemon`, active profile holds summary, and degradation causal rules.

### 2. Logical Sidebar Navigation
- Reorganized sidebar into four distinct, logical categories:
  - **SYSTEM**: Overview, Memory & Pressure, Power & Cooling
  - **HARDWARE & DEVICES**: Graphics (GPU), Battery, Sensors, Storage & Network, Processes
  - **CONTROL**: Profiles
  - **FLIGHT RECORDER**: History (Flight Recorder & timeline)
- **Pinned Settings**: Settings is now placed at the very bottom as a standalone, pinned action with its own subtle divider line above the Fedora host badge, eliminating the confusing "Diagnostics" label.

### 3. Application Language & Internationalization
- **English as Primary Default**: The application now starts in English by default across all platforms.
- **Configurable Locale in Settings**: Users can easily choose between `English (Default)`, `Svenska (Swedish)`, or `Follow System (OS Default)` under **Display & Language** in Settings.
- Dynamic runtime language switching via `PulseTelemetryController::setApplicationLanguage`.

### 4. About & Documentation Hub
- Fully restored and expanded **About & Documentation** section in `SettingsPage.qml`:
  - Live versioning and architecture summary.
  - Direct action buttons for online documentation (`README.md`), GitHub issue tracker, About dialog, and Keyboard Shortcuts cheat sheet.
  - Transparent license (GPLv3) and build information.

### 5. High-Resolution Visual Branding & Comprehensive Guides
- Brand-new SVG and rasterized banner (`assets/banner.svg` and `assets/banner.png`) featuring KDE Plasma 6 design aesthetics and live telemetry ECG motifs.
- Comprehensive user guide (`docs/user-guide.md`) detailing all 10 views, PSI pressure metrics, thermal velocity, and background daemon operations.
- Full set of 9 high-resolution live application screenshots in `docs/screenshots/`.

## Validation Status
- 84/84 CTest tests passing (100% test suite pass rate).
- Validated with `scripts/check-version.sh`, `appstreamcli validate`, and complete RPM packaging.
