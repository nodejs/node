/*
** Metamethod handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
**
** Portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_meta_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_frame.h"
#include "lj_bc.h"
#include "lj_vm.h"
#include "lj_strscan.h"

/* -- Metamethod handling ------------------------------------------------- */

/* String interning of metamethod names for fast indexing. */
void lj_meta_init(lua_State *L)
{
#define MMNAME(name)	"__" #name
  const char *metanames = MMDEF(MMNAME);
#undef MMNAME
  global_State *g = G(L);
  const char *p, *q;
  uint32_t mm;
  for (mm = 0, p = metanames; *p; mm++, p = q) {
    GCstr *s;
    for (q = p+2; *q && *q != '_'; q++) ;
    s = lj_str_new(L, p, (size_t)(q-p));
    /* NOBARRIER: g->gcroot[] is a GC root. */
    setgcref(g->gcroot[GCROOT_MMNAME+mm], obj2gco(s));
  }
}

/* Negative caching of a few fast metamethods. See the lj_meta_fast() macro. */
cTValue *lj_meta_cache(GCtab *mt, MMS mm, GCstr *name)
{
  cTValue *mo = lj_tab_getstr(mt, name);
  lua_assert(mm <= MM_FAST);
  if (!mo || tvisnil(mo)) {  /* No metamethod? */
    mt->nomm |= (uint8_t)(1u<<mm);  /* Set negative cache flag. */
    return NULL;
  }
  return mo;
}

/* Lookup metamethod for object. */
cTValue *lj_meta_lookup(lua_State *L, cTValue *o, MMS mm)
{
  GCtab *mt;
  if (tvistab(o))
    mt = tabref(tabV(o)->metatable);
  else if (tvisudata(o))
    mt = tabref(udataV(o)->metatable);
  else
    mt = tabref(basemt_obj(G(L), o));
  if (mt) {
    cTValue *mo = lj_tab_getstr(mt, mmname_str(G(L), mm));
    if (mo)
      return mo;
  }
  return niltv(L);
}

#if LJ_HASFFI
/* Tailcall from C function. */
int lj_meta_tailcall(lua_State *L, cTValue *tv)
{
  TValue *base = L->base;
  TValue *top = L->top;
  const BCIns *pc = frame_pc(base-1);  /* Preserve old PC from frame. */
  copyTV(L, base-1, tv);  /* Replace frame with new object. */
  top->u32.lo = LJ_CONT_TAILCALL;
  setframe_pc(top, pc);
  setframe_gc(top+1, obj2gco(L));  /* Dummy frame object. */
  setframe_ftsz(top+1, (int)((char *)(top+2) - (char *)base) + FRAME_CONT);
  L->base = L->top = top+2;
  /*
  ** before:   [old_mo|PC]    [... ...]
  **                         ^base     ^top
  ** after:    [new_mo|itype] [... ...] [NULL|PC] [dummy|delta]
  **                                                           ^base/top
  ** tailcall: [new_mo|PC]    [... ...]
  **                         ^base     ^top
  */
  return 0;
}
#endif

/* Setup call to metamethod to be run by Assembler VM. */
static TValue *mmcall(lua_State *L, ASMFunction cont, cTValue *mo,
		    cTValue *a, cTValue *b)
{
  /*
  **           |-- framesize -> top       top+1       top+2 top+3
  ** before:   [func slots ...]
  ** mm setup: [func slots ...] [cont|?]  [mo|tmtype] [a]   [b]
  ** in asm:   [func slots ...] [cont|PC] [mo|delta]  [a]   [b]
  **           ^-- func base                          ^-- mm base
  ** after mm: [func slots ...]           [result]
  **                ^-- copy to base[PC_RA] --/     for lj_cont_ra
  **                          istruecond + branch   for lj_cont_cond*
  **                                       ignore   for lj_cont_nop
  ** next PC:  [func slots ...]
  */
  TValue *top = L->top;
  if (curr_funcisL(L)) top = curr_topL(L);
  setcont(top, cont);  /* Assembler VM stores PC in upper word. */
  copyTV(L, top+1, mo);  /* Store metamethod and two arguments. */
  copyTV(L, top+2, a);
  copyTV(L, top+3, b);
  return top+2;  /* Return new base. */
}

/* -- C helpers for some instructions, called from assembler VM ----------- */

/* Helper for TGET*. __index chain and metamethod. */
cTValue *lj_meta_tget(lua_State *L, cTValue *o, cTValue *k)
{
  int loop;
  for (loop = 0; loop < LJ_MAX_IDXCHAIN; loop++) {
    cTValue *mo;
    if (LJ_LIKELY(tvistab(o))) {
      GCtab *t = tabV(o);
      cTValue *tv = lj_tab_get(L, t, k);
      if (!tvisnil(tv) ||
	  !(mo = lj_meta_fast(L, tabref(t->metatable), MM_index)))
	return tv;
    } else if (tvisnil(mo = lj_meta_lookup(L, o, MM_index))) {
      lj_err_optype(L, o, LJ_ERR_OPINDEX);
      return NULL;  /* unreachable */
    }
    if (tvisfunc(mo)) {
      L->top = mmcall(L, lj_cont_ra, mo, o, k);
      return NULL;  /* Trigger metamethod call. */
    }
    o = mo;
  }
  lj_err_msg(L, LJ_ERR_GETLOOP);
  return NULL;  /* unreachable */
}

/* Helper for TSET*. __newindex chain and metamethod. */
TValue *lj_meta_tset(lua_State *L, cTValue *o, cTValue *k)
{
  TValue tmp;
  int loop;
  for (loop = 0; loop < LJ_MAX_IDXCHAIN; loop++) {
    cTValue *mo;
    if (LJ_LIKELY(tvistab(o))) {
      GCtab *t = tabV(o);
      cTValue *tv = lj_tab_get(L, t, k);
      if (LJ_LIKELY(!tvisnil(tv))) {
	t->nomm = 0;  /* Invalidate negative metamethod cache. */
	lj_gc_anybarriert(L, t);
	return (TValue *)tv;
      } else if (!(mo = lj_meta_fast(L, tabref(t->metatable), MM_newindex))) {
	t->nomm = 0;  /* Invalidate negative metamethod cache. */
	lj_gc_anybarriert(L, t);
	if (tv != niltv(L))
	  return (TValue *)tv;
	if (tvisnil(k)) lj_err_msg(L, LJ_ERR_NILIDX);
	else if (tvisint(k)) { setnumV(&tmp, (lua_Number)intV(k)); k = &tmp; }
	else if (tvisnum(k) && tvisnan(k)) lj_err_msg(L, LJ_ERR_NANIDX);
	return lj_tab_newkey(L, t, k);
      }
    } else if (tvisnil(mo = lj_meta_lookup(L, o, MM_newindex))) {
      lj_err_optype(L, o, LJ_ERR_OPINDEX);
      return NULL;  /* unreachable */
    }
    if (tvisfunc(mo)) {
      L->top = mmcall(L, lj_cont_nop, mo, o, k);
      /* L->top+2 = v filled in by caller. */
      return NULL;  /* Trigger metamethod call. */
    }
    copyTV(L, &tmp, mo);
    o = &tmp;
  }
  lj_err_msg(L, LJ_ERR_SETLOOP);
  return NULL;  /* unreachable */
}

static cTValue *str2num(cTValue *o, TValue *n)
{
  if (tvisnum(o))
    return o;
  else if (tvisint(o))
    return (setnumV(n, (lua_Number)intV(o)), n);
  else if (tvisstr(o) && lj_strscan_num(strV(o), n))
    return n;
  else
    return NULL;
}

/* Helper for arithmetic instructions. Coercion, metamethod. */
TValue *lj_meta_arith(lua_State *L, TValue *ra, cTValue *rb, cTValue *rc,
		      BCReg op)
{
  MMS mm = bcmode_mm(op);
  TValue tempb, tempc;
  cTValue *b, *c;
  if ((b = str2num(rb, &tempb)) != NULL &&
      (c = str2num(rc, &tempc)) != NULL) {  /* Try coercion first. */
    setnumV(ra, lj_vm_foldarith(numV(b), numV(c), (int)mm-MM_add));
    return NULL;
  } else {
    cTValue *mo = lj_meta_lookup(L, rb, mm);
    if (tvisnil(mo)) {
      mo = lj_meta_lookup(L, rc, mm);
      if (tvisnil(mo)) {
	if (str2num(rb, &tempb) == NULL) rc = rb;
	lj_err_optype(L, rc, LJ_ERR_OPARITH);
	return NULL;  /* unreachable */
      }
    }
    return mmcall(L, lj_cont_ra, mo, rb, rc);
  }
}

/* In-place coercion of a number to a string. */
static LJ_AINLINE int tostring(lua_State *L, TValue *o)
{
  if (tvisstr(o)) {
    return 1;
  } else if (tvisnumber(o)) {
    setstrV(L, o, lj_str_fromnumber(L, o));
    return 1;
  } else {
    return 0;
  }
}

/* Helper for CAT. Coercion, iterative concat, __concat metamethod. */
TValue *lj_meta_cat(lua_State *L, TValue *top, int left)
{
  int fromc = 0;
  if (left < 0) { left = -left; fromc = 1; }
  do {
    int n = 1;
    if (!(tvisstr(top-1) || tvisnumber(top-1)) || !tostring(L, top)) {
      cTValue *mo = lj_meta_lookup(L, top-1, MM_concat);
      if (tvisnil(mo)) {
	mo = lj_meta_lookup(L, top, MM_concat);
	if (tvisnil(mo)) {
	  if (tvisstr(top-1) || tvisnumber(top-1)) top++;
	  lj_err_optype(L, top-1, LJ_ERR_OPCAT);
	  return NULL;  /* unreachable */
	}
      }
      /* One of the top two elements is not a string, call __cat metamethod:
      **
      ** before:    [...][CAT stack .........................]
      **                                 top-1     top         top+1 top+2
      ** pick two:  [...][CAT stack ...] [o1]      [o2]
      ** setup mm:  [...][CAT stack ...] [cont|?]  [mo|tmtype] [o1]  [o2]
      ** in asm:    [...][CAT stack ...] [cont|PC] [mo|delta]  [o1]  [o2]
      **            ^-- func base                              ^-- mm base
      ** after mm:  [...][CAT stack ...] <--push-- [result]
      ** next step: [...][CAT stack .............]
      */
      copyTV(L, top+2, top);  /* Careful with the order of stack copies! */
      copyTV(L, top+1, top-1);
      copyTV(L, top, mo);
      setcont(top-1, lj_cont_cat);
      return top+1;  /* Trigger metamethod call. */
    } else if (strV(top)->len == 0) {  /* Shortcut. */
      (void)tostring(L, top-1);
    } else {
      /* Pick as many strings as possible from the top and concatenate them:
      **
      ** before:    [...][CAT stack ...........................]
      ** pick str:  [...][CAT stack ...] [...... strings ......]
      ** concat:    [...][CAT stack ...] [result]
      ** next step: [...][CAT stack ............]
      */
      MSize tlen = strV(top)->len;
      char *buffer;
      int i;
      for (n = 1; n <= left && tostring(L, top-n); n++) {
	MSize len = strV(top-n)->len;
	if (len >= LJ_MAX_STR - tlen)
	  lj_err_msg(L, LJ_ERR_STROV);
	tlen += len;
      }
      buffer = lj_str_needbuf(L, &G(L)->tmpbuf, tlen);
      n--;
      tlen = 0;
      for (i = n; i >= 0; i--) {
	MSize len = strV(top-i)->len;
	memcpy(buffer + tlen, strVdata(top-i), len);
	tlen += len;
      }
      setstrV(L, top-n, lj_str_new(L, buffer, tlen));
    }
    left -= n;
    top -= n;
  } while (left >= 1);
  if (LJ_UNLIKELY(G(L)->gc.total >= G(L)->gc.threshold)) {
    if (!fromc) L->top = curr_topL(L);
    lj_gc_step(L);
  }
  return NULL;
}

/* Helper for LEN. __len metamethod. */
TValue * LJ_FASTCALL lj_meta_len(lua_State *L, cTValue *o)
{
  cTValue *mo = lj_meta_lookup(L, o, MM_len);
  if (tvisnil(mo)) {
    if (LJ_52 && tvistab(o))
      tabref(tabV(o)->metatable)->nomm |= (uint8_t)(1u<<MM_len);
    else
      lj_err_optype(L, o, LJ_ERR_OPLEN);
    return NULL;
  }
  return mmcall(L, lj_cont_ra, mo, o, LJ_52 ? o : niltv(L));
}

/* Helper for equality comparisons. __eq metamethod. */
TValue *lj_meta_equal(lua_State *L, GCobj *o1, GCobj *o2, int ne)
{
  /* Field metatable must be at same offset for GCtab and GCudata! */
  cTValue *mo = lj_meta_fast(L, tabref(o1->gch.metatable), MM_eq);
  if (mo) {
    TValue *top;
    uint32_t it;
    if (tabref(o1->gch.metatable) != tabref(o2->gch.metatable)) {
      cTValue *mo2 = lj_meta_fast(L, tabref(o2->gch.metatable), MM_eq);
      if (mo2 == NULL || !lj_obj_equal(mo, mo2))
	return (TValue *)(intptr_t)ne;
    }
    top = curr_top(L);
    setcont(top, ne ? lj_cont_condf : lj_cont_condt);
    copyTV(L, top+1, mo);
    it = ~(uint32_t)o1->gch.gct;
    setgcV(L, top+2, o1, it);
    setgcV(L, top+3, o2, it);
    return top+2;  /* Trigger metamethod call. */
  }
  return (TValue *)(intptr_t)ne;
}

#if LJ_HASFFI
TValue * LJ_FASTCALL lj_meta_equal_cd(lua_State *L, BCIns ins)
{
  ASMFunction cont = (bc_op(ins) & 1) ? lj_cont_condf : lj_cont_condt;
  int op = (int)bc_op(ins) & ~1;
  TValue tv;
  cTValue *mo, *o2, *o1 = &L->base[bc_a(ins)];
  cTValue *o1mm = o1;
  if (op == BC_ISEQV) {
    o2 = &L->base[bc_d(ins)];
    if (!tviscdata(o1mm)) o1mm = o2;
  } else if (op == BC_ISEQS) {
    setstrV(L, &tv, gco2str(proto_kgc(curr_proto(L), ~(ptrdiff_t)bc_d(ins))));
    o2 = &tv;
  } else if (op == BC_ISEQN) {
    o2 = &mref(curr_proto(L)->k, cTValue)[bc_d(ins)];
  } else {
    lua_assert(op == BC_ISEQP);
    setitype(&tv, ~bc_d(ins));
    o2 = &tv;
  }
  mo = lj_meta_lookup(L, o1mm, MM_eq);
  if (LJ_LIKELY(!tvisnil(mo)))
    return mmcall(L, cont, mo, o1, o2);
  else
    return (TValue *)(intptr_t)(bc_op(ins) & 1);
}
#endif

/* Helper for ordered comparisons. String compare, __lt/__le metamethods. */
TValue *lj_meta_comp(lua_State *L, cTValue *o1, cTValue *o2, int op)
{
  if (LJ_HASFFI && (tviscdata(o1) || tviscdata(o2))) {
    ASMFunction cont = (op & 1) ? lj_cont_condf : lj_cont_condt;
    MMS mm = (op & 2) ? MM_le : MM_lt;
    cTValue *mo = lj_meta_lookup(L, tviscdata(o1) ? o1 : o2, mm);
    if (LJ_UNLIKELY(tvisnil(mo))) goto err;
    return mmcall(L, cont, mo, o1, o2);
  } else if (LJ_52 || itype(o1) == itype(o2)) {
    /* Never called with two numbers. */
    if (tvisstr(o1) && tvisstr(o2)) {
      int32_t res = lj_str_cmp(strV(o1), strV(o2));
      return (TValue *)(intptr_t)(((op&2) ? res <= 0 : res < 0) ^ (op&1));
    } else {
    trymt:
      while (1) {
	ASMFunction cont = (op & 1) ? lj_cont_condf : lj_cont_condt;
	MMS mm = (op & 2) ? MM_le : MM_lt;
	cTValue *mo = lj_meta_lookup(L, o1, mm);
#if LJ_52
	if (tvisnil(mo) && tvisnil((mo = lj_meta_lookup(L, o2, mm))))
#else
	cTValue *mo2 = lj_meta_lookup(L, o2, mm);
	if (tvisnil(mo) || !lj_obj_equal(mo, mo2))
#endif
	{
	  if (op & 2) {  /* MM_le not found: retry with MM_lt. */
	    cTValue *ot = o1; o1 = o2; o2 = ot;  /* Swap operands. */
	    op ^= 3;  /* Use LT and flip condition. */
	    continue;
	  }
	  goto err;
	}
	return mmcall(L, cont, mo, o1, o2);
      }
    }
  } else if (tvisbool(o1) && tvisbool(o2)) {
    goto trymt;
  } else {
  err:
    lj_err_comp(L, o1, o2);
    return NULL;
  }
}

/* Helper for calls. __call metamethod. */
void lj_meta_call(lua_State *L, TValue *func, TValue *top)
{
  cTValue *mo = lj_meta_lookup(L, func, MM_call);
  TValue *p;
  if (!tvisfunc(mo))
    lj_err_optype_call(L, func);
  for (p = top; p > func; p--) copyTV(L, p, p-1);
  copyTV(L, func, mo);
}

/* Helper for FORI. Coercion. */
void LJ_FASTCALL lj_meta_for(lua_State *L, TValue *o)
{
  if (!lj_strscan_numberobj(o)) lj_err_msg(L, LJ_ERR_FORINIT);
  if (!lj_strscan_numberobj(o+1)) lj_err_msg(L, LJ_ERR_FORLIM);
  if (!lj_strscan_numberobj(o+2)) lj_err_msg(L, LJ_ERR_FORSTEP);
  if (LJ_DUALNUM) {
    /* Ensure all slots are integers or all slots are numbers. */
    int32_t k[3];
    int nint = 0;
    ptrdiff_t i;
    for (i = 0; i <= 2; i++) {
      if (tvisint(o+i)) {
	k[i] = intV(o+i); nint++;
      } else {
	k[i] = lj_num2int(numV(o+i)); nint += ((lua_Number)k[i] == numV(o+i));
      }
    }
    if (nint == 3) {  /* Narrow to integers. */
      setintV(o, k[0]);
      setintV(o+1, k[1]);
      setintV(o+2, k[2]);
    } else if (nint != 0) {  /* Widen to numbers. */
      if (tvisint(o)) setnumV(o, (lua_Number)intV(o));
      if (tvisint(o+1)) setnumV(o+1, (lua_Number)intV(o+1));
      if (tvisint(o+2)) setnumV(o+2, (lua_Number)intV(o+2));
    }
  }
}

