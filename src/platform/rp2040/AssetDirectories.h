#pragma once

#include <LittleFS.h>
#include <string>

namespace awtrix::pico {
// arduino-pico does not create missing parents on open or rename (ESP32 does).
inline bool ensureAssetParents(const std::string& path) {
  for (auto at = path.find('/', 1); at != std::string::npos; at = path.find('/', at + 1)) {
    const auto dir = path.substr(0, at);
    if (!LittleFS.exists(dir.c_str()) && !LittleFS.mkdir(dir.c_str())) return false;
  }
  return true;
}
}
