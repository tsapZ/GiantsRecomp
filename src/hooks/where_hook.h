// Where the player is, for problem reports: the level and player 1's position (where_hook.cpp).

#pragma once

#include <filesystem>

namespace giantsrecomp {

// Names the level after the level file the game opened last (skylanders-portal dev/where.h),
// spelled as in <game data>\level.
void InstallWhere(const std::filesystem::path& game_data_root);

}  // namespace giantsrecomp
