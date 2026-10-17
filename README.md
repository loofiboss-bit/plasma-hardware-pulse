# Loofi Pulse

Loofi Pulse is a Linux hardware monitor for KDE Plasma 6. It includes a Plasma panel applet and a standalone Kirigami application, built with C++20 and Qt 6.

## Version 0.8.0

The application and panel widget read CPU and GPU activity, temperatures, Linux Pressure Stall Information (PSI), memory and ZRAM data, block storage and network throughput, power and battery information, and concurrent process activity (including uninterruptible disk-wait `D`-state detection). Missing or invalid measurements are marked **Unavailable**. Measurement details include their source, quality and sample time; calculated pressure indices and estimated package power are labelled explicitly.

- **Overview** presents a persistent telemetry header bar, hero causal diagnosis banner with secondary multi-domain contributing signal pills and a circular PSI responsiveness ring gauge, and six fully clickable primary domain cards (**CPU load**, **CPU temperature**, **Graphics (GPU)**, **Memory pressure**, **Storage & I/O**, and **CPU package power**) with embedded live mini-sparklines that navigate directly to their corresponding detail pages. Additional sensor and data-quality details expand into a structured two-column status matrix.
- **History** keeps a configurable **5, 15, or 30-minute** retention window at 500, 1,000 or 2,000 ms intervals using incremental ring-buffer updates, on-demand metadata synthesis, and a **Clear History** reset action. An oscilloscope-grade chart with dynamic Y/X-axis gutters, interactive channel toggle chips (**CPU load**, **CPU temp**, **GPU load**, **Responsiveness index**, **CPU package power**, and **Disk I/O**), gradient-filled traces, and a 6-card Snapshot Inspector HUD covers live and historical telemetry alongside clickable **Recorded incidents** episode chips with incident durations.
- **Memory & Pressure** combines a circular PSI gauge with dual-bar (`some`/`full`) CPU, memory, and storage stall cards, a visual **RAM → ZRAM → Swap** allocation pipeline, and an interactive concurrent process radar with sort modes (**Combined**, **CPU %**, **Memory RSS**, and **Disk Wait**) plus explicit `DISK WAIT (D)` badges when processes stall in uninterruptible sleep.
- **Power & Cooling** organizes power and thermal telemetry into four accented domain cards (**CPU package & power limits**, **Graphics (GPU)**, **Battery & power source**, and **Cooling & device thermal**), an icon-equipped power profile switcher, a per-core load/thermal heatmap grid with **Sort by load**, and a two-column detected sensor matrix with thermal headroom bars.
- **Plasma 6 Panel Widget** renders a configurable telemetry pill in the panel (with optional toggles for **CPU temp**, **CPU %**, **GPU load/temp**, **RAM %**, **Responsiveness index**, **Disk/Network I/O**, and **CPU package power**) and provides an interactive 2x2 domain card grid plus a channel-switchable (**Temp**, **CPU**, **PSI**, **Power**) sparkline in its popup.

Ordinary warnings require three seconds of consistent observations, and recovery requires five seconds. New hardware throttle-counter events appear immediately. The causal diagnosis engine evaluates CPU/GPU thermals, PSI bottlenecks (including uninterruptible `D`-state disk wait context), power-saver profile capping under heavy load, sustained PL1 power-limit capping, and high battery discharge rates, while surfacing secondary concurrent stress signals in `contributingSignals`. Stable rule identities prevent unrelated diagnoses of the same severity from flickering between domains. **Why this status?** explains observed evidence, duration, missing measurements and recommended next steps. Temperature risk and storage temperature do not by themselves establish throttling or an I/O bottleneck.

Markdown and versioned JSON reports (schema version 2, with additive diagnostic and source metadata) include recorded values, network and disk throughput, data quality, contributing signals, and the covered period. Process names, IDs and start-time identities are excluded by default; enable **Include process names and IDs in reports** in Settings to include them. Each file is saved atomically, export errors identify the affected file, and successful exports offer an **Open folder** action. **Open System Monitor** opens KDE's process view for further investigation.

The standalone app remembers its polling interval, history retention window, notification preference, network interface selection, report privacy preference and selected chart curves, and supports keyboard shortcuts (`Ctrl+1..4` / `Alt+1..4` for page navigation, `Ctrl+P` for pause/resume, `Ctrl+S` to save a report, `Ctrl+Shift+C` to copy a report). The widget keeps its separate Plasma settings. Layouts follow the KDE theme and support keyboard navigation. Hardware support depends on the interfaces exposed by the running Linux kernel; unavailable sensors require no additional permissions.


### Understanding the measurements

- **Responsiveness index** is a calculated 0–100 score; higher is better. It is neither a measured response time nor raw PSI. Raw CPU, memory and storage PSI values describe time spent waiting; higher values indicate more pressure.
- **CPU package power** and **Battery discharge** have separate history channels. Package power is not whole-system power. Estimated package values are labelled. The legacy JSON `systemWatts` field is preserved for compatibility and must not be interpreted as measured whole-system power.
- Activity uses neutral channel colours. Warnings require observed pressure, thermal risk or throttle evidence; high CPU/GPU use and throughput alone are not failures.
- **Measurement details** shows each metric's source, sample time, quality and missing-data reason. Process rows expand to show the PID/start-time identity and last observation time; concurrent resource use does not establish causation.
- Disk and network rates use per-device baselines. Hotplug, disappearance and counter resets warm up independently. Network totals default to physical interfaces; Settings can explicitly select virtual interfaces, which may overlap physical traffic.
- History retains the configured 5/15/30-minute window across supported 250–10,000 ms sampling intervals. The view shows the actually recorded duration separately and preserves gaps during pause, suspend and missing readings.
- Settings groups sampling, retention, notifications, network selection and report privacy. Panel presets **Minimal**, **Balanced** and **Detailed** have bounded width; existing per-metric choices remain available as **Custom**.

Open a specific section with `loofi-pulse --page power --section gpu`, or `--page pressure --section storage`. Existing page names remain supported; unknown sections open the top of the page.

See [ROADMAP.md](ROADMAP.md) for the planned local incident sessions, additional hardware views and 1.0 qualification work. These later features are not part of v0.8.

## Build

On Fedora, install the KDE Frameworks, Plasma, Qt, and CMake development packages required by the project, then run:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

Run portable tests with:

```sh
ctest --test-dir build --output-on-failure
```

QML and metadata checks can also be run directly:

```sh
qmllint --ignore-settings -i build/bin/com/github/loofi/hardwarepulse/qmldir -I build/bin src/app/qml/*.qml package/contents/ui/*.qml
appstreamcli validate --no-net packaging/com.github.loofi.hardwarepulse.metainfo.xml
```

The live host telemetry sanity check is excluded by default because it depends on the sensors and interfaces of the machine running it. Enable it explicitly with `-DBUILD_LIVE_HARDWARE_TESTS=ON` when testing on a suitable Linux host.

To launch the standalone application from the build tree:

```sh
QML_IMPORT_PATH="$PWD/build/bin" ./build/bin/loofi-pulse
```

## Packaging & Installation

- **Local install**: `just install-local` installs into `~/.local` and upgrades/installs the Plasma 6 applet package.
- **Plasmoid bundle**: `just package-plasmoid` generates `build/dist/com.github.loofi.hardwarepulse.plasmoid`.
- **Fedora RPM**: `packaging/plasma-hardware-pulse.spec` provides the RPM build specification for Fedora KDE Plasma 6.

