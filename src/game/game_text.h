#pragma once

// The game's texts in its language: reading a scene's texts, and the names of magic items and
// adventure packs for the portal menu from the disc's item archives (see game_text.cpp).

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace giantsrecomp {

namespace alchemy {
class Iga8Archive;
}  // namespace alchemy

// The language pack the game runs with ("GERMAN.pak"; see game_text.cpp).
std::string LanguagePackName();

// The texts of an archive's scene in language pack `pack`: (key, text) pairs in the scene's
// order, the texts in UTF-8; nullopt without a scene or that pack.
std::optional<std::vector<std::pair<std::string, std::string>>> ReadSceneTexts(
    alchemy::Iga8Archive& archive, const std::string& pack);

// Reads the names from `rom_dir`/item on a background thread, in the language the game runs in,
// and registers the portal menu's figure name provider. Call once, after the language is applied.
void InstallGameText(const std::filesystem::path& rom_dir);

}  // namespace giantsrecomp
