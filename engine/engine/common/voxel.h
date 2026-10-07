/*
voxel.h - engine-internal block world functions (see voxel_api.h for the shared types)
*/
#ifndef VOXEL_H
#define VOXEL_H

#include "voxel_api.h"

typedef struct vox_trace_s
{
	qboolean	startsolid;
	qboolean	allsolid;
	float	fraction;
	vec3_t	endpos;
	vec3_t	normal;
	int	block;
	int	cell[3];
} vox_trace_t;

const vox_world_t *Vox_World( void );
qboolean Vox_Active( void );
void Vox_Clear( void );
qboolean Vox_TraceBox( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, vox_trace_t *tr );
qboolean Vox_BoxSolid( const vec3_t absmin, const vec3_t absmax );
qboolean Vox_PointSolid( const vec3_t p );
int Vox_TraceBlock( const vec3_t start, const vec3_t end, float *fraction );
void Vox_ClipTrace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, trace_t *trace, edict_t *world );
void Vox_ClipPMTrace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, pmtrace_t *trace );

#endif // VOXEL_H
