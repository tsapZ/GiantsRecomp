# Roadmap

Giants Recompiled, the second Skylanders game, built the same way as SwapForceRecomp and SSARecomp.

| Phase | State | What |
|---|---|---|
| 0 Repository and scaffold | ✅ | scaffold from SSARecomp, disc extracted, xex pinned |
| 1 Recompile and boot | ✅ | codegen, title screen (jump tables), menus, the first level with a figure |
| 1.1 Portal | ✅ | emulated portal and the portal menu; the menu opening by itself at the game's prompt, the portal check |
| 2 In-game | ⬜ | the first levels playable, saves, figures, intro skip |
| 3 Polish | ⬜ | the game's pictures and texts in the menus (portraits, item names, the upgrade view done; hats open), resolution, ultrawide; high frame rate works (`frame_rate_limit`), more levels to check |

## Now, next, later

| When | What | Who |
|---|---|---|
| **Now** | Playing further: levels, saves, the Giants themselves | me |
| Next | Hats in the details view: names (`HatName(00)` ... in the global scene's texts), icons (`misc/StoreItem_Hat_NNN.bld`, `haticons/sky2/*.png`) and stats (`Hat_Desc_NNN`); the figure's hat number to these still to check with a figure that wears one | me |
| After all games | The Skylanders hub | both |
