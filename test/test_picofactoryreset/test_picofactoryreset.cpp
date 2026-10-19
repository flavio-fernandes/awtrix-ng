#include <unity.h>
#include <string>
#include <vector>
#include "platform/rp2040/FactoryReset.h"
#include "platform/rp2040/SaveRetry.h"

void setUp() {}
void tearDown() {}

namespace {
struct Disk {
  std::string operations;
  std::vector<std::string> files{"/NVS/awtrix-cfg.bin", "/NVS/awtrix-ng.bin",
      "/ICONS/icon.gif", "/SCRIPTS/app.be", "/MELODIES/tune.txt", "/PALETTES/colors.json",
      "/app-order.json"};

  // The loop's way out: the pending saves (settings, scripts, app order) or the format.
  void reboot(const awtrix::PicoFactoryReset& reset) {
    reset.beforeReboot(
        [&] { operations += "save;"; files.push_back("/NVS/awtrix-ng.bin"); },
        [&] { operations += "stop;"; },
        [&] { operations += "format;"; files.clear(); });
    operations += "reboot;";
  }
};
}

// The request only queues the reset, so the HTTP or MQTT caller still gets its answer.
static void test_a_request_queues_the_reset() {
  awtrix::PicoFactoryReset reset;
  TEST_ASSERT_FALSE(reset.pending());
  reset.request();
  TEST_ASSERT_TRUE(reset.pending());
}

static void test_the_restart_formats_without_saving() {
  awtrix::PicoFactoryReset reset;
  Disk disk;
  reset.request();
  disk.reboot(reset);
  TEST_ASSERT_EQUAL_STRING("stop;format;reboot;", disk.operations.c_str());
  TEST_ASSERT_TRUE(disk.files.empty());
}

static void test_a_plain_reboot_saves_and_keeps_the_files() {
  awtrix::PicoFactoryReset reset;
  Disk disk;
  const auto before = disk.files.size();
  disk.reboot(reset);
  TEST_ASSERT_EQUAL_STRING("save;reboot;", disk.operations.c_str());
  TEST_ASSERT_EQUAL(before + 1, disk.files.size());
}

// The save before a reboot is retried a few times, and gives up rather than holding the reboot.
static void test_a_failing_save_is_retried_then_given_up() {
  int tries = 0, waits = 0;
  TEST_ASSERT_TRUE(awtrix::saveWithRetries([&] { return ++tries == 2; }, [&] { ++waits; }));
  TEST_ASSERT_EQUAL(2, tries);
  TEST_ASSERT_EQUAL(1, waits);
  tries = waits = 0;
  TEST_ASSERT_FALSE(awtrix::saveWithRetries([&] { ++tries; return false; }, [&] { ++waits; }));
  TEST_ASSERT_EQUAL(3, tries);
  TEST_ASSERT_EQUAL(2, waits);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_a_request_queues_the_reset);
  RUN_TEST(test_the_restart_formats_without_saving);
  RUN_TEST(test_a_plain_reboot_saves_and_keeps_the_files);
  RUN_TEST(test_a_failing_save_is_retried_then_given_up);
  return UNITY_END();
}
