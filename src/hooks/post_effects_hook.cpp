// Strength of the game's bloom, as in Swap Force, SuperChargers and Imaginators (their
// post_effects_hook.cpp; Cemu's Wii U packs offer the same).
//
// Giants has Toys for Bob's own bloom (tfb_bloom_extract_PS / tfb_bloom_combine_PS), not the
// later games' CBloomData: an effect object with _bloomRadiusX / _bloomRadiusY / _bloomIntensity /
// _blurAlpha at +44 / +48 / +52 / +56 (reflection names at 0x820D582C). sub_8219C730 draws the
// combine pass: it loads _bloomIntensity (lfs f8,52(r31) at 0x8219C9E4) and stores it three times
// into the colour vector of the pass's constants (+80 / +84 / +88 of its stack, the fourth word
// another value), which the following calls upload. The hook at the first store
// (config/default.toml, 0x8219C9E8) scales f8, so the intensity the shader adds is scaled; the
// object's own field stays as it is.
//
// Found 2026-10-22: the combine shader's handle (global 0x826ECF70, set by sub_822308F0 when it
// loads "tfb_bloom_combine_PS") and its only drawing user, sub_8219C730, then the load of +52 in it.

#include <algorithm>

#include <rex/cvar.h>
#include <rex/ppc/context.h>

REXCVAR_DEFINE_DOUBLE(bloom_strength, 1.0, "Graphics",
                      "Glow around bright areas: 0 = off, 1 = the game's, up to 3");

// 0x8219C9E8: f8 = the bloom intensity, stored into the combine pass's colour next.
void BloomStrengthHook(PPCRegister& f8) {
  const double scale = std::clamp(REXCVAR_GET(bloom_strength), 0.0, 3.0);
  if (scale == 1.0) return;
  f8.f64 = double(float(f8.f64 * scale));
}
