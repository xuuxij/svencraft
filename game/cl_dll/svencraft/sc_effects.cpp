/*
sc_effects.cpp - Svencraft: gunfire you can see

- Sven Co-op's view models (the MP5, the shotgun) ask for their muzzle flash with studio event 5005 and a script in
  events/ (sprite, scale, colour, render mode): read once, drawn like the engine's own muzzle flashes.
- Every shot lights its surroundings for an instant (a world dlight: the town's and the block world's per-pixel
  dlight passes pick it up), for every shooter the event reaches.
- A wisp of smoke leaves the muzzle.
- An explosion near the player (the server's SCBlast message: place and power) shakes the view, also in the air,
  harder the closer it is and less behind cover, kicks the head back and away from it, and flashes the screen
  when it is in sight; a close one dulls the hearing and leaves the ears ringing (s_muffle, s_earring: the
  engine's mixer, sound/s_mix.c; s_earring_vol 0.35 for the tone's volume).
cvars: cl_muzzlelight 1, cl_gunsmoke 1, cl_shell_life 10 (seconds a spent case lies around, ev_common.cpp),
cl_blastfx 1.
*/
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "r_efx.h"
#include "event_api.h"
#include "com_model.h"
#include "triangleapi.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sc_client.h"
#include "screenfade.h"
#include "shake.h"
#include "parsemsg.h"
#include "pmtrace.h"
#include "pm_defs.h"

static cvar_t *cl_muzzlelight, *cl_gunsmoke, *cl_blastfx;
extern vec3_t v_origin, v_angles;		// view.cpp: last frame's eye
void V_PunchAxis( int axis, float punch );
static int __MsgFunc_SCBlast( const char *pszName, int iSize, void *pbuf );
static int __MsgFunc_SCFog( const char *pszName, int iSize, void *pbuf );
static int __MsgFunc_SCSplash( const char *pszName, int iSize, void *pbuf );

typedef struct
{
	char	name[64];
	int	attachment;
	char	sprite[64];
	float	scale;
	int	rgb[3];
	int	rendermode;
	int	amt;
} scmuzzle_t;

#define SC_MAX_MUZZLES	16
static scmuzzle_t g_muzzles[SC_MAX_MUZZLES];
static int g_numMuzzles;

void SC_EffectsInit( void )
{
	cl_muzzlelight = gEngfuncs.pfnRegisterVariable( "cl_muzzlelight", "1", FCVAR_ARCHIVE );
	cl_gunsmoke = gEngfuncs.pfnRegisterVariable( "cl_gunsmoke", "1", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "cl_shell_life", "10", FCVAR_ARCHIVE );
	cl_blastfx = gEngfuncs.pfnRegisterVariable( "cl_blastfx", "1", FCVAR_ARCHIVE );
	gEngfuncs.pfnRegisterVariable( "cl_bulletlog", "0", 0 );	// each surface a round meets (ev_hldm.cpp), vs sc_bulletlog
	gEngfuncs.pfnHookUserMsg( "SCBlast", __MsgFunc_SCBlast );
	gEngfuncs.pfnHookUserMsg( "SCFog", __MsgFunc_SCFog );
	gEngfuncs.pfnHookUserMsg( "SCSplash", __MsgFunc_SCSplash );
}

// events/<name>: "key value" lines, // comments (see run/svencoop/events/muzzle_example.txt)
static const scmuzzle_t *SC_MuzzleScript( const char *name )
{
	for( int i = 0; i < g_numMuzzles; i++ )
		if( !stricmp( g_muzzles[i].name, name ))
			return &g_muzzles[i];
	if( g_numMuzzles >= SC_MAX_MUZZLES )
		return NULL;

	scmuzzle_t *m = &g_muzzles[g_numMuzzles++];
	memset( m, 0, sizeof( *m ));
	strncpy( m->name, name, sizeof( m->name ) - 1 );
	m->scale = 10.0f;
	m->rgb[0] = m->rgb[1] = m->rgb[2] = 255;
	m->rendermode = kRenderTransAdd;
	m->amt = 255;

	char path[128];
	int len = 0;
	snprintf( path, sizeof( path ), "events/%s", name );
	char *file = (char *)gEngfuncs.COM_LoadFile( path, 5, &len );
	if( !file )
		return m;
	char *text = (char *)malloc( len + 1 );
	if( text )
	{
		memcpy( text, file, len );
		text[len] = 0;
		for( char *line = strtok( text, "\r\n" ); line; line = strtok( NULL, "\r\n" ))
		{
			char key[64], val[64];
			if( sscanf( line, "%63s %63s", key, val ) != 2 || !strncmp( key, "//", 2 ))
				continue;
			if( !stricmp( key, "attachment" )) m->attachment = atoi( val );
			else if( !stricmp( key, "spritename" )) strncpy( m->sprite, val, sizeof( m->sprite ) - 1 );
			else if( !stricmp( key, "scale" )) m->scale = (float)atof( val );
			else if( !stricmp( key, "colorR" )) m->rgb[0] = atoi( val );
			else if( !stricmp( key, "colorG" )) m->rgb[1] = atoi( val );
			else if( !stricmp( key, "colorB" )) m->rgb[2] = atoi( val );
			else if( !stricmp( key, "rendermode" )) m->rendermode = atoi( val );
			else if( !stricmp( key, "transparency" )) m->amt = atoi( val );
		}
		free( text );
	}
	gEngfuncs.COM_FreeFile( file );
	return m;
}

// studio event 5005: the flash the script describes, at the model's attachment, for a frame or two
void SC_ScriptMuzzleFlash( const cl_entity_t *ent, const char *script )
{
	const scmuzzle_t *m = SC_MuzzleScript( script );
	if( !m || !m->sprite[0] )
		return;
	// the sprite must be precached by the server (sc_inventory.cpp)
	int index = 0;
	struct model_s *mdl = gEngfuncs.CL_LoadModel( m->sprite, &index );
	if( !mdl )
		return;
	int att = m->attachment < 0 ? 0 : ( m->attachment > 3 ? 3 : m->attachment );
	TEMPENTITY *t = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh( (float *)&ent->attachment[att], mdl );
	if( !t )
		return;
	t->entity.curstate.rendermode = m->rendermode;
	t->entity.curstate.renderamt = m->amt;
	t->entity.curstate.rendercolor.r = m->rgb[0];
	t->entity.curstate.rendercolor.g = m->rgb[1];
	t->entity.curstate.rendercolor.b = m->rgb[2];
	t->entity.curstate.renderfx = 0;
	t->entity.curstate.scale = m->scale * 0.015f;	// Sven's 10: a little over the Glock's flash (0.1)
	t->entity.curstate.framerate = 10;
	t->entity.curstate.frame = gEngfuncs.pfnRandomLong( 0, t->frameMax );
	t->entity.angles[2] = gEngfuncs.pfnRandomLong( 0, 359 );
	t->flags |= FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP;
	t->die = gEngfuncs.GetClientTime() + 0.03f;
}

// a shot: an instant of light around the muzzle, and a wisp of smoke
void SC_GunshotEffects( int idx, const float *src, const float *forward, float radius )
{
	vec3_t at;
	for( int i = 0; i < 3; i++ )
		at[i] = src[i] + forward[i] * 24.0f;

	if( !cl_muzzlelight || cl_muzzlelight->value )
	{
		dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( 0 );
		if( dl )
		{
			VectorCopy( at, dl->origin );
			dl->radius = radius;
			dl->color.r = 255;
			dl->color.g = 192;
			dl->color.b = 110;
			dl->die = gEngfuncs.GetClientTime() + 0.05f;
			dl->decay = radius * 12.0f;
		}
	}

	if( !cl_gunsmoke || cl_gunsmoke->value )
	{
		int sprite = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/wep_smoke_02.spr" );
		if( sprite )
		{
			vec3_t vel = { forward[0] * 10.0f, forward[1] * 10.0f, 14.0f + gEngfuncs.pfnRandomFloat( 0.0f, 8.0f ) };
			TEMPENTITY *t = gEngfuncs.pEfxAPI->R_TempSprite( at, vel, 0.12f, sprite, kRenderTransAlpha, kRenderFxNone, 0.22f, 1.1f, FTENT_SPRANIMATE | FTENT_FADEOUT );
			if( t )
			{
				t->entity.curstate.rendercolor.r = t->entity.curstate.rendercolor.g = t->entity.curstate.rendercolor.b = 200;
				t->entity.angles[2] = gEngfuncs.pfnRandomLong( 0, 359 );
			}
		}
	}
}

// a round from the air into water: droplets thrown up where it goes in, a breath of mist, a plip (Sven's water
// impacts; the server finds the same for monsters' rounds, dlls/svencraft/sc_blast.cpp)
static void SC_SplashAt( const float *a, int size );

static int __MsgFunc_SCSplash( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	vec3_t org;
	org[0] = READ_COORD(); org[1] = READ_COORD(); org[2] = READ_COORD();
	SC_SplashAt( org, READ_BYTE());
	return 1;
}

static bool SC_Liquid( const float *p )
{
	int c = gEngfuncs.PM_PointContents( (float *)p, NULL );
	return c == CONTENTS_WATER || c == CONTENTS_SLIME || c == CONTENTS_LAVA;
}

void SC_BulletSplash( const float *src, const float *end )
{
	vec3_t dir, a, b, mid;
	VectorSubtract( end, src, dir );
	VectorNormalize( dir );
	VectorCopy( src, a );
	VectorMA( end, -2.0f, dir, b );
	if( SC_Liquid( a ) || !SC_Liquid( b ))
		return;
	for( int i = 0; i < 12; i++ )		// the surface: where the air ends
	{
		for( int k = 0; k < 3; k++ )
			mid[k] = ( a[k] + b[k] ) * 0.5f;
		if( SC_Liquid( mid ))
		{
			VectorCopy( mid, b );
		}
		else
		{
			VectorCopy( mid, a );
		}
	}
	SC_SplashAt( a, 0 );
}

// the splash itself where something met the water: size 0 a round (from the shot's event, or the server's SCSplash
// for monsters'), 1-6 a body falling in (the server's, by how fast it fell)
static void SC_SplashAt( const float *a, int size )
{
	// droplets thrown up, falling back
	short color = gEngfuncs.pEfxAPI->R_LookupColor( 205, 220, 225 );
	float now = gEngfuncs.GetClientTime();
	float spread = 40.0f + 18.0f * size, rise = 170.0f + 45.0f * size;
	for( int i = 0; i < 14 + 22 * size; i++ )
	{
		particle_t *p = gEngfuncs.pEfxAPI->R_AllocParticle( NULL );
		if( !p )
			break;
		VectorCopy( a, p->org );
		p->org[2] += 1.0f;
		p->org[0] += gEngfuncs.pfnRandomFloat( -3.0f, 3.0f ) * size;
		p->org[1] += gEngfuncs.pfnRandomFloat( -3.0f, 3.0f ) * size;
		p->vel[0] = gEngfuncs.pfnRandomFloat( -spread, spread );
		p->vel[1] = gEngfuncs.pfnRandomFloat( -spread, spread );
		p->vel[2] = gEngfuncs.pfnRandomFloat( 0.4f, 1.0f ) * rise;
		p->color = color;
		p->die = now + gEngfuncs.pfnRandomFloat( 0.4f, 0.9f ) + 0.12f * size;
		p->type = pt_grav;
	}
	// the spout (Opposing Force's splash sprite) and a breath of spray
	int drops = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/wsplash3.spr" );
	if( drops )
	{
		float scale = gEngfuncs.pfnRandomFloat( 0.55f, 0.7f ) + 0.35f * size;
		vec3_t org = { a[0], a[1], a[2] + 22.0f * scale };
		TEMPENTITY *t = gEngfuncs.pEfxAPI->R_DefaultSprite( org, drops, 30.0f - 2.0f * size );
		if( t )
		{
			t->entity.curstate.scale = scale;
			t->entity.curstate.rendermode = kRenderTransAdd;
			t->entity.curstate.renderamt = 255;
		}
	}
	int puff = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/Puff1.spr" );
	if( puff )
	{
		vec3_t org = { a[0], a[1], a[2] + 4.0f }, vel = { 0.0f, 0.0f, 20.0f };
		TEMPENTITY *t = gEngfuncs.pEfxAPI->R_TempSprite( org, vel, 0.3f + 0.25f * size, puff, kRenderTransAlpha, kRenderFxNone, 0.4f,
			0.7f + 0.15f * size, FTENT_SPRANIMATE | FTENT_FADEOUT );
		if( t )
		{
			t->entity.curstate.rendercolor.r = 215;
			t->entity.curstate.rendercolor.g = 225;
			t->entity.curstate.rendercolor.b = 230;
			t->entity.angles[2] = gEngfuncs.pfnRandomLong( 0, 359 );
		}
	}
	if( size )
		return;		// a body: its wade sound is the movement code's
	char plip[32];
	snprintf( plip, sizeof( plip ), "player/pl_slosh%d.wav", gEngfuncs.pfnRandomLong( 1, 4 ));
	gEngfuncs.pEventAPI->EV_PlaySound( -1, (float *)a, 0, plip, 0.5f, ATTN_NORM, 0, gEngfuncs.pfnRandomLong( 110, 130 ));
}

//
// explosions near the player
//
static float g_shakeAmp, g_shakeEnd, g_shakeStart;

static int __MsgFunc_SCBlast( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	vec3_t org, dir, fwd, right, up;
	org[0] = READ_COORD(); org[1] = READ_COORD(); org[2] = READ_COORD();
	float power = READ_BYTE() * 0.1f;
	if( cl_blastfx && !cl_blastfx->value )
		return 1;
	VectorSubtract( org, v_origin, dir );
	float k = 1.0f - VectorNormalize( dir ) / ( 200.0f + 160.0f * power );
	if( k <= 0.0f )
		return 1;
	k *= k;
	pmtrace_t *tr = gEngfuncs.PM_TraceLine( v_origin, org, PM_TRACELINE_PHYSENTSONLY, 2, -1 );
	bool seen = tr && tr->fraction > 0.95f;
	float now = gEngfuncs.GetClientTime();
	float amp = 14.0f * k * ( seen ? 1.0f : 0.6f );
	if( amp > g_shakeAmp * Q_max( 0.0f, ( g_shakeEnd - now ) / Q_max( g_shakeEnd - g_shakeStart, 0.01f )))
	{
		g_shakeAmp = amp;
		g_shakeStart = now;
		g_shakeEnd = now + 0.4f + 1.2f * k;
	}
	AngleVectors( v_angles, fwd, right, up );
	V_PunchAxis( 0, -( 2.0f + 8.0f * k ));
	V_PunchAxis( 2, ( DotProduct( dir, right ) > 0.0f ? -5.0f : 5.0f ) * k );
	float facing = DotProduct( dir, fwd );
	if( seen && facing > -0.2f )
	{
		screenfade_t sf;
		memset( &sf, 0, sizeof( sf ));
		float len = 0.25f + 0.5f * k;
		sf.fader = 255; sf.fadeg = 228; sf.fadeb = 185;
		sf.fadealpha = (byte)Q_min( 255.0f, 200.0f * k * Q_max( 0.35f, facing ));
		sf.fadeEnd = sf.fadeTotalEnd = sf.fadeReset = now + len;
		sf.fadeSpeed = sf.fadealpha / len;
		sf.fadeFlags = FFADE_IN;
		gEngfuncs.pfnSetScreenFade( &sf );
	}
	if( k > 0.2f )
	{
		// the ears: dulled and ringing for a few seconds (the engine's mixer, s_mix.c, lets them recover)
		float ears = Q_min( 1.0f, ( k - 0.2f ) * 1.25f ) * ( seen ? 1.0f : 0.7f );
		gEngfuncs.Cvar_SetValue( "s_muffle", Q_max( ears, gEngfuncs.pfnGetCvarFloat( "s_muffle" )));
		gEngfuncs.Cvar_SetValue( "s_earring", Q_max( ears, gEngfuncs.pfnGetCvarFloat( "s_earring" )));
	}
	return 1;
}

// called by view.cpp for the eye (scale 1) and the gun (a little less): a jolt that dies away
void SC_ApplyBlastShake( float *origin, float *angles, float scale )
{
	static float frameTime = -1.0f;
	static vec3_t ofs, ang;
	float now = gEngfuncs.GetClientTime();
	if( now >= g_shakeEnd || g_shakeAmp <= 0.0f )
		return;
	if( now != frameTime )
	{
		float fade = ( g_shakeEnd - now ) / Q_max( g_shakeEnd - g_shakeStart, 0.01f );
		float a = g_shakeAmp * fade * fade;
		for( int i = 0; i < 3; i++ )
			ofs[i] = gEngfuncs.pfnRandomFloat( -a, a ) * 0.5f;
		ang[0] = gEngfuncs.pfnRandomFloat( -a, a ) * 0.25f;
		ang[1] = gEngfuncs.pfnRandomFloat( -a, a ) * 0.15f;
		ang[2] = gEngfuncs.pfnRandomFloat( -a, a ) * 0.35f;
		frameTime = now;
	}
	for( int i = 0; i < 3; i++ )
	{
		origin[i] += ofs[i] * scale;
		angles[i] += ang[i] * scale;
	}
}

//
// fog (the server's env_fog / sc_fog: dlls/svencraft/sc_fog.cpp), applied every frame before the world is drawn
//
static struct
{
	int on, sky;
	float col[3], start, end;
} g_SCFogState;

static int __MsgFunc_SCFog( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	g_SCFogState.on = READ_BYTE();
	g_SCFogState.col[0] = READ_BYTE();
	g_SCFogState.col[1] = READ_BYTE();
	g_SCFogState.col[2] = READ_BYTE();
	g_SCFogState.start = READ_SHORT();
	g_SCFogState.end = READ_SHORT();
	g_SCFogState.sky = READ_BYTE();
	return 1;
}

void SC_ApplyFog( void )
{
	static bool was;
	if( g_SCFogState.on )
	{
		gEngfuncs.pTriAPI->Fog( g_SCFogState.col, g_SCFogState.start, g_SCFogState.end, 1 );
		gEngfuncs.pTriAPI->FogParams( 0.0f, g_SCFogState.sky );	// linear; the sky clear unless the map asks
		was = true;
	}
	else if( was )
	{
		gEngfuncs.pTriAPI->Fog( g_SCFogState.col, 0.0f, 0.0f, 0 );
		was = false;
	}
}

//
// sc_emitter (dlls/svencraft/sc_emitter.cpp): steam, smoke or fire puffs from a point while it is in view.
// The entity carries kind (skin), puffs a second (body), size (scale), tint (rendercolor) and opacity (renderamt).
//
static float g_flEmitNext[2048];

// a puff grows as it rises and slows down, and the air carries it a little
static void SC_PuffThink( struct tempent_s *t, float frametime, float currenttime )
{
	float grow = t->entity.curstate.iuser1 * 0.01f;
	t->entity.curstate.scale += grow * frametime;
	for( int k = 0; k < 2; k++ )
		t->entity.baseline.origin[k] *= 1.0f - 0.4f * frametime;
	t->entity.baseline.origin[0] += 6.0f * frametime;	// a light breeze
	t->entity.baseline.origin[2] *= 1.0f - 0.25f * frametime;
	// tempents move by baseline.origin as velocity
	for( int k = 0; k < 3; k++ )
		t->entity.origin[k] += t->entity.baseline.origin[k] * frametime;
}

void SC_EmitterThink( struct cl_entity_s *ent )
{
	int idx = ent->index;
	if( idx < 0 || idx >= 2048 || !ent->curstate.modelindex )
		return;
	float now = gEngfuncs.GetClientTime();
	float rate = ent->curstate.body > 0 ? (float)ent->curstate.body : 6.0f;
	if( g_flEmitNext[idx] > now + 1.0f || now - g_flEmitNext[idx] > 0.5f )
		g_flEmitNext[idx] = now;	// a new map, or back in view after a while
	int kind = ent->curstate.skin;
	while( g_flEmitNext[idx] <= now )
	{
		g_flEmitNext[idx] += 1.0f / rate;
		vec3_t org, vel = { 0, 0, 0 };
		VectorCopy( ent->origin, org );
		org[0] += gEngfuncs.pfnRandomFloat( -4.0f, 4.0f );
		org[1] += gEngfuncs.pfnRandomFloat( -4.0f, 4.0f );
		float life, rise, grow;
		int mode;
		if( kind == 2 )		{ life = 0.8f; rise = 50.0f; grow = 15; mode = kRenderTransAdd; }	// fire
		else if( kind == 1 )	{ life = 4.0f; rise = 55.0f; grow = 30; mode = kRenderTransAlpha; }	// smoke
		else			{ life = 2.6f; rise = 22.0f; grow = 30; mode = kRenderTransAlpha; }	// steam: soft wisps (blended, white on a bright street too)
		vel[0] = gEngfuncs.pfnRandomFloat( -8.0f, 8.0f );
		vel[1] = gEngfuncs.pfnRandomFloat( -8.0f, 8.0f );
		vel[2] = rise * gEngfuncs.pfnRandomFloat( 0.8f, 1.2f );
		float scale = ent->curstate.scale > 0.0f ? ent->curstate.scale : 0.6f;
		TEMPENTITY *t = gEngfuncs.pEfxAPI->R_TempSprite( org, vel, scale * gEngfuncs.pfnRandomFloat( 0.8f, 1.1f ), ent->curstate.modelindex,
			mode, kRenderFxNone, ent->curstate.renderamt / 255.0f, life * gEngfuncs.pfnRandomFloat( 0.85f, 1.15f ), FTENT_SPRANIMATE | FTENT_FADEOUT );
		if( !t )
			return;
		t->entity.curstate.rendercolor = ent->curstate.rendercolor;
		t->entity.curstate.framerate = 12.0f;
		t->entity.curstate.iuser1 = (int)grow;
		t->entity.angles[2] = gEngfuncs.pfnRandomFloat( 0.0f, 359.0f );
		VectorCopy( vel, t->entity.baseline.origin );
		VectorClear( t->entity.curstate.velocity );
		t->flags |= FTENT_CLIENTCUSTOM;
		t->flags &= ~FTENT_GRAVITY;
		t->callback = SC_PuffThink;
	}
}
