# Public development readiness

This is a preparation checklist, not evidence that remote settings or publication have changed. Record the exact repository/candidate revision and readback for owner-managed actions.

## Repository content

- [x] Run the final shared validation and complete the [v0.12 record](validation-v0.12.0.md), including package and soak results.
- [x] Audit full Git history and candidate working-tree additions with checksum-verified Gitleaks v8.30.1. Review exact findings with redacted output. Revoke/rotate a real exposed credential before making history public; deleting its latest occurrence does not remove it from history.
- [x] Review tracked source/history for private telemetry, local paths that expose private data, binary dumps and unwanted artifacts. Preserve necessary licenses and provenance.
- [x] Verify README, version metadata, development release notes, contribution guidance and issue/PR templates against actual behavior.
- [x] Keep physical desktop/driver combinations unqualified where they have not been exercised.

The local audit found no secrets in 32 commits, the candidate working tree, all 60 historical Actions logs or extracted assets from three historical releases. Checksums matched all 18 historical payloads with manifests. The repository-wide Actions artifact inventory returned zero retained artifacts on 2026-10-22. This evidence is revision-bound; scan again after subsequent changes. Existing commit/tag metadata is preserved by the owner's choice. The checkout's Git email is configured locally to the account's verified noreply address for future commits; global Git settings are unchanged.

Suggested public metadata: description **Local hardware monitoring and incident investigation for Fedora KDE Plasma 6**; topics `kde-plasma`, `plasma6`, `qt6`, `qml`, `linux`, `fedora`, `hardware-monitor`, `telemetry`. These suggestions have not been applied remotely.

## Owner-managed GitHub settings

The following changes require a separately authorized remote action; they are not applied by these files:

- [ ] Verify the intended public repository and default `main` branch, description, topics and license display before changing visibility.
- [ ] Enable [private vulnerability reporting](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository), then read back that the public security page offers a working private reporting entry point. Do not advertise an unavailable contact channel.
- [ ] Protect `main` with pull requests, required conversation resolution and passing `Fedora 44 validation` / `Secret scan` checks. Enable required checks only after their exact names exist in a successful run.
- [ ] Block force pushes and branch deletion. Keep any owner bypass explicit and limited. Record the configured review policy accurately; repository preparation does not impose an additional approval requirement.
- [ ] Restrict release-tag creation/update/deletion to maintainers using an applicable tag ruleset. The workflow additionally checks exact VERSION and ancestry from `origin/main`.
- [ ] Confirm Actions defaults are read-only and fork pull requests do not receive repository secrets or write tokens. This repository uses `pull_request`, not `pull_request_target`.
- [ ] Enable applicable GitHub dependency/security features where available and record which are actually enabled. Availability is not assumed.

Branch protection, vulnerability reporting and secret/security settings are GitHub-side state. A committed policy file does not configure them.

## Development publication gate

- [ ] Commit the reviewed candidate and observe canonical Fedora 44 CI at that exact commit.
- [ ] Create an explicitly authorized `v0.12.0` tag whose commit is reachable from `origin/main` and whose VERSION matches exactly.
- [ ] Observe both read-only jobs and the separate tag-only release rebuild. Release assets are built again from committed source using the shared validation script; there is no dependency on Actions artifact storage quota.
- [ ] Confirm the resulting GitHub release is marked **prerelease**, then independently read back the tag/commit, assets and checksums. Downloaded hashes should match the published checksum manifest.
- [ ] Verify any separately requested COPR, KDE Store or other publication surface independently. No such publication is implied by the GitHub release workflow.

Tagging and publishing are separate from preparing this repository. Re-running a tag job against an already existing release should fail for maintainer review, rather than silently replacing published assets.

## Workflow and scanner provenance

Checkout uses `actions/checkout` v4 pinned to `11d5960a326750d5838078e36cf38b85af677262`, with full history and persisted credentials disabled. Validation jobs have `contents: read`; only the tag-only release job has `contents: write`. The Fedora container is pinned to the Fedora 44 release series, with package/security updates intentionally following that series.

Secret scanning downloads the official [Gitleaks v8.30.1 release](https://github.com/gitleaks/gitleaks/releases/tag/v8.30.1) archive and checksum manifest, verifies the selected archive before execution and redacts output. It scans full fetched Git history and a temporary snapshot of tracked plus non-ignored untracked working-tree files. Ignored build/audit data and symlink targets outside the checkout are not copied. The maintainer separately reviews extracted published payloads and available remote logs. A download/checksum/scan failure fails CI; there is no bypass on scanner failure.
