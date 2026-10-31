// The game's names for magic items and adventure packs, in its language, for the portal menu,
// and the levels' names for the Discord status.
//
// Text in this game: a scene (a .bld's level.bld, IGZ 6) has a StringInfo object for every text it
// uses, +0x08 pointing to its key ("Anvil_String"). The same archive has a language pack per
// language (ENGLISH.pak, GERMAN.pak, HISPANIC.pak, ...; IGZ 6 too) holding one LanguagePackInfo:
// +0x14 its language (the game's own enum, 9 = German), +0x18 a memory reference to the texts,
// UTF-16BE and NUL-terminated, in the order of the scene's StringInfo objects. Worked out on
// global.bld (908 keys and as many texts: "203_PortalError_TagTypeNotOnPortal" = "Bitte setze
// einen Skylander auf das Magische Portal.", "109_Toy_Name" = "Ninjini") and every item archive
// (2026-10-20).
// Each magic item and adventure pack has an archive rom/item/Item_<name>.bld whose first
// "..._String" text is its name: "Anvil Rain" / "Ambossregen", "Dragonfire Cannon" /
// "Drachenfeuer-Kanone"; kArchives maps them to catalog ids (each checked by its English name).
// The game's names win over the catalog's for the plain variants only: the golden Dragonfire
// Cannon (alternate deco) and the Platinum Treasure Chest (deco 2 of Hidden Treasure) keep theirs.
// Sidekicks keep the catalog's too: the game calls Barkley "TreeRexling".
// The pack is the one the game shows: GERMAN and ITALIAN through the region hook, HISPANIC for
// Spanish (this disc's Latin American text), PORTUGUESE, FRENCH, else ENGLISH.

#include "game_text.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "alchemy/iga8_archive.h"
#include "alchemy/igz6_file.h"
#include "portal_rex/discord_presence.h"
#include "portal_rex/portal_menu.h"

REXCVAR_DECLARE(uint32_t, user_language);

namespace giantsrecomp {

// The games' shared Alchemy readers (skylanders-portal src/alchemy).
namespace alchemy = skylanders::alchemy;
namespace {

constexpr char kItemDir[] = "item";
constexpr char kScene[] = "level.bld";
constexpr std::string_view kNameSuffix = "_String";
constexpr char kGlobalArchive[] = "permanent/global.bld";
constexpr std::string_view kLevelPrefix = "Level_";  // "Level_214_Tutorial_2"
constexpr std::string_view kHubPrefix = "Level_Hub";
constexpr std::string_view kShipNameKey = "ShipName_Default";
constexpr uint32_t kKeyField = 0x08;
constexpr uint32_t kTextsField = 0x18;
constexpr uint16_t kAlternateDeco = 0x400;
constexpr uint16_t kDecoIdMask = 0x1FF;

struct ItemArchive {
  const char* file;
  uint16_t id;
};
constexpr ItemArchive kArchives[] = {
    {"Item_Anvil.bld", 200},           {"Item_SecretStash.bld", 201},
    {"Item_Regeneration.bld", 202},    {"Item_Pirates.bld", 203},
    {"Item_Hourglass.bld", 204},       {"Item_Shield.bld", 205},
    {"Item_Potion.bld", 206},          {"Item_Zapper.bld", 207},
    {"Item_Cannon.bld", 208},          {"Item_Catapult.bld", 209},
    {"Item_Location_Dragon.bld", 300}, {"Item_Location_Ice.bld", 301},
    {"Item_Location_Pirate.bld", 302}, {"Item_Location_Undead.bld", 303},
    {"Item_PvP_BonusLevel.bld", 304},
};
// The Platinum Treasure Chest: Hidden Treasure's id with deco 2.
constexpr uint16_t kHiddenTreasure = 201;
constexpr uint16_t kPlatinumDeco = 2;

std::mutex g_mutex;
std::unordered_map<uint16_t, std::string> g_names;  // by catalog id, UTF-8

// The language pack the game uses for an Xbox 360 language id (user_language).
const char* PackName(uint32_t language) {
  switch (language) {
    case 3:
      return "GERMAN.pak";
    case 4:
      return "FRENCH.pak";
    case 5:
      return "HISPANIC.pak";
    case 6:
      return "ITALIAN.pak";
    case 9:
      return "PORTUGUESE.pak";
    default:
      return "ENGLISH.pak";
  }
}

void AppendUtf8(std::string& out, uint32_t c) {
  if (c < 0x80) {
    out.push_back(char(c));
  } else if (c < 0x800) {
    out.push_back(char(0xC0 | (c >> 6)));
    out.push_back(char(0x80 | (c & 0x3F)));
  } else if (c < 0x10000) {
    out.push_back(char(0xE0 | (c >> 12)));
    out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
    out.push_back(char(0x80 | (c & 0x3F)));
  } else {
    out.push_back(char(0xF0 | (c >> 18)));
    out.push_back(char(0x80 | ((c >> 12) & 0x3F)));
    out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
    out.push_back(char(0x80 | (c & 0x3F)));
  }
}

// The UTF-16BE texts, NUL-separated, as UTF-8.
std::vector<std::string> SplitTexts(const std::vector<uint8_t>& data) {
  std::vector<std::string> out(1);
  for (size_t i = 0; i + 1 < data.size(); i += 2) {
    uint32_t c = (uint32_t(data[i]) << 8) | data[i + 1];
    if (!c) {
      out.emplace_back();
      continue;
    }
    if (c >= 0xD800 && c < 0xDC00 && i + 3 < data.size()) {
      const uint32_t low = (uint32_t(data[i + 2]) << 8) | data[i + 3];
      if (low >= 0xDC00 && low < 0xE000) {
        c = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
        i += 2;
      }
    }
    AppendUtf8(out.back(), c);
  }
  return out;
}

std::optional<alchemy::Igz6File> ParseEntry(alchemy::Iga8Archive* archive, size_t index) {
  return alchemy::Igz6File::Parse(
      [archive, index](uint64_t offset, size_t size, uint8_t* out) {
        return archive->Read(index, offset, size, out);
      },
      archive->length(index));
}

// The name text of an item archive: its scene's first "..._String" text.
std::optional<std::string> ItemName(const std::filesystem::path& path, const std::string& pack) {
  auto archive = alchemy::Iga8Archive::Open(path);
  if (!archive) return std::nullopt;
  const auto texts = ReadSceneTexts(*archive, pack);
  if (!texts) return std::nullopt;
  for (const auto& [key, text] : *texts) {
    if (key.ends_with(kNameSuffix)) return text;
  }
  return std::nullopt;
}

void Load(std::filesystem::path rom, std::string pack) {
  int found = 0;
  for (const ItemArchive& item : kArchives) {
    auto name = ItemName(rom / kItemDir / item.file, pack);
    if (!name || name->empty()) continue;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_names[item.id] = std::move(*name);
    ++found;
  }
  REXLOG_INFO("Game text: {} item names from {}", found, pack);
  // The levels' names for the Discord status, from global.bld: "Level_<number>_<name>" (the
  // level's name: "Level_214_Tutorial_2" = "Junkyard Isles" / "Schrottplatzinseln"; keys whose text
  // is a file name, "Level_207" = "Level_207_Arkus_Adventure", are left out), and the ship, the
  // hub's stages (Level_Hub_2_Stage_1 to 4), as "ShipName_Default" ("Dreadyacht").
  auto names = std::make_shared<std::unordered_map<std::string, std::string>>();
  std::string ship;
  if (auto global = alchemy::Iga8Archive::Open(rom / kGlobalArchive)) {
    if (const auto texts = ReadSceneTexts(*global, pack)) {
      for (const auto& [key, text] : *texts) {
        if (key == kShipNameKey) ship = text;
        if (!key.starts_with(kLevelPrefix) || text.starts_with(kLevelPrefix)) continue;
        const size_t number = kLevelPrefix.size(), end = key.find('_', number);
        if (end == std::string::npos || end == number) continue;
        names->emplace(key.substr(number, end - number), text);
      }
    }
  }
  REXLOG_INFO("Game text: {} level names", names->size());
  skylanders::SetDiscordLevelNames([names, ship](const std::string& level) {
    if (level.starts_with(kHubPrefix)) return ship;
    if (!level.starts_with(kLevelPrefix)) return std::string();
    const size_t number = kLevelPrefix.size(), end = level.find('_', number);
    const auto it = names->find(level.substr(number, end == std::string::npos ? end : end - number));
    return it == names->end() ? std::string() : it->second;
  });
}

}  // namespace

std::string LanguagePackName() { return PackName(REXCVAR_GET(user_language)); }

std::optional<std::vector<std::pair<std::string, std::string>>> ReadSceneTexts(
    alchemy::Iga8Archive& archive, const std::string& pack) {
  const auto scene_index = archive.Find(kScene);
  const auto pack_index = archive.Find(pack);
  if (!scene_index || !pack_index) return std::nullopt;
  auto scene = ParseEntry(&archive, *scene_index);
  auto texts_file = ParseEntry(&archive, *pack_index);
  if (!scene || !texts_file) return std::nullopt;
  const auto info = texts_file->Objects("LanguagePackInfo");
  if (info.empty()) return std::nullopt;
  const auto data = texts_file->MemoryField(info.front(), kTextsField);
  if (!data) return std::nullopt;
  std::vector<std::string> texts = SplitTexts(*data);
  const auto keys = scene->Objects("StringInfo");
  std::vector<std::pair<std::string, std::string>> out;
  for (size_t i = 0; i < keys.size() && i < texts.size(); ++i) {
    out.emplace_back(scene->StringField(keys[i], kKeyField), std::move(texts[i]));
  }
  return out;
}

void InstallGameText(const std::filesystem::path& rom_dir) {
  std::thread(Load, rom_dir, LanguagePackName()).detach();
  skylanders::SetFigureNameProvider(
      [](uint16_t id, uint16_t variant) -> std::optional<std::string> {
        if ((variant & kAlternateDeco) ||
            (id == kHiddenTreasure && (variant & kDecoIdMask) == kPlatinumDeco)) {
          return std::nullopt;
        }
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_names.find(id);
        if (it == g_names.end()) return std::nullopt;
        return it->second;
      });
}

}  // namespace giantsrecomp
