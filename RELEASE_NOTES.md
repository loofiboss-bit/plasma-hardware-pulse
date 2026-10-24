# Loofi Pulse v0.13.0 — Telemetry, Diagnostics & UX Enhancements

Loofi Pulse v0.13.0 expands hardware telemetry, causal root-cause diagnostics, and post-incident investigation capabilities while maintaining strict privacy boundaries and lightweight KDE Plasma 6 integration.

## Key Highlights

### 1. Hardware Telemetry & Thermal Discovery
- **Multi-Fan Discovery & RPM Indicators**: Discovers all active cooling fans via sysfs `hwmon` with labeled RPM progress indicators and speed ceilings in Power & Thermal.
- **NVMe Controller Health**: Detects NVMe controllers via `/sys/class/nvme/nvme*`, decoding the 8-bit `critical_warning` bitmask (available spare, reliability degradation, read-only mode, backup device) and composite temperatures.

### 2. Causal Root-Cause Diagnosis & Notifications
- **Single-Core Saturation Verdict (`cpu.single-core-saturation`)**: Identifies asymmetric single-thread bottlenecking when total CPU load appears moderate but a single core is pegged at ≥95% alongside elevated PSI pressure.
- **Differentiated NVMe Critical Warnings**: Hardware faults (low spare, degraded reliability, read-only state) trigger Severity 3 (`CriticalThrottle`) and emit immediate desktop notifications (`KNotification` event `hardwareCritical`). Pure temperature threshold exceedance is classified as elevated thermal risk (Severity 2).

### 3. Investigation & Flight Recorder Tools
- **Privacy-Redacted Kernel Log Correlation**: Correlates dmesg/journald hardware events within `±15s` of an incident. Automatically masks IP addresses, MAC addresses, and private user home paths.
- **Side-by-Side Incident Comparison**: Directly compares two recorded incidents in Flight Recorder, computing deltas for peak temperatures, CPU load averages, worst responsiveness scores, and top culprit processes.
- **Enriched Report Exports**: Markdown and JSON report exports include correlated kernel events, NVMe storage health, and incident comparison deltas while preserving schema 2 and format 1 compatibility.

### 4. Plasmoid & UI Visual Refinements
- **Three Panel Display Styles**: Users can choose between Text Pill (0), Mini Sparkline (1), and Ring Gauge (2) in the Plasma 6 panel.
- **Overview Card Improvements**: Dynamic gradient sparklines and direct action buttons on domain cards.

## Compatibility and Privacy

- `.pulse-session` format 1 and JSON report schema 2 remain fully backward-compatible. Additional data is structured under `metadata` and non-breaking sample fields.
- Kernel logs are filtered strictly to hardware subsystems (thermal, CPU, GPU, NVMe, memory, power) and scrubbed for personal identifiers before presentation or export.
- Background monitoring, login startup, and persistent history remain explicit opt-ins.

## Validation Status

Automated validation suite passes 100% on Fedora 44 KDE. See [docs/validation-v0.13.0.md](docs/validation-v0.13.0.md) for complete verification records.
