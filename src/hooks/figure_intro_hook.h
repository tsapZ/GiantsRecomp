// A shorter Skylander intro on the portal (figure_intro_hook.cpp), applied from the scripts'
// check-value hook (attribute_cheats_hook.cpp).

#pragma once

#include <cstdint>

#include <rex/ppc/context.h>

namespace giantsrecomp::figure_intro {

// The waits of the portal's arrival sequence the option ends.
enum class Wait : uint8_t {
  kNone,
  kDeadline,   // time >= ui data.ui mode timer: true once the stage's time is up
  kAnim,       // ui data.load complete object anim.<time left> > 0: true while the animation plays
  kSoundTime,  // time < ui data.load complete object sound timer: true while the battle cry waits
  kSound,      // ui data.load complete object sound.<time left> > 0: true while it plays
};

// Whether the option is on (figure_intro_seconds above 0).
bool On();
// Which wait the check op `op` is (the caller keeps the answer by op).
Wait Classify(uint8_t* base, uint32_t op);
// Called for each wait as it is checked: once the sequence has run figure_intro_seconds, every
// wait comes out as over.
void Apply(Wait wait, PPCRegister& r3);

}  // namespace giantsrecomp::figure_intro
