# Loofi Pulse v0.9.0

Loofi Pulse now supports a complete local performance investigation: detect a problem, inspect its evidence, compare before/during/after, save the recording and reopen it later.

## Changes

- One D-Bus-activated user collector shared by the app and Plasma widgets, with asynchronous collection and explicit connection/quality states.
- Stable incidents, manual problem markers, frozen investigations, contextual comparisons, centralized notification deduplication and snooze.
- Atomic `.pulse-session` v1 save/open with bounded validation. Process identities remain excluded from saved output by default; background monitoring and login startup are separate opt-ins.
- Dedicated Processes and Storage & Network pages, device selection, multiple GPU/battery inventories and capability explanations.
- Pixel-bounded history plots that retain peaks and gaps, an incremental numeric cache and threaded rasterization.
- Existing configuration, navigation and JSON report schema 2 remain compatible. Added `--session`, `--incident`, Ctrl+O and Ctrl+Shift+S.

## Validation and limits

Local Fedora 44 validation passed 52/52 CTest cases, QML checks, AppStream, RPM/SRPM builds and disposable-container clean installation and upgrade from v0.8.0. A 35-minute offscreen UI/service workload used 1,000 simulated processes at 250 ms with 30-minute history: overall event-loop p95 0.8 ms and final full-History window p95 0.5 ms. The extreme-value full-history regression measured p95 66.0 ms and high synthetic rendering CPU usage.

Small residual RSS growth remained after the full-history warmup (about 1.5 MB UI and 1.4 MB collector over four minutes); literal zero RSS growth is not established. The three-window core workload retained 7,201 points with bounded steady-state allocation. See [validation details](docs/validation-v0.9.0.md).

Physical Plasma/Wayland, screen-reader, suspend/hotplug, GPU-driver and multiple-battery/USB-C combinations require separate qualification. Offscreen timing is not physical input/compositor latency. RPMs are unsigned; verify SHA256SUMS before installation.

## Install on Fedora

Download the x86_64 RPM and SHA256SUMS, verify its checksum, then install with `sudo dnf install ./plasma-hardware-pulse-0.9.0-1.fc44.x86_64.rpm`. Preserve settings and saved sessions. Older manual user-local app, QML or widget copies can override the packaged version; move those specific copies to a backup before switching to the system package. Restart an already-running Pulse app/collector and reload an existing widget to use updated code.
