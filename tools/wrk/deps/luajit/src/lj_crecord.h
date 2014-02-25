/*
** Trace recorder for C data operations.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_CRECORD_H
#define _LJ_CRECORD_H

#include "lj_obj.h"
#include "lj_jit.h"
#include "lj_ffrecord.h"

#if LJ_HASJIT && LJ_HASFFI
LJ_FUNC void LJ_FASTCALL recff_cdata_index(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_cdata_call(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_cdata_arith(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_clib_index(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_ffi_new(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_ffi_errno(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_ffi_string(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_ffi_copy(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_ffi_fill(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_ffi_typeof(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_ffi_istype(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_ffi_abi(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_ffi_xof(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL recff_ffi_gc(jit_State *J, RecordFFData *rd);
LJ_FUNC void LJ_FASTCALL lj_crecord_tonumber(jit_State *J, RecordFFData *rd);
#endif

#endif
