#pragma once

// The game's own pictures for the portal menu: the collection screen's portraits of every
// Skylander, sidekick and magic item, and the element symbols, read from the disc's archives (see
// toy_portraits.cpp).

#include <filesystem>

namespace giantsrecomp {

// Loads the pictures from the game's files in `rom_dir` on a background thread and registers the
// portal menu's figure and art picture providers. Until they are loaded (or if they can't be read)
// the menu draws its own symbols. Call once.
void InstallToyPortraits(const std::filesystem::path& rom_dir);

}  // namespace giantsrecomp
