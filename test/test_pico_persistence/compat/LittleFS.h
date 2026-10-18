#pragma once
// Deterministic filesystem test double, NOT a flash emulator. Used to exercise
// the production adapter, config and restore code with injected I/O failures.
#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <string>
inline std::map<std::string, std::string> testFiles;
inline std::set<std::string> testDirs;
inline bool failWrite = false, failRename = false;
class File {
 public:
  File() = default;
  File(std::string path, bool write) : path_(std::move(path)), open_(true) {
    if (write) testFiles[path_].clear();
  }
  explicit operator bool() const { return open_; }
  std::size_t size() const { return testFiles[path_].size(); }
  bool isDirectory() const { return testDirs.count(path_); }
  std::size_t write(const uint8_t* b, std::size_t n) {
    if (!open_) return 0;
    if (failWrite && n) --n;
    testFiles[path_].append(reinterpret_cast<const char*>(b), n); return n;
  }
  std::size_t print(const char* s) { return write(reinterpret_cast<const uint8_t*>(s), std::strlen(s)); }
  std::size_t read(uint8_t* b, std::size_t n) {
    if (!open_) return 0;
    auto& value = testFiles[path_];
    n = std::min(n, value.size() - at_);
    if (n) std::memcpy(b, value.data() + at_, n);
    at_ += n; return n;
  }
  int read() { uint8_t b; return read(&b, 1) ? b : -1; }
  String readString() { return String(testFiles[path_].c_str()); }
  void flush() {}
  void close() { open_ = false; }
 private:
  std::string path_;
  bool open_ = false;
  std::size_t at_ = 0;
};
class LittleFSConfig { public: void setAutoFormat(bool) {} };
struct FSInfo { std::size_t totalBytes = 524288, usedBytes = 0; };
class TestFS {
 public:
  void setConfig(const LittleFSConfig&) {}
  bool begin() { return true; }
  bool info(FSInfo& info) { for (const auto& f : testFiles) info.usedBytes += f.second.size(); return true; }
  bool exists(const char* p) { return testFiles.count(p) || testDirs.count(p); }
  bool mkdir(const char* p) { testDirs.insert(p); return true; }
  bool parentExists(const std::string& path) {
    const auto slash = path.rfind('/');
    return slash == 0 || slash == std::string::npos || testDirs.count(path.substr(0, slash));
  }
  bool rmdir(const char* p) {
    const std::string prefix = std::string(p) + '/';
    for (const auto& f : testFiles) if (f.first.find(prefix) == 0) return false;
    for (const auto& d : testDirs) if (d.find(prefix) == 0) return false;
    return testDirs.erase(p);
  }
  File open(const char* p, const char* mode = "r") {
    if (mode[0] == 'w' && !parentExists(p)) return {};
    return mode[0] == 'w' || exists(p) ? File(p, mode[0] == 'w') : File();
  }
  bool remove(const char* p) { return testFiles.erase(p); }
  bool rename(const char* from, const char* to) {
    if (failRename || !testFiles.count(from) || !parentExists(to)) return false;
    testFiles[to] = testFiles[from]; testFiles.erase(from); return true;
  }
};
inline TestFS LittleFS;
