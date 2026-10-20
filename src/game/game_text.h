#pragma once

// The game's names for magic items and adventure packs in its language, for the portal menu, read
// from the disc's item archives (see game_text.cpp).

#include <filesystem>

namespace giantsrecomp {

// Reads the names from `rom_dir`/item on a background thread, in the language the game runs in,
// and registers the portal menu's figure name provider. Call once, after the language is applied.
void InstallGameText(const std::filesystem::path& rom_dir);

}  // namespace giantsrecomp
