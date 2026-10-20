#pragma once

// The hats for the portal menu's details view: the game's name, picture and bonuses of the hat a
// figure wears, read from the disc's archives (see toy_hats.cpp).

#include <filesystem>
#include <memory>
#include <string>

#include "portal_rex/portal_menu.h"

namespace giantsrecomp {

// Reads the hats from `rom_dir` on a background thread and registers the portal menu's hat
// provider. Call once, after the language is applied.
void InstallToyHats(const std::filesystem::path& rom_dir);

// The picture for a "hat:..." art key of the hats above; nullptr while loading or for none.
std::shared_ptr<const skylanders::FigureImage> HatImage(const std::string& key);

}  // namespace giantsrecomp
