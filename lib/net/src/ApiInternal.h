#pragma once
// Private to lib/net: shared by Api.cpp, ApiState.cpp, ApiScreen.cpp and StaticFiles.cpp.
#if FEATURE_DASHBOARD
#include <WebServer.h>
#include <stddef.h>

#include "Api.h"

namespace net {
namespace api {
WebServer& server();
const ApiHooks& hooks();
const char* hostname();
const char* deviceName();

// Every /api/* reply goes through these: application/json (or the given type) and no-store.
void sendBytes(int code, const char* type, const uint8_t* data, size_t n);
void sendJson(int code, const char* json, size_t n);
void sendError(int code, const char* error, const char* path = "");

void handleState();   // ApiState.cpp
void handleScreen();  // ApiScreen.cpp
// StaticFiles.cpp: serves GET/HEAD for an embedded file (or the built-in page). False if none.
bool serveStatic(const char* path);
}  // namespace api
}  // namespace net
#endif
