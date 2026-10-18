#include <unity.h>

#include <Preferences.h>

#include "persistence/DeviceConfig.h"
#include "persistence/Filesystem.h"
#include "persistence/DocumentFile.h"
#include "platform/rp2040/AssetDirectories.h"
#include "persistence/RadioStore.h"
#include "persistence/NvsSettings.h"
#include "core/Settings.h"
#include "persistence/SystemConfigApply.h"
#include "platform/rp2040/PicoRestoreSink.h"
#include "platform/rp2040/AssetProbe.h"
#include "core/sound/AudioRouter.h"
#include "core/api/CapabilitiesJson.h"
#include "platform/rp2040/DisplayProfile.h"
#include "platform/rp2040/Rp2040Profile.h"

// The production DeviceConfig, NvsSettings, LittleFS adapter and restore sink of the Pico, run
// against the simulated filesystem in compat/.
using namespace awtrix;
void setUp() { testFiles.clear(); testDirs.clear(); failWrite = failRename = failOpen = shortRead = false; }
void tearDown() {}

void legacy_panel_height_is_ignored_and_removed() {
  Preferences p;
  TEST_ASSERT_TRUE(p.begin("awtrix-cfg", false));
  p.putInt("ph", 16);
  p.end();
  DeviceConfig cfg;
  cfg.load();
  TEST_ASSERT_EQUAL(11, cfg.panelHeight);
  TEST_ASSERT_TRUE(cfg.save());
  TEST_ASSERT_TRUE(p.begin("awtrix-cfg", true));
  TEST_ASSERT_FALSE(p.isKey("ph"));
  TEST_ASSERT_EQUAL(11, p.getInt("pheight", -1));
  p.end();
}

void panel_height_uses_fresh_key() {
  DeviceConfig cfg;
  cfg.panelHeight = 11;
  TEST_ASSERT_TRUE(cfg.save());
  Preferences p;
  TEST_ASSERT_TRUE(p.begin("awtrix-cfg", true));
  TEST_ASSERT_EQUAL(11, p.getInt("pheight", -1));
  TEST_ASSERT_FALSE(p.isKey("ph"));
  p.end();
  // A stale legacy value must not override the fresh key either.
  TEST_ASSERT_TRUE(p.begin("awtrix-cfg", false));
  p.putInt("ph", 16);
  p.end();
  DeviceConfig loaded;
  loaded.load();
  TEST_ASSERT_EQUAL(11, loaded.panelHeight);
  TEST_ASSERT_TRUE(loaded.save());
  TEST_ASSERT_TRUE(p.begin("awtrix-cfg", true));
  TEST_ASSERT_FALSE(p.isKey("ph"));
  TEST_ASSERT_EQUAL(11, p.getInt("pheight", -1));
  p.end();
}

void config_and_settings_reboot_roundtrip() {
  TEST_ASSERT_TRUE(fs::begin());
  TEST_ASSERT_TRUE(testDirs.count("/ICONS") && testDirs.count("/MELODIES") && testDirs.count("/PALETTES"));
  DeviceConfig before;
  before.wifiSsid = "test-ssid";
  before.wifiPass = "test-pass";
  before.panelHeight = 11;
  before.mqttPort = 8883;
  before.tempOffset = -7.5f;
  before.wifiConnectTimeout = 32100;
  before.mqttEnabled = true;
  before.minBrightness = 22;
  TEST_ASSERT_TRUE(before.save());
  TEST_ASSERT_FALSE(before.persistencePending);
  DeviceConfig after;
  after.load();
  TEST_ASSERT_EQUAL_STRING("test-ssid", after.wifiSsid.c_str());
  TEST_ASSERT_EQUAL_STRING("test-pass", after.wifiPass.c_str());
  TEST_ASSERT_EQUAL(11, after.panelHeight);
  TEST_ASSERT_EQUAL(8883, after.mqttPort);
  TEST_ASSERT_EQUAL_FLOAT(-7.5f, after.tempOffset);
  TEST_ASSERT_EQUAL(32100, after.wifiConnectTimeout);
  TEST_ASSERT_TRUE(after.mqttEnabled);
  TEST_ASSERT_EQUAL(22, after.minBrightness);
  Settings settings;
  settings.applyRead(api::JsonReader(R"({"brightness":42})"));
  TEST_ASSERT_EQUAL(42, settings.brightness);
  nvs::saveSettings(settings);
  Settings loaded;
  nvs::loadSettings(loaded);
  TEST_ASSERT_EQUAL(42, loaded.brightness);
  std::string a, b;
  api::JsonWriter wa(a), wb(b);
  settings.writeMembers(wa);
  loaded.writeMembers(wb);
  TEST_ASSERT_EQUAL_STRING(a.c_str(), b.c_str());
  // A damaged snapshot is not half read: the defaults load instead.
  testFiles["/NVS/awtrix-cfg.bin"][0] ^= 1;
  DeviceConfig defaults;
  defaults.load();
  TEST_ASSERT_TRUE(defaults.wifiSsid.empty());
}

// A damaged snapshot loads the defaults, and the next save replaces it instead of failing.
void damaged_config_is_saved_afresh() {
  static int lines = 0;
  lines = 0;
  storage::setKvLog([](const char*) { ++lines; });
  testFiles["/NVS/awtrix-cfg.bin"] = "garbage, not a snapshot";
  DeviceConfig cfg;
  cfg.load();
  TEST_ASSERT_TRUE(cfg.wifiSsid.empty());
  cfg.wifiSsid = "after-damage";
  cfg.mqttPort = 1999;
  TEST_ASSERT_TRUE(cfg.save());
  TEST_ASSERT_FALSE(cfg.persistencePending);
  TEST_ASSERT_EQUAL_STRING("garbage, not a snapshot", testFiles["/NVS/awtrix-cfg.bin.bad"].c_str());
  TEST_ASSERT_EQUAL(1, lines);
  DeviceConfig loaded;
  loaded.load();
  TEST_ASSERT_EQUAL_STRING("after-damage", loaded.wifiSsid.c_str());
  TEST_ASSERT_EQUAL(1999, loaded.mqttPort);
  TEST_ASSERT_TRUE(loaded.save());
  TEST_ASSERT_EQUAL(1, lines);
  storage::setKvLog(nullptr);
}

void failed_writes_keep_last_config() {
  DeviceConfig cfg;
  cfg.wifiSsid = "old";
  TEST_ASSERT_TRUE(cfg.save());
  cfg.wifiSsid = "new";
  failWrite = true;
  TEST_ASSERT_FALSE(cfg.save());
  failWrite = false;
  TEST_ASSERT_TRUE(cfg.persistencePending);
  DeviceConfig loaded;
  loaded.load();
  TEST_ASSERT_EQUAL_STRING("old", loaded.wifiSsid.c_str());
  failRename = true;
  TEST_ASSERT_FALSE(cfg.save());
  failRename = false;
  loaded.load();
  TEST_ASSERT_EQUAL_STRING("old", loaded.wifiSsid.c_str());
  TEST_ASSERT_FALSE(testFiles.count("/NVS/awtrix-cfg.bin.tmp"));
}

void restore_and_disabled_stores() {
  DeviceConfig cfg;
  std::string err;
  backup::PicoRestoreSink sink(cfg, nullptr);
  TEST_ASSERT_TRUE(sink.applyWifi("restored", "test-pass", err));
  TEST_ASSERT_TRUE(cfg.wifiSsid.empty());
  sink.commit();
  DeviceConfig rebooted;
  rebooted.load();
  TEST_ASSERT_EQUAL_STRING("restored", rebooted.wifiSsid.c_str());
  // An aborted asset leaves the old file and no staging file behind.
  testFiles["/ICONS/test.gif"] = "old";
  TEST_ASSERT_TRUE(sink.beginFile("/ICONS/test.gif", err));
  TEST_ASSERT_TRUE(sink.writeFile(reinterpret_cast<const uint8_t*>("new"), 3));
  sink.abortFile();
  TEST_ASSERT_EQUAL_STRING("old", testFiles["/ICONS/test.gif"].c_str());
  TEST_ASSERT_FALSE(testFiles.count("/ICONS/test.gif.restore.tmp"));
  TEST_ASSERT_TRUE(sink.beginFile("/ICONS/test.gif", err));
  TEST_ASSERT_TRUE(sink.writeFile(reinterpret_cast<const uint8_t*>("new"), 3));
  TEST_ASSERT_TRUE(sink.endFile());
  TEST_ASSERT_EQUAL_STRING("new", testFiles["/ICONS/test.gif"].c_str());
  // A short write is never published.
  TEST_ASSERT_TRUE(sink.beginFile("/ICONS/test.gif", err));
  failWrite = true;
  TEST_ASSERT_FALSE(sink.writeFile(reinterpret_cast<const uint8_t*>("bad"), 3));
  failWrite = false;
  TEST_ASSERT_FALSE(sink.endFile());
  TEST_ASSERT_EQUAL_STRING("new", testFiles["/ICONS/test.gif"].c_str());
  TEST_ASSERT_FALSE(sink.beginFile("/NVS/awtrix-cfg.bin", err));
  TEST_ASSERT_FALSE(sink.beginFile("/ICONS/../bad", err));
  radiostore::save("[]");
  TEST_ASSERT_FALSE(radiostore::pending());
  TEST_ASSERT_FALSE(testFiles.count("/radio.json"));
}

void restore_creates_nested_asset_directories() {
  DeviceConfig cfg;
  std::string err;
  backup::PicoRestoreSink sink(cfg, nullptr);
  TEST_ASSERT_TRUE(sink.beginFile("/ICONS/sub/deeper/smile.gif", err));
  TEST_ASSERT_TRUE(sink.writeFile(reinterpret_cast<const uint8_t*>("GIF8"), 4));
  TEST_ASSERT_TRUE(sink.endFile());
  TEST_ASSERT_EQUAL_STRING("GIF8", testFiles["/ICONS/sub/deeper/smile.gif"].c_str());
  TEST_ASSERT_TRUE(testDirs.count("/ICONS") && testDirs.count("/ICONS/sub") &&
                   testDirs.count("/ICONS/sub/deeper"));
  TEST_ASSERT_FALSE(sink.beginFile("/ICONS/sub/", err));
  TEST_ASSERT_FALSE(sink.beginFile("/ICONS//x.gif", err));
  TEST_ASSERT_FALSE(sink.beginFile("/apploop.json", err));
  TEST_ASSERT_FALSE(sink.beginFile("/ICONSX/x.gif", err));
}

// R23: a backup made on an ESP32 restores everything but the ESP32's pins and panel.
void esp32_backup_keeps_the_pico_wiring() {
  DeviceConfig cfg;
  const DeviceConfig pico = cfg;
  std::string err;
  backup::PicoRestoreSink sink(cfg, nullptr);
  constexpr auto system = R"({"pinMatrix":32,"pinBtnLeft":26,"pinBtnSelect":27,"pinBtnRight":14,"pinBattery":34,"pinLdr":35,"pinBuzzer":15,"pinI2cSda":21,"pinI2cScl":22,"pinDfRx":23,"pinDfTx":18,"panelWidth":32,"panels":2,"mqttHost":"esp.broker","mqttEnabled":true})";
  TEST_ASSERT_TRUE_MESSAGE(sink.applySystem(system, err), err.c_str());
  sink.commit();
  DeviceConfig rebooted;
  rebooted.load();
  TEST_ASSERT_EQUAL_STRING("esp.broker", rebooted.mqttHost.c_str());
  TEST_ASSERT_TRUE(rebooted.mqttEnabled);
  TEST_ASSERT_EQUAL_INT(pico.pinMatrix, rebooted.pinMatrix);
  TEST_ASSERT_EQUAL_INT(pico.pinBtnLeft, rebooted.pinBtnLeft);
  TEST_ASSERT_EQUAL_INT(pico.pinLdr, rebooted.pinLdr);
  TEST_ASSERT_EQUAL_INT(pico.pinBuzzer, rebooted.pinBuzzer);
  TEST_ASSERT_EQUAL_INT(pico.pinI2cSda, rebooted.pinI2cSda);
  TEST_ASSERT_EQUAL_INT(pico.panelWidth, rebooted.panelWidth);
  TEST_ASSERT_EQUAL_INT(pico.panels, rebooted.panels);
  std::string pinErr;
  TEST_ASSERT_TRUE(rebooted.validatePins(pinErr));
}

// A Galactic Unicorn refuses panel edits it cannot draw, and a restore keeps its own pins and panel
// while taking the rest of a backup made on a board wired differently.
void fixed_board_system_config() {
  const auto& soc = pins::rp2040Profile();
  DeviceConfig cfg;
  cfg.setPinSet(soc.defaults);
  cfg.panelWidth = 53; cfg.panelHeight = 11;
  int applied = 0; sysconfig::ApplyError err;
  auto put = [&](const char* json, sysconfig::Origin origin = sysconfig::Origin::Interactive) {
    err = {};
    return sysconfig::apply(cfg, api::JsonReader(json), applied, err, origin);
  };
  TEST_ASSERT_FALSE(put(R"({"ldrOnGround":true})"));
  TEST_ASSERT_EQUAL(400, err.status);
  TEST_ASSERT_EQUAL_STRING("ldrOnGround", err.field.c_str());
  TEST_ASSERT_FALSE(cfg.ldrOnGround);
  TEST_ASSERT_TRUE(put(R"({"ldrOnGround":true,"hostname":"restored"})", sysconfig::Origin::Restore));
  TEST_ASSERT_FALSE(cfg.ldrOnGround);
  TEST_ASSERT_EQUAL_STRING("restored", cfg.hostname.c_str());
  TEST_ASSERT_FALSE(put(R"({"panelWidth":64})"));
  TEST_ASSERT_EQUAL(422, err.status); TEST_ASSERT_EQUAL_STRING("panelWidth", err.field.c_str());
  TEST_ASSERT_FALSE(put(R"({"panelHeight":14})"));
  TEST_ASSERT_EQUAL_STRING("panelHeight", err.field.c_str());
  TEST_ASSERT_FALSE(put(R"({"panelWiring":"columns"})"));
  TEST_ASSERT_EQUAL_STRING("panelWiring", err.field.c_str());
  TEST_ASSERT_EQUAL(53, cfg.panelWidth); TEST_ASSERT_EQUAL(11, cfg.panelHeight);
  TEST_ASSERT_TRUE(put(R"({"panelHeight":8,"mirror":true})"));
  TEST_ASSERT_EQUAL(8, cfg.panelHeight); TEST_ASSERT_TRUE(cfg.mirror);
  // An ESP32 backup: its own pins, width and wiring, and a height this panel cannot show.
  TEST_ASSERT_TRUE(put(R"({"hostname":"from-esp32","panelWidth":32,"panelHeight":16,)"
                       R"("panelWiring":"columns","pinMatrix":32,"pinBtnLeft":26,"pinLdr":45})",
                       sysconfig::Origin::Restore));
  TEST_ASSERT_EQUAL_STRING("from-esp32", cfg.hostname.c_str());
  TEST_ASSERT_EQUAL(53, cfg.panelWidth); TEST_ASSERT_EQUAL(8, cfg.panelHeight);
  TEST_ASSERT_TRUE(cfg.panelWiring == Wiring::Rows);
  TEST_ASSERT_EQUAL(-1, cfg.pinMatrix); TEST_ASSERT_EQUAL(0, cfg.pinBtnLeft);
  TEST_ASSERT_EQUAL(28, cfg.pinLdr);
  // Edited by hand, a pin past this chip's GPIOs is still out of range.
  TEST_ASSERT_FALSE(put(R"({"pinLdr":35})"));
  TEST_ASSERT_EQUAL_STRING("pinLdr", err.field.c_str());
  // A valid restored height is taken; fixed width, wiring and pins stay the board's own.
  TEST_ASSERT_TRUE(put(R"({"panelHeight":11})", sysconfig::Origin::Restore));
  TEST_ASSERT_EQUAL(11, cfg.panelHeight);
  TEST_ASSERT_TRUE(put(R"({"panelHeight":8})", sysconfig::Origin::Restore));
  TEST_ASSERT_EQUAL(8, cfg.panelHeight);
}
// Devices running PR 1 already store the height under "pheight", the key older upstream used for
// the same quantity. A stored value the Unicorn cannot show (outside 8-16, or 9, 16) never reaches
// its panel: the board adopts its default 11 rows at boot.
void stored_panel_height_outside_the_range_is_not_used() {
  for (int stored : {0, 7, 9, 16, 17, 99, -1}) {
    Preferences p;
    TEST_ASSERT_TRUE(p.begin("awtrix-cfg", false));
    p.putInt("pheight", stored);
    p.end();
    DeviceConfig cfg;
    cfg.load();
    TEST_ASSERT_EQUAL(stored, cfg.panelHeight);
    sysconfig::adoptFixedBoard(cfg, pins::rp2040Profile());
    TEST_ASSERT_EQUAL(11, cfg.panelHeight);
  }
}

void fixed_board_adopted_at_boot() {
  const auto& soc = pins::rp2040Profile();
  // What an earlier build may have stored: another pin map (I2C on 4/5) and panel size.
  DeviceConfig cfg;
  cfg.setPinSet(pins::esp32Profile().defaults);
  cfg.pinI2cSda = 4; cfg.pinI2cScl = 5;
  cfg.panelWidth = 64; cfg.panels = 2; cfg.panelHeight = 16;
  int applied = 0; sysconfig::ApplyError err;
  TEST_ASSERT_FALSE(sysconfig::apply(cfg, api::JsonReader(R"({"debugMode":true})"), applied, err));
  sysconfig::adoptFixedBoard(cfg, soc);
  TEST_ASSERT_EQUAL(-1, cfg.pinI2cSda); TEST_ASSERT_EQUAL(-1, cfg.pinI2cScl);
  TEST_ASSERT_EQUAL(28, cfg.pinLdr); TEST_ASSERT_EQUAL(-1, cfg.pinBuzzer);
  TEST_ASSERT_EQUAL(53, cfg.panelWidth); TEST_ASSERT_EQUAL(1, cfg.panels);
  TEST_ASSERT_EQUAL(11, cfg.panelHeight);
  TEST_ASSERT_TRUE(sysconfig::apply(cfg, api::JsonReader(R"({"debugMode":true})"), applied, err));
  cfg.panelHeight = 8;
  sysconfig::adoptFixedBoard(cfg, soc);
  TEST_ASSERT_EQUAL(8, cfg.panelHeight);
  // A board whose pins and panel are the user's is left alone.
  DeviceConfig esp;
  esp.pinI2cSda = 17; esp.panelWidth = 64; esp.panelHeight = 16;
  sysconfig::adoptFixedBoard(esp, pins::esp32Profile());
  TEST_ASSERT_EQUAL(17, esp.pinI2cSda); TEST_ASSERT_EQUAL(64, esp.panelWidth);
  TEST_ASSERT_EQUAL(16, esp.panelHeight);
}

void saved_melody_is_found_by_bare_name() {
  pico::AssetProbe assets;
  testFiles["/MELODIES/x.txt"] = "x:d=4,o=5,b=120:c";
  testFiles["/MP3/x.mp3"] = "test mp3";
  TEST_ASSERT_TRUE(assets.hasFile("/MELODIES/x.txt"));
  TEST_ASSERT_FALSE(assets.hasFile("/MELODIES/missing.txt"));
  TEST_ASSERT_TRUE(assets.hasFile("/MP3/x.mp3"));
  TEST_ASSERT_FALSE(assets.hasFile("/MP3/missing.mp3"));
  testFiles.erase("/MP3/x.mp3");
  struct Tone : sound::IToneSink {
    std::string asked;
    void begin() override {}
    void setVolumes(const sound::Volumes&) override {}
    bool playRtttl(const std::string&, sound::Group) override { return false; }
    bool playMelodyFile(const std::string& name, sound::Group) override {
      asked = name;
      return name == "x";
    }
    void stop() override {}
    void tick() override {}
    bool isPlaying() const override { return !asked.empty(); }
  } tone;
  sound::AudioRouter router;
  router.setTone(&tone);
  sound::Choices choices;
  DispatchDetail detail;
  TEST_ASSERT_TRUE(sound::parse(R"({"file":"x"})", sound::Origin::Play, choices, detail));
  // Reproduce the missing-wiring failure, then attach the production Pico probe.
  TEST_ASSERT_EQUAL(static_cast<int>(sound::PlayResult::NotFound),
      static_cast<int>(router.play(choices, sound::Group::Alert, "", detail)));
  router.setAssets(&assets);
  TEST_ASSERT_EQUAL(static_cast<int>(sound::PlayResult::Ok),
      static_cast<int>(router.play(choices, sound::Group::Alert, "", detail)));
  TEST_ASSERT_EQUAL_STRING("x", tone.asked.c_str());
}

void capabilities_report_pending_pico_height() {
  DeviceConfig cfg;
  cfg.panelHeight = 11;
  auto check = [&](int active, int requested, bool restart) {
    PlatformDescriptor platform{"rp2040", pico::displayProfile(53, active, cfg.panelHeight), false};
    const auto json = api::capabilitiesJson({}, {}, {}, {}, &platform);
    const auto display = api::memberValue(api::JsonReader(json), "display");
    long long value = 0;
    bool pending = false;
    TEST_ASSERT_TRUE(api::memberValue(display, "height").asLong(value));
    TEST_ASSERT_EQUAL(active, value);
    TEST_ASSERT_TRUE(api::memberValue(display, "requestedHeight").asLong(value));
    TEST_ASSERT_EQUAL(requested, value);
    TEST_ASSERT_TRUE(api::memberValue(display, "restartRequired").asBool(pending));
    TEST_ASSERT_EQUAL(restart, pending);
  };
  check(11, 11, false);
  int applied = 0;
  sysconfig::ApplyError err;
  TEST_ASSERT_TRUE(sysconfig::apply(cfg, api::JsonReader(R"({"panelHeight":8})"), applied, err));
  check(11, 8, true);
  check(8, 8, false); // after reboot, the board and canvas use the saved height
}

void asset_write_recreates_pruned_parents() {
  TEST_ASSERT_TRUE(fs::begin());
  TEST_ASSERT_TRUE(document::write("/MELODIES/last.txt", "last:d=8,o=5,b=160:c6"));
  TEST_ASSERT_TRUE(LittleFS.remove("/MELODIES/last.txt"));
  TEST_ASSERT_TRUE(LittleFS.rmdir("/MELODIES"));
  TEST_ASSERT_FALSE(LittleFS.exists("/MELODIES"));
  TEST_ASSERT_TRUE(document::write("/MELODIES/probe.txt", "probe:d=8,o=5,b=160:c6"));
  TEST_ASSERT_EQUAL_STRING("probe:d=8,o=5,b=160:c6", testFiles["/MELODIES/probe.txt"].c_str());
  for (const char* path : {"/ICONS/sub/test.gif", "/PALETTES/sub/test.json",
                           "/SCRIPTS/sub/source.js", "/SCRIPTS/sub/data.json"}) {
    TEST_ASSERT_TRUE(document::write(path, "asset"));
    TEST_ASSERT_EQUAL_STRING("asset", testFiles[path].c_str());
  }
}

void read_errors_never_move_the_config() {
  DeviceConfig cfg;
  cfg.wifiSsid = "intact";
  TEST_ASSERT_TRUE(cfg.save());
  const auto original = testFiles["/NVS/awtrix-cfg.bin"];
  for (bool shortFailure : {false, true}) {
    failOpen = !shortFailure;
    shortRead = shortFailure;
    cfg.wifiSsid = "changed";
    TEST_ASSERT_FALSE(cfg.save());
    TEST_ASSERT_EQUAL_STRING(original.c_str(), testFiles["/NVS/awtrix-cfg.bin"].c_str());
    TEST_ASSERT_FALSE(LittleFS.exists("/NVS/awtrix-cfg.bin.bad"));
  }
  for (const auto& damaged : {std::string("bad"), std::string(storage::KvPreferences::kMaxBytes + 1, 'x')}) {
    testFiles["/NVS/awtrix-cfg.bin"] = damaged;
    failWrite = true;
    TEST_ASSERT_FALSE(cfg.save());
    failWrite = false;
    TEST_ASSERT_EQUAL_STRING(damaged.c_str(), testFiles["/NVS/awtrix-cfg.bin"].c_str());
    failRename = true;
    TEST_ASSERT_FALSE(cfg.save());
    failRename = false;
    TEST_ASSERT_EQUAL_STRING(damaged.c_str(), testFiles["/NVS/awtrix-cfg.bin"].c_str());
    TEST_ASSERT_TRUE(cfg.save());
    TEST_ASSERT_EQUAL_STRING(damaged.c_str(), testFiles["/NVS/awtrix-cfg.bin.bad"].c_str());
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(read_errors_never_move_the_config);
  RUN_TEST(asset_write_recreates_pruned_parents);
  RUN_TEST(capabilities_report_pending_pico_height);
  RUN_TEST(saved_melody_is_found_by_bare_name);
  RUN_TEST(legacy_panel_height_is_ignored_and_removed);
  RUN_TEST(panel_height_uses_fresh_key);
  RUN_TEST(config_and_settings_reboot_roundtrip);
  RUN_TEST(failed_writes_keep_last_config);
  RUN_TEST(restore_and_disabled_stores);
  RUN_TEST(restore_creates_nested_asset_directories);
  RUN_TEST(esp32_backup_keeps_the_pico_wiring);
  RUN_TEST(fixed_board_system_config);
  RUN_TEST(fixed_board_adopted_at_boot);
  RUN_TEST(stored_panel_height_outside_the_range_is_not_used);
  RUN_TEST(damaged_config_is_saved_afresh);
  return UNITY_END();
}
