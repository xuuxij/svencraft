# Reference projects: what we can reuse

Survey of open-source Xash3D / GoldSrc projects against Svencraft's code (2026-10-07). Licences matter: the engine
is GPLv3-or-later (xash3d-fwgs), the game DLLs are under the Half-Life SDK licence, so GPL code can only go into
the engine and HL-SDK code only into the game. Our forks are current: upstream xash3d-fwgs and hlsdk-portable had
no commits newer than our bases (see UPSTREAM.md).

## Gameplay (game DLLs)

| # | What | Where | For us | Effort | Licence |
|---|---|---|---|---|---|
| 1 | **Opposing Force content**: M249, Deagle (laser), sniper, knife, pipe wrench, shock rifle, spore launcher, displacer, grapple; ally/medic/torch grunts, male assassin, shock trooper, pit drone, gonome, voltigore, shock roach, Otis, zombie Barney/soldier, op4 mortar, black-ops Apache/Osprey, ropes. Weapons client-predicted. | hlsdk-portable branch `opforfixed` (`dlls/gearbox/*`, ~26k lines) | Sven Co-op's arsenal and bestiary. Same base commit as our fork: merges with 3 conflicting files (`cbase.h`, `player.cpp`, `pm_shared.c`). Then: penetration power, recoil profiles and hotbar items for the new guns. Skip `ctf_*`, night vision. | M | HL SDK |
| 2 | **hlfixed**: crowbar/gauss/tripmine/handgrenade fixes, `chargerfix`/`explosionfix` defaults, Barney won't shoot through players, `env_sound` room type saved and resent | hlsdk-portable branch `hlfixed` (7 commits) | Merges cleanly. Check its room-type resend against `sc_acoustics.cpp`. | S | HL SDK |
| 3 | **Monster lag compensation** (our engine's `sv_unlag` rewinds players only) | SevenKewp `dlls/util/lagcomp.cpp` (256 lines) | Wrap `FireBulletsPlayer` (and melee traces) in begin/end; monsters are what co-op players shoot | S | HL SDK |
| 4 | **Stats hooks**: BulletsFired/BulletHit with a shot id (a shotgun blast counts once), PlayerDamaged, MonsterKilled; per-map best/last stats with grades | Half-Life: Decay, hlsdk-portable `decay-pc` (`dlls/decay/decay_gamerules.cpp`) | Design template for "one place for game events" (stats + AI DM stream) | S | HL SDK |
| 5 | **Sven co-op mechanics**: per-player inventory snapshot (section-entry loadout), per-player damage on monsters (score, assists), kill-weapon attribution, antiblock, respawn zones, survival mode, Sven monster keyvalues (`is_player_ally`, `classify`, `displayname`...), status bar with name/health coloured by relationship | SevenKewp (`dlls/player/CBasePlayer.cpp` SaveInventory/LoadInventory, `CBaseMonster::LogPlayerDamage`, `DeathNotice`) | Building blocks for co-op sections and stats; adapt, it is restructured. It has no section reset (ours to write). | S-M each | HL SDK |
| 6 | **Penetration ideas**: range falloff per bullet type, max surfaces per type, max penetrating distance, per-entity penetrable override, penetration depth in damage events | ReGameDLL_CS `FireBullets3` | Our exit trace is better than CS's fixed jump; take the tuning ideas into `sc_ballistics.h` | S | MIT (since 2025) |
| 7 | **Event bus / logging**: ~95 game event types, career tasks, standard HL log lines (stats parsers read them) | ReGameDLL_CS `game_shared/GameEvent.h`, `career_tasks.cpp` | Template for the DM event stream and stats | S | MIT |
| 8 | Sven-only monsters: babygarg, robogrunt, hwgrunt, bodyguard, tor, kingpin, stukabat, sentry, miniturret | SevenKewp `dlls/monster/` | After opfor; depend on its CBaseMonster extensions | M each | HL SDK |
| 9 | Sven weapon tuning numbers (M249 67 ms / 6°, Deagle 220 ms 6° or 500 ms 0.5° lasered, sniper 2 s 0.11° FOV 18) | SevenKewp_data `weapons/*.txt` | Numbers for `sc_gunfeel.h` only | - | none (numbers only) |
| 10 | Test bots | hlsdk-portable `bot10` (botman) | Soak-test co-op; no navmesh; not for the DM | M | informal |

Taken so far: #1 and #2 (merged), #3 (`sc_lagcomp.cpp`), SevenKewp's medkit and revive (`sc_medkit.cpp`, with
its data repo's skill values), its heavy weapons and robot grunts as the references for ours (`sc_hwgrunt.cpp`,
`sc_robogrunt.cpp`, on Half-Life's grunt rather than SevenKewp's grunt base).

## Rendering and effects (engine / client)

Our ref_gl is fixed-function GL with multitexture; ARB shader entry points are loaded and `gl_fbo.c` already has a
screen render target and depth-compare textures.

| # | What | Where | For us | Effort | Licence |
|---|---|---|---|---|---|
| 1 | **Flashlight as a projected spotlight cone** (eye-linear texgen + texture matrix, 1D falloff), later with a depth-map shadow | Ideas: Paranoia 1 `gl_light_dynamic.cpp`; maths: PrimeXT `gl_dlight.cpp` `R_SetupLightProjection`; offsets: HL-RTX `vk_light.c` | Replace the falloff texture in `R_DlightsDraw` (`gl_dynworld.c`, voxel pass too) for spot lights | S-M | re-implement |
| 2 | **Bloom**, later tonemap + auto-exposure (night) | PrimeXT `glsl/postfx/gaussblurmip_fp.glsl`, `bloom_fp.glsl`, `gl_postprocess.cpp` | Before the blit in `GL_PresentScreenTarget`; needs a small GLSL loader | S-M | GPLv3 (engine only) |
| 3 | **Data-driven impact particles** (bouncing stretched sparks, rotating chips, lit smoke; material -> decal/particles/sounds table) | PrimeXT `client/render/gl_rpart.cpp`, `gfx/particles/effects.txt`, `scripts/materials.def` | Would replace the switch in `EV_HLDM_ImpactEffects`; engine-side (GPL) or re-implement the formats in cl_dll | M | GPLv3 |
| 4 | **Map smoke/fire/steam emitters** (`env_particle`) | Spirit of HL, hlsdk-portable `sohl1.2` (`cl_dll/particlesys.cpp` etc.) | Chimney smoke, burning wrecks; drops into cl_dll/dlls; unlit sprites | S | HL SDK |
| 5 | `env_fog`, `env_dlight`/`env_elight`, `lightfader`/`trigger_lightstyle` | `sohl1.2` | Map entities; fog needs our dyn/voxel passes to honour it | S each | HL SDK |
| 6 | Better wall puffs (rise, hug walls, wind) | hlsdk-portable `delta_particles` `ev_hldm.cpp` | Fold into our impact puff | S | HL SDK |
| 7 | Texture lights (`lights.rad`) in our dyn-face lightmaps; texture suffix convention `_norm`, `_gloss`, `_luma`, `_hmap`, `_detail` | HL-RTX `loadRadData`; PrimeXT docs | Lit windows/signs in `Dyn_LightGroup`; adopt names now | S-M | re-implement |
| 8 | Cascaded sun shadow maps; normal/parallax maps | PrimeXT `gl_shadows.cpp`, `shadow_proj.h`, `parallax.h` | The right path for a moving sun and the graphics phase; needs a GLSL world pass | L | GPLv3 |

## Not useful for us (now)

- **PrimeXT as a whole**: its renderer lives in client.dll and replaces the engine's; knows nothing of our dyn/voxel
  worlds; needs its own map tools; GPL bars it from our client DLL. Mine it for shaders and maths.
- **Half-Life-RTX-clone**: an unfinished 2022 Vulkan/ray-tracing snapshot, no licence file; ideas and formats only.
- **ReHLDS**: replaces HLDS; we run Xash3D. **metamod-fwgs**: GPL plugin layer; we own both DLLs, so the AI DM talks
  to ours directly (`sc_state` + a command channel). **hlzbot-***: old, GPL heritage.
- **mainui_cpp, freevgui**: already in our engine build (current); mainui is where the player stats page will go.
- **newbspguy**: not needed while maps are generated by script. (Checking map limits for it found the shadow-caster
  copy wasting half the clipnodes; fixed with `zhlt_noclip`.)
- **decay-pc** beyond the stats pattern; **delta_particles** beyond the puffs (its particle system is SoHL's).
