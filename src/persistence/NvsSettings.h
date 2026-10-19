#pragma once

#include "core/Settings.h"

namespace awtrix {
namespace nvs {

void loadSettings(Settings& s);
// True once the snapshot is stored. On the Pico that means committed to LittleFS at end().
bool saveSettings(const Settings& s);
// True while the last saveSettings() did not store its snapshot: a restore then answers 507, and
// the device's own retry (the settings stay dirty) clears it.
bool settingsPending();

}
}
