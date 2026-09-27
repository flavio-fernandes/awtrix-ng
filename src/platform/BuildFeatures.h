#pragma once
#include "core/FeatureSet.h"

// Scripting is a build option on RP2040, off unless the env sets -D AWTRIX_FEATURE_SCRIPTING=1
// (see [env:galactic_unicorn_scripting]); everywhere else it is on unless turned off.
#ifndef AWTRIX_FEATURE_SCRIPTING
#if defined(AWTRIX_PLATFORM_RP2040)
#define AWTRIX_FEATURE_SCRIPTING 0
#else
#define AWTRIX_FEATURE_SCRIPTING 1
#endif
#endif
#ifndef AWTRIX_FEATURE_MP3
#define AWTRIX_FEATURE_MP3 1
#endif
#ifndef AWTRIX_FEATURE_RADIO
#define AWTRIX_FEATURE_RADIO 1
#endif
#ifndef AWTRIX_FEATURE_OUTBOUND_TLS
#define AWTRIX_FEATURE_OUTBOUND_TLS 1
#endif
#ifndef AWTRIX_FEATURE_BROWSER_OTA
#define AWTRIX_FEATURE_BROWSER_OTA 1
#endif

namespace awtrix::platform {
inline constexpr FeatureSet buildFeatures() {
  return {bool(AWTRIX_FEATURE_SCRIPTING), bool(AWTRIX_FEATURE_MP3),
          bool(AWTRIX_FEATURE_RADIO), bool(AWTRIX_FEATURE_OUTBOUND_TLS),
          bool(AWTRIX_FEATURE_BROWSER_OTA)};
}
}
