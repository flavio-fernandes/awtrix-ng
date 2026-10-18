#pragma once

#include <LittleFS.h>
#include "core/sound/AudioSinks.h"

namespace awtrix::pico {
// arduino-pico exposes LittleFS through File, not the ESP32's POSIX VFS.
class AssetProbe final : public sound::IAssetProbe {
 public:
  bool hasFile(const std::string& path) const override {
    return LittleFS.exists(path.c_str());
  }
};
}
