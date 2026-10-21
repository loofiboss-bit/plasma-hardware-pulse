# Loofi Pulse

Loofi Pulse is a local hardware monitor and incident investigation tool for KDE Plasma 6. The Qt 6/Kirigami desktop app and Plasma widget share a user-session collector. **Fedora 44 KDE is the primary development target.**

**v0.12.0 is a development prerelease.** Automated tests, package builds, and physical desktop qualification are separate evidence. See [the validation record](docs/validation-v0.12.0.md) for current results and pending checks; this README does not certify every GPU, battery, driver, or Plasma setup.

## What it does

- Explains current CPU, GPU, memory and I/O pressure, thermal risk, and power readings with evidence and unavailable-data states.
- Keeps 5, 15 or 30 minutes of in-memory history at 250–10,000 ms intervals. Mark incidents, select a range, and compare before/during/after.
- Inspects readable processes and individual storage, network, GPU and battery devices. **Process inspection is read-only.** Open KDE System Monitor for process management.
- Saves and opens local `.pulse-session` investigations, and exports JSON/Markdown reports. Process identities are excluded from saved output by default.
- Offers separate opt-ins for background monitoring, start at login, and persistent numerical history. The widget provides configurable panel metrics and incident navigation.

Pulse reads interfaces exposed by Linux and the active drivers. A detected device can have no readable measurements. It does not request elevated privileges for routine collection. High activity alone is not evidence of throttling or poor responsiveness; concurrent process use does not establish causation.

## Install on Fedora KDE

A complete installation needs the application, `loofi-pulse-service`, its session D-Bus activation file, the native QML plugin, and the widget. A `.plasmoid` ZIP alone **does not contain the native plugin or collector** and cannot provide a complete installation.

From [the v0.12.0 development prerelease](https://github.com/loofiboss-bit/plasma-hardware-pulse/releases/tag/v0.12.0), download the matching Fedora/architecture RPM and `SHA256SUMS` into the same directory. Verify the downloaded files before installing:

```sh
sha256sum --check --ignore-missing SHA256SUMS
sudo dnf install ./plasma-hardware-pulse-0.12.0-1.fc44.x86_64.rpm
loofi-pulse
```

Do not assume an RPM exists until the v0.12.0 release page lists it. GitHub’s `/releases/latest` may still point to an older stable-marked development build; use the explicit v0.12.0 tag or select the intended prerelease from the complete releases list. Dependency resolution requires the appropriate Fedora repositories. After upgrading, close Pulse and remove/re-add an already running widget as needed to load the new native plugin. Enable background monitoring or login startup only when wanted; installation does not enable them. Installing an RPM changes the local system and is separate from building or testing source.

## Build and verify

Requirements are C++20, CMake 3.24+, Qt 6, KDE Frameworks 6.21+ (typed KI18n QML API)/Plasma 6, and a session D-Bus. The complete Fedora dependency list is in [the spec](packaging/plasma-hardware-pulse.spec); CI additionally installs test, lint and packaging tools with [its Fedora 44 dependency script](scripts/install-ci-dependencies.sh). That script installs packages as root and is intended for disposable CI containers.

With dependencies installed, run the shared validation entry point:

```sh
bash scripts/validate.sh
```

It builds in `build/validation`, runs deterministic and isolated D-Bus/QML tests, checks QML/AppStream/version metadata, and installs into a temporary staging directory. It does not install on the normal desktop or publish anything. Override `PULSE_BUILD_DIR` and `PULSE_BUILD_JOBS` when needed.

For an iterative build:

```sh
cmake -S . -B build/dev -DBUILD_TESTING=ON -DBUILD_LIVE_HARDWARE_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build/dev --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build/dev --output-on-failure
```

A build-tree app still requires a working collector activation path. Use the isolated service/UI tests for development; do not point an ordinary user session at fixture services. `just install-local` deliberately changes the user's installation and Plasma environment configuration; read the recipe before using it.

## Collection, saved data and privacy

The collector uses user-session D-Bus service/interface `com.github.loofi.hardwarepulse.Collector1` and object `/com/github/loofi/hardwarepulse/Collector1`. Clients share the fastest requested interval and longest requested retention. The collector runs with the user's existing permissions; the session bus is not a security boundary against other applications running as the same user.

Rolling recordings stay in memory. With no clients and background monitoring disabled, the collector exits after an idle delay. **Save an investigation explicitly to retain it.** Persistent long-term history is a separate opt-in SQLite store of numerical minute aggregates. It excludes process identities; explicit session/report exports can include them when enabled. Review saved files before sharing: device identities, hardware details and timestamps remain potentially identifying.

Session format 1 is bounded to 64 MiB and 7,201 telemetry points. Invalid input must not replace the current investigation. Reports retain JSON schema 2. Writes use atomic saves, and failures are reported. Persistent history is bounded and its storage/error state is visible; corruption recovery preserves the old database for inspection rather than presenting it as healthy history.

## Measurement semantics

- The responsiveness index is a calculated score from Linux PSI, **not measured latency**. Higher raw PSI means more time waiting.
- Package power and battery discharge are separate. Estimates are labelled; the legacy JSON `systemWatts` field does not establish measured whole-system power.
- GPU readings retain device identity and the selected temperature sensor. System diagnosis considers available GPU devices; a display selection does not limit that assessment. Temperature alone does not establish a GPU bottleneck.
- Average disk request time derives from completed operations and accumulated request milliseconds. It is not application latency or a percentile; an interval with no completed requests has no average.
- Warmup, missing/malformed data, disconnects, stale readings and recording gaps are distinct from measured zero. Rates reset after disappearance, counter rollback or suspend.
- Ordinary diagnosis warnings require sustained evidence; new hardware throttle-counter events are immediate. CPU heat alone does not confirm active throttling, and storage heat alone does not confirm I/O pressure.

## Navigation and investigations

```sh
loofi-pulse --page power --section gpu
loofi-pulse --page pressure --section storage
loofi-pulse --session /path/to/investigation.pulse-session
loofi-pulse --incident INCIDENT_ID
```

An incident ID refers to the current shared recording; save a session to preserve it across collector exit. `Ctrl+1..4` / `Alt+1..4` navigate the main pages; `Ctrl+P` pauses/resumes, `Ctrl+S` exports, `Ctrl+Shift+C` copies a report, `Ctrl+O` opens a session, and `Ctrl+Shift+S` saves one.

## Contribute and qualify

Start with [CONTRIBUTING.md](CONTRIBUTING.md), [the roadmap](ROADMAP.md), and [SECURITY.md](SECURITY.md). [CHANGELOG.md](CHANGELOG.md) records changes; [RELEASE_NOTES.md](RELEASE_NOTES.md) describes this development candidate. The [public launch checklist](docs/public-readiness.md) tracks remote settings and publication work separately from repository changes.

Portable/offscreen tests do not qualify real Plasma/Wayland behavior, assistive technology, physical suspend/resume, hotplug, vendor drivers, multi-battery/USB-C charging, or an installed-package upgrade. Record the environment and exact results for each combination exercised. A short performance test does not establish a 35-minute retention soak or ordinary desktop responsiveness.

License: [GPL-3.0-or-later](LICENSE); AppStream metadata is CC0-1.0.
