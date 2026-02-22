/*
 * *****************************************************************************
 * CLIMATE CHAMBER CONTROLLER
 * *****************************************************************************
 * Non-blocking control system with simulated sensors (10x speedup for testing)
 * - Ring buffers for last 200 samples (RH, Temp, CO2)
 * - Non-preemptive action state machine
 * - Measurement cycle with swirl/median/evaluate/wait stages
 * *****************************************************************************
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

// --- Configuration ---
#define SIMULATE_SENSORS 1  // 1 = simulated sensors, 0 = real sensors

static constexpr uint8_t SPEEDUP = 10;  // 10x faster than real-time for testing

// Ring buffer size
static constexpr uint16_t RING_BUFFER_SIZE = 100;

// --- Data structures ---

// Sensor readings
struct Sensors {
  int co2;      // ppm
  float rh;     // % relative humidity
  float temp;   // °C temperature
};

// --- API Functions ---

// Initialize controller (call once in setup)
void controller_init();

// Periodic tick (call in loop)
void controller_tick();

// Get last 100 samples for API/plotting
// Arrays must have space for 100 elements
void controller_get_last200(float *rh_out, float *temp_out, int *co2_out);

// CO2 Setpoint management
void controller_set_co2_setpoint(uint16_t ppm);
uint16_t controller_get_co2_setpoint();

// RH Setpoint management
void controller_set_rh_setpoint(float percent);
float controller_get_rh_setpoint();

// Temperature Setpoint management
void controller_set_temp_setpoint(float celsius);
float controller_get_temp_setpoint();
