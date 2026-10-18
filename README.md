# Loofi Pulse

Loofi Pulse is a local Linux hardware monitor and incident investigation tool for KDE Plasma 6. The C++20/Qt 6 application and Plasma widget share one user-session collector. Fedora KDE is the primary target.

## Version 0.10.0 (UI/UX Refinement)

- **Segmented Navigation & Window Frame**: Non-wrapping responsive navigation tabs with active indicator line and adaptive short labels below 980px. Polished header with status pill and theme tokens.
- **Structured Processes Table**: Replaced raw text concatenations with a column-aligned table (PROCESS, PID, USER, CPU %, RAM, STATE) featuring mini CPU load meters, semantic state badges (`D` disk wait, `R`, `S`), live process counters, and a dedicated inspector card.
- **Storage & Network Telemetry Cards**: Replaced monolithic GroupBoxes with modern cards displaying read/write speed gauges, IOPS, latency, directional network transfer meters, and drop/error counter badges.
- **Flight Recorder Incident Workflow**: Organized history controls into 3 distinct clusters (Playback, Incident Actions, Session Export) and added sleek comparative analysis tiles (Before / During / After).
- **Refined Overview & Diagnosis**: Nested causal diagnosis card with rule badge, evidence checkmarks, signal chips, and recommendation pill.
- **Streamlined Plasma 6 Widget**: Unified compact action bar, unclipped domain card titles (`CPU & SoC`, `Memory & PSI`, `GPU & Sensors`, `Power & Battery`), and vertically centered panel indicators.

## Version 0.9.0

- **Overview** connects the current diagnosis to CPU, GPU, temperature, memory pressure, storage and power details. **Why this status?** explains evidence, uncertainty and recommended next steps. High activity alone remains neutral.
- **History** retains 5, 15 or 30 minutes in memory at supported 250–10,000 ms sampling intervals. Mark a problem, select an incident or range, compare before/during/after, and freeze an investigation while live collection continues. Missing observations, warmup and recording gaps remain visible.
- **Processes** lists readable processes, including small idle processes, with search, sorting, owner, state, CPU and memory. PID plus process start time identifies a process across updates. Historical observations contain the union of the top eight CPU, memory and disk-wait processes, at most 24 rows per observation. Inaccessible or exited processes are omitted. Concurrent use does not establish causation.
- **Storage & Network** exposes individual devices, throughput, read/write IOPS, average disk request time and network error/drop counters. Physical disk aggregates exclude partitions and virtual devices. Network totals default to physical interfaces; explicitly selected virtual interfaces can overlap physical traffic.
- **Power & Cooling** presents package power, power limits, GPU telemetry, batteries, cooling and per-core data. Multiple batteries remain individually visible. Combined levels require compatible energy readings; USB/USB-C sources are recognized when Linux exposes their online state. Missing voltage is never replaced with an invented value.
- **GPU selection** binds readings to a device identity. The capability overview explains missing kernel/driver interfaces. A detected GPU can lack readable busy, temperature, frequency, power or memory measurements.
- **Local sessions** explicitly save and reopen an investigation without replacing the live recording. **Export selected incident** and the complete recording report have separate actions. Saved values retain their original diagnosis and quality metadata.
- **Plasma widget** keeps configurable panel metrics and presets, plus popup diagnostics and incident navigation into the application.

### Collection and notifications

`loofi-pulse-service` owns collection and serves the application and widgets through the versioned user-session D-Bus name/interface `com.github.loofi.hardwarepulse.Collector1`, at `/com/github/loofi/hardwarepulse/Collector1`. D-Bus activates it on demand. Collection runs away from the UI thread with one outstanding job; clients request their own sampling and retention settings. The fastest interval and longest requested retention determine shared collection.

**Background monitoring** is off by default. **Start at login** is a separate opt-in setting; installing the package does not enable either setting. With no clients and background monitoring disabled, the collector exits after 30 seconds. Ordinary rolling recordings are memory-only and disappear when the collector exits. Save an investigation explicitly to retain it.

Incident notifications are coordinated by the collector, with an open action, critical escalation and a 15-minute snooze. Notifications and background monitoring have separate controls. Service disconnection, warmup, pause and stale data are distinct states; a disconnected client must not treat old values as current readings.

### Sessions, reports and privacy

`.pulse-session` format version 1 preserves original samples, device identities, quality, gaps, markers and diagnosis engine version. Files are validated before opening: the supported format, ordered timestamps and valid references are required, with limits of **64 MiB** and **7,201 telemetry points**. An invalid file cannot replace the current investigation. Saves are atomic; a write failure is reported to the user.

Reports retain JSON schema version 2 and existing fields alongside additive metadata. Session and report exports exclude process names, PIDs, start-time identities and owners by default. Enable **Include process names and IDs in reports** only when those details are appropriate to share. Live process inspection is local and separate from export privacy. Review any saved session or report before sharing: system and device details remain present even with process redaction.

The app offers to save an unsaved investigation when explicitly quitting monitoring. Successful report files remain available if another file fails to export.

### Measurement semantics

- **Responsiveness index** is a calculated 0–100 score, not measured latency. Raw PSI measures time spent waiting; higher pressure means more waiting.
- **CPU package power** and **Battery discharge** are separate measurements. Estimated package power is labelled. The legacy JSON `systemWatts` field does not establish measured whole-system power.
- **Average disk request time** is derived from completed read/write operations and their accumulated request milliseconds in `/proc/diskstats`; it is not application response time or a latency percentile. An interval without completed requests has no average.
- Per-device baselines reset after disappearance, replacement, counter rollback or suspend. The first new reading warms up instead of producing a rate from lifetime counters.
- Ordinary diagnosis warnings require sustained evidence; new hardware throttle-counter events are immediate. Heat alone does not establish active throttling, and storage temperature alone does not establish an I/O bottleneck.
- Missing, malformed or unreadable measurements are unavailable. Pulse does not request additional privileges to fabricate complete sensor coverage.

### Navigation

Existing page identifiers, configuration keys and shortcuts remain supported. `Ctrl+1..4` / `Alt+1..4` navigate existing pages; `Ctrl+P` pauses/resumes, `Ctrl+S` exports a report, and `Ctrl+Shift+C` copies it. `Ctrl+O` opens a session and `Ctrl+Shift+S` saves one.

```sh
loofi-pulse --page power --section gpu
loofi-pulse --page pressure --section storage
loofi-pulse --session /path/to/investigation.pulse-session
loofi-pulse --incident INCIDENT_ID
```

An incident identifier refers to the shared collector's recording. Save a session to preserve it across collector exit. Process termination and system administration remain outside the investigation workflow; **Open System Monitor** can open KDE's process view.

## Build and verify

Install the development dependencies listed in `packaging/plasma-hardware-pulse.spec`, then run:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_LIVE_HARDWARE_TESTS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
qmllint --ignore-settings -i build/bin/com/github/loofi/hardwarepulse/qmldir -I build/bin src/app/qml/*.qml package/contents/ui/*.qml
appstreamcli validate --no-net packaging/com.github.loofi.hardwarepulse.metainfo.xml
```

The normal test suite uses deterministic filesystem fixtures, session validation and isolated D-Bus/UI tests. Live-host telemetry sanity checks require `-DBUILD_LIVE_HARDWARE_TESTS=ON` and suitable readable interfaces. The complete application requires a session D-Bus; build-tree service tests should use an isolated bus to avoid interacting with an installed collector.

For a sustained 35-minute UI/service workload with 1,000 simulated processes, run the following after building. It samples every 250 ms, fills 30 minutes of history, then opens the production History page. The test reports five-minute UI-delay percentiles and UI/collector CPU and RSS; the normal CTest run keeps a five-second UI workload and a separate ten-second full-history regression. The latter prefills 7,201 synthetic points with peaks and gaps, rolls the window every 250 ms, and verifies bounded plot projection and UI latency. It does not replace the wall-clock retention test.

```sh
mkdir -p build/soak-state/{config,data,cache}
PULSE_SOAK_MS=2100000 QTEST_FUNCTION_TIMEOUT=2400000 \
QT_QPA_PLATFORM=offscreen QML_IMPORT_PATH="$PWD/build/bin" \
XDG_DATA_DIRS="$PWD/build/share:/usr/local/share:/usr/share" \
XDG_CONFIG_HOME="$PWD/build/soak-state/config" \
XDG_DATA_HOME="$PWD/build/soak-state/data" \
XDG_CACHE_HOME="$PWD/build/soak-state/cache" \
dbus-run-session -- ./build/tests/test_collector_v09 actualQmlProcessPageResponsiveness
```

## Packaging and qualification

The Fedora spec builds RPM/SRPM packages containing the application, collector, D-Bus activation file, QML plugin and widget. `just package-plasmoid` creates a widget bundle; the widget also requires the matching installed native plugin and collector. A widget ZIP alone does not provide those binaries.

CI builds and runs CTest, QML and AppStream checks, checks staged application/collector/activation artifacts, and builds RPM/SRPM packages. A successful source or staged build is distinct from an installed-package upgrade or real desktop test.

**Qualification boundary:** deterministic fixtures cover multiple GPUs and batteries, USB-PD, virtual devices, unavailable readings and counter resets. They do not qualify real Intel/AMD/NVIDIA drivers, physical hotplug, USB-C charging, suspend/resume, Plasma/Wayland desktop interaction, screen readers or upgrade behavior on an ordinary user system. Those combinations must be recorded separately when physically exercised. A performance acceptance target is also not a measured result; record the exact workload, CPU/RSS and UI-delay results before claiming it passed.

`just install-local` modifies the user's desktop installation and configuration; run it only when intentionally installing. Development and CI checks do not publish a release or install the package on the ordinary user system. See [ROADMAP.md](ROADMAP.md) for the remaining release qualification work and [SECURITY.md](SECURITY.md) for privacy and vulnerability reporting.
