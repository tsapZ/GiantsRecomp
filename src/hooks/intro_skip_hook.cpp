// Skips the startup logo movies and, optionally, the opening movie of a new game.
//
// BinkOpen (sub_82153BE8, Bink 2) reads the 44-byte file header into r1+96 and then checks its
// signature (the "KB2e" compare that follows); the midasm hook at 0x82153DB0 (config/default.toml)
// runs in between, on the file and the in-memory header path alike. When the header belongs to one
// of the movies below, recognised by the file-size and frame-count fields that are unique to them,
// the frame count at +8 is set to 1. Bink then plays one frame and reports the movie finished, so
// the game moves on as if it had played out. The address and the logos' header values are
// TheBiemGamer's (GiantsRecomp, MIT, same default.xex), checked against this disc's files and the
// generated code; the same approach as SwapForceRecomp's and SSARecomp's intro skip.

#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

#include "ultrawide_hook.h"

// Off by default (the owner's choice, 2026-10-22): the game starts as on the console, with its
// logo movies; skipping them is the option.
REXCVAR_DEFINE_BOOL(skip_intro, false, "Game",
                    "Skip the Activision and Toys for Bob logo movies at startup.");
REXCVAR_DEFINE_BOOL(skip_opening_movie, false, "Game",
                    "Skip the opening movie when starting a new game.");

namespace {

struct Movie {
  uint32_t size_field;  // header +4: file size minus 8
  uint32_t frames;      // header +8
  bool opening;         // true: controlled by skip_opening_movie instead of skip_intro
  const char* name;
};

// Header values read from the disc's files (all "KB2e").
constexpr Movie kMovies[] = {
    {0x005A25B8, 226, false, "movies/ATVInewlogo_640.mov"},   // Activision logo
    {0x0052AF84, 300, false, "movies/tfb_logo_640x456.mov"},  // Toys for Bob logo
    {0x0DCAA9A0, 7643, true, "movies/FMV01.mov"},             // new-game opening movie
};

}  // namespace

void BinkHeaderLoaded(PPCRegister& r1) {
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  uint8_t* header = base + r1.u32 + 96;
  // Every movie, skipped or not (ultrawide: bars at the sides while it plays).
  giantsrecomp::MovieOpened(rex::memory::load_and_swap<uint32_t>(header + 0x14),
                            rex::memory::load_and_swap<uint32_t>(header + 0x18));
  const bool skip_logos = REXCVAR_GET(skip_intro);
  const bool skip_opening = REXCVAR_GET(skip_opening_movie);
  if (!skip_logos && !skip_opening) {
    return;
  }
  // The file stores these fields little-endian, but Bink's reader has already swapped the header
  // into native (big-endian) words by the time it reaches this point.
  const uint32_t size_field = rex::memory::load_and_swap<uint32_t>(header + 4);
  const uint32_t frames = rex::memory::load_and_swap<uint32_t>(header + 8);
  for (const Movie& movie : kMovies) {
    if (size_field != movie.size_field || frames != movie.frames) {
      continue;
    }
    if (movie.opening ? skip_opening : skip_logos) {
      rex::memory::store_and_swap<uint32_t>(header + 8, 1);
      REXLOG_INFO("Skipping movie {} ({} frames)", movie.name, frames);
    }
    return;
  }
}
