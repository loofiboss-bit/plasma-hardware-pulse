// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Native Breeze button component (formerly custom PulseButton).
// Fully adheres to system color scheme and desktop theme.
QQC2.Button {
    id: control

    property bool isPrimary: false
    property color accentColor: Kirigami.Theme.highlightColor

    highlighted: isPrimary
}
