/*
 * *****************************************************************************
 * CONFIGURATION
 * *****************************************************************************
 * Central configuration file for all system constants
 * Organizes magic numbers and configuration values in one location
 * *****************************************************************************
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

// =============================================================================
// SYSTEM CONFIGURATION
// =============================================================================

namespace Config {

// --- Testing & Simulation ---
constexpr bool SIMULATE_SENSORS = true;      // Use simulated sensors for testing
constexpr uint8_t SPEEDUP_FACTOR = 10;       // 10x faster than real-time
constexpr unsigned long SERIAL_BAUD_RATE = 115200;
constexpr unsigned long SERIAL_TIMEOUT_MS = 3000;

// --- Storage ---
constexpr unsigned long PERSIST_INTERVAL_MS = 5000;  // Auto-save every 5 seconds
constexpr uint8_t STORAGE_NUM_VALUES = 10;
constexpr uint32_t FLASH_RING_BUFFER_SLOTS = 100;
constexpr uint32_t FLASH_SLOT_SIZE_BYTES = 64;

// --- Data Collection ---
constexpr uint16_t SENSOR_RING_BUFFER_SIZE = 200;    // 200 samples per sensor
constexpr unsigned long SAMPLE_INTERVAL_MS = 3000;   // Sample every 3 seconds

// =============================================================================
// TIMING CONSTANTS
// =============================================================================

// --- Real-Time Operations (not scaled) ---
constexpr unsigned long POLL_INTERVAL_MS = 200;      // UI poll rate

// --- Control Loop Timings (scaled by SPEEDUP_FACTOR) ---
constexpr unsigned long MEDIAN_DURATION_MS = 5000;         // 5s median sampling
constexpr unsigned long SWIRL_DURATION_MS = 10000;         // 10s air mixing
constexpr unsigned long WAIT_AFTER_SWIRL_MS = 10000;       // 10s settling time
constexpr unsigned long FRESHAIR_DURATION_MS = 60000;      // 60s fresh air
constexpr unsigned long POST_FRESHAIR_WAIT_MS = 60000;     // 60s wait
constexpr unsigned long FOGGER_DURATION_MS = 3000;         // 3s fog injection
constexpr unsigned long POST_FOGGER_WAIT_MS = 60000;       // 60s wait

// --- Heater Control ---
constexpr unsigned long HEATER_CHECK_INTERVAL_MS = 1000;   // Check every second

// =============================================================================
// SENSOR CONFIGURATION
// =============================================================================

// --- Simulated Sensor Ranges ---
namespace  Simulation {
  // CO2 Sensors (ppm)
  constexpr int CO2_MIN = 450;
  constexpr int CO2_MAX = 3000;
  constexpr int CO2_INITIAL_MAIN = 800;
  constexpr int CO2_INITIAL_SECONDARY = 820;
  constexpr int CO2_PULSE_MAGNITUDE = 500;
  constexpr float CO2_PULSE_PROBABILITY = 0.005f;    // 0.5% per sample
  constexpr float CO2_SECONDARY_PULSE_PROBABILITY = 0.003f;  // 0.3% per sample
  constexpr uint8_t CO2_PULSE_DURATION_SAMPLES = 10;
  
  // Humidity Sensors (%)
  constexpr float RH_MIN = 85.0f;
  constexpr float RH_MAX = 99.5f;
  constexpr float RH_INITIAL_MAIN = 92.0f;
  constexpr float RH_INITIAL_SECONDARY = 90.5f;
  constexpr float RH_DRIFT_RATE = 0.3f;
  constexpr float RH_NOISE_AMPLITUDE = 0.5f;
  
  // Temperature Sensors (°C)
  constexpr float TEMP_INNER_MIN = 18.0f;
  constexpr float TEMP_INNER_MAX = 35.0f;
  constexpr float TEMP_INNER_INITIAL_MAIN = 25.0f;
  constexpr float TEMP_INNER_INITIAL_SECONDARY = 24.0f;
  constexpr float TEMP_INNER_DRIFT_RATE = 0.2f;
  constexpr float TEMP_INNER_NOISE_AMPLITUDE = 0.3f;
  
  constexpr float TEMP_OUTER_MIN = 15.0f;
  constexpr float TEMP_OUTER_MAX = 32.0f;
  constexpr float TEMP_OUTER_INITIAL = 22.0f;
  constexpr float TEMP_OUTER_DRIFT_RATE = 0.2f;
  constexpr float TEMP_OUTER_NOISE_AMPLITUDE = 0.3f;
}

// --- Sensor Averaging ---
constexpr uint8_t MEDIAN_SAMPLE_COUNT = 10;

// =============================================================================
// CONTROL PARAMETERS
// =============================================================================

// --- CO2 Control ---
namespace CO2 {
  constexpr uint16_t SETPOINT_MIN = 400;
  constexpr uint16_t SETPOINT_MAX = 10000;
  constexpr uint16_t SETPOINT_DEFAULT = 400;
  constexpr uint16_t SETPOINT_STEP = 100;
  constexpr int CONTROL_THRESHOLD = 100;              // Trigger action if off by >100ppm
}

// --- Humidity Control ---
namespace Humidity {
  constexpr float SETPOINT_MIN = 82.0f;
  constexpr float SETPOINT_MAX = 96.0f;
  constexpr float SETPOINT_DEFAULT = 96.0f;
  constexpr float SETPOINT_STEP = 1.0f;
  constexpr float HYSTERESIS_BAND = 2.0f;            // ±2% hysteresis
}

// --- Temperature Control ---
namespace Temperature {
  constexpr float SETPOINT_MIN = 18.0f;
  constexpr float SETPOINT_MAX = 32.0f;
  constexpr float SETPOINT_DEFAULT = 30.0f;
  constexpr float SETPOINT_STEP = 1.0f;
  constexpr float HEATER_ON_THRESHOLD = 1.0f;        // Turn on 1°C below setpoint
  constexpr float HEATER_OFF_THRESHOLD = 0.0f;       // Turn off at setpoint
}

// =============================================================================
// WEB INTERFACE CONFIGURATION
// =============================================================================

namespace WebUI {
  constexpr unsigned long CACHE_DURATION_MS = 1000;   // Cache API responses for 1s
  constexpr uint16_t HTTP_PORT = 80;
  
  // Chart Configuration
  constexpr uint16_t CHART_HEIGHT_PX = 150;
  constexpr uint16_t STATUS_CHART_HEIGHT_PX = 40;
  constexpr uint16_t CHART_MAX_WIDTH_PX = 900;
  constexpr uint16_t CHART_UPDATE_INTERVAL_MS = 3000;
  
  // Chart Colors (hex strings without #)
  namespace Colors {
    constexpr const char* CO2_MAIN = "f44336";        // Red
    constexpr const char* CO2_SECONDARY = "e91e63";   // Pink
    constexpr const char* RH_MAIN = "2196F3";         // Blue
    constexpr const char* RH_SECONDARY = "64B5F6";    // Light blue
    constexpr const char* TEMP_MAIN = "4CAF50";       // Green
    constexpr const char* TEMP_SECONDARY = "66BB6A";  // Medium green
    constexpr const char* TEMP_OUTER = "8BC34A";      // Light green
    constexpr const char* FOGGER = "9C27B0";          // Purple
    constexpr const char* SWIRLER = "FF9800";         // Orange
    constexpr const char* FRESHAIR = "00BCD4";        // Cyan
    constexpr const char* HEATER = "FF5722";          // Red-orange
  }
  
  // Value Formatting
  constexpr int CO2_ROUNDING_PPM = 50;                // Round CO2 to nearest 50ppm
  constexpr int RH_TEMP_DECIMAL_PLACES = 1;           // 1 decimal for RH and temp
}

} // namespace Config
