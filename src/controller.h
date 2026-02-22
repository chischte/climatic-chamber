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
static constexpr uint16_t RING_BUFFER_SIZE = 200;

// --- Data structures ---

// Sensor readings
struct Sensors {
  int co2;         // ppm (main)
  int co2_2;       // ppm (second sensor)
  float rh;        // % relative humidity (main)
  float rh_2;      // % relative humidity (second sensor)
  float temp;      // °C temperature (main)
  float temp_2;    // °C temperature (second sensor)
  float temp_outer;// °C temperature (outer box)
};

// --- API Functions ---

// Initialize controller (call once in setup)
void controller_init();

// Periodic tick (call in loop)
void controller_tick();

// Get last 100 samples for API/plotting
// Arrays must have space for 100 elements
void controller_get_last200(float *rh_out, float *temp_out, int *co2_out);

// Get additional sensors (CO2_2, RH_2, temp_2, temp_outer)
void controller_get_additional_sensors(int *co2_2_out, float *rh_2_out, float *temp_2_out, float *temp_outer_out);

// Get last 100 output states (0=OFF, 1=ON)
void controller_get_outputs(int *fogger_out, int *swirler_out, int *freshair_out);

// Get last 100 heater states (0=OFF, 1=ON)
void controller_get_heater(int *heater_out);

// CO2 Setpoint management
void controller_set_co2_setpoint(uint16_t ppm);
uint16_t controller_get_co2_setpoint();

// RH Setpoint management
void controller_set_rh_setpoint(float percent);
float controller_get_rh_setpoint();

// Temperature Setpoint management
void controller_set_temp_setpoint(float celsius);
float controller_get_temp_setpoint();
