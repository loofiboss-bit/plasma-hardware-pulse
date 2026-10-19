#! /usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later

podir=${podir:-po}
XGETTEXT=${XGETTEXT:-xgettext}

$XGETTEXT --from-code=UTF-8 -C --qt -ki18n -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 \
    `find src/ -name "*.cpp" -o -name "*.h" -o -name "*.qml"` \
    -o "$podir/loofi-pulse.pot" 2>/dev/null || true

$XGETTEXT --from-code=UTF-8 -C --qt -ki18n -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 \
    `find package/ -name "*.qml" -o -name "*.js"` \
    -o "$podir/plasma_applet_com.github.loofi.hardwarepulse.pot" 2>/dev/null || true
