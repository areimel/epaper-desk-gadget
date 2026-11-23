#pragma once
#include "Canvas.h"

// Temporary hardware check: shows which way up the image is and whether any edge is cut off.
// `step` adds one numbered bar per call so a fast refresh has something new to show.
void drawTestCard(Canvas& canvas, const char* version, int step);
