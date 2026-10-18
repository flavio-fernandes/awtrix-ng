#include <LittleFS.h>
#include <Preferences.h>
#include <nvs.h>

namespace awtrix::storage {
namespace {
void (*g_log)(const char* line) = nullptr;

// /NVS/<namespace>.bin, replaced through a verified temporary so a power cut leaves either the
// old or the new snapshot.
class Backend final : public KvBackend {
 public:
  Read read(const std::string& name, std::string& bytes) override {
    const auto path = "/NVS/" + name + ".bin";
    if (!LittleFS.exists(path.c_str())) return Read::Missing;
    File f = LittleFS.open(path.c_str(), "r");
    if (!f || f.size() > KvPreferences::kMaxBytes) return Read::Error;
    bytes.resize(f.size());
    return f.read(reinterpret_cast<uint8_t*>(&bytes[0]), bytes.size()) == bytes.size()
               ? Read::Ok : Read::Error;
  }
  bool replace(const std::string& name, const std::string& bytes) override {
    if (!LittleFS.exists("/NVS") && !LittleFS.mkdir("/NVS")) return false;
    const auto path = "/NVS/" + name + ".bin";
    const auto temp = path + ".tmp";
    File f = LittleFS.open(temp.c_str(), "w");
    if (!f) return false;
    bool ok = f.write(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()) == bytes.size();
    f.flush();
    f.close();
    // Check the staged bytes before publishing. Never delete the old snapshot first.
    f = LittleFS.open(temp.c_str(), "r");
    ok = ok && f && f.size() == bytes.size();
    for (std::size_t i = 0; ok && i < bytes.size(); ++i) ok = f.read() == uint8_t(bytes[i]);
    if (f) f.close();
    if (ok && LittleFS.rename(temp.c_str(), path.c_str())) return true;
    LittleFS.remove(temp.c_str());
    return false;
  }
  // Renamed, so the next read finds no file and the line is logged once.
  void setAside(const std::string& name) override {
    const auto path = "/NVS/" + name + ".bin";
    if (!LittleFS.exists(path.c_str())) return;
    const auto bad = path + ".bad";
    LittleFS.remove(bad.c_str());
    const bool kept = LittleFS.rename(path.c_str(), bad.c_str());
    if (!g_log) return;
    const std::string line = "nvs: " + path + " is damaged, starting it afresh" +
                             (kept ? " (kept as " + bad + ")" : "");
    g_log(line.c_str());
  }
};
}
void setKvLog(void (*log)(const char* line)) { g_log = log; }
KvBackend& littleFsKvBackend() { static Backend backend; return backend; }
KvPreferences& nvsHandle() { static KvPreferences store(littleFsKvBackend()); return store; }
}

void Preferences::end() { store_.end(); }
