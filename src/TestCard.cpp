#include "TestCard.h"

#include <stdio.h>

void drawTestCard(Canvas& canvas, const char* version, int step) {
  u8g2_t* g = canvas.g();
  canvas.clear();
  u8g2_DrawFrame(g, 0, 0, Canvas::W, Canvas::H);          // 1 px border on all four edges
  u8g2_DrawFrame(g, 2, 2, Canvas::W - 4, Canvas::H - 4);  // second line 2 px in
  u8g2_DrawBox(g, 4, 4, 12, 12);                          // solid square marks the top left corner
  u8g2_DrawLine(g, 0, Canvas::H - 1, 40, Canvas::H - 41);  // diagonal from the bottom left corner
  u8g2_SetFont(g, u8g2_font_helvB10_tf);
  u8g2_DrawStr(g, 22, 16, "TOP LEFT");
  u8g2_DrawStr(g, Canvas::W - 4 - u8g2_GetStrWidth(g, "TOP RIGHT"), 16, "TOP RIGHT");
  u8g2_DrawStr(g, Canvas::W - 4 - u8g2_GetStrWidth(g, "BOTTOM RIGHT"), Canvas::H - 6, "BOTTOM RIGHT");
  u8g2_SetFont(g, u8g2_font_ncenB18_tf);
  u8g2_DrawStr(g, 50, 52, "Desk Gadget");
  u8g2_SetFont(g, u8g2_font_helvR08_tf);
  char line[48];
  snprintf(line, sizeof(line), "%s  step %d", version, step);
  u8g2_DrawStr(g, 50, 68, line);
  for (int i = 0; i < step && i < 8; i++) u8g2_DrawBox(g, 50 + i * 14, 76, 10, 10);
}
