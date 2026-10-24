Name:           plasma-hardware-pulse
Version:        0.13.0
Release:        1%{?dist}
Summary:        Hardware, temperature, memory pressure, and power monitor for KDE Plasma 6

License:        GPL-3.0-or-later AND CC0-1.0
URL:            https://github.com/loofiboss-bit/plasma-hardware-pulse
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.24
BuildRequires:  bash
BuildRequires:  coreutils
BuildRequires:  util-linux-core
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  dbus-daemon
BuildRequires:  kf6-rpm-macros
BuildRequires:  kf6-kcmutils
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-kcrash-devel
BuildRequires:  kf6-ki18n-devel >= 6.21
BuildRequires:  kf6-kirigami-devel
BuildRequires:  kf6-knotifications-devel
BuildRequires:  kf6-kwindowsystem-devel
BuildRequires:  gettext
BuildRequires:  glibc-langpack-sv
BuildRequires:  kf6-kpackage
BuildRequires:  kf6-qqc2-desktop-style
BuildRequires:  libplasma-devel
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  libappstream-glib

Requires:       kf6-kirigami
Requires:       kf6-qqc2-desktop-style
Requires:       libplasma
Requires:       dbus-daemon

%description
Loofi Pulse provides a KDE Plasma 6 panel applet and a Kirigami desktop
application for viewing system hardware readings, temperatures, Linux memory
and I/O pressure, per-device storage and network activity, processes, power,
and recent telemetry history. A shared user-session collector supports the app
and widget. Incident investigations can be saved and reopened locally; background
monitoring and login startup are optional.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake_kf6 \
    -DBUILD_TESTING=ON \
    -DBUILD_LIVE_HARDWARE_TESTS=OFF
%cmake_build

%install
%cmake_install
%find_lang %{name} --all-name

%check
appstream-util validate-relax --nonet %{buildroot}%{_kf6_metainfodir}/com.github.loofi.hardwarepulse.metainfo.xml
QT_QPA_PLATFORM=offscreen QML_IMPORT_PATH="%{_vpath_builddir}/bin" ctest --test-dir "%{_vpath_builddir}" --output-on-failure

%files -f %{name}.lang
%license LICENSE
%doc README.md ROADMAP.md CHANGELOG.md CONTRIBUTING.md SECURITY.md RELEASE_NOTES.md
%{_bindir}/loofi-pulse
%{_bindir}/loofi-pulse-service
%{_datadir}/dbus-1/services/com.github.loofi.hardwarepulse.Collector1.service
%{_libdir}/qt6/qml/com/github/loofi/hardwarepulse/
%{_datadir}/plasma/plasmoids/com.github.loofi.hardwarepulse/
%{_datadir}/knotifications6/plasma_applet_com.github.loofi.hardwarepulse.notifyrc
%{_datadir}/icons/hicolor/scalable/apps/com.github.loofi.hardwarepulse.svg
%{_datadir}/applications/com.github.loofi.hardwarepulse.desktop
%{_kf6_metainfodir}/com.github.loofi.hardwarepulse.metainfo.xml

%changelog
* Sun Oct 25 2026 Loofi <loofi@github.com> - 0.13.0-1
- Power Profiles Daemon (PPD) control, active holds tracking, and profile-throttling causal rule
- Wi-Fi link telemetry (SSID, 2.4/5/6 GHz, dBm, bitrate, duplex) and wireless degradation causal rule
- ZRAM capacity, space saved metrics, and cgroup v2 slice memory attribution (user, system, machine)
- Smart Flight Recorder auto-bookmarks, SQLite v3 schema extensions, and interactive HTML report export
- Modern KDE 6 Glass and Accent Glows UI styling, rotating fan indicators, and Plasmoid alert glows
- Complete Swedish localization (po/sv) and 100% CTest coverage
- Add multi-fan telemetry discovery and RPM progress in Power & Thermal
- Add NVMe controller discovery, critical warning bitmask decoding and composite temperature
- Implement causal diagnosis verdicts for single-core CPU saturation and NVMe critical warnings with KNotification integration
- Add privacy-redacted kernel log correlation (+-15s window) around incidents
- Add side-by-side incident comparison and delta metrics in Flight Recorder
- Add gradient sparklines and direct action buttons on OverviewPage cards
- Add 3 panel display styles in Plasmoid: Text Pill, Mini Sparkline, and Ring Gauge
- Enrich Markdown, HTML and JSON report exports with kernel logs, NVMe health and comparison analysis
* Wed Oct 21 2026 Loofi <loofi@github.com> - 0.12.0-1
- Development stabilization: honest telemetry quality, multi-GPU diagnosis, collector lifecycle and bounded persistent history
- Keep process inspection read-only and delegate process management to KDE System Monitor
- Prepare public contribution, security, validation and least-privilege release workflows
* Mon Oct 19 2026 Loofi <loofi@github.com> - 0.11.0-1
- Major daily-driver release: robust single-instance D-Bus lifecycle, collector auto-reconnect with exponential backoff, logind sleep/resume recovery, dedicated Settings and Preferences Hub, persistent opt-in SQLite WAL telemetry history with trends visualization, system tray integration with live telemetry tooltip and quick actions, safe process termination with UID and PID recycling verification, native Swedish localization (ki18n), and comprehensive accessibility refinements
* Sun Oct 18 2026 Loofi <loofi@github.com> - 0.10.0-1
- Comprehensive UI/UX overhaul: structured Plasma 6 layout, responsive navigation tabs, column-aligned process table, polished storage/network cards, streamlined incident action workflow, and refined Plasmoid widget
* Sun Oct 18 2026 Loofi <loofi@github.com> - 0.9.0-1
- Add a shared session collector, incident investigations and local session files
- Add full process inspection, per-device storage and network data, and multiple GPU and battery support
* Sat Oct 17 2026 Loofi <loofi@github.com> - 0.8.0-1
- Clarify measurement semantics, stabilize diagnoses, harden telemetry identity and history, and simplify application and panel workflows
* Sat Oct 17 2026 Loofi <loofi@github.com> - 0.7.0-1
- Release 0.7.0 with network RX/TX throughput, D-state process I/O wait radar, multi-domain causal contributing signals, configurable 5/15/30-minute history retention, and configurable Plasma 6 panel metrics and sparkline channels
* Sat Oct 17 2026 Loofi <loofi@github.com> - 0.6.0-1
- Release 0.6.0 with Telemetry Studio UI/UX redesign, PulseTheme design system, circular PSI gauges, embedded live sparklines, collision-free History oscilloscope, RAM/ZRAM/Swap pipeline, and refined Plasma 6 widget
* Sat Oct 17 2026 Loofi <loofi@github.com> - 0.5.0-1
- Release 0.5.0 with power-aware causal diagnosis, interactive Overview domain cards, incident timeline chips, structured Power & Cooling cards, and deep-linked Plasma widget navigation
* Sat Oct 17 2026 Loofi <loofi@github.com> - 0.4.0-1
- Release 0.4.0 with incremental telemetry history, adaptive process radar, CPU hwmon power, Intel Xe GPU frequency, and disk I/O throughput
