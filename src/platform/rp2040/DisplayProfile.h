#pragma once

#include "core/render/DisplayProfile.h"
#include "hal/GalacticUnicornDisplay.h"

namespace awtrix::pico {
// The board and runtime canvas keep their boot geometry until restart.
inline DisplayProfile displayProfile(int width, int activeHeight, int requestedHeight) {
  DisplayProfile display{width, activeHeight, true};
  // Eight rows is compatibility mode; ConfigRules rejects nine and ten.
  display.limits = {53, 53, 8, 11, 53 * 11};
  display.requestedHeight = galactic::sanitizeHeight(requestedHeight);
  return display;
}
}
