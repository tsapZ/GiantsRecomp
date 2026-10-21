#pragma once

// The game's own fonts for the portal menus (src/game/game_fonts.cpp).

#include <filesystem>

#include "portal_rex/game_font.h"

namespace giantsrecomp {

// The game's fonts from its files (rom/permanent/global.bld for the menus' titles,
// rom/permanent/bootstrap.bld for their text); one left empty if it can't be read, so the menus
// keep the Windows font for it.
skylanders::GameFonts LoadGameFonts(const std::filesystem::path& rom_dir);

}  // namespace giantsrecomp
