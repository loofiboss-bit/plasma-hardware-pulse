# v0.12.0 development validation record

Started: 2026-10-21; continued 2026-10-22. Target: Fedora 44 / KDE Plasma 6. Status: **development candidate; automated integration passed; package and 35-minute offscreen qualification passed**.

This record separates tests performed on the candidate from publication, host installation and physical qualification. Commands, source snapshot, environment and measured results are recorded below. Pending remote and physical checks remain explicitly separate.

## Candidate identity and environment

| Field | Evidence |
| --- | --- |
| Baseline | v0.11.0 review started at commit `6012dc2` |
| Candidate | VERSION `0.12.0`; working-tree stabilization changes until committed |
| Candidate source snapshot | Uncommitted tree archive SHA256 `6104708d59941615b826bb14515fd62021ebcb7e494089f00124d5e020501b57`; 108 source files; no commit/tag created |
| Build environment | Fedora 44 x86_64, GCC 16.2.1, CMake 4.3.0, Qt 6.11.2, Plasma 6.7.5, Kirigami/KI18n 6.31.0 |
| Publication class | Development prerelease; stable daily-use qualification not claimed |

## Automated and package checks

| Check | Command or evidence | Result |
| --- | --- | --- |
| Version alignment | `bash scripts/check-version.sh` | Passed on integrated candidate |
| Full shared validation | `PULSE_BUILD_DIR=build/review-20261021 bash scripts/validate.sh` | Passed: 69/69 CTest entries, 89.07 seconds |
| Focused core/hardware regressions | `cmake --build build/review-20261021 --target test_pulsecore test_hardware_v09 -j2`; `ctest --test-dir build/review-20261021 --output-on-failure -R '^(pulsecore_|hardware_v09$)' -j2` | 35/35 passed during implementation; included in successful 69/69 integrated suite |
| Lifecycle, notifications, session and persistent-history regressions | Isolated D-Bus/CTest cases in the full suite | Passed in the integrated 69/69 suite |
| QML lint | Shared validation script, app and widget QML using typed `KI18n` API | Passed; zero QML warnings |
| AppStream | `appstreamcli validate --no-net packaging/com.github.loofi.hardwarepulse.metainfo.xml` | Passed on integrated candidate |
| Staged installation | Temporary DESTDIR in shared validation; binaries, plugin, collector activation, metadata/widget and absence of default autostart | Passed, including both Swedish catalogs |
| RPM/SRPM build | `bash scripts/build-release.sh` from clean committed source, or record an explicitly identified candidate snapshot | Passed: Fedora 44 `rpmbuild -ba` from the identified 108-file snapshot; package `%check` 69/69; `build/v012-final-dist/rpmbuild.log` |
| Package content/dependencies | `rpm -qpl` / `rpm -qpR` on candidate packages | Passed: activation, native plugin, widget, both Swedish catalogs and native library dependencies present; no default autostart |
| Clean/upgrade package qualification | Disposable Fedora environment with prior settings, installed candidate and readback | Passed in disposable Fedora 44 containers: clean install and v0.11-to-v0.12 upgrade, `rpm -V`, version readback and offscreen smoke; all five config/session fingerprints preserved by the RPM upgrade before first start; after start, selected unit/retention/notification/quiet-hours values remained correct and both session fingerprints remained identical while Qt populated configuration defaults; installed plugin reopened an actual v0.11 session with its original engine version |
| Release guards | Missing tag, mismatched VERSION and dirty release-source checks | Passed: all rejected with exit 1 before build/publication; `build/review-20261021/release-guard-validation.log` |
| Shell syntax and final diff | `bash -n scripts/*.sh`; `git diff --check` and scope review | Passed: shell syntax, version metadata, YAML parsing, whitespace and scope review; no generated build/audit files tracked |

### Regression traceability

| Requirement | Executed coverage |
| --- | --- |
| Storage limits, migration/legacy preservation, calendar time, same-minute resume and read failures | `historystore`: size-cap/reclamation, consistent migration backup, recorder timestamp mapping, partial-minute reopen, read/write error recovery |
| Duplicate frames, clear while disabled and retained-incident suppression | `collector_v09` real isolated service cases |
| Trend timeout/disconnect, invalid replies, incident pairing and reverse reply order | `controller_trends_errors_ordering` plus actual trend QML assertions |
| GPU sensor ordering/multiple devices, hot idle GPU, RAPL above 250 W, incomplete PSI | `hardware_v09` and three added `pulsecore` telemetry regressions |
| Tray/app save, failure, cancel and discard; narrow page navigation and C/F differences | Real `qml_testSessionDialogsAndUnsavedClose` / navigation / widget cases |
| English/Swedish view names, page identifiers, focus and accessible names | `qml_testAccessibilityAndFocusAudit` data rows and `i18n` / catalog validation |
| Session/report version compatibility, retained original diagnoses and read-only process boundary | `sessions_v09`, isolated imported-session cases and `controller_testProcessInspectionHasNoMutationApi` |

Release packaging builds from committed `git archive HEAD` source. A local modified-tree package needs its own snapshot identity and cannot be presented as the future tagged release package.

Candidate packages are under `build/v012-final-dist/rpmbuild/`: `RPMS/x86_64/plasma-hardware-pulse-0.12.0-1.fc44.x86_64.rpm` and `SRPMS/plasma-hardware-pulse-0.12.0-1.fc44.src.rpm`, with separate debuginfo/debugsource packages. Package qualification logs are under `build/qualification-env/`. The Fedora container image defaults to `%_install_langs en_US`; the clean-install catalog check explicitly enabled `en_US:sv` inside the disposable container. No host language policy was changed. A previous harness attempt stopped on the image's language exclusion; the corrected qualification passed.

Local review assets (unpublished) are grouped in `build/v012-final-dist/candidate-assets/`, including source, RPM/SRPM, debug packages, widget bundle and a verified `SHA256SUMS`. The qualified production/test sources remain identical to the source manifest; only final changelog, contributor guidance, release notes and validation/readiness documentation were edited after snapshot creation. These are candidate review packages, not committed/tagged release artifacts.

## Sustained performance

The acceptance workload is a 35-minute wall-clock run, 250 ms collection, 30-minute in-memory history and 1,000 simulated processes. Persistent SQLite retention, migration and physical size reclamation are covered separately by deterministic storage regressions. The UI harness records CPU, RSS and UI-delay p95 for successive windows; collection scan timing and the maximum-one-in-flight assertion are covered by the separate `performance_v09` core workload, not instrumented per window in this UI run. The core workload uses synthetic recorder time and does not replace elapsed wall-clock qualification. The target UI-delay p95 is at most 100 ms. Verify that memory stabilizes after the retention window fills. Targets are not measurements.

After building, run in isolated state:

```sh
PULSE_SOAK_BUILD="$PWD/build/review-20261021"
mkdir -p "$PULSE_SOAK_BUILD/soak-state/"{config,data,cache}
PULSE_SOAK_MS=2100000 QTEST_FUNCTION_TIMEOUT=2400000 \
QT_QPA_PLATFORM=offscreen QML_IMPORT_PATH="$PULSE_SOAK_BUILD/bin" \
XDG_DATA_DIRS="$PULSE_SOAK_BUILD/share:/usr/local/share:/usr/share" \
XDG_CONFIG_HOME="$PULSE_SOAK_BUILD/soak-state/config" \
XDG_DATA_HOME="$PULSE_SOAK_BUILD/soak-state/data" \
XDG_CACHE_HOME="$PULSE_SOAK_BUILD/soak-state/cache" \
dbus-run-session -- "$PULSE_SOAK_BUILD/tests/test_collector_v09" actualQmlProcessPageResponsiveness
```

**Result: passed**, exit 0, 2,100.380 seconds of actual wall-clock rendering (35 minutes), 3 QtTest cases passed / 0 failed. The 1,000-process fixture and 250 ms collector continued throughout; after 30 minutes the actual History page displayed the full rolling recording. Final retained count was 7,201 points. Overall UI event-loop delay p95 upper bound was **1.6 ms**; every five-minute window was at most **2.5 ms**, below the 100 ms requirement.

| Elapsed minutes | View | UI delay p95 upper bound | Peak UI RSS | Peak collector RSS |
| --- | --- | --- | --- | --- |
| 0.0–5.0 | Processes | 1.8 ms | 181.3 MiB | 81.0 MiB |
| 5.0–10.0 | Processes | 2.1 ms | 234.7 MiB | 114.0 MiB |
| 10.0–15.0 | Processes | 1.9 ms | 288.1 MiB | 147.6 MiB |
| 15.0–20.0 | Processes | 2.4 ms | 341.3 MiB | 181.2 MiB |
| 20.0–25.0 | Processes | 2.5 ms | 395.6 MiB | 215.3 MiB |
| 25.0–30.0 | Processes | 2.5 ms | 448.9 MiB | 249.3 MiB |
| 30.0–35.0 | History | 0.4 ms | 461.6 MiB | 252.0 MiB |

UI CPU was 446.594 seconds (21.26% of one core); collector CPU was 85.970 seconds (4.09% of one core). Final RSS was 483,745,792 bytes (461.3 MiB) for UI and 263,979,008 bytes (251.8 MiB) for the collector. From the one-minute full-History render warmup baseline to the end, UI RSS increased 2,621,440 bytes (2.50 MiB) and collector RSS 897,024 bytes (0.86 MiB). This bounded late growth, together with the rolling count, supports retention stabilization over the measured final four minutes; it is not a claim about arbitrarily long operation. Log: `build/review-20261021/soak-v012.log`.

The fixture emitted startup warnings for missing QTest application organization metadata (QSettings) and temporary graphics objects outside the scene. A bus-connection message followed test teardown. These are retained in the log; the runtime is not claimed warning-free. Production startup sets application identity, and installed-package smoke checks passed. The separate QML lint result above had zero warnings. No TypeError, ReferenceError, binding-loop or assignment errors were recorded.

The normal short CTest workload, synthetic 90-minute recorder timestamps and full-history projection regression do not replace this elapsed wall-clock check or physical desktop observation.

## Security, repository and release evidence

| Check | Result |
| --- | --- |
| Full Git-history secret scan | Maintainer audit 2026-10-21: official checksum-verified Gitleaks v8.30.1, historical Git content at reviewed baseline `6012dc2`, no findings. Final integrated history scan passed, 32 commits; `build/public-audit/candidate-final-scan.log`. |
| Candidate working-tree audit | Passed: checksum-verified Gitleaks 8.30.1 scanned the integrated tracked/non-ignored untracked tree, approximately 1.30 MB, with no findings; scan rerun after final documentation integration |
| Historical Actions logs and release payload audit | Maintainer audit 2026-10-21: all 60 fetched historical run logs, 3 historical releases / 21 downloaded assets; 18 payload checksums matched their manifests. Gitleaks found no secrets in the scanned logs or extracted payloads. The repository-wide paginated Actions artifact inventory returned zero retained artifacts on 2026-10-22; no artifact payload remained to download. Raw audit logs/files are kept in ignored local build/public-audit storage and are not included in Git. |
| Scanner false-positive review | No findings in the historical scan scopes above; no broad ignore rules added. No candidate working-tree findings. |
| Canonical Fedora 44 GitHub Actions | Not run for this candidate: commit/push were explicitly excluded. Baseline main/tag CI passed; that is not candidate CI evidence. |
| Branch/PR protections and private vulnerability reporting | Pending owner-managed setup/readback; see public-readiness.md |
| GitHub visibility/tag/release/assets/checksum readback | Not performed by repository preparation |
| COPR / KDE Store / other publication surfaces | Not performed; no publication claim |

## Physical qualification matrix

All rows below remain **unqualified** until physically exercised and recorded on the exact installed candidate. Fixtures and offscreen tests are useful automated evidence, not substitutes.

| Scenario | Status |
| --- | --- |
| Fedora 44 Plasma Wayland application + installed widget | Unqualified |
| Tray close/restore and notification actions on a real session | Unqualified |
| Physical suspend/resume with active subscriptions and history | Unqualified |
| Keyboard, screen reader, large fonts, narrow window and themes | Unqualified |
| Intel, AMD and NVIDIA driver telemetry | Unqualified by vendor/driver combination |
| Hybrid GPU selection and physical hotplug | Unqualified |
| Multiple batteries and USB-C charging/discharging transitions | Unqualified |
| Ordinary-user installed-package upgrade preserving settings | Unqualified unless separately recorded above |

## Interpretation

A public development version can be useful while hardware combinations remain unqualified, provided those limits remain visible. A stable release needs its own explicit acceptance decision. No host installation, remote settings change, tag, merge, release or package publication follows automatically from this record.
