// Pictures of the Skylanders for the portal menu: the portraits the game's collection screen shows.
//
// Every Skylander has a scene archive of its own, rom/character/NNN_<model>[_<deco>].bld. NNN is
// its figure id, the same as the portal catalog's (032_Cynder, 109_Djini for Ninjini, 100_Eagle
// for Jet-Vac); the scene names it once more as "NNN_Portal_Toy". The suffix marks a deco with a
// model of its own: _Legendary, _Alt (Polar Whirlwind, Royal Double Trouble, Scarlet Ninjini,
// Granite Crusher, ...), _Red (Punch Pop Fizz), _Dark (Dark Spyro). The scene (the archive's
// level.bld, IGZ version 6) holds the portraits as 256x256 DXT5 textures named
// "levels/includes/ui_main/sprites/collections/sky2/portraits/<name>.png": "cynder",
// "cynder_series2", "eruptor_lightcore", "eaglelegendary", "giantmagic_alt", ...
// Indexed on this disc (2026-10-20, every character archive): all Skylanders of Spyro's Adventure
// and Giants have one; sidekicks and magic items have none in their scenes.
//
// Which one a figure gets, by the catalog variant (year << 12 | Series 2 << 11 | alternate deco
// << 10 | LightCore << 9 | deco id): an alternate deco 3 is the Legendary, any other alternate
// deco the _Alt / _Red model; within those the picture whose name has "series2" for a Series 2
// figure and "lightcore" for a LightCore one. Every portrait of the figure is scored on these and
// the best one wins, so a figure without a picture of its own (most chase variants) gets the
// nearest. Spyro's Adventure's variants have ids of their own: 400 + n is the Legendary of n
// (404 Legendary Bash), 28 is Dark Spyro (016_Spyro_Dark).
// The element symbols come from CardGame_Streaming.bld (the card game's "element_icon_<name>").

#include "toy_portraits.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <rex/logging.h>

#include "game/alchemy/iga8_archive.h"
#include "game/alchemy/igz6_file.h"
#include "portal_rex/portal_menu.h"

namespace giantsrecomp {
namespace {

using Clock = std::chrono::steady_clock;
using skylanders::FigureImage;

constexpr std::string_view kPortraitPath = "/collections/sky2/portraits/";
constexpr std::string_view kElementPrefix = "element_icon_";
constexpr char kElementArchive[] = "CardGame_Streaming.bld";
constexpr char kScene[] = "level.bld";

// Variant bits of the portal catalog (portal/figure_catalog.h).
constexpr uint16_t kSeries2 = 0x800;
constexpr uint16_t kAlternateDeco = 0x400;
constexpr uint16_t kLightCore = 0x200;
constexpr uint16_t kDecoIdMask = 0x1FF;
constexpr uint16_t kLegendaryDeco = 3;
// Spyro's Adventure's own variant ids.
constexpr uint16_t kLegendaryIds = 400;
constexpr uint16_t kDarkSpyro = 28;
constexpr uint16_t kSpyro = 16;

struct Portrait {
  std::string deco;  // the archive's suffix in lower case: "", "legendary", "alt", "red", "dark"
  std::string name;  // the picture's file name in lower case, without ".png"
  std::shared_ptr<const FigureImage> image;
};

std::mutex g_mutex;
std::unordered_map<uint16_t, std::vector<Portrait>> g_portraits;               // by figure id
std::unordered_map<std::string, std::shared_ptr<const FigureImage>> g_art;    // by art key

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return char(std::tolower(c)); });
  return s;
}

bool Has(const std::string& s, std::string_view part) {
  return s.find(part) != std::string::npos;
}

// The file name of a picture's source path, without ".png".
std::string PictureName(const std::string& path) {
  std::string name = Lower(path.substr(path.find_last_of('/') + 1));
  if (name.size() > 4 && name.ends_with(".png")) name.resize(name.size() - 4);
  return name;
}

std::shared_ptr<const FigureImage> ToFigureImage(RgbaImage&& image) {
  auto out = std::make_shared<FigureImage>();
  out->width = image.width;
  out->height = image.height;
  out->rgba = std::move(image.pixels);
  return out;
}

// Calls `use` for every picture in the scene of `archive_path` whose path passes `want`.
template <typename Want, typename Use>
int ReadPictures(const std::filesystem::path& archive_path, Want want, Use use) {
  auto archive = alchemy::Iga8Archive::Open(archive_path);
  if (!archive) return 0;
  const auto scene = archive->Find(kScene);
  if (!scene) return 0;
  auto* a = archive.get();
  const size_t index = *scene;
  auto igz = alchemy::Igz6File::Parse(
      [a, index](uint64_t offset, size_t size, uint8_t* out) {
        return a->Read(index, offset, size, out);
      },
      archive->length(index));
  if (!igz) return 0;
  int count = 0;
  for (const auto& image : igz->Images()) {
    if (!want(image)) continue;
    if (auto rgba = igz->Decode(image)) {
      use(image, std::move(*rgba));
      ++count;
    }
  }
  return count;
}

void Load(std::filesystem::path dir) {
  const auto start = Clock::now();
  std::error_code ec;
  int portraits = 0, archives = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    const std::string file = entry.path().filename().string();
    // NNN_<model>[_<deco>].bld
    if (file.size() < 9 || !entry.path().extension().string().ends_with(".bld") ||
        !std::isdigit((unsigned char)file[0]) || !std::isdigit((unsigned char)file[1]) ||
        !std::isdigit((unsigned char)file[2]) || file[3] != '_') {
      continue;
    }
    const uint16_t id = uint16_t(std::stoi(file.substr(0, 3)));
    const std::string stem = Lower(entry.path().stem().string());
    std::string deco;
    for (const char* suffix : {"legendary", "alt", "red", "dark"}) {
      if (stem.ends_with(std::string("_") + suffix)) deco = suffix;
    }
    ++archives;
    portraits += ReadPictures(
        entry.path(),
        [](const alchemy::Igz6File::Image& image) {
          return Has(Lower(image.name), kPortraitPath) && image.width == image.height;
        },
        [&](const alchemy::Igz6File::Image& image, RgbaImage&& rgba) {
          Portrait portrait{deco, PictureName(image.name), ToFigureImage(std::move(rgba))};
          std::lock_guard<std::mutex> lock(g_mutex);
          g_portraits[id].push_back(std::move(portrait));
        });
  }
  // The element symbols: "element_icon_<name>", the game's names for undead and tech differ.
  static const std::unordered_map<std::string, std::string> kElements = {
      {"magic", "magic"}, {"water", "water"}, {"air", "air"},     {"death", "undead"},
      {"mech", "tech"},   {"fire", "fire"},   {"earth", "earth"}, {"life", "life"},
  };
  const int elements = ReadPictures(
      dir / kElementArchive,
      [](const alchemy::Igz6File::Image& image) {
        return PictureName(image.name).starts_with(kElementPrefix);
      },
      [&](const alchemy::Igz6File::Image& image, RgbaImage&& rgba) {
        const std::string element = PictureName(image.name).substr(kElementPrefix.size());
        if (auto it = kElements.find(element); it != kElements.end()) {
          std::lock_guard<std::mutex> lock(g_mutex);
          g_art["element:" + it->second] = ToFigureImage(std::move(rgba));
        }
      });
  REXLOG_INFO("Toy portraits: {} portraits from {} archives, {} element symbols, in {} ms",
              portraits, archives, elements,
              std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
}

std::shared_ptr<const FigureImage> Lookup(uint16_t id, uint16_t variant) {
  uint16_t base = id;
  std::string deco;
  if (id >= kLegendaryIds && id < kLegendaryIds + 100) {
    base = id - kLegendaryIds;
    deco = "legendary";
  } else if (id == kDarkSpyro) {
    base = kSpyro;
    deco = "dark";
  } else if (variant & kAlternateDeco) {
    deco = (variant & kDecoIdMask) == kLegendaryDeco ? "legendary" : "alt";
  }
  const bool series2 = variant & kSeries2, lightcore = variant & kLightCore;
  std::lock_guard<std::mutex> lock(g_mutex);
  auto it = g_portraits.find(base);
  if (it == g_portraits.end()) return nullptr;
  const Portrait* best = nullptr;
  int best_score = -1;
  for (const Portrait& p : it->second) {
    const bool alt = p.deco == "alt" || p.deco == "red";
    int score = 0;
    if (p.deco == deco || (deco == "alt" && alt)) score += 8;
    if (deco == "alt" ? Has(p.name, "_alt") || Has(p.name, "red")
                      : !deco.empty() && Has(p.name, deco)) {
      score += 4;
    }
    if (Has(p.name, "series2") == series2) score += 2;
    if (Has(p.name, "lightcore") == lightcore) score += 1;
    if (score > best_score) {
      best = &p;
      best_score = score;
    }
  }
  return best ? best->image : nullptr;
}

}  // namespace

void InstallToyPortraits(const std::filesystem::path& character_dir) {
  std::thread(Load, character_dir).detach();
  skylanders::SetFigureImageProvider(
      [](uint16_t id, uint16_t variant) { return Lookup(id, variant); });
  skylanders::SetArtImageProvider([](const std::string& key) -> std::shared_ptr<const FigureImage> {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_art.find(key);
    return it == g_art.end() ? nullptr : it->second;
  });
}

}  // namespace giantsrecomp
