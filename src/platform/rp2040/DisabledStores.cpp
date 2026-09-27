#include "persistence/ScriptStore.h"
#include "persistence/RadioStore.h"
#include "platform/BuildFeatures.h"

namespace awtrix {
// The scripting build links the real persistence/ScriptStore.cpp instead.
#if !AWTRIX_FEATURE_SCRIPTING
void ScriptStore::save(const std::string&, const std::string&) {}
void ScriptStore::remove(const std::string&) {}
void ScriptStore::loadAll(const LoadFn&) {}
std::vector<std::string> ScriptStore::names() const { return {}; }
bool ScriptStore::readSource(const std::string&, std::string& out) const { out.clear(); return false; }
bool ScriptStore::readStore(const std::string&, std::string& out) const { out.clear(); return false; }
void ScriptStore::storeChanged(const std::string&, const std::string&) {}
void ScriptStore::tick(int64_t) {}
void ScriptStore::flush() {}
#endif
namespace radiostore {
void save(const std::string&) {}
void load(CoreEngine&) {}
}
}
