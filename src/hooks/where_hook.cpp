// Where the player is, for problem reports (skylanders-portal dev/where.h).
//
// The level: the level file the game opened last (rom\level: Level_*, Challenge_Level_*,
// Arena_Challenge_*, PvP_Level_*, Title; not the ship's parts, the challenge and arena meshes,
// which load beside a level). The game asks for them in lower case; the names are spelled as on the
// disc.
//
// The player: the static word 0x826710BC holds an object whose +0x50 points at the one with the
// player's position at +0x3C (Trap Team's chain has the same shape). Found 2026-10-29 from memory
// snapshots on the ship of the owner's save (Level_214) while walking; the same in a fresh run and
// in the hub's first stage. That object stays over a level change, with the old level's place while
// the new one loads, so a new level's place counts once the player was put at the level's start (a
// jump of more than 20 in one frame; walking is about 0.13) and has been there for 2 s, or after 30
// s in the level without one (put somewhere else later, a scene's end or a respawn, it gets one
// more line once it has stayed put for 10 s; the chain follows the camera's cuts in a scene), and
// only while the level loader is idle (no script's archive request waiting at 0x826721DC, the
// request holder kept at 0x826F425C with no name at its +8, as the developer kit's level_loaded).
// The title has no player. Read at the start of sub_821E93F8, the script system's update, once a
// frame.

#include "hooks/where_hook.h"

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <rex/hook.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "dev/where.h"

namespace {

constexpr uint32_t kPlayerGlobal = 0x826710BC;
constexpr uint32_t kPlayerLink = 0x50;
constexpr uint32_t kPlayerPosition = 0x3C;
constexpr uint32_t kPendingArchive = 0x826721DC;
constexpr uint32_t kHolderPointer = 0x826F425C;

std::unordered_map<std::string, std::string> g_spelling;  // lower case -> as on the disc; set before the game runs

uint32_t Load32(uint8_t* base, uint32_t address) { return rex::memory::load_and_swap<uint32_t>(base + address); }
float LoadFloat(uint8_t* base, uint32_t address) { return rex::memory::load_and_swap<float>(base + address); }
bool GuestPointer(uint32_t v) { return v >= 0x40000000 && v < 0xC0000000; }

// Committed, readable guest memory: the chain is read beside the game's own code, so a word in it
// may point at a page nobody allocated (an object freed while a level changes), and reading that
// would take the game down.
bool Readable(uint32_t address, uint32_t length) {
  if (!GuestPointer(address)) return false;
  auto* memory = rex::system::kernel_state() ? rex::system::kernel_memory() : nullptr;
  auto* heap = memory ? memory->LookupHeap(address) : nullptr;
  rex::memory::HeapAllocationInfo info{};
  if (!heap || !heap->QueryRegionInfo(address, &info)) return false;
  return (info.state & rex::memory::kMemoryAllocationCommit) && (info.protect & rex::memory::kMemoryProtectRead) &&
         uint64_t(address) + length <= uint64_t(info.base_address) + info.region_size;
}

std::string Lower(std::string s) {
  for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// "D:\level\level_214_tutotial_2.bld" -> "Level_214_Tutotial_2"; "" for any other file.
std::string LevelOfArchive(std::string_view path) {
  static const std::regex level(
      R"([\\/]level[\\/]((?:level|arena_challenge|challenge_level|pvp_level)_[^\\/.]+|title)\.(?:bld|arc)$)",
      std::regex::icase);
  std::cmatch m;
  if (!std::regex_search(path.data(), path.data() + path.size(), m, level)) return {};
  const std::string name = m[1].str();
  const auto it = g_spelling.find(Lower(name));
  return it != g_spelling.end() ? it->second : name;
}

}  // namespace

namespace giantsrecomp {

void InstallWhere(const std::filesystem::path& game_data_root) {
  std::error_code ec;
  for (const auto& e : std::filesystem::directory_iterator(game_data_root / "level", ec)) {
    if (e.path().extension() == ".bld") {
      const std::string stem = e.path().stem().string();
      g_spelling[Lower(stem)] = stem;
    }
  }
  skylanders::dev::WatchLevelArchives(&LevelOfArchive);
}

}  // namespace giantsrecomp

extern "C" void __imp__sub_821E93F8(PPCContext& __restrict ctx, uint8_t* base);

REX_HOOK_RAW(sub_821E93F8) {
  using Clock = std::chrono::steady_clock;
  // The game's thread only: since when the loader is idle, the level and the place last seen,
  // when it came, whether and when the player was put somewhere since.
  static Clock::time_point idle_since, level_since, jumped_at, moved_logged;
  static bool was_idle = false, jumped = false, have_last = false, moved = false, reported = false;
  static std::string seen_level;
  static float last[3] = {}, told[3] = {};  // the place last seen, the place last told
  const auto now = Clock::now();
  const uint32_t request_holder = Load32(base, kHolderPointer);
  const bool idle = !Load32(base, kPendingArchive) && Readable(request_holder + 8, 4) && !Load32(base, request_holder + 8);
  if (idle && !was_idle) idle_since = now;
  was_idle = idle;
  const std::string level = skylanders::dev::ArchiveLevel();
  if (!level.empty()) {
    if (level != seen_level) {
      seen_level = level;
      level_since = now;
      jumped = false;
      reported = false;
    }
    const uint32_t holder = Load32(base, kPlayerGlobal);
    const uint32_t object = Readable(holder + kPlayerLink, 4) ? Load32(base, holder + kPlayerLink) : 0;
    bool player = false;
    float place[3] = {};
    if (Readable(object + kPlayerPosition, 12)) {
      for (int i = 0; i < 3; ++i) place[i] = LoadFloat(base, object + kPlayerPosition + 4 * i);
      if (have_last && std::fabs(place[0] - last[0]) + std::fabs(place[1] - last[1]) + std::fabs(place[2] - last[2]) > 20) {
        jumped = true;
        jumped_at = now;
        // Put somewhere else after the level's place was told (a respawn, a scene's end; a scene's
        // cuts move it every few seconds): one more line once it stays put for 10 s.
        if (reported) moved = true;
      }
      std::memcpy(last, place, sizeof(last));
      have_last = true;
      const bool settled = (jumped && now - jumped_at >= std::chrono::seconds(2)) ||
                           now - level_since >= std::chrono::seconds(30);
      player = settled && idle && now - idle_since >= std::chrono::seconds(2) && level != "Title";
    }
    skylanders::dev::ReportPlace(level, player, place[0], place[1], place[2]);
    if (moved && player && now - jumped_at >= std::chrono::seconds(10) &&
        now - moved_logged >= std::chrono::seconds(10)) {
      moved = false;
      // Back where it was told (a camera's cut and back): nothing new.
      if (std::fabs(place[0] - told[0]) + std::fabs(place[1] - told[1]) + std::fabs(place[2] - told[2]) > 5) {
        moved_logged = now;
        std::memcpy(told, place, sizeof(told));
        skylanders::dev::LogPlace("moved");
      }
    }
    if (player && !reported) {
      reported = true;
      std::memcpy(told, place, sizeof(told));
    }
  }
  __imp__sub_821E93F8(ctx, base);
}
