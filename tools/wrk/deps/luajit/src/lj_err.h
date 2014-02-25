/*
** Error handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_ERR_H
#define _LJ_ERR_H

#include <stdarg.h>

#include "lj_obj.h"

typedef enum {
#define ERRDEF(name, msg) \
  LJ_ERR_##name, LJ_ERR_##name##_ = LJ_ERR_##name + sizeof(msg)-1,
#include "lj_errmsg.h"
  LJ_ERR__MAX
} ErrMsg;

LJ_DATA const char *lj_err_allmsg;
#define err2msg(em)	(lj_err_allmsg+(int)(em))

LJ_FUNC GCstr *lj_err_str(lua_State *L, ErrMsg em);
LJ_FUNCA_NORET void LJ_FASTCALL lj_err_throw(lua_State *L, int errcode);
LJ_FUNC_NORET void lj_err_mem(lua_State *L);
LJ_FUNC_NORET void lj_err_run(lua_State *L);
LJ_FUNC_NORET void lj_err_msg(lua_State *L, ErrMsg em);
LJ_FUNC_NORET void lj_err_lex(lua_State *L, GCstr *src, const char *tok,
			      BCLine line, ErrMsg em, va_list argp);
LJ_FUNC_NORET void lj_err_optype(lua_State *L, cTValue *o, ErrMsg opm);
LJ_FUNC_NORET void lj_err_comp(lua_State *L, cTValue *o1, cTValue *o2);
LJ_FUNC_NORET void lj_err_optype_call(lua_State *L, TValue *o);
LJ_FUNC_NORET void lj_err_callermsg(lua_State *L, const char *msg);
LJ_FUNC_NORET void lj_err_callerv(lua_State *L, ErrMsg em, ...);
LJ_FUNC_NORET void lj_err_caller(lua_State *L, ErrMsg em);
LJ_FUNC_NORET void lj_err_arg(lua_State *L, int narg, ErrMsg em);
LJ_FUNC_NORET void lj_err_argv(lua_State *L, int narg, ErrMsg em, ...);
LJ_FUNC_NORET void lj_err_argtype(lua_State *L, int narg, const char *xname);
LJ_FUNC_NORET void lj_err_argt(lua_State *L, int narg, int tt);

#endif
