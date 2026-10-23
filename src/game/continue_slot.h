#pragma once

// Weiterspielen (skylanders-portal continue_game) into the save played last: this game's
// "Speicherplatz wählen" screen always starts on slot 1, so before the continue presses' A the
// cursor goes to the slot whose save was written last. See continue_slot.cpp.

#include <filesystem>

namespace rex::memory {
class Memory;
}  // namespace rex::memory

namespace giantsrecomp {

// With continue_game on and a save played last in slot 2-4: registers the step that moves the
// slot cursor there (skylanders::SetContinueStepProvider). Call once, after the runtime is set up.
void StartContinueSlot(rex::memory::Memory* memory, const std::filesystem::path& user_data_root);

}  // namespace giantsrecomp
