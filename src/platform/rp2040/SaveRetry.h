#pragma once

namespace awtrix {

// A LittleFS commit can fail for a moment (a full volume being trimmed, an open that finds no
// heap). Before a reboot it is the last chance, so it gets a few tries; save returns true once
// stored and wait runs between tries. Kept portable so host tests can count the attempts.
template <typename Save, typename Wait>
bool saveWithRetries(Save save, Wait wait, int attempts = 3) {
  for (int attempt = 1; attempt <= attempts; ++attempt) {
    if (save()) return true;
    if (attempt < attempts) wait();
  }
  return false;
}

}
