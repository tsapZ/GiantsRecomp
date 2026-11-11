// A shorter Skylander intro when a figure comes onto the portal (Core of Light #65, option, off by
// default; Swap Force, Trap Team, SuperChargers and Imaginators have it as figure_intro_seconds too).
//
// Giants' portal UI (its script, the macros "Portal UI Mode - N" and "UI Update - N") loads a new
// figure in mode 3 ("Load Tag", the real loading) and then shows it in mode 4: "UI Update - 4.
// Loading Tag Complete Seq" runs every frame, a row of states (ui data.state) gated by
//
//   check [<the game's time>] >= ui data.ui mode timer     (each state sets the next deadline)
//
// with waits for the figure's animation on the portal (ui data.load complete object anim, its time
// left > 0), for the battle cry's start (time < ui data.load complete object sound timer) and for
// the battle cry itself (ui data.load complete object sound, its time left > 0). Its last state sets
// ui data.ui mode complete, and the portal control's "Magic Moment - 4 - Condition - Show Minion"
// moves on to the jump into the level ("UI Update - 4. Loading Tag Complete Leap Off").
//
// With figure_intro_seconds set, the hook makes those waits come out as over once the sequence has
// run that long: the remaining states run within a few frames and the game's own leap off follows.
// Loading and the jump keep their time.
//
// The waits are told apart by their sides (the operator, the right side the path "ui data  " (two
// spaces in the script) with "ui mode timer" or "load complete object sound timer", or the left
// side "ui data  " with "load complete object anim" or "load complete object sound" against 0) and
// by the macro whose op list holds them: "ui data  " is a parameter of every "UI Update" macro, so
// the list entry that points at the op is searched within 2 MB of it and followed back to the
// macro's definition (types 820DFDC8 and 820DFE40, the name at +8), as in Trap Team. That walk is
// done once per op that passes the side tests and its answer kept here, so the caller's
// direct-mapped table can drop an op without the walk running again. One op can be in several
// lists: the animation check is shared with the ambient mode and the leap off. So the sequence's
// own deadline check (the first op of its update, every frame it runs) times it, and a shared wait
// is ended only in a frame the sequence runs in.
//
// The layout is the script file's (IGZ version 6): a check's left path +0x1C, operator +0x20 (0 <=,
// 1 ==, 2 >=, 3 <, 4 >, 5 !=), right side +0x24 (an expression 820DD808 whose +8 is a value stack:
// count +8, items +0x14, a literal +0x20 when it has no items); a macro parameter's or member's
// name +0x1C. Found 2026-11-11 by decoding the portal UI script in a Level_214 heap snapshot.

#include "figure_intro_hook.h"

#include <chrono>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>

#include "script_objects.h"

REXCVAR_DEFINE_DOUBLE(figure_intro_seconds, 0.0, "Game",
                      "How long a Skylander's intro may last when it comes onto the portal, in "
                      "seconds (0: as long as the game's)");

namespace giantsrecomp::figure_intro {
namespace {

using script::NameIs;
using script::Readable;
using script::StackItem;

constexpr uint32_t kGreaterEqual = 2, kLess = 3, kGreater = 4;
constexpr uint32_t kExpression = 0x820DD808;
constexpr uint32_t kMacroDef = 0x820DFDC8;
constexpr uint32_t kFlowMacroDef = 0x820DFE40;
constexpr uint32_t kPage = 0x1000;
constexpr uint32_t kSearchSpan = 0x200000;
constexpr uint32_t kMaxOpsBack = 4096;
constexpr char kUiData[] = "ui data  ";
constexpr char kSequence[] = "UI Update - 4. Loading Tag Complete Seq";

// Guest words read with one readability check per 4 KB page (a walk reads many words on few pages,
// and each check is two system calls).
class PageReader {
 public:
  explicit PageReader(uint8_t* base) : base_(base) {}

  bool Read(uint32_t address, uint32_t& out) {
    if (address & 3) return false;
    const uint32_t page = address & ~(kPage - 1);
    if (!readable_.count(page)) {
      if (!Readable(base_, page, kPage)) return false;
      readable_.insert(page);
    }
    out = rex::memory::load_and_swap<uint32_t>(base_ + address);
    return true;
  }

 private:
  uint8_t* base_;
  std::unordered_set<uint32_t> readable_;
};

// The macro definition whose op list holds the entry at `entry` (the entries before it are op
// pointers back to the definition), or 0.
uint32_t MacroOfListEntry(PageReader& reader, uint32_t entry) {
  for (uint32_t back = 0; back < kMaxOpsBack && entry >= 4 * back; ++back) {
    uint32_t item = 0, type = 0;
    if (!reader.Read(entry - 4 * back, item) || !reader.Read(item, type)) return 0;
    if (type == kMacroDef || type == kFlowMacroDef) return item;
    if (type < 0x820D0000 || type >= 0x820F0000) return 0;  // not an op (the ops' vtables are there)
  }
  return 0;
}

// Which op lists hold `op`: the arrival sequence's, others. One op can sit in several macros' lists
// (the script compiler shares an op that is the same in each: the animation check is in "UI Update
// - 2. Ambient", the sequence and its leap off alike, seen 2026-11-11).
struct Listed {
  bool sequence = false;
  bool other = false;
};

Listed ListedIn(uint8_t* base, uint32_t op) {
  Listed listed;
  PageReader reader(base);
  uint8_t want[4];
  rex::memory::store_and_swap<uint32_t>(want, op);
  const uint32_t from = (op > kSearchSpan ? op - kSearchSpan : 0) & ~(kPage - 1);
  const uint32_t to = op < 0xFFFFFFFFu - kSearchSpan ? op + kSearchSpan : 0xFFFFF000u;
  for (uint32_t page = from; page < to; page += kPage) {
    const uint8_t* p = Readable(base, page, kPage);
    if (!p) continue;
    for (uint32_t at = 0; at < kPage; at += 4) {
      if (std::memcmp(p + at, want, 4) != 0) continue;
      const uint32_t macro = MacroOfListEntry(reader, page + at);
      if (!macro) continue;
      (NameIs(base, macro + 8, kSequence) ? listed.sequence : listed.other) = true;
    }
  }
  return listed;
}

// The wait the op's sides make it, before the macro is known. Every check op of every script comes
// here once while the option is on, so the cheap tests come first and read only what the game has
// just used to compare (the op, its right side's expression and value stack with its literal, its
// left path's stack): no system calls for most checks; only the names are read carefully.
Wait BySides(uint8_t* base, uint32_t op) {
  using rex::memory::load_and_swap;
  const uint32_t oper = load_and_swap<uint32_t>(base + op + 0x20);
  if (oper != kGreaterEqual && oper != kLess && oper != kGreater) return Wait::kNone;
  const uint32_t rhs = load_and_swap<uint32_t>(base + op + 0x24);
  const uint32_t left_stack = load_and_swap<uint32_t>(base + op + 0x1C);
  if (!rhs || !left_stack || load_and_swap<uint32_t>(base + rhs) != kExpression) return Wait::kNone;
  const uint32_t stack = load_and_swap<uint32_t>(base + rhs + 8);
  if (!stack) return Wait::kNone;
  const uint32_t right_count = load_and_swap<uint32_t>(base + stack + 8);
  const uint32_t left_count = load_and_swap<uint32_t>(base + left_stack + 8);
  if (oper != kGreater && left_count == 1 && right_count == 2) {
    // time >= ui data.ui mode timer, time < ui data.load complete object sound timer
    const uint32_t data = StackItem(base, rhs + 8, 0), member = StackItem(base, rhs + 8, 1);
    if (!data || !member || !NameIs(base, data + 0x1C, kUiData)) return Wait::kNone;
    if (oper == kGreaterEqual && NameIs(base, member + 0x1C, "ui mode timer")) return Wait::kDeadline;
    if (oper == kLess && NameIs(base, member + 0x1C, "load complete object sound timer")) return Wait::kSoundTime;
    return Wait::kNone;
  }
  if (oper == kGreater && left_count == 3 && right_count == 0 && load_and_swap<uint32_t>(base + stack + 0x20) == 0) {
    // ui data.load complete object anim.<time left> > 0, the same for the sound
    const uint32_t data = StackItem(base, op + 0x1C, 0), member = StackItem(base, op + 0x1C, 1);
    if (!data || !member || !NameIs(base, data + 0x1C, kUiData)) return Wait::kNone;
    if (NameIs(base, member + 0x1C, "load complete object anim")) return Wait::kAnim;
    if (NameIs(base, member + 0x1C, "load complete object sound")) return Wait::kSound;
  }
  return Wait::kNone;
}

}  // namespace

bool On() { return REXCVAR_GET(figure_intro_seconds) > 0.0; }

Wait Classify(uint8_t* base, uint32_t op) {
  const Wait wait = BySides(base, op);
  if (wait == Wait::kNone) return Wait::kNone;
  // The macro walk's answer by op and sides (another level's script can reuse the address).
  using rex::memory::load_and_swap;
  const uint64_t sides =
      (uint64_t(load_and_swap<uint32_t>(base + op + 0x1C)) << 32) | load_and_swap<uint32_t>(base + op + 0x24);
  struct Known {
    uint64_t sides;
    Listed listed;
  };
  thread_local std::unordered_map<uint32_t, Known> known;
  auto it = known.find(op);
  if (it == known.end() || it->second.sides != sides) {
    if (known.size() > 4096) known.clear();
    it = known.insert_or_assign(op, Known{sides, ListedIn(base, op)}).first;
  }
  const Listed& listed = it->second.listed;
  if (!listed.sequence) return Wait::kNone;
  // The deadline check at the top of the sequence (op 6) runs every frame the sequence runs: it
  // times the sequence, so it has to be the sequence's own. The other waits may be shared: Apply
  // ends them only in a frame the sequence runs in.
  if (wait == Wait::kDeadline && listed.other) return Wait::kNone;
  return wait;
}

void Apply(Wait wait, PPCRegister& r3) {
  using Clock = std::chrono::steady_clock;
  thread_local Clock::time_point start, beat;
  thread_local bool beating = false;
  const auto now = Clock::now();
  if (wait == Wait::kDeadline) {
    if (!beating || now - beat > std::chrono::seconds(1)) start = now;  // a new arrival sequence
    beating = true;
    beat = now;
  } else if (!beating || now - beat > std::chrono::milliseconds(100)) {
    return;  // a shared wait outside the sequence (the ambient mode's animation check)
  }
  const double limit = REXCVAR_GET(figure_intro_seconds);
  if (limit <= 0.0 || std::chrono::duration<double>(now - start).count() < limit) return;
  // The deadline is over when the check is true; the other three wait while theirs is true.
  const bool over = wait == Wait::kDeadline ? r3.s32 >= 1 : r3.s32 < 1;
  if (over) return;
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXLOG_INFO("Skylander intro: ends after {:.1f} s (figure_intro_seconds)", limit);
  }
  r3.u64 = wait == Wait::kDeadline ? 1 : 0;
}

}  // namespace giantsrecomp::figure_intro
