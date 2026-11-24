#include "PageCommon.h"
#include "Text.h"

namespace pages {

void renderNotes(const PageContext& ctx, Canvas& canvas) {
  const auto& notes = ctx.feed.notes;
  char title[limits::NOTE_TITLE_MAX + 1];
  text::fold(notes.title, title, sizeof title);
  begin(canvas, title[0] ? title : "Notes", nullptr);
  if (notes.lineCount == 0) {
    draw::emptyState(canvas, "Nothing noted", "Add notes in the web app");
    return;
  }
  constexpr int PITCH = 20;
  constexpr int FIRST_BASELINE = 32;
  constexpr int BOX = 11;
  constexpr int INDENT = MARGIN + BOX + 6;
  // With any checkbox on the page, plain lines line up with the checkbox text so the list
  // reads as one column; without one they start at the margin.
  bool anyTodo = false;
  int n = notes.lineCount < limits::NOTE_LINES_MAX ? notes.lineCount : limits::NOTE_LINES_MAX;
  for (int i = 0; i < n; i++) anyTodo = anyTodo || notes.lines[i].todo;
  for (int i = 0; i < n; i++) {
    const NoteLine& line = notes.lines[i];
    char buf[limits::NOTE_TEXT_MAX + 1];
    text::fold(line.text, buf, sizeof buf);
    int base = FIRST_BASELINE + i * PITCH;
    if (line.todo) draw::checkbox(canvas, MARGIN, base - 10, line.done);
    int x = (line.todo || anyTodo) ? INDENT : MARGIN;
    oneLine(canvas, Font::BodyBold, x, base, Canvas::W - MARGIN - x, buf);
  }
}

}  // namespace pages
