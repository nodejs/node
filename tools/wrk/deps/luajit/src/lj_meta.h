/*
** Metamethod handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_META_H
#define _LJ_META_H

#include "lj_obj.h"

/* Metamethod handling */
LJ_FUNC void lj_meta_init(lua_State *L);
LJ_FUNC cTValue *lj_meta_cache(GCtab *mt, MMS mm, GCstr *name);
LJ_FUNC cTValue *lj_meta_lookup(lua_State *L, cTValue *o, MMS mm);
#if LJ_HASFFI
LJ_FUNC int lj_meta_tailcall(lua_State *L, cTValue *tv);
#endif

#define lj_meta_fastg(g, mt, mm) \
  ((mt) == NULL ? NULL : ((mt)->nomm & (1u<<(mm))) ? NULL : \
   lj_meta_cache(mt, mm, mmname_str(g, mm)))
#define lj_meta_fast(L, mt, mm)	lj_meta_fastg(G(L), mt, mm)

/* C helpers for some instructions, called from assembler VM. */
LJ_FUNCA cTValue *lj_meta_tget(lua_State *L, cTValue *o, cTValue *k);
LJ_FUNCA TValue *lj_meta_tset(lua_State *L, cTValue *o, cTValue *k);
LJ_FUNCA TValue *lj_meta_arith(lua_State *L, TValue *ra, cTValue *rb,
			       cTValue *rc, BCReg op);
LJ_FUNCA TValue *lj_meta_cat(lua_State *L, TValue *top, int left);
LJ_FUNCA TValue * LJ_FASTCALL lj_meta_len(lua_State *L, cTValue *o);
LJ_FUNCA TValue *lj_meta_equal(lua_State *L, GCobj *o1, GCobj *o2, int ne);
LJ_FUNCA TValue * LJ_FASTCALL lj_meta_equal_cd(lua_State *L, BCIns ins);
LJ_FUNCA TValue *lj_meta_comp(lua_State *L, cTValue *o1, cTValue *o2, int op);
LJ_FUNCA void lj_meta_call(lua_State *L, TValue *func, TValue *top);
LJ_FUNCA void LJ_FASTCALL lj_meta_for(lua_State *L, TValue *o);

#endif
