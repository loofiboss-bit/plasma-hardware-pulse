# Contributing to Loofi Pulse

Loofi Pulse is a Fedora KDE-focused development project. Contributions should improve trustworthy local investigation without silently substituting zero for missing measurements or expanding routine privileges.

## Before changing code

Search existing issues and code first. For a bug, include version/commit, Fedora/distribution and Plasma versions, session type, relevant kernel/driver, repeatable steps, and expected versus actual behavior. Report vulnerabilities through [SECURITY.md](SECURITY.md), not an ordinary issue. Sanitize logs and saved sessions before sharing them.

Keep changes focused and preserve unrelated work. Discuss larger changes before implementation. Follow existing C++20/Qt/KDE patterns; code, comments and repository documentation use English. User-visible text uses the existing KDE translation mechanism. New telemetry needs explicit validity, identity and rate-baseline handling.

## Local validation

Install the development requirements listed in the Fedora spec, plus QML/AppStream/test tools, then run:

```sh
bash scripts/validate.sh
```

The same script is used by CI and release packaging. It checks aligned versions, builds, runs the portable suite, checks QML/AppStream, and verifies a temporary staged installation. It does not install onto the normal desktop. The Fedora dependency installer in `scripts/` is intended for disposable Fedora 44 CI containers, not an automatic change to a contributor's host.

Run focused tests while iterating. Add regression cases for behavior changes and missing/malformed data, counter rollback, warmup and device replacement when relevant. Use isolated D-Bus and temporary XDG directories for service/UI tests. Do not let a development test control an installed collector or normal user data.

Live hardware checks are opt-in (`BUILD_LIVE_HARDWARE_TESTS=ON`). Offscreen tests are automated evidence only; record any physical desktop/driver qualification separately. State skipped checks explicitly in the PR.

## Compatibility and privacy

Preserve session format 1, report schema 2 and existing configuration/page identifiers unless a deliberate compatibility change is agreed. Collector changes must consider old/new client behavior. Recorded diagnoses are historical evidence, not input for silent reinterpretation by a newer engine.

Keep process inspection read-only. Persistent numerical history must not receive process identities, command lines or owners. Saved sessions/reports must preserve default process redaction. Review exported data and secret-scan findings rather than adding broad exclusions.

## Submitting a pull request

Use your GitHub noreply address for new commits when you want to keep your personal email private. Set it for this checkout with `git config --local user.email "YOUR_GITHUB_NOREPLY_ADDRESS"`; this does not rewrite existing commits.

Explain the problem, resulting behavior, relevant compatibility/privacy changes, and exact verification. Small fixes usually need a short description and a focused test. Do not claim remote CI or physical qualification before observing it. A maintainer reviews and merges changes; publication and host installation are separate actions.

Maintainers can build RPM/SRPM, source and widget assets with `bash scripts/build-release.sh` from a clean committed checkout. This invokes the shared validation first and writes to `build/validation/dist/`. It does not publish. The tag workflow checks the exact VERSION and main-branch ancestry before creating a development prerelease. See [the public readiness checklist](docs/public-readiness.md) for owner-managed settings and publication gates.
