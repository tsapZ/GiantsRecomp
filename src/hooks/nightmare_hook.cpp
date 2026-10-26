// Nightmare from the start, as an option (SwapForceRecomp #74; Swap Force, Trap Team and
// Imaginators have it as nightmare_from_start too).
//
// Giants unlocks its hardest difficulty in a script, not in code: the front end's new-game path
// (Title.bld's level.bld) reads system flag 3 (Story Mode finished) through the macro "Save -
// Get/Set System Flag" and calls the difficulty menu with it:
//
//   use "Save - Get/Set System Flag" (setting ID 3, value -1, do save 1)
//   use "Difficulty UI - Show Difficulty Menu"
//     allow nightmare = [value]        <- this parameter
//     force default option = 1
//
// The menu shows Nightmare only when "allow nightmare" isn't 0. The same menu is also called from
// the game itself with "allow nightmare" = 0, so the menu's own check is left alone; instead the
// new-game call's parameter gets the value 1. A macro parameter is bound in sub_821C8930, and on
// the path for a value (a ValueRHSVariant) r3 is the parameter's value (its +0x30) at 0x821C8990
// (config/default.toml), just before it is evaluated. With nightmare_from_start on, the hook swaps
// in the next parameter's value, the literal 1 of "force default option" (the call's next object,
// +0x90), when r31 is that call's "allow nightmare": the parameter's name, its value the "value" of
// "Save - Get/Set System Flag", and the next one "force default option" = 1. The object layout is
// the script file's (IGZ version 6; checked 2026-10-26 in Title.bld): a parameter's name +0x1C and
// value +0x30; a ValueRHSVariant's value stack +8; a stack's count +8, its items +0x14, a literal
// +0x20; a macro parameter's macro +0x38, a macro's name +8. The parameter's own name is read
// directly (the game has just used it); every pointer past it is checked to be readable first. Nothing is written: the save keeps its own flag, so with the
// setting off again Nightmare is locked until the game unlocks it.

#include <cstdint>
#include <cstring>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_BOOL(nightmare_from_start, false, "Game",
                    "Nightmare difficulty can be picked from the start, without finishing Story Mode "
                    "first (the save is not changed)");

namespace {

// The guest bytes at `address`, or null when they aren't all readable.
const uint8_t* Readable(uint8_t* base, uint32_t address, uint32_t size) {
  if (address < 0x10000 || address > 0xFFFFFFFFu - size) return nullptr;
  for (const uint32_t at : {address, address + size - 1}) {
    size_t length = 1;
    rex::memory::PageAccess access = rex::memory::PageAccess::kNoAccess;
    if (!rex::memory::QueryProtect(base + at, length, access) || access == rex::memory::PageAccess::kNoAccess) {
      return nullptr;
    }
  }
  return base + address;
}

bool ReadU32(uint8_t* base, uint32_t address, uint32_t& out) {
  const uint8_t* p = Readable(base, address, 4);
  if (!p) return false;
  out = rex::memory::load_and_swap<uint32_t>(p);
  return true;
}

// Whether the name pointer at `address` points to exactly `text` (with its terminator).
bool NameIs(uint8_t* base, uint32_t address, const char* text) {
  uint32_t string = 0;
  if (!ReadU32(base, address, string)) return false;
  const uint32_t size = uint32_t(std::strlen(text)) + 1;
  const uint8_t* p = Readable(base, string & ~1u, size);  // a tag bit off (Trap Team sets one)
  return p && std::memcmp(p, text, size) == 0;
}

// A ValueRHSVariant's value stack: its item count, and its last item (0 when it has none).
bool VariantStack(uint8_t* base, uint32_t variant, uint32_t& count, uint32_t& last, uint32_t& stack) {
  uint32_t items = 0;
  last = 0;
  if (!ReadU32(base, variant + 8, stack) || !ReadU32(base, stack + 8, count) || count > 16) return false;
  return count == 0 || (ReadU32(base, stack + 0x14, items) && ReadU32(base, items + 4 * (count - 1), last));
}

// The literal 1 to bind "allow nightmare" to (the next parameter's value), or 0.
uint32_t NightmareValue(uint8_t* base, uint32_t param, uint32_t value) {
  // The parameter being bound and its name the game has just used: read directly (this runs for
  // every parameter of every macro call while the setting is on).
  static constexpr char kName[] = "allow nightmare";
  const uint32_t name = rex::memory::load_and_swap<uint32_t>(base + param + 0x1C) & ~1u;
  if (!name || std::strncmp(reinterpret_cast<const char*>(base + name), kName, sizeof(kName)) != 0) return 0;
  uint32_t count = 0, item = 0, stack = 0, macro = 0;
  if (!VariantStack(base, value, count, item, stack) || count != 1 || !NameIs(base, item + 0x1C, "value") ||
      !ReadU32(base, item + 0x38, macro) || !NameIs(base, macro + 8, "Save - Get/Set System Flag")) {
    return 0;
  }
  const uint32_t next = param + 0x90;
  uint32_t one = 0, literal = 0;
  if (!NameIs(base, next + 0x1C, "force default option") || !ReadU32(base, next + 0x30, one) ||
      !VariantStack(base, one, count, item, stack) || count != 0 || !ReadU32(base, stack + 0x20, literal) ||
      literal != 1) {
    return 0;
  }
  return one;
}

}  // namespace

void NightmareParamHook(PPCRegister& r3, PPCRegister& r31) {
  if (!REXCVAR_GET(nightmare_from_start)) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  const uint32_t one = NightmareValue(base, r31.u32, r3.u32);
  if (!one) return;
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("Nightmare difficulty open from the start (nightmare_from_start)");
  }
  r3.u64 = one;
}
