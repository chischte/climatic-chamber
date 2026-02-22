#pragma once

#include <Arduino.h>
#include <WiFi.h>

// WiFi Configuration Constants
static constexpr uint16_t WIFI_SERVER_PORT = 80;
static constexpr int WIFI_MAX_RETRIES = 3;
static constexpr unsigned long WIFI_ATTEMPT_TIMEOUT_MS = 20000UL;
static constexpr unsigned long WIFI_RETRY_DELAY_MS = 2000UL;
static constexpr unsigned long WIFI_HEARTBEAT_MS = 30000UL;

// Initialize WiFi and server (call once in setup)
void wifi_init(const char *ssid, const char *pass);

// Periodic tick for WiFi status monitoring (call in loop)
void wifi_tick();

// Get server instance for web server module
WiFiServer* wifi_get_server();
