#include <unity.h>
#include <vector>
#include "persistence/KvPreferences.h"
using namespace awtrix::storage;

class MemoryDisk : public KvBackend {
 public:
  std::map<std::string, std::string> files;
  bool fail = false;
  int writes = 0;
  std::vector<std::string> setAside;
  Read read(const std::string& n, std::string& b) override {
    if (!files.count(n)) return Read::Missing;
    b = files[n]; return Read::Ok;
  }
  bool replace(const std::string& n, const std::string& b) override {
    if (fail) return false;
    files[n] = b; ++writes; return true;
  }
  void setAside(const std::string& n) override { setAside.push_back(n); }
};
void setUp() {}
void tearDown() {}
void roundtrip() {
  MemoryDisk disk;
  {
    KvPreferences p(disk);
    TEST_ASSERT_TRUE(p.begin("awtrix-cfg"));
    TEST_ASSERT_EQUAL(8, p.put("ssid", KvPreferences::String, "test-net"));
    p.put("pass", KvPreferences::String, "test-only");
    p.putNumber("signed", KvPreferences::Int, uint32_t(-123), 4);
    p.putNumber("uint", KvPreferences::UInt, 0xffffffffu, 4);
    p.putNumber("bool", KvPreferences::Bool, 1, 1);
    p.putNumber("uchar", KvPreferences::UChar, 255, 1);
    p.putNumber("ushort", KvPreferences::UShort, 65535, 2);
    p.putNumber("float", KvPreferences::Float, 0xc1100000u, 4);
    p.put("empty", KvPreferences::String, "");
    TEST_ASSERT_EQUAL(0, disk.writes);
    TEST_ASSERT_TRUE(p.end());
  }
  KvPreferences rebooted(disk);
  TEST_ASSERT_TRUE(rebooted.begin("awtrix-cfg", true));
  TEST_ASSERT_EQUAL_STRING("test-net", rebooted.get("ssid", KvPreferences::String).c_str());
  TEST_ASSERT_EQUAL_STRING("test-only", rebooted.get("pass", KvPreferences::String).c_str());
  TEST_ASSERT_EQUAL_UINT32(uint32_t(-123), rebooted.getNumber("signed", KvPreferences::Int, 0));
  TEST_ASSERT_EQUAL_UINT32(0xffffffffu, rebooted.getNumber("uint", KvPreferences::UInt, 0));
  TEST_ASSERT_EQUAL(1, rebooted.getNumber("bool", KvPreferences::Bool, 0));
  TEST_ASSERT_EQUAL(255, rebooted.getNumber("uchar", KvPreferences::UChar, 0));
  TEST_ASSERT_EQUAL(65535, rebooted.getNumber("ushort", KvPreferences::UShort, 0));
  TEST_ASSERT_EQUAL_HEX32(0xc1100000u, rebooted.getNumber("float", KvPreferences::Float, 0));
  TEST_ASSERT_TRUE(rebooted.isKey("empty"));
  TEST_ASSERT_EQUAL_STRING("", rebooted.get("empty", KvPreferences::String, "fallback").c_str());
  TEST_ASSERT_EQUAL(77, rebooted.getNumber("signed", KvPreferences::UInt, 77));
  TEST_ASSERT_EQUAL(0, rebooted.put("ssid", KvPreferences::String, "no"));
  TEST_ASSERT_FALSE(rebooted.clear());
  TEST_ASSERT_FALSE(rebooted.remove("ssid"));
  TEST_ASSERT_TRUE(rebooted.end());
  TEST_ASSERT_EQUAL(1, disk.writes);
}
void corrupt_snapshots_are_rejected() {
  MemoryDisk disk; KvPreferences p(disk);
  p.begin("ns"); p.put("key", KvPreferences::String, "value"); p.end();
  const auto valid = disk.files["ns"];
  // Every truncation and every single-byte mutation, including type and checksum.
  // A read-only open (every load) refuses them, so the defaults apply.
  for (std::size_t n = 0; n < valid.size(); ++n) {
    disk.files["ns"] = valid.substr(0, n);
    TEST_ASSERT_FALSE(p.begin("ns", true));
    TEST_ASSERT_FALSE(p.isKey("key"));
    disk.files["ns"] = valid;
    disk.files["ns"][n] ^= 1;
    TEST_ASSERT_FALSE(p.begin("ns", true));
  }
  disk.files["ns"] = valid + "trailing";
  TEST_ASSERT_FALSE(p.begin("ns", true));
  disk.files["ns"] = std::string(KvPreferences::kMaxBytes + 1, 'x');
  TEST_ASSERT_FALSE(p.begin("ns", true));
  TEST_ASSERT_TRUE(disk.setAside.empty());
}
// A write starts a damaged namespace afresh instead of blocking every later save.
void damaged_snapshots_are_replaced_on_write() {
  MemoryDisk disk; KvPreferences p(disk);
  p.begin("ns"); p.put("key", KvPreferences::String, "value"); p.end();
  auto damaged = disk.files["ns"];
  damaged[damaged.size() / 2] ^= 1;
  const std::string versions[] = {damaged, std::string(KvPreferences::kMaxBytes + 1, 'x'),
                                  "AXKV\x7f" + disk.files["ns"].substr(5)};
  for (const auto& bad : versions) {
    disk.files["ns"] = bad;
    disk.setAside.clear();
    TEST_ASSERT_TRUE(p.begin("ns", false));
    TEST_ASSERT_EQUAL(1, disk.setAside.size());
    TEST_ASSERT_EQUAL_STRING("ns", disk.setAside[0].c_str());
    TEST_ASSERT_FALSE(p.isKey("key"));
    TEST_ASSERT_EQUAL(5, p.put("fresh", KvPreferences::String, "saved"));
    TEST_ASSERT_TRUE(p.end());
    KvPreferences reader(disk);
    TEST_ASSERT_TRUE(reader.begin("ns", true));
    TEST_ASSERT_EQUAL_STRING("saved", reader.get("fresh", KvPreferences::String).c_str());
    TEST_ASSERT_FALSE(reader.isKey("key"));
    reader.end();
  }
  // Even with nothing put, end() publishes an empty snapshot over the bad one.
  disk.files["ns"] = damaged;
  TEST_ASSERT_TRUE(p.begin("ns"));
  TEST_ASSERT_TRUE(p.end());
  TEST_ASSERT_TRUE(p.begin("ns", true));
  p.end();
}
void atomic_failure_and_namespaces() {
  MemoryDisk disk; KvPreferences p(disk);
  TEST_ASSERT_FALSE(p.begin("missing", true));
  TEST_ASSERT_FALSE(p.begin("../bad"));
  p.begin("one"); p.put("key", KvPreferences::String, "old"); p.end();
  p.begin("two"); p.put("key", KvPreferences::String, "other"); p.end();
  p.begin("one"); p.put("key", KvPreferences::String, "new");
  disk.fail = true; TEST_ASSERT_FALSE(p.end()); disk.fail = false;
  p.begin("one"); TEST_ASSERT_EQUAL_STRING("old", p.get("key", KvPreferences::String).c_str());
  const auto writes = disk.writes;
  p.put("key", KvPreferences::String, "old"); p.end(); TEST_ASSERT_EQUAL(writes, disk.writes);
  p.begin("one");
  TEST_ASSERT_EQUAL(0, p.put("../bad", KvPreferences::String, "bad"));
  TEST_ASSERT_EQUAL(0, p.put("large", KvPreferences::String, std::string(KvPreferences::kMaxBytes, 'x')));
  TEST_ASSERT_EQUAL(0, p.putNumber("bad", KvPreferences::Bool, 2, 1));
  TEST_ASSERT_TRUE(p.remove("key")); p.end();
  p.begin("one", true); TEST_ASSERT_FALSE(p.isKey("key")); p.end();
  p.begin("two", true); TEST_ASSERT_EQUAL_STRING("other", p.get("key", KvPreferences::String).c_str()); p.end();
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(roundtrip);
  RUN_TEST(corrupt_snapshots_are_rejected);
  RUN_TEST(damaged_snapshots_are_replaced_on_write);
  RUN_TEST(atomic_failure_and_namespaces);
  return UNITY_END();
}
