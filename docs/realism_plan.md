# Svencraft: realism features and a Minecraft day/night cycle

Read-only investigation, 2026-10-07. Line numbers are for the current working trees (`game/` and `engine/`, including
the uncommitted Svencraft changes). Nothing here has been built or run. Effort estimates are for one developer who
knows this code.

**Status (2026-10-07, later):** Task A is built and tested in game (A1-A7, sprint excepted): see DESIGN.md
("Gunfire", "Recoil and spread", "Penetration", "Feel"). Differences from this plan: the server counted the aim punch
twice (`v_angle` already holds it after the move, as in GoldSrc), fixed in `CBasePlayer::PostThink`; glass and wood
hits use fine particles, not the gib models; MP5 kick 1.15 so it climbs. Task B (day/night) is not started.

Another session was editing the code during this survey: `dynworld.c`, `gl_voxel.c`, `sc_blast.cpp`, `combat.cpp`,
`make_town.py` and new files such as `sc_acoustics.cpp` changed while it was written. Line numbers were re-checked
at 07:10 and may drift again; the function names are the stable anchors.

## Key findings

These came up during the survey. Some are bugs, some are constraints the plans below have to respect.

1. **The MP5 and shotgun show no muzzle-flash sprite.** Sven Co-op's `v_9mmAR.mdl` and `v_shotgun.mdl` fire studio
   event **5005** with a script name (`Muzzle_mp5.txt`, `muzzle_SG.txt` in `run/svencoop/events/`). The handler
   `HUD_StudioEvent` (`cl_dll/entity.cpp:543-569`) only knows 5001/5011/5021/5031/5002/5004. The Glock and .357 use
   5001 and do show a flash.
2. **Player gunfire never lights the world.** For studio models `EF_MUZZLEFLASH` only makes an *entity* light of
   radius 24 (`ref/gl/gl_studio.c:2449-2461`). The world dlight path exists only for alias models
   (`engine/client/cl_tent.c:2745-2758`). The per-pixel dlight passes on the diggable world and the blocks
   (`gl_dynworld.c:699-708`, `gl_voxel.c:1070-1102`) never get a gunshot light.
3. **Recoil doesn't move the aim.** Every gun kick is `V_PunchAxis` on the client-only `g_ev_punchangle`
   (`ev_hldm.cpp:494, 556, 611, 668, 704, 752, 867, 1289, 1329, 1569`). That angle is added to the view
   (`view.cpp:632`) but never to the aim. Spread is a constant per gun, and moving or jumping costs no accuracy.
4. **`UTIL_ScreenShake` has no distance falloff and only shakes grounded players** (`dlls/util.cpp:671`, `684-685`).
   Half-Life grenades and rockets (`CGrenade::Explode`, `ggrenade.cpp:55-140`) don't shake at all. The creeper
   (`sc_creeper.cpp:585`), car (`sc_car.cpp:238`) and barrel (`sc_car.cpp:342`) do.
5. **Bug in `PM_CheckFalling`.** It sets the *roll* punch (`pmove->punchangle[2]`) and then clamps the *pitch*
   (`pm_shared.c:2830-2835`). The roll is unclamped: a 580 u/s landing rolls the view 7.5 degrees.
6. **Bug in `Dyn_ParseLights`: the diggable world's sun is 3 degrees off the BSP's.** The compiled BSP's
   `light_environment` has `"angles" "-48 210 0"` and no `"pitch"` key (hlcsg folded it in; checked by reading the
   BSP entity lump). `Dyn_ParseLights` (`dynworld.c:328-338`) reads only `pitch` (default -45) and the *yaw* of
   `angles`. So the diggable world is lit at -45 degrees while the BSP brush entities were baked at -48. Fix: when
   `pitch` is missing, use `angles[0]`.
7. **The diggable and block worlds aren't networked.** `Dyn_LoadForMap` runs only in `sv_init.c:1041`, and the
   renderer reads that same in-process state through `ref_api` (`ref_common.c:454-458`). Anything client-side that
   asks those worlds (traces, light) is correct only on the listen-server host until world sync exists. This
   affects multiplayer risk for every feature below.
8. **A trace that starts inside a block stops dead** (fraction 0, `voxel.c:407-414`). Half-Life's Gauss "trace
   forward from inside the wall" trick (`gauss.cpp:480-486`) doesn't work in the block world. The penetration
   design below traces *backwards from outside* instead.
9. **Brightness floors stop real night.** Lightmap bytes are clamped to at least 20/128 (`dynworld.c:1551`), vertex
   light to at least 40/200 (`1568`, `981`, `1038`), and model light to at least 30 (`1179`). The torch pass
   compares the torch against the *baked* vertex luminance (`gl_dynworld.c:401`), so it also has to follow the time
   of day.
10. **The light compiler is Sven Co-op's SC-RAD (VHLT based)** (`maps_src/svencraft_sandbox.log`). The sandbox BSP
    has 19 `light` entities plus one `light_environment`, and only **style 0** on 4424 of 4688 faces (scanned from
    the BSP). That leaves room for a dimmable sun style.
11. **New since the survey started (another session).**
    - `SC_BulletHitWorld` (`sc_blast.cpp:231-…`, called from `combat.cpp:1525-1526` and the monster `FireBullets`):
      bullets now wear down and break glass, leaves, planks and logs (toughness table `sc_blast.cpp:218-229`) and
      throw `SC_Debris` splinters.
    - `sc_acoustics.cpp` sends each player a room type (`SVC_ROOMTYPE`) twice a second.

    Both matter for A4 and A6 below.

---

# Task A: realism features in the Half-Life/Sven tradition

## A0. Ranking (payoff ÷ effort)

| Rank | Feature | Payoff | Effort | Where it runs | Netcode/prediction risk |
|---|---|---|---|---|---|
| 1 | **Muzzle flash, smoke and shells** (task item 6): handle event 5005, gunshot dlight, smoke puffs, longer-lived shells | High (every shot) | S, 0.5-1 d | client only | none |
| 2 | **View model lag/sway, step-synced bob, landing dip** (item 1) | High (every second of play) | S, 0.5-1 d | client only | none |
| 3 | **Fall damage and landing** (item 5): Minecraft×5 curve, soft landings, fix the punch bug, landing slowdown | Medium | S, 0.5 d | shared `pm_shared` + server | low, if constants are shared |
| 4 | **Explosion feel** (item 4): distance-scaled shake (also airborne), flash, head kick, muffled ears with ringing | High (creepers are the core enemy) | M, 1-1.5 d (one engine audio change) | server message, client effects, engine mixer | none (cosmetic) |
| 5 | **Recoil and spread bloom** (item 2): real aim kick, spread grows when moving, airborne or firing, recovers when still | High (changes the gunplay) | M, 2 d + tuning | shared weapon code + prediction plumbing | **medium** (must be predicted exactly) |
| 6 | **Bullet penetration** (item 3): glass, wood, thin metal and grates, through all three worlds | Medium (few thin surfaces in the town) | M-L, 2-3 d | server, plus a client mirror for effects | medium (cosmetic mismatches) |
| 7 | Cheap extras (item 7): impact effects per material, damage flinch, lowering the gun near walls, strafe roll, sprint | Medium each | S each | mostly client | low |

Defaults below keep the Sven look. Each feature has a cvar to turn it off. Values are a starting point for tuning
in game.

---

## A1. Muzzle flash, smoke and shell casings (task item 6, rank 1)

### What exists
- **Shells.** `EV_EjectBrass` (`cl_dll/ev_common.cpp:138-144`) calls `R_TempModel` with a fixed **2.5 s life**. The
  velocity comes from `EV_GetDefaultShellInfo` (`ev_common.cpp:153-190`: right 50-70, up 100-150, forward 25).
  Glock and MP5 eject `models/shell.mdl`; the shotgun ejects `models/shotgunshell.mdl` (`ev_hldm.cpp:487-499`,
  `549-564`, `603-616`, `660-673`). The Python ejects nothing, which is right for a revolver. The engine plays the
  bounce sounds (`TE_BOUNCE_SHELL` / `TE_BOUNCE_SHOTSHELL`).
- **View-model flash.** `EV_MuzzleFlash` (`ev_common.cpp:195-206`) sets `EF_MUZZLEFLASH` on the view model. The
  studio renderer turns that into a 24-unit *entity* light (`gl_studio.c:2449-2461`). The sprite comes from studio
  event 5001 → `R_MuzzleFlash` (`entity.cpp:547-549` → `cl_tent.c:835-860`). **The Sven MP5 and shotgun use 5005
  instead and get no sprite** (finding 1). Model events, read from the `.mdl` files:
  `v_9mmhandgun`: 5001 "15"; `v_357`: 5001 "15"; `v_9mmAR`: 5005 `Muzzle_mp5.txt` on fire1-3; `v_shotgun`: 5005
  `muzzle_SG.txt` on shoot1/shoot_FullAUTO/shoot_double. The `p_` models have no events.
- **Smoke.** None after shots. Sven ships `sprites/wep_smoke_01.spr`, `wep_smoke_02.spr`, `Puff1-3.spr`,
  `wallpuff.spr`, plus flash sprites `MP5Flash.spr`, `SGflash.spr`, `muzzleflash1-3.spr`.
- **Shell models in Sven:** `shell.mdl`, `shotgunshell.mdl`, `shell_357.mdl`, `shell_762.mdl`, `saw_shell.mdl`,
  `bm_shell.mdl`.

### Proposal
1. **Handle event 5005.** Parse `events/<name>.txt` once (keys `attachment`, `bone`, `offset`, `spritename`, `scale`,
   `colorR/G/B`, `rendermode`, `transparency`; see `run/svencoop/events/muzzle_example.txt`) into a small cache.
   On the event, spawn a one-frame additive sprite tempent at `entity->attachment[n]` with a random roll, the same
   way `R_MuzzleFlash` does.
2. **Gunshot world light.** In every `EV_Fire*` add a short client dlight at the muzzle, for *every* shooter (events
   run for everyone in the PAS), so other players' shots light the street too. The per-pixel dlight passes already
   draw it on the diggable world and the blocks.
3. **Smoke.** After a shot, spawn a puff of `wep_smoke_0[12].spr` at the view model's muzzle: alpha ~60, rising
   12-20 u/s, about 1 s, fading out. For automatic fire, emit it a little after the trigger is released (barrel
   smoke). Also spawn one at the third-person gun position for remote players.
4. **Shells.** Make the life a cvar (default 10 s; tempents recycle themselves when the pool is full). On the
   Python's reload, drop six `shell_357.mdl` casings, triggered by the view model's existing
   `5004 "weapons/357_shellsout.wav"` event (the hook is in `HUD_StudioEvent`, so no server change). Optionally
   eject shotgun shells on the pump sound rather than at the shot.

### Code sketch (client)
```cpp
// entity.cpp, HUD_StudioEvent (543-569): Sven Co-op's scripted muzzle flashes
case 5005:
	SC_ScriptMuzzleFlash( entity, event->options );	// cl_dll/svencraft/sc_effects.cpp (new)
	break;
case 5004:
	gEngfuncs.pfnPlaySoundByNameAtLocation( (char *)event->options, 1.0, (float *)&entity->attachment[0] );
	if( strstr( event->options, "357_shellsout" ))
		SC_RevolverShells( entity );		// six shell_357.mdl tempents falling from the cylinder
	break;

// sc_effects.cpp
typedef struct { char name[64]; int attachment; int sprite; float scale; byte rgb[3]; int rendermode; int amt; } scmuzzle_t;
void SC_ScriptMuzzleFlash( const cl_entity_t *ent, const char *script )
{
	const scmuzzle_t *m = SC_MuzzleScript( script );	// parsed once from events/<script> (gEngfuncs.COM_LoadFile)
	if( !m || !m->sprite ) return;
	TEMPENTITY *t = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh( (float *)ent->attachment[bound( 0, m->attachment, 3 )], gEngfuncs.hudGetModelByIndex( m->sprite ));
	if( !t ) return;
	t->entity.curstate.rendermode = m->rendermode;		// 5 = additive
	t->entity.curstate.renderamt = m->amt;
	t->entity.curstate.rendercolor.r = m->rgb[0]; /* g, b */
	t->entity.curstate.scale = m->scale * 0.01f;		// Sven's "scale 10": start here and tune in game
	t->entity.angles[2] = gEngfuncs.pfnRandomLong( 0, 359 );
	t->die = gEngfuncs.GetClientTime() + 0.01f;		// one frame, like R_MuzzleFlash
	t->entity.curstate.frame = gEngfuncs.pfnRandomLong( 0, t->frameMax );
}

// ev_common.cpp: the shot lights the room (world dlights are per-pixel on the diggable world and the blocks)
void EV_MuzzleLight( event_args_t *args, const float *forward, float radius )
{
	if( !cl_muzzlelight->value ) return;
	vec3_t org;
	VectorCopy( args->origin, org );
	org[2] += args->ducking ? VEC_DUCK_VIEW : DEFAULT_VIEWHEIGHT;
	dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( 0 );
	VectorMA( org, 32.0f, forward, dl->origin );
	dl->radius = radius;				// 9mm 140, MP5 150, shotgun/357 180
	dl->color.r = 255; dl->color.g = 196; dl->color.b = 120;
	dl->die = gEngfuncs.GetClientTime() + 0.05f;
	dl->decay = radius * 16.0f;
}
// call it next to each EV_EjectBrass / sound in EV_FireGlock_Impl, EV_FireMP5, EV_FireShotGun*, EV_FirePython
```

### Cvars
`cl_muzzlelight 1`, `cl_gunsmoke 1`, `cl_shell_life 10`.

### Risks
- Cosmetic and client-only.
- An MP5 at 10 rounds/s keeps a dlight alive about half the time. That adds a per-pixel pass on the cells it
  touches (`R_DlightsDraw`, `gl_dynworld.c:471`). Cheap, but worth watching with many players.
- The view model is drawn with its own FOV (`default_fov 120` here), so world-space smoke at a view-model attachment
  can sit slightly off. Spawn smoke at `vecSrc + forward*24` if it looks wrong.
- Sven feel: Sven Co-op shows these flashes (that is what the 5005 scripts are for), so this *restores* the Sven
  look.

---

## A2. View model lag/sway and walk bob (task item 1, rank 2)

### What exists (`cl_dll/view.cpp`)
- `V_CalcBob` (`177-217`): Quake bob. One vertical scalar that cycles on the clock (`cl_bobcycle 0.8`,
  `cl_bob 0.01`, `cl_bobup 0.5`; cvars at `1613-1615`). It is **frozen while airborne**: it returns the old value
  when `onground == -1` (`185-190`).
- `V_CalcNormalRefdef` (`411-783`) applies it:
  - eye height `+= bob` (`444`);
  - gun pushed forward/up by the bob and tilted yaw/roll/pitch (`593-602`);
  - gun angles copied from the view (`573-583`, `V_CalcGunAngle` `282-303`);
  - the shake (`456-457`, gun `591`);
  - punch added (`629-634`);
  - stair-step smoothing (`636-660`);
  - with `cl_viewbob 1` (`hud.cpp:351`, on in `config.cfg`) the gun's interpolation is disabled (`772-778`).
- **There is no view-model lag:** the gun is locked to the view. The footstep timing lives in `PM_UpdateStepSound`
  (`pm_shared.c:~636-760`: a step every 300 ms at ≥210 u/s, 400 ms walking, slower ducked), unrelated to the bob
  clock. `sv_rollangle` is 0 (`sv_main.c:103`, `config.cfg:270`), so there is no strafe roll.

### Proposal
- **Lag/sway** (Source SDK's `CalcViewModelLag`, adapted). The gun's origin trails the turn: a remembered facing
  catches up at 5/s, faster after a flick. The gun also drifts against the movement and cants a little into a
  strafe. It moves the gun's *origin*, so hidden parts of the HL view models stay hidden. Turn it off when zoomed
  (`gHUD.m_iFOV < 90`).
- **Step-synced bob (`cl_bobstyle 1`).** The phase advances by π per footstep at `PM_UpdateStepSound`'s rate. The
  eye dips on every footfall (`|sin|`) and the gun swings sideways once per stride (`sin`). The amplitude eases in
  and out with speed instead of freezing in the air. `cl_bobstyle 0` keeps HL's exact bob.
- **Landing dip.** A critically damped spring pushed by the vertical speed of the last airborne frame. The eye
  drops up to ~10 units on a hard landing and comes back in about 0.25 s; the gun follows at 60%.

### Code sketch (`view.cpp`)
```cpp
cvar_t *cl_viewlag, *cl_bobstyle, *cl_bobamt_vert, *cl_bobamt_lat, *cl_landbob;	// registered in V_Init (1604)

// the gun trails the turn and drifts against the motion (Source's CalcViewModelLag)
static void V_CalcViewModelLag( struct ref_params_s *pparams, cl_entity_t *view )
{
	static vec3_t lastFacing;
	static float side, up;
	float dt = bound( 0.0f, pparams->frametime, 0.1f ), scale = cl_viewlag->value;
	vec3_t diff;

	if( scale <= 0.0f || pparams->health <= 0 || gHUD.m_iFOV < 90 || VectorIsNull( lastFacing ))
	{
		VectorCopy( pparams->forward, lastFacing );
		return;
	}
	VectorSubtract( pparams->forward, lastFacing, diff );	// pparams->forward has no punch (view.cpp:541)
	float speed = 5.0f, len = Length( diff );
	if( len > 1.5f ) speed *= len / 1.5f;			// catch up after a flick
	VectorMA( lastFacing, Q_min( speed * dt, 1.0f ), diff, lastFacing );
	VectorNormalize( lastFacing );
	VectorMA( view->origin, -5.0f * scale, diff, view->origin );

	float ts = bound( -1.5f, -DotProduct( pparams->simvel, pparams->right ) * 0.0035f, 1.5f );
	float tu = bound( -1.5f, -pparams->simvel[2] * 0.0015f, 1.5f );
	side += ( ts - side ) * Q_min( dt * 8.0f, 1.0f );
	up += ( tu - up ) * Q_min( dt * 8.0f, 1.0f );
	VectorMA( view->origin, side * scale, pparams->right, view->origin );
	VectorMA( view->origin, up * scale, pparams->up, view->origin );
	view->angles[ROLL] += side * 2.0f * scale;
}

// a dip on every footfall at the footstep sound's rate (pm_shared.c PM_UpdateStepSound), a sideways gun swing per stride
static float V_CalcBob2( struct ref_params_s *pparams, float *lateral )
{
	static double phase;
	static float amount;
	float dt = bound( 0.0f, pparams->frametime, 0.1f );
	float speed = sqrt( pparams->simvel[0] * pparams->simvel[0] + pparams->simvel[1] * pparams->simvel[1] );
	float maxspd = pparams->movevars->maxspeed > 0.0f ? pparams->movevars->maxspeed : 270.0f;
	float target = ( pparams->onground != -1 && pparams->waterlevel < 2 ) ? Q_min( speed / maxspd, 1.2f ) : 0.0f;
	amount += ( target - amount ) * Q_min( dt * 6.0f, 1.0f );	// eases out in the air (HL freezes it)
	phase += dt * M_PI / ( speed >= 210.0f ? 0.3f : 0.4f );	// pi per footstep
	*lateral = (float)sin( phase ) * cl_bobamt_lat->value * amount;
	return ( 0.5f - (float)fabs( sin( phase ))) * cl_bobamt_vert->value * amount;
}

// the eye sinks on a hard landing and springs back
static float V_LandingDip( struct ref_params_s *pparams )
{
	static float pos, vel, lastvz;
	static int wasair;
	float dt = bound( 0.0f, pparams->frametime, 0.05f );
	if( cl_landbob->value && pparams->onground != -1 && wasair && lastvz < -250.0f )
		vel -= Q_min( -lastvz - 200.0f, 800.0f ) * 0.12f;
	wasair = pparams->onground == -1;
	lastvz = pparams->simvel[2];		// the landing frame's own velocity is already clipped to 0
	vel += ( -180.0f * pos - 26.8f * vel ) * dt;	// k = 180, c = 2*sqrt(k): critically damped, ~0.25 s
	pos = bound( -10.0f, pos + vel * dt, 2.0f );
	return pos;
}
```
Hook-up in `V_CalcNormalRefdef`:
- `440`: `bob = cl_bobstyle->value ? V_CalcBob2( pparams, &lat ) : V_CalcBob( pparams );`
- `444`: add `+ dip` (where `dip = V_LandingDip( pparams )`);
- after `588`: `V_CalcViewModelLag( pparams, view );`
- `593-602`: with style 1, use `VectorMA( view->origin, lat, pparams->right, view->origin ); view->angles[ROLL] -= lat * 1.5f; view->origin[2] += bob * 0.6f + dip * 0.6f;`
  in place of HL's forward push.
- All of this must stay before the `cl_viewbob` copy (`772-778`).

### Cvars
`cl_viewlag 1` (0 = rigid Sven gun), `cl_bobstyle 1` (0 = HL bob), `cl_bobamt_vert 1.2`, `cl_bobamt_lat 0.8`,
`cl_landbob 1`. Optionally `sv_rollangle 1.5` for a subtle strafe roll; it is a movevar, so the server sets it.

### Risks
- None for prediction: everything is computed from the predicted `simorg`/`simvel` and only moves the camera and
  the view model.
- Sven feel: lag over 1.5 or a big bob reads as "modern shooter". Keep the defaults subtle; HL bob is one cvar away.
- `cl_vsmoothing` (`view.cpp:678-725`) shifts `view->origin` after the lag, which is fine.
- The hand/tool view models (`v_schand.mdl`, tool models) get the same lag. Check that the block held in the hand
  doesn't show its cut edge at maximum sway.

---

## A3. Fall damage and landing (task item 5, rank 3)

### What Half-Life (and Sven) do
Sven Co-op inherits this as far as I know, with the `mp_falldamage` cvar.

| Fall speed (u/s) | Height at sv_gravity 800 | Effect |
|---|---|---|
| ≥ 350 (`PLAYER_FALL_PUNCH_THRESHHOLD`) | ≥ 77 u (~2 blocks) | step sound, view roll `0.013 × v` (`pm_shared.c:2830`) |
| > 580 (`PLAYER_MAX_SAFE_FALL_SPEED`) | > 210 u (~5.3 blocks) | `pl_fallpain3` (`2787-2800`); damage |
| 1024 (`PLAYER_FATAL_FALL_SPEED`) | 655 u (~16 blocks) | 100 damage |

- **Single player** (the launcher's `+map` starts one): `CHalfLifeRules::FlPlayerFallDamage`
  (`singleplay_gamerules.cpp:202-208`), linear at **0.225 HP per u/s above 580**.
- **Multiplayer:** `mp_falldamage 0` (`game.cpp:36`) gives a fixed 10 (`multiplay_gamerules.cpp:469-486`).
- Damage is applied in `CBasePlayer::PostThink` (`player.cpp:2649-2683`), but only when the speed is over 580
  (`2660`). Water cancels it (`2652`). Armor doesn't absorb it (`467`).
- The `pm_shared` thresholds are duplicated in `dlls/player.h:21-25` and `pm_shared.c:79-83`.

### Proposal: Minecraft×5, with a "realistic" option
- **`sc_falldamage 1` (Minecraft×5, default).** Height from speed, `h = v² / (2·sv_gravity)`. Damage is
  `5 × max(0, ⌈h/40⌉ − 3)`: 5 HP per block beyond 3 blocks, which is Minecraft's 1 per block ×5, per DESIGN.md's
  "Minecraft numbers x5". It starts at 3 blocks (120 u ≈ 438 u/s) and is fatal from full health at 23 blocks.
- **`sc_falldamage 2` (realistic).** `100 × ((h − 120) / 480)^1.6`: 4 m → 2 HP, 6 m → 11, 10 m → 42, 15 m (600 u)
  → fatal.
- **Soft landings.** Snow (`CHAR_TEX_SNOW`) ×0.5; landing in water 0 (already); leaves (block 14, `D` in
  `materials.txt`) ×0.8; ducking at impact ×0.8 (a rolled landing).
- **Shared threshold.** Damage now starts *below* 580, so the `player.cpp:2660` gate and the pain sound in
  `PM_CheckFalling` must use one shared constant. Put `SC_FALL_HURT_SPEED` (438) in `pm_shared.h` and use it in
  both places.
- **Landing feel, in `PM_CheckFalling`** (shared, so predicted):
  - fix the clamp (`2832-2835` clamps `[0]`; it should clamp `[2]` to about 4 degrees);
  - add a pitch-down punch `punchangle[0] = min( (v−350)·0.012, 5 )` (it moves the aim a little, like a real stumble);
  - over the hurt speed, scale horizontal velocity by 0.5, so hard landings kill momentum.
- **Limp** (server, optional): over 20 damage, `g_engfuncs.pfnSetClientMaxspeed( edict, 0.6 × maxspeed )` for
  1.5 s. Client maxspeed is in clientdata, so the movement is predicted.

### Code sketch
```cpp
// dlls/svencraft/sc_player.cpp (new): one fall rule for both game modes
float SC_FallDamage( CBasePlayer *p )
{
	float v = p->m_flFallVelocity, g = Q_max( CVAR_GET_FLOAT( "sv_gravity" ), 1.0f );
	float h = v * v / ( 2.0f * g ), dmg;
	if( sc_falldamage.value >= 2.0f )
		dmg = h <= 120.0f ? 0.0f : 100.0f * powf(( h - 120.0f ) / 480.0f, 1.6f );
	else
		dmg = 5.0f * Q_max( 0.0f, ceilf( h / VOX_BLOCK_SIZE ) - 3.0f );
	char tex = SC_GroundTexType( p );		// TRACE_TEXTURE straight down (World_TraceTexture, pm_trace.c:862)
	if( tex == CHAR_TEX_SNOW ) dmg *= 0.5f;
	if( p->pev->flags & FL_DUCKING ) dmg *= 0.8f;
	return dmg;
}
// singleplay_gamerules.cpp:202 / multiplay_gamerules.cpp:469:
//	if( sc_falldamage.value ) return SC_FallDamage( pPlayer );
// player.cpp:2660: else if( m_flFallVelocity > SC_FALL_HURT_SPEED )   (shared with pm_shared.c:2787)
```

### Cvars
`sc_falldamage 1` (0 = Half-Life's, 1 = Minecraft×5, 2 = realistic).

### Risks
- Prediction: only the punch and the velocity change are in `pm_shared`. Both sides run them, so there is no
  mismatch as long as the thresholds are the shared constants.
- `m_flFallVelocity` is the speed at impact. A ladder grab mid-fall resets it, as in HL.
- Sven feel: falls hurt from a lower height than in Sven. That is the Minecraft rule the design asks for, and
  `sc_falldamage 0` restores HL.

---

## A4. Explosion feel near the player (task item 4, rank 4)

### What exists
- **Half-Life grenades, rockets, satchels, tripmines:** `CGrenade::Explode` (`ggrenade.cpp:55-140`) sends
  `TE_EXPLOSION` (`77`; the engine adds a dlight of radius 200, `cl_tent.c:1593-1599`), does `RadiusDamage`, then
  `SC_HLExplosion` (`104` → `sc_blast.cpp:200-205`). **No shake, no flash, nothing for the ears.**
- **Creeper:** `CCreeper::Explode` (`sc_creeper.cpp:535-592`): fireball, puffs, `TE_DLIGHT`,
  `UTIL_ScreenShake( c, 14, 160, 1, 900 )` (`585`).
- **Car:** `CSCCar::Explode` (`sc_car.cpp:207-…`): shake at `238`. **Barrel:** `CSCBarrel::Explode` (`319-…`):
  shake at `342`, `SC_Explosion` at `352`.
- `UTIL_ScreenShake` (`util.cpp:658-…`) gives **full amplitude anywhere inside the radius and nothing to airborne
  players** (`671`, `684-685`).
- Damage itself only punches 2 degrees (`player.cpp:615`).
- Audio: the engine has GoldSrc DSP presets (`s_dsp.c:72-105`; 14-16 "water" have a lowpass) and one mixer path
  for all world sounds (`S_PaintChannels`, `s_mix.c:545-574`: channels → `roombuffer` → `SX_RoomFX` → gain).

### Proposal
- **Server.** One message, `SCBlast(origin, power×10, flags)`, sent `MSG_PAS` from:
  - `SC_Explosion` (`sc_blast.cpp:122`), which covers every HL explosion through `SC_HLExplosion`, plus the
    creeper and the barrel;
  - `CSCCar::Explode`.

  Drop the three `UTIL_ScreenShake` calls (`sc_creeper.cpp:585`, `sc_car.cpp:238`, `342`).
- **Client** (`cl_dll/svencraft/sc_feel.cpp`, new). Everything is scaled by `k = (1 − d/R)²`, with
  `R = 200 + 160·power` (creeper 680 u, grenade 470, rocket 600):
  - **Shake:** noise-based, added in `V_CalcNormalRefdef` after `V_ApplyShake` (`view.cpp:457`). Amplitude
    `14k`, also in the air, ×0.6 when the blast isn't in line of sight. Duration `0.4 + 1.2k` s.
  - **Head kick:** `V_PunchAxis(PITCH, −(2 + 8k))`, plus roll ±5k away from the blast side (visual,
    `g_ev_punchangle`). Optionally a small server `pev->punchangle` for `k > 0.6` so a close blast also spoils
    your aim.
  - **Flash:** only when there is line of sight (`PM_TraceLine` from the eye) and you face it. `pfnSetScreenFade`
    warm white, alpha `200k·max(0.35, facing)`, fading over `0.25 + 0.5k` s.
  - **Ears:** for `k > 0.2`, set `s_muffle` and `s_earring` to `(k − 0.2) × 1.25`. The engine decays both.
- **Engine** (`s_mix.c`, `S_PaintChannels`). After `SX_RoomFX`:
  - a one-pole lowpass on `roombuffer` (cutoff from ~8 kHz down to ~600 Hz) and a −6 dB dip, both scaled by
    `s_muffle`;
  - a generated ~3.6 kHz sine with a slow beat, scaled by `s_earring` and mixed *after* the lowpass (so the ringing
    stays clear while the world goes dull);
  - both cvars decay at about 0.25/s.

  No assets are needed. A no-engine fallback would be setting `room_type 16` for a few seconds, but it doesn't
  work: `sc_acoustics.cpp` now re-sends each player's room type (`SVC_ROOMTYPE`) twice a second and would undo it
  within half a second. Keep the muffle separate from the room DSP.

### Code sketch
```cpp
// sc_blast.cpp: tell everyone in earshot how big it was
void SC_BlastFeel( const Vector &c, float power )
{
	MESSAGE_BEGIN( MSG_PAS, gmsgSCBlast, c );
		WRITE_COORD( c.x ); WRITE_COORD( c.y ); WRITE_COORD( c.z );
		WRITE_BYTE( (int)Q_min( power * 10.0f, 255.0f ));
	MESSAGE_END();
}
// at the top of SC_Explosion (sc_blast.cpp:122) and in CSCCar::Explode (power 2.5)

// cl_dll/svencraft/sc_feel.cpp
static float g_shakeAmp, g_shakeEnd;
static int __MsgFunc_SCBlast( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	vec3_t org, dir, fwd, right;
	org[0] = READ_COORD(); org[1] = READ_COORD(); org[2] = READ_COORD();
	float power = READ_BYTE() * 0.1f;
	if( !cl_blastfx->value ) return 1;
	VectorSubtract( org, v_origin, dir );			// v_origin: last frame's eye (view.cpp:84)
	float k = 1.0f - VectorNormalize( dir ) / ( 200.0f + 160.0f * power );
	if( k <= 0.0f ) return 1;
	k *= k;
	bool seen = gEngfuncs.PM_TraceLine( v_origin, org, PM_TRACELINE_PHYSENTSONLY, 2, -1 )->fraction > 0.95f;
	float now = gEngfuncs.GetClientTime();
	g_shakeAmp = Q_max( g_shakeAmp, 14.0f * k * ( seen ? 1.0f : 0.6f ));
	g_shakeEnd = Q_max( g_shakeEnd, now + 0.4f + 1.2f * k );
	AngleVectors( v_angles, fwd, right, NULL );
	V_PunchAxis( 0, -( 2.0f + 8.0f * k ));
	V_PunchAxis( 2, ( DotProduct( dir, right ) > 0.0f ? -5.0f : 5.0f ) * k );
	float facing = DotProduct( dir, fwd );
	if( seen && facing > -0.2f )
	{
		screenfade_t sf = { 0 };
		sf.fader = 255; sf.fadeg = 228; sf.fadeb = 185;
		sf.fadealpha = (byte)( 200.0f * k * Q_max( 0.35f, facing ));
		sf.fadeEnd = now + 0.25f + 0.5f * k;
		sf.fadeSpeed = sf.fadealpha / ( sf.fadeEnd - now );
		sf.fadeFlags = FFADE_IN;
		gEngfuncs.pfnSetScreenFade( &sf );
	}
	if( k > 0.2f )
	{
		float ears = Q_min( 1.0f, ( k - 0.2f ) * 1.25f );
		gEngfuncs.Cvar_SetValue( "s_muffle", Q_max( ears, gEngfuncs.pfnGetCvarFloat( "s_muffle" )));
		gEngfuncs.Cvar_SetValue( "s_earring", Q_max( ears, gEngfuncs.pfnGetCvarFloat( "s_earring" )));
	}
	return 1;
}
// view.cpp after line 457: SC_ApplyBlastShake( pparams->vieworg, pparams->viewangles ), a noise offset of g_shakeAmp
// fading to g_shakeEnd
```

### Cvars
Client: `cl_blastfx 1`. Engine: `s_muffle 0`, `s_earring 0` (set by the client), and `s_earring_vol 0.35` for players
who hate the tone.

### Risks
- Cosmetic, so no prediction issue. One extra small message per explosion.
- Chained creepers or cars stack effects, which is why the code takes `max` and doesn't add.
- The screen fade is shared with `env_fade`/ScreenFade messages (story scripting); a blast overwrites a running
  scripted fade. Skip the flash while `pfnGetScreenFade` shows a scripted fade (`FFADE_STAYOUT`).
- Sven feel: Sven itself is restrained here. Keep the flash and ringing for *close* blasts only (`k > 0.2` is about
  55% of R).

---

## A5. Per-weapon recoil and spread bloom (task item 2, rank 5)

### What exists
- **Constant spread per gun:**
  - Glock: primary `0.01`, secondary `0.1` (`dlls/glock.cpp:106-114`, used at `169`);
  - MP5: 3° single player / 6° multiplayer (`mp5.cpp:160-173`);
  - Python: 1° (`python.cpp:195`);
  - shotgun: 10° single player, 10×5° / 20×5° multiplayer (`shotgun.cpp:26-27`, `161-166`, `232-237`).
- **The bullets are predicted exactly.** `FireBulletsPlayer` uses `UTIL_SharedRandomFloat( shared_rand + … )` on
  the server (`combat.cpp:1507-1508`) and on the client (`hl_weapons.cpp:291-317`), and the event carries the spread
  offset (`fparam1/2`). Buckshot is the exception: the client rolls its own random (`ev_hldm.cpp:394-406`), as in HL.
- **Recoil is visual only** (finding 3): `g_ev_punchangle`, decayed by `V_DropPunchAngle` (`view.cpp:1577-1585`).
  The aim uses `v_angle + pev->punchangle` (`player.cpp:4420`), but no gun writes `pev->punchangle`.
  `PM_DropPunchAngle` (`pm_shared.c:2915-2923`, called at `2971`) already decays `pev->punchangle` identically on
  both sides at `10 + 0.5·len` degrees/s.
- **Prediction plumbing:**
  - server weapon timers decay in `CBasePlayer::PostThink` (`player.cpp:2719-2759`); the client mirrors them in
    `HUD_WeaponsPostThink` (`hl_weapons.cpp:948-996`);
  - weapon state goes through `GetWeaponData` (`dlls/client.cpp:1657-1706`) → `weapon_data_t`;
  - Sven's `delta.lst` (used through `basedir svencoop`) sends `fuser4` with 22 bits, ×1000, which HL doesn't use;
  - `HUD_WeaponsPostThink` runs *after* the client's movement for the command (`cl_pmove.c:931` then `938`), so
    `to->client` is the post-move state, like the server's `PostThink`.

### Proposal
- **Spread** = `base + move·min(speed/maxspeed, 1) + bloom (+ air when airborne)`, ×`duck` when crouched.
  - `bloom` grows by `shot` per round up to `maxbloom` and recovers at 0.10/s.
  - It lives in `CBasePlayerWeapon::m_flBloom` and is predicted through `weapon_data_t.fuser4`.
  - Speed and flags come from the *post-move* state on both sides (new `CBasePlayer::m_flSpreadSpeed` /
    `m_iSpreadFlags`).
- **Aim kick:** `pev->punchangle.x -= kick` and `.y += shared random ±kickyaw`, capped at -10. It is recovered by
  the existing `PM_DropPunchAngle`. At HL's decay rate, kicks under ~1 degree per shot at 10 rounds/s don't climb.
  The MP5's 0.9 + bloom climbs slowly, as intended.
- **Predict the punch:**
  - client: `player.pev->punchangle = to->client.punchangle` before `ItemPostFrame` and copy it back after
    (`hl_weapons.cpp` around `805` and `883`);
  - server: unchanged (`clientdata.punchangle`, `client.cpp:1757`).
- **Visual punch:** halve the event `V_PunchAxis` values so the total view kick stays near HL's.
- **Starting table** (VECTOR_CONE units: 0.0087 = 1 degree):

| Gun | base | move | air | duck | per shot | max bloom | kick (deg up) | kick yaw (±deg) |
|---|---|---|---|---|---|---|---|---|
| Glock primary | 0.010 | 0.025 | 0.08 | 0.8 | 0.012 | 0.05 | 0.8 | 0.25 |
| Glock rapid | 0.100 | 0.030 | 0.10 | 0.9 | 0.010 | 0.05 | 0.6 | 0.4 |
| MP5 | 0.026 | 0.035 | 0.09 | 0.75 | 0.006 | 0.045 | 0.9 | 0.45 |
| .357 | 0.0087 | 0.040 | 0.12 | 0.8 | 0.040 | 0.06 | 3.0 | 0.6 |
| Shotgun (cone) | 0.087 | 0.020 | 0.05 | 0.9 | 0.010 | 0.03 | 2.5 / 4.0 double | 0.8 |
| Crossbow, zoomed | 0 | 0.050 | 0.10 | 1.0 | 0 | 0 | 1.0 | 0 |

### Code sketch
```cpp
// dlls/svencraft/sc_gunfeel.h (new; included by the weapon .cpp files, which both DLLs compile: cl_dll/wscript:47-62)
typedef struct { float base, move, air, duck, shot, maxbloom, kick, kickyaw; } scgunfeel_t;
#define SC_BLOOM_RECOVER	0.10f
static const scgunfeel_t g_SCFeelMP5 = { 0.026f, 0.035f, 0.09f, 0.75f, 0.006f, 0.045f, 0.9f, 0.45f };
/* ... one per row of the table ... */

inline float SC_GunSpread( CBasePlayerWeapon *w, const scgunfeel_t &f )
{
	CBasePlayer *p = w->m_pPlayer;
	float s = f.base + f.move * Q_min( p->m_flSpreadSpeed / Q_max( p->pev->maxspeed, 1.0f ), 1.0f ) + w->m_flBloom;
	if( !( p->m_iSpreadFlags & FL_ONGROUND )) s += f.air;
	if( p->m_iSpreadFlags & FL_DUCKING ) s *= f.duck;
	return s;
}
inline void SC_GunKick( CBasePlayerWeapon *w, const scgunfeel_t &f )
{
	CBasePlayer *p = w->m_pPlayer;
	w->m_flBloom = Q_min( w->m_flBloom + f.shot, f.maxbloom );
	p->pev->punchangle.x = Q_max( p->pev->punchangle.x - f.kick, -10.0f );
	p->pev->punchangle.y += UTIL_SharedRandomFloat( p->random_seed + 31, -f.kickyaw, f.kickyaw );
}

// mp5.cpp PrimaryAttack (160-173 become):
float s = SC_GunSpread( this, g_SCFeelMP5 );
vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( s, s, s ), 8192, BULLET_PLAYER_MP5, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed );
SC_GunKick( this, g_SCFeelMP5 );

// server, player.cpp PostThink before ItemPostFrame (2641):
m_flSpreadSpeed = pev->velocity.Length2D(); m_iSpreadFlags = pev->flags;
// server timer decay (player.cpp:2744 block):
gun->m_flBloom = Q_max( gun->m_flBloom - gpGlobals->frametime * SC_BLOOM_RECOVER, 0.0f );
// client.cpp GetWeaponData (1701): item->fuser4 = gun->m_flBloom;
// hl_weapons.cpp:774   pCurrent->m_flBloom = pfrom->fuser4;
// hl_weapons.cpp:805   player.m_flSpreadSpeed = to->client.velocity.Length2D(); player.m_iSpreadFlags = to->client.flags;
//                      player.pev->punchangle = to->client.punchangle;
// hl_weapons.cpp:883   to->client.punchangle = player.pev->punchangle;
// hl_weapons.cpp:941   pto->fuser4 = pCurrent->m_flBloom;
// hl_weapons.cpp:954   pto->fuser4 = Q_max( pto->fuser4 - cmd->msec / 1000.0f * SC_BLOOM_RECOVER, 0.0f );
```

### Cvars
`sc_gunfeel 1` (server, sent to the client weapons through physinfo or a cached `pmove->PM_Info_ValueForKey`, so both
sides agree; 0 = HL's constant cones and no aim kick), and client-side `cl_dyncrosshair 0` (optional, 1 draws the
current spread).

### Risks
- **Prediction mismatch** if any input differs between sides: speed and flags must be post-move on both, the bloom
  decay must use the same per-command time, and the punch must be predicted. A mismatch shows as client decals and
  tracers that don't match where the server hits, and as the view snapping when the server's punch arrives.
  `clientdata` sends velocity with limited precision, so `move·speed/maxspeed` can differ by a hair. Quantizing
  the speed ratio to 1/16 removes that.
- The Svencraft tool weapons (pickaxe/shovel/axe/hand) aren't predicted. Leave them out.
- Sven feel: the Half-Life guns as they are here are laser-accurate and only kick the camera. This is the biggest
  gameplay change of the list; make it a server cvar and tune it with the creepers' closing speed in mind.
- Autoaim (`GetAutoaimVector`, `player.cpp:4420`) already includes `punchangle`, so it follows the kick.

---

## A6. Bullet penetration (task item 3, rank 6)

### What exists
- Bullets stop at the first hit: server `CBaseEntity::FireBulletsPlayer` (`combat.cpp:1488-1567`, trace at `1517`),
  client mirror `EV_HLDM_FireBullets` (`ev_hldm.cpp:381-459`, trace at `426`).
- **New (another session):** on a world hit the server calls `SC_BulletHitWorld` (`combat.cpp:1525-1526` →
  `sc_blast.cpp:231-…`). It accumulates damage per 40-unit cell and breaks glass (toughness 1), leaves (12), planks
  and crafting tables (110) and logs (160) in both the block world and the town (`sc_blast.cpp:218-229`), with
  `SC_Debris` splinters on every hit. Penetration must cooperate with it: call it for *each* surface the round
  touches (entry, and the next surfaces after an exit), with the damage scaled the same way, so a window shot
  through breaks *and* the round carries on.
- Material type on both sides comes from the same engine lookup: `World_TraceTexture` (`pm_trace.c:862-886`, used
  by `SV_TraceTexture` and `PM_TraceTexture`). It reports blocks as `sc_block<id>` and diggable faces by texture.
  `run/svencraft/sound/materials.txt` maps them to `CHAR_TEX_*`: planks/logs W, glass Y, metal M, stone/brick C,
  dirt/grass D. The server's lookup is inside `TEXTURETYPE_PlaySound` (`dlls/sound.cpp:1463-1519`); the client's
  is `EV_HLDM_PlayTextureSound` (`ev_hldm.cpp:92-239`).
- The point tests see all three worlds: `SV_TruePointContents` (`sv_world.c:~784`) and `PM_PointContents`
  (`pm_trace.c:712`) both include `Vox_PointSolid || Dyn_PointSolid`.
- The town: walls are 16 u brick/concrete (`make_town.py:371`, `T = 16`); desks and wood panels are 2-3 u planks;
  windows are `func_breakable` glass about 4 u thick (`make_town.py:237-242`); crates are `func_breakable` wood;
  cars are solid `sc_car` brush entities. Gauss secondary already punches walls (`gauss.cpp:477-521`).

### Proposal
- Per-bullet **power** in "wood units":
  - 9mm/MP5 16 (through glass, desks and panels; not walls or a planks block);
  - .357 40 (through a 5 u metal sheet, a planks block just barely);
  - buckshot 6 per pellet.
- Material **cost per unit of thickness:**
  - glass 0.5, wood 1, grate 0.25;
  - vent/computer (sheet metal) 1.5, snow 1;
  - dirt 3, tile 4, metal 5, concrete/brick 8;
  - flesh and liquid: stop (no over-penetration in phase 1).
- **The exit is found from outside** (finding 8):
  1. `far = hit + dir · min(power/cost, 48)`;
  2. if `far` is solid (PointContents), the wall is too thick;
  3. otherwise trace `far → hit` and take its end as the exit.

  This works for BSP brush entities (glass, crates, doors, cars), blocks and diggable brushes alike, and the
  backward trace's plane is the exit face for the exit decal.
- Damage after each surface: `×(power_left / power_start)`, at least 0.25. Up to 3 surfaces.
- The client mirror runs the same loop for decals, impact particles and texture sounds at the exits. The tracer is
  drawn to the final end.
- Map content: the cars are already `M` and the windows `Y` in `materials.txt` (lines 44-60). But the town's desk
  and floor wood `DESK_GEN` and `M_WOOD1` aren't listed, so they fall back to concrete: they sound like concrete
  today and would stop every round. Add them, and any door or crate textures, as `W` in `tools/make_materials.py`.
  `WOOD_PANEL_01` is already `W`.

### Code sketch (server; the client is the same with `EV_PlayerTrace` / `gEngfuncs.PM_PointContents`)
```cpp
// common/sc_ballistics.h (new, shared)
inline float SC_PenCost( char t )
{
	switch( t )
	{
	case CHAR_TEX_GRATE: return 0.25f;	case CHAR_TEX_GLASS: return 0.5f;	case CHAR_TEX_WOOD: return 1.0f;
	case CHAR_TEX_SNOW: return 1.0f;	case CHAR_TEX_VENT: case CHAR_TEX_COMPUTER: return 1.5f;
	case CHAR_TEX_DIRT: return 3.0f;	case CHAR_TEX_TILE: return 4.0f;	case CHAR_TEX_METAL: return 5.0f;
	case CHAR_TEX_CONCRETE: return 8.0f;	default: return 0.0f;	// flesh, slosh: the round stops
	}
}
inline float SC_PenPower( int b ) { return b == BULLET_PLAYER_357 ? 40.0f : b == BULLET_PLAYER_BUCKSHOT ? 6.0f : ( b == BULLET_PLAYER_9MM || b == BULLET_PLAYER_MP5 ) ? 16.0f : 0.0f; }

// combat.cpp FireBulletsPlayer, per shot (replaces the single trace and hit block, 1516-1560)
Vector start = vecSrc;
float power = SC_PenPower( iBulletType ), power0 = Q_max( power, 1.0f ), scale = 1.0f;
for( int surf = 0; surf < 3; surf++ )
{
	Vector vecEnd = start + vecDir * flDistance;
	UTIL_TraceLine( start, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );
	if( tr.flFraction == 1.0f ) break;
	CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );
	if( tr.pHit == INDEXENT( 0 ))					// as now (combat.cpp:1525), per surface
		SC_BulletHitWorld( tr.vecEndPos, tr.vecPlaneNormal, SC_BulletDamage( iBulletType, iDamage ) * scale );
	SC_BulletHit( pEntity, &tr, vecDir, iBulletType, iDamage, scale, pevAttacker, start, vecEnd );	// the old switch, damage × scale
	char tex = SC_TraceTexType( &tr, start, vecEnd );	// factored out of TEXTURETYPE_PlaySound (sound.cpp:1463)
	float cost = SC_PenCost( tex );
	if( cost <= 0.0f || power <= 0.0f || ( pEntity && pEntity->MyMonsterPointer()))
		break;
	Vector far = tr.vecEndPos + vecDir * Q_min( power / cost, 48.0f );
	if( UTIL_PointContents( far ) == CONTENTS_SOLID ) break;		// blocks and the town too (sv_world.c:784)
	TraceResult back;
	UTIL_TraceLine( far, tr.vecEndPos, dont_ignore_monsters, ENT( pev ), &back );
	if( back.fStartSolid || back.flFraction >= 1.0f ) break;
	float thick = ( back.vecEndPos - tr.vecEndPos ).Length();
	power -= thick * cost;
	if( power <= 0.0f ) break;
	scale = Q_max( 0.25f, power / power0 );
	DecalGunshot( &back, iBulletType );				// the exit hole, facing out of the far side
	start = back.vecEndPos + vecDir * 1.0f;
}
```

### Cvars
`sc_penetration 1` (server; the client reads it the same way as `sc_gunfeel`).

### Risks
- Only the effects (decals, sounds, particles) are mirrored on the client, so a mismatch is cosmetic. Buckshot
  already mismatches (separate random).
- Remote clients don't have the diggable or block world (finding 7), so their mirror sees air there. That is the
  existing gap.
- The backward trace can hit a *second* object between the first surface and `far` and overestimate the thickness.
  That errs on the safe side.
- Window glass (`func_breakable`, `health 20`) that a round breaks is removed later in the frame and still traces
  as solid for this round, which is fine.
- Performance: two extra traces per penetrable hit, worst case 3 surfaces. The MP5 at 10 rounds/s is negligible.
- Decal budget: the diggable world keeps 1024 decals and 4096 fragments (`gl_dynworld.c:22-23`); exit holes use
  them up faster.

---

## A7. Other cheap wins (task item 7)

- **Impact effects by material** (`EV_HLDM_GunshotDecalTrace`, `ev_hldm.cpp:263-305`). Today every hit gets the same
  particles plus a ricochet whine half the time, even on wood and dirt. Pass the texture type from
  `EV_HLDM_PlayTextureSound`:
  - concrete/dirt: `sprites/wallpuff.spr` / `Puff1-3.spr` dust;
  - wood: 2-3 tiny `woodgibs.mdl` splinters (`R_BreakModel`);
  - glass: `glassgibs.mdl` shards;
  - metal: `R_SparkShower` and ricochet;
  - ricochet sounds only on metal and concrete.

  S effort, high payoff. The new server-side `SC_Debris` splinters from `SC_BulletHitWorld` cover glass, leaves,
  planks and logs in the worlds already, so the client effects shouldn't double them (skip wood and glass debris on
  world hits, or move the splinters client-side so they don't cost a network message per bullet).
- **Damage flinch.** `player.cpp:615` always punches -2. Scale it by damage, `-clamp(dmg·0.15, 1, 6)`, and add roll
  by hit side (`g_vecAttackDir`). S.
- **Lower the gun near walls.** Client trace 32 u forward from the eye; when blocked, slide `view->origin` back and
  down by up to 6 u and pitch it 15°. Purely cosmetic. S.
- **Strafe roll.** `sv_rollangle 1-2` (currently 0) turns on HL's own `V_CalcViewRoll` (`view.cpp:326-346`).
  Trivial.
- **Sprint, Minecraft-style.** Double-tap forward or `+alt1` (`IN_ALT1` is free in HL's usercmd) gives
  `maxspeed ×1.3` in `PM_CheckParamters` (`pm_shared.c:2931`, shared so predicted), with a +10 FOV kick. Stamina or
  hunger would need a predicted variable (one of `pmove->fuser*` after checking HL's use). M. Changes the Sven feel
  (Sven has no sprint), so it needs a cvar.
- **Blood and gibs.** HL's `TraceBleed`/gibs already work, and the decals stick to the diggable world. Cheap upgrade:
  Sven's `blood_01.spr`/`bloodspray.spr` for the impact sprite, and `gib_skull`, `gib_lung`, `gib_legbone` in the
  gib set. S.
- **Hit confirmation** (a quiet tick on a hit, from a server message in `TraceAttack` when the attacker is a player).
  Useful against small Minecraft mobs, but not in the Sven tradition; off by default.

---

# Task B: Minecraft-style day/night cycle

## B1. What lights the world today

| Part | How it is lit | Where | Day/night difficulty |
|---|---|---|---|
| Diggable town (`.dyn`) | CPU ray-traced lightmap per 256-unit cell, a luxel every 16 u: bounce `0.55·sky`, sky visibility from 5 upward rays (`2.2·vis·sky`), sun with 4-ray soft edges (`0.85·lambert·sun`), lamps (`I/dist·lambert·0.5`, shadowed). Packed into RGBA (`×100`, 128 = full, drawn doubled); vertex RGBA too (`×200`). Digging relights the cells below and down-sun of the hole (`Dyn_DirtyLightAround`, 791-824) | `dynworld.c`: `Dyn_LuxelSample` 1303-1340, `Dyn_LightGroup` 1376-1470 (sun combined at 1432-1433), `Dyn_PackLightmaps` 1514-1579, `Dyn_BuildCell` 1581-1656, `Dyn_GetCell` 1670-1690 (~1.5 ms/cell, ~600 cells). Renderer `gl_dynworld.c`: upload `R_DynLightmap` 339-356, lightmap pass `679-686` (`GL_DST_COLOR, GL_SRC_COLOR`), torch pass 359-410 / 688-697, dlights 699-708 | Medium: the sources are summed into one byte triple and lost. **Keep them apart** |
| Models | `r_worldlight_override` (`ref_light.c:471`, `610-614`) → `R_VoxModelLight` (`gl_voxel.c:235-268`) → `Dyn_ModelLight` (`dynworld.c:1115-1186`): sky 5 rays, sun 4 rays, lamps; cached per 16 u until a carve | as listed | Easy: cache the components, combine per query |
| Block world | Vertex colour `shade × (0.55 + 0.15·ao) × max(sky ? 1 : 0.25, torch)`; sky is binary (open column above, under the town's "roof", now updated per dug column) | `gl_voxel.c`: `R_VoxBuildSection` 789-915 (843, 876-878), `R_VoxSky` 320-330, `R_VoxComputeRoof`/`R_VoxRoofColumn` 725-777, block light flood `R_VoxComputeLight` 455-536 | Easy: Minecraft's own trick (a light table) fits |
| BSP: brush entities (cars, burnt wrecks, doors, crates, glass), lamp posts, bedrock, sky shell | SC-RAD static lightmaps, `-bounce 1 -extra` (`maplib.py:142-152`); one `light_environment` (`make_town.py:661-662`); shadows of the town from the hidden `sc_shadow` func_wall (`make_town.py:663-671`, removed at map start `sc_world.cpp:375`). **Style 0 only** | `maps_src/svencraft_sandbox.log:268` "1 light styles" | Easy *if* the sun can be compiled into its own style |
| Sky | Skybox `grassy` (`make_town.py:659`, `world.cpp:655-658`), drawn with `GL_REPLACE` | `gl_warp.c:303-339` (313) | Easy: tint plus a box swap or crossfade |
| Fog | None. TriAPI fog exists (`gl_triapi.c:266-308`); multi-pass fog colours are handled for BSP (`gl_rsurf.c:311-337`) but not for the diggable world's passes | | Easy-medium |

Parsing: `Dyn_ParseLights` (`dynworld.c:314-365`) reads `light_environment` (`_light`, `_diffuse_light`, `pitch`,
yaw of `angles`; note finding 6) and every `light*` with an origin (up to 256). It is called from
`Dyn_LoadForMap` (`489-490`). The game DLL's `CEnvLight` also publishes `sv_skycolor_*` / `sv_skyvec_*`
(`lights.cpp:145-193`), which are **movevars** (`sv_main.c:105-110`, `221-230`) and so already networked to every
client.

## B2. Goals and constraints

- **Minecraft's rule.** Time runs on 24000 ticks (default 20 minutes; `0` sunrise, `6000` noon, `12000` sunset,
  `18000` midnight). Sky light dims from "15" to "4" over about 1.5 minutes at dusk. Block light (torches) doesn't
  change. Hostile mobs spawn in darkness. Zombies and skeletons burn in the sun.
- **Noon must look exactly like today.** The keyframe at the map's compiled sun (pitch -48, yaw 210) reproduces
  the current lightmaps bit for bit, so artists and screenshots don't change.
- **Fixed-function GL** (`gl_dynworld.c`, `gl_voxel.c` use `pgl*` client arrays and blending). GLSL entry points
  exist (`gl_opengl.c:397`, for the gl2 shim) but no shader pipeline for these worlds yet. Phase 1 must not need
  shaders.
- **Don't re-run the ray tracer for time of day.** Moving shadows have to be precomputed or keyframed.
- **Opt-in per map.** Worldspawn `sc_daycycle "1"`; story maps can lock the time.

## B3. The clock and how it reaches everyone

- **Authority: the game DLL.** `sc_time` (ticks), advanced in `StartFrame` by
  `24000 / sc_daylength` per second (`sc_daylength 1200`). Commands `sc_time set day|noon|sunset|night|midnight|<t>`
  and `sc_time lock 0|1` (cheats, like `/time set`). Saved with the game later.
- **Transport: the existing movevars, no new protocol.** About once a second, if the value changed by more than a
  small step, set:
  - `sv_skyvec_x/y/z`: the direction the sunlight travels, same meaning as `CEnvLight`'s;
  - `sv_skycolor_r/g/b`: sun colour × intensity, 0-255.

  The engine sends changed movevars to every client. The renderer and the engine's diggable world read them, and
  everything else (sky colour, moon, lamps, fog, sky box) is a function of the **sun elevation** `−skyvec_z` through
  one shared header. The client smooths between updates (lerp the weights over 1 s).
- **One curve for all three code bases** (`common/sc_daytime.h`, plain C, included by `dynworld.c`, `ref/gl` and the
  game):

```c
// common/sc_daytime.h: the time of day as light, shared by the game, the engine and the renderer
typedef struct { float sun, moon, sky, lamp, floor; float suntint[3], skytint[3]; } sc_daylight_t;

static inline float SC_Smooth( float a, float b, float x ) { x = ( x - a ) / ( b - a ); x = x < 0 ? 0 : x > 1 ? 1 : x; return x * x * ( 3 - 2 * x ); }

// elev: sine of the sun's elevation (-1 midnight .. 1 zenith); noon of a map whose sun is at 48 degrees gives sin(48°)
static inline void SC_Daylight( float elev, sc_daylight_t *d )
{
	float low = 1.0f - SC_Smooth( 0.05f, 0.45f, elev );			// golden hour
	d->sun  = SC_Smooth( -0.02f, 0.25f, elev );				// the sun fades in the last ~15 degrees
	d->moon = 0.10f * SC_Smooth( 0.0f, 0.25f, -elev );			// same shadows as the sun, cold and dim
	d->sky  = 0.12f + 0.88f * SC_Smooth( -0.20f, 0.30f, elev );		// Minecraft: sky light 15 -> ~4
	d->lamp = 1.0f;								// phase 1: lamps always on (see B4)
	d->floor = 6.0f + 14.0f * SC_Smooth( -0.2f, 0.2f, elev );		// lightmap byte floor (today 20)
	d->suntint[0] = 1.0f; d->suntint[1] = 1.0f - 0.25f * low; d->suntint[2] = 1.0f - 0.55f * low;
	float night = 1.0f - SC_Smooth( -0.25f, 0.1f, elev );
	d->skytint[0] = 1.0f - 0.45f * night; d->skytint[1] = 1.0f - 0.35f * night; d->skytint[2] = 1.0f;
}
```

- **Sun path.** The sun follows a great circle tilted so its highest point is the compiled noon sun (elevation 48
  degrees, on the side opposite the light's yaw of 210). It rises and sets 90 degrees to either side of that
  azimuth. Along the circle angle `θ = 2π·t/24000`, the elevation is `asin(sin 48° · sin θ)`, and the azimuth from
  the rising point is `atan(cos 48° · tan θ)`. Noon is then exactly the compiled sun. In phase 1 only the *strength*
  follows the path; the shadows stay at the noon direction.

## B4. Diggable world: keep the light sources apart, recombine on the CPU

**Store layers per luxel instead of the final colour.** In `Dyn_LightGroup` the luxel already has the parts
separated: `base` = bounce + sky + lamps, and `sunaa`. Split `base` further:
- `sky`: the scalar visibility `vis`, 0..1;
- `sun`: `sunaa·lambert`, 0..1;
- `lamp`: lamps RGB, scaled by 1/2.0 into a byte.

The bounce term `0.55·sky_color` stays a constant of the sky. That is **5 bytes per atlas texel** next to the 4-byte
RGBA (+125% CPU memory for lightmaps; no extra GPU memory), plus the same 5 bytes per vertex for `rgba`. The
neighbour fill (`1435-1469`) runs on each layer the same way, since it is linear.

**Re-pack** (`Dyn_RepackCell`, new) writes the existing RGBA atlas and vertex RGBA from the layers and the current
weights. In-place with `GL_UpdateTexture` (`gl_draw.c:60`), so no texture is recreated:

```c
// dynworld.c: the cell's light again for the current time of day (no rays; ~32K texels -> ~0.1 ms)
static void Dyn_RepackCell( dyncell_t *cell, const dynlayers_t *L, const dyndaylight_t *d )
{
	int n = cell->lmsize[0] * cell->lmsize[1];
	for( int i = 0; i < n; i++ )
	{
		byte *out = &cell->lightmap[i * 4];
		float sky = L->lm[i].sky * ( 1.0f / 255.0f ), sun = L->lm[i].sun * ( 1.0f / 255.0f );
		for( int c = 0; c < 3; c++ )
		{
			float v = d->ambient[c] + sky * d->sky[c] + sun * d->sun[c] + L->lm[i].lamp[c] * ( 2.0f / 255.0f ) * d->lamp;
			out[c] = (byte)bound( d->floor, Q_min( v, DYN_LM_MAX ) * 100.0f, 255.0f );
		}
	}
	/* same for cell->verts[k].rgba from L->vert[k] (x200, floor 2*d->floor) */
	cell->lmrev++;
}
// d->ambient = 0.55 * sky_color * skytint * day.sky; d->sky = 2.2 * sky_color * skytint * day.sky;
// d->sun = 0.85 * ( sun_color * suntint * day.sun + moon_color * day.moon ); moon_color = ( 0.55, 0.65, 1.0 ) * |sun_color|
```

- **When.** `Dyn_GetCell` (`1670`) re-packs a cell whose `daylight_rev` is stale, with its own budget (16 per frame).
  It is only called for *visible* cells, so off-screen cells catch up when seen. A cell more than 2 steps stale is
  re-packed regardless of budget, so it never pops in old.
- **Quantization.** Steps of the weights of 1/64. A 20-minute day changes light only around dusk and dawn, roughly
  one step per second there and none at noon or midnight.
- **Cost.** ~0.1-0.2 ms per cell including upload, so 600 cells is about 100 ms in total, spread to ≤ 2 ms per frame
  during transitions and 0 otherwise.
- **Renderer** (`gl_dynworld.c`):
  - `R_DynLightmap` compares a new `dyncell_t.lmrev` (add to `dynworld_api.h:31-42`) instead of `rev`, and updates
    in place when the size is unchanged;
  - `R_DynTorchColors` must key on `lmrev` too (`363`, `366`, `407`), because the torch-vs-existing-light test uses
    the vertex luminance (`401`), so torches correctly look brighter at night.
- **Fog in the diggable world's passes** (if fog is enabled at night):
  - lightmap multiply pass: fog colour 0.5 grey (doubled blend → neutral);
  - additive torch and dlight passes: black;
  - restore afterwards (same idea as `gl_rsurf.c:311-337`).
- **Lamps.** Phase 1 keeps every map lamp on, as Half-Life maps do. Phase 2 option: a `sc_night "1"` key on
  outdoor `light` entities (street lamps) puts them in a second lamp layer (+3 bytes) weighted by `1 − day.sun`.
  They switch on at dusk; interior lights stay on.
- **Floors** (finding 9). The `bound( 20, …)` / `bound( 40, …)` clamps become `d->floor` (20 at day, 6 at night), so
  nights can be properly dark while caves keep their own look.

## B5. Moving sun: shadows

| Option | Look | Build cost | Memory | Verdict |
|---|---|---|---|---|
| **A. Fixed direction** (noon), strength from the curve, moon = same shadows tinted blue | Minecraft has no shadows at all; low-sun hours keep noon shadows but fade out | none | +0 | **Phase 1** |
| **B. K keyframe directions** (e.g. elevations 8°, 25°, 48°(noon), 25° W, 8° W): one sun-visibility byte per keyframe per luxel; shadows cross-fade between the two keyframes around the current sun | Long golden-hour shadows, believable motion (a slow penumbra slide) | Sun rays are 1 (+4 at edges) of ~6 + lamps per luxel. 5 keyframes ≈ ×1.8 build time, ~2.7 ms/cell, ~1.6 s for 600 cells, still lazy per cell and per dig | +4 bytes per luxel | **Phase 3** |
| C. Continuous: re-trace the sun layer in the background as the sun moves | Exact | ~0.4-0.6 ms of sun rays per cell, so ~300 ms per sun step for the town; needs a worker thread plus locking against carving, and a cross-fade to hide per-cell pops | +1 layer | Not worth it before GLSL or shadow maps |
| D. Real-time shadow maps (GPU) | Exact, dynamic objects too | Needs the shader path | — | Graphics phase in DESIGN.md |

**With option B:**
- `Dyn_LightGroup`'s edge anti-aliasing (`1395-1430`) runs per keyframe.
- The sun term in the re-pack becomes `lambert_t · lerp(vis_k, vis_k+1)` with the *current* direction's Lambert,
  computed per face in the re-pack from the stored face normal (keep a per-group normal index).
- `Dyn_ModelLight` stores K sun bits per cache entry.
- Digging rebuilds a cell at about 2.7 ms instead of 1.5 ms; `DYNR_REBUILDS_PER_FRAME 3` (`gl_dynworld.c:20`) still
  fits a frame.
- `Dyn_DirtyLightAround` (`dynworld.c:791-824`) sweeps "down the sun's rays" with the one `sun_dir` (`813`). With
  keyframes it must sweep down *every* keyframe direction. That dirties more cells per dig (low suns cast long), so
  limit low keyframes' rays to ~1500 u, or relight their layers lazily over the next seconds.

## B6. Models

`Dyn_ModelLight` (`dynworld.c:1115-1186`):
- Cache the parts per 16-unit key: sky vis, sun vis (×K in phase 3) and lamp RGB, instead of `rgb`.
- Combine them with the current weights on every query.
- Return `lightdir = −sun_dir` only while `day.sun + day.moon` is meaningful.

`R_VoxModelLight` (`gl_voxel.c:235-268`) is unchanged. Its torch `max` makes monsters near torches glow at night,
which is right.

## B7. Block world: Minecraft's light table

Today the colour is baked per vertex as `shade·ao · max(sky ? 1 : 0.25, torch(t))` (`gl_voxel.c:843`, `876-878`).
That is exactly Minecraft's `max(skylight·daylight, blocklight)`. Do what Minecraft does:
- The vertex colour keeps only `shade·(0.55 + 0.15·ao)`.
- A new `float lm[2]` per vertex gives (sky level, block level) / 16. Corners average like the smooth block light
  does now, so fractional coordinates interpolate.
- A **16×16 light table texture** on texture unit 1 (`GL_MODULATE`) holds `max(sky(s)·day.sky·skytint,
  torchcurve(b)·torchcolour)`. It is re-uploaded (1 KB) whenever the weights change. Row `s = 0` is the cave's 0.25,
  untouched by time.
- Multitexture is already used the same way in `R_DlightsDraw` (`gl_dynworld.c:555-561`).

Cost: zero per-frame CPU, a one-time `vvert_t` size increase (+8 bytes). Optional later: flood sky light sideways
into cave mouths (BFS like `R_VoxComputeLight`) to get Minecraft's gradients instead of today's binary roof.

## B8. BSP lightmaps (cars, doors, crates, glass, lamp posts)

**Yes, the sun can go into a light style we dim.**
1. **Compile.** Give the `light_environment` `"style" "20"` (`make_town.py:661`). VHLT/SC-RAD reads `style` for
   every `light*` entity, and the sky diffuse part of `light_environment` shares it. **To verify** with one compile:
   the RAD log's "1 light styles" (`svencraft_sandbox.log:268`) should become 2, and faces should carry styles
   {0, 20}. The 19 `light` lamps stay style 0.
2. **Fine-grained style values (engine, ~10 lines).** A style string is `'a'..'z'` (steps of 1/12, `×22`,
   `ref_light.c:57-61`), too coarse for a dusk fade on cars next to a smoothly fading street. In `CL_SetLightstyle`
   (`cl_tent.c:2389-2405`), accept `"#0.375"`: one entry, `map[0] = atof·12`. `CL_RunLightStyles` already uses the
   float.
3. **Game.** The time-of-day think calls `LIGHT_STYLE( 20, "#<day.sun-ish weight>" )` on each weight step. Style
   changes re-light the affected BSP faces automatically (`cached_light` check, `ref_light.c:704`). That is a few
   hundred faces here.
4. **Limits.** At most 4 styles per face. With phase 3 keyframes in the BSP, use **3 sun styles** (morning, noon,
   evening light_environments, if SC-RAD accepts several, which also needs a test compile) plus lamps on style 0.
   Each style duplicates the face lightmaps (the lighting lump is 1.2 MB now). Otherwise keep brush entities on the
   fixed sun; they are small, and their shadows come from the `sc_shadow` copy anyway.

## B9. Sky, fog, sun and moon

- **Phase 1.**
  - The server swaps `sv_skyname` at thresholds: `grassy` → `dusk` → `night` → `morning` → `grassy` (all in
    `run/svencoop/gfx/env/`). Movevars reload it on change (`cl_parse.c:195-196`), with a small one-off hitch
    (6 TGAs).
  - Tint: `R_DrawSkyBox` (`gl_warp.c:303-339`) switches `GL_REPLACE` (`313`) to `GL_MODULATE` with
    `pglColor3f(skytint × brightness)`, so the box darkens smoothly between swaps.
- **Phase 3.**
  - Crossfade two loaded sets (draw set A, then set B with alpha `GL_SRC_ALPHA`); preload the night set at map load.
  - Add a sun or moon glow sprite along `−skyvec`.
- **Fog.** The client calls `gEngfuncs.pTriAPI->Fog( colour, start, end, 1 )` from `V_CalcRefdef` each frame: day
  off or far, night dark blue at ~600-2500 u, dusk warm haze. Mind the diggable world's multi-pass fog (B4).
  `TriFog` with linear fog also fogs the sky box (`gl_triapi.c:300-301`), which suits a night box.

## B10. Monsters at night (Minecraft's rules, adapted)

- **Light level at a spot (server).** `L = max( 15·skyvis·day.sky, block light, lamp level )`, where:
  - `skyvis`: a `TraceLine` straight up reaching sky;
  - block light: `14 − Manhattan distance` in blocks to the nearest torch or burning furnace, from a list kept by
    `sc_mining` (`SC_IsTorch`) and the furnace code, so no per-attempt volume scan;
  - lamp level: a new **`dyn_api_t` v2** entry `LightAt(p, …)` that returns `Dyn_ModelLight`'s components on the
    server. `Dyn_ParseLights` already runs server-side, and the engine has no renderer dependency.

  Spawn allowed if `L ≤ 7` (Minecraft before 1.18).
- **Spawner** (`dlls/svencraft/sc_spawn.cpp`, `StartFrame` once a second):
  1. pick a random player and a random point 24-64 blocks away;
  2. drop it to the ground (trace down; needs 2 blocks of headroom, 72 u, and a solid floor);
  3. reject if within 24 blocks of *any* player or visible to one (Sven style: no pop-in);
  4. check the light level.

  Cap: `sc_mobcap 8` hostile per player (Minecraft's 70 is far too many for HL AI). Packs of 1-3. Despawn: farther
  than 32 blocks and unseen, 1/30 chance per second. Story-placed monsters are never despawned.
- **Who spawns:** `monster_creeper` now; Minecraft zombie, skeleton and spider as they are made (`monster_zombie`
  could stand in). Sven/HL monsters stay map-placed.
- **Daylight burning:** zombie and skeleton types with `day.sun > 0.5`, sky above and not in water get
  `DMG_BURN` 5 HP/s (Minecraft's 1 ×5) and a fire sprite (the car's fire sprite precache exists). Creepers don't burn.
- `sc_alliance` relationships (`SC_Relationship`) are unchanged; night just brings more invaders. Story hook: rifts
  could spawn more at night.

## B11. Phased plan

| Phase | Content | Dev effort | Runtime cost |
|---|---|---|---|
| **0. Groundwork** | `sc_time`/`sc_daylength`/`sc_daycycle`/`sc_time set`; `common/sc_daytime.h`; movevars publishing; numeric light styles (`#x`); fix `Dyn_ParseLights` pitch (finding 6); a quick look-dev prototype (client `pfnSetScreenFade` with `FFADE_MODULATE \| FFADE_STAYOUT` in a night blue) to pick colours before engine work | 0.5-1 d | none |
| **1. Fixed-sun day/night** | Diggable-world layers + re-pack + `lmrev` + torch key + floors; layered `Dyn_ModelLight`; block-world light table; BSP sun style (compile + `LIGHT_STYLE`); sky tint + skyname swaps; night fog | 2-3 d | re-pack ≤ 2 ms/frame during dusk and dawn only; +5 B/texel CPU memory; +8 B/vertex in the block world |
| **2. Night gameplay** | Server light query (`dyn_api` v2, torch list); spawner with caps and despawn; burning in daylight; optional night-only street lamps layer | 1-2 d | ~0.1 ms/s for spawn attempts |
| **3. Moving sun** | 5 sun keyframes per luxel (lerped), models too; BSP 3 sun styles if SC-RAD allows; sky crossfade, sun and moon sprites | 2-4 d | build ×1.8 (~2.7 ms/cell, lazy); +4 B/luxel |
| **4. Graphics phase** (DESIGN.md "Final phase") | GLSL combine for the diggable world (layers as textures, no CPU re-pack, continuous); real-time sun shadow maps | later | GPU |

## B12. Risks

- **Multiplayer.** The time of day is safe (movevars and light styles are networked). The worlds that it lights
  aren't networked yet (finding 7), so remote clients wouldn't see the diggable world anyway.
- **Look.** A cell re-packed one step later than its neighbour differs by ~1.5% brightness, which is invisible.
  Lower floors make interiors without lamps pitch black at night; that is intended (Minecraft), and torches matter
  again.
- **BSP vs diggable mismatch.** Brush entities follow the sun only through the style weight, and only once the
  compile test (B8) passes. Until then cars stay day-lit at night: a visible glitch, so do B8 inside phase 1.
- **Scripting and Sven feel.** Story maps need fixed times (`sc_time lock`) and per-map opt-in (`sc_daycycle`). A
  realistic Sven map at night with only streetlamps is atmospheric, but Sven's own maps have fixed, baked lighting.
  Keep the default day.
- **Saves.** `sc_time` must go into save files when saving exists (DESIGN.md "Planned: saving").

---

## Appendix: new cvars

| Cvar | Side | Default | Feature |
|---|---|---|---|
| `cl_muzzlelight` | client | 1 | A1 gunshot dlight |
| `cl_gunsmoke` | client | 1 | A1 smoke |
| `cl_shell_life` | client | 10 | A1 shells |
| `cl_viewlag` | client | 1 | A2 sway (0 = rigid) |
| `cl_bobstyle` | client | 1 | A2 (0 = HL bob) |
| `cl_bobamt_vert` / `cl_bobamt_lat` | client | 1.2 / 0.8 | A2 |
| `cl_landbob` | client | 1 | A2/A3 landing dip |
| `sc_falldamage` | server | 1 | A3 (0 HL, 1 Minecraft×5, 2 realistic) |
| `cl_blastfx` | client | 1 | A4 |
| `s_muffle`, `s_earring`, `s_earring_vol` | engine | 0, 0, 0.35 | A4 |
| `sc_gunfeel` | server (to client weapons) | 1 | A5 |
| `cl_dyncrosshair` | client | 0 | A5 |
| `sc_penetration` | server (to client mirror) | 1 | A6 |
| `sc_daycycle` | server / worldspawn key | 0 (map opts in) | B |
| `sc_time`, `sc_daylength` | server | 6000, 1200 | B3 |
| `sc_mobcap` | server | 8 | B10 |
