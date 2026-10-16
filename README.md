# Loofi Pulse (`plasma-hardware-pulse`)

**Loofi Pulse** is a causal and topological hardware monitor for **KDE Plasma 6**, built with **C++20, Qt6/QML, libplasma, and Kirigami**.

Instead of dumping dozens of disconnected raw numbers, Loofi Pulse correlates **driving process ➔ power draw / PSI stall ➔ clock frequency / throttling ➔ thermal headroom** in real time.

## Architecture & Key Capabilities

1. **Causal Bottleneck & Thermal Diagnosis (`CausalDiagnosisEngine`)**:
   - Computes **Thermal Headroom ($\Delta T$)** relative to hardware `temp*_max`/`crit` limits alongside **Thermal Velocity ($\frac{dT}{dt}$ in °C/s)**.
   - Reads hardware `thermal_throttle` counters (`/sys/devices/system/cpu/cpu*/thermal_throttle/*_throttle_count`) and attributes spikes to the top active process in `/proc/[pid]/stat`.
2. **Linux PSI (Pressure Stall Information) & ZRAM Pipeline (`PsiMemorySampler`)**:
   - Reads `/proc/pressure/{cpu,memory,io}` (`some` and `full`) to measure *actual user-perceivable stall time* rather than raw RAM allocation.
   - Parses `/sys/block/zram0/mm_stat` to display real-time **ZRAM compression ratio** (e.g. `4.1× compression`) and physical RAM saved.
3. **Energy Flow & Power Control (`PowerFlowSampler`)**:
   - Displays Package/Battery Watts, Intel RAPL PL1/PL2 power limits, and provides 1-click **Power Profile** switching (`power-saver`, `balanced`, `performance`) over D-Bus (`net.hadess.PowerProfiles`).
4. **Flight Recorder with Spike Attribution (`FlightRecorder`)**:
   - Circular 5-minute history buffer (1-second resolution) recording both telemetry and the top driving process at every sample. Click or drag anywhere on the timeline to inspect historical spikes.

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
