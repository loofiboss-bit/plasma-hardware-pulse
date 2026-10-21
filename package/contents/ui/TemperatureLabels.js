.pragma library

function formatTemperature(celsius, unit) {
    if (typeof celsius !== "number" || !isFinite(celsius)) return "N/A"
    return Math.round(unit === "F" ? celsius * 9 / 5 + 32 : celsius) + (unit === "F" ? "°F" : "°C")
}

function formatTemperatureDelta(celsius, unit) {
    if (typeof celsius !== "number" || !isFinite(celsius)) return "N/A"
    return Math.round(unit === "F" ? celsius * 9 / 5 : celsius) + (unit === "F" ? "°F" : "°C")
}
