#pragma once

// The game's controller picture for the settings menu's controls page (controller_picture.cpp).

#include <filesystem>
#include <memory>
#include <string>

#include "portal_rex/portal_menu.h"
#include "portal_rex/settings_menu.h"

namespace giantsrecomp {

// Reads the picture from the disc in the background (see the .cpp). `rom_dir` is the game's
// folder (rom/).
void InstallControllerPicture(const std::filesystem::path& rom_dir);

// The cleaned picture for the menu's art provider ("controller:xbox360"), nullptr for other keys,
// while loading or if the disc doesn't have it.
std::shared_ptr<const skylanders::FigureImage> ControllerPictureImage(const std::string& key);

// The picture's size and the buttons on it, for skylanders::SetControllerPicture.
skylanders::ControllerPicture ControllerPictureLayout();

}  // namespace giantsrecomp
