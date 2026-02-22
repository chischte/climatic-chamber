#pragma once

#include <Arduino.h>
#include <WiFi.h>

typedef void (*WebIncrementHandler)();

typedef struct {
    WiFiServer *server;
    uint16_t *values;
    size_t values_len;
    WebIncrementHandler on_increment;
} WebServerConfig;

void web_server_handle(const WebServerConfig *config);
