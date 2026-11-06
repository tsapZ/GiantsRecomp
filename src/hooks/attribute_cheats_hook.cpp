// Always crit and elemental power everywhere, as cheats (SkylandersCoreOfLight #48; Swap Force,
// Trap Team, SuperChargers and Imaginators have them under the same setting names).
//
// Giants rolls both in its scripts, in the macro "Combat - Hurt Creature" that every hit goes through,
// built as Trap Team's "On Hurt - Player On Hurt" (read 2026-11-06 from a heap snapshot of Level_214,
// debug notes in the kit). The check value op's execute is sub_821E2438 (vtable 820E1448 slot 27): it
// resolves the left path (+0x1C), compares it with the right side (+0x24) by the operator (+0x20) in
// sub_821E4410 and, when the result in r3 is below 1, skips the op's next +0x1A ops. The compare
// knows the same six operators as Trap Team's: 0 <=, 1 ==, 2 >=, 3 <, 4 >, 5 !=. AttributeCheckHook
// sits at 0x821E2490, right after the compare (config/default.toml): r3 is the result, r31 the op.
//
// Crit. The macro's parameter "critical hit" comes in as -1. Only when the hurter is a player's
// creature (a reference check and the minions set before it; the enemies' hits jump past), the macro
// checks `critical hit < 0` and sets it to the hurter's minion description.total crit. Then for
// every hit:
//
//   check critical hit <= 0                   -> no crit (the enemies' hits all end here, with -1)
//   critical hit = critical hit + 20 / - 100  (the roll)
//   check critical hit > 0                    -> damage = damage + damage * 0.5, critical hit = 1
//
// With the cheat on, the `< 0` check marks the hit as a player's, and for that hit the `<= 0` check
// comes out false and the first `> 0` one true: every player hit is critical, the enemies' hits are
// untouched. A later `> 0` check of the same hit reads the outcome (critical hit is 1 then).
//
// Element. Unless Versus_Mode is on, the macro sets the global Scratch G3 to 1, and back to 0 unless
// the hurter is player 1's creature and the global ZoneElement_P1_Correct is 1 (player 2's: P2), i.e.
// it stands in an elemental zone of its own element; with Scratch G3 > 0 it adds the hurter's total
// elemental strength to the damage. Those two `== 1` checks are the only checks of the two variables
// in the scripts loaded there (debug tool giants_checks.py over the snapshot); with the cheat on they
// come out true, as if every area were the Skylander's zone.
//
// The layout is the script file's (IGZ version 6): a value stack's count +8, items +0x14 and literal
// +0x20 (Trap Team's +0x24); a macro parameter (type 820DFFB0) has its name at +0x1C, a global
// variable instance (type 820DF500) at +8. Each op is classified once by its variables' names and the
// answer kept by op address. Nothing is written to the game's memory.

#include <cstdint>
#include <memory>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

#include "script_objects.h"

REXCVAR_DEFINE_BOOL(cheat_always_crit, false, "Extras",
                    "Every hit of your Skylanders is a critical hit");
REXCVAR_DEFINE_BOOL(cheat_elemental_power, false, "Extras",
                    "Your Skylanders get their element's bonus in every area, as if each area were "
                    "theirs");

namespace {

using giantsrecomp::script::NameIs;
using rex::memory::load_and_swap;

enum class Kind : uint8_t {
  kNone,
  kCritUnset,    // critical hit < 0: reached only for a player's hit
  kCritNone,     // critical hit <= 0: no crit
  kCritRoll,     // critical hit > 0 after the roll: a crit
  kZoneCorrect,  // ZoneElement_P1_Correct / ZoneElement_P2_Correct == 1
};

// The check's right side is the literal `value` (a ValueRHSVariant whose value stack has no items).
bool RightIsLiteral(uint8_t* base, uint32_t op, uint32_t value) {
  const uint32_t rhs = load_and_swap<uint32_t>(base + op + 0x24);
  const uint32_t stack = rhs ? load_and_swap<uint32_t>(base + rhs + 8) : 0;
  return stack && load_and_swap<uint32_t>(base + stack + 8) == 0 &&
         load_and_swap<uint32_t>(base + stack + 0x20) == value;
}

Kind Classify(uint8_t* base, uint32_t op) {
  // Every check op of every script comes here once while a cheat is on, so the cheap tests come
  // first and read only what the game has just used (the op, its right side's literal, its left path
  // and the variable it resolved): most checks end there. Only the few left get the name compare,
  // which asks the memory's protection (system calls; in Trap Team a dozen per op made a 70 ms hitch
  // when a new area's scripts first ran).
  const uint32_t oper = load_and_swap<uint32_t>(base + op + 0x20);
  uint32_t literal;
  if (oper == 1) {
    literal = 1;
  } else if (oper == 0 || oper == 3 || oper == 4) {
    literal = 0;
  } else {
    return Kind::kNone;
  }
  if (!RightIsLiteral(base, op, literal)) return Kind::kNone;
  const uint32_t path = load_and_swap<uint32_t>(base + op + 0x1C);
  const uint32_t count = path ? load_and_swap<uint32_t>(base + path + 8) : 0;
  const uint32_t items = path ? load_and_swap<uint32_t>(base + path + 0x14) : 0;
  if (!count || count > 16 || !items) return Kind::kNone;
  const uint32_t last = load_and_swap<uint32_t>(base + items + 4 * (count - 1));
  if (!last) return Kind::kNone;
  const uint32_t type = load_and_swap<uint32_t>(base + last);
  if (oper == 1) {
    // A global variable instance (its name at +8).
    if (count != 1 || type != 0x820DF500) return Kind::kNone;
    if (NameIs(base, last + 8, "ZoneElement_P1_Correct") ||
        NameIs(base, last + 8, "ZoneElement_P2_Correct")) {
      return Kind::kZoneCorrect;
    }
    return Kind::kNone;
  }
  // The macro parameter alone (a path of one item).
  if (count != 1 || type != 0x820DFFB0 || !NameIs(base, last + 0x1C, "critical hit")) {
    return Kind::kNone;
  }
  return oper == 3 ? Kind::kCritUnset : oper == 0 ? Kind::kCritNone : Kind::kCritRoll;
}

// The hit being checked is a player's (the crit checks of one hit run in order on the scripts' thread).
thread_local bool g_player_hit = false;

void Apply(Kind kind, PPCRegister& r3) {
  switch (kind) {
    case Kind::kCritUnset:
      // Reached only for a player's hit; the result itself stays.
      g_player_hit = true;
      return;
    case Kind::kCritNone:
      if (g_player_hit && REXCVAR_GET(cheat_always_crit)) r3.u64 = 0;
      return;
    case Kind::kCritRoll:
      if (g_player_hit && REXCVAR_GET(cheat_always_crit)) {
        static bool logged = false;
        if (!logged) {
          logged = true;
          REXLOG_INFO("Extras: always crit made a hit critical (roll {})", r3.s32 >= 1 ? "won" : "lost");
        }
        r3.u64 = 1;
      }
      // The later `> 0` checks of the same hit read the crit's outcome: no forcing there.
      g_player_hit = false;
      return;
    case Kind::kZoneCorrect:
      if (REXCVAR_GET(cheat_elemental_power)) {
        static bool logged = false;
        if (!logged) {
          logged = true;
          REXLOG_INFO("Extras: elemental power everywhere ({} zone)", r3.s32 >= 1 ? "own" : "other");
        }
        r3.u64 = 1;
      }
      return;
    case Kind::kNone:
      return;
  }
}

}  // namespace

void AttributeCheckHook(PPCRegister& r3, PPCRegister& r31) {
  if (!REXCVAR_GET(cheat_always_crit) && !REXCVAR_GET(cheat_elemental_power)) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  // Each op's answer is kept by its address, with its left and right side's pointers to tell a new
  // script's op at the same address (another level) from it. Every check op of every script passes
  // here, so the answers sit in a direct-mapped table (a hash map's lookups cost the hub about 1 % in
  // the multipliers' hook, multiplier_cheats_hook.cpp, 2026-11-06); a slot two ops share is only
  // worked out again, and Classify's cheap tests keep that cheap.
  struct Slot {
    uint32_t op;
    Kind kind;
    uint64_t sides;
  };
  constexpr uint32_t kSlots = 16384;  // the index below is the hash's top 14 bits
  // Made on first use: a thread_local array would be reserved in every thread of the process.
  thread_local std::unique_ptr<Slot[]> table;
  if (!table) table.reset(new Slot[kSlots]());
  const uint32_t op = r31.u32;
  const uint64_t sides =
      (uint64_t(load_and_swap<uint32_t>(base + op + 0x1C)) << 32) | load_and_swap<uint32_t>(base + op + 0x24);
  Slot& slot = table[(op * 2654435761u) >> 18];
  if (slot.op != op || slot.sides != sides) slot = {op, Classify(base, op), sides};
  if (slot.kind != Kind::kNone) Apply(slot.kind, r3);
}
