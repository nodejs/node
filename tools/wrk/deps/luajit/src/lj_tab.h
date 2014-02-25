/*
** Table handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_TAB_H
#define _LJ_TAB_H

#include "lj_obj.h"

/* Hash constants. Tuned using a brute force search. */
#define HASH_BIAS	(-0x04c11db7)
#define HASH_ROT1	14
#define HASH_ROT2	5
#define HASH_ROT3	13

/* Scramble the bits of numbers and pointers. */
static LJ_AINLINE uint32_t hashrot(uint32_t lo, uint32_t hi)
{
#if LJ_TARGET_X86ORX64
  /* Prefer variant that compiles well for a 2-operand CPU. */
  lo ^= hi; hi = lj_rol(hi, HASH_ROT1);
  lo -= hi; hi = lj_rol(hi, HASH_ROT2);
  hi ^= lo; hi -= lj_rol(lo, HASH_ROT3);
#else
  lo ^= hi;
  lo = lo - lj_rol(hi, HASH_ROT1);
  hi = lo ^ lj_rol(hi, HASH_ROT1 + HASH_ROT2);
  hi = hi - lj_rol(lo, HASH_ROT3);
#endif
  return hi;
}

#define hsize2hbits(s)	((s) ? ((s)==1 ? 1 : 1+lj_fls((uint32_t)((s)-1))) : 0)

LJ_FUNCA GCtab *lj_tab_new(lua_State *L, uint32_t asize, uint32_t hbits);
#if LJ_HASJIT
LJ_FUNC GCtab * LJ_FASTCALL lj_tab_new1(lua_State *L, uint32_t ahsize);
#endif
LJ_FUNCA GCtab * LJ_FASTCALL lj_tab_dup(lua_State *L, const GCtab *kt);
LJ_FUNC void LJ_FASTCALL lj_tab_free(global_State *g, GCtab *t);
LJ_FUNCA void lj_tab_reasize(lua_State *L, GCtab *t, uint32_t nasize);

/* Caveat: all getters except lj_tab_get() can return NULL! */

LJ_FUNCA cTValue * LJ_FASTCALL lj_tab_getinth(GCtab *t, int32_t key);
LJ_FUNC cTValue *lj_tab_getstr(GCtab *t, GCstr *key);
LJ_FUNCA cTValue *lj_tab_get(lua_State *L, GCtab *t, cTValue *key);

/* Caveat: all setters require a write barrier for the stored value. */

LJ_FUNCA TValue *lj_tab_newkey(lua_State *L, GCtab *t, cTValue *key);
LJ_FUNC TValue *lj_tab_setinth(lua_State *L, GCtab *t, int32_t key);
LJ_FUNC TValue *lj_tab_setstr(lua_State *L, GCtab *t, GCstr *key);
LJ_FUNC TValue *lj_tab_set(lua_State *L, GCtab *t, cTValue *key);

#define inarray(t, key)		((MSize)(key) < (MSize)(t)->asize)
#define arrayslot(t, i)		(&tvref((t)->array)[(i)])
#define lj_tab_getint(t, key) \
  (inarray((t), (key)) ? arrayslot((t), (key)) : lj_tab_getinth((t), (key)))
#define lj_tab_setint(L, t, key) \
  (inarray((t), (key)) ? arrayslot((t), (key)) : lj_tab_setinth(L, (t), (key)))

LJ_FUNCA int lj_tab_next(lua_State *L, GCtab *t, TValue *key);
LJ_FUNCA MSize LJ_FASTCALL lj_tab_len(GCtab *t);

#endif
