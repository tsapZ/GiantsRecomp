// Ultrawide: a wider field of view for the game's 3D camera when the window is wider than 16:9.
//
// SwapForceRecomp's hook (its src/hooks/ultrawide_hook.cpp has how the camera was found there):
// every frame the renderer sets up the scene camera, stores the display's aspect ratio (16:9) and
// the field of view, and builds the projection with the perspective builder (r3 = matrix, f1 =
// horizontal FOV, f2 = vertical FOV, f3 = aspect, f4 = near, f5 = far; radians; the horizontal FOV
// when f1 >= 0). The guest frame stays 16:9; with present_letterbox off (the app turns it off while
// this is active, unless set by hand) the presenter stretches it across the wider window, so the
// camera has to see that much more to the sides for the picture to keep its proportions ("Hor+"):
// the hook at the call widens the aspect and, if the call gives a horizontal FOV, that one too,
// keeping the vertical FOV. The HUD is drawn into the same 16:9 frame and is stretched.
//
// Found 2026-10-21: the builder sub_822D0240 in a dump of this game's code by Swap Force's
// (42 of its first 48 instructions the same); the calls with a probe, a hook at each of the
// builder's callers logging its arguments in play: all four run with the display's 16:9 and a
// horizontal FOV that follows the camera (the vertical one passed as -1 degree, unset):
// 0x82198530 every frame with the camera's FOV, 0x82198580 every frame with 40 degrees more (a
// wider frustum, presumably for culling), 0x82199988 and 0x821999B0 every third frame with the
// camera's FOV. All four are widened alike, so nothing the wider view shows is culled.
//
// The HUD (ultrawide_hud): the UI is laid out on a screen object 1280 x 720 (+180 width, +184
// height, floats), and the UI camera's set-up (sub_821910A8) takes its size from there into the
// camera (+112 / +116) and builds its orthographic projection with it (0 .. width; bl
// sub_822D0100 at 0x82191188). The idea, from GiantsRecomp by TheBiemGamer (its
// src/hooks/ultrawide_hook.cpp sets that width in its ortho hook; read, not copied): give the
// screen object the window's width at the UI's height, 720 x the window's aspect. The game then
// lays its UI out for a wider screen, with what it anchors to the right edge at the new right
// edge, and the projection squeezes those 1707 units into the 16:9 frame, which the presenter
// stretches back: the HUD keeps its proportions and sits at the window's edges. The hook, before
// the set-up reads the size (lfs f0,184(r11) at 0x821910E8, r11 = the screen object), writes that
// width and puts the game's own back when ultrawide is off.

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#include "ultrawide_hook.h"

#include <rex/cvar.h>
#include <rex/graphics/video_mode_util.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DECLARE(std::string, resolution);

REXCVAR_DEFINE_BOOL(ultrawide_fov, true, "Graphics",
                    "On a window wider than 16:9 (resolution), show more to the sides instead of "
                    "black bars: a wider field of view and the picture stretched across the "
                    "window");
REXCVAR_DEFINE_BOOL(ultrawide_hud, true, "Graphics",
                    "With ultrawide_fov on a wider window, the HUD and the menus at the window's "
                    "edges in their own proportions instead of stretched");

namespace {

constexpr double kGameAspect = 16.0 / 9.0;

// The window's aspect ratio from `resolution`, or 0 if it isn't set.
double WindowAspect() {
  int32_t width = 0, height = 0;
  if (!rex::graphics::video_mode_util::TryParseResolutionPreset(REXCVAR_GET(resolution), width,
                                                                height) ||
      width <= 0 || height <= 0) {
    return 0;
  }
  return double(width) / double(height);
}

}  // namespace

namespace giantsrecomp {

// Whether the picture should fill a wider window (the presenter then must not letterbox).
bool UltrawideActive() {
  return REXCVAR_GET(ultrawide_fov) && WindowAspect() > kGameAspect * 1.01;
}

}  // namespace giantsrecomp

// At 0x82198530, 0x82198580, 0x82199988 and 0x821999B0 (bl sub_822D0240): f1 = horizontal FOV
// (radians; negative when the vertical one is given), f2 = vertical FOV, f3 = aspect.
void UltrawideFovHook(PPCRegister& f1, PPCRegister& f2, PPCRegister& f3) {
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("Ultrawide: camera FOV h {} v {}, aspect {}", f1.f64, f2.f64, f3.f64);
  }
  if (!giantsrecomp::UltrawideActive() || f3.f64 <= 0) return;
  const double wide = WindowAspect();
  if (wide <= f3.f64) return;
  const double scale = wide / f3.f64;
  if (f1.f64 >= 0) {
    // Same vertical FOV: tan(h'/2) = tan(h/2) * scale.
    f1.f64 = double(float(2.0 * std::atan(std::tan(f1.f64 * 0.5) * scale)));
  }
  f3.f64 = double(float(wide));
}

// At 0x821910E8 (lfs f0,184(r11) in sub_821910A8): r11 = the UI's screen object, +180 its width,
// +184 its height.
void UltrawideHudHook(PPCRegister& r11) {
  if (!r11.u32) return;
  uint8_t* screen = rex::system::kernel_state()->memory()->virtual_membase() + r11.u32;
  auto load = [](const uint8_t* p) {
    const uint32_t bits = rex::memory::load_and_swap<uint32_t>(p);
    float v;
    std::memcpy(&v, &bits, 4);
    return v;
  };
  auto store = [](uint8_t* p, float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    rex::memory::store_and_swap<uint32_t>(p, bits);
  };
  // The game's own width: what the field holds unless it is the one written here.
  static float own_width = 0, written = 0;
  const float width = load(screen + 180), height = load(screen + 184);
  if (width != written || own_width == 0) own_width = width;
  float want = own_width;
  if (REXCVAR_GET(ultrawide_hud) && giantsrecomp::UltrawideActive() && height > 0) {
    want = float(double(height) * WindowAspect());
  }
  if (want == width) return;
  store(screen + 180, want);
  written = want;
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) REXLOG_INFO("Ultrawide: UI screen {} x {} -> {} wide", width, height, want);
}
