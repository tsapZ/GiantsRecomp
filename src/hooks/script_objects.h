// Reading the game's script objects (the levels' IGZ scripts, as loaded) from guest memory, for
// the hooks that pick one op or expression out of all the scripts by its variables' names
// (health_hook.cpp; Trap Team has the same file).
//
// The layout is the script file's (IGZ version 6 here, the same offsets as Trap Team's version 8;
// checked 2026-11-02 in a Level_214 snapshot): a stack (a variable path, or a value) has its
// count at +8 and its items at +0x14; a variable's or macro parameter's name is at +0x1C. A name's
// pointer has its lowest bit set in memory (the strings start at even addresses; seen 2026-10-26:
// "state" at ...24 read as ...25). Every pointer is checked to be readable before it is followed.

#pragma once

#include <cstdint>
#include <cstring>

#include <rex/memory.h>
#include <rex/memory/utils.h>

namespace giantsrecomp::script {

// The guest bytes at `address`, or null when they aren't all readable.
inline const uint8_t* Readable(uint8_t* base, uint32_t address, uint32_t size) {
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

inline bool ReadU32(uint8_t* base, uint32_t address, uint32_t& out) {
  const uint8_t* p = Readable(base, address, 4);
  if (!p) return false;
  out = rex::memory::load_and_swap<uint32_t>(p);
  return true;
}

// Whether the name pointer at `address` points to exactly `text` (with its terminator).
inline bool NameIs(uint8_t* base, uint32_t address, const char* text) {
  uint32_t string = 0;
  if (!ReadU32(base, address, string)) return false;
  const uint32_t size = uint32_t(std::strlen(text)) + 1;
  const uint8_t* p = Readable(base, string & ~1u, size);  // the tag bit off
  return p && std::memcmp(p, text, size) == 0;
}

// Item `index` of the stack the pointer at `stack_field` points to (counted from the end when
// negative: -1 is the last), or 0.
inline uint32_t StackItem(uint8_t* base, uint32_t stack_field, int index) {
  uint32_t stack = 0, count = 0, items = 0, item = 0;
  if (!ReadU32(base, stack_field, stack) || !ReadU32(base, stack + 8, count) || count == 0 || count > 16 ||
      !ReadU32(base, stack + 0x14, items)) {
    return 0;
  }
  const int64_t at = index < 0 ? int64_t(count) + index : index;
  if (at < 0 || at >= int64_t(count) || !ReadU32(base, items + 4 * uint32_t(at), item)) return 0;
  return item;
}

// The last item of the stack the pointer at `stack_field` points to: a path's variable.
inline uint32_t LastItem(uint8_t* base, uint32_t stack_field) { return StackItem(base, stack_field, -1); }

}  // namespace giantsrecomp::script
