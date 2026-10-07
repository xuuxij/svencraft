/*
gl_dynworld.c - draws Svencraft's diggable map geometry

The engine keeps the meshes and lightmaps (per 256-unit cell) and rebuilds them after digging; this draws the
visible cells the way Half-Life draws its world: the textures (loaded by name from the WADs), then the cell's
lightmap multiplied over them, doubled (Half-Life's overbright). Cells within reach of a torch's light get a
third, additive pass: the torch light per vertex, where it is brighter than what is there already (gl_voxel.c
floods the light).

Dynamic lights (muzzle flashes, explosions, the flashlight) are added per pixel in a last pass, here and in the
block world (gl_voxel.c), with GoldSrc's falloff for lightmapped dlights.

Decals (bullet holes, blood, scorch marks) shot at this geometry are clipped to its triangles near the hit and
kept per cell; they are drawn between the textures and the lightmap, so they are lit like the wall under them,
and fitted again to a cell that was dug.
*/
#include "gl_local.h"
#include "dynworld_api.h"

#define DYNR_REBUILDS_PER_FRAME	3
#define DYNR_RELIGHTS_PER_FRAME	6
#define DYNR_MAX_DECALS		1024
#define DYNR_MAX_FRAGS		4096
#define DYNR_FRAG_VERTS		10
#define DYNR_DECAL_DIST		4.0f	// how far off the hit a surface can be and still get the decal
static const float dyn_torch_color[3] = { 1.0f, 0.84f, 0.6f };

typedef struct
{
	unsigned int	meshrev, lightrev;
	int		numverts;
	byte		*rgba;		// torch light to add, per vertex
	unsigned int	lmrev;		// the mesh revision the lightmap texture is from
	int		lmtex;
	int		frags;		// first decal fragment on this cell (-1: none)
	unsigned int	fragrev;	// the mesh revision the fragments were cut from
} dynlit_t;

typedef struct
{
	vec3_t	pos, normal;
	vec3_t	axis[2];	// the decal's right and down, world units per whole decal folded in below
	float	size[2];
	int	texture;
	qboolean	used;
} dyndecal_t;

typedef struct
{
	int	decal, cell;
	int	prev, next;	// in the cell's list
	int	numverts;
	float	v[DYNR_FRAG_VERTS][5];	// xyz, st
} dynfrag_t;

static dyndecal_t	dyn_decals[DYNR_MAX_DECALS];
static dynfrag_t	dyn_frags[DYNR_MAX_FRAGS];
static int		dyn_nextdecal, dyn_nextfrag;

static struct
{
	unsigned int	world_rev;
	int		texnums[DYN_MAX_TEXTURES];
	qboolean	loaded;
	dynlit_t	*lit;		// per cell
	int		numlit;
} dr;

static void R_DynUnlinkFrag( int f )
{
	dynfrag_t *fr = &dyn_frags[f];
	if( fr->decal < 0 )
		return;
	if( fr->prev >= 0 )
		dyn_frags[fr->prev].next = fr->next;
	else if( dr.lit && fr->cell < dr.numlit )
		dr.lit[fr->cell].frags = fr->next;
	if( fr->next >= 0 )
		dyn_frags[fr->next].prev = fr->prev;
	fr->decal = -1;
}

void R_DynClearDecals( void )
{
	for( int i = 0; i < DYNR_MAX_FRAGS; i++ )
		dyn_frags[i].decal = -1;
	for( int i = 0; i < DYNR_MAX_DECALS; i++ )
		dyn_decals[i].used = false;
	for( int i = 0; i < dr.numlit; i++ )
		dr.lit[i].frags = -1;
	dyn_nextdecal = dyn_nextfrag = 0;
}

// keep the part of a polygon (xyz, st) where sign * (v[k] - val) >= 0
static int R_DynClipFrag( float in[][5], int n, float out[][5], int k, float val, float sign )
{
	int m = 0;
	for( int i = 0; i < n; i++ )
	{
		const float *a = in[i], *b = in[( i + 1 ) % n];
		float da = sign * ( a[k] - val ), db = sign * ( b[k] - val );
		if( da >= 0 && m < DYNR_FRAG_VERTS )
			memcpy( out[m++], a, sizeof( float ) * 5 );
		if(( da >= 0 ) != ( db >= 0 ) && m < DYNR_FRAG_VERTS )
		{
			float f = da / ( da - db );
			for( int j = 0; j < 5; j++ )
				out[m][j] = a[j] + ( b[j] - a[j] ) * f;
			m++;
		}
	}
	return m;
}

// the decal's pieces on one cell's triangles
static void R_DynCutDecal( int di, int ci, const dyncell_t *c )
{
	const dyndecal_t *d = &dyn_decals[di];
	for( int v = 0; v + 2 < c->numverts; v += 3 )
	{
		const dynvert_t *t = &c->verts[v];
		vec3_t e1, e2, n;
		float poly[DYNR_FRAG_VERTS][5], tmp[DYNR_FRAG_VERTS][5];
		int num = 3;

		VectorSubtract( t[1].xyz, t[0].xyz, e1 );
		VectorSubtract( t[2].xyz, t[0].xyz, e2 );
		CrossProduct( e1, e2, n );
		if( VectorNormalizeLength( n ) < 0.0001f )
			continue;
		if( fabs( DotProduct( n, d->normal )) < 0.7f )
			continue;	// a face turned another way (the decal wraps no corners, like Half-Life's)
		if( fabs( DotProduct( d->pos, n ) - DotProduct( t[0].xyz, n )) > DYNR_DECAL_DIST )
			continue;
		for( int k = 0; k < 3; k++ )
		{
			vec3_t rel;
			VectorSubtract( t[k].xyz, d->pos, rel );
			VectorCopy( t[k].xyz, poly[k] );
			poly[k][3] = DotProduct( rel, d->axis[0] ) / d->size[0] + 0.5f;
			poly[k][4] = DotProduct( rel, d->axis[1] ) / d->size[1] + 0.5f;
		}
		if(( poly[0][3] < 0 && poly[1][3] < 0 && poly[2][3] < 0 ) || ( poly[0][3] > 1 && poly[1][3] > 1 && poly[2][3] > 1 )
			|| ( poly[0][4] < 0 && poly[1][4] < 0 && poly[2][4] < 0 ) || ( poly[0][4] > 1 && poly[1][4] > 1 && poly[2][4] > 1 ))
			continue;
		num = R_DynClipFrag( poly, num, tmp, 3, 0.0f, 1.0f );
		num = R_DynClipFrag( tmp, num, poly, 3, 1.0f, -1.0f );
		num = R_DynClipFrag( poly, num, tmp, 4, 0.0f, 1.0f );
		num = R_DynClipFrag( tmp, num, poly, 4, 1.0f, -1.0f );
		if( num < 3 )
			continue;

		// the oldest fragment makes room
		int f = dyn_nextfrag;
		dyn_nextfrag = ( dyn_nextfrag + 1 ) % DYNR_MAX_FRAGS;
		R_DynUnlinkFrag( f );
		dynfrag_t *fr = &dyn_frags[f];
		fr->decal = di;
		fr->cell = ci;
		fr->numverts = num;
		memcpy( fr->v, poly, sizeof( float ) * 5 * num );
		fr->prev = -1;
		fr->next = dr.lit[ci].frags;
		if( fr->next >= 0 )
			dyn_frags[fr->next].prev = f;
		dr.lit[ci].frags = f;
	}
}

// a decal shot at the world: placed on the diggable geometry when the hit is on it
qboolean R_DynDecalShoot( int texture, const vec3_t pos, float scale )
{
	const dynworld_t *w = gEngfuncs.Dyn_World ? gEngfuncs.Dyn_World( ) : NULL;
	int width = 1, height = 1, c0[3], c1[3];
	float best = DYNR_DECAL_DIST;
	vec3_t normal;

	if( !w || !w->active || !dr.lit || dr.world_rev != w->world_rev )
		return false;
	R_GetTextureParms( &width, &height, texture );
	scale = bound( 0.01f, scale, 16.0f );
	float size = Q_max( width, height ) / scale * 0.5f;

	// the face under the hit: the nearest triangle the point is over
	for( int k = 0; k < 3; k++ )
	{
		c0[k] = Q_max( 0, (int)floor(( pos[k] - size - DYNR_DECAL_DIST - w->gridmins[k] ) / DYN_CELL_SIZE ));
		c1[k] = Q_min( w->gridsize[k] - 1, (int)floor(( pos[k] + size + DYNR_DECAL_DIST - w->gridmins[k] ) / DYN_CELL_SIZE ));
	}
	VectorClear( normal );
	for( int z = c0[2]; z <= c1[2]; z++ )
		for( int y = c0[1]; y <= c1[1]; y++ )
			for( int x = c0[0]; x <= c1[0]; x++ )
			{
				const dyncell_t *c = &w->cells[( z * w->gridsize[1] + y ) * w->gridsize[0] + x];
				for( int v = 0; v + 2 < c->numverts; v += 3 )
				{
					const dynvert_t *t = &c->verts[v];
					vec3_t e1, e2, n, ep, cr;
					qboolean inside = true;
					VectorSubtract( t[1].xyz, t[0].xyz, e1 );
					VectorSubtract( t[2].xyz, t[0].xyz, e2 );
					CrossProduct( e1, e2, n );
					if( VectorNormalizeLength( n ) < 0.0001f )
						continue;
					float dist = fabs( DotProduct( pos, n ) - DotProduct( t[0].xyz, n ));
					if( dist >= best )
						continue;
					for( int k = 0; k < 3 && inside; k++ )
					{
						VectorSubtract( t[( k + 1 ) % 3].xyz, t[k].xyz, ep );
						VectorSubtract( pos, t[k].xyz, e2 );
						CrossProduct( ep, e2, cr );
						if( DotProduct( cr, n ) < -0.5f )
							inside = false;
					}
					if( !inside )
						continue;
					best = dist;
					VectorCopy( n, normal );
				}
			}
	if( VectorIsNull( normal ))
		return false;

	// faces are two-sided: the decal faces the viewer (who is nearly always the one who shot it)
	vec3_t toview;
	VectorSubtract( RI.rvp.vieworigin, pos, toview );
	if( DotProduct( toview, normal ) < 0 )
		VectorNegate( normal, normal );

	int di = dyn_nextdecal;
	dyn_nextdecal = ( dyn_nextdecal + 1 ) % DYNR_MAX_DECALS;
	for( int f = 0; f < DYNR_MAX_FRAGS; f++ )
		if( dyn_frags[f].decal == di )
			R_DynUnlinkFrag( f );
	dyndecal_t *d = &dyn_decals[di];
	vec3_t up = { 0, 0, 1 }, fwd;
	if( fabs( normal[2] ) > 0.7f )
		VectorSet( up, 0, 1, 0 );
	VectorCopy( pos, d->pos );
	VectorCopy( normal, d->normal );
	// right = forward x up, looking at the face (forward = -normal); down is the texture's t
	VectorNegate( normal, fwd );
	VectorMA( up, -DotProduct( up, normal ), normal, up );
	VectorNormalize( up );
	CrossProduct( fwd, up, d->axis[0] );
	VectorNegate( up, d->axis[1] );
	d->size[0] = width / scale;
	d->size[1] = height / scale;
	d->texture = texture;
	d->used = true;

	for( int z = c0[2]; z <= c1[2]; z++ )
		for( int y = c0[1]; y <= c1[1]; y++ )
			for( int x = c0[0]; x <= c1[0]; x++ )
			{
				int ci = ( z * w->gridsize[1] + y ) * w->gridsize[0] + x;
				R_DynCutDecal( di, ci, &w->cells[ci] );
			}
	return true;
}

// a cell dug since its decals were cut: fit them to it again (gone with the wall they were on)
static void R_DynRefitDecals( int ci, const dyncell_t *c )
{
	int ids[256], num = 0;
	for( int f = dr.lit[ci].frags, next; f >= 0; f = next )
	{
		int k;
		next = dyn_frags[f].next;
		for( k = 0; k < num && ids[k] != dyn_frags[f].decal; k++ );
		if( k == num && num < 256 )
			ids[num++] = dyn_frags[f].decal;
		R_DynUnlinkFrag( f );
	}
	for( int k = 0; k < num; k++ )
		R_DynCutDecal( ids[k], ci, c );
	dr.lit[ci].fragrev = c->rev;
}

static void R_DynDrawDecals( int ci, const dyncell_t *c )
{
	if( dr.lit[ci].frags >= 0 && dr.lit[ci].fragrev != c->rev )
		R_DynRefitDecals( ci, c );
	dr.lit[ci].fragrev = c->rev;
	if( dr.lit[ci].frags < 0 )
		return;

	pglEnable( GL_BLEND );
	pglDepthMask( GL_FALSE );
	pglDepthFunc( GL_LEQUAL );
	if( gl_polyoffset.value )
		GL_PushPolygonOffset( -1.0f, -gl_polyoffset.value );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	for( int f = dr.lit[ci].frags; f >= 0; f = dyn_frags[f].next )
	{
		const dynfrag_t *fr = &dyn_frags[f];
		const dyndecal_t *d = &dyn_decals[fr->decal];
		GL_Bind( XASH_TEXTURE0, d->texture );
		if( FBitSet( R_GetTexture( d->texture )->flags, TF_PREMULTIPLIED ))
			pglBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
		else
			pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglBegin( GL_POLYGON );
		for( int i = 0; i < fr->numverts; i++ )
		{
			pglTexCoord2f( fr->v[i][3], fr->v[i][4] );
			pglVertex3fv( fr->v[i] );
		}
		pglEnd();
	}
	if( gl_polyoffset.value )
		GL_PopPolygonOffset();
}

static void R_DynFreeLit( void )
{
	for( int i = 0; i < dr.numlit; i++ )
	{
		free( dr.lit[i].rgba );
		if( dr.lit[i].lmtex )
			GL_FreeTexture( dr.lit[i].lmtex );
	}
	free( dr.lit );
	dr.lit = NULL;
	dr.numlit = 0;
	R_DynClearDecals();
}

void R_DynNewMap( void )
{
	dr.loaded = false;
	dr.world_rev = 0;
	R_DynFreeLit();
}

// the cell's lightmap as a texture, uploaded again after the cell was rebuilt
static int R_DynLightmap( int index, const dyncell_t *c )
{
	dynlit_t *l = &dr.lit[index];

	if( l->lmtex && l->lmrev == c->rev )
		return l->lmtex;
	if( l->lmtex )
		GL_FreeTexture( l->lmtex );
	l->lmtex = 0;
	if( c->lightmap && c->lmsize[0] > 0 && c->lmsize[1] > 0 )
	{
		char name[32];
		Q_snprintf( name, sizeof( name ), "*dynlm%d", index );
		l->lmtex = GL_CreateTexture( name, c->lmsize[0], c->lmsize[1], c->lightmap, TF_NOMIPMAP|TF_CLAMP );
	}
	l->lmrev = c->rev;
	return l->lmtex;
}

// torch light per vertex: each vertex looks a few units out from its triangle's face
static const byte *R_DynTorchColors( int index, const dyncell_t *c, int *budget )
{
	dynlit_t *l = &dr.lit[index];

	if( l->rgba && l->meshrev == c->rev && l->lightrev == R_VoxLightRev() && l->numverts == c->numverts )
		return l->rgba;
	if( *budget <= 0 )
		return l->rgba && l->numverts == c->numverts && l->meshrev == c->rev ? l->rgba : NULL;	// stale light for a frame or two
	( *budget )--;
	if( l->numverts != c->numverts || !l->rgba )
	{
		free( l->rgba );
		l->rgba = malloc( c->numverts * 4 );
		if( !l->rgba )
			return NULL;
		l->numverts = c->numverts;
	}
	for( int v = 0; v + 2 < c->numverts; v += 3 )
	{
		vec3_t e1, e2, n;
		VectorSubtract( c->verts[v + 1].xyz, c->verts[v].xyz, e1 );
		VectorSubtract( c->verts[v + 2].xyz, c->verts[v].xyz, e2 );
		CrossProduct( e1, e2, n );
		if( VectorNormalizeLength( n ) < 0.0001f )
			VectorSet( n, 0, 0, 1 );
		for( int k = 0; k < 3; k++ )
		{
			const dynvert_t *dv = &c->verts[v + k];
			byte *out = &l->rgba[( v + k ) * 4];
			vec3_t q;
			float t;

			VectorMA( dv->xyz, 6.0f, n, q );
			t = R_VoxLightAt( q );
			if( t <= 0.0f )
			{
				// the faces are two-sided: try the other side before giving up
				VectorMA( dv->xyz, -6.0f, n, q );
				t = R_VoxLightAt( q );
			}
			// warm light added where the torch is brighter than what is there already (a plain per-channel
			// maximum would turn a blue-grey shadow pink)
			float lum = 0.3f * dv->rgba[0] + 0.59f * dv->rgba[1] + 0.11f * dv->rgba[2];
			for( int ch = 0; ch < 3; ch++ )
				out[ch] = (byte)Q_min( 255.0f, Q_max( 0.0f, 225.0f * t * dyn_torch_color[ch] - lum ));
			out[3] = 255;
		}
	}
	l->meshrev = c->rev;
	l->lightrev = R_VoxLightRev();
	return l->rgba;
}

// the dynamic lights alive this frame (not the dark ones)
int R_GatherDlights( const dlight_t **out, int max )
{
	int n = 0;
	for( int i = 0; i < MAX_DLIGHTS && n < max; i++ )
	{
		const dlight_t *l = &gp_dlights[i];
		if( l->die < gp_cl->time || l->radius <= 0.0f || l->dark )
			continue;
		out[n++] = l;
	}
	return n;
}

static qboolean R_IsFlashlight( const dlight_t *l );

qboolean R_DlightsTouch( const dlight_t **lights, int num, const vec3_t mins, const vec3_t maxs )
{
	for( int i = 0; i < num; i++ )
	{
		const dlight_t *l = lights[i];
		if( R_IsFlashlight( l ))
		{
			// the beam reaches from the eye: any box in its range and in front (R_SpotDraw does the cone)
			const float *eye = RI.rvp.vieworigin, *fwd = RI.vforward;
			vec3_t c, half;
			for( int k = 0; k < 3; k++ )
			{
				c[k] = ( mins[k] + maxs[k] ) * 0.5f - eye[k];
				half[k] = ( maxs[k] - mins[k] ) * 0.5f;
			}
			float reach = VectorLength( half );
			if( VectorLength( c ) - reach < 700.0f && DotProduct( c, fwd ) > -reach )
				return true;
			continue;
		}
		if( l->origin[0] + l->radius >= mins[0] && l->origin[0] - l->radius <= maxs[0] && l->origin[1] + l->radius >= mins[1]
			&& l->origin[1] - l->radius <= maxs[1] && l->origin[2] + l->radius >= mins[2] && l->origin[2] - l->radius <= maxs[2] )
			return true;
	}
	return false;
}

// GoldSrc's dlight falloff across a face, as a texture: full in the middle, linear to nothing at the edge
static int R_DlightTexture( void )
{
	static int tex;
	if( !tex || !R_GetTexture( tex )->texnum )
	{
		byte pix[64 * 64 * 4];
		for( int y = 0; y < 64; y++ )
			for( int x = 0; x < 64; x++ )
			{
				float dx = ( x + 0.5f ) / 32.0f - 1.0f, dy = ( y + 0.5f ) / 32.0f - 1.0f;
				byte v = (byte)( Q_max( 0.0f, 1.0f - sqrt( dx * dx + dy * dy )) * 255.0f );
				byte *p = &pix[( y * 64 + x ) * 4];
				p[0] = p[1] = p[2] = v;
				p[3] = 255;
			}
		tex = GL_CreateTexture( "*dlightfalloff", 64, 64, pix, TF_NOMIPMAP|TF_CLAMP );
	}
	return tex;
}

typedef struct
{
	float	xyz[3];
	float	st[2];		// the surface's texture
	float	fall[2];	// the falloff texture
	byte	rgba[4];
} dlvert_t;

// the local player's flashlight is a cone from the eye, not GoldSrc's ball of light where the view trace ends
// (cl_tent.c CL_UpdateFlashlight still places that dlight: the BSP world and the models keep using it)
#define SPOT_TAN	0.40f		// the cone's half-angle, about 22 degrees
#define SPOT_RANGE	700.0f

static qboolean R_IsFlashlight( const dlight_t *l )
{
	return l->key == gp_cl->playernum + 1 && l->radius == 80.0f;
}

// the flashlight's spot: a bright middle, soft at the rim
static int R_SpotTexture( void )
{
	static int tex;
	if( !tex || !R_GetTexture( tex )->texnum )
	{
		byte pix[64 * 64 * 4];
		for( int y = 0; y < 64; y++ )
			for( int x = 0; x < 64; x++ )
			{
				float dx = ( x + 0.5f ) / 32.0f - 1.0f, dy = ( y + 0.5f ) / 32.0f - 1.0f;
				float r = sqrt( dx * dx + dy * dy );
				float v = bound( 0.0f, ( 0.96f - r ) / 0.3f, 1.0f ) * ( 0.7f + 0.3f * Q_max( 0.0f, 1.0f - r ));
				byte *p = &pix[( y * 64 + x ) * 4];
				p[0] = p[1] = p[2] = (byte)( v * 255.0f );
				p[3] = 255;
			}
		tex = GL_CreateTexture( "*flashlightspot", 64, 64, pix, TF_NOMIPMAP|TF_CLAMP );
	}
	return tex;
}

typedef struct
{
	float	xyz[3];
	float	st[2];
	float	proj[4];	// the spot projected from the eye: s, t, 0, q (divided per pixel)
	byte	rgba[4];
} spotvert_t;

static void R_SpotDraw( const dlight_t *l, const float *xyz, const float *st, int stride, int numverts, int texture, qboolean alphatest )
{
	static spotvert_t *out;
	static int outmax;
	int n = 0;
	const float *eye = RI.rvp.vieworigin, *fwd = RI.vforward, *right = RI.vright, *up = RI.vup;

	if( outmax < numverts )
	{
		free( out );
		outmax = numverts * 2;
		out = malloc( sizeof( spotvert_t ) * outmax );
		if( !out )
		{
			outmax = 0;
			return;
		}
	}
	for( int v = 0; v + 2 < numverts && n + 3 <= outmax; v += 3 )
	{
		const float *p[3], *t[3];
		float along[3], sx[3], sy[3];
		qboolean behind = false, past = true, outside[4] = { true, true, true, true };
		for( int k = 0; k < 3; k++ )
		{
			vec3_t rel;
			p[k] = (const float *)((const byte *)xyz + ( v + k ) * stride );
			t[k] = (const float *)((const byte *)st + ( v + k ) * stride );
			VectorSubtract( p[k], eye, rel );
			along[k] = DotProduct( rel, fwd );
			sx[k] = DotProduct( rel, right );
			sy[k] = DotProduct( rel, up );
			if( along[k] < 2.0f )
				behind = true;
			if( along[k] < SPOT_RANGE )
				past = false;
			float lim = along[k] * SPOT_TAN;
			if( sx[k] < lim ) outside[0] = false;
			if( sx[k] > -lim ) outside[1] = false;
			if( sy[k] < lim ) outside[2] = false;
			if( sy[k] > -lim ) outside[3] = false;
		}
		if( behind || past || outside[0] || outside[1] || outside[2] || outside[3] )
			continue;
		// facing the beam lights brighter (faces are two-sided)
		vec3_t e1, e2, nrm, mid;
		VectorSubtract( p[1], p[0], e1 );
		VectorSubtract( p[2], p[0], e2 );
		CrossProduct( e1, e2, nrm );
		if( VectorNormalizeLength( nrm ) < 0.0001f )
			continue;
		for( int k = 0; k < 3; k++ )
			mid[k] = ( p[0][k] + p[1][k] + p[2][k] ) / 3.0f - eye[k];
		VectorNormalize( mid );
		float lam = 0.35f + 0.65f * fabs( DotProduct( nrm, mid ));
		for( int k = 0; k < 3; k++ )
		{
			spotvert_t *o = &out[n + k];
			float q = along[k] * SPOT_TAN;
			float fade = 1.0f - along[k] / SPOT_RANGE;
			fade = fade > 0.0f ? fade * fade : 0.0f;
			float i = 255.0f * lam * fade;
			VectorCopy( p[k], o->xyz );
			o->st[0] = t[k][0];
			o->st[1] = t[k][1];
			o->proj[0] = 0.5f * ( q + sx[k] );
			o->proj[1] = 0.5f * ( q + sy[k] );
			o->proj[2] = 0.0f;
			o->proj[3] = q;
			o->rgba[0] = (byte)Q_min( 255.0f, i );
			o->rgba[1] = (byte)Q_min( 255.0f, i * 0.96f );
			o->rgba[2] = (byte)Q_min( 255.0f, i * 0.88f );
			o->rgba[3] = 255;
		}
		n += 3;
	}
	if( !n )
		return;

	if( alphatest ) pglEnable( GL_ALPHA_TEST );
	GL_Bind( XASH_TEXTURE0, texture );
	pglVertexPointer( 3, GL_FLOAT, sizeof( spotvert_t ), out[0].xyz );
	pglTexCoordPointer( 2, GL_FLOAT, sizeof( spotvert_t ), out[0].st );
	pglEnableClientState( GL_COLOR_ARRAY );
	pglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( spotvert_t ), out[0].rgba );
	GL_Bind( XASH_TEXTURE1, R_SpotTexture( ));
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglTexCoordPointer( 4, GL_FLOAT, sizeof( spotvert_t ), out[0].proj );
	pglDrawArrays( GL_TRIANGLES, 0, n );
	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	GL_CleanUpTextureUnits( 1 );
	GL_SelectTexture( XASH_TEXTURE0 );
	pglDisableClientState( GL_COLOR_ARRAY );
	if( alphatest ) pglDisable( GL_ALPHA_TEST );
}

// the dynamic lights' added light over a triangle list (Quake 3's way, with GoldSrc's numbers): for every
// triangle in reach of a light, the light's strength on the triangle's plane times the falloff texture laid
// over the plane around the point under the light, times the surface's own texture. The caller sets additive
// blending and depth EQUAL; unit 1 is left off again.
void R_DlightsDraw( const dlight_t **lights, int num, const float *xyz, const float *st, int stride, int numverts, int texture, qboolean alphatest )
{
	static dlvert_t *out;
	static int outmax;
	int n = 0;

	if( outmax < numverts )
	{
		free( out );
		outmax = numverts * 2;
		out = malloc( sizeof( dlvert_t ) * outmax );
		if( !out )
		{
			outmax = 0;
			return;
		}
	}
	for( int i = 0; i < num; i++ )
	{
		const dlight_t *l = lights[i];
		if( R_IsFlashlight( l ))
		{
			R_SpotDraw( l, xyz, st, stride, numverts, texture, alphatest );	// its own pass, after these
			continue;
		}
		for( int v = 0; v + 2 < numverts; v += 3 )
		{
			const float *p[3], *t[3];
			vec3_t e1, e2, nrm, d, impact, u, w, axis = { 0, 0, 1 };
			for( int k = 0; k < 3; k++ )
			{
				p[k] = (const float *)((const byte *)xyz + ( v + k ) * stride );
				t[k] = (const float *)((const byte *)st + ( v + k ) * stride );
			}
			// quick: the triangle's box against the light's
			if( Q_min( p[0][0], Q_min( p[1][0], p[2][0] )) > l->origin[0] + l->radius || Q_max( p[0][0], Q_max( p[1][0], p[2][0] )) < l->origin[0] - l->radius
				|| Q_min( p[0][1], Q_min( p[1][1], p[2][1] )) > l->origin[1] + l->radius || Q_max( p[0][1], Q_max( p[1][1], p[2][1] )) < l->origin[1] - l->radius
				|| Q_min( p[0][2], Q_min( p[1][2], p[2][2] )) > l->origin[2] + l->radius || Q_max( p[0][2], Q_max( p[1][2], p[2][2] )) < l->origin[2] - l->radius )
				continue;
			VectorSubtract( p[1], p[0], e1 );
			VectorSubtract( p[2], p[0], e2 );
			CrossProduct( e1, e2, nrm );
			if( VectorNormalizeLength( nrm ) < 0.0001f )
				continue;
			VectorSubtract( l->origin, p[0], d );
			float pd = DotProduct( d, nrm );	// faces are two-sided: lit from either side
			float rad = l->radius - fabs( pd );
			if( rad <= l->minlight || rad <= 1.0f )
				continue;
			VectorMA( l->origin, -pd, nrm, impact );
			if( fabs( nrm[2] ) > 0.9f )
				VectorSet( axis, 1, 0, 0 );
			CrossProduct( nrm, axis, u );
			VectorNormalize( u );
			CrossProduct( nrm, u, w );
			float sc = 0.5f / rad;
			byte r = (byte)Q_min( 255.0f, rad * l->color.r / 64.0f ), g = (byte)Q_min( 255.0f, rad * l->color.g / 64.0f ), b = (byte)Q_min( 255.0f, rad * l->color.b / 64.0f );
			if( n + 3 > outmax )
				break;
			for( int k = 0; k < 3; k++ )
			{
				dlvert_t *o = &out[n + k];
				vec3_t rel;
				VectorSubtract( p[k], impact, rel );
				VectorCopy( p[k], o->xyz );
				o->st[0] = t[k][0];
				o->st[1] = t[k][1];
				o->fall[0] = DotProduct( rel, u ) * sc + 0.5f;
				o->fall[1] = DotProduct( rel, w ) * sc + 0.5f;
				o->rgba[0] = r; o->rgba[1] = g; o->rgba[2] = b; o->rgba[3] = 255;
			}
			// all of it off one side of the falloff: nothing to add
			if(( out[n].fall[0] < 0 && out[n + 1].fall[0] < 0 && out[n + 2].fall[0] < 0 ) || ( out[n].fall[0] > 1 && out[n + 1].fall[0] > 1 && out[n + 2].fall[0] > 1 )
				|| ( out[n].fall[1] < 0 && out[n + 1].fall[1] < 0 && out[n + 2].fall[1] < 0 ) || ( out[n].fall[1] > 1 && out[n + 1].fall[1] > 1 && out[n + 2].fall[1] > 1 ))
				continue;
			n += 3;
		}
		if( n + 3 > outmax )
			break;
	}
	if( !n )
		return;

	if( alphatest ) pglEnable( GL_ALPHA_TEST );
	GL_Bind( XASH_TEXTURE0, texture );
	pglVertexPointer( 3, GL_FLOAT, sizeof( dlvert_t ), out[0].xyz );
	pglTexCoordPointer( 2, GL_FLOAT, sizeof( dlvert_t ), out[0].st );
	pglEnableClientState( GL_COLOR_ARRAY );
	pglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( dlvert_t ), out[0].rgba );
	GL_Bind( XASH_TEXTURE1, R_DlightTexture( ));
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglTexCoordPointer( 2, GL_FLOAT, sizeof( dlvert_t ), out[0].fall );
	pglDrawArrays( GL_TRIANGLES, 0, n );
	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	GL_CleanUpTextureUnits( 1 );
	GL_SelectTexture( XASH_TEXTURE0 );
	pglDisableClientState( GL_COLOR_ARRAY );
	if( alphatest ) pglDisable( GL_ALPHA_TEST );
}

// fog over passes: the first pass fogs to the fog's colour; a pass that multiplies what is there (the lightmap,
// doubled) must fog to neutral grey and a pass that adds (torch light, dynamic lights) to black, or the fog would
// be counted twice. kind: 0 the fog's own colour, 1 neutral for a 2x multiply, 2 black for an add
void R_FogPass( int kind )
{
	static const float grey[4] = { 0.5f, 0.5f, 0.5f, 1.0f }, black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	if( !pglIsEnabled( GL_FOG ))
		return;
	pglFogfv( GL_FOG_COLOR, kind == 1 ? grey : kind == 2 ? black : RI.fogColor );
}

static void R_DynLoadTextures( const dynworld_t *w )
{
	for( int i = 0; i < w->numtextures; i++ )
	{
		char path[64];

		// WAD lumps are reachable by name; textures embedded in the map win when present
		Q_snprintf( path, sizeof( path ), "%s.mip", w->texnames[i] );
		dr.texnums[i] = GL_LoadTexture( path, NULL, 0, 0 );
		if( !dr.texnums[i] )
		{
			gEngfuncs.Con_Printf( S_WARN "R_DynLoadTextures: can't find texture %s\n", w->texnames[i] );
			dr.texnums[i] = tr.defaultTexture;
		}
	}
	dr.loaded = true;
}

static void R_DynDrawBatches( const dyncell_t *c )
{
	for( int b = 0; b < c->numbatches; b++ )
	{
		const dynbatch_t *bt = &c->batches[b];
		GL_Bind( XASH_TEXTURE0, dr.texnums[bt->texture] );
		pglDrawArrays( GL_TRIANGLES, bt->first, bt->count );
	}
}

void R_DrawDynWorld( void )
{
	const dynworld_t *w;
	int budget = DYNR_REBUILDS_PER_FRAME;

	if( !gEngfuncs.Dyn_World )
		return;
	w = gEngfuncs.Dyn_World( );
	if( !w || !w->active )
		return;

	int n = w->gridsize[0] * w->gridsize[1] * w->gridsize[2];
	int relight = DYNR_RELIGHTS_PER_FRAME;
	const dlight_t *dlights[MAX_DLIGHTS];
	int numdlights = R_GatherDlights( dlights, MAX_DLIGHTS );

	if( !dr.loaded || dr.world_rev != w->world_rev )
	{
		R_DynFreeLit();
		R_DynLoadTextures( w );
		dr.world_rev = w->world_rev;
	}
	if( !dr.lit )
	{
		dr.lit = calloc( n, sizeof( dynlit_t ));
		dr.numlit = dr.lit ? n : 0;
		for( int i = 0; i < dr.numlit; i++ )
			dr.lit[i].frags = -1;
	}
	if( !dr.lit )
		return;

	pglEnable( GL_DEPTH_TEST );
	pglDisable( GL_ALPHA_TEST );
	GL_Cull( GL_NONE );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	pglEnableClientState( GL_VERTEX_ARRAY );
	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	for( int i = 0; i < n; i++ )
	{
		const dyncell_t *c = &w->cells[i];
		vec3_t mins, maxs;

		// a cell holds the triangles whose middles are inside it: they reach at most ~32 units out
		VectorSet( mins, c->mins[0] - 48, c->mins[1] - 48, c->mins[2] - 48 );
		VectorSet( maxs, c->maxs[0] + 48, c->maxs[1] + 48, c->maxs[2] + 48 );
		if( R_CullBox( mins, maxs ))
			continue;

		c = gEngfuncs.Dyn_GetCell( i, &budget );
		if( !c || !c->numverts )
			continue;

		int lmtex = R_DynLightmap( i, c );
		const byte *torch = NULL;
		if( R_VoxLightTouches( c->mins, c->maxs ))
			torch = R_DynTorchColors( i, c, &relight );

		pglVertexPointer( 3, GL_FLOAT, sizeof( dynvert_t ), c->verts[0].xyz );

		// the textures; without a lightmap, lit by the vertex colours instead
		pglDisable( GL_BLEND );
		pglDepthMask( GL_TRUE );
		pglDepthFunc( GL_LEQUAL );
		pglTexCoordPointer( 2, GL_FLOAT, sizeof( dynvert_t ), c->verts[0].st );
		if( lmtex )
			pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		else
		{
			pglEnableClientState( GL_COLOR_ARRAY );
			pglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( dynvert_t ), c->verts[0].rgba );
		}
		R_DynDrawBatches( c );
		if( !lmtex )
			pglDisableClientState( GL_COLOR_ARRAY );

		// decals, under the lightmap
		R_DynDrawDecals( i, c );

		pglEnable( GL_BLEND );
		pglDepthMask( GL_FALSE );
		pglDepthFunc( GL_EQUAL );

		// the lightmap, doubled: dst * src + src * dst
		if( lmtex )
		{
			R_FogPass( 1 );
			pglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
			pglTexCoordPointer( 2, GL_FLOAT, sizeof( dynvert_t ), c->verts[0].lm );
			GL_Bind( XASH_TEXTURE0, lmtex );
			pglDrawArrays( GL_TRIANGLES, 0, c->numverts );
		}

		R_FogPass( 2 );

		// torch light, added
		if( torch )
		{
			pglBlendFunc( GL_ONE, GL_ONE );
			pglTexCoordPointer( 2, GL_FLOAT, sizeof( dynvert_t ), c->verts[0].st );
			pglEnableClientState( GL_COLOR_ARRAY );
			pglColorPointer( 4, GL_UNSIGNED_BYTE, 4, torch );
			R_DynDrawBatches( c );
			pglDisableClientState( GL_COLOR_ARRAY );
		}

		// dynamic lights (muzzle flashes, explosions, the flashlight), added
		if( numdlights && R_DlightsTouch( dlights, numdlights, c->mins, c->maxs ))
		{
			pglBlendFunc( GL_ONE, GL_ONE );
			for( int b = 0; b < c->numbatches; b++ )
			{
				const dynbatch_t *bt = &c->batches[b];
				R_DlightsDraw( dlights, numdlights, c->verts[bt->first].xyz, c->verts[bt->first].st, sizeof( dynvert_t ), bt->count, dr.texnums[bt->texture], false );
			}
		}
		R_FogPass( 0 );
	}

	pglDisable( GL_BLEND );
	pglDepthMask( GL_TRUE );
	pglDepthFunc( GL_LEQUAL );
	pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	GL_Cull( GL_FRONT );
}
