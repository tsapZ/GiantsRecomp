#pragma once

namespace giantsrecomp {

// Resolves the `language` cvar (a friendly name, or a raw numeric Xbox 360 language ID for
// experimentation) and applies it to the `user_language` cvar that XGetLanguage/
// ExGetXConfigSetting actually read. Call once at startup, before the game can query either.
void ApplyLanguageSetting();

// Whether Spanish means the disc's Latin American set (HISPANIC.pak, hispanic/vo_*) instead of
// Spain's (SPANISH.pak, spanish/vo_*): language = spanish_latam, auto on a Latin American Spanish
// Windows, or a raw user_language 5 (the game's own choice without the region hook). Valid after
// ApplyLanguageSetting.
bool LatinAmericanSpanish();

}  // namespace giantsrecomp
