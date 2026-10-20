// Opens the portal menu when the game asks a player for a Skylander.
//
// Giants has no portal prompt object like Swap Force's CMagicMoment or the prompt static of
// Spyro's Adventure: its portal screens are run by a game script, the "Portal Control" state
// machine of the global level includes. Found 2026-10-20 by comparing guest memory snapshots taken
// at the prompt, in play, in the pause menu, in a cutscene and on the title screen (input script
// `save`, two runs). The pause bookkeeping of tfbXenonApplication changes for the pause menu as
// well, so it can't tell the two apart. The script heap is laid out the same in every run, and two
// of the script's variables change exactly with the prompt. Both are ScriptReference objects
// (vtable 0x820DF750; class names from the classes' meta registrations), +8 their name, +0x1C
// their value:
//   "portal error string"  0 while no portal screen is up, else the StringInfo (vtable 0x820DD548,
//                          +8 its text key, +0x18 the UTF-16 text) of the message on screen:
//                          "203_PortalError_TagTypeNotOnPortal" = "Please put a Skylander on the
//                          Portal of Power." at both prompts of a run (the first one, and the one
//                          after the figure was taken off again); 0 in play, in cutscenes and in
//                          the pause menu
//   "current state name"   the current state of the state machine, an OpDefineMacroSpecialization
//                          (vtable 0x820DFE40) named after it: "Portal Control - In Game - 0 -
//                          Idle" in play, "Portal Control - Portal - 0 - Idle P1 Only" at the
//                          prompt for player 1, "... Idle P2 Only" for player 2 (measured: player 2
//                          joining, and taking player 2's figure off), "Portal Control - Magic
//                          Moment - ..." while a figure comes in
// The other error keys come from the script's strings; what each screen opens (kErrors) follows
// their names and is measured only where marked.
// Several state machines have a "current state name"; the one whose state is a "Portal Control -"
// one is this. Both variables are found by a scan of the physical heap for their class and name
// (they were at 0xB86650E4 and 0xB8679458 in every run, but the scan doesn't rely on that) and
// checked again on every poll, so a script that was loaded anew is found anew.
//
// Polling instead of a mid-asm hook keeps the generated code untouched and costs a few guest reads
// every 30 ms. It only reads.

#include "portal_prompt_watch.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/system/xmemory.h>
#include <rex/types.h>

#include "portal_rex/portal_menu.h"

namespace giantsrecomp {
namespace {

using Clock = std::chrono::steady_clock;

constexpr uint32_t kScriptReferenceVtable = 0x820DF750;
constexpr uint32_t kStringInfoVtable = 0x820DD548;
constexpr uint32_t kStateVtable = 0x820DFE40;  // OpDefineMacroSpecialization
constexpr uint32_t kNameOffset = 0x08;
constexpr uint32_t kValueOffset = 0x1C;
constexpr uint32_t kVariableSize = kValueOffset + 4;
constexpr std::string_view kErrorVariable = "portal error string";
constexpr std::string_view kStateVariable = "current state name";
constexpr std::string_view kPortalStatePrefix = "Portal Control - ";
// The states of a figure sequence: a portal screen ("Portal Control - Portal ...", which also
// covers "Portal Init") or a figure coming in.
constexpr std::string_view kBusyStatePrefixes[] = {"Portal Control - Portal",
                                                   "Portal Control - Magic Moment"};
// The view of physical memory the game's heap is in (the other two alias it); 64 KB pages.
constexpr uint32_t kScanStart = 0xA0000000;
constexpr uint32_t kScanEnd = 0xC0000000;
constexpr uint32_t kScanStep = 0x10000;
// The script variables exist from the title screen on; until then, look again this often.
constexpr auto kRescanInterval = std::chrono::seconds(2);

// The portal check's reasons (Swap Force's EMagicMomentReason values, which the portal menu
// library uses); any other value gets the check's general title.
constexpr uint32_t kCheckReasonOther = 0;
constexpr uint32_t kCheckReasonTooManySkylanders = 1;
constexpr uint32_t kCheckReasonTooManyItems = 2;

struct ErrorAction {
  std::string_view key_prefix;  // the key's number; its name follows
  bool ask;                     // true: the player needs a Skylander (menu), false: portal check
  uint32_t reason;              // for the portal check
};
// The game's portal screens, by text key ("NNN_PortalError_<name>").
constexpr ErrorAction kErrors[] = {
    {"203_", true, 0},                               // TagTypeNotOnPortal (measured)
    {"301_", true, 0},                               // ToyMissing
    {"101_", false, kCheckReasonOther},              // TooManyTags_Gen
    {"102_", false, kCheckReasonTooManySkylanders},  // TooManyMinions
    {"304_", false, kCheckReasonTooManySkylanders},  // NeedsRemoval_Minion
    {"103_", false, kCheckReasonTooManyItems},       // TooManyItems
    {"105_", false, kCheckReasonTooManyItems},       // RemoveItems
    {"106_", false, kCheckReasonOther},              // RemoveLocations
    {"111_", false, kCheckReasonOther},              // TooManyLocations
    {"112_", false, kCheckReasonOther},              // ToyDataMismatch
    {"305_", false, kCheckReasonOther},              // ClearThePortal
};
// 306_PortalError_ConfirmPVP asks a question the game's own buttons answer; nothing to open.

// How long the game must keep waiting before the menu opens, so it comes up with the prompt
// rather than during a one-frame flicker.
constexpr auto kOpenDelay = std::chrono::milliseconds(100);
// The error screen must stay this long before the portal check opens, and if it was closed with
// the error still up, it comes back after a while.
constexpr auto kErrorDelay = std::chrono::milliseconds(400);
constexpr auto kErrorRepeat = std::chrono::seconds(8);

std::thread g_thread;
std::atomic<bool> g_stop{false};
std::atomic<bool> g_active{false};  // a figure sequence state at the last poll

// How many bytes from `address` on are committed, readable guest memory (0 if none).
uint32_t ReadableBytes(rex::memory::Memory* memory, uint32_t address) {
  rex::memory::BaseHeap* heap = memory->LookupHeap(address);
  rex::memory::HeapAllocationInfo info{};
  if (!heap || !heap->QueryRegionInfo(address, &info) ||
      !(info.state & rex::memory::kMemoryAllocationCommit) ||
      !(info.protect & rex::memory::kMemoryProtectRead)) {
    return 0;
  }
  // The region is counted in whole pages from the one that holds `address`.
  const uint64_t end = uint64_t(address & ~(heap->page_size() - 1)) + info.region_size;
  return end > address ? uint32_t(std::min<uint64_t>(end - address, UINT32_MAX)) : 0;
}

uint32_t ReadU32(rex::memory::Memory* memory, uint32_t address) {
  return rex::memory::load_and_swap<uint32_t>(memory->TranslateVirtual<const uint8_t*>(address));
}

// The NUL-terminated name at `address`; empty if it isn't readable or doesn't end in time.
std::string ReadName(rex::memory::Memory* memory, uint32_t address) {
  constexpr uint32_t kMaxName = 128;
  if (!address) return {};
  const uint32_t readable = std::min(ReadableBytes(memory, address), kMaxName);
  const char* text = memory->TranslateVirtual<const char*>(address);
  const char* end = static_cast<const char*>(std::memchr(text, 0, readable));
  return end ? std::string(text, end) : std::string();
}

// The name of the object at `object` if it is one of class `vtable`, else empty.
std::string ObjectName(rex::memory::Memory* memory, uint32_t object, uint32_t vtable) {
  if (!object || ReadableBytes(memory, object) < kNameOffset + 4 ||
      ReadU32(memory, object) != vtable) {
    return {};
  }
  return ReadName(memory, ReadU32(memory, object + kNameOffset));
}

// The name of what the script variable at `variable` holds, if it is one of class `vtable`.
std::string ValueName(rex::memory::Memory* memory, uint32_t variable, uint32_t vtable) {
  return ObjectName(memory, ReadU32(memory, variable + kValueOffset), vtable);
}

bool IsVariable(rex::memory::Memory* memory, uint32_t object, std::string_view name) {
  return object && ReadableBytes(memory, object) >= kVariableSize &&
         ObjectName(memory, object, kScriptReferenceVtable) == name;
}

bool IsPortalStateVariable(rex::memory::Memory* memory, uint32_t object) {
  return IsVariable(memory, object, kStateVariable) &&
         ValueName(memory, object, kStateVtable).starts_with(kPortalStatePrefix);
}

struct Variables {
  uint32_t error = 0;  // "portal error string"
  uint32_t state = 0;  // the Portal Control state machine's "current state name"
};

// Scans the physical heap for the two script variables. Takes a few tens of milliseconds.
Variables FindVariables(rex::memory::Memory* memory) {
  Variables found;
  const uint32_t needle = rex::byte_swap(kScriptReferenceVtable);  // as it is in guest memory
  uint64_t address = kScanStart;
  while (address < kScanEnd && !(found.error && found.state)) {
    rex::memory::BaseHeap* heap = memory->LookupHeap(uint32_t(address));
    rex::memory::HeapAllocationInfo info{};
    if (!heap || !heap->QueryRegionInfo(uint32_t(address), &info) || !info.region_size) {
      address += kScanStep;
      continue;
    }
    const uint64_t region_end = address + info.region_size;  // `address` is page aligned
    if ((info.state & rex::memory::kMemoryAllocationCommit) &&
        (info.protect & rex::memory::kMemoryProtectRead) && info.region_size >= kVariableSize) {
      const uint8_t* host = memory->TranslateVirtual<const uint8_t*>(uint32_t(address));
      const uint64_t words = (info.region_size - kVariableSize) / 4 + 1;
      for (uint64_t i = 0; i < words; ++i) {
        uint32_t word;
        std::memcpy(&word, host + 4 * i, 4);
        if (word != needle) continue;
        const uint32_t object = uint32_t(address + 4 * i);
        if (!found.error && IsVariable(memory, object, kErrorVariable)) {
          found.error = object;
        } else if (!found.state && IsPortalStateVariable(memory, object)) {
          found.state = object;
        }
      }
    }
    address = std::max(region_end, address + kScanStep);
  }
  return found;
}

const ErrorAction* FindError(const std::string& key) {
  for (const ErrorAction& action : kErrors) {
    if (key.starts_with(action.key_prefix)) return &action;
  }
  return nullptr;
}

void Watch(rex::memory::Memory* memory) {
  Variables vars;
  std::optional<Clock::time_point> last_scan;
  std::string last_error, last_state;
  std::optional<uint32_t> waiting_for;  // player whose prompt is up
  Clock::time_point since{};
  bool opened = false;
  std::optional<Clock::time_point> error_since, check_shown;
  while (!g_stop.load()) {
    const auto now = Clock::now();
    if (vars.error && !IsVariable(memory, vars.error, kErrorVariable)) vars.error = 0;
    if (vars.state && !IsPortalStateVariable(memory, vars.state)) vars.state = 0;
    if ((!vars.error || !vars.state) && (!last_scan || now - *last_scan >= kRescanInterval)) {
      const Variables before = vars;
      const Variables found = FindVariables(memory);
      if (!vars.error) vars.error = found.error;
      if (!vars.state) vars.state = found.state;
      last_scan = now;
      if (vars.error != before.error || vars.state != before.state) {
        REXLOG_INFO("Portal prompt: the game's portal script variables are at {:08X} and {:08X}",
                    vars.error, vars.state);
      }
    }

    const std::string error = vars.error ? ValueName(memory, vars.error, kStringInfoVtable) : "";
    const std::string state = vars.state ? ValueName(memory, vars.state, kStateVtable) : "";
    if (error != last_error) {
      REXLOG_INFO("Portal prompt: portal screen \"{}\"", error);
      last_error = error;
    }
    if (state != last_state) {
      REXLOG_INFO("Portal prompt: state \"{}\"", state);
      last_state = state;
    }
    g_active.store(std::any_of(std::begin(kBusyStatePrefixes), std::end(kBusyStatePrefixes),
                               [&](std::string_view prefix) { return state.starts_with(prefix); }));
    const ErrorAction* action = FindError(error);

    if (action && !action->ask) {
      if (!error_since) error_since = now;
      if (now - *error_since >= kErrorDelay && (!check_shown || now - *check_shown >= kErrorRepeat)) {
        REXLOG_INFO("Portal prompt: the game shows \"{}\", opening the portal check", error);
        skylanders::ShowPortalCheck(action->reason);
        check_shown = now;
      }
    } else if (error_since) {
      if (check_shown) skylanders::ClosePortalCheck();
      error_since.reset();
      check_shown.reset();
    }

    // The prompt is player 2's in the "... P2 ..." states, else player 1's.
    std::optional<uint32_t> player;
    if (action && action->ask) player = state.find(" P2") != std::string::npos ? 1u : 0u;
    if (player != waiting_for) {
      if (waiting_for && opened) skylanders::CloseAutoOpenedPortalMenu();
      waiting_for = player;
      since = now;
      opened = false;
    }
    if (waiting_for && !opened && now - since >= kOpenDelay) {
      REXLOG_INFO("Portal prompt: the game asks player {} for a Skylander", *waiting_for + 1);
      skylanders::AutoOpenPortalMenu(*waiting_for);
      opened = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
}

}  // namespace

void StartPortalPromptWatch(rex::memory::Memory* memory) {
  if (!memory || g_thread.joinable()) return;
  g_stop.store(false);
  g_thread = std::thread(Watch, memory);
}

bool PortalPromptActive() { return g_active.load(); }

void StopPortalPromptWatch() {
  g_stop.store(true);
  if (g_thread.joinable()) g_thread.join();
}

}  // namespace giantsrecomp
