#include "Canvas.h"

#include <string.h>

namespace {
// A display that only exists in memory: U8g2 needs the geometry, nothing is ever sent.
const u8x8_display_info_t kInfo = {
    /* chip_enable_level */ 0, /* chip_disable_level */ 1,
    /* post_chip_enable_wait_ns */ 0, /* pre_chip_disable_wait_ns */ 0,
    /* reset_pulse_width_ms */ 0, /* post_reset_wait_ms */ 0,
    /* sda_setup_time_ns */ 0, /* sck_pulse_width_ns */ 0,
    /* sck_clock_hz */ 1000000UL, /* spi_mode */ 0, /* i2c_bus_clock_100kHz */ 0,
    /* data_setup_time_ns */ 0, /* write_pulse_width_ns */ 0,
    /* tile_width */ Canvas::ROW_BYTES, /* tile_height */ 16,
    /* default_x_offset */ 0, /* flipmode_x_offset */ 0,
    /* pixel_width */ Canvas::W, /* pixel_height */ Canvas::H};

uint8_t memoryDisplay(u8x8_t* u8x8, uint8_t msg, uint8_t, void*) {
  if (msg == U8X8_MSG_DISPLAY_SETUP_MEMORY) u8x8_d_helper_display_setup_memory(u8x8, &kInfo);
  return 1;
}
}  // namespace

Canvas::Canvas() {
  u8g2_SetupDisplay(&u8g2_, memoryDisplay, u8x8_dummy_cb, u8x8_dummy_cb, u8x8_dummy_cb);
  u8g2_SetupBuffer(&u8g2_, buffer_, TILE_ROWS, u8g2_ll_hvline_horizontal_right_lsb, U8G2_R0);
  u8g2_SetFontMode(&u8g2_, 1);  // transparent glyph background, so text can sit on a filled bar
  clear();
}

void Canvas::clear() {
  memset(buffer_, 0, sizeof(buffer_));
  u8g2_SetDrawColor(&u8g2_, 1);
}

bool Canvas::pixel(int x, int y) const {
  if (x < 0 || y < 0 || x >= W || y >= H) return false;
  return (buffer_[y * ROW_BYTES + x / 8] & (0x80 >> (x % 8))) != 0;
}

size_t Canvas::packRows(uint8_t* out) const {
  memcpy(out, buffer_, PACKED_BYTES);
  return PACKED_BYTES;
}
