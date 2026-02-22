#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the flash-backed region for a ring buffer. Returns true on success.
// Parameters: total region size in bytes, slot size in bytes, number of slots.
bool fb_init(uint64_t region_bytes, uint32_t slot_size, uint32_t num_slots);

// Read a slot from the flash region. Returns true on success.
bool fb_read_slot(uint32_t slot, void *buffer, size_t len);

// Write (program) a slot to the flash region. Returns true on success.
bool fb_write_slot(uint32_t slot, const void *buffer, size_t len);

// Erase the reserved flash region. Returns true on success.
bool fb_erase_region();

// Query helpers
bool fb_available();
uint64_t fb_region_start();
uint64_t fb_region_size();

#ifdef __cplusplus
}
#endif
