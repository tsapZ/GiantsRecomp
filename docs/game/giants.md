# Skylanders: Giants (Xbox 360)

What we know about the game and the dump this project targets, and what still has to be checked.

## The game

- The second Skylanders game, 2012, published by Activision, developed by Toys for Bob. It reads
  its own figures (Giants, Series 2) and Spyro's Adventure's; later games' figures don't work.

## Target dump

| Property | Value | Source |
|---|---|---|
| ISO size | 8,738,846,720 bytes | measured |
| Disc content | 988 files, 8,354,790,081 bytes | `extract-xiso`, measured |
| `default.xex` size | 7,020,544 bytes, in the disc root | measured |
| `default.xex` SHA-256 | `12d1e29e4f07fbc559bc1a4988dd6bf346c3d16e30cbf862e2683e862f9fc09e` | measured, in `default.xex.sha256` |
| Title ID | `415608DA` (media ID `52FFE94B`) | XEX execution info |
| Version | `0.0.0.2` (base version the same), disc 1 of 1 | XEX execution info |
| Region flags | `0xFFFFFFFF`: region free | XEX security info |
| Languages | English, French, Spanish (the North American set); German and Italian come out English | the title screen under each `language` |

## Differences that matter

- **Alchemy, like Swap Force, not Spyro's Adventure's engine.** The disc has `alchemy.xml` and
  folders `character\`, `item\`, `level\`, `misc\`, `movies\`, `outfit\`, `permanent\` with
  `.arc` / `.bld` files (596 `.bld`, 295 `.arc`), movies as Bink 2 `.mov`. The archives start
  with `IGA\x1a`, version 8 with a little-endian header; Swap Force's `.pak` are version 10 with
  a big-endian one, so SwapForceRecomp's archive reader needs adapting before the portal menu can
  use the game's pictures and names.
- **The portal screens are a game script.** The "Portal Control" state machine (global level
  includes) runs them: states such as `Portal Control - In Game - 0 - Idle`, `Portal Control -
  Portal - 0 - Idle P1 Only` (the prompt), `Portal Control - Magic Moment - 4 - Show Minion P1`
  (a figure coming in), and the variable "portal error string" with the message's text key
  (`203_PortalError_TagTypeNotOnPortal`, `102_PortalError_TooManyMinions`, ...; "minion" is a
  Skylander, "tag" a figure). Script variables are named objects (class ScriptReference, +8
  name, +0x1C value), so they can be found by name. src/game/portal_prompt_watch.cpp has the
  details.
- **Scaffolded from SSARecomp** (the newest app: language "auto", the shared figure folder, the
  portal and settings menus); none of its game hooks or file readers, which are for Spyro's
  Adventure's executable and formats.
