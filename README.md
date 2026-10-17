# Loofi Pulse

Loofi Pulse is a Linux hardware monitor for KDE Plasma 6. It includes a Plasma panel applet and a standalone Kirigami application, built with C++20 and Qt 6.

## Version 0.4.0

The application and panel widget read CPU and GPU activity, temperatures, Linux Pressure Stall Information (PSI), memory and ZRAM data, block storage throughput, power and battery information, and concurrent process activity. Missing or invalid measurements are marked **Unavailable**. Measurement details include their source, quality and sample time; calculated pressure indices and estimated package power are labelled explicitly.

- **Overview** presents the current status with severity indicators, explanation, next step and primary measurements with visual progress bars. Additional sensor and data-quality details can be expanded.
- **History** keeps five minutes by elapsed time at 500, 1,000 or 2,000 ms intervals using incremental ring-buffer updates and on-demand metadata synthesis. Curves use recorded timestamps, color-coded legends, Y-axis scale labels, and hover scrub inspection, breaking cleanly across missing data, pauses and sleep. Selected samples remain frozen even after they leave the rolling chart.
- **Memory & Pressure** explains time spent waiting for CPU, memory or I/O alongside real-time `/proc/diskstats` read/write throughput (MiB/s). The calculated pressure index is not a measured response time.
- **Power & Cooling** shows available power (via RAPL, `hwmon` CPU power sensors such as `k10temp`/`zenpower`/`amd_energy`/`coretemp`, or battery discharge), battery health, cooling, Intel `i915`/`xe` and AMD GPU frequencies, and a responsive multi-column per-core load and temperature grid. Power profiles come from the system service; profile changes are asynchronous and verified before success is reported.

Ordinary warnings require three seconds of consistent observations, and recovery requires five seconds. New hardware throttle-counter events appear immediately. Temperature risk and storage temperature do not by themselves establish throttling or an I/O bottleneck. Process activity is scanned adaptively using low-allocation `/proc` iteration and shown as context, without claiming causation.

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

