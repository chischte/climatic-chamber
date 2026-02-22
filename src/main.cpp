/*
 * *****************************************************************************
 * CLIMATIC CHAMBER
 * *****************************************************************************
 * Closed-loop control of temperature, relative humidity, and CO₂ levels
 * with fresh air management, implemented on an Arduino Portenta Machine
 * Control platform.
 * *****************************************************************************
 */

#include <Arduino.h>
#include <Arduino_PortentaMachineControl.h>
#include <WiFi.h>
#include <stdint.h>

// Configuration
static constexpr const char *WIFI_SSID = "mueschbache";
static constexpr const char *WIFI_PASS = "Schogg1chueche";
static constexpr uint16_t SERVER_PORT = 80;
static constexpr unsigned long SERIAL_BAUD = 115200;
static constexpr unsigned long SERIAL_INTERVAL_MS = 1000UL;
static constexpr int WIFI_MAX_RETRIES = 3;
static constexpr unsigned long WIFI_ATTEMPT_TIMEOUT_MS = 15000UL;
static constexpr unsigned long WIFI_RETRY_DELAY_MS = 2000UL;

// Networking
static WiFiServer server(SERVER_PORT);

// Application state
static volatile uint32_t g_counter = 0; // RAM-only counter

// Timing
static unsigned long g_lastSerialMs = 0;

// Helpers
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

// Forward declarations (small functions)
static void connectWiFi();
static void scanAndReportNetworks();
static void startHttpServerIfConnected();
static void serialTicker();
static void handleHttpClients();
static void serveIndex(WiFiClient &client);
static void handleIncrement(WiFiClient &client);

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(100);
  connectWiFi();
  // Counter is RAM-only; initialize to 0 for clarity
  g_counter = 0;
}

void loop() {
  serialTicker();
  handleHttpClients();
}

// ---------- WiFi + Server ----------
static void connectWiFi() {
  Serial.print("Connecting to WiFi '");
  Serial.print(WIFI_SSID);
  Serial.println("'...");

  for (int attempt = 1; attempt <= WIFI_MAX_RETRIES; ++attempt) {
    Serial.print("WiFi attempt ");
    Serial.print(attempt);
    Serial.print("/");
    Serial.println(WIFI_MAX_RETRIES);

    WiFi.disconnect(); // reset any previous state
    delay(500);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long start = millis();
    int prev = WiFi.status();
    Serial.print("  Status: ");
    Serial.print(prev);
    Serial.print(" (");
    Serial.print(wifiStatusToString(prev));
    Serial.println(")");

    while (WiFi.status() != WL_CONNECTED &&
           (millis() - start) < WIFI_ATTEMPT_TIMEOUT_MS) {
      delay(300);
      int s = WiFi.status();
      if (s != prev) {
        prev = s;
        Serial.print("  Status changed: ");
        Serial.print(s);
        Serial.print(" (");
        Serial.print(wifiStatusToString(s));
        Serial.println(")");
      } else {
        Serial.print('.');
      }
    }

    int finalStatus = WiFi.status();
    if (finalStatus == WL_CONNECTED) {
      Serial.println();
      Serial.print("Connected! IP: ");
      Serial.println(WiFi.localIP());
      startHttpServerIfConnected();
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
  scanAndReportNetworks();
}

static void startHttpServerIfConnected() { server.begin(); }

static void scanAndReportNetworks() {
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
    if (ss == WIFI_SSID) {
      found = true;
      Serial.print("Found target SSID with RSSI: ");
      Serial.println(rssi);
    }
  }
  if (!found) {
    Serial.print("Target SSID '");
    Serial.print(WIFI_SSID);
    Serial.println("' not found.");
  }
}

// ---------- Serial ticker ----------
static void serialTicker() {
  unsigned long now = millis();
  if (now - g_lastSerialMs < SERIAL_INTERVAL_MS)
    return;
  g_lastSerialMs = now;

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("WiFi: ");
    Serial.println(wifiStatusToString(WiFi.status()));
  }
}

// ---------- HTTP handling ----------
static void handleHttpClients() {
  WiFiClient client = server.accept();
  if (!client)
    return;

  // Read request line and headers until blank line
  String line = "";
  bool firstLine = true;
  String method, path;

  while (client.connected()) {
    if (!client.available())
      continue;
    char c = client.read();
    if (c == '\r')
      continue;
    if (c == '\n') {
      if (firstLine) {
        firstLine = false;
        int sp1 = line.indexOf(' ');
        int sp2 = line.indexOf(' ', sp1 + 1);
        if (sp1 > 0 && sp2 > sp1) {
          method = line.substring(0, sp1);
          path = line.substring(sp1 + 1, sp2);
        }
      }
      if (line.length() == 0) {
        // end of headers
        break;
      }
      line = "";
    } else {
      line += c;
    }
  }

  if (path == "/inc") {
    handleIncrement(client);
  } else {
    serveIndex(client);
  }

  delay(1);
  client.stop();
}

static void sendJson(WiFiClient &client, const String &body) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println(body);
}

static void sendHtml(WiFiClient &client, const String &body) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println(body);
}

static void serveIndex(WiFiClient &client) {
  String html;
  html.reserve(256);
  html += "<!DOCTYPE html><html><head><meta "
          "charset=\"utf-8\"><title>Counter</title>";
  html += "<meta name=\"viewport\" "
          "content=\"width=device-width,initial-scale=1\"></head><body>";
  html += "<h1 id=\"count\">Counter: ";
  html += String(g_counter);
  html += "</h1>";
  html += "<button id=\"inc\">Increment</button>";
  html += "<script>document.getElementById('inc').onclick=function(){fetch('/"
          "inc').then(r=>r.json()).then(j=>{document.getElementById('count')."
          "innerText='Counter: '+j.count});};</script>";
  html += "</body></html>";
  sendHtml(client, html);
}

static void handleIncrement(WiFiClient &client) {
  g_counter++;
  String json = "{\"count\":" + String(g_counter) + "}";
  sendJson(client, json);
}
