// Every Skylander's upgrades for the portal menu's details view.
//
// The texts are in the Skylander's scene archive (rom/character/NNN_<model>.bld, see
// toy_portraits.cpp) as "NNN_..." keys (game_text.cpp has how texts are stored), named by hand in
// many spellings: the upgrades as Name(kk) / Desc(kk) (also "Name00", "Descr(kk)"), the two
// starting attacks as Action1PowerName, Action1_Ability_Name, PrimaryAttackName, ..., the two paths
// as UpgradePath1Name, Upgrade_PathA, UpgradeBranch_Left, ..., with "desc" in the description's
// key. Counted over every character archive (2026-10-20). The upgrades are laid out the same way
// in all of them (checked on Cynder and Ninjini by what the descriptions say): 00-03 the four
// basic upgrades, 04-06 the first path, 07-09 the second, 10 the Soul Gem, 11 the Wow Pow (only
// Spyro's Adventure's Skylanders have one; shown for their Series 2 figures).
// The prices are the global scene's Minion_UpgradeCosts (global.bld, ValueInfo objects
// "UpgradeCost_(01)" ... "(09)", the value at +0x20): 500, 700, 900, 1200 for the basic upgrades,
// 1700, 2200, 3000 along a path, 4000 for the Soul Gem, 5000 for the Wow Pow. They are constants
// here: reading them would take decoding most of that 42 MB scene.
// The upgrade bits of a figure (FigureStats::upgrades) follow the game's own field list
// pp_Flags1_UpgradeBits: bit 0 a path is chosen, bit 1 the second path, pp_Upgrade(1) ... (8) from
// bit 2 (the basic four, the chosen path's three, the Soul Gem), pp_Upgrade_WowPow at 10 and
// pp_Upgrade_AltPath_Stored from 11 (the other path's upgrades); the same as in Swap Force.
// The pictures sit in the character's .arc as textures of their own,
// ".../upgradeposes/<model>/final/<model>_<suffix>.png/...tex.igz", 512x360 DXT5 stored upside
// down (every one checked came out so: Cynder's, Ninjini's): <suffix> "up1" ... "up4",
// "up5a" ... "up7b", "up8" and "wowpow" for Spyro's Adventure's Skylanders, "upgrade1" ...,
// "upgradesoulgem" for Giants', "primary" / "secondary" for the starting attacks. Each is read when
// the details view first asks for it.

#include "toy_upgrades.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <map>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <rex/logging.h>

#include "game/alchemy/iga8_archive.h"
#include "game/alchemy/igz6_file.h"
#include "game/game_text.h"

namespace giantsrecomp {
namespace {

using skylanders::FigureImage;
using skylanders::FigureUpgrade;
using skylanders::FigureUpgradePath;
using skylanders::FigureUpgrades;

constexpr char kCharacterDir[] = "character";
constexpr std::string_view kPosesPath = "/upgradeposes/";
constexpr std::string_view kKeyPrefix = "upgrade:";

constexpr uint32_t kCosts[] = {500, 700, 900, 1200, 1700, 2200, 3000, 4000, 5000};
constexpr int kBasicCount = 4;
constexpr int kPathLength = 3;
constexpr int kFirstPath = 4;
constexpr int kSoulGem = 10;
constexpr int kWowPow = 11;
constexpr int kFirstUpgradeBit = 2;
constexpr int kFirstPathBit = kFirstUpgradeBit + kBasicCount;
constexpr int kSoulGemBit = kFirstPathBit + kPathLength;
constexpr int kWowPowBit = kSoulGemBit + 1;
constexpr int kFirstAlternateBit = kWowPowBit + 1;

// Catalog variant bits and Spyro's Adventure's own variant ids (as in toy_portraits.cpp).
constexpr uint16_t kSeries2 = 0x800;
constexpr uint16_t kLegendaryIds = 400;
constexpr uint16_t kDarkSpyro = 28;
constexpr uint16_t kSpyro = 16;

struct CharacterText {
  std::string start_name[2], start_desc[2];
  std::string path_name[2], path_desc[2];
  std::map<int, std::string> name, desc;  // by upgrade number
  std::filesystem::path arc;
  std::unordered_map<std::string, size_t> pictures;  // suffix -> the .arc's entry
};

std::mutex g_mutex;
std::unordered_map<uint16_t, CharacterText> g_characters;
std::unordered_map<uint32_t, std::shared_ptr<const FigureUpgrades>> g_upgrades;  // id << 1 | S2
std::unordered_map<std::string, std::shared_ptr<const FigureImage>> g_images;
std::unordered_set<std::string> g_requested;
std::deque<std::string> g_queue;
bool g_worker = false;

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}

bool Has(const std::string& s, std::string_view part) {
  return s.find(part) != std::string::npos;
}

// The game's button glyphs in its texts, as the button's letter: "[" the first attack (X; Whirlwind
// "Hold [ to charge up a super powerful Rainbow of Doom"), "]" the second (Y; Sonic Boom's babies),
// "{" the third (B; "Press { to fly"). Counted over the character texts: no other glyph is used.
std::string ButtonNames(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '[': out += 'X'; break;
      case ']': out += 'Y'; break;
      case '{': out += 'B'; break;
      default: out.push_back(c);
    }
  }
  return out;
}

// Sorts one text of a character's scene into `t`; `key` is without its "NNN_".
void Classify(std::string key, const std::string& game_text, CharacterText& t) {
  static const std::regex kUpgrade(R"(^(name|descr?)\(?(\d\d)\)?$)");
  static const std::regex kPath(R"((?:path|branch)_?(1|2|a|b|left|right))");
  const std::string text = ButtonNames(game_text);
  key = Lower(std::move(key));
  std::smatch m;
  if (std::regex_match(key, m, kUpgrade)) {
    (m[1] == "name" ? t.name : t.desc)[std::stoi(m[2])] = text;
    return;
  }
  const bool desc = Has(key, "desc");
  if (std::regex_search(key, m, kPath)) {
    const std::string which = m[1];
    const int path = which == "1" || which == "a" || which == "left" ? 0 : 1;
    (desc ? t.path_desc : t.path_name)[path] = text;
    return;
  }
  int start = -1;
  if (Has(key, "action1") || Has(key, "primary")) {
    start = 0;
  } else if (Has(key, "action2") || Has(key, "secondary")) {
    start = 1;
  }
  if (start < 0) return;
  if (desc) {
    t.start_desc[start] = text;
  } else if (Has(key, "name")) {
    t.start_name[start] = text;
  }
}

// The picture suffixes an upgrade may have, Spyro's Adventure's naming first.
std::vector<std::string> PictureSuffixes(int upgrade) {
  if (upgrade < kBasicCount) {
    const std::string n = std::to_string(upgrade + 1);
    return {"up" + n, "upgrade" + n};
  }
  if (upgrade < kSoulGem) {
    const int in_path = (upgrade - kFirstPath) % kPathLength;
    const char path = (upgrade - kFirstPath) / kPathLength == 0 ? 'a' : 'b';
    const std::string n = std::to_string(kBasicCount + 1 + in_path) + path;
    return {"up" + n, "upgrade" + n};
  }
  if (upgrade == kSoulGem) return {"up8", "upgrade8", "upgradesoulgem"};
  return {"wowpow"};
}

std::string PictureKey(uint16_t id, const CharacterText& t,
                       const std::vector<std::string>& suffixes) {
  for (const std::string& suffix : suffixes) {
    if (t.pictures.contains(suffix)) {
      return std::string(kKeyPrefix) + std::to_string(id) + ":" + suffix;
    }
  }
  return {};
}

void Load(std::filesystem::path rom, std::string pack) {
  std::error_code ec;
  int characters = 0;
  for (const auto& entry : std::filesystem::directory_iterator(rom / kCharacterDir, ec)) {
    const std::string file = entry.path().filename().string();
    if (file.size() < 9 || !entry.path().extension().string().ends_with(".bld") ||
        !std::isdigit((unsigned char)file[0]) || !std::isdigit((unsigned char)file[1]) ||
        !std::isdigit((unsigned char)file[2]) || file[3] != '_') {
      continue;
    }
    // The decos share their Skylander's upgrades.
    const std::string stem = Lower(entry.path().stem().string());
    if (stem.ends_with("_legendary") || stem.ends_with("_alt") || stem.ends_with("_red") ||
        stem.ends_with("_dark")) {
      continue;
    }
    auto archive = alchemy::Iga8Archive::Open(entry.path());
    if (!archive) continue;
    const auto texts = ReadSceneTexts(*archive, pack);
    if (!texts) continue;
    const std::string prefix = file.substr(0, 4);
    CharacterText t;
    for (const auto& [key, text] : *texts) {
      if (key.starts_with(prefix)) Classify(key.substr(prefix.size()), text, t);
    }
    if (t.name.size() <= kSoulGem) continue;  // not a playable Skylander
    t.arc = entry.path();
    t.arc.replace_extension(".arc");
    if (auto arc = alchemy::Iga8Archive::Open(t.arc)) {
      for (size_t i = 0; i < arc->size(); ++i) {
        const std::string name = Lower(arc->name(i));
        const size_t at = name.find(kPosesPath);
        const size_t png = name.find(".png", at == std::string::npos ? 0 : at);
        if (at == std::string::npos || png == std::string::npos) continue;
        const std::string base = name.substr(0, png);
        t.pictures[base.substr(base.find_last_of("/_") + 1)] = i;
      }
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_characters[uint16_t(std::stoi(file.substr(0, 3)))] = std::move(t);
    ++characters;
  }
  REXLOG_INFO("Toy upgrades: the upgrades of {} Skylanders from {}", characters, pack);
}

std::shared_ptr<const FigureUpgrades> Build(uint16_t id, const CharacterText& t, bool series2) {
  auto result = std::make_shared<FigureUpgrades>();
  auto text = [](const std::map<int, std::string>& m, int i) {
    auto it = m.find(i);
    return it == m.end() ? std::string() : it->second;
  };
  auto upgrade = [&](int i, int bit, uint32_t cost) {
    FigureUpgrade u;
    u.name = text(t.name, i);
    u.description = text(t.desc, i);
    u.bit = bit;
    u.cost = cost;
    u.image = PictureKey(id, t, PictureSuffixes(i));
    return u;
  };
  FigureUpgradePath start;
  start.kind = FigureUpgradePath::Kind::kStart;
  for (int k = 0; k < 2; ++k) {
    if (t.start_name[k].empty()) continue;
    FigureUpgrade u;
    u.name = t.start_name[k];
    u.description = t.start_desc[k];
    u.image = PictureKey(id, t, {k == 0 ? "primary" : "secondary"});
    start.upgrades.push_back(std::move(u));
  }
  if (!start.upgrades.empty()) result->paths.push_back(std::move(start));
  FigureUpgradePath basic;
  basic.kind = FigureUpgradePath::Kind::kBasic;
  for (int i = 0; i < kBasicCount; ++i) {
    basic.upgrades.push_back(upgrade(i, kFirstUpgradeBit + i, kCosts[i]));
  }
  result->paths.push_back(std::move(basic));
  for (int p = 0; p < 2; ++p) {
    FigureUpgradePath path;
    path.kind = FigureUpgradePath::Kind::kPath;
    path.name = t.path_name[p];
    path.description = t.path_desc[p];
    for (int j = 0; j < kPathLength; ++j) {
      FigureUpgrade u = upgrade(kFirstPath + kPathLength * p + j, kFirstPathBit + j,
                                kCosts[kBasicCount + j]);
      u.alternate_bit = kFirstAlternateBit + j;
      path.upgrades.push_back(std::move(u));
    }
    result->paths.push_back(std::move(path));
  }
  FigureUpgradePath soul_gem;
  soul_gem.kind = FigureUpgradePath::Kind::kSoulGem;
  soul_gem.upgrades.push_back(upgrade(kSoulGem, kSoulGemBit, kCosts[kBasicCount + kPathLength]));
  result->paths.push_back(std::move(soul_gem));
  if (series2 && t.name.contains(kWowPow)) {
    FigureUpgradePath wow_pow;
    wow_pow.kind = FigureUpgradePath::Kind::kWowPow;
    wow_pow.upgrades.push_back(upgrade(kWowPow, kWowPowBit, kCosts[kBasicCount + kPathLength + 1]));
    result->paths.push_back(std::move(wow_pow));
  }
  return result;
}

std::shared_ptr<const FigureUpgrades> Lookup(uint16_t id, uint16_t variant) {
  if (id >= kLegendaryIds && id < kLegendaryIds + 100) {
    id = id - kLegendaryIds;
  } else if (id == kDarkSpyro) {
    id = kSpyro;
  }
  const bool series2 = variant & kSeries2;
  std::lock_guard<std::mutex> lock(g_mutex);
  const uint32_t key = (uint32_t(id) << 1) | uint32_t(series2);
  if (auto it = g_upgrades.find(key); it != g_upgrades.end()) return it->second;
  auto character = g_characters.find(id);
  if (character == g_characters.end()) return nullptr;
  return g_upgrades[key] = Build(id, character->second, series2);
}

// Reads one upgrade picture: the .arc entry is an IGZ with a single igImage2.
std::shared_ptr<const FigureImage> ReadPicture(const std::filesystem::path& arc_path, size_t entry) {
  auto arc = alchemy::Iga8Archive::Open(arc_path);
  if (!arc) return nullptr;
  auto data = arc->ReadAll(entry);
  if (!data) return nullptr;
  auto bytes = std::make_shared<std::vector<uint8_t>>(std::move(*data));
  auto igz = alchemy::Igz6File::Parse(
      [bytes](uint64_t offset, size_t size, uint8_t* out) {
        if (offset + size > bytes->size()) return false;
        std::copy_n(bytes->data() + offset, size, out);
        return true;
      },
      bytes->size());
  if (!igz) return nullptr;
  const auto images = igz->Images();
  if (images.empty()) return nullptr;
  auto rgba = igz->Decode(images.front());
  if (!rgba) return nullptr;
  // Stored upside down.
  const size_t row = size_t(rgba->width) * 4;
  for (int y = 0; y < rgba->height / 2; ++y) {
    std::swap_ranges(rgba->pixels.begin() + y * row, rgba->pixels.begin() + (y + 1) * row,
                     rgba->pixels.begin() + (rgba->height - 1 - y) * row);
  }
  auto out = std::make_shared<FigureImage>();
  out->width = rgba->width;
  out->height = rgba->height;
  out->rgba = std::move(rgba->pixels);
  return out;
}

// Reads the requested pictures one after the other, and ends when there are none left.
void PictureWorker() {
  for (;;) {
    std::string key;
    std::filesystem::path arc;
    size_t entry = 0;
    {
      std::lock_guard<std::mutex> lock(g_mutex);
      if (g_queue.empty()) {
        g_worker = false;
        return;
      }
      key = std::move(g_queue.front());
      g_queue.pop_front();
      // "upgrade:<id>:<suffix>"
      const size_t colon = key.find(':', kKeyPrefix.size());
      const uint16_t id = uint16_t(std::stoi(key.substr(kKeyPrefix.size(), colon)));
      auto character = g_characters.find(id);
      if (colon == std::string::npos || character == g_characters.end()) continue;
      auto picture = character->second.pictures.find(key.substr(colon + 1));
      if (picture == character->second.pictures.end()) continue;
      arc = character->second.arc;
      entry = picture->second;
    }
    auto image = ReadPicture(arc, entry);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_images[key] = std::move(image);
  }
}

}  // namespace

void InstallToyUpgrades(const std::filesystem::path& rom_dir) {
  std::thread(Load, rom_dir, LanguagePackName()).detach();
  skylanders::SetFigureUpgradesProvider(
      [](uint16_t id, uint16_t variant) { return Lookup(id, variant); });
}

std::shared_ptr<const FigureImage> UpgradeImage(const std::string& key) {
  if (!key.starts_with(kKeyPrefix)) return nullptr;
  std::lock_guard<std::mutex> lock(g_mutex);
  if (auto it = g_images.find(key); it != g_images.end()) return it->second;
  if (g_requested.insert(key).second) {
    g_queue.push_back(key);
    if (!g_worker) {
      g_worker = true;
      std::thread(PictureWorker).detach();
    }
  }
  return nullptr;
}

}  // namespace giantsrecomp
