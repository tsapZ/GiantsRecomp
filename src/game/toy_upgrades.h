#pragma once

// Every Skylander's upgrades for the portal menu's details view, as the game's upgrade screen
// shows them: names and descriptions in the game's language, prices and the upgrade pictures,
// read from the disc's character archives (see toy_upgrades.cpp).

#include <filesystem>
#include <memory>
#include <string>

#include "portal_rex/portal_menu.h"

namespace giantsrecomp {

// Reads the upgrade texts from `rom_dir`/character on a background thread and registers the
// portal menu's upgrade provider. Call once, after the language is applied.
void InstallToyUpgrades(const std::filesystem::path& rom_dir);

// The picture for an "upgrade:..." art key of the upgrades above; nullptr while it is still
// loading (it is read on first request) or if there is none. Called on the UI thread.
std::shared_ptr<const skylanders::FigureImage> UpgradeImage(const std::string& key);

}  // namespace giantsrecomp
