#pragma once
#include "Canvas.h"
#include "Font.h"

// Drawing helpers every dashboard page shares. All text is Latin-1 (already folded), NUL
// terminated, drawn with the U8g2 ASCII string calls so bytes 0xA0-0xFF stay Latin-1 glyphs.
// Colour: 1 = black, 0 = white. Helpers leave the draw colour at 1 and never allocate.
namespace draw {

struct Box {
  int x, y, w, h;
};

constexpr int TITLE_BAR_H = 14;  // rows 0..13; content starts at y = 14

// Black bar across the top with white text: title at the left (Font::SmallBold) and an optional
// right-aligned note (Font::Small, may be null). The note is dropped if it would touch the
// title; a title that is too wide alone is cut with "...".
void titleBar(Canvas& canvas, const char* title, const char* right);

// Plain text with the baseline at baselineY. null draws nothing.
void text(Canvas& canvas, Font font, int x, int baselineY, const char* s);
void textRight(Canvas& canvas, Font font, int rightX, int baselineY, const char* s);  // ink ends at rightX-1
void textCenter(Canvas& canvas, Font font, int centerX, int baselineY, const char* s);
int textWidth(Font font, const char* s);  // pixels from the origin to the last ink column

// Word-wrapped text in box, top-aligned, at most maxLines lines and never more than fit the box
// height; the last line gets "..." when the text is cut. Returns the lines drawn.
int paragraph(Canvas& canvas, Font font, Box box, const char* s, int maxLines);

// Uses the first font of ladder (largest first, n entries) in which all of s wraps into box
// without an ellipsis; if none fits, the last font with an ellipsis. Lines are centred
// horizontally, the block vertically. Returns the font used.
Font fitted(Canvas& canvas, const Font* ladder, int n, Box box, const char* s);

void checkbox(Canvas& canvas, int x, int y, bool done);  // 11x11, 2 px outline, tick when done

// Two centred lines for a page with nothing to show, below the title bar: line1 in
// Font::BodyBold, line2 (may be null) in Font::Small.
void emptyState(Canvas& canvas, const char* line1, const char* line2);

void hairline(Canvas& canvas, int x, int y, int w);  // 1 px horizontal separator

}  // namespace draw
