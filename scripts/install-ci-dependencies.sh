#!/usr/bin/env bash
set -euo pipefail
# Fedora 44 CI containers only. This installs packages; it is not a host setup command.
source /etc/os-release
if [[ ${ID:-} != fedora || ${VERSION_ID:-} != 44 ]]; then
    echo 'This dependency installer requires a Fedora 44 CI container.' >&2
    exit 1
fi
dnf install --assumeyes \
    cmake bash coreutils util-linux-core extra-cmake-modules gcc-c++ git gh \
    curl tar gzip zip python3 ripgrep rpm-build redhat-rpm-config kf6-rpm-macros \
    libappstream-glib dbus-daemon kf6-kcmutils kf6-kcoreaddons-devel \
    kf6-kcrash-devel kf6-ki18n-devel kf6-kirigami-devel kf6-knotifications-devel \
    kf6-kwindowsystem-devel kf6-kpackage kf6-qqc2-desktop-style libplasma-devel \
    qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel appstream gettext \
    glibc-langpack-sv
