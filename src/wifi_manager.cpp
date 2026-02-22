#include "wifi_manager.h"

// Internal state
static WiFiServer g_server(WIFI_SERVER_PORT);
static bool g_ipPrinted = false;
static int g_lastStatus = -1;
static unsigned long g_lastHeartbeatMs = 0;

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

void wifi_init(const char *ssid, const char *pass) {
  if (ssid == nullptr || pass == nullptr) {
    return;
  }

  Serial.print("WiFi: Connecting to ");
  Serial.println(ssid);

  WiFi.disconnect();
  delay(100);

  for (int attempt = 1; attempt <= WIFI_MAX_RETRIES; ++attempt) {
    WiFi.begin(ssid, pass);

    unsigned long start = millis();
    int lastStatus = -1;

    while ((millis() - start) < WIFI_ATTEMPT_TIMEOUT_MS) {
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
      g_server.begin();
      return;
    }

    Serial.println();
    Serial.print("  Attempt ");
    Serial.print(attempt);
    Serial.print(" failed (status ");
    Serial.print(finalStatus);
    Serial.println(")");

    if (attempt < WIFI_MAX_RETRIES) {
      Serial.print("  Retrying in ");
      Serial.print(WIFI_RETRY_DELAY_MS / 1000);
      Serial.println("s...");
      delay(WIFI_RETRY_DELAY_MS);
    }
  }

  // All retries exhausted
  Serial.println("WiFi: All connection attempts failed. Scanning networks...");
  scanAndReportNetworks(ssid);
}

void wifi_tick() {
  if (!Serial) {
    g_ipPrinted = false;
    return;
  }

  unsigned long now = millis();
  int status = WiFi.status();
  if (status != g_lastStatus) {
    g_lastStatus = status;
    g_lastHeartbeatMs = now;
    Serial.print("WiFi: ");
    Serial.println(wifiStatusToString(status));
  } else if ((now - g_lastHeartbeatMs) >= WIFI_HEARTBEAT_MS) {
    g_lastHeartbeatMs = now;
    Serial.print("WiFi: ");
    Serial.println(wifiStatusToString(status));
  }

  if (status == WL_CONNECTED) {
    if (!g_ipPrinted) {
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      g_ipPrinted = true;
    }
  } else {
    g_ipPrinted = false;
  }
}

WiFiServer* wifi_get_server() {
  return &g_server;
}
