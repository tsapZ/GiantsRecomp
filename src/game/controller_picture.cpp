// The game's controller picture for the settings menu's controls page, as Swap Force's, Trap
// Team's and SuperChargers' (SwapForceRecomp src/game/game_art.cpp, TrapTeamRecomp and
// SuperChargersRecomp src/game/controller_picture.cpp).
//
// item/UI_Options_Item.arc (the options screen's archive) has
// ".../ui_main/sprites/controls/sky2/controls_360.png/0x18bb5ce3.png.igb.tex.igz", found 2026-10-22
// by listing the texture names of every archive on the disc (many level archives carry a copy):
// 537x454 DXT5, stored upside down like the portraits, a drawing of the white pad, its top edge
// (no shoulder buttons in view) at the picture's top, with pure red callout lines, four to five
// pixels wide and without a shadow, from the sticks and buttons to the picture's edges. To clean
// it: the line pixels (IsCallout below) and two pixels around them are filled in straight across
// their line, between the first pixels on either side that aren't line (along the column for a
// pixel in a horizontal line, along the row for one in a vertical line), so the pad's outline and
// shading go on through where a line crossed them; what is left of the lines' red in the pad's
// edges (the DXT blocks mix it into their neighbours) is turned grey. The B button's own dark red
// is kept out of both. Then only the opaque part joined to the guide button is kept, with two
// pixels of its soft edge, and the picture is cut to kCrop. The buttons' places were measured on
// the picture (the face buttons' colours, the sticks' and the d-pad's dark grey, the guide
// button's green ring) and are given in the full picture's pixels; the shoulder buttons' spots sit
// on the top edge where they would be.

#include "controller_picture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <rex/logging.h>

#include "alchemy/iga8_archive.h"
#include "alchemy/igz6_file.h"
#include "alchemy/xenos_dxt.h"

namespace giantsrecomp {

// The games' shared Alchemy readers (skylanders-portal src/alchemy).
namespace alchemy = skylanders::alchemy;
namespace {

using skylanders::FigureImage;

constexpr char kArtKey[] = "controller:xbox360";
constexpr char kArchive[] = "item/UI_Options_Item.arc";
constexpr std::string_view kTexture = "/controls/sky2/controls_360.png/";
constexpr int kWidth = 537, kHeight = 454;
// The pad's box in the picture, with a few pixels to spare.
constexpr int kCropX = 24, kCropY = 96, kCropW = 500, kCropH = 352;
constexpr int kLineGrow = 2;  // pixels filled in around each line pixel
constexpr int kSeedX = 272, kSeedY = 193;  // the guide button

std::mutex g_mutex;
std::shared_ptr<const FigureImage> g_picture;

bool IsBButton(const uint8_t* p) { return p[3] > 200 && p[0] > 60 && p[0] < 155 && p[1] < 70 && p[2] < 70; }

// The lines' red: pure red anywhere, the darker mixes of it with the pad away from the B button.
bool IsCallout(const uint8_t* p, bool near_b_button) {
  const int r = p[0], g = p[1], b = p[2];
  if (p[3] <= 20) return false;
  if (r > 200 && g < 40 && b < 40) return true;
  return !near_b_button && r > 100 && r - g > 45 && r - b > 45 && std::abs(g - b) < 25;
}

// Grows `mask` by one pixel (4-neighbourhood) `n` times.
void Grow(std::vector<uint8_t>& mask, int w, int h, int n) {
  std::vector<uint8_t> next(mask.size());
  for (int k = 0; k < n; ++k) {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const size_t i = size_t(y) * w + x;
        next[i] = mask[i] || (x > 0 && mask[i - 1]) || (x + 1 < w && mask[i + 1]) || (y > 0 && mask[i - w]) ||
                  (y + 1 < h && mask[i + w]);
      }
    }
    mask.swap(next);
  }
}

std::shared_ptr<FigureImage> Clean(const alchemy::RgbaImage& in) {
  const int w = in.width, h = in.height;
  if (w != kWidth || h != kHeight || in.pixels.size() < size_t(w) * h * 4) return nullptr;
  const uint8_t* src = in.pixels.data();
  std::vector<uint8_t> b_button(size_t(w) * h);
  for (size_t i = 0; i < b_button.size(); ++i) b_button[i] = IsBButton(&src[i * 4]);
  Grow(b_button, w, h, 3);
  std::vector<uint8_t> unknown(size_t(w) * h);
  for (size_t i = 0; i < unknown.size(); ++i) unknown[i] = IsCallout(&src[i * 4], b_button[i] != 0);
  Grow(unknown, w, h, kLineGrow);
  // How far the line pixels go on along their row and their column.
  std::vector<int> row_run(size_t(w) * h), column_run(size_t(w) * h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w;) {
      int end = x;
      while (end < w && unknown[size_t(y) * w + end]) ++end;
      for (int k = x; k < end; ++k) row_run[size_t(y) * w + k] = end - x;
      x = end > x ? end : x + 1;
    }
  }
  for (int x = 0; x < w; ++x) {
    for (int y = 0; y < h;) {
      int end = y;
      while (end < h && unknown[size_t(end) * w + x]) ++end;
      for (int k = y; k < end; ++k) column_run[size_t(k) * w + x] = end - y;
      y = end > y ? end : y + 1;
    }
  }
  std::vector<float> px(in.pixels.begin(), in.pixels.end());
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t i = size_t(y) * w + x;
      if (!unknown[i]) continue;
      // Across the line: up and down in a horizontal one, left and right in a vertical one.
      const bool horizontal = row_run[i] > column_run[i];
      const int dx = horizontal ? 0 : 1, dy = horizontal ? 1 : 0;
      int x0 = x, y0 = y, x1 = x, y1 = y;
      while (x0 >= 0 && y0 >= 0 && unknown[size_t(y0) * w + x0]) x0 -= dx, y0 -= dy;
      while (x1 < w && y1 < h && unknown[size_t(y1) * w + x1]) x1 += dx, y1 += dy;
      const bool has0 = x0 >= 0 && y0 >= 0, has1 = x1 < w && y1 < h;
      if (!has0 && !has1) continue;
      const uint8_t* p0 = &src[(has0 ? size_t(y0) * w + x0 : size_t(y1) * w + x1) * 4];
      const uint8_t* p1 = &src[(has1 ? size_t(y1) * w + x1 : size_t(y0) * w + x0) * 4];
      const float t = float(horizontal ? y - y0 : x - x0) / float(horizontal ? y1 - y0 : x1 - x0);
      for (int c = 0; c < 4; ++c) px[i * 4 + c] = p0[c] * (1.0f - t) + p1[c] * t;
    }
  }
  // The red the DXT blocks mixed into the pad's edges next to a line: grey like the pad.
  for (size_t i = 0; i < b_button.size(); ++i) {
    float* p = &px[i * 4];
    if (!b_button[i] && p[3] > 0 && p[0] - p[1] > 25 && p[0] - p[2] > 25 && std::abs(p[1] - p[2]) < 30) {
      p[0] = (p[1] + p[2]) / 2;
    }
  }
  // Keep the opaque part joined to the guide button, with two pixels of its soft edge.
  std::vector<uint8_t> opaque(size_t(w) * h), keep(size_t(w) * h);
  for (size_t i = 0; i < opaque.size(); ++i) opaque[i] = px[i * 4 + 3] > 100;
  std::vector<std::pair<int, int>> stack = {{kSeedX, kSeedY}};
  while (!stack.empty()) {
    const auto [x, y] = stack.back();
    stack.pop_back();
    if (x < 0 || y < 0 || x >= w || y >= h) continue;
    const size_t i = size_t(y) * w + x;
    if (keep[i] || !opaque[i]) continue;
    keep[i] = 1;
    stack.insert(stack.end(), {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}});
  }
  Grow(keep, w, h, 2);
  auto out = std::make_shared<FigureImage>();
  out->width = kCropW;
  out->height = kCropH;
  out->rgba.resize(size_t(kCropW) * kCropH * 4);
  for (int y = 0; y < kCropH; ++y) {
    for (int x = 0; x < kCropW; ++x) {
      const size_t i = size_t(kCropY + y) * w + kCropX + x;
      uint8_t* o = &out->rgba[(size_t(y) * kCropW + x) * 4];
      for (int c = 0; c < 4; ++c) o[c] = uint8_t(std::clamp(std::lround(px[i * 4 + c]), 0L, 255L));
      if (!keep[i]) o[3] = 0;
    }
  }
  return out;
}

std::shared_ptr<FigureImage> Read(const std::filesystem::path& rom_dir) {
  auto archive = alchemy::Iga8Archive::Open(rom_dir / kArchive);
  if (!archive) return nullptr;
  for (size_t i = 0; i < archive->size(); ++i) {
    if (archive->name(i).find(kTexture) == std::string::npos) continue;
    auto* a = archive.get();
    auto igz = alchemy::Igz6File::Parse(
        [a, i](uint64_t offset, size_t size, uint8_t* out) { return a->Read(i, offset, size, out); },
        archive->length(i));
    if (!igz) return nullptr;
    for (const auto& image : igz->Images()) {
      if (auto rgba = igz->Decode(image)) {
        alchemy::FlipVertically(*rgba);  // stored upside down
        return Clean(*rgba);
      }
    }
    return nullptr;
  }
  return nullptr;
}

}  // namespace

void InstallControllerPicture(const std::filesystem::path& rom_dir) {
  std::thread([rom_dir] {
    auto picture = Read(rom_dir);
    if (!picture) {
      REXLOG_WARN("Controller picture: not found in {}", (rom_dir / kArchive).string());
      return;
    }
    REXLOG_INFO("Controller picture: {}x{} from the Controls screen", picture->width, picture->height);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_picture = std::move(picture);
  }).detach();
}

std::shared_ptr<const FigureImage> ControllerPictureImage(const std::string& key) {
  if (key != kArtKey) return nullptr;
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_picture;
}

skylanders::ControllerPicture ControllerPictureLayout() {
  // In the full picture's pixels (see the top).
  static const skylanders::ControllerSpot kSpots[] = {
      {"guide", 272, 193, 28}, {"back", 216, 196, 12}, {"start", 326, 196, 12}, {"y", 416, 155, 15},
      {"x", 379, 191, 15},     {"b", 455, 192, 15},    {"a", 417, 230, 15},     {"ls", 126, 192, 31},
      {"rs", 342, 278, 30},    {"up", 194, 246, 12},   {"down", 194, 306, 12},  {"left", 164, 276, 12},
      {"right", 224, 276, 12}, {"lb", 128, 113, 14},   {"rb", 420, 112, 14},
  };
  skylanders::ControllerPicture picture;
  picture.art_key = kArtKey;
  picture.width = kCropW;
  picture.height = kCropH;
  for (skylanders::ControllerSpot spot : kSpots) {
    spot.x -= kCropX;
    spot.y -= kCropY;
    picture.spots.push_back(spot);
  }
  return picture;
}

}  // namespace giantsrecomp
