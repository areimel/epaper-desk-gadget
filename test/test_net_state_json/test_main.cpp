#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

// lib/net is ignored on native (lib_ignore), so its pure header is included by path. Canvas.h and
// Snapshots.h come first so the library finder pulls in lib/epaper and lib/sources.
#include "Canvas.h"
#include "Snapshots.h"
#include "../../lib/net/src/StateJson.h"

void setUp() {}
void tearDown() {}

static char out[1024];
static char fixture[4096];

static net::StateInfo sample() {
  net::StateInfo s = {};
  s.name = "Desk Gadget";
  s.version = "0.1.0-dev";
  s.board = "Heltec Wireless Paper";
  s.ip = "192.168.1.50";
  s.host = "desk-gadget.local";
  s.time = 1795452060;
  s.timeSynced = true;
  s.page = "weather";
  s.weather = {SourceState::Ok, 1795451400, ""};
  s.hn = {SourceState::Stale, 1795448000, "no network"};
  s.calendar = {SourceState::Never, 0, ""};
  s.heapFree = 182000;
  s.refreshCount = 12;
  return s;
}

static const char* kind(JsonVariantConst v) {
  if (v.is<JsonObjectConst>()) return "object";
  if (v.is<JsonArrayConst>()) return "array";
  if (v.is<bool>()) return "bool";
  if (v.is<long long>()) return "integer";
  if (v.is<double>()) return "number";
  if (v.is<const char*>()) return "string";
  return "null";
}

// Same keys (both ways) and the same JSON type at every path.
static void sameShape(JsonVariantConst want, JsonVariantConst got, const char* path) {
  char msg[160];
  snprintf(msg, sizeof msg, "type at '%s'", path);
  TEST_ASSERT_EQUAL_STRING_MESSAGE(kind(want), kind(got), msg);
  if (!want.is<JsonObjectConst>()) return;
  JsonObjectConst w = want.as<JsonObjectConst>(), g = got.as<JsonObjectConst>();
  snprintf(msg, sizeof msg, "key count at '%s'", path);
  TEST_ASSERT_EQUAL_MESSAGE(w.size(), g.size(), msg);
  for (JsonPairConst kv : w) {
    char sub[128];
    snprintf(sub, sizeof sub, "%s.%s", path, kv.key().c_str());
    sameShape(kv.value(), g[kv.key()], sub);
  }
}

void test_matches_mock_fixture_shape() {
  FILE* f = fopen("device-ui/mock/fixtures/state.json", "rb");
  TEST_ASSERT_NOT_NULL_MESSAGE(f, "device-ui/mock/fixtures/state.json (cwd must be the project root)");
  const size_t n = fread(fixture, 1, sizeof fixture - 1, f);
  fclose(f);
  JsonDocument want, got;
  TEST_ASSERT_FALSE(deserializeJson(want, fixture, n));
  const size_t len = net::stateJson(sample(), out, sizeof out);
  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_FALSE(deserializeJson(got, out, len));
  sameShape(want.as<JsonVariantConst>(), got.as<JsonVariantConst>(), "$");
}

void test_values_and_limits_from_header() {
  JsonDocument got;
  TEST_ASSERT_FALSE(deserializeJson(got, out, net::stateJson(sample(), out, sizeof out)));
  TEST_ASSERT_EQUAL(limits::DWELL_MIN_S, got["limits"]["dwellMin"].as<int>());
  TEST_ASSERT_EQUAL(limits::TZ_MAX, got["limits"]["tzMax"].as<int>());
  TEST_ASSERT_EQUAL(Canvas::ROW_BYTES, got["screen"]["rowBytes"].as<int>());
  TEST_ASSERT_EQUAL_STRING("stale", got["sources"]["hn"]["state"]);
  TEST_ASSERT_EQUAL_STRING("no network", got["sources"]["hn"]["error"]);
  TEST_ASSERT_EQUAL_STRING("never", got["sources"]["calendar"]["state"]);
  TEST_ASSERT_TRUE(got["time"].as<long long>() == 1795452060LL);
}

void test_state_names() {
  TEST_ASSERT_EQUAL_STRING("never", net::stateName(SourceState::Never));
  TEST_ASSERT_EQUAL_STRING("ok", net::stateName(SourceState::Ok));
  TEST_ASSERT_EQUAL_STRING("stale", net::stateName(SourceState::Stale));
  TEST_ASSERT_EQUAL_STRING("error", net::stateName(SourceState::Error));
}

void test_offline_empty_strings_and_small_buffer() {
  net::StateInfo s = sample();
  s.ip = "";
  s.page = "";
  JsonDocument got;
  TEST_ASSERT_FALSE(deserializeJson(got, out, net::stateJson(s, out, sizeof out)));
  TEST_ASSERT_EQUAL_STRING("", got["ip"]);
  TEST_ASSERT_EQUAL_STRING("", got["page"]);
  TEST_ASSERT_EQUAL(0, net::stateJson(s, out, 64));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_matches_mock_fixture_shape);
  RUN_TEST(test_values_and_limits_from_header);
  RUN_TEST(test_state_names);
  RUN_TEST(test_offline_empty_strings_and_small_buffer);
  return UNITY_END();
}
