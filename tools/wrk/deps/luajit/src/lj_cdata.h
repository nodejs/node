/*
** C data management.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_CDATA_H
#define _LJ_CDATA_H

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_ctype.h"

#if LJ_HASFFI

/* Get C data pointer. */
static LJ_AINLINE void *cdata_getptr(void *p, CTSize sz)
{
  if (LJ_64 && sz == 4) {  /* Support 32 bit pointers on 64 bit targets. */
    return ((void *)(uintptr_t)*(uint32_t *)p);
  } else {
    lua_assert(sz == CTSIZE_PTR);
    return *(void **)p;
  }
}

/* Set C data pointer. */
static LJ_AINLINE void cdata_setptr(void *p, CTSize sz, const void *v)
{
  if (LJ_64 && sz == 4) {  /* Support 32 bit pointers on 64 bit targets. */
    *(uint32_t *)p = (uint32_t)(uintptr_t)v;
  } else {
    lua_assert(sz == CTSIZE_PTR);
    *(void **)p = (void *)v;
  }
}

/* Allocate fixed-size C data object. */
static LJ_AINLINE GCcdata *lj_cdata_new(CTState *cts, CTypeID id, CTSize sz)
{
  GCcdata *cd;
#ifdef LUA_USE_ASSERT
  CType *ct = ctype_raw(cts, id);
  lua_assert((ctype_hassize(ct->info) ? ct->size : CTSIZE_PTR) == sz);
#endif
  cd = (GCcdata *)lj_mem_newgco(cts->L, sizeof(GCcdata) + sz);
  cd->gct = ~LJ_TCDATA;
  cd->ctypeid = ctype_check(cts, id);
  return cd;
}

/* Variant which works without a valid CTState. */
static LJ_AINLINE GCcdata *lj_cdata_new_(lua_State *L, CTypeID id, CTSize sz)
{
  GCcdata *cd = (GCcdata *)lj_mem_newgco(L, sizeof(GCcdata) + sz);
  cd->gct = ~LJ_TCDATA;
  cd->ctypeid = id;
  return cd;
}

LJ_FUNC GCcdata *lj_cdata_newref(CTState *cts, const void *pp, CTypeID id);
LJ_FUNC GCcdata *lj_cdata_newv(CTState *cts, CTypeID id, CTSize sz,
			       CTSize align);

LJ_FUNC void LJ_FASTCALL lj_cdata_free(global_State *g, GCcdata *cd);
LJ_FUNCA TValue * LJ_FASTCALL lj_cdata_setfin(lua_State *L, GCcdata *cd);

LJ_FUNC CType *lj_cdata_index(CTState *cts, GCcdata *cd, cTValue *key,
			      uint8_t **pp, CTInfo *qual);
LJ_FUNC int lj_cdata_get(CTState *cts, CType *s, TValue *o, uint8_t *sp);
LJ_FUNC void lj_cdata_set(CTState *cts, CType *d, uint8_t *dp, TValue *o,
			  CTInfo qual);

#endif

#endif
