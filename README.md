# Loofi Pulse

Loofi Pulse is a Linux hardware monitor for KDE Plasma 6. It includes a Plasma panel applet and a standalone Kirigami application, built with C++20 and Qt 6.

## Version 0.6.0

The application and panel widget read CPU and GPU activity, temperatures, Linux Pressure Stall Information (PSI), memory and ZRAM data, block storage throughput, power and battery information, and concurrent process activity. Missing or invalid measurements are marked **Unavailable**. Measurement details include their source, quality and sample time; calculated pressure indices and estimated package power are labelled explicitly.

- **Overview** presents a persistent telemetry header bar, hero causal diagnosis banner with a circular PSI responsiveness ring gauge, and six fully clickable primary domain cards (**CPU load**, **CPU temperature**, **Graphics (GPU)**, **Memory pressure**, **Storage & disk I/O**, and **Package power**) with embedded live mini-sparklines that navigate directly to their corresponding detail pages. Additional sensor and data-quality details expand into a structured two-column status matrix.
- **History** keeps five minutes by elapsed time at 500, 1,000 or 2,000 ms intervals using incremental ring-buffer updates and on-demand metadata synthesis. An oscilloscope-grade chart with dedicated Y/X-axis gutters, interactive channel toggle chips, gradient-filled traces, and a 6-card Snapshot Inspector HUD covers CPU load, CPU temperature, GPU load, pressure index, and package power alongside clickable **Recorded incidents** episode chips.
- **Memory & Pressure** combines a circular PSI gauge with dual-bar (`some`/`full`) CPU, memory, and storage stall cards, a visual **RAM → ZRAM → Swap** allocation pipeline, and a dual-bar (CPU % + Memory RSS) concurrent process radar.
- **Power & Cooling** organizes power and thermal telemetry into four accented domain cards (**CPU package & power limits**, **Graphics (GPU)**, **Battery & power source**, and **Cooling & device thermal**), an icon-equipped power profile switcher, a per-core load/thermal heatmap grid with **Sort by load**, and a two-column detected sensor matrix with thermal headroom bars.
- **Plasma 6 Panel Widget** renders a tightly grouped telemetry pill in the panel (without wide-panel stretching gaps) and provides an interactive 2x2 domain card grid plus a collision-free 5-minute temperature sparkline in its popup.

Ordinary warnings require three seconds of consistent observations, and recovery requires five seconds. New hardware throttle-counter events appear immediately. The causal diagnosis engine evaluates CPU/GPU thermals, PSI bottlenecks, power-saver profile capping under heavy load, sustained PL1 power-limit capping, and high battery discharge rates. Temperature risk and storage temperature do not by themselves establish throttling or an I/O bottleneck. Process activity is scanned adaptively using low-allocation `/proc` iteration and shown as context, without claiming causation.

Markdown and versioned JSON reports include recorded values, data quality and the covered period. Each file is saved atomically, export errors identify the affected file, and successful exports offer an **Open folder** action. **Open System Monitor** opens KDE's process view for further investigation.

The standalone app remembers its polling interval, notification preference and selected chart curves. The widget keeps its separate Plasma settings. Layouts follow the KDE theme and support keyboard navigation. Hardware support depends on the interfaces exposed by the running Linux kernel; unavailable sensors require no additional permissions.

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

