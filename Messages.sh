#! /usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
podir=${podir:-po}
XGETTEXT=${XGETTEXT:-xgettext}
mkdir -p "$podir"
mapfile -d '' app_sources < <(find src -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.qml' -o -name '*.js' \) -print0 | sort -z)
mapfile -d '' widget_sources < <(find package -type f \( -name '*.qml' -o -name '*.js' \) -print0 | sort -z)
"$XGETTEXT" --from-code=UTF-8 --language=C++ --qt -ki18n -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -ki18nd:2 -ki18ndc:2c,3 \
    "${app_sources[@]}" -o "$podir/loofi-pulse.pot"
"$XGETTEXT" --from-code=UTF-8 --language=C++ --qt -ki18n -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -ki18nd:2 -ki18ndc:2c,3 \
    "${widget_sources[@]}" -o "$podir/plasma_applet_com.github.loofi.hardwarepulse.pot"
