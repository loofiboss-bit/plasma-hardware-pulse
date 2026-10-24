# Loofi Pulse v0.13.0 — Comprehensive Evolution

Loofi Pulse v0.13.0 delivers a major evolutionary upgrade across system power management, wireless link quality telemetry, memory compression and slice attribution, smart incident flight recording, and modern KDE 6 Glass UI/UX styling.

## Key Highlights

### 1. Power Profiles Daemon & Active Holds
- **Interactive Profile Quick-Switcher**: Direct profile switching between `power-saver`, `balanced`, and `performance` from both the app header and Plasmoid popup.
- **Active Profile Holds (`ActiveProfileHolds`)**: Queries D-Bus holds placed by desktop environments or background tools, surfacing hold reasons and preventing mystery throttling.
- **Causal Throttling Rule (`power.profile-throttling`)**: Detects when an active `power-saver` or quiet profile restricts performance during demanding workloads while connected to AC power.

### 2. Hardware Link Quality & Wireless Telemetry
- **Rich Wireless Link Metrics**: Real-time discovery of active SSID, frequency band (2.4 GHz, 5 GHz, 6 GHz), signal strength (% and dBm), and negotiated bitrate.
- **Ethernet Duplex & Carrier State**: Inspects physical network interfaces for full/half duplex mode, carrier presence, and negotiated link speeds.
- **Causal Wireless Degradation Rule (`network.wifi-link-degradation`)**: Alerts users when packet drops and aggressive rate down-negotiation correlate with weak signal or channel interference.

### 3. Memory Compression & Cgroup v2 Attribution
- **ZRAM Capacity & Savings**: Monitors `/sys/block/zram*` to report actual compressed data size, uncompressed data size, space saved (GiB), and effective compression ratio.
- **Cgroup v2 Slice Distribution**: Breaks down memory consumption across `user.slice` (desktop apps), `system.slice` (system services), and `machine.slice` (VMs / containers).
- **ZRAM Exhaustion Rule (`memory.zram-exhaustion`)**: Flags critical memory bottlenecking when ZRAM approaches device capacity and swap write stalls induce thread waiting.

### 4. Smart Flight Recorder & Interactive HTML Incident Reports
- **Automated Incident Bookmarks**: Automatically flags and bookmarks causal rule triggers with a 60-second stabilization cooldown.
- **Self-Contained Interactive HTML Reports**: Generates standalone HTML incident reports (`loofi-pulse-report-%1.html`) featuring embedded responsive SVG timeline curves, kernel log tables, and comparative delta tables.
- **SQLite v3 Schema Extensions**: Adds persistent trend storage for `batteryHealth`, `zramRatio`, and `wifiSignal`.

### 5. Modern KDE 6 Glass & Accent Glows UI/UX
- **Glass Surfaces & Accent Glows**: Translucent card surfaces, subtle borders, and dynamic glowing accents highlighting active severity alerts.
- **Interactive Sparklines**: Scrub indicator lines and data tooltips on domain cards.
- **Animated Cooling Indicators**: Rotating fan blades reflecting live RPM speeds.
- **Plasmoid Widget Redesign**: Pulsating alert glows for critical warnings, prominent 1-click incident bookmarking, and instant power profile switching.

### 6. Localization & Packaging
- **Complete Swedish Localization**: 100% translated UI, diagnostic verdict headlines, and technical causal evidence in `po/sv`.
- **Fedora 44 & Plasma 6 Packaging**: Updated AppStream metainfo and RPM spec definitions.

## Compatibility and Privacy

- `.pulse-session` format 1 and JSON report schema 2 remain fully backward-compatible. Additional telemetry is serialized under safe schema extensions.
- Kernel logs are filtered strictly to hardware subsystems and scrubbed for IP addresses, MAC addresses, and private paths.
- Background monitoring, autostart, and persistent history remain strictly opt-in.

## Validation Status

Automated validation suite passes 100% on Fedora 44 KDE. See [docs/validation-v0.13.0.md](docs/validation-v0.13.0.md) for complete verification records.
