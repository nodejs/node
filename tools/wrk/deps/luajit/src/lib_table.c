/*
** Table library.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lib_table_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_tab.h"
#include "lj_lib.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_table

LJLIB_CF(table_foreachi)
{
  GCtab *t = lj_lib_checktab(L, 1);
  GCfunc *func = lj_lib_checkfunc(L, 2);
  MSize i, n = lj_tab_len(t);
  for (i = 1; i <= n; i++) {
    cTValue *val;
    setfuncV(L, L->top, func);
    setintV(L->top+1, i);
    val = lj_tab_getint(t, (int32_t)i);
    if (val) { copyTV(L, L->top+2, val); } else { setnilV(L->top+2); }
    L->top += 3;
    lua_call(L, 2, 1);
    if (!tvisnil(L->top-1))
      return 1;
    L->top--;
  }
  return 0;
}

LJLIB_CF(table_foreach)
{
  GCtab *t = lj_lib_checktab(L, 1);
  GCfunc *func = lj_lib_checkfunc(L, 2);
  L->top = L->base+3;
  setnilV(L->top-1);
  while (lj_tab_next(L, t, L->top-1)) {
    copyTV(L, L->top+2, L->top);
    copyTV(L, L->top+1, L->top-1);
    setfuncV(L, L->top, func);
    L->top += 3;
    lua_call(L, 2, 1);
    if (!tvisnil(L->top-1))
      return 1;
    L->top--;
  }
  return 0;
}

LJLIB_ASM(table_getn)		LJLIB_REC(.)
{
  lj_lib_checktab(L, 1);
  return FFH_UNREACHABLE;
}

LJLIB_CF(table_maxn)
{
  GCtab *t = lj_lib_checktab(L, 1);
  TValue *array = tvref(t->array);
  Node *node;
  lua_Number m = 0;
  ptrdiff_t i;
  for (i = (ptrdiff_t)t->asize - 1; i >= 0; i--)
    if (!tvisnil(&array[i])) {
      m = (lua_Number)(int32_t)i;
      break;
    }
  node = noderef(t->node);
  for (i = (ptrdiff_t)t->hmask; i >= 0; i--)
    if (!tvisnil(&node[i].val) && tvisnumber(&node[i].key)) {
      lua_Number n = numberVnum(&node[i].key);
      if (n > m) m = n;
    }
  setnumV(L->top-1, m);
  return 1;
}

LJLIB_CF(table_insert)		LJLIB_REC(.)
{
  GCtab *t = lj_lib_checktab(L, 1);
  int32_t n, i = (int32_t)lj_tab_len(t) + 1;
  int nargs = (int)((char *)L->top - (char *)L->base);
  if (nargs != 2*sizeof(TValue)) {
    if (nargs != 3*sizeof(TValue))
      lj_err_caller(L, LJ_ERR_TABINS);
    /* NOBARRIER: This just moves existing elements around. */
    for (n = lj_lib_checkint(L, 2); i > n; i--) {
      /* The set may invalidate the get pointer, so need to do it first! */
      TValue *dst = lj_tab_setint(L, t, i);
      cTValue *src = lj_tab_getint(t, i-1);
      if (src) {
	copyTV(L, dst, src);
      } else {
	setnilV(dst);
      }
    }
    i = n;
  }
  {
    TValue *dst = lj_tab_setint(L, t, i);
    copyTV(L, dst, L->top-1);  /* Set new value. */
    lj_gc_barriert(L, t, dst);
  }
  return 0;
}

LJLIB_CF(table_remove)		LJLIB_REC(.)
{
  GCtab *t = lj_lib_checktab(L, 1);
  int32_t e = (int32_t)lj_tab_len(t);
  int32_t pos = lj_lib_optint(L, 2, e);
  if (!(1 <= pos && pos <= e))  /* Nothing to remove? */
    return 0;
  lua_rawgeti(L, 1, pos);  /* Get previous value. */
  /* NOBARRIER: This just moves existing elements around. */
  for (; pos < e; pos++) {
    cTValue *src = lj_tab_getint(t, pos+1);
    TValue *dst = lj_tab_setint(L, t, pos);
    if (src) {
      copyTV(L, dst, src);
    } else {
      setnilV(dst);
    }
  }
  setnilV(lj_tab_setint(L, t, e));  /* Remove (last) value. */
  return 1;  /* Return previous value. */
}

LJLIB_CF(table_concat)
{
  luaL_Buffer b;
  GCtab *t = lj_lib_checktab(L, 1);
  GCstr *sep = lj_lib_optstr(L, 2);
  MSize seplen = sep ? sep->len : 0;
  int32_t i = lj_lib_optint(L, 3, 1);
  int32_t e = (L->base+3 < L->top && !tvisnil(L->base+3)) ?
	      lj_lib_checkint(L, 4) : (int32_t)lj_tab_len(t);
  luaL_buffinit(L, &b);
  if (i <= e) {
    for (;;) {
      cTValue *o;
      lua_rawgeti(L, 1, i);
      o = L->top-1;
      if (!(tvisstr(o) || tvisnumber(o)))
	lj_err_callerv(L, LJ_ERR_TABCAT, lj_typename(o), i);
      luaL_addvalue(&b);
      if (i++ == e) break;
      if (seplen)
	luaL_addlstring(&b, strdata(sep), seplen);
    }
  }
  luaL_pushresult(&b);
  return 1;
}

/* ------------------------------------------------------------------------ */

static void set2(lua_State *L, int i, int j)
{
  lua_rawseti(L, 1, i);
  lua_rawseti(L, 1, j);
}

static int sort_comp(lua_State *L, int a, int b)
{
  if (!lua_isnil(L, 2)) {  /* function? */
    int res;
    lua_pushvalue(L, 2);
    lua_pushvalue(L, a-1);  /* -1 to compensate function */
    lua_pushvalue(L, b-2);  /* -2 to compensate function and `a' */
    lua_call(L, 2, 1);
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
  } else {  /* a < b? */
    return lua_lessthan(L, a, b);
  }
}

static void auxsort(lua_State *L, int l, int u)
{
  while (l < u) {  /* for tail recursion */
    int i, j;
    /* sort elements a[l], a[(l+u)/2] and a[u] */
    lua_rawgeti(L, 1, l);
    lua_rawgeti(L, 1, u);
    if (sort_comp(L, -1, -2))  /* a[u] < a[l]? */
      set2(L, l, u);  /* swap a[l] - a[u] */
    else
      lua_pop(L, 2);
    if (u-l == 1) break;  /* only 2 elements */
    i = (l+u)/2;
    lua_rawgeti(L, 1, i);
    lua_rawgeti(L, 1, l);
    if (sort_comp(L, -2, -1)) {  /* a[i]<a[l]? */
      set2(L, i, l);
    } else {
      lua_pop(L, 1);  /* remove a[l] */
      lua_rawgeti(L, 1, u);
      if (sort_comp(L, -1, -2))  /* a[u]<a[i]? */
	set2(L, i, u);
      else
	lua_pop(L, 2);
    }
    if (u-l == 2) break;  /* only 3 elements */
    lua_rawgeti(L, 1, i);  /* Pivot */
    lua_pushvalue(L, -1);
    lua_rawgeti(L, 1, u-1);
    set2(L, i, u-1);
    /* a[l] <= P == a[u-1] <= a[u], only need to sort from l+1 to u-2 */
    i = l; j = u-1;
    for (;;) {  /* invariant: a[l..i] <= P <= a[j..u] */
      /* repeat ++i until a[i] >= P */
      while (lua_rawgeti(L, 1, ++i), sort_comp(L, -1, -2)) {
	if (i>=u) lj_err_caller(L, LJ_ERR_TABSORT);
	lua_pop(L, 1);  /* remove a[i] */
      }
      /* repeat --j until a[j] <= P */
      while (lua_rawgeti(L, 1, --j), sort_comp(L, -3, -1)) {
	if (j<=l) lj_err_caller(L, LJ_ERR_TABSORT);
	lua_pop(L, 1);  /* remove a[j] */
      }
      if (j<i) {
	lua_pop(L, 3);  /* pop pivot, a[i], a[j] */
	break;
      }
      set2(L, i, j);
    }
    lua_rawgeti(L, 1, u-1);
    lua_rawgeti(L, 1, i);
    set2(L, u-1, i);  /* swap pivot (a[u-1]) with a[i] */
    /* a[l..i-1] <= a[i] == P <= a[i+1..u] */
    /* adjust so that smaller half is in [j..i] and larger one in [l..u] */
    if (i-l < u-i) {
      j=l; i=i-1; l=i+2;
    } else {
      j=i+1; i=u; u=j-2;
    }
    auxsort(L, j, i);  /* call recursively the smaller one */
  }  /* repeat the routine for the larger one */
}

LJLIB_CF(table_sort)
{
  GCtab *t = lj_lib_checktab(L, 1);
  int32_t n = (int32_t)lj_tab_len(t);
  lua_settop(L, 2);
  if (!tvisnil(L->base+1))
    lj_lib_checkfunc(L, 2);
  auxsort(L, 1, n);
  return 0;
}

#if LJ_52
LJLIB_PUSH("n")
LJLIB_CF(table_pack)
{
  TValue *array, *base = L->base;
  MSize i, n = (uint32_t)(L->top - base);
  GCtab *t = lj_tab_new(L, n ? n+1 : 0, 1);
  /* NOBARRIER: The table is new (marked white). */
  setintV(lj_tab_setstr(L, t, strV(lj_lib_upvalue(L, 1))), (int32_t)n);
  for (array = tvref(t->array) + 1, i = 0; i < n; i++)
    copyTV(L, &array[i], &base[i]);
  settabV(L, base, t);
  L->top = base+1;
  lj_gc_check(L);
  return 1;
}
#endif

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_table(lua_State *L)
{
  LJ_LIB_REG(L, LUA_TABLIBNAME, table);
#if LJ_52
  lua_getglobal(L, "unpack");
  lua_setfield(L, -2, "unpack");
#endif
  return 1;
}

