// Gold and XP multipliers, as cheats (SkylandersCoreOfLight #48; Swap Force, SuperChargers and
// Imaginators have them under the same setting names, the rows from skylanders-portal).
//
// Giants pays out both in its scripts (read 2026-11-06 from a heap snapshot of Level_214):
//
//   "Game - Award Money" (parameter amount):       ... val = the player's money;
//                                                  amount = val + amount
//                                                  amount > 65000 -> amount = 65000; then saved
//   "Game - Award Experience" (parameter amount):  amount = exp value * amount
//                                                  amount = Difficulty XP Multiplier * amount
//                                                  then added to the Skylander
//
// Every gold pickup and XP award goes through them. At the set-value op's entry (sub_821E42D8,
// ScriptAssignEntryHook in health_hook.cpp) OnAssign tells those two expressions by their functions
// ('+' is sub_821E4908, '*' sub_821E4F20) and names (the right side the macro's own parameter
// "amount", its macro +0x38 named as above; the left side "val" / "Difficulty XP Multiplier"), and
// remembers the one about to run. The '+' and '*' store left op right on one of three paths each,
// picked by the values' types (config/default.toml), with the expression in r30 on all of them: the
// hooks there scale the right value, the award, by the multiplier when it is positive (a negative
// amount would be money spent). The game's own cap of 65000 gold stays. Nothing is written to the
// game's memory; the gold and the levels a Skylander gets are saved to its figure by the game as
// usual.

#include "multiplier_cheats_hook.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>

#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

#include "portal_rex/achievements_ui.h"
#include "script_objects.h"

namespace {

using giantsrecomp::script::LastItem;
using giantsrecomp::script::NameIs;
using rex::memory::load_and_swap;

constexpr uint32_t kExpression = 0x820DD808;  // an expression: left +8, right +0x10, function +0x14
constexpr uint32_t kAdd = 0x821E4908;
constexpr uint32_t kMultiply = 0x821E4F20;

enum class Kind : uint8_t { kNone, kGold, kXp };

uint8_t* Base() { return rex::system::kernel_state()->memory()->virtual_membase(); }

// Whether the stack at `stack_field`'s last item is the parameter "amount" of the macro `macro`.
bool IsAmountOf(uint8_t* base, uint32_t stack_field, const char* macro) {
  const uint32_t amount = LastItem(base, stack_field);
  uint32_t owner = 0;
  return amount && NameIs(base, amount + 0x1C, "amount") &&
         giantsrecomp::script::ReadU32(base, amount + 0x38, owner) && owner && NameIs(base, owner + 8, macro);
}

// The last item of the path the stack at `stack_field` holds, read directly (the game is about to
// evaluate it), or 0.
uint32_t LastItemDirect(uint8_t* base, uint32_t stack_field) {
  const uint32_t stack = load_and_swap<uint32_t>(base + stack_field);
  if (!stack) return 0;
  const uint32_t count = load_and_swap<uint32_t>(base + stack + 8);
  const uint32_t items = load_and_swap<uint32_t>(base + stack + 0x14);
  if (!count || count > 16 || !items) return 0;
  return load_and_swap<uint32_t>(base + items + 4 * (count - 1));
}

Kind Classify(uint8_t* base, uint32_t expression, uint32_t function) {
  // The cheap test first: the right side is a macro parameter (type 820DFFB0). Only those go on to
  // the names, which ask the memory's protection (system calls).
  const uint32_t right = LastItemDirect(base, expression + 0x10);
  if (!right || load_and_swap<uint32_t>(base + right) != 0x820DFFB0) return Kind::kNone;
  const uint32_t left = LastItem(base, expression + 8);
  if (!left) return Kind::kNone;
  if (function == kAdd && NameIs(base, left + 0x1C, "val") &&
      IsAmountOf(base, expression + 0x10, "Game - Award Money")) {
    return Kind::kGold;
  }
  if (function == kMultiply && NameIs(base, left + 0x1C, "Difficulty XP Multiplier") &&
      IsAmountOf(base, expression + 0x10, "Game - Award Experience")) {
    return Kind::kXp;
  }
  return Kind::kNone;
}

// The award about to be computed (OnAssign), for the arithmetic hooks.
thread_local uint32_t g_expression = 0;
thread_local Kind g_kind = Kind::kNone;

int Factor(Kind kind) {
  return kind == Kind::kGold ? skylanders::CheatGoldMultiplier()
                             : kind == Kind::kXp ? skylanders::CheatXpMultiplier() : 1;
}

// The multiplier for this arithmetic op's right value, or 0 when it isn't the remembered award.
int Take(const PPCRegister& r30, Kind want) {
  if (!g_expression || r30.u32 != g_expression || g_kind != want) return 0;
  g_expression = 0;
  const int k = Factor(want);
  return k > 1 ? k : 0;
}

void LogOnce(Kind kind, double before, double after) {
  static bool gold_logged = false, xp_logged = false;
  bool& logged = kind == Kind::kGold ? gold_logged : xp_logged;
  if (logged) return;
  logged = true;
  REXLOG_INFO("Extras: the {} multiplier scaled an award ({} -> {})", kind == Kind::kGold ? "gold" : "XP",
              before, after);
}

void ScaleInt(PPCRegister& value, Kind kind, int k) {
  const int64_t before = value.s32;
  if (before <= 0) return;
  const int64_t after = std::min<int64_t>(before * k, std::numeric_limits<int32_t>::max());
  value.s64 = after;
  LogOnce(kind, double(before), double(after));
}

void ScaleFloat(PPCRegister& value, Kind kind, int k) {
  const double before = value.f64;
  if (!(before > 0.0)) return;
  value.f64 = double(float(before * k));
  LogOnce(kind, before, value.f64);
}

}  // namespace

namespace giantsrecomp::multipliers {

void OnAssign(uint32_t expression) {
  g_expression = 0;
  if (skylanders::CheatGoldMultiplier() <= 1 && skylanders::CheatXpMultiplier() <= 1) return;
  uint8_t* base = Base();
  // The expression the game is about to evaluate: read directly. Only '+' and '*' go on, and their
  // answers are kept per expression, with both sides' pointers to tell another level's expression at
  // the same address. Thousands of them run a frame, so the answers sit in a direct-mapped table
  // (a hash map's lookups cost the hub about 1 % with the multipliers on, 2026-11-06); a slot two
  // expressions share is only worked out again, and the cheap test in Classify keeps that cheap.
  if (!expression || load_and_swap<uint32_t>(base + expression) != kExpression) return;
  const uint32_t function = load_and_swap<uint32_t>(base + expression + 0x14);
  if (function != kAdd && function != kMultiply) return;
  struct Slot {
    uint32_t expression;
    Kind kind;
    uint64_t sides;
  };
  constexpr uint32_t kSlots = 8192;  // the index below is the hash's top 13 bits
  // Made on first use: a thread_local array would be reserved in every thread of the process.
  thread_local std::unique_ptr<Slot[]> table;
  if (!table) table.reset(new Slot[kSlots]());
  const uint64_t sides = (uint64_t(load_and_swap<uint32_t>(base + expression + 8)) << 32) |
                         load_and_swap<uint32_t>(base + expression + 0x10);
  Slot& slot = table[(expression * 2654435761u) >> 19];
  if (slot.expression != expression || slot.sides != sides) {
    slot = {expression, Classify(base, expression, function), sides};
  }
  if (slot.kind == Kind::kNone) return;
  g_expression = expression;
  g_kind = slot.kind;
}

}  // namespace giantsrecomp::multipliers

// '+' (sub_821E4908): add r8,r10,r11, two ints, r11 the right.
void ScriptAddIntsHook(PPCRegister& r30, PPCRegister& r11) {
  if (const int k = Take(r30, Kind::kGold)) ScaleInt(r11, Kind::kGold, k);
}

// fadds f12,f0,f13: the right a float in f13.
void ScriptAddFloatHook(PPCRegister& r30, PPCRegister& f13) {
  if (const int k = Take(r30, Kind::kGold)) ScaleFloat(f13, Kind::kGold, k);
}

// fadds f10,f11,f0: a float plus an int, the int already made a float in f11.
void ScriptAddIntRightHook(PPCRegister& r30, PPCRegister& f11) {
  if (const int k = Take(r30, Kind::kGold)) ScaleFloat(f11, Kind::kGold, k);
}

// '*' (sub_821E4F20): mullw r8,r11,r9, two ints, r9 the right.
void ScriptMultiplyIntsHook(PPCRegister& r30, PPCRegister& r9) {
  if (const int k = Take(r30, Kind::kXp)) ScaleInt(r9, Kind::kXp, k);
}

// fmuls f12,f0,f13: the right a float in f13.
void ScriptMultiplyFloatHook(PPCRegister& r30, PPCRegister& f13) {
  if (const int k = Take(r30, Kind::kXp)) ScaleFloat(f13, Kind::kXp, k);
}

// fmuls f10,f11,f0: a float times an int, the int already made a float in f11.
void ScriptMultiplyIntRightHook(PPCRegister& r30, PPCRegister& f11) {
  if (const int k = Take(r30, Kind::kXp)) ScaleFloat(f11, Kind::kXp, k);
}
