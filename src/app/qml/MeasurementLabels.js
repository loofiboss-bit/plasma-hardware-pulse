.pragma library

function metricLabel(key) {
    const labels = {
        cpuLoad: "CPU load", cpuTemperature: "CPU temperature", thermalHeadroom: "Thermal headroom",
        gpuLoad: "GPU activity", gpuTemperature: "GPU temperature", cpuPressure: "Raw CPU pressure",
        memoryPressure: "Raw memory pressure", ioPressure: "Raw I/O pressure", pressureIndex: "Responsiveness index",
        memoryUsage: "Memory use", zram: "Compressed memory (zram)", diskThroughput: "Storage throughput",
        netThroughput: "Network throughput", packagePower: "CPU package power", fanSpeed: "Fan speed",
        battery: "Battery status", batteryPower: "Battery power", batteryDischarge: "Battery discharge",
        processes: "Process observations", processActivity: "Process observations", powerProfile: "Power profile"
    }
    return labels[key] || String(key).replace(/([a-z])([A-Z])/g, "$1 $2").replace(/^./, function(c) { return c.toUpperCase() })
}

function qualityLabel(state) {
    const labels = {
        available: "Available", unavailable: "Unavailable", calculated: "Calculated", estimated: "Estimated",
        "warming-up": "Warming up", "not-active": "Not active", "not-present": "Not present",
        stale: "Reading is old", paused: "Paused", disconnected: "Disconnected"
    }
    return labels[state] || String(state || "Unavailable").replace(/-/g, " ").replace(/^./, function(c) { return c.toUpperCase() })
}

function formatTemperature(celsius, unit) {
    if (typeof celsius !== "number" || isNaN(celsius)) return "N/A"
    if (unit === "F") {
        const f = (celsius * 9 / 5) + 32
        return Math.round(f) + "°F"
    }
    return Math.round(celsius) + "°C"
}

// Temperature differences do not include the Fahrenheit zero-point offset.
function formatTemperatureDelta(celsius, unit) {
    if (typeof celsius !== "number" || !isFinite(celsius)) return "N/A"
    return Math.round(unit === "F" ? celsius * 9 / 5 : celsius) + (unit === "F" ? "°F" : "°C")
}
