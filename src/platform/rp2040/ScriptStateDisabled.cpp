#include "platform/BuildFeatures.h"

// The no-scripting half of the switch; ScriptHeapRp2040.cpp and core/script/ are the other.
#if !AWTRIX_FEATURE_SCRIPTING

#include "core/script/ScriptInfo.h"
#include "core/script/ScriptHeap.h"

namespace awtrix::script {
std::map<std::string, ScriptInfo> scriptInfo(const ScriptHost*) { return {}; }
namespace heap {
Info info() { return {"unavailable", 0, false}; }
std::size_t growthBudget() { return 0; }
}
}

#endif
