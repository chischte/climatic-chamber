/*
 * *****************************************************************************
 * CLIMATE CHAMBER CONTROLLER - PUBLIC API
 * *****************************************************************************
 * Non-blocking climate control system with:
 * - Multi-sensor monitoring (CO2, humidity, temperature)
 * - Ring buffer data collection (200 samples per sensor)
 * - Non-preemptive action state machine
 * - Measurement cycle with median filtering
 * - Independent heater control
 * 
 * Configuration:
 * - SIMULATE_SENSORS: Use simulated sensors (defined in config.h)
 * - SPEEDUP_FACTOR: Time acceleration for testing
 * 
 * Control Loop:
 * - Reads sensors continuously
 * - Calculates median values for decision-making
 * - Executes control actions (fogger, fresh air, heater)
 * - Stores data in ring buffers for web visualization
 * *****************************************************************************
 */

#pragma once

#include "config.h"
#include <stdint.h>

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Sensor readings from all sensors
 * 
 * Contains readings from 7 sensors:
 * - 2 CO2 sensors (main + secondary)
 * - 2 humidity sensors (main + secondary)  
 * - 3 temperature sensors (main + secondary + outer)
 */
struct Sensors {
  int co2;           ///< CO2 concentration (ppm) from main sensor
  int co2_2;         ///< CO2 concentration (ppm) from secondary sensor
  float rh;          ///< Relative humidity (%) from main sensor
  float rh_2;        ///< Relative humidity (%) from secondary sensor
  float temp;        ///< Temperature (°C) from main inner sensor
  float temp_2;      ///< Temperature (°C) from secondary inner sensor
  float temp_outer;  ///< Temperature (°C) from outer box sensor
};

// =============================================================================
// CONTROLLER API
// =============================================================================

/**
 * @brief Initialize the climate controller
 * 
 * Must be called once during setup before any other controller functions.
 * Initializes sensors, actuators, and internal state machines.
 */
void controller_init();

/**
 * @brief Execute one iteration of the control loop
 * 
 * Call this repeatedly in the main loop. Performs non-blocking operations:
 * - Reads sensors
 * - Updates ring buffers
 * - Executes control decisions
 * - Manages actuator states
 */
void controller_tick();

// =============================================================================
// DATA RETRIEVAL
// =============================================================================

/**
 * @brief Get last 200 samples from primary sensors
 * 
 * @param rh_out    Output array for humidity data (must have 200 elements)
 * @param temp_out  Output array for temperature data (must have 200 elements)
 * @param co2_out   Output array for CO2 data (must have 200 elements)
 * 
 * @note Returns data oldest to newest. If buffer not full, pads with zeros.
 */
void controller_get_last200(float *rh_out, float *temp_out, int *co2_out);

/**
 * @brief Get last 200 samples from additional sensors
 * 
 * @param co2_2_out      Output array for secondary CO2 (200 elements)
 * @param rh_2_out       Output array for secondary humidity (200 elements)
 * @param temp_2_out     Output array for secondary temperature (200 elements)
 * @param temp_outer_out Output array for outer temperature (200 elements)
 */
void controller_get_additional_sensors(int *co2_2_out, float *rh_2_out, 
                                       float *temp_2_out, float *temp_outer_out);

/**
 * @brief Get last 200 output states for primary actuators
 * 
 * @param fogger_out    Output array for fogger states (0=OFF, 1=ON)
 * @param swirler_out   Output array for swirler states (0=OFF, 1=ON)
 * @param freshair_out  Output array for fresh air states (0=OFF, 1=ON)
 */
void controller_get_outputs(int *fogger_out, int *swirler_out, int *freshair_out);

/**
 * @brief Get last 200 heater states
 * 
 * @param heater_out Output array for heater states (0=OFF, 1=ON)
 */
void controller_get_heater(int *heater_out);

// =============================================================================
// SETPOINT MANAGEMENT
// =============================================================================

/**
 * @brief Set CO2 target level
 * @param ppm Target CO2 concentration (400-10000 ppm)
 */
void controller_set_co2_setpoint(uint16_t ppm);

/**
 * @brief Get current CO2 setpoint
 * @return Target CO2 concentration in ppm
 */
uint16_t controller_get_co2_setpoint();

/**
 * @brief Set humidity target level
 * @param percent Target relative humidity (82-96 %)
 */
void controller_set_rh_setpoint(float percent);

/**
 * @brief Get current humidity setpoint
 * @return Target relative humidity in percent
 */
float controller_get_rh_setpoint();

/**
 * @brief Set temperature target level
 * @param celsius Target temperature (18-32 °C)
 */
void controller_set_temp_setpoint(float celsius);

/**
 * @brief Get current temperature setpoint
 * @return Target temperature in degrees Celsius
 */
float controller_get_temp_setpoint();
