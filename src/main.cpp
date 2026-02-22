/*
 * *****************************************************************************
 * CLIMATIC CHAMBER
 * *****************************************************************************
 * Closed-loop control of temperature, relative humidity, and CO₂ levels
 * with fresh air management, implemented on an Arduino Portenta Machine
 * Control platform.
 * *****************************************************************************
 */

#include "controller.h"
#include "credentials.h"
#include "storage.h"
#include "web_server.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <Arduino_PortentaMachineControl.h>

// Configuration
static constexpr unsigned long SERIAL_BAUD = 115200;

// Web server configuration
static WebServerConfig g_webConfig = {};

// Callback for web server increment action
static void onIncrement();

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart) < 3000UL) {
    delay(10);
  }
  Serial.println("Booting...");
  
  // Initialize storage and load persisted data
  storage_init();
  storage_load();
  
  // Initialize climate chamber controller
  controller_init();
  
  // Configure web server
  g_webConfig = {wifi_get_server(), storage_get_values_mutable(), 
                 storage_get_num_values(), onIncrement};
  
  // Initialize WiFi and connect
  wifi_init(WIFI_SSID, WIFI_PASS);
  
  Serial.println("Setup complete");
}
// ==================== END SETUP ====================

void loop() {
  controller_tick();  // Climate chamber control
  wifi_tick();
  web_server_handle(&g_webConfig);
  storage_tick();
}
// Callback for web server increment action
static void onIncrement() {
  storage_increment_value(0);
}
