#include "persistence/DocumentFile.h"

#include <LittleFS.h>
#if defined(AWTRIX_PLATFORM_RP2040)
#include "platform/rp2040/AssetDirectories.h"
#endif

namespace awtrix::document {

bool write(const char* path, const std::string& bytes) {
#if defined(AWTRIX_PLATFORM_RP2040)
  if (!pico::ensureAssetParents(path)) return false;
#endif
  constexpr const char* temporary = "/.document.tmp";
  File file = LittleFS.open(temporary, "w");
  if (!file) return false;
  const bool complete = file.write(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()) == bytes.size();
  file.flush();
  file.close();
  if (complete && LittleFS.rename(temporary, path)) return true;
  LittleFS.remove(temporary);
  return false;
}

}
