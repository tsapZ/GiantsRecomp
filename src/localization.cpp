#include "localization.h"

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

REXCVAR_DEFINE_STRING(language, "auto", "Game",
                      "Game text/voice language: auto (the Windows display language if it is one "
                      "of the following, else English), english, french, german, spanish, "
                      "italian. A raw Xbox 360 language ID (a number) also works, for languages "
                      "the disc supports that aren't confirmed here yet.");

namespace giantsrecomp {

namespace {

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

// The Windows display language if this disc has it (English, French, Spanish); English for any
// other, so the app's menus and the game speak the same language.
std::string SystemLanguage() {
#ifdef _WIN32
  switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_FRENCH:
      return "french";
    case LANG_SPANISH:
      return "spanish";
    default:
      break;
  }
#endif
  return "english";
}

}  // namespace

void ApplyLanguageSetting() {
  // A direct --user_language / user_language=<n> override (raw, for experimentation) always
  // wins: don't clobber it with whatever `language` defaults to (a real bug this had until now --
  // it unconditionally overwrote user_language, silently discarding a direct override).
  if (rex::cvar::HasNonDefaultValue("user_language")) {
    REXLOG_INFO("Language: user_language already set directly ({}); leaving it alone",
               REXCVAR_GET(user_language));
    return;
  }

  // Xbox 360 XLanguage IDs (include/rex/system/xcontent.h). This disc (the North American one)
  // has English, French and Spanish: checked on its title screen, German and Italian come out
  // English. The other two names stay accepted for a disc that has them.
  static const std::unordered_map<std::string, uint32_t> kNames = {
      {"english", 1}, {"german", 3}, {"french", 4}, {"spanish", 5}, {"italian", 6},
  };

  std::string setting = Lower(REXCVAR_GET(language));
  if (setting == "auto") {
    setting = SystemLanguage();
    REXLOG_INFO("Language: auto, from the Windows display language");
  }
  if (auto it = kNames.find(setting); it != kNames.end()) {
    REXCVAR_SET(user_language, it->second);
    REXLOG_INFO("Language: {} (id {})", setting, it->second);
    return;
  }

  uint32_t numeric_id = 0;
  auto [ptr, ec] = std::from_chars(setting.data(), setting.data() + setting.size(), numeric_id);
  if (ec == std::errc() && ptr == setting.data() + setting.size()) {
    REXCVAR_SET(user_language, numeric_id);
    REXLOG_INFO("Language: raw id {}", numeric_id);
    return;
  }

  REXLOG_WARN("Language: '{}' is not a known name or a number; using English", setting);
  REXCVAR_SET(user_language, 1u);
}

}  // namespace giantsrecomp
