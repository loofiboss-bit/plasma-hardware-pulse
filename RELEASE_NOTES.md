# Loofi Pulse v0.14.0 — Diagnostic Intelligence & Reliability

This is a development candidate for Fedora KDE Plasma 6. It improves how Loofi Pulse describes diagnostic certainty, keeps telemetry fresh, and connects incident reports to kernel events. It does not claim that automated checks establish physical hardware or desktop qualification.

## Changes

### Diagnostic confidence

- Diagnostics explicitly identify observations, suspected causes, and confirmed problems. They show missing evidence and uncertainty, and keep recommendations tied to the available readings.
- High CPU load plus a power-saver profile is no longer called throttling. A single PL1 reading is treated as an observation, and CPU heat does not by itself confirm throttling.
- Wi-Fi signal strength is presented as NetworkManager's percentage. It is not converted into a made-up dBm value or treated as proof of poor throughput.
- Wi-Fi SSID, signal, frequency, and bitrate remain attached to the correct adapter when D-Bus replies arrive out of order.
- Old or unavailable Wi-Fi and power-profile readings are excluded from current diagnosis.

### Flight Recorder and reports

- Kernel journal queries run asynchronously, use wall-clock sample times, and include event timestamps and incident IDs in exports.
- Markdown reports summarize incidents with evidence, uncertainty, recommendations, and the completeness of before/during/after comparisons. HTML and JSON exports retain schema 2 and include available report context.
- Default report exports redact process identities and network identifiers. Kernel messages are privacy-filtered before export.

### Polling and interface behavior

- Inactive desktop windows no longer force the fastest shared collector interval; active clients still set the fastest requested interval and all clients retain their requested history.
- The Flight Recorder coalesces automatic history projection and repainting to two updates per second while keeping direct sample selection immediate.
- Fan and alert animations pause when their page or widget is hidden and are disabled when KDE reports reduced motion.
- The existing app and widget remain on the shared Qt 6, Kirigami, Plasma 6, and Collector1 architecture.

## Compatibility and privacy

- `.pulse-session` format 1, JSON report schema 2, Collector1, existing settings, and diagnostic rule IDs remain supported. Added session evidence fields are optional.
- Processing stays local. Routine monitoring requires no new privileges. Background monitoring, login startup, and persistent history remain separate opt-ins.

## Validation status

See [docs/validation-v0.14.0.md](docs/validation-v0.14.0.md) for automated results and outstanding manual hardware checks. This remains a development candidate; no release artifacts are implied.
