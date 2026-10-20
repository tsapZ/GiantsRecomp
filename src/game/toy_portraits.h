#pragma once

// The game's own pictures for the portal menu: every Skylander's portrait from the collection
// screen and the element symbols, read from the disc's character archives (see
// toy_portraits.cpp).

#include <filesystem>

namespace giantsrecomp {

// Loads the pictures from `character_dir` (rom/character) on a background thread and registers
// the portal menu's figure and art picture providers. Until they are loaded (or if they can't be
// read) the menu draws its own symbols. Call once.
void InstallToyPortraits(const std::filesystem::path& character_dir);

}  // namespace giantsrecomp
