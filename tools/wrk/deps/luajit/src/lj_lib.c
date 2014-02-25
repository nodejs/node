/*
** Library function support.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_lib_c
#define LUA_CORE

#include "lauxlib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_func.h"
#include "lj_bc.h"
#include "lj_dispatch.h"
#include "lj_vm.h"
#include "lj_strscan.h"
#include "lj_lib.h"

/* -- Library initialization ---------------------------------------------- */

static GCtab *lib_create_table(lua_State *L, const char *libname, int hsize)
{
  if (libname) {
    luaL_findtable(L, LUA_REGISTRYINDEX, "_LOADED", 16);
    lua_getfield(L, -1, libname);
    if (!tvistab(L->top-1)) {
      L->top--;
      if (luaL_findtable(L, LUA_GLOBALSINDEX, libname, hsize) != NULL)
	lj_err_callerv(L, LJ_ERR_BADMODN, libname);
      settabV(L, L->top, tabV(L->top-1));
      L->top++;
      lua_setfield(L, -3, libname);  /* _LOADED[libname] = new table */
    }
    L->top--;
    settabV(L, L->top-1, tabV(L->top));
  } else {
    lua_createtable(L, 0, hsize);
  }
  return tabV(L->top-1);
}

void lj_lib_register(lua_State *L, const char *libname,
		     const uint8_t *p, const lua_CFunction *cf)
{
  GCtab *env = tabref(L->env);
  GCfunc *ofn = NULL;
  int ffid = *p++;
  BCIns *bcff = &L2GG(L)->bcff[*p++];
  GCtab *tab = lib_create_table(L, libname, *p++);
  ptrdiff_t tpos = L->top - L->base;

  /* Avoid barriers further down. */
  lj_gc_anybarriert(L, tab);
  tab->nomm = 0;

  for (;;) {
    uint32_t tag = *p++;
    MSize len = tag & LIBINIT_LENMASK;
    tag &= LIBINIT_TAGMASK;
    if (tag != LIBINIT_STRING) {
      const char *name;
      MSize nuv = (MSize)(L->top - L->base - tpos);
      GCfunc *fn = lj_func_newC(L, nuv, env);
      if (nuv) {
	L->top = L->base + tpos;
	memcpy(fn->c.upvalue, L->top, sizeof(TValue)*nuv);
      }
      fn->c.ffid = (uint8_t)(ffid++);
      name = (const char *)p;
      p += len;
      if (tag == LIBINIT_CF)
	setmref(fn->c.pc, &G(L)->bc_cfunc_int);
      else
	setmref(fn->c.pc, bcff++);
      if (tag == LIBINIT_ASM_)
	fn->c.f = ofn->c.f;  /* Copy handler from previous function. */
      else
	fn->c.f = *cf++;  /* Get cf or handler from C function table. */
      if (len) {
	/* NOBARRIER: See above for common barrier. */
	setfuncV(L, lj_tab_setstr(L, tab, lj_str_new(L, name, len)), fn);
      }
      ofn = fn;
    } else {
      switch (tag | len) {
      case LIBINIT_SET:
	L->top -= 2;
	if (tvisstr(L->top+1) && strV(L->top+1)->len == 0)
	  env = tabV(L->top);
	else  /* NOBARRIER: See above for common barrier. */
	  copyTV(L, lj_tab_set(L, tab, L->top+1), L->top);
	break;
      case LIBINIT_NUMBER:
	memcpy(&L->top->n, p, sizeof(double));
	L->top++;
	p += sizeof(double);
	break;
      case LIBINIT_COPY:
	copyTV(L, L->top, L->top - *p++);
	L->top++;
	break;
      case LIBINIT_LASTCL:
	setfuncV(L, L->top++, ofn);
	break;
      case LIBINIT_FFID:
	ffid++;
	break;
      case LIBINIT_END:
	return;
      default:
	setstrV(L, L->top++, lj_str_new(L, (const char *)p, len));
	p += len;
	break;
      }
    }
  }
}

/* -- Type checks --------------------------------------------------------- */

TValue *lj_lib_checkany(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (o >= L->top)
    lj_err_arg(L, narg, LJ_ERR_NOVAL);
  return o;
}

GCstr *lj_lib_checkstr(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (o < L->top) {
    if (LJ_LIKELY(tvisstr(o))) {
      return strV(o);
    } else if (tvisnumber(o)) {
      GCstr *s = lj_str_fromnumber(L, o);
      setstrV(L, o, s);
      return s;
    }
  }
  lj_err_argt(L, narg, LUA_TSTRING);
  return NULL;  /* unreachable */
}

GCstr *lj_lib_optstr(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  return (o < L->top && !tvisnil(o)) ? lj_lib_checkstr(L, narg) : NULL;
}

#if LJ_DUALNUM
void lj_lib_checknumber(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && lj_strscan_numberobj(o)))
    lj_err_argt(L, narg, LUA_TNUMBER);
}
#endif

lua_Number lj_lib_checknum(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top &&
	(tvisnumber(o) || (tvisstr(o) && lj_strscan_num(strV(o), o)))))
    lj_err_argt(L, narg, LUA_TNUMBER);
  if (LJ_UNLIKELY(tvisint(o))) {
    lua_Number n = (lua_Number)intV(o);
    setnumV(o, n);
    return n;
  } else {
    return numV(o);
  }
}

int32_t lj_lib_checkint(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && lj_strscan_numberobj(o)))
    lj_err_argt(L, narg, LUA_TNUMBER);
  if (LJ_LIKELY(tvisint(o))) {
    return intV(o);
  } else {
    int32_t i = lj_num2int(numV(o));
    if (LJ_DUALNUM) setintV(o, i);
    return i;
  }
}

int32_t lj_lib_optint(lua_State *L, int narg, int32_t def)
{
  TValue *o = L->base + narg-1;
  return (o < L->top && !tvisnil(o)) ? lj_lib_checkint(L, narg) : def;
}

int32_t lj_lib_checkbit(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && lj_strscan_numberobj(o)))
    lj_err_argt(L, narg, LUA_TNUMBER);
  if (LJ_LIKELY(tvisint(o))) {
    return intV(o);
  } else {
    int32_t i = lj_num2bit(numV(o));
    if (LJ_DUALNUM) setintV(o, i);
    return i;
  }
}

GCfunc *lj_lib_checkfunc(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && tvisfunc(o)))
    lj_err_argt(L, narg, LUA_TFUNCTION);
  return funcV(o);
}

GCtab *lj_lib_checktab(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && tvistab(o)))
    lj_err_argt(L, narg, LUA_TTABLE);
  return tabV(o);
}

GCtab *lj_lib_checktabornil(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (o < L->top) {
    if (tvistab(o))
      return tabV(o);
    else if (tvisnil(o))
      return NULL;
  }
  lj_err_arg(L, narg, LJ_ERR_NOTABN);
  return NULL;  /* unreachable */
}

int lj_lib_checkopt(lua_State *L, int narg, int def, const char *lst)
{
  GCstr *s = def >= 0 ? lj_lib_optstr(L, narg) : lj_lib_checkstr(L, narg);
  if (s) {
    const char *opt = strdata(s);
    MSize len = s->len;
    int i;
    for (i = 0; *(const uint8_t *)lst; i++) {
      if (*(const uint8_t *)lst == len && memcmp(opt, lst+1, len) == 0)
	return i;
      lst += 1+*(const uint8_t *)lst;
    }
    lj_err_argv(L, narg, LJ_ERR_INVOPTM, opt);
  }
  return def;
}

