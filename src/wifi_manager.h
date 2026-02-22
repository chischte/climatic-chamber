#pragma once

#include <Arduino.h>
#include <WiFi.h>

typedef struct {
    const char *ssid;
    const char *pass;
    int max_retries;
    unsigned long attempt_timeout_ms;
    unsigned long retry_delay_ms;
    unsigned long heartbeat_ms;
    WiFiServer *server;
} WifiConfig;

typedef struct {
    bool ip_printed;
    int last_status;
    unsigned long last_heartbeat_ms;
} WifiState;

void wifi_connect(const WifiConfig *config);

void wifi_serial_ticker(const WifiConfig *config, WifiState *state);
