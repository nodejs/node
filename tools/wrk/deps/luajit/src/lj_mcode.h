/*
** Machine code management.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_MCODE_H
#define _LJ_MCODE_H

#include "lj_obj.h"

#if LJ_HASJIT || LJ_HASFFI
LJ_FUNC void lj_mcode_sync(void *start, void *end);
#endif

#if LJ_HASJIT

#include "lj_jit.h"

LJ_FUNC void lj_mcode_free(jit_State *J);
LJ_FUNC MCode *lj_mcode_reserve(jit_State *J, MCode **lim);
LJ_FUNC void lj_mcode_commit(jit_State *J, MCode *m);
LJ_FUNC void lj_mcode_abort(jit_State *J);
LJ_FUNC MCode *lj_mcode_patch(jit_State *J, MCode *ptr, int finish);
LJ_FUNC_NORET void lj_mcode_limiterr(jit_State *J, size_t need);

#define lj_mcode_commitbot(J, m)	(J->mcbot = (m))

#endif

#endif
