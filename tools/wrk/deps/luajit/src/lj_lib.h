/*
** Library function support.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_LIB_H
#define _LJ_LIB_H

#include "lj_obj.h"

/*
** A fallback handler is called by the assembler VM if the fast path fails:
**
** - too few arguments:   unrecoverable.
** - wrong argument type:   recoverable, if coercion succeeds.
** - bad argument value:  unrecoverable.
** - stack overflow:        recoverable, if stack reallocation succeeds.
** - extra handling:        recoverable.
**
** The unrecoverable cases throw an error with lj_err_arg(), lj_err_argtype(),
** lj_err_caller() or lj_err_callermsg().
** The recoverable cases return 0 or the number of results + 1.
** The assembler VM retries the fast path only if 0 is returned.
** This time the fallback must not be called again or it gets stuck in a loop.
*/

/* Return values from fallback handler. */
#define FFH_RETRY	0
#define FFH_UNREACHABLE	FFH_RETRY
#define FFH_RES(n)	((n)+1)
#define FFH_TAILCALL	(-1)

LJ_FUNC TValue *lj_lib_checkany(lua_State *L, int narg);
LJ_FUNC GCstr *lj_lib_checkstr(lua_State *L, int narg);
LJ_FUNC GCstr *lj_lib_optstr(lua_State *L, int narg);
#if LJ_DUALNUM
LJ_FUNC void lj_lib_checknumber(lua_State *L, int narg);
#else
#define lj_lib_checknumber(L, narg)	lj_lib_checknum((L), (narg))
#endif
LJ_FUNC lua_Number lj_lib_checknum(lua_State *L, int narg);
LJ_FUNC int32_t lj_lib_checkint(lua_State *L, int narg);
LJ_FUNC int32_t lj_lib_optint(lua_State *L, int narg, int32_t def);
LJ_FUNC int32_t lj_lib_checkbit(lua_State *L, int narg);
LJ_FUNC GCfunc *lj_lib_checkfunc(lua_State *L, int narg);
LJ_FUNC GCtab *lj_lib_checktab(lua_State *L, int narg);
LJ_FUNC GCtab *lj_lib_checktabornil(lua_State *L, int narg);
LJ_FUNC int lj_lib_checkopt(lua_State *L, int narg, int def, const char *lst);

/* Avoid including lj_frame.h. */
#define lj_lib_upvalue(L, n) \
  (&gcref((L->base-1)->fr.func)->fn.c.upvalue[(n)-1])

#if LJ_TARGET_WINDOWS
#define lj_lib_checkfpu(L) \
  do { setnumV(L->top++, (lua_Number)1437217655); \
    if (lua_tointeger(L, -1) != 1437217655) lj_err_caller(L, LJ_ERR_BADFPU); \
    L->top--; } while (0)
#else
#define lj_lib_checkfpu(L)	UNUSED(L)
#endif

/* Push internal function on the stack. */
static LJ_AINLINE void lj_lib_pushcc(lua_State *L, lua_CFunction f,
				     int id, int n)
{
  GCfunc *fn;
  lua_pushcclosure(L, f, n);
  fn = funcV(L->top-1);
  fn->c.ffid = (uint8_t)id;
  setmref(fn->c.pc, &G(L)->bc_cfunc_int);
}

#define lj_lib_pushcf(L, fn, id)	(lj_lib_pushcc(L, (fn), (id), 0))

/* Library function declarations. Scanned by buildvm. */
#define LJLIB_CF(name)		static int lj_cf_##name(lua_State *L)
#define LJLIB_ASM(name)		static int lj_ffh_##name(lua_State *L)
#define LJLIB_ASM_(name)
#define LJLIB_SET(name)
#define LJLIB_PUSH(arg)
#define LJLIB_REC(handler)
#define LJLIB_NOREGUV
#define LJLIB_NOREG

#define LJ_LIB_REG(L, regname, name) \
  lj_lib_register(L, regname, lj_lib_init_##name, lj_lib_cf_##name)

LJ_FUNC void lj_lib_register(lua_State *L, const char *libname,
			     const uint8_t *init, const lua_CFunction *cf);

/* Library init data tags. */
#define LIBINIT_LENMASK	0x3f
#define LIBINIT_TAGMASK	0xc0
#define LIBINIT_CF	0x00
#define LIBINIT_ASM	0x40
#define LIBINIT_ASM_	0x80
#define LIBINIT_STRING	0xc0
#define LIBINIT_MAXSTR	0x39
#define LIBINIT_SET	0xfa
#define LIBINIT_NUMBER	0xfb
#define LIBINIT_COPY	0xfc
#define LIBINIT_LASTCL	0xfd
#define LIBINIT_FFID	0xfe
#define LIBINIT_END	0xff

/* Exported library functions. */

typedef struct RandomState RandomState;
LJ_FUNC uint64_t LJ_FASTCALL lj_math_random_step(RandomState *rs);

#endif
