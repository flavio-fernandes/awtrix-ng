#include <LittleFS.h>
#include <Preferences.h>
#include <nvs.h>

namespace awtrix::storage {
namespace {
void (*g_log)(const char* line) = nullptr;

// /NVS/<namespace>.bin is never moved before a verified replacement is published.
class Backend final : public KvBackend {
 public:
  Read read(const std::string& name, std::string& bytes) override {
    const auto path = "/NVS/" + name + ".bin";
    if (!LittleFS.exists(path.c_str())) return Read::Missing;
    File f = LittleFS.open(path.c_str(), "r");
    if (!f) return Read::Error;
    if (f.size() > KvPreferences::kMaxBytes) return Read::Oversized;
    bytes.resize(f.size());
    return bytes.empty() || f.read(reinterpret_cast<uint8_t*>(&bytes[0]), bytes.size()) == bytes.size()
               ? Read::Ok : Read::Error;
  }
  bool replace(const std::string& name, const std::string& bytes, bool damaged) override {
    if (!LittleFS.exists("/NVS") && !LittleFS.mkdir("/NVS")) return false;
    const auto path = "/NVS/" + name + ".bin";
    const auto temp = path + ".tmp", stagedBad = path + ".bad.tmp", bad = path + ".bad";
    File f = LittleFS.open(temp.c_str(), "w");
    if (!f) return false;
    bool ok = f.write(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()) == bytes.size();
    f.flush();
    f.close();
    f = LittleFS.open(temp.c_str(), "r");
    ok = ok && f && f.size() == bytes.size();
    for (std::size_t i = 0; ok && i < bytes.size(); ++i) ok = f.read() == uint8_t(bytes[i]);
    if (f) f.close();
    // Copy even oversized damage in bounded chunks. A failed copy/save leaves the original
    // at its live path; a power cut never turns a transient I/O error into a missing config.
    if (ok && damaged) ok = copy(path, stagedBad);
    if (ok && LittleFS.rename(temp.c_str(), path.c_str())) {
      if (damaged) {
        const bool kept = LittleFS.rename(stagedBad.c_str(), bad.c_str());
        if (!kept) LittleFS.remove(stagedBad.c_str());
        if (g_log) {
          const std::string line = "nvs: " + path + " is damaged, starting it afresh" +
                                   (kept ? " (kept as " + bad + ")" : "");
          g_log(line.c_str());
        }
      }
      return true;
    }
    LittleFS.remove(temp.c_str());
    LittleFS.remove(stagedBad.c_str());
    return false;
  }
 private:
  static bool copy(const std::string& from, const std::string& to) {
    File input = LittleFS.open(from.c_str(), "r");
    if (!input) return false;
    File output = LittleFS.open(to.c_str(), "w");
    if (!output) return false;
    uint8_t buffer[128];
    auto remaining = input.size();
    while (remaining) {
      const auto n = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
      if (input.read(buffer, n) != n || output.write(buffer, n) != n) return false;
      remaining -= n;
    }
    output.flush();
    output.close();
    return true;
  }
};
}
void setKvLog(void (*log)(const char* line)) { g_log = log; }
KvBackend& littleFsKvBackend() { static Backend backend; return backend; }
KvPreferences& nvsHandle() { static KvPreferences store(littleFsKvBackend()); return store; }
}

bool Preferences::end() { return store_.end(); }
