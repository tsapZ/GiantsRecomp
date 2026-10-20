// Measures the game's speed from its own clock (developer tool, game_clock_log).
//
// sub_822F18D8 advances the game clock once per frame (called from sub_8223C4B8, the game update):
// it takes the time since the last frame, scales it by the clock's time scale (+0x0C) and keeps it
// between a maximum (+0x14) and a minimum (+0x10) before it adds it to the clock (+0x18) and stores
// it as this frame's step (+0x1C). The Cemu graphic packs for the Wii U version patch out a clamp
// like that one against a "speed-up above 120 FPS" (cemu_graphic_packs,
// SkylandersGiants/Mods/FPS/patch_dt.asm). Found 2026-10-20 from that hint, among the functions that
// reference the float 120.0 (0x42F00000; here it is where the clock's running time wraps).
//
// In this executable the minimum is 0 (logged once at start), so the clamp never raises a step:
// measured with game_clock_log at 230-240 FPS (resolution scale 1, title and first level), the game
// clock ran exactly as fast as real time (speed 1.000 in every 5 s window), as at 125-130 FPS in
// play. No fix is needed here; the hook stays to measure the speed precisely.
//
// The midasm hook at 0x822F197C (config/default.toml) runs at the minimum's compare
// (fcmpu cr6,f0,f13: f0 = the step, f13 = the minimum, r31 = the clock) and changes nothing.

#include <atomic>
#include <chrono>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

REXCVAR_DEFINE_BOOL(game_clock_log, false, "Debug",
                    "Log every 5 s how much game time passed against real time (developer tool "
                    "for the game speed at high frame rates).");

namespace {

using Clock = std::chrono::steady_clock;

// game_clock_log: game time and frames since the last report (the game thread only).
double g_game_time = 0.0;
uint32_t g_frames = 0;
Clock::time_point g_since{};

}  // namespace

void GameClockMinStepHook(PPCRegister& f0, PPCRegister& f13, PPCRegister& r31) {
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("Game clock {:08X}: minimum step {:.6f} s", r31.u32, f13.f64);
  }
  if (!REXCVAR_GET(game_clock_log)) return;
  const auto now = Clock::now();
  if (g_since == Clock::time_point{}) g_since = now;
  g_game_time += f0.f64 > f13.f64 ? f0.f64 : f13.f64;
  ++g_frames;
  const double real = std::chrono::duration<double>(now - g_since).count();
  if (real >= 5.0) {
    REXLOG_INFO("Game clock: {:.3f} s of game time in {:.3f} s ({} frames, {:.1f} FPS), "
                "speed {:.3f}",
                g_game_time, real, g_frames, g_frames / real, g_game_time / real);
    g_game_time = 0.0;
    g_frames = 0;
    g_since = now;
  }
}
