/*
** FFI C callback handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_CCALLBACK_H
#define _LJ_CCALLBACK_H

#include "lj_obj.h"
#include "lj_ctype.h"

#if LJ_HASFFI

/* Really belongs to lj_vm.h. */
LJ_ASMF void lj_vm_ffi_callback(void);

LJ_FUNC MSize lj_ccallback_ptr2slot(CTState *cts, void *p);
LJ_FUNCA lua_State * LJ_FASTCALL lj_ccallback_enter(CTState *cts, void *cf);
LJ_FUNCA void LJ_FASTCALL lj_ccallback_leave(CTState *cts, TValue *o);
LJ_FUNC void *lj_ccallback_new(CTState *cts, CType *ct, GCfunc *fn);
LJ_FUNC void lj_ccallback_mcode_free(CTState *cts);

#endif

#endif
