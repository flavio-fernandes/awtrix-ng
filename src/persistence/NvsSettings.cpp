#include "persistence/NvsSettings.h"

#include <Preferences.h>

#include <string>

#include "core/SettingsBlob.h"

namespace awtrix {
namespace nvs {

namespace {
const char* kNs = "awtrix-ng";
// All settings live in one JSON blob under a single key: they change far more often than the
// device config, and one blob costs one NVS write instead of dozens.
const char* kKey = "settings";
}

void loadSettings(Settings& s) {
  Preferences p;
  if (!p.begin(kNs, true)) return;
  const String blob = p.getString(kKey, "");
  p.end();
  settingsblob::apply(s, std::string_view(blob.c_str(), blob.length()));
}

bool saveSettings(const Settings& s) {
  const std::string out = settingsblob::encode(s);
  Preferences p;
  const bool wrote = p.begin(kNs, false) && p.putString(kKey, out.c_str()) == out.size();
#if defined(AWTRIX_PLATFORM_RP2040)
  // The Pico commits the namespace file at end(), so that is where a full or failing LittleFS shows.
  return p.end() && wrote;
#else
  p.end();
  return wrote;
#endif
}

}
}
