/*
 * *****************************************************************************
 * STORAGE MODULE IMPLEMENTATION
 * *****************************************************************************
 */

#include "storage.h"
#include "flash_ringbuffer.h"

// Data structure: 64 bytes per slot
struct DataEntry {
  uint32_t sequence;   // 4 bytes - sequence number (highest = newest)
  uint16_t values[10]; // 20 bytes - 10 × 10-bit values (stored as uint16_t)
  uint8_t crc;         // 1 byte - CRC8 checksum
  uint8_t padding[39]; // 39 bytes - padding to 64 bytes
} __attribute__((packed));

static_assert(sizeof(DataEntry) == 64, "DataEntry must be exactly 64 bytes");

// Internal state
static uint8_t g_ringBuffer[RING_BUFFER_TOTAL_SIZE];
static bool g_storageInitialized = false;
static uint32_t g_currentSlot = 0;
static bool g_flashAvailable = false;

// Application data
static uint16_t g_values[NUM_VALUES] = {0};
static bool g_valuesDirty = false;
static unsigned long g_lastValueChangeMs = 0;

// Forward declarations
static uint8_t crc8(const void *data, size_t len);
static void saveDataToRingBuffer();

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

// Initialize storage system
void storage_init() {
  Serial.print("Initializing Ring Buffer (");
  Serial.print(RING_BUFFER_TOTAL_SIZE);
  Serial.println(" bytes)...");

  g_flashAvailable = fb_init(RING_BUFFER_TOTAL_SIZE, RING_BUFFER_SLOT_SIZE,
                             RING_BUFFER_NUM_SLOTS);
  if (g_flashAvailable) {
    Serial.println("Flash block device initialized for ring buffer");
  } else {
    Serial.println("Flash block device not available; using RAM ring buffer");
  }

  memset(g_ringBuffer, 0xFF, RING_BUFFER_TOTAL_SIZE);

  Serial.print("Ring Buffer initialized: ");
  Serial.print(RING_BUFFER_NUM_SLOTS);
  Serial.println(" slots available");

  g_storageInitialized = true;
}

// Load persisted data from storage
void storage_load() {
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

  if (g_flashAvailable) {
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
    uint32_t start_slot =
        (RING_BUFFER_NUM_SLOTS > 10) ? (RING_BUFFER_NUM_SLOTS - 10) : 0;
    for (uint32_t slot = start_slot; slot < RING_BUFFER_NUM_SLOTS; slot++) {
      uint32_t addr = slot * RING_BUFFER_SLOT_SIZE;
      DataEntry *entry_ptr = (DataEntry *)(g_ringBuffer + addr);
      uint8_t calculated_crc = crc8(entry_ptr, 24);
      if (calculated_crc != entry_ptr->crc) {
        continue;
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

// Save data to ring buffer
static void saveDataToRingBuffer() {
  if (!g_storageInitialized) {
    return;
  }

  DataEntry entry;
  memset(&entry, 0, sizeof(entry));

  uint32_t prevSlot =
      (g_currentSlot == 0) ? (RING_BUFFER_NUM_SLOTS - 1) : (g_currentSlot - 1);
  uint32_t prevSeq = 0xFFFFFFFF;

  if (g_flashAvailable) {
    DataEntry prevEntry;
    if (fb_read_slot(prevSlot, &prevEntry, RING_BUFFER_SLOT_SIZE)) {
      prevSeq = prevEntry.sequence;
    }
    entry.sequence = (prevSeq != 0xFFFFFFFF) ? (prevSeq + 1) : 1;
    memcpy(entry.values, g_values, sizeof(entry.values));
    entry.crc = crc8(&entry, 24);

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
        fb_erase_region();
      }
    }

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

  // Fallback: RAM buffer
  uint32_t prevAddr = prevSlot * RING_BUFFER_SLOT_SIZE;
  DataEntry *prevEntry_ptr = (DataEntry *)(g_ringBuffer + prevAddr);
  entry.sequence = (prevEntry_ptr->sequence != 0xFFFFFFFF)
                       ? (prevEntry_ptr->sequence + 1)
                       : 1;
  memcpy(entry.values, g_values, sizeof(entry.values));
  entry.crc = crc8(&entry, 24);

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

// Periodic tick - handles auto-persistence
void storage_tick() {
  if (!g_storageInitialized || !g_valuesDirty) {
    return;
  }

  unsigned long now = millis();
  if ((now - g_lastValueChangeMs) < PERSIST_INTERVAL_MS) {
    return;
  }

  saveDataToRingBuffer();
}

// Get values (read-only)
const uint16_t* storage_get_values() {
  return g_values;
}

// Get values (mutable for web server)
uint16_t* storage_get_values_mutable() {
  return g_values;
}

// Get number of values
uint8_t storage_get_num_values() {
  return NUM_VALUES;
}

// Increment a value
void storage_increment_value(uint8_t index) {
  if (index < NUM_VALUES) {
    g_values[index]++;
    g_valuesDirty = true;
    g_lastValueChangeMs = millis();
  }
}

// Set a value
void storage_set_value(uint8_t index, uint16_t value) {
  if (index < NUM_VALUES) {
    g_values[index] = value;
    g_valuesDirty = true;
    g_lastValueChangeMs = millis();
  }
}

// Force immediate save
void storage_save_now() {
  if (g_storageInitialized && g_valuesDirty) {
    saveDataToRingBuffer();
  }
}

// CO2 Setpoint management (stored in values[1])
void storage_set_co2_setpoint(uint16_t ppm) {
  // Clamp to valid range: 400-10000 ppm
  if (ppm < 400) ppm = 400;
  if (ppm > 10000) ppm = 10000;
  storage_set_value(1, ppm);
}

uint16_t storage_get_co2_setpoint() {
  uint16_t setpoint = g_values[1];
  // Default to 800 ppm if not set or out of range
  if (setpoint < 400 || setpoint > 10000) {
    setpoint = 800;
    storage_set_co2_setpoint(setpoint);
  }
  return setpoint;
}

// RH Setpoint management (stored in values[2], scaled by 10)
void storage_set_rh_setpoint(float percent) {
  // Clamp to valid range: 82-96%
  if (percent < 82.0f) percent = 82.0f;
  if (percent > 96.0f) percent = 96.0f;
  uint16_t scaled = (uint16_t)(percent * 10.0f); // 89.0 -> 890
  storage_set_value(2, scaled);
}

float storage_get_rh_setpoint() {
  uint16_t scaled = g_values[2];
  // Default to 89.0% if not set
  if (scaled < 820 || scaled > 960) {
    scaled = 890; // 89.0%
    storage_set_value(2, scaled);
  }
  return scaled / 10.0f;
}

// Temperature Setpoint management (stored in values[3], scaled by 10)
void storage_set_temp_setpoint(float celsius) {
  // Clamp to valid range: 18-32°C
  if (celsius < 18.0f) celsius = 18.0f;
  if (celsius > 32.0f) celsius = 32.0f;
  uint16_t scaled = (uint16_t)(celsius * 10.0f); // 25.0 -> 250
  storage_set_value(3, scaled);
}

float storage_get_temp_setpoint() {
  uint16_t scaled = g_values[3];
  // Default to 25.0°C if not set
  if (scaled < 180 || scaled > 320) {
    scaled = 250; // 25.0°C
    storage_set_value(3, scaled);
  }
  return scaled / 10.0f;
}
