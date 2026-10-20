#pragma once

// Opens the portal menu when the game asks a player for a Skylander ("Player 1: Please put a
// Skylander on the Portal of Power."), closes it again if the game stops waiting before a figure
// was chosen, and opens the portal check on the game's "too many on the portal" screens. Reads the
// game's portal script ("Portal Control") from guest memory; see portal_prompt_watch.cpp.

namespace rex::memory {
class Memory;
}  // namespace rex::memory

namespace giantsrecomp {

// Starts the watcher thread. Call once, after the runtime is set up.
void StartPortalPromptWatch(rex::memory::Memory* memory);

// Stops and joins it; call before the runtime (and its guest memory) goes away.
void StopPortalPromptWatch();

// True while the game runs its figure sequence (a portal screen is up, or a figure is coming in),
// as of the last poll.
bool PortalPromptActive();

}  // namespace giantsrecomp
