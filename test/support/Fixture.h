#pragma once
#include <stdio.h>
#include <unity.h>

// Loads a test fixture (path relative to test/fixtures/) into a static buffer. Tests run with
// the project root as the working directory. Fails the test with a clear message if the file is
// missing or too large. The buffer is NOT NUL-terminated beyond the returned length plus one
// spare byte; use the returned length.
struct Fixture {
  static constexpr size_t CAP = 16384;
  char buf[CAP + 1];
  size_t len = 0;

  explicit Fixture(const char* name) {
    char path[160];
    snprintf(path, sizeof path, "test/fixtures/%s", name);
    FILE* f = fopen(path, "rb");
    if (!f) {
      char msg[240];
      snprintf(msg, sizeof msg, "fixture not found: %s (cwd must be the project root)", path);
      TEST_FAIL_MESSAGE(msg);
      return;
    }
    len = fread(buf, 1, CAP, f);
    bool tooBig = fgetc(f) != EOF;
    fclose(f);
    buf[len] = 0;
    if (tooBig) TEST_FAIL_MESSAGE("fixture larger than Fixture::CAP");
  }
};
