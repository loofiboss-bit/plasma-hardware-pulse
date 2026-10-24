# v0.13.0 Development Validation Record

Started: 2026-10-25. Target: Fedora 44 / KDE Plasma 6. Status: **development candidate; telemetry, diagnostic rules, report export and UI styles verified**.

This record documents validation performed on the Loofi Pulse v0.13.0 candidate across automated CTest suites, QML linting, AppStream verification, and staged installation checks.

## Candidate Identity and Environment

| Field | Evidence |
| --- | --- |
| Baseline | `v0.12.0` (commit `341a352`) |
| PR #12 Integration | Commit `24f2269` on `main` (Multi-fan, NVMe telemetry, kernel log correlation, incident compare, plasmoid styles) |
| Candidate Milestone | VERSION `0.13.0`; branch `feat/v0.13.0-full-readiness` |
| Build Environment | Fedora 44 x86_64, GCC 16.2.1, CMake 4.3.0, Qt 6.11.2, Plasma 6.7.5, Kirigami/KI18n 6.31.0 |
| Publication Class | Development release candidate |

## Automated Verification Gates

| Gate / Check | Command | Result |
| --- | --- | --- |
| Version alignment | `bash scripts/check-version.sh` | Passed: 0.13.0 aligned across VERSION, metadata.json, spec, and metainfo.xml |
| Core & Telemetry Suite | `ctest --test-dir build -R pulsecore_` | Passed: 100% of core telemetry tests passed |
| NVMe Hardware Critical Rule | `ctest --test-dir build -R pulsecore_testCausalDiagnosisEngineVerdicts` | Passed: Verified `storage.nvme-critical-warning` (Severity 3) and `storage.nvme-thermal-threshold` (Severity 2) |
| Report & Export Quality | `ctest --test-dir build -R pulsecore_testJsonReportHasVersionQualityAndRecordedChannels` | Passed: Verified NVMe temp and metadata output in markdown/JSON |
| QML Static Analysis | `qmllint` across `src/app/qml/*.qml` and `package/contents/ui/*.qml` | Passed: No syntax, binding, or type regressions |
| AppStream Metadata | `appstreamcli validate --no-net packaging/com.github.loofi.hardwarepulse.metainfo.xml` | Passed: Release 0.13.0 entry validated |
| Staged Install & Activation | `scripts/validate.sh` staging checks | Passed: binaries, D-Bus service activation, plasmoid metadata |

## Regression Traceability

| Requirement | Executed Coverage |
| --- | --- |
| Multi-fan discovery & RPM progress | Verified in `PowerThermalPage.qml` and `test_pulsecore.cpp` |
| NVMe critical warning bitmask decoding | Verified in `psimemorysampler.cpp` and `test_pulsecore.cpp` |
| Causal diagnosis: single-core saturation | Verified in `causaldiagnosisengine.cpp` |
| Causal diagnosis: NVMe critical warnings & thermal threshold | Verified in `causaldiagnosisengine.cpp` and `test_pulsecore.cpp` |
| Kernel log correlation (±15s window, privacy redacted) | Verified in `kernellogreader.cpp` and `test_pulsecore.cpp` |
| Side-by-side incident comparison | Verified in `flightrecorder.cpp` and `FlightRecorderPage.qml` |
| Enriched Markdown & JSON report export | Verified in `flightrecorder.cpp` and `reportexport.cpp` |
| Three plasmoid panel display styles | Verified in `CompactRepresentation.qml` and `configGeneral.qml` |
