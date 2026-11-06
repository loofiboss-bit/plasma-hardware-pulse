# v0.14.0 Development Validation Record

Date: 2026-11-03. Candidate: **Diagnostic Intelligence & Reliability**. Status: automated validation and a local Fedora RPM installation passed; interactive desktop and physical hardware qualification remains outstanding.

## Candidate and environment

| Field | Evidence |
| --- | --- |
| Baseline | Latest `origin/main`, `758504833f627abd5efb197439930251976e2ae6` |
| Working branch | `codex/v0.14.0-diagnostic-intelligence` |
| Version | `0.14.0`, aligned by `scripts/check-version.sh` |
| Build environment | Fedora 44 x86_64, GCC 16.2.1, Qt 6.11.2, CMake 4.3.0 |
| Build type | RelWithDebInfo validation; RPM built with Fedora release flags; live hardware tests disabled |
| Publication | Development candidate; local RPM built and installed, with no release artifacts published |

## Automated verification

| Gate | Command | Result |
| --- | --- | --- |
| Complete validation | `PULSE_BUILD_DIR=build/v014-final PULSE_BUILD_JOBS=1 bash scripts/validate.sh` | Passed |
| Version alignment | Included in `scripts/validate.sh` | Passed: all version metadata is `0.14.0` |
| Build | CMake configure and build in `build/v014-final` | Passed |
| CTest | Included in `scripts/validate.sh` | Passed: 83/83 tests, including core, D-Bus, session, QML, accessibility, notification, collector and performance checks |
| QML lint | `qmllint --ignore-settings` over app and widget QML | Passed with one existing layout-anchor warning at `FlightRecorderPage.qml:1481` |
| AppStream | `appstreamcli validate --no-net packaging/com.github.loofi.hardwarepulse.metainfo.xml` | Passed |
| Staged installation | `DESTDIR=<temporary directory> cmake --install build/v014-final` and activation/path assertions | Passed: binaries, QML plugin, widget, D-Bus activation and desktop metadata installed; no login autostart was added |
| RPM spec parsing | `rpmspec -q packaging/plasma-hardware-pulse.spec` | Passed: resolves as `plasma-hardware-pulse-0.14.0-1` |
| Fedora RPM build | `rpmbuild -bb --nocheck packaging/plasma-hardware-pulse.spec` | Passed: produced `plasma-hardware-pulse-0.14.0-1.fc44.x86_64.rpm` and Fedora debug subpackages from the working-tree source snapshot |
| RPM package check | `rpm -q plasma-hardware-pulse`; `rpm -V plasma-hardware-pulse`; `rpm -qf` on app, service, QML plugin and widget | Passed: package installed at `0.14.0-1.fc44`, files are RPM-owned, verification reports no modifications |
| RPM DNF transaction | `sudo -n dnf install -y <local RPM>` | Passed: installed one package, removed none |
| Diff hygiene | `git diff --check` | Passed |

## Manual host installation

- Replaced the prior manual source install and removed its manifest-listed application, service, plugin, widget, desktop, locale and metadata files. Removed the stale user-local D-Bus activation entry and the QML environment override that prepended `~/.local` plugin paths.
- Installed `plasma-hardware-pulse-0.14.0-1.fc44.x86_64` with DNF. The previous `plasma-hardware-pulse-0.13.0-1.fc44` package was absent; the DNF transaction installed one package and removed none.
- Verified `loofi-pulse 0.14.0`, a successful offscreen `--smoke-test`, and D-Bus activation through `/usr/bin/loofi-pulse-service`. `kpackagetool6` now resolves the widget from `/usr/share/plasma/plasmoids/com.github.loofi.hardwarepulse/`.
- Confirmed RPM ownership of the application, service, native QML plugin, widget and activation file; `rpm -V plasma-hardware-pulse` reported no modified or missing files.
- Cleared the app QML and test caches after the installation smoke checks. Existing Pulse configuration and saved report data were retained.
- The applet package and widget QML tests passed, but its visual behavior in the running Plasma panel was not independently qualified.

## Performance measurements

The collector regression harness uses an isolated D-Bus session, a 1,000-process fixture, offscreen Qt Quick rendering and synthetic samples. It does not exercise the desktop compositor or physical devices.

| Scenario | Result |
| --- | --- |
| Processes page, 5.25-second run | 0.8 ms event-loop p95 upper bound; UI CPU 14.2% of one core; collector CPU 2.1%; UI RSS grew 2.7 MB and collector RSS 0.8 MB |
| History page, 7,201 retained samples and 250 ms updates, 9.74-second run | 2.7 ms event-loop p95 upper bound against the 100 ms gate; 64-column native projection 40.5 ms; UI CPU 43.2% of one core; collector CPU 3.8%; UI RSS grew 8.0 MB and collector RSS 1.6 MB |

The History canvas coalesces automatic plot projection and repainting to at most twice per second while retaining every sample at its configured cadence; direct pointer and keyboard selection still repaints immediately. The 30-minute configured history was prefilled synthetically; this was not a 30-minute wall-clock retention soak. No directly comparable pre-change full-history memory baseline was recorded in this validation, so the measured 8.0 MB UI growth remains a qualification datapoint rather than a before/after claim.

## Regression coverage

| Behavior | Coverage |
| --- | --- |
| Observation/suspected/confirmed states, missing evidence, unavailable profiles, zero-percent Wi-Fi and false-throttling cases | `test_pulsecore.cpp` diagnosis and telemetry cases |
| NetworkManager signal percentage is not dBm; zero percent remains visible only when available | Core telemetry and `MeasurementLabels.js` controller-action test |
| Delayed/reordered NetworkManager replies keep SSID, signal, frequency and bitrate associated with the correct Wi-Fi adapter | `controller_wifi_multi_adapter` |
| Journal realtime timestamps, all-boot kernel transport query, IPv6 and OOM privacy, PCI BDF preservation | `test_pulsecore.cpp` kernel-log cases |
| Diagnosis confidence and power-profile availability survive JSON export | `testJsonReportPreservesDiagnosisAndPowerProfileAvailability` |
| Process opt-in does not expose network identifiers | `testReportRedactsNetworkIdentifiersEvenWhenProcessDetailsAreIncluded` |
| Hidden-client polling/process-scan backoff, retained history, stale timeout and activity recovery | `collector_v09` isolated D-Bus regression |
| Verified profile changes reach live properties and saved samples immediately | `controller_async_profile_success` |
| Async kernel-log loading, HTML/Markdown/JSON report behavior and UI navigation | Controller, QML and D-Bus integration tests |

## Manual qualification still required

- Qualify install, upgrade and removal from a published, signed release RPM on a fresh Fedora KDE Plasma 6 system; this host used a local development RPM.
- Exercise the installed app and widget interactively in a real Wayland session.
- Check compact widget layouts, theme changes, reduced-motion behavior, keyboard navigation, screen reader names, large text and narrow windows.
- Verify live NetworkManager readings across Wi-Fi devices, roaming, disconnect/reconnect and a real 0% signal condition; no hardware dBm measurement is claimed.
- Exercise Power Profiles Daemon changes, suspend/resume, device hotplug and kernel-event correlation across a real reboot.
- Run a sustained hardware/desktop retention and memory soak; current performance evidence is a short synthetic offscreen test.

These checks were not available in the automated environment. No elevated privileges were introduced for normal monitoring; background monitoring, login startup and persistent history remain separate opt-ins.
