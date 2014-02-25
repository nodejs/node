/*
** Debug library.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lib_debug_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_lib.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_debug

LJLIB_CF(debug_getregistry)
{
  copyTV(L, L->top++, registry(L));
  return 1;
}

LJLIB_CF(debug_getmetatable)
{
  lj_lib_checkany(L, 1);
  if (!lua_getmetatable(L, 1)) {
    setnilV(L->top-1);
  }
  return 1;
}

LJLIB_CF(debug_setmetatable)
{
  lj_lib_checktabornil(L, 2);
  L->top = L->base+2;
  lua_setmetatable(L, 1);
#if !LJ_52
  setboolV(L->top-1, 1);
#endif
  return 1;
}

LJLIB_CF(debug_getfenv)
{
  lj_lib_checkany(L, 1);
  lua_getfenv(L, 1);
  return 1;
}

LJLIB_CF(debug_setfenv)
{
  lj_lib_checktab(L, 2);
  L->top = L->base+2;
  if (!lua_setfenv(L, 1))
    lj_err_caller(L, LJ_ERR_SETFENV);
  return 1;
}

/* ------------------------------------------------------------------------ */

static void settabss(lua_State *L, const char *i, const char *v)
{
  lua_pushstring(L, v);
  lua_setfield(L, -2, i);
}

static void settabsi(lua_State *L, const char *i, int v)
{
  lua_pushinteger(L, v);
  lua_setfield(L, -2, i);
}

static void settabsb(lua_State *L, const char *i, int v)
{
  lua_pushboolean(L, v);
  lua_setfield(L, -2, i);
}

static lua_State *getthread(lua_State *L, int *arg)
{
  if (L->base < L->top && tvisthread(L->base)) {
    *arg = 1;
    return threadV(L->base);
  } else {
    *arg = 0;
    return L;
  }
}

static void treatstackoption(lua_State *L, lua_State *L1, const char *fname)
{
  if (L == L1) {
    lua_pushvalue(L, -2);
    lua_remove(L, -3);
  }
  else
    lua_xmove(L1, L, 1);
  lua_setfield(L, -2, fname);
}

LJLIB_CF(debug_getinfo)
{
  lj_Debug ar;
  int arg, opt_f = 0, opt_L = 0;
  lua_State *L1 = getthread(L, &arg);
  const char *options = luaL_optstring(L, arg+2, "flnSu");
  if (lua_isnumber(L, arg+1)) {
    if (!lua_getstack(L1, (int)lua_tointeger(L, arg+1), (lua_Debug *)&ar)) {
      setnilV(L->top-1);
      return 1;
    }
  } else if (L->base+arg < L->top && tvisfunc(L->base+arg)) {
    options = lua_pushfstring(L, ">%s", options);
    setfuncV(L1, L1->top++, funcV(L->base+arg));
  } else {
    lj_err_arg(L, arg+1, LJ_ERR_NOFUNCL);
  }
  if (!lj_debug_getinfo(L1, options, &ar, 1))
    lj_err_arg(L, arg+2, LJ_ERR_INVOPT);
  lua_createtable(L, 0, 16);  /* Create result table. */
  for (; *options; options++) {
    switch (*options) {
    case 'S':
      settabss(L, "source", ar.source);
      settabss(L, "short_src", ar.short_src);
      settabsi(L, "linedefined", ar.linedefined);
      settabsi(L, "lastlinedefined", ar.lastlinedefined);
      settabss(L, "what", ar.what);
      break;
    case 'l':
      settabsi(L, "currentline", ar.currentline);
      break;
    case 'u':
      settabsi(L, "nups", ar.nups);
      settabsi(L, "nparams", ar.nparams);
      settabsb(L, "isvararg", ar.isvararg);
      break;
    case 'n':
      settabss(L, "name", ar.name);
      settabss(L, "namewhat", ar.namewhat);
      break;
    case 'f': opt_f = 1; break;
    case 'L': opt_L = 1; break;
    default: break;
    }
  }
  if (opt_L) treatstackoption(L, L1, "activelines");
  if (opt_f) treatstackoption(L, L1, "func");
  return 1;  /* Return result table. */
}

LJLIB_CF(debug_getlocal)
{
  int arg;
  lua_State *L1 = getthread(L, &arg);
  lua_Debug ar;
  const char *name;
  int slot = lj_lib_checkint(L, arg+2);
  if (tvisfunc(L->base+arg)) {
    L->top = L->base+arg+1;
    lua_pushstring(L, lua_getlocal(L, NULL, slot));
    return 1;
  }
  if (!lua_getstack(L1, lj_lib_checkint(L, arg+1), &ar))
    lj_err_arg(L, arg+1, LJ_ERR_LVLRNG);
  name = lua_getlocal(L1, &ar, slot);
  if (name) {
    lua_xmove(L1, L, 1);
    lua_pushstring(L, name);
    lua_pushvalue(L, -2);
    return 2;
  } else {
    setnilV(L->top-1);
    return 1;
  }
}

LJLIB_CF(debug_setlocal)
{
  int arg;
  lua_State *L1 = getthread(L, &arg);
  lua_Debug ar;
  TValue *tv;
  if (!lua_getstack(L1, lj_lib_checkint(L, arg+1), &ar))
    lj_err_arg(L, arg+1, LJ_ERR_LVLRNG);
  tv = lj_lib_checkany(L, arg+3);
  copyTV(L1, L1->top++, tv);
  lua_pushstring(L, lua_setlocal(L1, &ar, lj_lib_checkint(L, arg+2)));
  return 1;
}

static int debug_getupvalue(lua_State *L, int get)
{
  int32_t n = lj_lib_checkint(L, 2);
  const char *name;
  lj_lib_checkfunc(L, 1);
  name = get ? lua_getupvalue(L, 1, n) : lua_setupvalue(L, 1, n);
  if (name) {
    lua_pushstring(L, name);
    if (!get) return 1;
    copyTV(L, L->top, L->top-2);
    L->top++;
    return 2;
  }
  return 0;
}

LJLIB_CF(debug_getupvalue)
{
  return debug_getupvalue(L, 1);
}

LJLIB_CF(debug_setupvalue)
{
  lj_lib_checkany(L, 3);
  return debug_getupvalue(L, 0);
}

LJLIB_CF(debug_upvalueid)
{
  GCfunc *fn = lj_lib_checkfunc(L, 1);
  int32_t n = lj_lib_checkint(L, 2) - 1;
  if ((uint32_t)n >= fn->l.nupvalues)
    lj_err_arg(L, 2, LJ_ERR_IDXRNG);
  setlightudV(L->top-1, isluafunc(fn) ? (void *)gcref(fn->l.uvptr[n]) :
					(void *)&fn->c.upvalue[n]);
  return 1;
}

LJLIB_CF(debug_upvaluejoin)
{
  GCfunc *fn[2];
  GCRef *p[2];
  int i;
  for (i = 0; i < 2; i++) {
    int32_t n;
    fn[i] = lj_lib_checkfunc(L, 2*i+1);
    if (!isluafunc(fn[i]))
      lj_err_arg(L, 2*i+1, LJ_ERR_NOLFUNC);
    n = lj_lib_checkint(L, 2*i+2) - 1;
    if ((uint32_t)n >= fn[i]->l.nupvalues)
      lj_err_arg(L, 2*i+2, LJ_ERR_IDXRNG);
    p[i] = &fn[i]->l.uvptr[n];
  }
  setgcrefr(*p[0], *p[1]);
  lj_gc_objbarrier(L, fn[0], gcref(*p[1]));
  return 0;
}

#if LJ_52
LJLIB_CF(debug_getuservalue)
{
  TValue *o = L->base;
  if (o < L->top && tvisudata(o))
    settabV(L, o, tabref(udataV(o)->env));
  else
    setnilV(o);
  L->top = o+1;
  return 1;
}

LJLIB_CF(debug_setuservalue)
{
  TValue *o = L->base;
  if (!(o < L->top && tvisudata(o)))
    lj_err_argt(L, 1, LUA_TUSERDATA);
  if (!(o+1 < L->top && tvistab(o+1)))
    lj_err_argt(L, 2, LUA_TTABLE);
  L->top = o+2;
  lua_setfenv(L, 1);
  return 1;
}
#endif

/* ------------------------------------------------------------------------ */

static const char KEY_HOOK = 'h';

static void hookf(lua_State *L, lua_Debug *ar)
{
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail return"};
  lua_pushlightuserdata(L, (void *)&KEY_HOOK);
  lua_rawget(L, LUA_REGISTRYINDEX);
  if (lua_isfunction(L, -1)) {
    lua_pushstring(L, hooknames[(int)ar->event]);
    if (ar->currentline >= 0)
      lua_pushinteger(L, ar->currentline);
    else lua_pushnil(L);
    lua_call(L, 2, 0);
  }
}

static int makemask(const char *smask, int count)
{
  int mask = 0;
  if (strchr(smask, 'c')) mask |= LUA_MASKCALL;
  if (strchr(smask, 'r')) mask |= LUA_MASKRET;
  if (strchr(smask, 'l')) mask |= LUA_MASKLINE;
  if (count > 0) mask |= LUA_MASKCOUNT;
  return mask;
}

static char *unmakemask(int mask, char *smask)
{
  int i = 0;
  if (mask & LUA_MASKCALL) smask[i++] = 'c';
  if (mask & LUA_MASKRET) smask[i++] = 'r';
  if (mask & LUA_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}

LJLIB_CF(debug_sethook)
{
  int arg, mask, count;
  lua_Hook func;
  (void)getthread(L, &arg);
  if (lua_isnoneornil(L, arg+1)) {
    lua_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  } else {
    const char *smask = luaL_checkstring(L, arg+2);
    luaL_checktype(L, arg+1, LUA_TFUNCTION);
    count = luaL_optint(L, arg+3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  lua_pushlightuserdata(L, (void *)&KEY_HOOK);
  lua_pushvalue(L, arg+1);
  lua_rawset(L, LUA_REGISTRYINDEX);
  lua_sethook(L, func, mask, count);
  return 0;
}

LJLIB_CF(debug_gethook)
{
  char buff[5];
  int mask = lua_gethookmask(L);
  lua_Hook hook = lua_gethook(L);
  if (hook != NULL && hook != hookf) {  /* external hook? */
    lua_pushliteral(L, "external hook");
  } else {
    lua_pushlightuserdata(L, (void *)&KEY_HOOK);
    lua_rawget(L, LUA_REGISTRYINDEX);   /* get hook */
  }
  lua_pushstring(L, unmakemask(mask, buff));
  lua_pushinteger(L, lua_gethookcount(L));
  return 3;
}

/* ------------------------------------------------------------------------ */

LJLIB_CF(debug_debug)
{
  for (;;) {
    char buffer[250];
    fputs("lua_debug> ", stderr);
    if (fgets(buffer, sizeof(buffer), stdin) == 0 ||
	strcmp(buffer, "cont\n") == 0)
      return 0;
    if (luaL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
	lua_pcall(L, 0, 0, 0)) {
      fputs(lua_tostring(L, -1), stderr);
      fputs("\n", stderr);
    }
    lua_settop(L, 0);  /* remove eventual returns */
  }
}

/* ------------------------------------------------------------------------ */

#define LEVELS1	12	/* size of the first part of the stack */
#define LEVELS2	10	/* size of the second part of the stack */

LJLIB_CF(debug_traceback)
{
  int arg;
  lua_State *L1 = getthread(L, &arg);
  const char *msg = lua_tostring(L, arg+1);
  if (msg == NULL && L->top > L->base+arg)
    L->top = L->base+arg+1;
  else
    luaL_traceback(L, L1, msg, lj_lib_optint(L, arg+2, (L == L1)));
  return 1;
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_debug(lua_State *L)
{
  LJ_LIB_REG(L, LUA_DBLIBNAME, debug);
  return 1;
}

