#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
PULSE_BUILD_DIR=${PULSE_BUILD_DIR:-build/validation}
bash scripts/check-version.sh
cmake -S . -B "$PULSE_BUILD_DIR" -DBUILD_TESTING=ON -DBUILD_LIVE_HARDWARE_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$PULSE_BUILD_DIR" --parallel "${PULSE_BUILD_JOBS:-2}"
QMLLINT_BIN=$(command -v qmllint || true)
if [[ -z $QMLLINT_BIN ]]; then QMLLINT_BIN=/usr/lib64/qt6/bin/qmllint; fi
"$QMLLINT_BIN" --ignore-settings -i "$PULSE_BUILD_DIR/bin/com/github/loofi/hardwarepulse/qmldir" \
    -I "$PULSE_BUILD_DIR/bin" src/app/qml/*.qml package/contents/ui/*.qml
PULSE_BUILD_ABSOLUTE=$(cd -- "$PULSE_BUILD_DIR" && pwd)
QT_QPA_PLATFORM=offscreen QML_IMPORT_PATH="$PULSE_BUILD_ABSOLUTE/bin" \
    ctest --test-dir "$PULSE_BUILD_DIR" --output-on-failure
appstreamcli validate --no-net packaging/com.github.loofi.hardwarepulse.metainfo.xml
PULSE_INSTALL_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/loofi-pulse-install.XXXXXX")
trap 'rm -rf -- "$PULSE_INSTALL_ROOT"' EXIT
DESTDIR="$PULSE_INSTALL_ROOT" cmake --install "$PULSE_BUILD_DIR"
test -x "$PULSE_INSTALL_ROOT/usr/bin/loofi-pulse"
test -x "$PULSE_INSTALL_ROOT/usr/bin/loofi-pulse-service"
PULSE_SERVICE_FILE="$PULSE_INSTALL_ROOT/usr/share/dbus-1/services/com.github.loofi.hardwarepulse.Collector1.service"
test -f "$PULSE_SERVICE_FILE"
rg --fixed-strings --line-regexp 'Name=com.github.loofi.hardwarepulse.Collector1' "$PULSE_SERVICE_FILE"
rg --fixed-strings --line-regexp 'Exec=/usr/bin/loofi-pulse-service' "$PULSE_SERVICE_FILE"
test ! -e "$PULSE_INSTALL_ROOT/etc/xdg/autostart/com.github.loofi.hardwarepulse.desktop"
test -f "$PULSE_INSTALL_ROOT/usr/share/applications/com.github.loofi.hardwarepulse.desktop"
test -f "$PULSE_INSTALL_ROOT/usr/share/metainfo/com.github.loofi.hardwarepulse.metainfo.xml"
test -f "$PULSE_INSTALL_ROOT/usr/share/plasma/plasmoids/com.github.loofi.hardwarepulse/metadata.json"
find "$PULSE_INSTALL_ROOT/usr" -path '*/com/github/loofi/hardwarepulse/libhardwarepulseplugin.so' -print -quit | rg .
git -c safe.directory="$PWD" diff --check
