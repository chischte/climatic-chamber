/*
 * *****************************************************************************
 * CLIMATIC CHAMBER
 * *****************************************************************************
 * Closed-loop control of temperature, relative humidity, and CO₂ levels
 * with fresh air management, implemented on an Arduino Portenta Machine
 * Control platform.
 * *****************************************************************************
 */

#include "flash_ringbuffer.h"
#include <Arduino.h>
#include <Arduino_PortentaMachineControl.h>
#include <WiFi.h>


// Configuration
static constexpr const char *WIFI_SSID = "mueschbache";
static constexpr const char *WIFI_PASS = "Schogg1chueche";
static constexpr uint16_t SERVER_PORT = 80;
static constexpr unsigned long SERIAL_BAUD = 115200;
static constexpr unsigned long SERIAL_INTERVAL_MS = 1000UL;
static constexpr int WIFI_MAX_RETRIES = 3;
static constexpr unsigned long WIFI_ATTEMPT_TIMEOUT_MS = 20000UL;
static constexpr unsigned long WIFI_RETRY_DELAY_MS = 2000UL;
static constexpr unsigned long PERSIST_INTERVAL_MS = 5000UL;

// Networking
static WiFiServer server(SERVER_PORT);

// Ring Buffer Storage (RAM-based, ready for QSPI Flash upgrade)
static constexpr uint32_t RING_BUFFER_NUM_SLOTS =
    100; // ~270 days of daily storage
static constexpr uint32_t RING_BUFFER_SLOT_SIZE = 64; // 64 bytes per slot
static constexpr uint32_t RING_BUFFER_TOTAL_SIZE =
    RING_BUFFER_NUM_SLOTS * RING_BUFFER_SLOT_SIZE;

// Data structure: 64 bytes per slot
struct DataEntry {
  uint32_t sequence;   // 4 bytes - sequence number (highest = newest)
  uint16_t values[10]; // 20 bytes - 10 × 10-bit values (stored as uint16_t)
  uint8_t crc;         // 1 byte - CRC8 checksum
  uint8_t padding[39]; // 39 bytes - padding to 64 bytes
} __attribute__((packed));

static_assert(sizeof(DataEntry) == 64, "DataEntry must be exactly 64 bytes");

// RAM-based ring buffer (TODO: upgrade to QSPI Flash via HAL or mbed
// BlockDevice later)
static uint8_t
    g_ringBuffer[RING_BUFFER_TOTAL_SIZE]; // 6.4 KB ring buffer in RAM
static bool g_storageInitialized = false;
static uint32_t g_currentSlot = 0; // Current write position in ring buffer
// Flash-backed storage
// flash helper availability (managed by flash_ringbuffer)
static bool g_flashAvailable = false;

// Application state (values[0] = counter for backwards compatibility)
static uint16_t g_values[10] = {0};
static bool g_valuesDirty = false;
static unsigned long g_lastValueChangeMs = 0;

// Timing
static unsigned long g_lastSerialMs = 0;
static bool g_ipPrinted = false; // Track if IP was already printed

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

// Forward declarations
static uint8_t crc8(const void *data, size_t len);
static void initStorage();
static void loadDataFromRingBuffer();
static void saveDataToRingBuffer();
static void persistDataIfDue();
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
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart) < 3000UL) {
    delay(10);
  }
  Serial.println("Booting...");
  initStorage();
  loadDataFromRingBuffer();
  connectWiFi();
  Serial.println("EXIT SETUP");
}

void loop() {
  serialTicker();
  handleHttpClients();
  persistDataIfDue();
}

// (flash debug helpers removed)

// ---------- Storage (Ring Buffer with Wear-Leveling on QSPI Flash) ----------

// CRC8 calculation for data integrity
static uint8_t crc8(const void *data, size_t len) {
  const uint8_t *p = (const uint8_t *)data;
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= p[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x07;
      } else {
        crc = crc << 1;
      }
    }
  }
  return crc;
}

// Initialize Ring Buffer (RAM-based storage)
static void initStorage() {
  Serial.print("Initializing Ring Buffer (");
  Serial.print(RING_BUFFER_TOTAL_SIZE);
  Serial.println(" bytes)...");

  // Try to initialize QSPI flash block device and reserve a region at the end
  // Try to initialize flash-backed ring buffer region. fb_init returns true
  // when a suitable block device is available and a region can be reserved.
  g_flashAvailable = fb_init(RING_BUFFER_TOTAL_SIZE, RING_BUFFER_SLOT_SIZE,
                             RING_BUFFER_NUM_SLOTS);
  if (g_flashAvailable) {
    Serial.println("Flash block device initialized for ring buffer");
  } else {
    Serial.println("Flash block device not available; using RAM ring buffer");
  }

  // Initialize RAM buffer to erased state (0xFF) in case flash not used
  memset(g_ringBuffer, 0xFF,
         RING_BUFFER_TOTAL_SIZE); // 0xFF = unwritten flash-like state

  Serial.print("Ring Buffer initialized: ");
  Serial.print(RING_BUFFER_NUM_SLOTS);
  Serial.println(" slots available");

  g_storageInitialized = true;
}

// Load most recent data from ring buffer
static void loadDataFromRingBuffer() {
  if (!g_storageInitialized) {
    Serial.println("Storage not initialized; using default values");
    memset(g_values, 0, sizeof(g_values));
    return;
  }

  Serial.println("Scanning ring buffer for newest valid entry...");

  DataEntry entry;
  uint32_t highestSeq = 0;
  uint32_t highestSlot = 0;
  bool found = false;
  // If flash region is available, scan that; otherwise scan RAM buffer.
  if (g_flashAvailable) {
    // scan all slots in flash region
    for (uint32_t slot = 0; slot < RING_BUFFER_NUM_SLOTS; slot++) {
      if (!fb_read_slot(slot, &entry, RING_BUFFER_SLOT_SIZE))
        continue;
      uint8_t calculated_crc = crc8(&entry, 24);
      if (calculated_crc != entry.crc)
        continue;
      if (entry.sequence == 0xFFFFFFFF)
        continue;
      if (entry.sequence > highestSeq) {
        highestSeq = entry.sequence;
        highestSlot = slot;
        found = true;
      }
    }
  } else {
    // Scan last 10 slots in RAM to find newest entry (with highest sequence
    // number)
    uint32_t start_slot =
        (RING_BUFFER_NUM_SLOTS > 10) ? (RING_BUFFER_NUM_SLOTS - 10) : 0;
    for (uint32_t slot = start_slot; slot < RING_BUFFER_NUM_SLOTS; slot++) {
      uint32_t addr = slot * RING_BUFFER_SLOT_SIZE;
      DataEntry *entry_ptr = (DataEntry *)(g_ringBuffer + addr);
      uint8_t calculated_crc = crc8(entry_ptr, 24); // 4B seq + 20B values
      if (calculated_crc != entry_ptr->crc) {
        continue; // Invalid CRC, skip this slot
      }
      if (entry_ptr->sequence == 0xFFFFFFFF) {
        continue;
      }
      if (entry_ptr->sequence > highestSeq) {
        highestSeq = entry_ptr->sequence;
        highestSlot = slot;
        found = true;
      }
    }
  }

  if (found) {
    // read the winning slot into entry to copy values
    if (g_flashAvailable) {
      if (fb_read_slot(highestSlot, &entry, RING_BUFFER_SLOT_SIZE)) {
        memcpy(g_values, entry.values, sizeof(entry.values));
      } else {
        memset(g_values, 0, sizeof(g_values));
      }
    } else {
      uint32_t addr = highestSlot * RING_BUFFER_SLOT_SIZE;
      DataEntry *entry_ptr = (DataEntry *)(g_ringBuffer + addr);
      memcpy(g_values, entry_ptr->values, sizeof(entry_ptr->values));
    }
    g_currentSlot = (highestSlot + 1) % RING_BUFFER_NUM_SLOTS;
    Serial.print("Loaded data from slot ");
    Serial.print(highestSlot);
    Serial.print(" (seq=");
    Serial.print(highestSeq);
    Serial.print(", values[0]=");
    Serial.print(g_values[0]);
    Serial.println(")");
  } else {
    memset(g_values, 0, sizeof(g_values));
    g_currentSlot = 0;
    Serial.println("No valid entries found; starting fresh");
  }
}

// Save data to next slot in ring buffer
static void saveDataToRingBuffer() {
  if (!g_storageInitialized) {
    return;
  }

  DataEntry entry;
  memset(&entry, 0, sizeof(entry));

  // Find next sequence number by reading previous slot
  uint32_t prevSlot =
      (g_currentSlot == 0) ? (RING_BUFFER_NUM_SLOTS - 1) : (g_currentSlot - 1);
  uint32_t prevSeq = 0xFFFFFFFF;

  if (g_flashAvailable) {
    // read previous entry from flash
    DataEntry prevEntry;
    if (fb_read_slot(prevSlot, &prevEntry, RING_BUFFER_SLOT_SIZE)) {
      prevSeq = prevEntry.sequence;
    }
    entry.sequence = (prevSeq != 0xFFFFFFFF) ? (prevSeq + 1) : 1;
    memcpy(entry.values, g_values, sizeof(entry.values));
    entry.crc = crc8(&entry, 24); // 4B seq + 20B values

    // If destination already contains programmed data (not 0xFF), we are
    // wrapping; erase the whole region before writing to keep append-only
    // semantics.
    uint8_t probe[RING_BUFFER_SLOT_SIZE];
    if (fb_read_slot(g_currentSlot, probe, RING_BUFFER_SLOT_SIZE)) {
      bool erased = true;
      for (size_t i = 0; i < RING_BUFFER_SLOT_SIZE; ++i) {
        if (probe[i] != 0xFF) {
          erased = false;
          break;
        }
      }
      if (!erased) {
        // Erase whole region (aligned) before wrapping
        fb_erase_region();
      }
    }

    // Program the entry
    if (!fb_write_slot(g_currentSlot, &entry, RING_BUFFER_SLOT_SIZE)) {
      Serial.println("Flash program failed");
    } else {
      Serial.print("Saved to flash slot ");
      Serial.print(g_currentSlot);
      Serial.print(" (seq=");
      Serial.print(entry.sequence);
      Serial.print(", values[0]=");
      Serial.print(g_values[0]);
      Serial.println(")");
    }

    g_currentSlot = (g_currentSlot + 1) % RING_BUFFER_NUM_SLOTS;
    g_valuesDirty = false;
    return;
  }

  // Fallback: write to RAM buffer
  uint32_t prevAddr = prevSlot * RING_BUFFER_SLOT_SIZE;
  DataEntry *prevEntry_ptr = (DataEntry *)(g_ringBuffer + prevAddr);
  entry.sequence = (prevEntry_ptr->sequence != 0xFFFFFFFF)
                       ? (prevEntry_ptr->sequence + 1)
                       : 1;
  memcpy(entry.values, g_values, sizeof(entry.values));
  entry.crc = crc8(&entry, 24); // 4B seq + 20B values

  // Write to current slot in RAM buffer
  uint32_t addr = g_currentSlot * RING_BUFFER_SLOT_SIZE;
  memcpy(&g_ringBuffer[addr], &entry, RING_BUFFER_SLOT_SIZE);

  Serial.print("Saved to RAM slot ");
  Serial.print(g_currentSlot);
  Serial.print(" (seq=");
  Serial.print(entry.sequence);
  Serial.print(", values[0]=");
  Serial.print(g_values[0]);
  Serial.println(")");

  g_currentSlot = (g_currentSlot + 1) % RING_BUFFER_NUM_SLOTS;
  g_valuesDirty = false;
}

// Persist data if dirty and debounce interval has passed
static void persistDataIfDue() {
  if (!g_storageInitialized || !g_valuesDirty) {
    return;
  }

  unsigned long now = millis();
  if ((now - g_lastValueChangeMs) < PERSIST_INTERVAL_MS) {
    return;
  }

  saveDataToRingBuffer();
}

// ---------- WiFi + Server ----------
static void connectWiFi() {
  Serial.print("WiFi: Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.disconnect();
  delay(100);

  for (int attempt = 1; attempt <= WIFI_MAX_RETRIES; ++attempt) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);

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
  static int lastStatus = -1;
  static unsigned long lastHeartbeatMs = 0;

  if (!Serial) {
    g_ipPrinted = false;
    return;
  }

  unsigned long now = millis();
  int status = WiFi.status();
  if (status != lastStatus) {
    lastStatus = status;
    lastHeartbeatMs = now;
    Serial.print("WiFi: ");
    Serial.println(wifiStatusToString(status));
  } else if ((now - lastHeartbeatMs) >= 30000UL) {
    lastHeartbeatMs = now;
    Serial.print("WiFi: ");
    Serial.println(wifiStatusToString(status));
  }

  if (status == WL_CONNECTED) {
    // Print IP once per connection, even if the monitor is opened later
    if (!g_ipPrinted) {
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      g_ipPrinted = true;
    }
  } else {
    // Reset flag when disconnected
    g_ipPrinted = false;
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
  html += String(g_values[0]);
  html += "</h1>";
  html += "<button id=\"inc\">Increment</button>";
  html += "<script>document.getElementById('inc').onclick=function(){fetch('/"
          "inc').then(r=>r.json()).then(j=>{document.getElementById('count')."
          "innerText='Counter: '+j.count});};</script>";
  html += "</body></html>";
  sendHtml(client, html);
}

static void handleIncrement(WiFiClient &client) {
  g_values[0]++;
  g_valuesDirty = true;
  g_lastValueChangeMs = millis();
  String json = "{\"count\":" + String(g_values[0]) + "}";
  sendJson(client, json);
}
