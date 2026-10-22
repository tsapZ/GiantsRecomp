// The game's own fonts for the portal menus, read from the owner's game files at start-up, like
// the pictures (nothing of them is in the repository).
//
// Two igBitmapFonts, each in the scene (level.bld, IGZ version 6) of a permanent archive: the UI's
// display font "hdd:/tfb/content/fonts/contemp" (permanent/global.bld, size 85) for the titles and
// the plain "hdd:/tfb/content/fonts/simple" (permanent/bootstrap.bld, size 45) for the text. The
// layout is the later games' (TrapTeamRecomp src/game/game_fonts.cpp): +0x0C the font's size in
// pixels, +0x48 its igCharMetricsList (the count at +0x08, at +0x14 a pointer to its igCharMetrics
// pointers); an igCharMetrics has +0x08 the u16 code point, +0x0C the advance, +0x10 / +0x14 the
// cell's offset, +0x18 / +0x1C its size, +0x20 / +0x24 its top-left texture coordinates. Here the
// cells are a whole line high (offset 0) and the advance is the cell's width with ordinary side
// bearings (7 % of the size; Trap Team's cells have a padding to take off, these none). The pages,
// "fonts/360/contemp.png" (1024x1024) and "fonts/360/simple.png" (512x512), are DXT5, Xbox 360
// tiled, the glyphs grey with their coverage in the alpha. Read 2026-10-21 from this disc: 177
// glyphs each, German's ä ö ü ß Ä Ö Ü among them, upright.
// Both pages end with a row of the pad's buttons, which the game's texts write as punctuation:
// "<" and "[" A, ">" and "{" B, "]" X, "}" and "`" Y, "&" and "\" the left and right stick,
// "*" Back, "=" Start, "_" "|" "~" "^" the d-pad up, down, left, right (looked at 2026-10-22 on
// both pages, the two fonts alike). Those glyphs are left out, so the menus' own font draws these
// characters: with them every ">" of the settings menu came out as a dot (the coverage of the
// button's round shape).

#include "game_fonts.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string_view>
#include <vector>

#include <rex/logging.h>

#include "game/alchemy/iga8_archive.h"
#include "game/alchemy/igz6_file.h"

namespace giantsrecomp {
namespace {

using skylanders::GameFont;
using skylanders::GameFontGlyph;
using skylanders::GameFontPage;

constexpr char kTitleArchive[] = "global.bld";
constexpr char kTitlePage[] = "fonts/360/contemp.png";
constexpr char kBodyArchive[] = "bootstrap.bld";
constexpr char kBodyPage[] = "fonts/360/simple.png";
constexpr char kScene[] = "level.bld";

constexpr uint32_t kFontSizeField = 0x0C;
constexpr uint32_t kGlyphListField = 0x48;
constexpr uint32_t kListCountField = 0x08;
constexpr uint32_t kListItemsField = 0x14;
constexpr uint32_t kCodepointField = 0x08;
constexpr uint32_t kAdvanceField = 0x0C;
constexpr uint32_t kOffsetXField = 0x10;
constexpr uint32_t kOffsetYField = 0x14;
constexpr uint32_t kWidthField = 0x18;
constexpr uint32_t kHeightField = 0x1C;
constexpr uint32_t kU0Field = 0x20;
constexpr uint32_t kV0Field = 0x24;
// The button symbols' code points (see the top).
constexpr std::string_view kButtonSymbols = "&*<=>[\\]^_`{|}~";

float F32(const alchemy::Igz6File& igz, uint64_t object, uint32_t field) {
  const uint32_t bits = igz.Field(object, field);
  float value;
  std::memcpy(&value, &bits, 4);
  return value;
}

// From a line's top to the baseline: the bottom of the H's ink.
float Ascent(const GameFont& font) {
  for (const GameFontGlyph& g : font.glyphs) {
    if (g.codepoint != 'H') continue;
    const GameFontPage& page = font.pages[g.page];
    for (int row = g.height - 1; row >= 0; --row) {
      for (int col = 0; col < g.width; ++col) {
        if (page.alpha[size_t(g.y + row) * page.width + g.x + col] >= 128) {
          return g.y_offset + row + 1;
        }
      }
    }
  }
  return font.size * 0.8f;
}

std::shared_ptr<const GameFont> ReadFont(const std::filesystem::path& archive_path,
                                         std::string_view page_name) {
  auto archive = alchemy::Iga8Archive::Open(archive_path);
  if (!archive) return nullptr;
  const auto scene = archive->Find(kScene);
  if (!scene) return nullptr;
  auto* a = archive.get();
  const size_t index = *scene;
  auto igz = alchemy::Igz6File::Parse(
      [a, index](uint64_t offset, size_t size, uint8_t* out) {
        return a->Read(index, offset, size, out);
      },
      archive->length(index));
  if (!igz) return nullptr;
  const auto fonts = igz->Objects("igBitmapFont");
  if (fonts.empty()) return nullptr;
  const uint64_t font_object = fonts.front();

  // The page, its coverage the alpha of the decoded DXT5.
  const auto images = igz->Images();
  const auto image = std::find_if(images.begin(), images.end(),
                                  [page_name](const auto& i) { return i.name == page_name; });
  if (image == images.end()) return nullptr;
  const auto rgba = igz->Decode(*image);
  if (!rgba || rgba->width <= 0 || rgba->height <= 0) return nullptr;
  auto font = std::make_shared<GameFont>();
  font->size = F32(*igz, font_object, kFontSizeField);
  GameFontPage& page = font->pages.emplace_back();
  page.width = rgba->width;
  page.height = rgba->height;
  page.alpha.resize(size_t(page.width) * page.height);
  for (size_t i = 0; i < page.alpha.size(); ++i) page.alpha[i] = rgba->pixels[4 * i + 3];

  const auto list = igz->Pointer(igz->Field(font_object, kGlyphListField));
  if (!list) return nullptr;
  const auto items = igz->Pointer(igz->Field(*list, kListItemsField));
  if (!items) return nullptr;
  const uint32_t count = igz->Field(*list, kListCountField);
  for (uint32_t k = 0; k < count && k < 4096; ++k) {
    const auto g = igz->Pointer(igz->Field(*items, 4 * k));
    if (!g) continue;
    GameFontGlyph glyph;
    glyph.codepoint = igz->Field(*g, kCodepointField) >> 16;
    if (glyph.codepoint < 0x80 && kButtonSymbols.find(char(glyph.codepoint)) != std::string_view::npos) {
      continue;
    }
    glyph.advance = F32(*igz, *g, kAdvanceField);
    glyph.x_offset = F32(*igz, *g, kOffsetXField);
    glyph.y_offset = F32(*igz, *g, kOffsetYField);
    glyph.x = int(std::lround(F32(*igz, *g, kU0Field) * page.width));
    glyph.y = int(std::lround(F32(*igz, *g, kV0Field) * page.height));
    glyph.width = std::min(int(std::lround(F32(*igz, *g, kWidthField))), page.width - glyph.x);
    glyph.height = std::min(int(std::lround(F32(*igz, *g, kHeightField))), page.height - glyph.y);
    if (glyph.x < 0 || glyph.y < 0) continue;
    font->glyphs.push_back(glyph);
  }
  if (font->glyphs.empty() || font->size <= 0) return nullptr;
  font->ascent = Ascent(*font);
  return font;
}

}  // namespace

skylanders::GameFonts LoadGameFonts(const std::filesystem::path& rom_dir) {
  const auto start = std::chrono::steady_clock::now();
  skylanders::GameFonts fonts;
  fonts.title = ReadFont(rom_dir / "permanent" / kTitleArchive, kTitlePage);
  fonts.body = ReadFont(rom_dir / "permanent" / kBodyArchive, kBodyPage);
  REXLOG_INFO("Game fonts: title {} glyphs, body {} glyphs, in {} ms",
              fonts.title ? fonts.title->glyphs.size() : 0,
              fonts.body ? fonts.body->glyphs.size() : 0,
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start).count());
  return fonts;
}

}  // namespace giantsrecomp
