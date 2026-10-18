# Security Policy

## Supported Versions

Security fixes are provided for the latest published release. Development builds may receive fixes before the next release.

| Version | Supported |
| --- | --- |
| Latest release | Yes |
| Older releases | No, unless stated in that release's notes |

## Reporting a Vulnerability

Please do not report security vulnerabilities in public issues. Use [GitHub's private vulnerability reporting](https://github.com/loofiboss-bit/plasma-hardware-pulse/security/advisories/new) if it is enabled. If that option is unavailable, contact the [repository owner](https://github.com/loofiboss-bit) through a private contact method listed on their profile. Include a concise description, affected version or commit, steps to reproduce, and any relevant impact.

The maintainer will acknowledge reports as soon as practical, assess the issue, and coordinate a fix and disclosure timeline with the reporter. Please allow time for a fix before sharing vulnerability details publicly. Avoid including real credentials or personal data in reports.

## Local telemetry and saved output

The application and widget communicate with `loofi-pulse-service` on the user's session D-Bus. The collector runs with the user's existing privileges; it is not a system service or a privilege boundary against other applications running as the same user. Readable local process information is used for live inspection. Background monitoring and login startup are separate opt-in settings.

Ordinary rolling telemetry is held in memory. Sessions and reports are written only through explicit save/export actions. Process names, PIDs, start-time identities and owners are excluded from saved output by default; enabling process details includes potentially identifying information. Device identities, timestamps and hardware measurements can also reveal information about the machine. Review files before sharing them.

Imported `.pulse-session` files are untrusted data. The loader validates the format version, ordering and references before displaying the recording and enforces a 64 MiB file limit and a 7,201-point telemetry limit. Opening a session must not replace the live recording on error. Saved files are written atomically. These protections do not make a shared session anonymous or establish protection against an application already running as the same user.
