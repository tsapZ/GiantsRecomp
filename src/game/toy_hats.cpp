// The hats for the portal menu's details view, in the game's language.
//
// A figure stores its hat as the game's kTfbSpyroTag_Hat_* number (portal FigureStats::hat;
// Swap Force's executable has the enum: 1 Army, 2 French, ..., 46 VintageBaseballCap). Giants has
// an archive for every hat it sells, rom/misc/StoreItem_Hat_NNN.bld with NNN = number - 1 (000
// holds "army.png", 045 "vintage_baseball_cap.png"): its scene has the hat's icon
// (".../haticons/sky2/<name>.png", 128x128, a UI sprite) and the text "Hat_Desc_NNN" with the
// bonuses, one per line ("+5 Armor", "+ 10 Critical Hit", "*+3 Speed"). The names are the
// global scene's texts "HatName(kk)" with kk = number - 1 as well: the first 37 checked one by one
// against the enum, "Combat Hat" = Army to "Trojan Helmet" = Trojan (2026-10-20). Hats Giants
// doesn't sell (later games') get neither; the menu then shows their number.
//
// Each bonus also gets the stat's symbol from the game's stats screen
// (character/UI_StatsScreen_Extra.bld: "armor", "criticalhit", "elementalstrength", "speed",
// 128x128 UI sprites next to the hat icons, art keys "stat:<name>"). The bonus text is in the
// game's language, so which stat a line is comes from the same text in ENGLISH.pak, line by line
// ("Elemental Power", "Armor", "Critical Hit", "Speed"; the packs list the bonuses in the same
// order, e.g. "+7 Critical Hit\n+7 Elemental Power" = "+7 Kritische Treffer\n+7 Elementarkraft").

#include "toy_hats.h"

#include <cstdio>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_map>

#include <rex/logging.h>

#include "game/alchemy/iga8_archive.h"
#include "game/alchemy/igz6_file.h"
#include "game/game_text.h"

namespace giantsrecomp {
namespace {

using skylanders::FigureImage;
using skylanders::HatInfo;
using skylanders::HatStat;

constexpr char kGlobalArchive[] = "permanent/global.bld";
constexpr char kHatArchives[] = "misc/StoreItem_Hat_%03u.bld";
constexpr char kScene[] = "level.bld";
constexpr std::string_view kIconPath = "/haticons/";
constexpr std::string_view kKeyPrefix = "hat:";
constexpr uint32_t kMaxHat = 255;
constexpr char kStatsArchive[] = "character/UI_StatsScreen_Extra.bld";
constexpr char kEnglishPack[] = "ENGLISH.pak";

// A bonus's stat by its English name, and the stats screen's picture of it.
constexpr std::pair<std::string_view, const char*> kStatPictures[] = {
    {"Elemental Power", "elementalstrength"},
    {"Armor", "armor"},
    {"Critical Hit", "criticalhit"},
    {"Speed", "speed"},
};

std::mutex g_mutex;
std::unordered_map<uint16_t, std::shared_ptr<const HatInfo>> g_hats;
std::unordered_map<std::string, std::shared_ptr<const FigureImage>> g_images;

// "+5 Armor\n+5 Elemental Power" -> the two bonuses.
std::vector<HatStat> ParseBonuses(const std::string& text) {
  static const std::regex kBonus(R"(([+-])\s*(\d+)\s+([^\n]+))");
  std::vector<HatStat> out;
  for (std::sregex_iterator it(text.begin(), text.end(), kBonus), end; it != end; ++it) {
    HatStat stat;
    stat.value = float(std::stoi((*it)[2])) * ((*it)[1] == "-" ? -1.0f : 1.0f);
    stat.name = (*it)[3];
    while (!stat.name.empty() && (stat.name.back() == '\r' || stat.name.back() == ' ')) {
      stat.name.pop_back();
    }
    out.push_back(std::move(stat));
  }
  return out;
}

// Calls `use` with every picture of the archive's scene whose name `want` accepts.
template <typename Want, typename Use>
void ReadPictures(alchemy::Iga8Archive& archive, Want want, Use use) {
  const auto scene_index = archive.Find(kScene);
  if (!scene_index) return;
  auto* a = &archive;
  const size_t index = *scene_index;
  auto igz = alchemy::Igz6File::Parse(
      [a, index](uint64_t offset, size_t size, uint8_t* out) {
        return a->Read(index, offset, size, out);
      },
      archive.length(index));
  if (!igz) return;
  for (const auto& image : igz->Images()) {
    if (!want(image.name)) continue;
    auto rgba = igz->Decode(image);
    if (!rgba) continue;
    auto out = std::make_shared<FigureImage>();
    out->width = rgba->width;
    out->height = rgba->height;
    out->rgba = std::move(rgba->pixels);
    if (!use(image.name, std::move(out))) return;
  }
}

std::shared_ptr<const FigureImage> ReadIcon(alchemy::Iga8Archive& archive) {
  std::shared_ptr<const FigureImage> icon;
  ReadPictures(
      archive, [](const std::string& name) { return name.find(kIconPath) != std::string::npos; },
      [&](const std::string&, std::shared_ptr<const FigureImage> image) {
        icon = std::move(image);
        return false;  // the first one
      });
  return icon;
}

// The stat symbols, as art keys "stat:<name>".
int ReadStatPictures(const std::filesystem::path& rom) {
  auto archive = alchemy::Iga8Archive::Open(rom / kStatsArchive);
  if (!archive) return 0;
  int count = 0;
  ReadPictures(
      *archive,
      [](const std::string& name) {
        for (const auto& [stat, picture] : kStatPictures) {
          if (name.ends_with(std::string("/") + picture + ".png")) return true;
        }
        return false;
      },
      [&](const std::string& name, std::shared_ptr<const FigureImage> image) {
        const size_t slash = name.rfind('/');
        const std::string picture = name.substr(slash + 1, name.size() - slash - 5);
        std::lock_guard<std::mutex> lock(g_mutex);
        g_images["stat:" + picture] = std::move(image);
        ++count;
        return true;
      });
  return count;
}

// The symbols for `stats` from the same bonuses in English.
void AddStatPictures(std::vector<HatStat>& stats, const std::vector<HatStat>& english) {
  for (size_t i = 0; i < stats.size() && i < english.size(); ++i) {
    for (const auto& [stat, picture] : kStatPictures) {
      if (english[i].name == stat) stats[i].image = std::string("stat:") + picture;
    }
  }
}

void Load(std::filesystem::path rom, std::string pack) {
  // The names: "HatName(kk)" of the global scene.
  std::unordered_map<uint32_t, std::string> names;
  static const std::regex kName(R"(^HatName\((\d+)\)$)");
  if (auto global = alchemy::Iga8Archive::Open(rom / kGlobalArchive)) {
    if (auto texts = ReadSceneTexts(*global, pack)) {
      for (const auto& [key, text] : *texts) {
        std::smatch m;
        if (std::regex_match(key, m, kName)) names[uint32_t(std::stoi(m[1])) + 1] = text;
      }
    }
  }
  const int stat_pictures = ReadStatPictures(rom);
  int with_picture = 0;
  for (uint32_t hat = 1; hat <= kMaxHat; ++hat) {
    char file[64];
    std::snprintf(file, sizeof(file), kHatArchives, hat - 1);
    auto info = std::make_shared<HatInfo>();
    if (auto it = names.find(hat); it != names.end()) info->name = it->second;
    std::shared_ptr<const FigureImage> icon;
    if (auto archive = alchemy::Iga8Archive::Open(rom / file)) {
      if (auto texts = ReadSceneTexts(*archive, pack)) {
        for (const auto& [key, text] : *texts) {
          if (key.starts_with("Hat_Desc_")) info->stats = ParseBonuses(text);
        }
      }
      if (auto texts = ReadSceneTexts(*archive, kEnglishPack)) {
        for (const auto& [key, text] : *texts) {
          if (key.starts_with("Hat_Desc_")) AddStatPictures(info->stats, ParseBonuses(text));
        }
      }
      icon = ReadIcon(*archive);
    }
    if (info->name.empty() && !icon) continue;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (icon) {
      info->image = std::string(kKeyPrefix) + std::to_string(hat);
      g_images[info->image] = std::move(icon);
      ++with_picture;
    }
    g_hats[uint16_t(hat)] = std::move(info);
  }
  REXLOG_INFO("Toy hats: {} hat names, {} with picture and bonuses, {} stat symbols, from {}",
              names.size(), with_picture, stat_pictures, pack);
}

}  // namespace

void InstallToyHats(const std::filesystem::path& rom_dir) {
  std::thread(Load, rom_dir, LanguagePackName()).detach();
  skylanders::SetHatInfoProvider([](uint16_t hat) -> std::shared_ptr<const HatInfo> {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_hats.find(hat);
    return it == g_hats.end() ? nullptr : it->second;
  });
}

std::shared_ptr<const FigureImage> HatImage(const std::string& key) {
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_images.find(key);
  return it == g_images.end() ? nullptr : it->second;
}

}  // namespace giantsrecomp
