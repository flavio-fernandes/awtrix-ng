#pragma once

namespace awtrix {

// A factory reset must never return to the runtime: dirty settings, scripts and
// app order could otherwise recreate files after the format. Keep the sequence
// portable so host tests can substitute the board operations.
template <typename StopWatchdog, typename Format, typename Reboot>
[[noreturn]] void picoFactoryReset(StopWatchdog stopWatchdog, Format format, Reboot reboot) {
  stopWatchdog();
  format();
  reboot();
  for (;;) {} // Also prevent persistence if the hardware reboot call returns.
}

}
