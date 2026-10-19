#pragma once

namespace awtrix {

// A factory reset is queued, as on the ESP32 (system/DeviceServices.h): the HTTP or MQTT caller
// gets its answer first, and the format runs on the way to the reboot. Once one is pending nothing
// is saved, so dirty settings, scripts and app order cannot recreate files after the format. Kept
// portable so host tests can substitute the board operations.
class PicoFactoryReset {
 public:
  void request() { pending_ = true; }
  bool pending() const { return pending_; }

  // Called just before the reboot: the pending saves, or, after a reset request, the format alone.
  template <typename Save, typename StopWatchdog, typename Format>
  void beforeReboot(Save save, StopWatchdog stopWatchdog, Format format) const {
    if (!pending_) {
      save();
      return;
    }
    // A format can outlast the watchdog.
    stopWatchdog();
    format();
  }

 private:
  bool pending_ = false;
};

}
