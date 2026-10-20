// Lets the game use the German and Italian text that is on this disc.
//
// The disc is the North American one, but global.bld and every character archive carry all twelve
// language packs (ENGLISH.pak, GERMAN.pak, ITALIAN.pak, ..., about 90 KB of text each). The game
// picks from them by console region: sub_8217F2D8 (the tfbXenonApplication setup) calls
// XGetGameRegion, and for 0x201 or 0x2FE-0x2FF (the PAL regions) it sets its locale (+52) to 1
// and allows the European languages (mask 0x006243F0 over its language enum at +56: Danish, Dutch,
// English, Finnish, French, German, Italian, Norwegian, Spanish, Swedish), else the North American
// ones (mask 0x00080141: English, French, Latin American Spanish, Portuguese). A language outside
// the set becomes English (6). The SDK's XGetGameRegion returns 0xFFFF, so German came out
// English. The midasm hook at 0x8217F350 (config/default.toml), the compare of that call's result
// in r3, reports the European region 0x2FE when the chosen language is only in the European set,
// and leaves the result alone otherwise, so Spanish keeps this disc's Latin American text. Only
// this call site sees the changed value. Found in the generated code (2026-10-20).

#include <cstdint>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

REXCVAR_DECLARE(uint32_t, user_language);

namespace {

// XLanguage ids (include/rex/system/xcontent.h) that only the European language set has.
constexpr uint32_t kGerman = 3;
constexpr uint32_t kItalian = 6;
constexpr uint32_t kEuropeRegion = 0x2FE;

}  // namespace

void GameRegionForLanguage(PPCRegister& r3) {
  const uint32_t language = REXCVAR_GET(user_language);
  if (language != kGerman && language != kItalian) return;
  REXLOG_INFO("Language: reporting region {:X} instead of {:X}, for the European language set",
              kEuropeRegion, r3.u32);
  r3.u64 = kEuropeRegion;
}
