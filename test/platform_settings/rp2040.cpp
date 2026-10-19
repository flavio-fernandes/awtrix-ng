#include <unity.h>
#include "persistence/SystemConfigApply.h"
#include "platform/rp2040/Rp2040Profile.h"
#include "core/SocProfileJson.h"
using namespace awtrix;
void setUp() {}
void tearDown() {}

static void panel_heights_and_capabilities() {
  DeviceConfig cfg;
  TEST_ASSERT_EQUAL(53, cfg.panelWidth);
  TEST_ASSERT_EQUAL(11, cfg.panelHeight);
  sysconfig::ApplyError err;
  int n;
  for (int h : {8, 11}) {
    const auto body = "{\"panelHeight\":" + std::to_string(h) + "}";
    TEST_ASSERT_TRUE(sysconfig::apply(cfg, api::JsonReader(body), n, err));
    TEST_ASSERT_EQUAL(h, cfg.panelHeight);
  }
  for (const char* body : {"{\"panelHeight\":9}", "{\"panelHeight\":10}",
                          "{\"panelHeight\":12}", "{\"panelWidth\":32}",
                          "{\"panels\":2}", "{\"panelSerpentine\":false}"}) {
    TEST_ASSERT_FALSE(sysconfig::apply(cfg, api::JsonReader(body), n, err));
    TEST_ASSERT_EQUAL(422, err.status);
    TEST_ASSERT_EQUAL_STRING("validationFailed", err.code.c_str());
    TEST_ASSERT_EQUAL(11, cfg.panelHeight);
    TEST_ASSERT_EQUAL(53, cfg.panelWidth);
  }
  const auto json = pins::toJson(pins::rp2040Profile());
  TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("\"panel\":{\"width\":53,\"heights\":[8,11]}"));
  TEST_ASSERT_TRUE(sysconfig::apply(cfg, api::JsonReader("{\"mirror\":true,\"rotate\":true}"), n, err));
  TEST_ASSERT_TRUE(cfg.mirror && cfg.rotate);
}

static void interactive_edits_reject_pins_atomically() {
  struct Case { const char* body; int status; const char* code; const char* field; };
  // A GPIO number outside the SoC's range fails the shared field rule, as on the ESP32; a valid
  // number that changes the soldered wiring is PR 1's 400 invalidPinConfig.
  const Case cases[] = {
      {R"({"pinMatrix":99})", 422, "validationFailed", "pinMatrix"},
      {R"({"pinLdr":35})", 422, "validationFailed", "pinLdr"},
      {R"({"pinBuzzer":5})", 400, "invalidPinConfig", ""},
      {R"({"pinBtnLeft":-1,"mqttHost":"not-applied"})", 400, "invalidPinConfig", ""},
      {R"({"pinI2sBclk":12})", 400, "invalidPinConfig", ""},
      {R"({"dfplayer":true,"mqttHost":"not-applied"})", 400, "invalidPinConfig", ""},
  };
  for (const auto& c : cases) {
    DeviceConfig cfg;
    sysconfig::ApplyError err;
    int n;
    TEST_ASSERT_FALSE(sysconfig::apply(cfg, api::JsonReader(c.body), n, err));
    TEST_ASSERT_EQUAL_MESSAGE(c.status, err.status, c.body);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(c.code, err.code.c_str(), c.body);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(c.field, err.field.c_str(), c.body);
    TEST_ASSERT_TRUE(cfg.mqttHost.empty());
    TEST_ASSERT_EQUAL(0, cfg.pinBtnLeft);
    TEST_ASSERT_EQUAL(-1, cfg.pinMatrix);
  }
}

static void fixed_pin_profile() {
  DeviceConfig cfg;
  std::string err;
  TEST_ASSERT_TRUE(cfg.validatePins(err));
  cfg.pinBuzzer = 5;
  TEST_ASSERT_FALSE(cfg.validatePins(err));
}

static void restores_an_esp32_backup_with_its_own_wiring() {
  DeviceConfig cfg;
  cfg.panelHeight = 8;
  const auto before = cfg;
  constexpr auto backup = R"({"pinMatrix":32,"pinBtnLeft":26,"pinBtnSelect":27,"pinBtnRight":14,"pinBattery":34,"pinLdr":35,"pinBuzzer":15,"pinI2cSda":21,"pinI2cScl":22,"panelWidth":32,"panelHeight":16,"panels":2,"mqttHost":"esp.broker","mqttEnabled":true})";
  sysconfig::ApplyError err;
  int n;
  TEST_ASSERT_TRUE(sysconfig::apply(cfg, api::JsonReader(backup), n, err, sysconfig::Origin::Restore));
  TEST_ASSERT_TRUE(cfg.mqttEnabled);
  TEST_ASSERT_EQUAL_STRING("esp.broker", cfg.mqttHost.c_str());
  TEST_ASSERT_EQUAL(before.pinMatrix, cfg.pinMatrix);
  TEST_ASSERT_EQUAL(before.pinLdr, cfg.pinLdr);
  TEST_ASSERT_EQUAL(before.pinI2cSda, cfg.pinI2cSda);
  TEST_ASSERT_EQUAL(before.panelWidth, cfg.panelWidth);
  TEST_ASSERT_EQUAL(before.panelHeight, cfg.panelHeight);
  TEST_ASSERT_EQUAL(before.panels, cfg.panels);
  std::string pinErr;
  TEST_ASSERT_TRUE(cfg.validatePins(pinErr));
}
static void restores_dfplayer_backup_without_replacing_fixed_flags() {
  DeviceConfig cfg;
  sysconfig::ApplyError err;
  int n;
  constexpr auto backup = R"({"dfplayer":true,"pinBattery":34,"pinI2cSda":21,"pinI2cScl":22,"ldrOnGround":true,"mqttEnabled":true,"mqttHost":"esp.broker","mqttPort":1884,"mqttPrefix":"restored"})";
  TEST_ASSERT_TRUE(sysconfig::apply(cfg, api::JsonReader(backup), n, err, sysconfig::Origin::Restore));
  TEST_ASSERT_FALSE(cfg.dfplayer);
  TEST_ASSERT_TRUE(cfg.mqttEnabled);
  TEST_ASSERT_EQUAL_STRING("esp.broker", cfg.mqttHost.c_str());
  TEST_ASSERT_EQUAL_STRING("restored", cfg.mqttPrefix.c_str());
  TEST_ASSERT_EQUAL(1884, cfg.mqttPort);
  TEST_ASSERT_EQUAL(pins::activeProfile().defaults.battery, cfg.pinBattery);
  TEST_ASSERT_EQUAL(pins::activeProfile().defaults.i2cSda, cfg.pinI2cSda);
  TEST_ASSERT_EQUAL(pins::activeProfile().defaults.i2cScl, cfg.pinI2cScl);
  // The light sensor is soldered, so a restore keeps the board's own polarity (review item N5).
  TEST_ASSERT_FALSE(cfg.ldrOnGround);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(restores_dfplayer_backup_without_replacing_fixed_flags);
  RUN_TEST(panel_heights_and_capabilities);
  RUN_TEST(fixed_pin_profile);
  RUN_TEST(interactive_edits_reject_pins_atomically);
  RUN_TEST(restores_an_esp32_backup_with_its_own_wiring);
  return UNITY_END();
}
