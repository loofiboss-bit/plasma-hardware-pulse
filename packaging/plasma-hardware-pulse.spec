Name:           plasma-hardware-pulse
Version:        0.5.0
Release:        1%{?dist}
Summary:        Hardware, temperature, memory pressure, and power monitor for KDE Plasma 6

License:        GPL-3.0-or-later AND CC0-1.0
URL:            https://github.com/loofiboss-bit/plasma-hardware-pulse
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.24
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  kf6-kcmutils
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kirigami-devel
BuildRequires:  kf6-knotifications-devel
BuildRequires:  kf6-kpackage
BuildRequires:  kf6-qqc2-desktop-style
BuildRequires:  libplasma-devel
BuildRequires:  plasma-workspace-devel
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  libappstream-glib

Requires:       kf6-kirigami
Requires:       kf6-qqc2-desktop-style
Requires:       libplasma

%description
Loofi Pulse provides a KDE Plasma 6 panel applet and a Kirigami desktop
application for viewing system hardware readings, temperatures, Linux memory
and I/O pressure, disk throughput, power, and recent telemetry history.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake_kf6 \
    -DBUILD_TESTING=ON \
    -DBUILD_LIVE_HARDWARE_TESTS=OFF
%cmake_build

%install
%cmake_install

%check
appstream-util validate-relax --nonet %{buildroot}%{_kf6_metainfodir}/com.github.loofi.hardwarepulse.metainfo.xml
QT_QPA_PLATFORM=offscreen QML_IMPORT_PATH="%{_vpath_builddir}/bin" ctest --test-dir "%{_vpath_builddir}" --output-on-failure

%files
%license LICENSE
%doc README.md
%{_bindir}/loofi-pulse
%{_libdir}/qt6/qml/com/github/loofi/hardwarepulse/
%{_datadir}/plasma/plasmoids/com.github.loofi.hardwarepulse/
%{_datadir}/knotifications6/plasma_applet_com.github.loofi.hardwarepulse.notifyrc
%{_datadir}/icons/hicolor/scalable/apps/com.github.loofi.hardwarepulse.svg
%{_datadir}/applications/com.github.loofi.hardwarepulse.desktop
%{_kf6_metainfodir}/com.github.loofi.hardwarepulse.metainfo.xml

%changelog
* Sat Oct 17 2026 Loofi <loofi@github.com> - 0.5.0-1
- Release 0.5.0 with power-aware causal diagnosis, interactive Overview domain cards, incident timeline chips, structured Power & Cooling cards, and deep-linked Plasma widget navigation
* Sat Oct 17 2026 Loofi <loofi@github.com> - 0.4.0-1
- Release 0.4.0 with incremental telemetry history, adaptive process radar, CPU hwmon power, Intel Xe GPU frequency, and disk I/O throughput
