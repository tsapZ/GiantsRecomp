#include "localization.h"
#include "portal/system_locale.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/logging.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Defined in the SDK (xam_user.cpp); both XGetLanguage and ExGetXConfigSetting read this.
REXCVAR_DECLARE(uint32_t, user_language);
// The console's country (the SDK's xam_locale.cpp), XGetCountry and the XConfig setting.
REXCVAR_DECLARE(uint32_t, user_country);

REXCVAR_DEFINE_STRING(language, "auto", "Game",
                      "Game text/voice language: auto (the Windows display language if it is one "
                      "of the following, else English), english, french, german, spanish (Spain), "
                      "spanish_latam (Latin America), italian, portuguese, dutch, swedish, norwegian, "
                      "danish, finnish. A raw Xbox 360 language ID (a number) also works, for experiments.");

namespace giantsrecomp {

namespace {

bool g_latin_american_spanish = false;
bool g_country_language = false;

// Dutch and the Nordic four aren't Xbox 360 language ids: as Spyro's Adventure (its localization.cpp, SSA #47),
// Giants takes them from English and the console's country, and only in its European language set (the region
// hook's mask: Danish, Dutch, Finnish, Norwegian, Swedish); every disc carries all twelve language packs
// (global.bld, the character archives). Core of Light #118. The countries are XC_COUNTRY ids.
const std::unordered_map<std::string, uint32_t>& CountryLanguages() {
  static const std::unordered_map<std::string, uint32_t> kLanguages = {
      {"dutch", 74u},      // XC_COUNTRY_NL
      {"swedish", 90u},    // XC_COUNTRY_SE
      {"norwegian", 75u},  // XC_COUNTRY_NO
      {"danish", 25u},     // XC_COUNTRY_DK
      {"finnish", 32u},    // XC_COUNTRY_FI
  };
  return kLanguages;
}

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// The Windows display language as one of the names below; English for any other.
std::string SystemLanguage() {
#ifdef _WIN32
  switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_GERMAN:
      return "german";
    case LANG_FRENCH:
      return "french";
    case LANG_SPANISH: {
      // Spain's Spanish for a Spain Windows, the Latin American set for any other Spanish one.
      const auto sub = SUBLANGID(GetUserDefaultUILanguage());
      return sub == SUBLANG_SPANISH || sub == SUBLANG_SPANISH_MODERN ? "spanish" : "spanish_latam";
    }
    case LANG_ITALIAN:
      return "italian";
    case LANG_PORTUGUESE:
      return "portuguese";
    case LANG_DUTCH:
      return "dutch";
    case LANG_SWEDISH:
      return "swedish";
    case LANG_NORWEGIAN:
      return "norwegian";
    case LANG_DANISH:
      return "danish";
    case LANG_FINNISH:
      return "finnish";
    default:
      break;
  }
  return "english";
#else
  const std::string lang = skylanders::PosixSystemLanguage(true);
  if (lang == "german" || lang == "french" || lang == "spanish" || lang == "spanish_latam" ||
      lang == "italian" || lang == "portuguese" || CountryLanguages().count(lang) != 0) {
    return lang;
  }
  return "english";
#endif
}

}  // namespace

void ApplyLanguageSetting() {
  // A direct --user_language / user_language=<n> override (raw, for experimentation) always
  // wins: don't clobber it with whatever `language` defaults to (a real bug this had until now --
  // it unconditionally overwrote user_language, silently discarding a direct override).
  if (rex::cvar::HasNonDefaultValue("user_language")) {
    g_latin_american_spanish = REXCVAR_GET(user_language) == 5;
    REXLOG_INFO("Language: user_language already set directly ({}); leaving it alone",
               REXCVAR_GET(user_language));
    return;
  }

  // Xbox 360 XLanguage IDs (include/rex/system/xcontent.h). The disc (the North American one)
  // has the text and voices of all five; German and Italian only with the region hook
  // (src/hooks/language_region_hook.cpp), which checked all five on the title screen and German
  // in the first level. Spanish comes twice: Spain's (SPANISH.pak, spanish/vo_*) in the European
  // set, which the region hook picks for "spanish", and the Latin American one (HISPANIC.pak,
  // hispanic/vo_*) in the North American set for "spanish_latam" (GiantsRecomp #31).
  static const std::unordered_map<std::string, uint32_t> kNames = {
      {"english", 1}, {"german", 3}, {"french", 4}, {"spanish", 5}, {"spanish_latam", 5}, {"italian", 6},
      // Portuguese (id 9): on the disc too, "Nível" in the HUD (2026-11-11, Core of Light #75).
      {"portuguese", 9},
  };

  std::string setting = Lower(REXCVAR_GET(language));
  if (setting == "auto") {
    setting = SystemLanguage();
    REXLOG_INFO("Language: auto, from the Windows display language");
  }
  if (auto it = kNames.find(setting); it != kNames.end()) {
    g_latin_american_spanish = setting == "spanish_latam";
    REXCVAR_SET(user_language, it->second);
    REXLOG_INFO("Language: {} (id {})", setting, it->second);
    return;
  }
  // Dutch and the Nordic four: English with their country, in the European set (the region hook asks
  // CountryLanguage()). A country set directly (--user_country) stays.
  if (auto it = CountryLanguages().find(setting); it != CountryLanguages().end()) {
    g_country_language = true;
    REXCVAR_SET(user_language, 1u);
    if (!rex::cvar::HasNonDefaultValue("user_country")) {
      REXCVAR_SET(user_country, it->second);
    }
    REXLOG_INFO("Language: {} (English with the country {})", setting, REXCVAR_GET(user_country));
    return;
  }

  uint32_t numeric_id = 0;
  auto [ptr, ec] = std::from_chars(setting.data(), setting.data() + setting.size(), numeric_id);
  if (ec == std::errc() && ptr == setting.data() + setting.size()) {
    g_latin_american_spanish = numeric_id == 5;
    REXCVAR_SET(user_language, numeric_id);
    REXLOG_INFO("Language: raw id {}", numeric_id);
    return;
  }

  REXLOG_WARN("Language: '{}' is not a known name or a number; using English", setting);
  REXCVAR_SET(user_language, 1u);
}

bool LatinAmericanSpanish() { return g_latin_american_spanish; }

bool CountryLanguage() { return g_country_language; }

}  // namespace giantsrecomp
