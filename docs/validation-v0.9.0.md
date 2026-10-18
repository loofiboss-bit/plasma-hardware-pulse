# Loofi Pulse v0.9.0 local validation

Status: implementation and local automated/package validation passed. The corrected sustained 35-minute run passed; final packages include the Follow Live correction. Physical desktop/hardware qualification remains open. These results were collected before release publication and installation on the ordinary system.

## Environment

Fedora 44 x86_64, kernel 7.2.11-200.fc44.x86_64; Qt 6.11.2, KF6 6.31.0, libplasma 6.7.5. Qt Quick tests use the offscreen platform. Package install/upgrade used two separate disposable Fedora 44 containers with normal DNF dependency resolution and scriptlets.

The local source-manifest/archive hash below identifies the implementation snapshot before the final release-workflow and release-documentation additions. The canonical tag workflow validates the final release commit and publishes its own archive and checksums.

## Completed checks

- CMake build and CTest: **52/52 passed** (final source including Follow Live correction); see test log (local evidence log).
- QML lint: passed without warnings; AppStream: passed; final diff whitespace check: passed.
- RPM/SRPM build and RPM `%check`: **52/52 passed**, including cleanup; RPM build log (local evidence log).
- Final SRPM source matches all **76** tracked/untracked project source files plus the packaged spec. Final source archive SHA256: `320f66635cdc9b564aed88059773fc5d15ed2d1d5511f7830e795ee8dc1beddb`.
- Clean Fedora installation and v0.8.0 -> v0.9.0 upgrade: passed. Configuration checksums, `rpm -V`, installed D-Bus activation/subscription, native application smoke and disabled autostart passed; compact evidence (local evidence log).
- Staged installation under a path containing spaces: activation, subscription and native app smoke passed. Regression test also covers Fedora absolute installation directories.
- Plasmoid installed successfully into an isolated package root. This does not qualify a running Plasma panel.
- Actual full-disk (1 MiB private tmpfs) and denied-write session probes preserved the previous session file; actionable failure messages returned. Full disk (local evidence log), denied write (local evidence log).
- An existing GPU sensor made unreadable by file permissions became unavailable with an explanation; permission probe (local evidence log).

## Performance

Production core workload, simulated timestamps and real filesystem scans:

virtual history=90 min, interval=250 ms, samples=21601, retained=7201/7201; processes=1000; scans=40; scan mean=17.172 ms max=24.886 ms; heartbeat p95=0.159 ms; RSS first=39976960 second=46444544 delta=6467584 bytes; CPU=0.902 s (8.86% of one core), wall=10.177 s; maximum in-flight=1; steady-state RSS=46481408 delta=36864 bytes

This covers three retention windows at 250 ms with 1,000 simulated processes. The core worker counter reached one in-flight operation. The service also serializes sensor/process work on a single Qt worker event loop.

The sustained production-QML/service run requests 35 minutes at 250 ms with 1,000 fixed simulated processes and 30-minute retention. It opens History at minute 30. The first completed run failed: full History p95 was 847.6 ms, above the 100 ms limit. The corrected projection uses a bounded incremental numeric cache, per-pixel extrema, bounded raster paths and threaded Canvas rendering. The final separate rolling full-history regression passed at p95 66.0 ms (synthetic extreme-value workload, UI CPU 18.117 s / 180.75% of one core, helper CPU 0.390 s / 3.89%, elapsed 10.023 s; UI RSS 450,813,952 -> 463,400,960 bytes, helper RSS 48,676,864 -> 49,434,624 bytes). Concurrent-build repeats exceeded 100 ms and are not accepted as passes. The renewed wall-clock run passed (2,099.800 s measured, requested 2,100 s; total test 2,101.563 s). Retained points: 7,201. Overall UI event-loop p95: **0.8 ms**; final five-minute History window: **0.5 ms**. Five-minute window p95 values: 1.1 / 0.8 / 0.8 / 0.8 / 0.8 / 0.8 / 0.5 ms. UI CPU: 473.945 s (22.57% of one core); helper CPU: 102.540 s (4.88%). History-only CPU was approximately 53.3% of one core for the UI and 4.8% for the helper.

RSS before filling -> end: UI 121,044,992 -> 468,537,344 bytes; helper 48,803,840 -> 263,983,104 bytes. After the full History page's one-minute warmup (minute 31), UI RSS changed 467,005,440 -> 468,537,344 (+1,531,904 bytes), helper 262,590,464 -> 263,983,104 (+1,392,640 bytes). This is small residual RSS growth, not literal zero growth; the separate three-retention-window core test shows bounded steady-state allocation (+36,864 bytes). These measurements support bounded retention but do not prove that process RSS can never increase during longer runs. Sustained log (local evidence log).

The sustained run loaded the corrected performance implementation before the final isolated-point rendering and Follow Live fixes. Neither changed the exercised collection/raster paths for this workload; the final source, including those fixes, passed the complete 52-test suite, full-history regression, RPM checks and clean/upgrade container checks. The earlier interrupted 14-minute run is excluded from acceptance evidence.

## Qualification boundary

No physical Plasma/Wayland, real screen reader, suspend/resume, hardware hotplug, Intel/AMD/NVIDIA driver matrix, multiple physical batteries or USB-C charging qualification was performed. Deterministic fixtures cover the corresponding data models and unavailable/reset cases. The QML process workload uses simulated procfs and lacks physical GPU/battery measurements. Offscreen timing is not compositor/input latency. Remote CI for these uncommitted changes was not run.

The optional direct user-namespace RPM-database probe was blocked by host permissions; it is not used as installation evidence. The full DNF container checks above replace it. Host security policy was not changed.

## Artifacts

The canonical GitHub tag workflow builds the source archive, plasmoid, RPM and SRPM and publishes SHA256SUMS. Release packages are unsigned. Local package hashes are separate from the canonical CI artifacts.
