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

// loc_82193A60: r5 = the scene's height, the screen's but at most 600.
void SceneHeightHook(PPCRegister& r5) {
  const Size& size = Configured();
  if (!size.width || r5.u32 != kGameHeight) return;
  r5.u64 = size.height;
  static std::atomic<int> logged{0};
  if (logged.fetch_add(1) < 4) REXLOG_INFO("Scene size: the scene's render target 1000x600 -> {}x{}", size.width, size.height);
}
