/*
** State and stack handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_STATE_H
#define _LJ_STATE_H

#include "lj_obj.h"

#define incr_top(L) \
  (++L->top >= tvref(L->maxstack) && (lj_state_growstack1(L), 0))

#define savestack(L, p)		((char *)(p) - mref(L->stack, char))
#define restorestack(L, n)	((TValue *)(mref(L->stack, char) + (n)))

LJ_FUNC void lj_state_relimitstack(lua_State *L);
LJ_FUNC void lj_state_shrinkstack(lua_State *L, MSize used);
LJ_FUNCA void LJ_FASTCALL lj_state_growstack(lua_State *L, MSize need);
LJ_FUNC void LJ_FASTCALL lj_state_growstack1(lua_State *L);

static LJ_AINLINE void lj_state_checkstack(lua_State *L, MSize need)
{
  if ((mref(L->maxstack, char) - (char *)L->top) <=
      (ptrdiff_t)need*(ptrdiff_t)sizeof(TValue))
    lj_state_growstack(L, need);
}

LJ_FUNC lua_State *lj_state_new(lua_State *L);
LJ_FUNC void LJ_FASTCALL lj_state_free(global_State *g, lua_State *L);
#if LJ_64
LJ_FUNC lua_State *lj_state_newstate(lua_Alloc f, void *ud);
#endif

#endif
