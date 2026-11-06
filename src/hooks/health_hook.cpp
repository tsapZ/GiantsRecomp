// No health loss, as a cheat (as Spyro's Adventure's and Trap Team's, under the same setting name).
//
// Giants counts health in its scripts. The macro "Combat - Hurt Creature" takes a hit off every
// creature, enemies included, with
//
//   creature info temp.hp = creature info temp.hp - damage
//
// The set-value op's execute (sub_821E1168, vtable 820E1178 slot 27) resolves its target path to the
// variable (sub_821DFCE8, r5 = 1) and hands it with the expression to sub_821E42D8 (r3 = the
// expression, r4 = the target variable): for this expression the hurt creature's own hp variable,
// the same object for all of a creature's hits (runs\health\giants_v5_long: the player's 16 hits all
// on one, 467 down to 139; each enemy's on its own). The "-" is sub_821E4C18, which stores left minus
// right on one of three paths by the values' types (config/default.toml); r30 holds the expression on
// all three.
//
// Telling the players' creatures: when a Skylander joins, the player's data sets a reference to its
// creature's hp, `[joined scratch.hp] -> [joined scratch.creature info.hp]` (op B81D20E0 in a
// Level_214 snapshot). The set-reference op's execute (sub_821D8408, vtable 820DF988 slot 27)
// resolves the source (r28) and calls the target variable's setter at 0x821D84B4 with r3 = where the
// reference is kept. That source is the player's hp variable (the same object as the hurt target in
// giants_v5_long); enemies never ran it. The hook remembers (where, which) pairs; a hit counts as a
// player's while a remembered reference still points at its target, so a variable a level change
// freed and an enemy got doesn't count. With cheat_no_health_loss on, the right value of such a
// hit's subtraction becomes 0: the hit lands (knockback, sound), the health stays; enemies lose
// health as before. Nothing is written to the game's memory.

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

#include "multiplier_cheats_hook.h"
#include "script_objects.h"

REXCVAR_DEFINE_BOOL(cheat_no_health_loss, false, "Extras",
                    "Skylanders don't lose health when they get hit (achievements don't unlock while it's on)");

namespace {

using giantsrecomp::script::LastItem;
using giantsrecomp::script::NameIs;
using giantsrecomp::script::StackItem;

uint8_t* Base() { return rex::system::kernel_state()->memory()->virtual_membase(); }
uint32_t Word(uint32_t a) { return a ? rex::memory::load_and_swap<uint32_t>(Base() + a) : 0; }

// Whether the expression at `rhs` is "creature info temp.hp - damage".
bool IsPlayerDamage(uint8_t* base, uint32_t rhs) {
  const uint32_t left_first = StackItem(base, rhs + 8, 0);
  const uint32_t left_last = LastItem(base, rhs + 8);
  const uint32_t right_last = LastItem(base, rhs + 0x10);
  return left_first && left_last && right_last && NameIs(base, right_last + 0x1C, "damage") &&
         NameIs(base, left_last + 0x1C, "hp") && NameIs(base, left_first + 0x1C, "creature info temp");
}

// Every set-value op passes sub_821E42D8, so the answer is kept per expression, with its two sides'
// pointers to tell another level's expression at the same address from it (the names cost memory
// protection queries, system calls; Trap Team's #16).
bool IsPlayerDamageCached(uint32_t rhs) {
  struct Seen {
    uint64_t sides;
    bool damage;
  };
  thread_local std::unordered_map<uint32_t, Seen> seen;
  const uint64_t sides = (uint64_t(Word(rhs + 8)) << 32) | Word(rhs + 0x10);
  if (auto it = seen.find(rhs); it != seen.end() && it->second.sides == sides) return it->second.damage;
  if (seen.size() > 65536) seen.clear();
  const bool damage = IsPlayerDamage(Base(), rhs);
  seen[rhs] = {sides, damage};
  return damage;
}

// A variable's name compared directly: the op and its paths are the objects the game is running
// right now, so they're read without the protection checks (those are system calls).
bool NameIsDirect(uint32_t variable, std::string_view text) {
  const uint32_t name = Word(variable + 0x1C) & ~1u;
  if (!name) return false;
  for (size_t i = 0; i <= text.size(); ++i) {
    const char c = char(rex::memory::load<uint8_t>(Base() + name + uint32_t(i)));
    if (c != (i < text.size() ? text[i] : '\0')) return false;
  }
  return true;
}

// Whether a set-reference op's target path is "joined scratch. ... .hp". Set-reference ops run all
// the time (a first try that read the names each time ran the game at 1 FPS), so the answer is kept
// per op, with its target path's pointer to tell another level's op at the same address from it.
bool IsJoinedHpReference(uint32_t op) {
  struct Seen {
    uint32_t path;
    bool joined;
  };
  thread_local std::unordered_map<uint32_t, Seen> seen;
  const uint32_t path = Word(op + 0x1C);
  if (auto it = seen.find(op); it != seen.end() && it->second.path == path) return it->second.joined;
  bool joined = false;
  const uint32_t count = Word(path + 8);
  if (path && count >= 2 && count <= 16) {
    const uint32_t items = Word(path + 0x14);
    const uint32_t first = Word(items), last = Word(items + 4 * (count - 1));
    joined = items && first && last && NameIsDirect(last, "hp") && NameIsDirect(first, "joined scratch");
  }
  if (seen.size() > 65536) seen.clear();
  seen[op] = {path, joined};
  return joined;
}

struct PlayerHp {
  uint32_t reference = 0;  // where the player's data keeps its reference to the hp variable
  uint32_t hp = 0;         // the hp variable it was set to
};
std::array<PlayerHp, 8> g_players;  // the latest joins, oldest overwritten first
size_t g_next_player = 0;
std::mutex g_players_mutex;

// Whether `hp` is a player's hp variable right now: a remembered reference still points at it
// (the reference variable, vtable 820DF750, keeps its target at +0x1C).
bool IsPlayerHp(uint32_t hp) {
  std::lock_guard<std::mutex> lock(g_players_mutex);
  for (const PlayerHp& p : g_players) {
    if (p.hp != hp || !p.reference) continue;
    // The reference variable keeps what it points at in its +0x1C (seen in giants_v6_ref).
    if (Word(p.reference + 0x1C) == hp) return true;
  }
  return false;
}

thread_local uint32_t g_skip_expression = 0;  // Hurt Creature's expression whose hit is a player's

bool SkipSubtraction(const PPCRegister& r30, const char* path) {
  if (!g_skip_expression || r30.u32 != g_skip_expression) return false;
  g_skip_expression = 0;
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("Extras: no health loss kept a hit's damage off ({})", path);
  }
  return true;
}

}  // namespace

// 0x821D84B4 in sub_821D8408 (set *reference to), just before the target variable's setter is
// called: r3 = where the reference is kept, r28 = the resolved source, r31 = the op.
void ScriptSetReferenceHook(PPCRegister& r3, PPCRegister& r28, PPCRegister& r31) {
  if (!r28.u32 || !IsJoinedHpReference(r31.u32)) return;
  {
    std::lock_guard<std::mutex> lock(g_players_mutex);
    for (const PlayerHp& p : g_players) {
      if (p.reference == r3.u32 && p.hp == r28.u32) return;
    }
    g_players[g_next_player] = {r3.u32, r28.u32};
    g_next_player = (g_next_player + 1) % g_players.size();
  }
  REXLOG_INFO("Extras: no health loss knows a player's creature (hp variable {:08X})", r28.u32);
}

// sub_821E42D8's entry: r3 = the expression, r4 = its target variable. Does nothing while the cheat
// is off; the players are learned all the same (ScriptSetReferenceHook), so turning it on mid-game
// works at once. The gold and XP multipliers (multiplier_cheats_hook.cpp) look at the same entry.
void ScriptAssignEntryHook(PPCRegister& r3, PPCRegister& r4) {
  giantsrecomp::multipliers::OnAssign(r3.u32);
  g_skip_expression = 0;
  if (!REXCVAR_GET(cheat_no_health_loss) || !IsPlayerDamageCached(r3.u32)) return;
  if (IsPlayerHp(r4.u32)) g_skip_expression = r3.u32;
}

// subf r8,r9,r11: two ints, r11 - r9.
void ScriptSubtractIntsHook(PPCRegister& r30, PPCRegister& r9) {
  if (SkipSubtraction(r30, "ints")) r9.u64 = 0;
}

// fsubs f12,f0,f13: both values floats.
void ScriptSubtractFloatHook(PPCRegister& r30, PPCRegister& f13) {
  if (SkipSubtraction(r30, "floats")) f13.f64 = 0.0;
}

// fsubs f10,f0,f11: a float minus an int, the int already made a float in f11.
void ScriptSubtractIntRightHook(PPCRegister& r30, PPCRegister& f11) {
  if (SkipSubtraction(r30, "float minus int")) f11.f64 = 0.0;
}
