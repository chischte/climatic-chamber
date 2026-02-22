#include "wifi_manager.h"

static const char *wifiStatusToString(int s) {
  switch (s) {
  case WL_NO_SHIELD:
    return "WL_NO_SHIELD";
  case WL_IDLE_STATUS:
    return "WL_IDLE_STATUS";
  case WL_NO_SSID_AVAIL:
    return "WL_NO_SSID_AVAIL";
  case WL_SCAN_COMPLETED:
    return "WL_SCAN_COMPLETED";
  case WL_CONNECTED:
    return "WL_CONNECTED";
  case WL_CONNECT_FAILED:
    return "WL_CONNECT_FAILED";
  case WL_CONNECTION_LOST:
    return "WL_CONNECTION_LOST";
  case WL_DISCONNECTED:
    return "WL_DISCONNECTED";
  default:
    return "WL_UNKNOWN";
  }
}

static void scanAndReportNetworks(const char *target_ssid) {
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    Serial.println("No networks found.");
    return;
  }
  bool found = false;
  for (int i = 0; i < n; ++i) {
    String ss = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    Serial.print(i);
    Serial.print(": ");
    Serial.print(ss);
    Serial.print(" (RSSI ");
    Serial.print(rssi);
    Serial.println(")");
    if (ss == target_ssid) {
      found = true;
      Serial.print("Found target SSID with RSSI: ");
      Serial.println(rssi);
    }
  }
  if (!found) {
    Serial.print("Target SSID '");
    Serial.print(target_ssid);
    Serial.println("' not found.");
  }
}

void wifi_connect(const WifiConfig *config) {
  if (config == nullptr) {
    return;
  }

  Serial.print("WiFi: Connecting to ");
  Serial.println(config->ssid);

  WiFi.disconnect();
  delay(100);

  for (int attempt = 1; attempt <= config->max_retries; ++attempt) {
    WiFi.begin(config->ssid, config->pass);

    unsigned long start = millis();
    int lastStatus = -1;

    while ((millis() - start) < config->attempt_timeout_ms) {
      int s = WiFi.status();
      if (s == WL_CONNECTED) {
        break;
      }
      if (s != lastStatus) {
        Serial.print("  Status (");
        Serial.print(wifiStatusToString(s));
        Serial.println(")");
        lastStatus = s;
      } else {
        Serial.print('.');
      }
      delay(500);
    }

    int finalStatus = WiFi.status();
    if (finalStatus == WL_CONNECTED) {
      Serial.println();
      Serial.print("Connected! IP: ");
      Serial.println(WiFi.localIP());
      if (config->server != nullptr) {
        config->server->begin();
      }
      return;
    }

    Serial.println();
    Serial.print("  Attempt ");
    Serial.print(attempt);
    Serial.print(" failed (status ");
    Serial.print(finalStatus);
    Serial.println(")");

    if (attempt < config->max_retries) {
      Serial.print("  Retrying in ");
      Serial.print(config->retry_delay_ms / 1000);
      Serial.println("s...");
      delay(config->retry_delay_ms);
    }
  }

  // All retries exhausted
  Serial.println("WiFi: All connection attempts failed. Scanning networks...");
  scanAndReportNetworks(config->ssid);
}

void wifi_serial_ticker(const WifiConfig *config, WifiState *state) {
  if (config == nullptr || state == nullptr) {
    return;
  }

  if (!Serial) {
    state->ip_printed = false;
    return;
  }

  unsigned long now = millis();
  int status = WiFi.status();
  if (status != state->last_status) {
    state->last_status = status;
    state->last_heartbeat_ms = now;
    Serial.print("WiFi: ");
    Serial.println(wifiStatusToString(status));
  } else if ((now - state->last_heartbeat_ms) >= config->heartbeat_ms) {
    state->last_heartbeat_ms = now;
    Serial.print("WiFi: ");
    Serial.println(wifiStatusToString(status));
  }

  if (status == WL_CONNECTED) {
    if (!state->ip_printed) {
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      state->ip_printed = true;
    }
  } else {
    state->ip_printed = false;
  }
}
