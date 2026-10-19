#include <unity.h>
#include <string>
#include <vector>
#include "platform/rp2040/FactoryReset.h"

void setUp() {}
void tearDown() {}

static void test_factory_reset_formats_then_reboots_without_returning_to_saves() {
  struct Reboot {};
  std::string operations;
  std::vector<std::string> files{"/NVS/awtrix-cfg.bin", "/NVS/awtrix-ng.bin",
      "/ICONS/icon.gif", "/SCRIPTS/app.be", "/MELODIES/tune.txt", "/PALETTES/colors.json",
      "/app-order.json"};
  bool saved = false;
  try {
    awtrix::picoFactoryReset(
        [&] { operations += "stop;"; },
        [&] { operations += "format;"; files.clear(); },
        [&] { operations += "reboot;"; throw Reboot{}; });
    saved = true; // Models runtime settings/script/app-order persistence after a return.
  } catch (const Reboot&) {}
  TEST_ASSERT_EQUAL_STRING("stop;format;reboot;", operations.c_str());
  TEST_ASSERT_TRUE(files.empty());
  TEST_ASSERT_FALSE(saved);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_factory_reset_formats_then_reboots_without_returning_to_saves);
  return UNITY_END();
}
