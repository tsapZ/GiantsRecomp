// Draws the 3D scene at another size than the Xbox 360's 1000x600 (experimental), as the other
// games' F4 > Grafik > "Natives 3D-Bild" (TrapTeamRecomp and SwapForceRecomp scene_size_hook.cpp).
//
// Unlike the later games, this one has no render pipeline file: sub_821939E0 makes the scene's
// render target in code, at the screen's size but at most 1000x600 (the 360's EDRAM holds a
// 1000x600 scene with 2x MSAA in one piece, which a 1280x720 one wouldn't). Found 2026-10-22: a
// RenderDoc capture in the first level (resolution scale 1) shows the scene resolved into a
// 1000x600 texture before the HUD goes on; the only code taking the screen's size with a
// 1000 / 600 limit is that function:
//   lwz r11,16(r3)   (the screen object at 0x826F42A0's width)   li r29,1000
//   cmplwi cr6,r11,1000 / bgt / mr r29,r11                         -> loc_82193A38: r29 = width
//   lwz r5,20(r3)    (its height)  cmplwi cr6,r5,600 / ble / li r5,600 -> loc_82193A60: r5 = height
// and then passes r29 / r5 as the size to sub_82196150, which makes the target. The hooks at the
// two places put scene_size into r29 and r5 when the game chose its own 1000 / 600 there. The
// same limit is in two small functions the code after the scene asks (sub_8218ED90 the width,
// sub_8218EDD8 the height, e.g. for the texture the scene is resolved into); their results get the
// same treatment.
//
// The EDRAM (GiantsRecomp #30): the game plans its EDRAM as the 360's 2048 tiles. A scene whose
// colour and depth surfaces (2x MSAA, tiles of 80x8 pixels) need more drew black in most places
// outside (the title, the Junkyard Isles): 1000x600, 1024x608 and 1040x608 (1976 tiles) were fine,
// 1120x640, 1264x608 and 1008x704 (2240 to 2432) black (measured 2026-10-25). As in Swap Force, a
// pass above 2048 tiles was cut into predicated-tiling strips (sub_822F9B20) and the depth surface
// put at the top of 2048 tiles (sub_822FA6E8), over the colours. With the SDK's 4096-tile EDRAM the
// two hooks below let a pass up to 4096 tiles go untiled and put the depth at the top of 4096
// (config/default.toml has the details). A scene above 4096 tiles (about 1440x816) keeps the game's
// size.

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>

#include <rex/cvar.h>
#include <rex/graphics/edram_layout.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

REXCVAR_DEFINE_STRING(scene_size, "", "Graphics",
                      "EXPERIMENTAL: draw the 3D scene at this size instead of the Xbox 360's "
                      "1000x600, e.g. 1280x720. Needs the larger emulated EDRAM, which it turns on "
                      "(edram_tile_count 4096, D3D12 and resolution_scale up to 5 only; otherwise "
                      "the game's size stays). Empty: the game's size")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

namespace {

constexpr uint32_t kGameWidth = 1000;
constexpr uint32_t kGameHeight = 600;
constexpr uint32_t kGameEdramTiles = 2048;  // the 360's EDRAM, which the game still plans with
// The game's screen (back buffer and HUD) size, which the scene must not match exactly (in Swap
// Force a scene at exactly the screen's size crashed in about half of its save loads).
constexpr uint32_t kScreenWidth = 1280;
constexpr uint32_t kScreenHeight = 720;
constexpr uint32_t kScreenSizeStep = 16;

struct Size {
  uint32_t width = 0, height = 0;
};

Size ConfiguredSize() {
  Size size;
  const std::string text = REXCVAR_GET(scene_size);
  unsigned w = 0, h = 0;
  if (!text.empty() && std::sscanf(text.c_str(), "%ux%u", &w, &h) == 2 && w >= 320 && h >= 240 && w <= 7680 &&
      h <= 4320) {
    size = {w, h};
    if (size.width == kScreenWidth && size.height == kScreenHeight) {
      size = {kScreenWidth - kScreenSizeStep, kScreenHeight - kScreenSizeStep};
    }
  } else if (!text.empty()) {
    REXLOG_WARN("Scene size: '{}' is not WIDTHxHEIGHT; keeping the game's size", text);
  }
  // Colour and depth at 2x MSAA must fit into the EDRAM the SDK runs (the hooks below move the game's
  // plan to it; see the top of this file).
  const uint32_t edram = std::max(kGameEdramTiles, rex::graphics::ActiveEdramLayout().tile_count);
  if (size.width && 2 * ((size.width + 79) / 80) * ((size.height + 7) / 8) > edram) {
    REXLOG_WARN("Scene size: {}x{} doesn't fit into {} EDRAM tiles; keeping 1000x600", size.width,
                size.height, edram);
    size = {};
  }
  // A larger scene doesn't fit the 360's EDRAM: without the larger one the game keeps its size.
  if (size.width * size.height > kGameWidth * kGameHeight && !rex::graphics::ActiveEdramLayout().is_extended()) {
    REXLOG_WARN("Scene size: {}x{} needs the 4096-tile EDRAM (D3D12, resolution scale up to 5); keeping "
                "1000x600",
                size.width, size.height);
    size = {};
  }
  return size;
}

const Size& Configured() {
  static const Size size = ConfiguredSize();
  return size;
}

}  // namespace

// loc_82193A38: r29 = the scene's width, the screen's but at most 1000.
void SceneWidthHook(PPCRegister& r29) {
  const Size& size = Configured();
  if (!size.width || r29.u32 != kGameWidth) return;
  r29.u64 = size.width;
}

// The end of sub_8218ED90 / sub_8218EDD8: r3 = the scene's width / height they return.
void SceneWidthResultHook(PPCRegister& r3) { SceneWidthHook(r3); }
void SceneHeightResultHook(PPCRegister& r3) {
  const Size& size = Configured();
  if (size.width && r3.u32 == kGameHeight) r3.u64 = size.height;
}

// Before 0x822F9C30 (cmpwi cr6,r5,0x800): r5 is the pass's tile count. Between 2048 and the larger
// EDRAM it becomes 2048, so the comparison takes the "fits" path (r5 isn't read on that path; the
// strip size it stores is then the whole surface).
void EdramTilingHook(PPCRegister& r5) {
  const uint32_t tiles = rex::graphics::ActiveEdramLayout().tile_count;
  if (tiles <= kGameEdramTiles || r5.u32 <= kGameEdramTiles || r5.u32 > tiles) return;
  static std::atomic<int> logged{0};
  if (logged.fetch_add(1) < 4) {
    REXLOG_INFO("Scene size: a pass of {} tiles fits into the {}-tile EDRAM, no tiling", r5.u32, tiles);
  }
  r5.u64 = kGameEdramTiles;
}

// Before 0x822FA8A8 (cmplwi cr6,r10,0xe10): r9 = 2048 - r11 is the depth surface's EDRAM base, r11
// its tile count; stw r9,0x80(r23) stores it next. With a 4096-tile EDRAM in use the depth goes to
// the top of that instead. Only r9 changes, and only when the EDRAM is the larger one.
void EdramDepthBaseHook(PPCRegister& r9, PPCRegister& r11) {
  const uint32_t tiles = rex::graphics::ActiveEdramLayout().tile_count;
  if (tiles <= kGameEdramTiles || r11.u32 > tiles) return;
  r9.u64 = tiles - r11.u32;
  static std::atomic<int> logged{0};
  if (logged.fetch_add(1) < 4) {
    REXLOG_INFO("Scene size: depth surface of {} tiles at EDRAM tile {} of {}", r11.u32, r9.u32, tiles);
  }
}

// loc_82193A60: r5 = the scene's height, the screen's but at most 600.
void SceneHeightHook(PPCRegister& r5) {
  const Size& size = Configured();
  if (!size.width || r5.u32 != kGameHeight) return;
  r5.u64 = size.height;
  static std::atomic<int> logged{0};
  if (logged.fetch_add(1) < 4) REXLOG_INFO("Scene size: the scene's render target 1000x600 -> {}x{}", size.width, size.height);
}
