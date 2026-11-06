# Loofi Pulse v0.15.0 — UI/UX Redesign & Visual Elevation

This is a development candidate for Fedora KDE Plasma 6. It introduces a comprehensive visual redesign across both the desktop application and the Plasma widget, powered by a unified design token system, GPU-accelerated charts, responsive navigation, and complete Swedish localization.

## Changes

### Visual Identity & Design System

- **New App Branding**: Redesigned modern application icon with an ECG pulse wave forming an "L" (`assets/logo.svg` and `assets/logo-symbolic.svg`), adhering to KDE Human Interface Guidelines.
- **Shared Design Tokens (`PulseTheme`)**: Centralized typography scale, standard spacing units, border radiuses, animation curves, and semantic palette colors respecting system Breeze light and dark schemes.
- **Reusable Component Library (`com.github.loofi.hardwarepulse`)**: Modern elevated surfaces (`PulseCard`), status badges (`StatusPill`), circular gauges (`PulseRingGauge`), trendlines (`PulseSparkline`), interactive multi-series charts (`PulseAreaChart`), metric tiles (`MetricCard`), and diagnostic findings rows (`FindingRow`).

### Desktop Application

- **Responsive Sidebar Navigation**: Collapsible navigation rail (`PulseSidebar`) with smooth width transitions (240px expanded to 68px collapsed on narrow viewports or toggle), categorized sections, live subsystem severity badges, and full keyboard navigation.
- **Redesigned Overview Page**: Features the 6-domain `HealthHero` ring with diagnostic verdict chips, a 2x3 responsive `MetricCard` grid with live trendlines, and a sorted diagnostic findings card.
- **Standardized Detail Pages**: `PowerThermalPage`, `PressureMemoryPage`, and `StorageNetworkPage` upgraded with `PulsePageHeader` (5/15/30-minute time window selector and incident trigger), hero `PulseAreaChart` time series, elevated device cards, and animated fan indicators.
- **Process & Flight Recorder Views**: Elevated search/sort toolbar, improved process table contrast, standardized header strip, and incident timeline markers.

### Plasma 6 Widget

- **Panel Representations**: Integrated `PulseRingGauge` (Ring gauge mode) and `PulseSparkline` (Sparkline mode) with smooth GPU-accelerated path rendering.
- **Popup Overview**: Elevated popup cards with `PulseCard` surfaces, theme integration, and smooth layout.

### Localization & Compatibility

- **Swedish Localization**: 100% complete Swedish translations (`po/sv/`) across the desktop application and Plasma widget.
- **Compatibility**: Preserves `.pulse-session` format 1, JSON report schema 2, Collector1 D-Bus API, and existing settings. Routine monitoring remains local and read-only.

## Validation status

Automated validation is verified via `scripts/validate.sh`, including QML linting, AppStream metadata validation, and test suite execution. This remains a development candidate.
