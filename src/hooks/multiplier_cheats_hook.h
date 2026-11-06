// Gold and XP multipliers (multiplier_cheats_hook.cpp), told about each set-value op by the scripts'
// assignment hook (health_hook.cpp, ScriptAssignEntryHook).

#pragma once

#include <cstdint>

namespace giantsrecomp::multipliers {

// sub_821E42D8's entry: `expression` is what the set-value op is about to evaluate.
void OnAssign(uint32_t expression);

}  // namespace giantsrecomp::multipliers
