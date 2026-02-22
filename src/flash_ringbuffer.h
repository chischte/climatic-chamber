#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Ring Buffer Configuration
static constexpr uint32_t RING_BUFFER_NUM_SLOTS = 100;  // ~270 days of daily storage
static constexpr uint32_t RING_BUFFER_SLOT_SIZE = 64;   // 64 bytes per slot
static constexpr uint32_t RING_BUFFER_TOTAL_SIZE = RING_BUFFER_NUM_SLOTS * RING_BUFFER_SLOT_SIZE;

// Storage Model
// - Fixed-size ring buffer carved from the tail of QSPI/FlashIAP when available;
//   otherwise the app falls back to RAM.
// Wear and Data Integrity
// - Wear is reduced by appending writes across slots and erasing the whole
//   region only on wrap.
// - Integrity relies on per-slot CRC and treating 0xFF as "unwritten".
// API: Initialization
// - Returns true on success.
// - Parameters: total region size in bytes, slot size in bytes, number of slots.
bool fb_init(uint64_t region_bytes, uint32_t slot_size, uint32_t num_slots);

// API: Read a slot from the flash region. Returns true on success.
bool fb_read_slot(uint32_t slot, void *buffer, size_t len);

// API: Write (program) a slot to the flash region. Returns true on success.
bool fb_write_slot(uint32_t slot, const void *buffer, size_t len);

// API: Erase the reserved flash region. Returns true on success.
bool fb_erase_region();

// API: Query helpers
bool fb_available();
uint64_t fb_region_start();
uint64_t fb_region_size();

#ifdef __cplusplus
}
#endif
