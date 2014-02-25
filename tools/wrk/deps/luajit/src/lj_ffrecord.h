/*
** Fast function call recorder.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_FFRECORD_H
#define _LJ_FFRECORD_H

#include "lj_obj.h"
#include "lj_jit.h"

#if LJ_HASJIT
/* Data used by handlers to record a fast function. */
typedef struct RecordFFData {
  TValue *argv;		/* Runtime argument values. */
  ptrdiff_t nres;	/* Number of returned results (defaults to 1). */
  uint32_t data;	/* Per-ffid auxiliary data (opcode, literal etc.). */
} RecordFFData;

LJ_FUNC int32_t lj_ffrecord_select_mode(jit_State *J, TRef tr, TValue *tv);
LJ_FUNC void lj_ffrecord_func(jit_State *J);
#endif

#endif
