# Loofi Pulse

Loofi Pulse is a Linux hardware monitor for KDE Plasma 6. It includes a Plasma panel applet and a standalone Kirigami application, built with C++20 and Qt 6.

## Version 0.3.0 (unreleased)

The application and panel widget read CPU and GPU activity, temperatures, Linux Pressure Stall Information (PSI), memory and ZRAM data, power and battery information, and concurrent process activity. Missing or invalid measurements are marked **Unavailable**. Measurement details include their source, quality and sample time; calculated pressure indices and estimated package power are labelled explicitly.

- **Overview** presents the current status, explanation, next step and primary measurements. Additional sensor and data-quality details can be expanded.
- **History** keeps five minutes by elapsed time at 500, 1,000 or 2,000 ms intervals. Curves use recorded timestamps and break across missing data, pauses and sleep. Selected samples remain frozen even after they leave the rolling chart.
- **Memory & Pressure** explains time spent waiting for CPU, memory or I/O. The calculated pressure index is not a measured response time.
- **Power & Cooling** shows available power, battery, cooling and sensor data. Power profiles come from the system service; profile changes are asynchronous and verified before success is reported.

Ordinary warnings require three seconds of consistent observations, and recovery requires five seconds. New hardware throttle-counter events appear immediately. Temperature risk and storage temperature do not by themselves establish throttling or an I/O bottleneck. Process activity is shown as context, without claiming causation.

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

## Project status

Loofi Pulse targets KDE Plasma 6 on Linux. Fedora is the primary development and verification environment. Distribution packaging and clean-install/upgrade verification are planned after the 0.3.0 reliability work.
