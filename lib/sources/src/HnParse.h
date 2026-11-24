#pragma once
#include <stddef.h>

#include "Snapshots.h"

// Builds the Algolia front-page request URL. Returns bytes written (excluding the NUL), or 0 if
// it does not fit.
size_t hnUrl(char* out, size_t cap);

// Fills the data fields of snap from an Algolia search reply of exactly n bytes (no NUL needed).
// Titles are folded to Latin-1. Returns false and leaves snap untouched if the reply is not
// usable; zero usable hits is usable (count = 0). Never touches snap.status.
bool parseHn(const char* json, size_t n, HnSnap& snap);
