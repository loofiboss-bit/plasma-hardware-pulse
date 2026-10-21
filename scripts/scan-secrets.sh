#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
# Official release binaries; verify the selected archive against the official checksum file.
PULSE_GITLEAKS_VERSION=8.30.1
case $(uname -m) in
    x86_64) PULSE_SCAN_ARCH=x64 ;;
    aarch64|arm64) PULSE_SCAN_ARCH=arm64 ;;
    *) echo 'Unsupported Gitleaks binary architecture.' >&2; exit 1 ;;
esac
PULSE_SCAN_TMP=$(mktemp -d "${TMPDIR:-/tmp}/loofi-pulse-secrets.XXXXXX")
trap 'rm -rf -- "$PULSE_SCAN_TMP"' EXIT
PULSE_SCAN_ARCHIVE="gitleaks_${PULSE_GITLEAKS_VERSION}_linux_${PULSE_SCAN_ARCH}.tar.gz"
PULSE_SCAN_CHECKSUMS="gitleaks_${PULSE_GITLEAKS_VERSION}_checksums.txt"
PULSE_SCAN_URL="https://github.com/gitleaks/gitleaks/releases/download/v${PULSE_GITLEAKS_VERSION}"
curl --fail --silent --show-error --location --proto '=https' --tlsv1.2 \
    "$PULSE_SCAN_URL/$PULSE_SCAN_ARCHIVE" -o "$PULSE_SCAN_TMP/$PULSE_SCAN_ARCHIVE"
curl --fail --silent --show-error --location --proto '=https' --tlsv1.2 \
    "$PULSE_SCAN_URL/$PULSE_SCAN_CHECKSUMS" -o "$PULSE_SCAN_TMP/$PULSE_SCAN_CHECKSUMS"
(cd "$PULSE_SCAN_TMP" && awk -v archive="$PULSE_SCAN_ARCHIVE" '$2 == archive { print; found=1 } END { if (!found) exit 1 }' \
    "$PULSE_SCAN_CHECKSUMS" | sha256sum --check --strict)
tar -xzf "$PULSE_SCAN_TMP/$PULSE_SCAN_ARCHIVE" -C "$PULSE_SCAN_TMP" gitleaks
"$PULSE_SCAN_TMP/gitleaks" version
"$PULSE_SCAN_TMP/gitleaks" git --redact=100 --no-banner --log-opts=--all .

# Scan current tracked and non-ignored untracked files, including uncommitted changes.
# Do not copy ignored build/audit data or dereference links outside the checkout.
PULSE_SCAN_SNAPSHOT="$PULSE_SCAN_TMP/worktree"
mkdir -p "$PULSE_SCAN_SNAPSHOT"
git -c safe.directory="$PWD" ls-files --cached --others --exclude-standard -z > "$PULSE_SCAN_TMP/files"
python3 - "$PWD" "$PULSE_SCAN_SNAPSHOT" "$PULSE_SCAN_TMP/files" <<'PY_SNAPSHOT'
import os
import pathlib
import shutil
import sys
root, snapshot, listing = map(pathlib.Path, sys.argv[1:])
for raw in listing.read_bytes().split(b'\0'):
    if not raw:
        continue
    relative = pathlib.Path(os.fsdecode(raw))
    if relative.is_absolute() or '..' in relative.parts:
        raise SystemExit('Unexpected path in Git file list')
    source = root / relative
    target = snapshot / relative
    if source.is_symlink():
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(os.readlink(source))
    elif source.is_file():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    elif source.exists():
        raise SystemExit('Directory or submodule needs an explicit secret-scan policy')
    # Tracked paths deleted from the working tree are covered by Git-history scanning.
PY_SNAPSHOT
"$PULSE_SCAN_TMP/gitleaks" dir --redact=100 --no-banner "$PULSE_SCAN_SNAPSHOT"
