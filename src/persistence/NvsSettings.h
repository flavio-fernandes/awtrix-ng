#pragma once

#include "core/Settings.h"

namespace awtrix {
namespace nvs {

void loadSettings(Settings& s);
// True once the snapshot is stored. On the Pico that means committed to LittleFS at end().
bool saveSettings(const Settings& s);

}
}
