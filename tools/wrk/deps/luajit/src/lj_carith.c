/*
** C data arithmetic.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#include "lj_obj.h"

#if LJ_HASFFI

#include "lj_gc.h"
#include "lj_err.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_ctype.h"
#include "lj_cconv.h"
#include "lj_cdata.h"
#include "lj_carith.h"

/* -- C data arithmetic --------------------------------------------------- */

/* Binary operands of an operator converted to ctypes. */
typedef struct CDArith {
  uint8_t *p[2];
  CType *ct[2];
} CDArith;

/* Check arguments for arithmetic metamethods. */
static int carith_checkarg(lua_State *L, CTState *cts, CDArith *ca)
{
  TValue *o = L->base;
  int ok = 1;
  MSize i;
  if (o+1 >= L->top)
    lj_err_argt(L, 1, LUA_TCDATA);
  for (i = 0; i < 2; i++, o++) {
    if (tviscdata(o)) {
      GCcdata *cd = cdataV(o);
      CTypeID id = (CTypeID)cd->ctypeid;
      CType *ct = ctype_raw(cts, id);
      uint8_t *p = (uint8_t *)cdataptr(cd);
      if (ctype_isptr(ct->info)) {
	p = (uint8_t *)cdata_getptr(p, ct->size);
	if (ctype_isref(ct->info)) ct = ctype_rawchild(cts, ct);
      } else if (ctype_isfunc(ct->info)) {
	p = (uint8_t *)*(void **)p;
	ct = ctype_get(cts,
	  lj_ctype_intern(cts, CTINFO(CT_PTR, CTALIGN_PTR|id), CTSIZE_PTR));
      }
      if (ctype_isenum(ct->info)) ct = ctype_child(cts, ct);
      ca->ct[i] = ct;
      ca->p[i] = p;
    } else if (tvisint(o)) {
      ca->ct[i] = ctype_get(cts, CTID_INT32);
      ca->p[i] = (uint8_t *)&o->i;
    } else if (tvisnum(o)) {
      ca->ct[i] = ctype_get(cts, CTID_DOUBLE);
      ca->p[i] = (uint8_t *)&o->n;
    } else if (tvisnil(o)) {
      ca->ct[i] = ctype_get(cts, CTID_P_VOID);
      ca->p[i] = (uint8_t *)0;
    } else if (tvisstr(o)) {
      TValue *o2 = i == 0 ? o+1 : o-1;
      CType *ct = ctype_raw(cts, cdataV(o2)->ctypeid);
      ca->ct[i] = NULL;
      ca->p[i] = NULL;
      ok = 0;
      if (ctype_isenum(ct->info)) {
	CTSize ofs;
	CType *cct = lj_ctype_getfield(cts, ct, strV(o), &ofs);
	if (cct && ctype_isconstval(cct->info)) {
	  ca->ct[i] = ctype_child(cts, cct);
	  ca->p[i] = (uint8_t *)&cct->size;  /* Assumes ct does not grow. */
	  ok = 1;
	} else {
	  ca->ct[1-i] = ct;  /* Use enum to improve error message. */
	  ca->p[1-i] = NULL;
	  break;
	}
      }
    } else {
      ca->ct[i] = NULL;
      ca->p[i] = NULL;
      ok = 0;
    }
  }
  return ok;
}

/* Pointer arithmetic. */
static int carith_ptr(lua_State *L, CTState *cts, CDArith *ca, MMS mm)
{
  CType *ctp = ca->ct[0];
  uint8_t *pp = ca->p[0];
  ptrdiff_t idx;
  CTSize sz;
  CTypeID id;
  GCcdata *cd;
  if (ctype_isptr(ctp->info) || ctype_isrefarray(ctp->info)) {
    if ((mm == MM_sub || mm == MM_eq || mm == MM_lt || mm == MM_le) &&
	(ctype_isptr(ca->ct[1]->info) || ctype_isrefarray(ca->ct[1]->info))) {
      uint8_t *pp2 = ca->p[1];
      if (mm == MM_eq) {  /* Pointer equality. Incompatible pointers are ok. */
	setboolV(L->top-1, (pp == pp2));
	return 1;
      }
      if (!lj_cconv_compatptr(cts, ctp, ca->ct[1], CCF_IGNQUAL))
	return 0;
      if (mm == MM_sub) {  /* Pointer difference. */
	intptr_t diff;
	sz = lj_ctype_size(cts, ctype_cid(ctp->info));  /* Element size. */
	if (sz == 0 || sz == CTSIZE_INVALID)
	  return 0;
	diff = ((intptr_t)pp - (intptr_t)pp2) / (int32_t)sz;
	/* All valid pointer differences on x64 are in (-2^47, +2^47),
	** which fits into a double without loss of precision.
	*/
	setintptrV(L->top-1, (int32_t)diff);
	return 1;
      } else if (mm == MM_lt) {  /* Pointer comparison (unsigned). */
	setboolV(L->top-1, ((uintptr_t)pp < (uintptr_t)pp2));
	return 1;
      } else {
	lua_assert(mm == MM_le);
	setboolV(L->top-1, ((uintptr_t)pp <= (uintptr_t)pp2));
	return 1;
      }
    }
    if (!((mm == MM_add || mm == MM_sub) && ctype_isnum(ca->ct[1]->info)))
      return 0;
    lj_cconv_ct_ct(cts, ctype_get(cts, CTID_INT_PSZ), ca->ct[1],
		   (uint8_t *)&idx, ca->p[1], 0);
    if (mm == MM_sub) idx = -idx;
  } else if (mm == MM_add && ctype_isnum(ctp->info) &&
      (ctype_isptr(ca->ct[1]->info) || ctype_isrefarray(ca->ct[1]->info))) {
    /* Swap pointer and index. */
    ctp = ca->ct[1]; pp = ca->p[1];
    lj_cconv_ct_ct(cts, ctype_get(cts, CTID_INT_PSZ), ca->ct[0],
		   (uint8_t *)&idx, ca->p[0], 0);
  } else {
    return 0;
  }
  sz = lj_ctype_size(cts, ctype_cid(ctp->info));  /* Element size. */
  if (sz == CTSIZE_INVALID)
    return 0;
  pp += idx*(int32_t)sz;  /* Compute pointer + index. */
  id = lj_ctype_intern(cts, CTINFO(CT_PTR, CTALIGN_PTR|ctype_cid(ctp->info)),
		       CTSIZE_PTR);
  cd = lj_cdata_new(cts, id, CTSIZE_PTR);
  *(uint8_t **)cdataptr(cd) = pp;
  setcdataV(L, L->top-1, cd);
  lj_gc_check(L);
  return 1;
}

/* 64 bit integer arithmetic. */
static int carith_int64(lua_State *L, CTState *cts, CDArith *ca, MMS mm)
{
  if (ctype_isnum(ca->ct[0]->info) && ca->ct[0]->size <= 8 &&
      ctype_isnum(ca->ct[1]->info) && ca->ct[1]->size <= 8) {
    CTypeID id = (((ca->ct[0]->info & CTF_UNSIGNED) && ca->ct[0]->size == 8) ||
		  ((ca->ct[1]->info & CTF_UNSIGNED) && ca->ct[1]->size == 8)) ?
		 CTID_UINT64 : CTID_INT64;
    CType *ct = ctype_get(cts, id);
    GCcdata *cd;
    uint64_t u0, u1, *up;
    lj_cconv_ct_ct(cts, ct, ca->ct[0], (uint8_t *)&u0, ca->p[0], 0);
    if (mm != MM_unm)
      lj_cconv_ct_ct(cts, ct, ca->ct[1], (uint8_t *)&u1, ca->p[1], 0);
    switch (mm) {
    case MM_eq:
      setboolV(L->top-1, (u0 == u1));
      return 1;
    case MM_lt:
      setboolV(L->top-1,
	       id == CTID_INT64 ? ((int64_t)u0 < (int64_t)u1) : (u0 < u1));
      return 1;
    case MM_le:
      setboolV(L->top-1,
	       id == CTID_INT64 ? ((int64_t)u0 <= (int64_t)u1) : (u0 <= u1));
      return 1;
    default: break;
    }
    cd = lj_cdata_new(cts, id, 8);
    up = (uint64_t *)cdataptr(cd);
    setcdataV(L, L->top-1, cd);
    switch (mm) {
    case MM_add: *up = u0 + u1; break;
    case MM_sub: *up = u0 - u1; break;
    case MM_mul: *up = u0 * u1; break;
    case MM_div:
      if (id == CTID_INT64)
	*up = (uint64_t)lj_carith_divi64((int64_t)u0, (int64_t)u1);
      else
	*up = lj_carith_divu64(u0, u1);
      break;
    case MM_mod:
      if (id == CTID_INT64)
	*up = (uint64_t)lj_carith_modi64((int64_t)u0, (int64_t)u1);
      else
	*up = lj_carith_modu64(u0, u1);
      break;
    case MM_pow:
      if (id == CTID_INT64)
	*up = (uint64_t)lj_carith_powi64((int64_t)u0, (int64_t)u1);
      else
	*up = lj_carith_powu64(u0, u1);
      break;
    case MM_unm: *up = (uint64_t)-(int64_t)u0; break;
    default: lua_assert(0); break;
    }
    lj_gc_check(L);
    return 1;
  }
  return 0;
}

/* Handle ctype arithmetic metamethods. */
static int lj_carith_meta(lua_State *L, CTState *cts, CDArith *ca, MMS mm)
{
  cTValue *tv = NULL;
  if (tviscdata(L->base)) {
    CTypeID id = cdataV(L->base)->ctypeid;
    CType *ct = ctype_raw(cts, id);
    if (ctype_isptr(ct->info)) id = ctype_cid(ct->info);
    tv = lj_ctype_meta(cts, id, mm);
  }
  if (!tv && L->base+1 < L->top && tviscdata(L->base+1)) {
    CTypeID id = cdataV(L->base+1)->ctypeid;
    CType *ct = ctype_raw(cts, id);
    if (ctype_isptr(ct->info)) id = ctype_cid(ct->info);
    tv = lj_ctype_meta(cts, id, mm);
  }
  if (!tv) {
    const char *repr[2];
    int i, isenum = -1, isstr = -1;
    if (mm == MM_eq) {  /* Equality checks never raise an error. */
      setboolV(L->top-1, 0);
      return 1;
    }
    for (i = 0; i < 2; i++) {
      if (ca->ct[i] && tviscdata(L->base+i)) {
	if (ctype_isenum(ca->ct[i]->info)) isenum = i;
	repr[i] = strdata(lj_ctype_repr(L, ctype_typeid(cts, ca->ct[i]), NULL));
      } else {
	if (tvisstr(&L->base[i])) isstr = i;
	repr[i] = lj_typename(&L->base[i]);
      }
    }
    if ((isenum ^ isstr) == 1)
      lj_err_callerv(L, LJ_ERR_FFI_BADCONV, repr[isstr], repr[isenum]);
    lj_err_callerv(L, mm == MM_len ? LJ_ERR_FFI_BADLEN :
		      mm == MM_concat ? LJ_ERR_FFI_BADCONCAT :
		      mm < MM_add ? LJ_ERR_FFI_BADCOMP : LJ_ERR_FFI_BADARITH,
		   repr[0], repr[1]);
  }
  return lj_meta_tailcall(L, tv);
}

/* Arithmetic operators for cdata. */
int lj_carith_op(lua_State *L, MMS mm)
{
  CTState *cts = ctype_cts(L);
  CDArith ca;
  if (carith_checkarg(L, cts, &ca)) {
    if (carith_int64(L, cts, &ca, mm) || carith_ptr(L, cts, &ca, mm)) {
      copyTV(L, &G(L)->tmptv2, L->top-1);  /* Remember for trace recorder. */
      return 1;
    }
  }
  return lj_carith_meta(L, cts, &ca, mm);
}

/* -- 64 bit integer arithmetic helpers ----------------------------------- */

#if LJ_32 && LJ_HASJIT
/* Signed/unsigned 64 bit multiplication. */
int64_t lj_carith_mul64(int64_t a, int64_t b)
{
  return a * b;
}
#endif

/* Unsigned 64 bit division. */
uint64_t lj_carith_divu64(uint64_t a, uint64_t b)
{
  if (b == 0) return U64x(80000000,00000000);
  return a / b;
}

/* Signed 64 bit division. */
int64_t lj_carith_divi64(int64_t a, int64_t b)
{
  if (b == 0 || (a == (int64_t)U64x(80000000,00000000) && b == -1))
    return U64x(80000000,00000000);
  return a / b;
}

/* Unsigned 64 bit modulo. */
uint64_t lj_carith_modu64(uint64_t a, uint64_t b)
{
  if (b == 0) return U64x(80000000,00000000);
  return a % b;
}

/* Signed 64 bit modulo. */
int64_t lj_carith_modi64(int64_t a, int64_t b)
{
  if (b == 0) return U64x(80000000,00000000);
  if (a == (int64_t)U64x(80000000,00000000) && b == -1) return 0;
  return a % b;
}

/* Unsigned 64 bit x^k. */
uint64_t lj_carith_powu64(uint64_t x, uint64_t k)
{
  uint64_t y;
  if (k == 0)
    return 1;
  for (; (k & 1) == 0; k >>= 1) x *= x;
  y = x;
  if ((k >>= 1) != 0) {
    for (;;) {
      x *= x;
      if (k == 1) break;
      if (k & 1) y *= x;
      k >>= 1;
    }
    y *= x;
  }
  return y;
}

/* Signed 64 bit x^k. */
int64_t lj_carith_powi64(int64_t x, int64_t k)
{
  if (k == 0)
    return 1;
  if (k < 0) {
    if (x == 0)
      return U64x(7fffffff,ffffffff);
    else if (x == 1)
      return 1;
    else if (x == -1)
      return (k & 1) ? -1 : 1;
    else
      return 0;
  }
  return (int64_t)lj_carith_powu64((uint64_t)x, (uint64_t)k);
}

#endif
