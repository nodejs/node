/*
** FFI library.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lib_ffi_c
#define LUA_LIB

#include <errno.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"

#if LJ_HASFFI

#include "lj_gc.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_ctype.h"
#include "lj_cparse.h"
#include "lj_cdata.h"
#include "lj_cconv.h"
#include "lj_carith.h"
#include "lj_ccall.h"
#include "lj_ccallback.h"
#include "lj_clib.h"
#include "lj_ff.h"
#include "lj_lib.h"

/* -- C type checks ------------------------------------------------------- */

/* Check first argument for a C type and returns its ID. */
static CTypeID ffi_checkctype(lua_State *L, CTState *cts, TValue *param)
{
  TValue *o = L->base;
  if (!(o < L->top)) {
  err_argtype:
    lj_err_argtype(L, 1, "C type");
  }
  if (tvisstr(o)) {  /* Parse an abstract C type declaration. */
    GCstr *s = strV(o);
    CPState cp;
    int errcode;
    cp.L = L;
    cp.cts = cts;
    cp.srcname = strdata(s);
    cp.p = strdata(s);
    cp.param = param;
    cp.mode = CPARSE_MODE_ABSTRACT|CPARSE_MODE_NOIMPLICIT;
    errcode = lj_cparse(&cp);
    if (errcode) lj_err_throw(L, errcode);  /* Propagate errors. */
    return cp.val.id;
  } else {
    GCcdata *cd;
    if (!tviscdata(o)) goto err_argtype;
    if (param && param < L->top) lj_err_arg(L, 1, LJ_ERR_FFI_NUMPARAM);
    cd = cdataV(o);
    return cd->ctypeid == CTID_CTYPEID ? *(CTypeID *)cdataptr(cd) : cd->ctypeid;
  }
}

/* Check argument for C data and return it. */
static GCcdata *ffi_checkcdata(lua_State *L, int narg)
{
  TValue *o = L->base + narg-1;
  if (!(o < L->top && tviscdata(o)))
    lj_err_argt(L, narg, LUA_TCDATA);
  return cdataV(o);
}

/* Convert argument to C pointer. */
static void *ffi_checkptr(lua_State *L, int narg, CTypeID id)
{
  CTState *cts = ctype_cts(L);
  TValue *o = L->base + narg-1;
  void *p;
  if (o >= L->top)
    lj_err_arg(L, narg, LJ_ERR_NOVAL);
  lj_cconv_ct_tv(cts, ctype_get(cts, id), (uint8_t *)&p, o, CCF_ARG(narg));
  return p;
}

/* Convert argument to int32_t. */
static int32_t ffi_checkint(lua_State *L, int narg)
{
  CTState *cts = ctype_cts(L);
  TValue *o = L->base + narg-1;
  int32_t i;
  if (o >= L->top)
    lj_err_arg(L, narg, LJ_ERR_NOVAL);
  lj_cconv_ct_tv(cts, ctype_get(cts, CTID_INT32), (uint8_t *)&i, o,
		 CCF_ARG(narg));
  return i;
}

/* -- C type metamethods -------------------------------------------------- */

#define LJLIB_MODULE_ffi_meta

/* Handle ctype __index/__newindex metamethods. */
static int ffi_index_meta(lua_State *L, CTState *cts, CType *ct, MMS mm)
{
  CTypeID id = ctype_typeid(cts, ct);
  cTValue *tv = lj_ctype_meta(cts, id, mm);
  TValue *base = L->base;
  if (!tv) {
    const char *s;
  err_index:
    s = strdata(lj_ctype_repr(L, id, NULL));
    if (tvisstr(L->base+1)) {
      lj_err_callerv(L, LJ_ERR_FFI_BADMEMBER, s, strVdata(L->base+1));
    } else {
      const char *key = tviscdata(L->base+1) ?
	strdata(lj_ctype_repr(L, cdataV(L->base+1)->ctypeid, NULL)) :
	lj_typename(L->base+1);
      lj_err_callerv(L, LJ_ERR_FFI_BADIDXW, s, key);
    }
  }
  if (!tvisfunc(tv)) {
    if (mm == MM_index) {
      cTValue *o = lj_meta_tget(L, tv, base+1);
      if (o) {
	if (tvisnil(o)) goto err_index;
	copyTV(L, L->top-1, o);
	return 1;
      }
    } else {
      TValue *o = lj_meta_tset(L, tv, base+1);
      if (o) {
	copyTV(L, o, base+2);
	return 0;
      }
    }
    tv = L->top-1;
  }
  return lj_meta_tailcall(L, tv);
}

LJLIB_CF(ffi_meta___index)	LJLIB_REC(cdata_index 0)
{
  CTState *cts = ctype_cts(L);
  CTInfo qual = 0;
  CType *ct;
  uint8_t *p;
  TValue *o = L->base;
  if (!(o+1 < L->top && tviscdata(o)))  /* Also checks for presence of key. */
    lj_err_argt(L, 1, LUA_TCDATA);
  ct = lj_cdata_index(cts, cdataV(o), o+1, &p, &qual);
  if ((qual & 1))
    return ffi_index_meta(L, cts, ct, MM_index);
  if (lj_cdata_get(cts, ct, L->top-1, p))
    lj_gc_check(L);
  return 1;
}

LJLIB_CF(ffi_meta___newindex)	LJLIB_REC(cdata_index 1)
{
  CTState *cts = ctype_cts(L);
  CTInfo qual = 0;
  CType *ct;
  uint8_t *p;
  TValue *o = L->base;
  if (!(o+2 < L->top && tviscdata(o)))  /* Also checks for key and value. */
    lj_err_argt(L, 1, LUA_TCDATA);
  ct = lj_cdata_index(cts, cdataV(o), o+1, &p, &qual);
  if ((qual & 1)) {
    if ((qual & CTF_CONST))
      lj_err_caller(L, LJ_ERR_FFI_WRCONST);
    return ffi_index_meta(L, cts, ct, MM_newindex);
  }
  lj_cdata_set(cts, ct, p, o+2, qual);
  return 0;
}

/* Common handler for cdata arithmetic. */
static int ffi_arith(lua_State *L)
{
  MMS mm = (MMS)(curr_func(L)->c.ffid - (int)FF_ffi_meta___eq + (int)MM_eq);
  return lj_carith_op(L, mm);
}

/* The following functions must be in contiguous ORDER MM. */
LJLIB_CF(ffi_meta___eq)		LJLIB_REC(cdata_arith MM_eq)
{
  return ffi_arith(L);
}

LJLIB_CF(ffi_meta___len)	LJLIB_REC(cdata_arith MM_len)
{
  return ffi_arith(L);
}

LJLIB_CF(ffi_meta___lt)		LJLIB_REC(cdata_arith MM_lt)
{
  return ffi_arith(L);
}

LJLIB_CF(ffi_meta___le)		LJLIB_REC(cdata_arith MM_le)
{
  return ffi_arith(L);
}

LJLIB_CF(ffi_meta___concat)	LJLIB_REC(cdata_arith MM_concat)
{
  return ffi_arith(L);
}

/* Forward declaration. */
static int lj_cf_ffi_new(lua_State *L);

LJLIB_CF(ffi_meta___call)	LJLIB_REC(cdata_call)
{
  CTState *cts = ctype_cts(L);
  GCcdata *cd = ffi_checkcdata(L, 1);
  CTypeID id = cd->ctypeid;
  CType *ct;
  cTValue *tv;
  MMS mm = MM_call;
  if (cd->ctypeid == CTID_CTYPEID) {
    id = *(CTypeID *)cdataptr(cd);
    mm = MM_new;
  } else {
    int ret = lj_ccall_func(L, cd);
    if (ret >= 0)
      return ret;
  }
  /* Handle ctype __call/__new metamethod. */
  ct = ctype_raw(cts, id);
  if (ctype_isptr(ct->info)) id = ctype_cid(ct->info);
  tv = lj_ctype_meta(cts, id, mm);
  if (tv)
    return lj_meta_tailcall(L, tv);
  else if (mm == MM_call)
    lj_err_callerv(L, LJ_ERR_FFI_BADCALL, strdata(lj_ctype_repr(L, id, NULL)));
  return lj_cf_ffi_new(L);
}

LJLIB_CF(ffi_meta___add)	LJLIB_REC(cdata_arith MM_add)
{
  return ffi_arith(L);
}

LJLIB_CF(ffi_meta___sub)	LJLIB_REC(cdata_arith MM_sub)
{
  return ffi_arith(L);
}

LJLIB_CF(ffi_meta___mul)	LJLIB_REC(cdata_arith MM_mul)
{
  return ffi_arith(L);
}

LJLIB_CF(ffi_meta___div)	LJLIB_REC(cdata_arith MM_div)
{
  return ffi_arith(L);
}

LJLIB_CF(ffi_meta___mod)	LJLIB_REC(cdata_arith MM_mod)
{
  return ffi_arith(L);
}

LJLIB_CF(ffi_meta___pow)	LJLIB_REC(cdata_arith MM_pow)
{
  return ffi_arith(L);
}

LJLIB_CF(ffi_meta___unm)	LJLIB_REC(cdata_arith MM_unm)
{
  return ffi_arith(L);
}
/* End of contiguous ORDER MM. */

LJLIB_CF(ffi_meta___tostring)
{
  GCcdata *cd = ffi_checkcdata(L, 1);
  const char *msg = "cdata<%s>: %p";
  CTypeID id = cd->ctypeid;
  void *p = cdataptr(cd);
  if (id == CTID_CTYPEID) {
    msg = "ctype<%s>";
    id = *(CTypeID *)p;
  } else {
    CTState *cts = ctype_cts(L);
    CType *ct = ctype_raw(cts, id);
    if (ctype_isref(ct->info)) {
      p = *(void **)p;
      ct = ctype_rawchild(cts, ct);
    }
    if (ctype_iscomplex(ct->info)) {
      setstrV(L, L->top-1, lj_ctype_repr_complex(L, cdataptr(cd), ct->size));
      goto checkgc;
    } else if (ct->size == 8 && ctype_isinteger(ct->info)) {
      setstrV(L, L->top-1, lj_ctype_repr_int64(L, *(uint64_t *)cdataptr(cd),
					       (ct->info & CTF_UNSIGNED)));
      goto checkgc;
    } else if (ctype_isfunc(ct->info)) {
      p = *(void **)p;
    } else if (ctype_isenum(ct->info)) {
      msg = "cdata<%s>: %d";
      p = (void *)(uintptr_t)*(uint32_t **)p;
    } else {
      if (ctype_isptr(ct->info)) {
	p = cdata_getptr(p, ct->size);
	ct = ctype_rawchild(cts, ct);
      }
      if (ctype_isstruct(ct->info) || ctype_isvector(ct->info)) {
	/* Handle ctype __tostring metamethod. */
	cTValue *tv = lj_ctype_meta(cts, ctype_typeid(cts, ct), MM_tostring);
	if (tv)
	  return lj_meta_tailcall(L, tv);
      }
    }
  }
  lj_str_pushf(L, msg, strdata(lj_ctype_repr(L, id, NULL)), p);
checkgc:
  lj_gc_check(L);
  return 1;
}

static int ffi_pairs(lua_State *L, MMS mm)
{
  CTState *cts = ctype_cts(L);
  CTypeID id = ffi_checkcdata(L, 1)->ctypeid;
  CType *ct = ctype_raw(cts, id);
  cTValue *tv;
  if (ctype_isptr(ct->info)) id = ctype_cid(ct->info);
  tv = lj_ctype_meta(cts, id, mm);
  if (!tv)
    lj_err_callerv(L, LJ_ERR_FFI_BADMM, strdata(lj_ctype_repr(L, id, NULL)),
		   strdata(mmname_str(G(L), mm)));
  return lj_meta_tailcall(L, tv);
}

LJLIB_CF(ffi_meta___pairs)
{
  return ffi_pairs(L, MM_pairs);
}

LJLIB_CF(ffi_meta___ipairs)
{
  return ffi_pairs(L, MM_ipairs);
}

LJLIB_PUSH("ffi") LJLIB_SET(__metatable)

#include "lj_libdef.h"

/* -- C library metamethods ----------------------------------------------- */

#define LJLIB_MODULE_ffi_clib

/* Index C library by a name. */
static TValue *ffi_clib_index(lua_State *L)
{
  TValue *o = L->base;
  CLibrary *cl;
  if (!(o < L->top && tvisudata(o) && udataV(o)->udtype == UDTYPE_FFI_CLIB))
    lj_err_argt(L, 1, LUA_TUSERDATA);
  cl = (CLibrary *)uddata(udataV(o));
  if (!(o+1 < L->top && tvisstr(o+1)))
    lj_err_argt(L, 2, LUA_TSTRING);
  return lj_clib_index(L, cl, strV(o+1));
}

LJLIB_CF(ffi_clib___index)	LJLIB_REC(clib_index 1)
{
  TValue *tv = ffi_clib_index(L);
  if (tviscdata(tv)) {
    CTState *cts = ctype_cts(L);
    GCcdata *cd = cdataV(tv);
    CType *s = ctype_get(cts, cd->ctypeid);
    if (ctype_isextern(s->info)) {
      CTypeID sid = ctype_cid(s->info);
      void *sp = *(void **)cdataptr(cd);
      CType *ct = ctype_raw(cts, sid);
      if (lj_cconv_tv_ct(cts, ct, sid, L->top-1, sp))
	lj_gc_check(L);
      return 1;
    }
  }
  copyTV(L, L->top-1, tv);
  return 1;
}

LJLIB_CF(ffi_clib___newindex)	LJLIB_REC(clib_index 0)
{
  TValue *tv = ffi_clib_index(L);
  TValue *o = L->base+2;
  if (o < L->top && tviscdata(tv)) {
    CTState *cts = ctype_cts(L);
    GCcdata *cd = cdataV(tv);
    CType *d = ctype_get(cts, cd->ctypeid);
    if (ctype_isextern(d->info)) {
      CTInfo qual = 0;
      for (;;) {  /* Skip attributes and collect qualifiers. */
	d = ctype_child(cts, d);
	if (!ctype_isattrib(d->info)) break;
	if (ctype_attrib(d->info) == CTA_QUAL) qual |= d->size;
      }
      if (!((d->info|qual) & CTF_CONST)) {
	lj_cconv_ct_tv(cts, d, *(void **)cdataptr(cd), o, 0);
	return 0;
      }
    }
  }
  lj_err_caller(L, LJ_ERR_FFI_WRCONST);
  return 0;  /* unreachable */
}

LJLIB_CF(ffi_clib___gc)
{
  TValue *o = L->base;
  if (o < L->top && tvisudata(o) && udataV(o)->udtype == UDTYPE_FFI_CLIB)
    lj_clib_unload((CLibrary *)uddata(udataV(o)));
  return 0;
}

#include "lj_libdef.h"

/* -- Callback function metamethods --------------------------------------- */

#define LJLIB_MODULE_ffi_callback

static int ffi_callback_set(lua_State *L, GCfunc *fn)
{
  GCcdata *cd = ffi_checkcdata(L, 1);
  CTState *cts = ctype_cts(L);
  CType *ct = ctype_raw(cts, cd->ctypeid);
  if (ctype_isptr(ct->info) && (LJ_32 || ct->size == 8)) {
    MSize slot = lj_ccallback_ptr2slot(cts, *(void **)cdataptr(cd));
    if (slot < cts->cb.sizeid && cts->cb.cbid[slot] != 0) {
      GCtab *t = cts->miscmap;
      TValue *tv = lj_tab_setint(L, t, (int32_t)slot);
      if (fn) {
	setfuncV(L, tv, fn);
	lj_gc_anybarriert(L, t);
      } else {
	setnilV(tv);
	cts->cb.cbid[slot] = 0;
	cts->cb.topid = slot < cts->cb.topid ? slot : cts->cb.topid;
      }
      return 0;
    }
  }
  lj_err_caller(L, LJ_ERR_FFI_BADCBACK);
  return 0;
}

LJLIB_CF(ffi_callback_free)
{
  return ffi_callback_set(L, NULL);
}

LJLIB_CF(ffi_callback_set)
{
  GCfunc *fn = lj_lib_checkfunc(L, 2);
  return ffi_callback_set(L, fn);
}

LJLIB_PUSH(top-1) LJLIB_SET(__index)

#include "lj_libdef.h"

/* -- FFI library functions ----------------------------------------------- */

#define LJLIB_MODULE_ffi

LJLIB_CF(ffi_cdef)
{
  GCstr *s = lj_lib_checkstr(L, 1);
  CPState cp;
  int errcode;
  cp.L = L;
  cp.cts = ctype_cts(L);
  cp.srcname = strdata(s);
  cp.p = strdata(s);
  cp.param = L->base+1;
  cp.mode = CPARSE_MODE_MULTI|CPARSE_MODE_DIRECT;
  errcode = lj_cparse(&cp);
  if (errcode) lj_err_throw(L, errcode);  /* Propagate errors. */
  lj_gc_check(L);
  return 0;
}

LJLIB_CF(ffi_new)	LJLIB_REC(.)
{
  CTState *cts = ctype_cts(L);
  CTypeID id = ffi_checkctype(L, cts, NULL);
  CType *ct = ctype_raw(cts, id);
  CTSize sz;
  CTInfo info = lj_ctype_info(cts, id, &sz);
  TValue *o = L->base+1;
  GCcdata *cd;
  if ((info & CTF_VLA)) {
    o++;
    sz = lj_ctype_vlsize(cts, ct, (CTSize)ffi_checkint(L, 2));
  }
  if (sz == CTSIZE_INVALID)
    lj_err_arg(L, 1, LJ_ERR_FFI_INVSIZE);
  if (!(info & CTF_VLA) && ctype_align(info) <= CT_MEMALIGN)
    cd = lj_cdata_new(cts, id, sz);
  else
    cd = lj_cdata_newv(cts, id, sz, ctype_align(info));
  setcdataV(L, o-1, cd);  /* Anchor the uninitialized cdata. */
  lj_cconv_ct_init(cts, ct, sz, cdataptr(cd),
		   o, (MSize)(L->top - o));  /* Initialize cdata. */
  if (ctype_isstruct(ct->info)) {
    /* Handle ctype __gc metamethod. Use the fast lookup here. */
    cTValue *tv = lj_tab_getinth(cts->miscmap, -(int32_t)id);
    if (tv && tvistab(tv) && (tv = lj_meta_fast(L, tabV(tv), MM_gc))) {
      GCtab *t = cts->finalizer;
      if (gcref(t->metatable)) {
	/* Add to finalizer table, if still enabled. */
	copyTV(L, lj_tab_set(L, t, o-1), tv);
	lj_gc_anybarriert(L, t);
	cd->marked |= LJ_GC_CDATA_FIN;
      }
    }
  }
  L->top = o;  /* Only return the cdata itself. */
  lj_gc_check(L);
  return 1;
}

LJLIB_CF(ffi_cast)	LJLIB_REC(ffi_new)
{
  CTState *cts = ctype_cts(L);
  CTypeID id = ffi_checkctype(L, cts, NULL);
  CType *d = ctype_raw(cts, id);
  TValue *o = lj_lib_checkany(L, 2);
  L->top = o+1;  /* Make sure this is the last item on the stack. */
  if (!(ctype_isnum(d->info) || ctype_isptr(d->info) || ctype_isenum(d->info)))
    lj_err_arg(L, 1, LJ_ERR_FFI_INVTYPE);
  if (!(tviscdata(o) && cdataV(o)->ctypeid == id)) {
    GCcdata *cd = lj_cdata_new(cts, id, d->size);
    lj_cconv_ct_tv(cts, d, cdataptr(cd), o, CCF_CAST);
    setcdataV(L, o, cd);
    lj_gc_check(L);
  }
  return 1;
}

LJLIB_CF(ffi_typeof)	LJLIB_REC(.)
{
  CTState *cts = ctype_cts(L);
  CTypeID id = ffi_checkctype(L, cts, L->base+1);
  GCcdata *cd = lj_cdata_new(cts, CTID_CTYPEID, 4);
  *(CTypeID *)cdataptr(cd) = id;
  setcdataV(L, L->top-1, cd);
  lj_gc_check(L);
  return 1;
}

LJLIB_CF(ffi_istype)	LJLIB_REC(.)
{
  CTState *cts = ctype_cts(L);
  CTypeID id1 = ffi_checkctype(L, cts, NULL);
  TValue *o = lj_lib_checkany(L, 2);
  int b = 0;
  if (tviscdata(o)) {
    GCcdata *cd = cdataV(o);
    CTypeID id2 = cd->ctypeid == CTID_CTYPEID ? *(CTypeID *)cdataptr(cd) :
						cd->ctypeid;
    CType *ct1 = lj_ctype_rawref(cts, id1);
    CType *ct2 = lj_ctype_rawref(cts, id2);
    if (ct1 == ct2) {
      b = 1;
    } else if (ctype_type(ct1->info) == ctype_type(ct2->info) &&
	       ct1->size == ct2->size) {
      if (ctype_ispointer(ct1->info))
	b = lj_cconv_compatptr(cts, ct1, ct2, CCF_IGNQUAL);
      else if (ctype_isnum(ct1->info) || ctype_isvoid(ct1->info))
	b = (((ct1->info ^ ct2->info) & ~(CTF_QUAL|CTF_LONG)) == 0);
    } else if (ctype_isstruct(ct1->info) && ctype_isptr(ct2->info) &&
	       ct1 == ctype_rawchild(cts, ct2)) {
      b = 1;
    }
  }
  setboolV(L->top-1, b);
  setboolV(&G(L)->tmptv2, b);  /* Remember for trace recorder. */
  return 1;
}

LJLIB_CF(ffi_sizeof)	LJLIB_REC(ffi_xof FF_ffi_sizeof)
{
  CTState *cts = ctype_cts(L);
  CTypeID id = ffi_checkctype(L, cts, NULL);
  CTSize sz;
  if (LJ_UNLIKELY(tviscdata(L->base) && cdataisv(cdataV(L->base)))) {
    sz = cdatavlen(cdataV(L->base));
  } else {
    CType *ct = lj_ctype_rawref(cts, id);
    if (ctype_isvltype(ct->info))
      sz = lj_ctype_vlsize(cts, ct, (CTSize)ffi_checkint(L, 2));
    else
      sz = ctype_hassize(ct->info) ? ct->size : CTSIZE_INVALID;
    if (LJ_UNLIKELY(sz == CTSIZE_INVALID)) {
      setnilV(L->top-1);
      return 1;
    }
  }
  setintV(L->top-1, (int32_t)sz);
  return 1;
}

LJLIB_CF(ffi_alignof)	LJLIB_REC(ffi_xof FF_ffi_alignof)
{
  CTState *cts = ctype_cts(L);
  CTypeID id = ffi_checkctype(L, cts, NULL);
  CTSize sz = 0;
  CTInfo info = lj_ctype_info(cts, id, &sz);
  setintV(L->top-1, 1 << ctype_align(info));
  return 1;
}

LJLIB_CF(ffi_offsetof)	LJLIB_REC(ffi_xof FF_ffi_offsetof)
{
  CTState *cts = ctype_cts(L);
  CTypeID id = ffi_checkctype(L, cts, NULL);
  GCstr *name = lj_lib_checkstr(L, 2);
  CType *ct = lj_ctype_rawref(cts, id);
  CTSize ofs;
  if (ctype_isstruct(ct->info) && ct->size != CTSIZE_INVALID) {
    CType *fct = lj_ctype_getfield(cts, ct, name, &ofs);
    if (fct) {
      setintV(L->top-1, ofs);
      if (ctype_isfield(fct->info)) {
	return 1;
      } else if (ctype_isbitfield(fct->info)) {
	setintV(L->top++, ctype_bitpos(fct->info));
	setintV(L->top++, ctype_bitbsz(fct->info));
	return 3;
      }
    }
  }
  return 0;
}

LJLIB_CF(ffi_errno)	LJLIB_REC(.)
{
  int err = errno;
  if (L->top > L->base)
    errno = ffi_checkint(L, 1);
  setintV(L->top++, err);
  return 1;
}

LJLIB_CF(ffi_string)	LJLIB_REC(.)
{
  CTState *cts = ctype_cts(L);
  TValue *o = lj_lib_checkany(L, 1);
  const char *p;
  size_t len;
  if (o+1 < L->top) {
    len = (size_t)ffi_checkint(L, 2);
    lj_cconv_ct_tv(cts, ctype_get(cts, CTID_P_CVOID), (uint8_t *)&p, o,
		   CCF_ARG(1));
  } else {
    lj_cconv_ct_tv(cts, ctype_get(cts, CTID_P_CCHAR), (uint8_t *)&p, o,
		   CCF_ARG(1));
    len = strlen(p);
  }
  L->top = o+1;  /* Make sure this is the last item on the stack. */
  setstrV(L, o, lj_str_new(L, p, len));
  lj_gc_check(L);
  return 1;
}

LJLIB_CF(ffi_copy)	LJLIB_REC(.)
{
  void *dp = ffi_checkptr(L, 1, CTID_P_VOID);
  void *sp = ffi_checkptr(L, 2, CTID_P_CVOID);
  TValue *o = L->base+1;
  CTSize len;
  if (tvisstr(o) && o+1 >= L->top)
    len = strV(o)->len+1;  /* Copy Lua string including trailing '\0'. */
  else
    len = (CTSize)ffi_checkint(L, 3);
  memcpy(dp, sp, len);
  return 0;
}

LJLIB_CF(ffi_fill)	LJLIB_REC(.)
{
  void *dp = ffi_checkptr(L, 1, CTID_P_VOID);
  CTSize len = (CTSize)ffi_checkint(L, 2);
  int32_t fill = 0;
  if (L->base+2 < L->top && !tvisnil(L->base+2)) fill = ffi_checkint(L, 3);
  memset(dp, fill, len);
  return 0;
}

#define H_(le, be)	LJ_ENDIAN_SELECT(0x##le, 0x##be)

/* Test ABI string. */
LJLIB_CF(ffi_abi)	LJLIB_REC(.)
{
  GCstr *s = lj_lib_checkstr(L, 1);
  int b = 0;
  switch (s->hash) {
#if LJ_64
  case H_(849858eb,ad35fd06): b = 1; break;  /* 64bit */
#else
  case H_(662d3c79,d0e22477): b = 1; break;  /* 32bit */
#endif
#if LJ_ARCH_HASFPU
  case H_(e33ee463,e33ee463): b = 1; break;  /* fpu */
#endif
#if LJ_ABI_SOFTFP
  case H_(61211a23,c2e8c81c): b = 1; break;  /* softfp */
#else
  case H_(539417a8,8ce0812f): b = 1; break;  /* hardfp */
#endif
#if LJ_ABI_EABI
  case H_(2182df8f,f2ed1152): b = 1; break;  /* eabi */
#endif
#if LJ_ABI_WIN
  case H_(4ab624a8,4ab624a8): b = 1; break;  /* win */
#endif
  case H_(3af93066,1f001464): b = 1; break;  /* le/be */
  default:
    break;
  }
  setboolV(L->top-1, b);
  setboolV(&G(L)->tmptv2, b);  /* Remember for trace recorder. */
  return 1;
}

#undef H_

LJLIB_PUSH(top-8) LJLIB_SET(!)  /* Store reference to miscmap table. */

LJLIB_CF(ffi_metatype)
{
  CTState *cts = ctype_cts(L);
  CTypeID id = ffi_checkctype(L, cts, NULL);
  GCtab *mt = lj_lib_checktab(L, 2);
  GCtab *t = cts->miscmap;
  CType *ct = ctype_get(cts, id);  /* Only allow raw types. */
  TValue *tv;
  GCcdata *cd;
  if (!(ctype_isstruct(ct->info) || ctype_iscomplex(ct->info) ||
	ctype_isvector(ct->info)))
    lj_err_arg(L, 1, LJ_ERR_FFI_INVTYPE);
  tv = lj_tab_setinth(L, t, -(int32_t)id);
  if (!tvisnil(tv))
    lj_err_caller(L, LJ_ERR_PROTMT);
  settabV(L, tv, mt);
  lj_gc_anybarriert(L, t);
  cd = lj_cdata_new(cts, CTID_CTYPEID, 4);
  *(CTypeID *)cdataptr(cd) = id;
  setcdataV(L, L->top-1, cd);
  lj_gc_check(L);
  return 1;
}

LJLIB_PUSH(top-7) LJLIB_SET(!)  /* Store reference to finalizer table. */

LJLIB_CF(ffi_gc)	LJLIB_REC(.)
{
  GCcdata *cd = ffi_checkcdata(L, 1);
  TValue *fin = lj_lib_checkany(L, 2);
  CTState *cts = ctype_cts(L);
  GCtab *t = cts->finalizer;
  CType *ct = ctype_raw(cts, cd->ctypeid);
  if (!(ctype_isptr(ct->info) || ctype_isstruct(ct->info) ||
	ctype_isrefarray(ct->info)))
    lj_err_arg(L, 1, LJ_ERR_FFI_INVTYPE);
  if (gcref(t->metatable)) {  /* Update finalizer table, if still enabled. */
    copyTV(L, lj_tab_set(L, t, L->base), fin);
    lj_gc_anybarriert(L, t);
    if (!tvisnil(fin))
      cd->marked |= LJ_GC_CDATA_FIN;
    else
      cd->marked &= ~LJ_GC_CDATA_FIN;
  }
  L->top = L->base+1;  /* Pass through the cdata object. */
  return 1;
}

LJLIB_PUSH(top-5) LJLIB_SET(!)  /* Store clib metatable in func environment. */

LJLIB_CF(ffi_load)
{
  GCstr *name = lj_lib_checkstr(L, 1);
  int global = (L->base+1 < L->top && tvistruecond(L->base+1));
  lj_clib_load(L, tabref(curr_func(L)->c.env), name, global);
  return 1;
}

LJLIB_PUSH(top-4) LJLIB_SET(C)
LJLIB_PUSH(top-3) LJLIB_SET(os)
LJLIB_PUSH(top-2) LJLIB_SET(arch)

#include "lj_libdef.h"

/* ------------------------------------------------------------------------ */

/* Create special weak-keyed finalizer table. */
static GCtab *ffi_finalizer(lua_State *L)
{
  /* NOBARRIER: The table is new (marked white). */
  GCtab *t = lj_tab_new(L, 0, 1);
  settabV(L, L->top++, t);
  setgcref(t->metatable, obj2gco(t));
  setstrV(L, lj_tab_setstr(L, t, lj_str_newlit(L, "__mode")),
	  lj_str_newlit(L, "K"));
  t->nomm = (uint8_t)(~(1u<<MM_mode));
  return t;
}

/* Register FFI module as loaded. */
static void ffi_register_module(lua_State *L)
{
  cTValue *tmp = lj_tab_getstr(tabV(registry(L)), lj_str_newlit(L, "_LOADED"));
  if (tmp && tvistab(tmp)) {
    GCtab *t = tabV(tmp);
    copyTV(L, lj_tab_setstr(L, t, lj_str_newlit(L, LUA_FFILIBNAME)), L->top-1);
    lj_gc_anybarriert(L, t);
  }
}

LUALIB_API int luaopen_ffi(lua_State *L)
{
  CTState *cts = lj_ctype_init(L);
  settabV(L, L->top++, (cts->miscmap = lj_tab_new(L, 0, 1)));
  cts->finalizer = ffi_finalizer(L);
  LJ_LIB_REG(L, NULL, ffi_meta);
  /* NOBARRIER: basemt is a GC root. */
  setgcref(basemt_it(G(L), LJ_TCDATA), obj2gco(tabV(L->top-1)));
  LJ_LIB_REG(L, NULL, ffi_clib);
  LJ_LIB_REG(L, NULL, ffi_callback);
  /* NOBARRIER: the key is new and lj_tab_newkey() handles the barrier. */
  settabV(L, lj_tab_setstr(L, cts->miscmap, &cts->g->strempty), tabV(L->top-1));
  L->top--;
  lj_clib_default(L, tabV(L->top-1));  /* Create ffi.C default namespace. */
  lua_pushliteral(L, LJ_OS_NAME);
  lua_pushliteral(L, LJ_ARCH_NAME);
  LJ_LIB_REG(L, NULL, ffi);  /* Note: no global "ffi" created! */
  ffi_register_module(L);
  return 1;
}

#endif
