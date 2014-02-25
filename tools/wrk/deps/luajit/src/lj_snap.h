/*
** Snapshot handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_SNAP_H
#define _LJ_SNAP_H

#include "lj_obj.h"
#include "lj_jit.h"

#if LJ_HASJIT
LJ_FUNC void lj_snap_add(jit_State *J);
LJ_FUNC void lj_snap_purge(jit_State *J);
LJ_FUNC void lj_snap_shrink(jit_State *J);
LJ_FUNC IRIns *lj_snap_regspmap(GCtrace *T, SnapNo snapno, IRIns *ir);
LJ_FUNC void lj_snap_replay(jit_State *J, GCtrace *T);
LJ_FUNC const BCIns *lj_snap_restore(jit_State *J, void *exptr);
LJ_FUNC void lj_snap_grow_buf_(jit_State *J, MSize need);
LJ_FUNC void lj_snap_grow_map_(jit_State *J, MSize need);

static LJ_AINLINE void lj_snap_grow_buf(jit_State *J, MSize need)
{
  if (LJ_UNLIKELY(need > J->sizesnap)) lj_snap_grow_buf_(J, need);
}

static LJ_AINLINE void lj_snap_grow_map(jit_State *J, MSize need)
{
  if (LJ_UNLIKELY(need > J->sizesnapmap)) lj_snap_grow_map_(J, need);
}

#endif

#endif
