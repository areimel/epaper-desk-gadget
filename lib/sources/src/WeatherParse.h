#pragma once
#include <stddef.h>

#include "FeedConfig.h"
#include "Snapshots.h"
#include "WmoText.h"

// Builds the Open-Meteo request URL for the feed's weather settings. Returns bytes written
// (excluding the NUL), or 0 if it does not fit or no place is set.
size_t weatherUrl(const FeedConfig& feed, char* out, size_t cap);

// Fills the data fields of snap from an Open-Meteo forecast reply of exactly n bytes (no NUL
// needed). Returns false and leaves snap untouched if the reply is not usable. Never touches
// snap.status.
bool parseWeather(const char* json, size_t n, WeatherSnap& snap);
