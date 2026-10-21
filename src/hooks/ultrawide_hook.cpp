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

#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>

#include "ultrawide_hook.h"

#include <rex/cvar.h>
#include <rex/graphics/video_mode_util.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

REXCVAR_DECLARE(std::string, resolution);

REXCVAR_DEFINE_BOOL(ultrawide_fov, true, "Graphics",
                    "On a window wider than 16:9 (resolution), show more to the sides instead of "
                    "black bars: a wider field of view and the picture stretched across the "
                    "window. The HUD is stretched with it");

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
