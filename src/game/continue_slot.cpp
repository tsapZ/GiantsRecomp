// Weiterspielen into the save played last (Core of Light #16, skylanders-portal continue_game).
//
// The library's continue presses A for player 1 until the game asks for a Skylander: the title,
// the autosave notice, "Spiel wählen" (on Story) and then "Speicherplatz wählen", a list of four
// slots that always starts on slot 1 (checked 2026-10-23 with the owner's save copy: the save in
// slot 4, slots 1-3 empty, the cursor on slot 1). So with a save played last in slot 2-4 a step
// before the presses' A moves the cursor down to it and lets the A go once it is there.
//
// The slot played last: the Save_Slot_<n>.xsg written last (<user data>\<profile>\415608DA\
// 00000001\Save_Slot_<n>\). The game makes all four files at its first start, the empty ones
// byte for byte the same, and writes a slot's file when a game in it starts or saves.
//
// Where the cursor is, from the menu script's variables (ScriptReference objects, vtable
// 0x820DF750, +8 the name, +0x1C the value, as in portal_prompt_watch.cpp): "current menu" holds
// the page shown (a "page info base" object), and "selection cur" the highlighted entry (an "item
// info base"). The page's member "items" (a script list, vtable 0x820DF2A8; its list at +0x18,
// with the count at +0x08 and the entries' array at +0x14) has the entries in their order: on the
// slot page four, the slots 1-4, one of which "selection cur" always is; "Spiel wählen" has three.
// Found 2026-10-23 by comparing the script heap (input script `save`, A0000000 + 512 MB) on
// "Spiel wählen", on the slot page and after one Down.
//
// It only reads guest memory and taps the d-pad as player 1; a real button press ends the
// continue presses and with them this step (the library's RealButtonPresses).

#include "continue_slot.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <rex/cvar.h>
#include <rex/input/input.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/system/xmemory.h>
#include <rex/types.h>

#include "portal_rex/portal_menu.h"
#include "portal_rex/portal_menu_input.h"

REXCVAR_DECLARE(bool, continue_game);  // skylanders-portal portal_menu.cpp

namespace giantsrecomp {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

constexpr const char* kTitleId = "415608DA";
constexpr int kSlots = 4;
constexpr uint32_t kScriptReferenceVtable = 0x820DF750;
constexpr uint32_t kScriptListVtable = 0x820DF2A8;
constexpr uint32_t kNameOffset = 0x08;
constexpr uint32_t kValueOffset = 0x1C;
constexpr uint32_t kListOffset = 0x18;        // in a script list variable: its list
constexpr uint32_t kListCountOffset = 0x08;   // in the list
constexpr uint32_t kListArrayOffset = 0x14;   // in the list: the entries
constexpr uint32_t kPageMembersSize = 0x100;  // where the page's member variables are looked for
constexpr std::string_view kSelectionVariable = "selection cur";
constexpr std::string_view kMenuVariable = "current menu";
constexpr std::string_view kItemsMember = "items";
// The script heap: the physical memory view (the other two alias it); 64 KB pages.
constexpr uint32_t kScanStart = 0xA0000000;
constexpr uint32_t kScanEnd = 0xC0000000;
constexpr uint32_t kScanStep = 0x10000;

// How many bytes from `address` on are committed, readable guest memory (0 if none).
uint32_t ReadableBytes(rex::memory::Memory* memory, uint32_t address) {
  rex::memory::BaseHeap* heap = memory->LookupHeap(address);
  rex::memory::HeapAllocationInfo info{};
  if (!heap || !heap->QueryRegionInfo(address, &info) ||
      !(info.state & rex::memory::kMemoryAllocationCommit) ||
      !(info.protect & rex::memory::kMemoryProtectRead)) {
    return 0;
  }
  const uint64_t end = uint64_t(address & ~(heap->page_size() - 1)) + info.region_size;
  return end > address ? uint32_t(std::min<uint64_t>(end - address, UINT32_MAX)) : 0;
}

// A guest word, 0 where nothing readable is.
uint32_t ReadU32(rex::memory::Memory* memory, uint32_t address) {
  if (ReadableBytes(memory, address) < 4) return 0;
  return rex::memory::load_and_swap<uint32_t>(memory->TranslateVirtual<const uint8_t*>(address));
}

// The name of a script object (+8, a tagged pointer to a pooled string); empty if none.
std::string ObjectName(rex::memory::Memory* memory, uint32_t object) {
  constexpr uint32_t kMaxName = 128;
  const uint32_t address = ReadU32(memory, object + kNameOffset) & ~1u;
  if (!object || !address) return {};
  const uint32_t readable = std::min(ReadableBytes(memory, address), kMaxName);
  if (!readable) return {};
  const char* text = memory->TranslateVirtual<const char*>(address);
  const char* end = static_cast<const char*>(std::memchr(text, 0, readable));
  return end ? std::string(text, end) : std::string();
}

bool IsVariable(rex::memory::Memory* memory, uint32_t object, std::string_view name) {
  return object && ReadU32(memory, object) == kScriptReferenceVtable && ObjectName(memory, object) == name;
}

// The first variable of each name in the script heap (0 where there is none).
std::vector<uint32_t> FindVariables(rex::memory::Memory* memory, const std::vector<std::string_view>& names) {
  std::vector<uint32_t> found(names.size(), 0);
  size_t missing = names.size();
  const uint32_t needle = rex::byte_swap(kScriptReferenceVtable);  // as it is in guest memory
  uint64_t address = kScanStart;
  while (address < kScanEnd && missing) {
    rex::memory::BaseHeap* heap = memory->LookupHeap(uint32_t(address));
    rex::memory::HeapAllocationInfo info{};
    if (!heap || !heap->QueryRegionInfo(uint32_t(address), &info) || !info.region_size) {
      address += kScanStep;
      continue;
    }
    const uint64_t region_end = address + info.region_size;
    if ((info.state & rex::memory::kMemoryAllocationCommit) &&
        (info.protect & rex::memory::kMemoryProtectRead) && info.region_size >= kValueOffset + 4) {
      const uint8_t* host = memory->TranslateVirtual<const uint8_t*>(uint32_t(address));
      const uint64_t words = (info.region_size - kValueOffset - 4) / 4 + 1;
      for (uint64_t i = 0; i < words && missing; ++i) {
        uint32_t word;
        std::memcpy(&word, host + 4 * i, 4);
        if (word != needle) continue;
        const std::string name = ObjectName(memory, uint32_t(address + 4 * i));
        for (size_t n = 0; n < names.size(); ++n) {
          if (!found[n] && name == names[n]) {
            found[n] = uint32_t(address + 4 * i);
            --missing;
          }
        }
      }
    }
    address = std::max(region_end, address + kScanStep);
  }
  return found;
}

// The entries of a menu page, in their order (its member "items"); empty if it has none.
std::vector<uint32_t> PageItems(rex::memory::Memory* memory, uint32_t page) {
  for (uint32_t offset = 0; page && offset < kPageMembersSize; offset += 4) {
    const uint32_t member = ReadU32(memory, page + offset);
    if (!member || ReadU32(memory, member) != kScriptListVtable || ObjectName(memory, member) != kItemsMember) {
      continue;
    }
    const uint32_t list = ReadU32(memory, member + kListOffset);
    const uint32_t count = ReadU32(memory, list + kListCountOffset);
    const uint32_t array = ReadU32(memory, list + kListArrayOffset);
    std::vector<uint32_t> items;
    for (uint32_t i = 0; list && array && i < count && i < 64; ++i) items.push_back(ReadU32(memory, array + 4 * i));
    return items;
  }
  return {};
}

std::string ReadFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// 1-4: the slot whose save file was written last, 0 if none differs from the others (no save).
int LastPlayedSlot(const fs::path& user_data_root) {
  std::error_code ec;
  int best = 0;
  fs::file_time_type best_time{};
  std::vector<std::string> contents;
  for (const auto& profile : fs::directory_iterator(user_data_root, ec)) {
    const fs::path dir = profile.path() / kTitleId / "00000001";
    if (!fs::is_directory(dir, ec)) continue;
    for (int n = 1; n <= kSlots; ++n) {
      const std::string name = "Save_Slot_" + std::to_string(n);
      const fs::path file = dir / name / (name + ".xsg");
      const auto time = fs::last_write_time(file, ec);
      if (ec) continue;
      contents.push_back(ReadFile(file));
      if (!best || time > best_time) {
        best = n;
        best_time = time;
      }
    }
    break;  // the first profile folder with the game's saves
  }
  bool differ = false;
  for (const std::string& c : contents) differ |= c != contents.front();
  return differ ? best : 0;
}

struct Step {
  rex::memory::Memory* memory;
  int target;  // 2-4
  uint32_t selection = 0, menu = 0;
  int taps = 0;
  bool done = false;
  Clock::time_point last_scan{};
};

// Before each continue A: true while it should wait (the cursor is on its way to the slot).
bool RunStep(Step& s) {
  if (s.done) return false;
  rex::memory::Memory* memory = s.memory;
  if (s.selection && !IsVariable(memory, s.selection, kSelectionVariable)) s.selection = 0;
  if (s.menu && !IsVariable(memory, s.menu, kMenuVariable)) s.menu = 0;
  if (!s.selection || !s.menu) {
    const auto now = Clock::now();
    if (now - s.last_scan < std::chrono::milliseconds(1000)) return false;
    s.last_scan = now;
    const std::vector<uint32_t> found = FindVariables(memory, {kSelectionVariable, kMenuVariable});
    if (!found[0] || !found[1]) return false;  // no menu yet: A as usual
    s.selection = found[0];
    s.menu = found[1];
    REXLOG_INFO("Continue slot: \"selection cur\" at {:08X}, \"current menu\" at {:08X}", s.selection, s.menu);
  }
  const std::vector<uint32_t> items = PageItems(memory, ReadU32(memory, s.menu + kValueOffset));
  if (items.size() != size_t(kSlots)) return false;  // another menu ("Spiel wählen" has three)
  const uint32_t target = items[size_t(s.target - 1)];
  if (ReadU32(memory, s.selection + kValueOffset) == target) {
    REXLOG_INFO("Continue slot: the cursor is on slot {}", s.target);
    s.done = true;
    return false;
  }
  if (s.taps >= 2 * kSlots) {
    REXLOG_WARN("Continue slot: the cursor didn't reach slot {}, A as usual", s.target);
    s.done = true;
    return false;
  }
  // One step down per call (every 1.5 s), so the highlight's move is over before the next look.
  if (!s.taps) REXLOG_INFO("Continue slot: on the slot screen, moving to slot {}", s.target);
  skylanders::menu_input::TapButton(0, rex::input::X_INPUT_GAMEPAD_DPAD_DOWN);
  ++s.taps;
  return true;
}

}  // namespace

void StartContinueSlot(rex::memory::Memory* memory, const fs::path& user_data_root) {
  if (!memory || !REXCVAR_GET(continue_game)) return;
  const int target = LastPlayedSlot(user_data_root);
  REXLOG_INFO("Continue slot: the save played last is in slot {}", target ? std::to_string(target) : "(none)");
  if (target < 2) return;
  auto step = std::make_shared<Step>(Step{memory, target});
  auto mutex = std::make_shared<std::mutex>();
  skylanders::SetContinueStepProvider([step, mutex] {
    std::lock_guard lock(*mutex);
    return RunStep(*step);
  });
}

}  // namespace giantsrecomp
