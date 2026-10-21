# Security policy

## Supported versions

This is development software. Security fixes target the current development branch and latest published development version. Older builds are not maintained unless explicitly stated in their release notes. There is no claim of stable-release support or a guaranteed response time.

## Report vulnerabilities privately

Do not put vulnerabilities, credentials or private telemetry in public issues. Use [GitHub private vulnerability reporting](https://github.com/loofiboss-bit/plasma-hardware-pulse/security/advisories/new) **when enabled**. Include the affected version/commit, repeatable steps, expected and actual behavior, and the impact. Use synthetic or redacted data where possible.

Private reporting requires a repository-owner setting; committing this policy does not enable it. The owner must enable and verify the reporting entry point before public launch, as tracked in [the readiness checklist](docs/public-readiness.md). If the private form is unavailable, use a private contact method explicitly published by [the repository owner](https://github.com/loofiboss-bit). If none is available, do not post exploit details publicly; a working private route remains a launch blocker.

The maintainer will assess reports and coordinate remediation and disclosure with the reporter. This volunteer project does not promise a service-level agreement.

## Trust boundaries and local data

- The app/widget and collector communicate over the user's session D-Bus. They run with the same user's existing privileges. This is not a boundary against another application already running as that user.
- Process inspection is read-only. Pulse does not send process termination signals; use KDE System Monitor for process management.
- Rolling observations are held in memory. Background monitoring, login startup and persistent numerical history are separate opt-ins.
- Persistent history stores numerical minute aggregates in a local SQLite database, excluding process identities and command lines. Explicit session/report exports are separate: process details are excluded by default but can be included by the user.
- Device identities, timestamps and hardware measurements can identify a machine even after process redaction. Review any file before sharing it. Deleted history and retained corruption-recovery copies need separate handling if the user requires secure removal.
- Imported `.pulse-session` files are untrusted. Format/version, ordering, references, size and point-count checks apply before activation. Limits are 64 MiB and 7,201 points. Atomic saves reduce partial-write risk, but do not make exported data anonymous.

## Supply chain and disclosure preparation

CI uses read-only repository contents permissions for validation and a separate tag-only release job with write permission. Checkout is pinned and does not persist credentials. Full Git history and a snapshot of tracked/non-ignored working-tree files are scanned with a pinned official Gitleaks release, checksum verification and redacted output. A clean scan is evidence from a particular revision, not proof that secrets can never exist. Never suppress a finding broadly; review the exact fingerprint and rotate a confirmed exposed credential before disclosure.

Repository visibility, vulnerability reporting, branch protection and optional GitHub security features are owner-managed remote settings. Their status is tracked separately from source changes and must be read back after an authorized change.
