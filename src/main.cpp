/*
 * *****************************************************************************
 * CLIMATIC CHAMBER - MAIN ENTRY POINT
 * *****************************************************************************
 * Closed-loop control of temperature, relative humidity, and CO₂ levels
 * with fresh air management, implemented on an Arduino Portenta Machine
 * Control platform.
 * 
 * Architecture:
 * - Non-blocking control loops
 * - Persistent storage with flash ring buffer
 * - WiFi-enabled web interface
 * - Simulated sensors for testing (10x speedup)
 * *****************************************************************************
 */

#include "config.h"
#include "controller.h"
#include "credentials.h"
#include "storage.h"
#include "web_server.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <Arduino_PortentaMachineControl.h>

// Web server configuration
static WebServerConfig g_webConfig = {};

/**
 * @brief Initialize all subsystems
 * 
 * Order of initialization:
 * 1. Serial communication for debugging
 * 2. Storage system and load persisted data
 * 3. Climate chamber controller
 * 4. WiFi connection
 */
void setup() {
  // Initialize serial communication
  Serial.begin(Config::SERIAL_BAUD_RATE);
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart) < Config::SERIAL_TIMEOUT_MS) {
    delay(10);
  }
  Serial.println(F("=== Climatic Chamber Control System ==="));
  Serial.println(F("Initializing..."));
  
  // Initialize storage and load persisted data
  Serial.print(F("Storage... "));
  storage_init();
  storage_load();
  Serial.println(F("OK"));
  
  // Initialize climate chamber controller
  Serial.print(F("Controller... "));
  controller_init();
  Serial.println(F("OK"));
  
  // Configure web server
  g_webConfig = {
    wifi_get_server(), 
    storage_get_values_mutable(), 
    storage_get_num_values(), 
    nullptr  // No increment callback needed
  };
  
  // Initialize WiFi and connect
  Serial.print(F("WiFi... "));
  wifi_init(WIFI_SSID, WIFI_PASS);
  Serial.println(F("OK"));
  
  Serial.println(F("=== System Ready ==="));
  Serial.println();
}

/**
 * @brief Main control loop
 * 
 * Executes non-blocking tick functions for all subsystems:
 * - Climate control (sensor reading, actuator control)
 * - WiFi connection management
 * - Web server request handling
 * - Storage persistence
 */
void loop() {
  controller_tick();
  wifi_tick();
  web_server_handle(&g_webConfig);
  storage_tick();
}
