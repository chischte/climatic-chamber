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
#include "web_server.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <Arduino_PortentaMachineControl.h>
#include <WiFi.h>

// Configuration
static constexpr const char *WIFI_SSID = "mueschbache";
static constexpr const char *WIFI_PASS = "Schogg1chueche";
static constexpr uint16_t SERVER_PORT = 80;
static constexpr unsigned long SERIAL_BAUD = 115200;
static constexpr int WIFI_MAX_RETRIES = 3;
static constexpr unsigned long WIFI_ATTEMPT_TIMEOUT_MS = 20000UL;
static constexpr unsigned long WIFI_RETRY_DELAY_MS = 2000UL;
static constexpr unsigned long WIFI_HEARTBEAT_MS = 30000UL;
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
static WifiState g_wifiState = {false, -1, 0};
static WifiConfig g_wifiConfig = {};
static WebServerConfig g_webConfig = {};

// Forward declarations
static uint8_t crc8(const void *data, size_t len);
static void initStorage();
static void loadDataFromRingBuffer();
static void saveDataToRingBuffer();
static void persistDataIfDue();
static void onIncrement();

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long serialStart = millis();
  while (!Serial && (millis() - serialStart) < 3000UL) {
    delay(10);
  }
  Serial.println("Booting...");
  initStorage();
  loadDataFromRingBuffer();
  g_webConfig = {&server, g_values, 10, onIncrement};
  g_wifiConfig = {WIFI_SSID,
                  WIFI_PASS,
                  WIFI_MAX_RETRIES,
                  WIFI_ATTEMPT_TIMEOUT_MS,
                  WIFI_RETRY_DELAY_MS,
                  WIFI_HEARTBEAT_MS,
                  &server};
  wifi_connect(&g_wifiConfig);
  Serial.println("EXIT SETUP");
}
// ==================== END SETUP ====================

void loop() {
  wifi_serial_ticker(&g_wifiConfig, &g_wifiState);
  web_server_handle(&g_webConfig);
  persistDataIfDue();
}
// ==================== END LOOP ====================
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

// ---------- Web callbacks ----------
static void onIncrement() {
  g_values[0]++;
  g_valuesDirty = true;
  g_lastValueChangeMs = millis();
}
