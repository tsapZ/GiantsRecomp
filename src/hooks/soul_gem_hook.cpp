// A Soul Gem picked up, from Giants' own scripts (Core of Light #71, the Soul Gem run). For now only a log line, to be
// checked against a real pickup before the run uses it.
//
// Why the scripts: the movie the lib watches (soul_gems.h, movies\<Name>.mov) opens only when the player says Yes to
// the preview, and Giants asks. The gem object's script (C:/tfb/Content/Levels/Includes/Scripts/ChampionUpgradeToken.ai,
// included by every level) sets the gem collected with "Save - Collection Get/Set - Champion Token" (champion name =
// "upgrade minion's name") and right after it uses "System - Achievements - Check Soul Gem Achievement", whatever the
// answer. That macro has no parameters and no other use in the scripts (found 2026-11-17 and 2026-11-21 in the Level_214
// snapshots with the dev kit's giants_macro_callers.py / giants_macro_args.py). So its use is the pickup.
//
// A use-macro op runs in its execute, sub_821C4490 (the op type's vtable 820E0028, slot 27; the dev kit's probe 8 of
// 2026-11-18 hooked it): r3 the op, its macro definition at +0x3C, a definition's name at +8 (a name's pointer may carry
// a tag bit). Every macro use of every script passes here, so the definition and its name pointer are read directly
// (the op is the one being run) and compared with the gem macro's once found; any other definition is looked at once
// (its name compared straight) and remembered in a table. Reads only.

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "dev/where.h"
#include "hooks/script_objects.h"

namespace {

constexpr char kGemMacro[] = "System - Achievements - Check Soul Gem Achievement";

// The gem macro's definitions once seen (the snapshots had two: global and per level), and the others looked at, each
// by the definition's address and its name pointer together: a level's definitions go at the next load and their
// memory is used again, so by the address alone another macro's definition where the gem macro's was before counted
// as a pickup at every use, and the gem macro's where another one's was before was never looked at (review
// 2026-11-24). A slot written over by another one only means one more name read.
std::array<std::atomic<uint64_t>, 8> g_gem{};
std::array<std::atomic<uint64_t>, 8192> g_other{};
std::atomic<int> g_told{0};  // pickup lines this session

// The definition's name compared straight, without script_objects.h's checked reads: those ask the system for the
// page protection twice per read, and this runs for every macro use of every script. With the cache below missing
// (more definitions than slots: they are per object), that cost Trap Team 0.6.0-beta.5 up to 40 % of its frame rate
// (CoL #71, a player's report 2026-11-21; dev kit A/B 176 -> 110 FPS in the Academy, 145 -> 85 in Level_313). The
// op is the one the game is running, so its definition and the definition's name are live; a name pointer outside
// the guest's heap and image is taken as no name. Byte by byte up to the first difference, so a short name at the
// end of the memory it sits in is never read past its terminator; the first byte decides most of them.
bool NameIsGemMacro(const uint8_t* base, uint32_t name) {
  name &= ~1u;  // the tag bit off
  if (name < 0x40000000u || name > 0xBFFFFF00u) return false;
  const uint8_t* text = base + name;
  size_t k = 0;
  while (kGemMacro[k] != 0 && text[k] == uint8_t(kGemMacro[k])) ++k;
  return kGemMacro[k] == 0 && text[k] == 0;
}

bool IsGemMacro(uint8_t* base, uint32_t def) {
  const uint32_t name = rex::memory::load_and_swap<uint32_t>(base + def + 8);
  const uint64_t key = (uint64_t(def) << 32) | name;
  for (const auto& g : g_gem) {
    if (g.load(std::memory_order_relaxed) == key) return true;
  }
  std::atomic<uint64_t>& other = g_other[(def >> 2) % g_other.size()];
  if (other.load(std::memory_order_relaxed) == key) return false;
  if (!NameIsGemMacro(base, name)) {
    other.store(key, std::memory_order_relaxed);
    return false;
  }
  for (auto& g : g_gem) {
    uint64_t empty = 0;
    if (g.compare_exchange_strong(empty, key)) break;
  }
  return true;
}

void MacroUsed(uint8_t* base, uint32_t op) {
  if (op < 0x10000 || op > 0xFFFFFF00u) return;
  const uint32_t def = rex::memory::load_and_swap<uint32_t>(base + op + 0x3C);
  if (def < 0x40000000u || def > 0xBFFFFF00u) return;  // (the name is read at +8)
#if defined(SKYLANDERS_DEVKIT)
  // The developer kit says once that the hook runs (no pickup needed to see it work).
  static std::atomic<bool> told{false};
  if (!told.load(std::memory_order_relaxed) && !told.exchange(true)) {  // (no atomic write per call)
    uint32_t name = 0;
    const uint8_t* text = giantsrecomp::script::ReadU32(base, def + 8, name)
                              ? giantsrecomp::script::Readable(base, name & ~1u, 1)
                              : nullptr;
    REXLOG_INFO("Kit: use-macro hook live, first macro {}",
                text ? std::string(reinterpret_cast<const char*>(text), strnlen(reinterpret_cast<const char*>(text), 96))
                     : std::string("?"));
  }
#endif
  if (def < 0x10000 || !IsGemMacro(base, def)) return;
  // At most 60 lines per session (Giants has 16 gems): a wrong match can't fill the log.
  if (g_told.fetch_add(1, std::memory_order_relaxed) >= 60) return;
  REXLOG_INFO("Soul Gem (script): picked up in {} (Check Soul Gem Achievement)", skylanders::dev::CurrentLevel());
  skylanders::dev::LogPlace("Soul Gem picked up");
}

}  // namespace

extern "C" void __imp__sub_821C4490(PPCContext& __restrict ctx, uint8_t* base);

REX_HOOK_RAW(sub_821C4490) {
  MacroUsed(base, ctx.r3.u32);
  __imp__sub_821C4490(ctx, base);
}
