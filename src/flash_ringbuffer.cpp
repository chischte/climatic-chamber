#include "flash_ringbuffer.h"
#include <mbed.h>

#if __has_include("QSPIFBlockDevice.h")
#include "QSPIFBlockDevice.h"
typedef QSPIFBlockDevice SelectedBlockDevice;
#define HAVE_BLOCKDEVICE 1
#elif __has_include("FlashIAPBlockDevice.h")
#include "FlashIAPBlockDevice.h"
typedef FlashIAPBlockDevice SelectedBlockDevice;
#define HAVE_BLOCKDEVICE 1
#else
#define HAVE_BLOCKDEVICE 0
#endif

#if HAVE_BLOCKDEVICE
static SelectedBlockDevice g_bd;
static uint64_t g_region_start = 0;
static uint64_t g_region_size = 0;
static uint32_t g_slot_size = 0;
static uint32_t g_num_slots = 0;
#endif

bool fb_init(uint64_t region_bytes, uint32_t slot_size, uint32_t num_slots) {
#if HAVE_BLOCKDEVICE
  int rc = g_bd.init();
  if (rc != 0) return false;
  uint64_t bd_size = g_bd.size();
  if (bd_size == 0) return false;
  uint64_t erase_size = g_bd.get_erase_size();

  uint64_t region_size = region_bytes;
  if (erase_size > 0) {
    region_size = ((region_size + erase_size - 1) / erase_size) * erase_size;
  }
  if (region_size > bd_size) {
    g_bd.deinit();
    return false;
  }

  g_region_size = region_size;
  g_region_start = bd_size - g_region_size; // use tail of flash
  g_slot_size = slot_size;
  g_num_slots = num_slots;

  return true;
#else
  (void)region_bytes; (void)slot_size; (void)num_slots;
  return false;
#endif
}

bool fb_read_slot(uint32_t slot, void *buffer, size_t len) {
#if HAVE_BLOCKDEVICE
  if (slot >= g_num_slots || len != g_slot_size) return false;
  uint64_t addr = g_region_start + (uint64_t)slot * g_slot_size;
  return (g_bd.read(buffer, addr, len) == 0);
#else
  (void)slot; (void)buffer; (void)len;
  return false;
#endif
}

bool fb_write_slot(uint32_t slot, const void *buffer, size_t len) {
#if HAVE_BLOCKDEVICE
  if (slot >= g_num_slots || len != g_slot_size) return false;
  uint64_t addr = g_region_start + (uint64_t)slot * g_slot_size;
  return (g_bd.program(buffer, addr, len) == 0);
#else
  (void)slot; (void)buffer; (void)len;
  return false;
#endif
}

bool fb_erase_region() {
#if HAVE_BLOCKDEVICE
  if (g_region_size == 0) return false;
  return (g_bd.erase(g_region_start, g_region_size) == 0);
#else
  return false;
#endif
}

bool fb_available() {
#if HAVE_BLOCKDEVICE
  return (g_region_size > 0);
#else
  return false;
#endif
}

uint64_t fb_region_start() {
#if HAVE_BLOCKDEVICE
  return g_region_start;
#else
  return 0;
#endif
}

uint64_t fb_region_size() {
#if HAVE_BLOCKDEVICE
  return g_region_size;
#else
  return 0;
#endif
}
