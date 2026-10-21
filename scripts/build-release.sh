#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
# Build committed source only, so RPMs and source archives represent the same revision.
if [[ -n $(git -c safe.directory="$PWD" status --porcelain --untracked-files=normal) ]]; then
    echo 'Release packaging requires a clean committed checkout.' >&2
    exit 1
fi
bash scripts/validate.sh
PULSE_VERSION=$(cat VERSION)
PULSE_BUILD_DIR=${PULSE_BUILD_DIR:-build/validation}
PULSE_DIST="$PULSE_BUILD_DIR/dist"
mkdir -p "$PULSE_DIST"
rm -f -- "$PULSE_DIST/"*.tar.gz "$PULSE_DIST/"*.rpm "$PULSE_DIST/"*.plasmoid "$PULSE_DIST/SHA256SUMS"
PULSE_RPM_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/loofi-pulse-rpm.XXXXXX")
trap 'rm -rf -- "$PULSE_RPM_ROOT"' EXIT
mkdir -p "$PULSE_RPM_ROOT/SOURCES"
git -c safe.directory="$PWD" archive --format=tar --prefix="plasma-hardware-pulse-$PULSE_VERSION/" HEAD \
    | gzip -n > "$PULSE_RPM_ROOT/SOURCES/plasma-hardware-pulse-$PULSE_VERSION.tar.gz"
rpmbuild -ba packaging/plasma-hardware-pulse.spec --define "_topdir $PULSE_RPM_ROOT" \
    --define "_smp_build_ncpus ${PULSE_BUILD_JOBS:-2}"
cp "$PULSE_RPM_ROOT/SOURCES/plasma-hardware-pulse-$PULSE_VERSION.tar.gz" "$PULSE_DIST/"
find "$PULSE_RPM_ROOT/RPMS" "$PULSE_RPM_ROOT/SRPMS" -name '*.rpm' -exec cp {} "$PULSE_DIST/" \;
# zip writes relative entries; the native plugin and collector are provided by the RPM.
PULSE_DIST_ABSOLUTE=$(cd -- "$PULSE_DIST" && pwd)
rm -f -- "$PULSE_DIST_ABSOLUTE/com.github.loofi.hardwarepulse.plasmoid"
(cd package && zip -r "$PULSE_DIST_ABSOLUTE/com.github.loofi.hardwarepulse.plasmoid" metadata.json contents/)
(cd "$PULSE_DIST" && sha256sum -- *.tar.gz *.rpm *.plasmoid > SHA256SUMS && sha256sum --check SHA256SUMS)
