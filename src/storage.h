/*
 * *****************************************************************************
 * STORAGE MODULE
 * *****************************************************************************
 * High-level storage API for application data with ring buffer persistence
 * Handles flash-backed or RAM-backed storage with wear-leveling
 * *****************************************************************************
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

// Storage Configuration
static constexpr unsigned long PERSIST_INTERVAL_MS = 5000UL;
static constexpr uint8_t NUM_VALUES = 10;

// Initialize storage system (call once in setup)
void storage_init();

// Load persisted data from storage (call once after init)
void storage_load();

// Periodic tick function (call in loop to handle auto-persistence)
void storage_tick();

// Get pointer to values array (read-only access)
const uint16_t* storage_get_values();

// Get mutable pointer to values array (for web server)
uint16_t* storage_get_values_mutable();

// Get number of values
uint8_t storage_get_num_values();

// Increment a value and mark for persistence
void storage_increment_value(uint8_t index);

// Set a value and mark for persistence
void storage_set_value(uint8_t index, uint16_t value);

// Force immediate save (normally called automatically by storage_tick)
void storage_save_now();

// CO2 Setpoint management (stored in values[1])
void storage_set_co2_setpoint(uint16_t ppm);
uint16_t storage_get_co2_setpoint();

// RH Setpoint management (stored in values[2], scaled by 10: 940 = 94.0%)
void storage_set_rh_setpoint(float percent);
float storage_get_rh_setpoint();

// Temperature Setpoint management (stored in values[3], scaled by 10: 250 = 25.0°C)
void storage_set_temp_setpoint(float celsius);
float storage_get_temp_setpoint();
