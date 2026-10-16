# Loofi Pulse (`plasma-hardware-pulse`)

**Loofi Pulse** is a causal and topological hardware monitor for **KDE Plasma 6**, built with **C++20, Qt6/QML, libplasma, and Kirigami**.

Instead of dumping dozens of disconnected raw numbers, Loofi Pulse correlates **driving process ➔ power draw / PSI stall ➔ clock frequency / throttling ➔ thermal headroom** in real time.

## Architecture & Key Capabilities (v0.2.0)

1. **Causal Bottleneck & Thermal Diagnosis (`CausalDiagnosisEngine`)**:
   - Computes **Thermal Headroom ($\Delta T$)** relative to hardware `temp*_max`/`crit` limits alongside **Thermal Velocity ($\frac{dT}{dt}$ in °C/s)**.
   - Evaluates **CPU Thermal Throttling**, **Linux Memory/I/O PSI Bottlenecks**, and **GPU Compute/Thermal Saturation** (including DRM VRAM & GPU Power).
   - Attributes CPU/thermal spikes to the top CPU consumer and **Memory PSI stalls to the top RAM/RSS consumer**, with 1-click **Process Termination (`SIGTERM`)** directly from the UI.
2. **Linux PSI (Pressure Stall Information), ZRAM & Top-5 Process Radar (`PsiMemorySampler` & `CpuSocSampler`)**:
   - Reads `/proc/pressure/{cpu,memory,io}` (`some`, `full`, and `ms/s` stall rates) to measure *actual user-perceivable stall time* rather than raw RAM allocation.
   - Parses `/sys/block/zram0/mm_stat` and `/proc/meminfo` to display real-time **ZRAM compression ratio**, allocator overhead, physical RAM saved, and Swap usage alongside a live **Top-5 CPU & Memory RSS Process Radar**.
3. **Energy Flow, Power Control & Battery Health (`PowerFlowSampler`)**:
   - Displays Package/Battery Watts, **Hardware Exact vs Estimated RAPL** status, Intel RAPL PL1/PL2 power limits, **Primary Fan RPM**, **Battery ETA (time remaining)**, and **Battery Health / Wear %**.
   - Provides non-blocking cached D-Bus polling and 1-click **Power Profile** switching (`power-saver`, `balanced`, `performance`) over `net.hadess.PowerProfiles`.
4. **Flight Recorder 2.0 with Incident Markers & Report Export (`FlightRecorder`)**:
   - Incremental 5-minute history buffer (configurable `500ms` / `1000ms` / `2000ms` poll rate + Pause/Resume) with toggleable multi-channel traces (**CPU Temp**, **CPU Load**, **PSI Score**, **System Power**) and visual **Throttle/Bottleneck bands**.
   - 1-click **Markdown & JSON Diagnostic Report Export** to clipboard or `~/.local/share/loofi-pulse/reports/`.

## Build, Test, and Run

```bash
# Configure and build
cmake -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)

# Run unit tests + headless QML smoke test
ctest --test-dir build --output-on-failure

# Launch the standalone Kirigami Studio application
QML_IMPORT_PATH="$PWD/build/bin" ./build/bin/loofi-pulse
```
