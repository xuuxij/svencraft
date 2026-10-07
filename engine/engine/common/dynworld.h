/*
dynworld.h - engine-internal destructible map geometry (see common/dynworld_api.h)
*/
#ifndef DYNWORLD_H
#define DYNWORLD_H

#include "dynworld_api.h"

const dynworld_t *Dyn_World( void );
qboolean Dyn_Active( void );
void Dyn_Clear( void );
qboolean Dyn_LoadForMap( const char *mapname, model_t *world );
qboolean Dyn_PointSolid( const vec3_t p );
qboolean Dyn_BoxSolid( const vec3_t absmin, const vec3_t absmax );
float Dyn_BoxVolume( const vec3_t mins, const vec3_t maxs, float *volume, int maxmaterials );
float Dyn_CarveBox( const vec3_t mins, const vec3_t maxs, float *volume, int maxmaterials );
void Dyn_ClipTrace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, trace_t *trace, edict_t *world );
void Dyn_ClipPMTrace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, pmtrace_t *trace );
const dyncell_t *Dyn_GetCell( int ci, int *budget );
void Dyn_ModelLight( const float *p, byte *rgb, float *lightdir );
const char *Dyn_TraceTexture( const vec3_t start, const vec3_t end, float *fraction );
const char *World_TraceTexture( const float *start, const float *end );	// pm_trace.c: blocks and diggable faces

#endif // DYNWORLD_H
