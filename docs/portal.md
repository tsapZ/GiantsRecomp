# Portal

The Portal of Power comes from the portal library (`thirdparty/skylanders-portal`, our fork's
branch `feature/portal-menu`), the same code as in SwapForceRecomp and SSARecomp. Its protocol, the
figure files and the menus are described in SwapForceRecomp's
[portal.md](https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/portal.md); this page covers
what is particular to Giants.

## Modes

`portal_mode` (F4 > Portal):

| Value | What |
|---|---|
| `software` | the emulated portal, filled from the portal menu (default) |
| `hybrid` | the emulated portal and the real USB portal together |
| `usb` | the real USB portal only |
| `none` | no portal |

The portal menu lists the figures of Spyro's Adventure and Giants (`portal_menu_games`), the ones
this game takes.

## Figures

`Documents\Skylanders\Figures`, one folder for every game (portal fork `SharedFiguresDir`), with a
subfolder per game. Test runs with a copied `--user_data_root` get their own folder next to the
copy.

## The menu at the game's portal screens

Giants has no portal prompt object like Swap Force's `CMagicMoment` or Spyro's Adventure's prompt
static: its portal screens are run by a game script, the "Portal Control" state machine.
`src/game/portal_prompt_watch.cpp` finds two of the script's variables by class and name (a scan of
the physical heap for ScriptReference objects, vtable `0x820DF750`, name at +8, value at +0x1C) and
polls them every 30 ms:

- **"current state name":** the state machine's state, e.g. `Portal Control - In Game - 0 - Idle`
  in play, `Portal Control - Portal - 0 - Idle P1 Only` at player 1's prompt, `... Idle P2 Only`
  at player 2's, `Portal Control - Magic Moment - ...` while a figure comes in.
- **"portal error string":** 0, or the text on screen (a StringInfo with its key at +8):

| Key | Screen | The watch opens |
|---|---|---|
| `203_PortalError_TagTypeNotOnPortal` | Please put a Skylander on the Portal of Power (measured) | the menu for the player of the state |
| `301_PortalError_ToyMissing` | a figure is missing | the menu |
| `102_PortalError_TooManyMinions` | too many Skylanders (measured) | the check for Skylanders |
| `304_..._NeedsRemoval_Minion` | the Skylander needs a break (out of health) | that player's menu, the figure marked tired |
| `103_..._TooManyItems`, `105_..._RemoveItems` | too many magic items | the check for items |
| `101_`, `106_`, `111_`, `112_`, `305_` | other portal errors (locations, figure data) | the check with its general title |

The keys come from the script's strings; what each opens follows their names and is measured only
where marked. The pause menu, the title and the autosave notice open nothing.

## Portal check

Lists everything on the emulated portal, marks what there is too much of, and takes off what the
player picks; B closes it, and while the error stays it comes back after 8 s. The title names what
is doubled: magic items, sidekicks ("Mini-Skylander" in German), or both.
