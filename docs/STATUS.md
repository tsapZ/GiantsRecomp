# Status

Updated at the end of every session. Only record what a build actually did.

## Current state (2026-10-20)

| Area | State |
|---|---|
| Repository | scaffolded from SSARecomp: build, app, portal (emulated, USB, hybrid), portal / player / settings menus, input scripts and test tools; none of its game hooks or file readers |
| Game data | disc extracted into `rom/` (988 files), `default.xex` hash pinned |
| SDK | our fork `tsapZ/rexglue-sdk`, branch `skylanders`, shared with SwapForceRecomp and SSARecomp |
| Codegen | not run yet |
| Boot | not tried yet |

## 2026-10-20: session log

- **Own repository** (the owner's call: every game done by us, each playable on its own and later
  through the hub). TheBiemGamer's GiantsRecomp (MIT) is read for reference and credited; nothing
  of it is copied without checking it against this executable.
- **Scaffold:** SSARecomp's tracked files, renamed; its Spyro's Adventure-specific parts left out.
- **Disc:** Alchemy engine like Swap Force, archives in an older IGA version:
  docs/game/giants.md.
