/*
** C type conversions.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#include "lj_obj.h"

#if LJ_HASFFI

#include "lj_err.h"
#include "lj_tab.h"
#include "lj_ctype.h"
#include "lj_cdata.h"
#include "lj_cconv.h"
#include "lj_ccallback.h"

/* -- Conversion errors --------------------------------------------------- */

/* Bad conversion. */
LJ_NORET static void cconv_err_conv(CTState *cts, CType *d, CType *s,
				    CTInfo flags)
{
  const char *dst = strdata(lj_ctype_repr(cts->L, ctype_typeid(cts, d), NULL));
  const char *src;
  if ((flags & CCF_FROMTV))
    src = lj_obj_typename[1+(ctype_isnum(s->info) ? LUA_TNUMBER :
			     ctype_isarray(s->info) ? LUA_TSTRING : LUA_TNIL)];
  else
    src = strdata(lj_ctype_repr(cts->L, ctype_typeid(cts, s), NULL));
  if (CCF_GETARG(flags))
    lj_err_argv(cts->L, CCF_GETARG(flags), LJ_ERR_FFI_BADCONV, src, dst);
  else
    lj_err_callerv(cts->L, LJ_ERR_FFI_BADCONV, src, dst);
}

/* Bad conversion from TValue. */
LJ_NORET static void cconv_err_convtv(CTState *cts, CType *d, TValue *o,
				      CTInfo flags)
{
  const char *dst = strdata(lj_ctype_repr(cts->L, ctype_typeid(cts, d), NULL));
  const char *src = lj_typename(o);
  if (CCF_GETARG(flags))
    lj_err_argv(cts->L, CCF_GETARG(flags), LJ_ERR_FFI_BADCONV, src, dst);
  else
    lj_err_callerv(cts->L, LJ_ERR_FFI_BADCONV, src, dst);
}

/* Initializer overflow. */
LJ_NORET static void cconv_err_initov(CTState *cts, CType *d)
{
  const char *dst = strdata(lj_ctype_repr(cts->L, ctype_typeid(cts, d), NULL));
  lj_err_callerv(cts->L, LJ_ERR_FFI_INITOV, dst);
}

/* -- C type compatibility checks ----------------------------------------- */

/* Get raw type and qualifiers for a child type. Resolves enums, too. */
static CType *cconv_childqual(CTState *cts, CType *ct, CTInfo *qual)
{
  ct = ctype_child(cts, ct);
  for (;;) {
    if (ctype_isattrib(ct->info)) {
      if (ctype_attrib(ct->info) == CTA_QUAL) *qual |= ct->size;
    } else if (!ctype_isenum(ct->info)) {
      break;
    }
    ct = ctype_child(cts, ct);
  }
  *qual |= (ct->info & CTF_QUAL);
  return ct;
}

/* Check for compatible types when converting to a pointer.
** Note: these checks are more relaxed than what C99 mandates.
*/
int lj_cconv_compatptr(CTState *cts, CType *d, CType *s, CTInfo flags)
{
  if (!((flags & CCF_CAST) || d == s)) {
    CTInfo dqual = 0, squal = 0;
    d = cconv_childqual(cts, d, &dqual);
    if (!ctype_isstruct(s->info))
      s = cconv_childqual(cts, s, &squal);
    if ((flags & CCF_SAME)) {
      if (dqual != squal)
	return 0;  /* Different qualifiers. */
    } else if (!(flags & CCF_IGNQUAL)) {
      if ((dqual & squal) != squal)
	return 0;  /* Discarded qualifiers. */
      if (ctype_isvoid(d->info) || ctype_isvoid(s->info))
	return 1;  /* Converting to/from void * is always ok. */
    }
    if (ctype_type(d->info) != ctype_type(s->info) ||
	d->size != s->size)
      return 0;  /* Different type or different size. */
    if (ctype_isnum(d->info)) {
      if (((d->info ^ s->info) & (CTF_BOOL|CTF_FP)))
	return 0;  /* Different numeric types. */
    } else if (ctype_ispointer(d->info)) {
      /* Check child types for compatibility. */
      return lj_cconv_compatptr(cts, d, s, flags|CCF_SAME);
    } else if (ctype_isstruct(d->info)) {
      if (d != s)
	return 0;  /* Must be exact same type for struct/union. */
    } else if (ctype_isfunc(d->info)) {
      /* NYI: structural equality of functions. */
    }
  }
  return 1;  /* Types are compatible. */
}

/* -- C type to C type conversion ----------------------------------------- */

/* Convert C type to C type. Caveat: expects to get the raw CType!
**
** Note: This is only used by the interpreter and not optimized at all.
** The JIT compiler will do a much better job specializing for each case.
*/
void lj_cconv_ct_ct(CTState *cts, CType *d, CType *s,
		    uint8_t *dp, uint8_t *sp, CTInfo flags)
{
  CTSize dsize = d->size, ssize = s->size;
  CTInfo dinfo = d->info, sinfo = s->info;
  void *tmpptr;

  lua_assert(!ctype_isenum(dinfo) && !ctype_isenum(sinfo));
  lua_assert(!ctype_isattrib(dinfo) && !ctype_isattrib(sinfo));

  if (ctype_type(dinfo) > CT_MAYCONVERT || ctype_type(sinfo) > CT_MAYCONVERT)
    goto err_conv;

  /* Some basic sanity checks. */
  lua_assert(!ctype_isnum(dinfo) || dsize > 0);
  lua_assert(!ctype_isnum(sinfo) || ssize > 0);
  lua_assert(!ctype_isbool(dinfo) || dsize == 1 || dsize == 4);
  lua_assert(!ctype_isbool(sinfo) || ssize == 1 || ssize == 4);
  lua_assert(!ctype_isinteger(dinfo) || (1u<<lj_fls(dsize)) == dsize);
  lua_assert(!ctype_isinteger(sinfo) || (1u<<lj_fls(ssize)) == ssize);

  switch (cconv_idx2(dinfo, sinfo)) {
  /* Destination is a bool. */
  case CCX(B, B):
    /* Source operand is already normalized. */
    if (dsize == 1) *dp = *sp; else *(int *)dp = *sp;
    break;
  case CCX(B, I): {
    MSize i;
    uint8_t b = 0;
    for (i = 0; i < ssize; i++) b |= sp[i];
    b = (b != 0);
    if (dsize == 1) *dp = b; else *(int *)dp = b;
    break;
    }
  case CCX(B, F): {
    uint8_t b;
    if (ssize == sizeof(double)) b = (*(double *)sp != 0);
    else if (ssize == sizeof(float)) b = (*(float *)sp != 0);
    else goto err_conv;  /* NYI: long double. */
    if (dsize == 1) *dp = b; else *(int *)dp = b;
    break;
    }

  /* Destination is an integer. */
  case CCX(I, B):
  case CCX(I, I):
  conv_I_I:
    if (dsize > ssize) {  /* Zero-extend or sign-extend LSB. */
#if LJ_LE
      uint8_t fill = (!(sinfo & CTF_UNSIGNED) && (sp[ssize-1]&0x80)) ? 0xff : 0;
      memcpy(dp, sp, ssize);
      memset(dp + ssize, fill, dsize-ssize);
#else
      uint8_t fill = (!(sinfo & CTF_UNSIGNED) && (sp[0]&0x80)) ? 0xff : 0;
      memset(dp, fill, dsize-ssize);
      memcpy(dp + (dsize-ssize), sp, ssize);
#endif
    } else {  /* Copy LSB. */
#if LJ_LE
      memcpy(dp, sp, dsize);
#else
      memcpy(dp, sp + (ssize-dsize), dsize);
#endif
    }
    break;
  case CCX(I, F): {
    double n;  /* Always convert via double. */
  conv_I_F:
    /* Convert source to double. */
    if (ssize == sizeof(double)) n = *(double *)sp;
    else if (ssize == sizeof(float)) n = (double)*(float *)sp;
    else goto err_conv;  /* NYI: long double. */
    /* Then convert double to integer. */
    /* The conversion must exactly match the semantics of JIT-compiled code! */
    if (dsize < 4 || (dsize == 4 && !(dinfo & CTF_UNSIGNED))) {
      int32_t i = (int32_t)n;
      if (dsize == 4) *(int32_t *)dp = i;
      else if (dsize == 2) *(int16_t *)dp = (int16_t)i;
      else *(int8_t *)dp = (int8_t)i;
    } else if (dsize == 4) {
      *(uint32_t *)dp = (uint32_t)n;
    } else if (dsize == 8) {
      if (!(dinfo & CTF_UNSIGNED))
	*(int64_t *)dp = (int64_t)n;
      else
	*(uint64_t *)dp = lj_num2u64(n);
    } else {
      goto err_conv;  /* NYI: conversion to >64 bit integers. */
    }
    break;
    }
  case CCX(I, C):
    s = ctype_child(cts, s);
    sinfo = s->info;
    ssize = s->size;
    goto conv_I_F;  /* Just convert re. */
  case CCX(I, P):
    if (!(flags & CCF_CAST)) goto err_conv;
    sinfo = CTINFO(CT_NUM, CTF_UNSIGNED);
    goto conv_I_I;
  case CCX(I, A):
    if (!(flags & CCF_CAST)) goto err_conv;
    sinfo = CTINFO(CT_NUM, CTF_UNSIGNED);
    ssize = CTSIZE_PTR;
    tmpptr = sp;
    sp = (uint8_t *)&tmpptr;
    goto conv_I_I;

  /* Destination is a floating-point number. */
  case CCX(F, B):
  case CCX(F, I): {
    double n;  /* Always convert via double. */
  conv_F_I:
    /* First convert source to double. */
    /* The conversion must exactly match the semantics of JIT-compiled code! */
    if (ssize < 4 || (ssize == 4 && !(sinfo & CTF_UNSIGNED))) {
      int32_t i;
      if (ssize == 4) {
	i = *(int32_t *)sp;
      } else if (!(sinfo & CTF_UNSIGNED)) {
	if (ssize == 2) i = *(int16_t *)sp;
	else i = *(int8_t *)sp;
      } else {
	if (ssize == 2) i = *(uint16_t *)sp;
	else i = *(uint8_t *)sp;
      }
      n = (double)i;
    } else if (ssize == 4) {
      n = (double)*(uint32_t *)sp;
    } else if (ssize == 8) {
      if (!(sinfo & CTF_UNSIGNED)) n = (double)*(int64_t *)sp;
      else n = (double)*(uint64_t *)sp;
    } else {
      goto err_conv;  /* NYI: conversion from >64 bit integers. */
    }
    /* Convert double to destination. */
    if (dsize == sizeof(double)) *(double *)dp = n;
    else if (dsize == sizeof(float)) *(float *)dp = (float)n;
    else goto err_conv;  /* NYI: long double. */
    break;
    }
  case CCX(F, F): {
    double n;  /* Always convert via double. */
  conv_F_F:
    if (ssize == dsize) goto copyval;
    /* Convert source to double. */
    if (ssize == sizeof(double)) n = *(double *)sp;
    else if (ssize == sizeof(float)) n = (double)*(float *)sp;
    else goto err_conv;  /* NYI: long double. */
    /* Convert double to destination. */
    if (dsize == sizeof(double)) *(double *)dp = n;
    else if (dsize == sizeof(float)) *(float *)dp = (float)n;
    else goto err_conv;  /* NYI: long double. */
    break;
    }
  case CCX(F, C):
    s = ctype_child(cts, s);
    sinfo = s->info;
    ssize = s->size;
    goto conv_F_F;  /* Ignore im, and convert from re. */

  /* Destination is a complex number. */
  case CCX(C, I):
    d = ctype_child(cts, d);
    dinfo = d->info;
    dsize = d->size;
    memset(dp + dsize, 0, dsize);  /* Clear im. */
    goto conv_F_I;  /* Convert to re. */
  case CCX(C, F):
    d = ctype_child(cts, d);
    dinfo = d->info;
    dsize = d->size;
    memset(dp + dsize, 0, dsize);  /* Clear im. */
    goto conv_F_F;  /* Convert to re. */

  case CCX(C, C):
    if (dsize != ssize) {  /* Different types: convert re/im separately. */
      CType *dc = ctype_child(cts, d);
      CType *sc = ctype_child(cts, s);
      lj_cconv_ct_ct(cts, dc, sc, dp, sp, flags);
      lj_cconv_ct_ct(cts, dc, sc, dp + dc->size, sp + sc->size, flags);
      return;
    }
    goto copyval;  /* Otherwise this is easy. */

  /* Destination is a vector. */
  case CCX(V, I):
  case CCX(V, F):
  case CCX(V, C): {
    CType *dc = ctype_child(cts, d);
    CTSize esize;
    /* First convert the scalar to the first element. */
    lj_cconv_ct_ct(cts, dc, s, dp, sp, flags);
    /* Then replicate it to the other elements (splat). */
    for (sp = dp, esize = dc->size; dsize > esize; dsize -= esize) {
      dp += esize;
      memcpy(dp, sp, esize);
    }
    break;
    }

  case CCX(V, V):
    /* Copy same-sized vectors, even for different lengths/element-types. */
    if (dsize != ssize) goto err_conv;
    goto copyval;

  /* Destination is a pointer. */
  case CCX(P, I):
    if (!(flags & CCF_CAST)) goto err_conv;
    dinfo = CTINFO(CT_NUM, CTF_UNSIGNED);
    goto conv_I_I;

  case CCX(P, F):
    if (!(flags & CCF_CAST) || !(flags & CCF_FROMTV)) goto err_conv;
    /* The signed conversion is cheaper. x64 really has 47 bit pointers. */
    dinfo = CTINFO(CT_NUM, (LJ_64 && dsize == 8) ? 0 : CTF_UNSIGNED);
    goto conv_I_F;

  case CCX(P, P):
    if (!lj_cconv_compatptr(cts, d, s, flags)) goto err_conv;
    cdata_setptr(dp, dsize, cdata_getptr(sp, ssize));
    break;

  case CCX(P, A):
  case CCX(P, S):
    if (!lj_cconv_compatptr(cts, d, s, flags)) goto err_conv;
    cdata_setptr(dp, dsize, sp);
    break;

  /* Destination is an array. */
  case CCX(A, A):
    if ((flags & CCF_CAST) || (d->info & CTF_VLA) || dsize != ssize ||
	d->size == CTSIZE_INVALID || !lj_cconv_compatptr(cts, d, s, flags))
      goto err_conv;
    goto copyval;

  /* Destination is a struct/union. */
  case CCX(S, S):
    if ((flags & CCF_CAST) || (d->info & CTF_VLA) || d != s)
      goto err_conv;  /* Must be exact same type. */
copyval:  /* Copy value. */
    lua_assert(dsize == ssize);
    memcpy(dp, sp, dsize);
    break;

  default:
  err_conv:
    cconv_err_conv(cts, d, s, flags);
  }
}

/* -- C type to TValue conversion ----------------------------------------- */

/* Convert C type to TValue. Caveat: expects to get the raw CType! */
int lj_cconv_tv_ct(CTState *cts, CType *s, CTypeID sid,
		   TValue *o, uint8_t *sp)
{
  CTInfo sinfo = s->info;
  if (ctype_isnum(sinfo)) {
    if (!ctype_isbool(sinfo)) {
      if (ctype_isinteger(sinfo) && s->size > 4) goto copyval;
      if (LJ_DUALNUM && ctype_isinteger(sinfo)) {
	int32_t i;
	lj_cconv_ct_ct(cts, ctype_get(cts, CTID_INT32), s,
		       (uint8_t *)&i, sp, 0);
	if ((sinfo & CTF_UNSIGNED) && i < 0)
	  setnumV(o, (lua_Number)(uint32_t)i);
	else
	  setintV(o, i);
      } else {
	lj_cconv_ct_ct(cts, ctype_get(cts, CTID_DOUBLE), s,
		       (uint8_t *)&o->n, sp, 0);
	/* Numbers are NOT canonicalized here! Beware of uninitialized data. */
	lua_assert(tvisnum(o));
      }
    } else {
      uint32_t b = s->size == 1 ? (*sp != 0) : (*(int *)sp != 0);
      setboolV(o, b);
      setboolV(&cts->g->tmptv2, b);  /* Remember for trace recorder. */
    }
    return 0;
  } else if (ctype_isrefarray(sinfo) || ctype_isstruct(sinfo)) {
    /* Create reference. */
    setcdataV(cts->L, o, lj_cdata_newref(cts, sp, sid));
    return 1;  /* Need GC step. */
  } else {
    GCcdata *cd;
    CTSize sz;
  copyval:  /* Copy value. */
    sz = s->size;
    lua_assert(sz != CTSIZE_INVALID);
    /* Attributes are stripped, qualifiers are kept (but mostly ignored). */
    cd = lj_cdata_new(cts, ctype_typeid(cts, s), sz);
    setcdataV(cts->L, o, cd);
    memcpy(cdataptr(cd), sp, sz);
    return 1;  /* Need GC step. */
  }
}

/* Convert bitfield to TValue. */
int lj_cconv_tv_bf(CTState *cts, CType *s, TValue *o, uint8_t *sp)
{
  CTInfo info = s->info;
  CTSize pos, bsz;
  uint32_t val;
  lua_assert(ctype_isbitfield(info));
  /* NYI: packed bitfields may cause misaligned reads. */
  switch (ctype_bitcsz(info)) {
  case 4: val = *(uint32_t *)sp; break;
  case 2: val = *(uint16_t *)sp; break;
  case 1: val = *(uint8_t *)sp; break;
  default: lua_assert(0); val = 0; break;
  }
  /* Check if a packed bitfield crosses a container boundary. */
  pos = ctype_bitpos(info);
  bsz = ctype_bitbsz(info);
  lua_assert(pos < 8*ctype_bitcsz(info));
  lua_assert(bsz > 0 && bsz <= 8*ctype_bitcsz(info));
  if (pos + bsz > 8*ctype_bitcsz(info))
    lj_err_caller(cts->L, LJ_ERR_FFI_NYIPACKBIT);
  if (!(info & CTF_BOOL)) {
    CTSize shift = 32 - bsz;
    if (!(info & CTF_UNSIGNED)) {
      setintV(o, (int32_t)(val << (shift-pos)) >> shift);
    } else {
      val = (val << (shift-pos)) >> shift;
      if (!LJ_DUALNUM || (int32_t)val < 0)
	setnumV(o, (lua_Number)(uint32_t)val);
      else
	setintV(o, (int32_t)val);
    }
  } else {
    lua_assert(bsz == 1);
    setboolV(o, (val >> pos) & 1);
  }
  return 0;  /* No GC step needed. */
}

/* -- TValue to C type conversion ----------------------------------------- */

/* Convert table to array. */
static void cconv_array_tab(CTState *cts, CType *d,
			    uint8_t *dp, GCtab *t, CTInfo flags)
{
  int32_t i;
  CType *dc = ctype_rawchild(cts, d);  /* Array element type. */
  CTSize size = d->size, esize = dc->size, ofs = 0;
  for (i = 0; ; i++) {
    TValue *tv = (TValue *)lj_tab_getint(t, i);
    if (!tv || tvisnil(tv)) {
      if (i == 0) continue;  /* Try again for 1-based tables. */
      break;  /* Stop at first nil. */
    }
    if (ofs >= size)
      cconv_err_initov(cts, d);
    lj_cconv_ct_tv(cts, dc, dp + ofs, tv, flags);
    ofs += esize;
  }
  if (size != CTSIZE_INVALID) {  /* Only fill up arrays with known size. */
    if (ofs == esize) {  /* Replicate a single element. */
      for (; ofs < size; ofs += esize) memcpy(dp + ofs, dp, esize);
    } else {  /* Otherwise fill the remainder with zero. */
      memset(dp + ofs, 0, size - ofs);
    }
  }
}

/* Convert table to sub-struct/union. */
static void cconv_substruct_tab(CTState *cts, CType *d, uint8_t *dp,
				GCtab *t, int32_t *ip, CTInfo flags)
{
  CTypeID id = d->sib;
  while (id) {
    CType *df = ctype_get(cts, id);
    id = df->sib;
    if (ctype_isfield(df->info) || ctype_isbitfield(df->info)) {
      TValue *tv;
      int32_t i = *ip, iz = i;
      if (!gcref(df->name)) continue;  /* Ignore unnamed fields. */
      if (i >= 0) {
      retry:
	tv = (TValue *)lj_tab_getint(t, i);
	if (!tv || tvisnil(tv)) {
	  if (i == 0) { i = 1; goto retry; }  /* 1-based tables. */
	  if (iz == 0) { *ip = i = -1; goto tryname; }  /* Init named fields. */
	  break;  /* Stop at first nil. */
	}
	*ip = i + 1;
      } else {
      tryname:
	tv = (TValue *)lj_tab_getstr(t, gco2str(gcref(df->name)));
	if (!tv || tvisnil(tv)) continue;
      }
      if (ctype_isfield(df->info))
	lj_cconv_ct_tv(cts, ctype_rawchild(cts, df), dp+df->size, tv, flags);
      else
	lj_cconv_bf_tv(cts, df, dp+df->size, tv);
      if ((d->info & CTF_UNION)) break;
    } else if (ctype_isxattrib(df->info, CTA_SUBTYPE)) {
      cconv_substruct_tab(cts, ctype_rawchild(cts, df),
			  dp+df->size, t, ip, flags);
    }  /* Ignore all other entries in the chain. */
  }
}

/* Convert table to struct/union. */
static void cconv_struct_tab(CTState *cts, CType *d,
			     uint8_t *dp, GCtab *t, CTInfo flags)
{
  int32_t i = 0;
  memset(dp, 0, d->size);  /* Much simpler to clear the struct first. */
  cconv_substruct_tab(cts, d, dp, t, &i, flags);
}

/* Convert TValue to C type. Caveat: expects to get the raw CType! */
void lj_cconv_ct_tv(CTState *cts, CType *d,
		    uint8_t *dp, TValue *o, CTInfo flags)
{
  CTypeID sid = CTID_P_VOID;
  CType *s;
  void *tmpptr;
  uint8_t tmpbool, *sp = (uint8_t *)&tmpptr;
  if (LJ_LIKELY(tvisint(o))) {
    sp = (uint8_t *)&o->i;
    sid = CTID_INT32;
    flags |= CCF_FROMTV;
  } else if (LJ_LIKELY(tvisnum(o))) {
    sp = (uint8_t *)&o->n;
    sid = CTID_DOUBLE;
    flags |= CCF_FROMTV;
  } else if (tviscdata(o)) {
    sp = cdataptr(cdataV(o));
    sid = cdataV(o)->ctypeid;
    s = ctype_get(cts, sid);
    if (ctype_isref(s->info)) {  /* Resolve reference for value. */
      lua_assert(s->size == CTSIZE_PTR);
      sp = *(void **)sp;
      sid = ctype_cid(s->info);
    }
    s = ctype_raw(cts, sid);
    if (ctype_isfunc(s->info)) {
      sid = lj_ctype_intern(cts, CTINFO(CT_PTR, CTALIGN_PTR|sid), CTSIZE_PTR);
    } else {
      if (ctype_isenum(s->info)) s = ctype_child(cts, s);
      goto doconv;
    }
  } else if (tvisstr(o)) {
    GCstr *str = strV(o);
    if (ctype_isenum(d->info)) {  /* Match string against enum constant. */
      CTSize ofs;
      CType *cct = lj_ctype_getfield(cts, d, str, &ofs);
      if (!cct || !ctype_isconstval(cct->info))
	goto err_conv;
      lua_assert(d->size == 4);
      sp = (uint8_t *)&cct->size;
      sid = ctype_cid(cct->info);
    } else if (ctype_isrefarray(d->info)) {  /* Copy string to array. */
      CType *dc = ctype_rawchild(cts, d);
      CTSize sz = str->len+1;
      if (!ctype_isinteger(dc->info) || dc->size != 1)
	goto err_conv;
      if (d->size != 0 && d->size < sz)
	sz = d->size;
      memcpy(dp, strdata(str), sz);
      return;
    } else {  /* Otherwise pass it as a const char[]. */
      sp = (uint8_t *)strdata(str);
      sid = CTID_A_CCHAR;
      flags |= CCF_FROMTV;
    }
  } else if (tvistab(o)) {
    if (ctype_isarray(d->info)) {
      cconv_array_tab(cts, d, dp, tabV(o), flags);
      return;
    } else if (ctype_isstruct(d->info)) {
      cconv_struct_tab(cts, d, dp, tabV(o), flags);
      return;
    } else {
      goto err_conv;
    }
  } else if (tvisbool(o)) {
    tmpbool = boolV(o);
    sp = &tmpbool;
    sid = CTID_BOOL;
  } else if (tvisnil(o)) {
    tmpptr = (void *)0;
    flags |= CCF_FROMTV;
  } else if (tvisudata(o)) {
    GCudata *ud = udataV(o);
    tmpptr = uddata(ud);
    if (ud->udtype == UDTYPE_IO_FILE)
      tmpptr = *(void **)tmpptr;
  } else if (tvislightud(o)) {
    tmpptr = lightudV(o);
  } else if (tvisfunc(o)) {
    void *p = lj_ccallback_new(cts, d, funcV(o));
    if (p) {
      *(void **)dp = p;
      return;
    }
    goto err_conv;
  } else {
  err_conv:
    cconv_err_convtv(cts, d, o, flags);
  }
  s = ctype_get(cts, sid);
doconv:
  if (ctype_isenum(d->info)) d = ctype_child(cts, d);
  lj_cconv_ct_ct(cts, d, s, dp, sp, flags);
}

/* Convert TValue to bitfield. */
void lj_cconv_bf_tv(CTState *cts, CType *d, uint8_t *dp, TValue *o)
{
  CTInfo info = d->info;
  CTSize pos, bsz;
  uint32_t val, mask;
  lua_assert(ctype_isbitfield(info));
  if ((info & CTF_BOOL)) {
    uint8_t tmpbool;
    lua_assert(ctype_bitbsz(info) == 1);
    lj_cconv_ct_tv(cts, ctype_get(cts, CTID_BOOL), &tmpbool, o, 0);
    val = tmpbool;
  } else {
    CTypeID did = (info & CTF_UNSIGNED) ? CTID_UINT32 : CTID_INT32;
    lj_cconv_ct_tv(cts, ctype_get(cts, did), (uint8_t *)&val, o, 0);
  }
  pos = ctype_bitpos(info);
  bsz = ctype_bitbsz(info);
  lua_assert(pos < 8*ctype_bitcsz(info));
  lua_assert(bsz > 0 && bsz <= 8*ctype_bitcsz(info));
  /* Check if a packed bitfield crosses a container boundary. */
  if (pos + bsz > 8*ctype_bitcsz(info))
    lj_err_caller(cts->L, LJ_ERR_FFI_NYIPACKBIT);
  mask = ((1u << bsz) - 1u) << pos;
  val = (val << pos) & mask;
  /* NYI: packed bitfields may cause misaligned reads/writes. */
  switch (ctype_bitcsz(info)) {
  case 4: *(uint32_t *)dp = (*(uint32_t *)dp & ~mask) | (uint32_t)val; break;
  case 2: *(uint16_t *)dp = (*(uint16_t *)dp & ~mask) | (uint16_t)val; break;
  case 1: *(uint8_t *)dp = (*(uint8_t *)dp & ~mask) | (uint8_t)val; break;
  default: lua_assert(0); break;
  }
}

/* -- Initialize C type with TValues -------------------------------------- */

/* Initialize an array with TValues. */
static void cconv_array_init(CTState *cts, CType *d, CTSize sz, uint8_t *dp,
			     TValue *o, MSize len)
{
  CType *dc = ctype_rawchild(cts, d);  /* Array element type. */
  CTSize ofs, esize = dc->size;
  MSize i;
  if (len*esize > sz)
    cconv_err_initov(cts, d);
  for (i = 0, ofs = 0; i < len; i++, ofs += esize)
    lj_cconv_ct_tv(cts, dc, dp + ofs, o + i, 0);
  if (ofs == esize) {  /* Replicate a single element. */
    for (; ofs < sz; ofs += esize) memcpy(dp + ofs, dp, esize);
  } else {  /* Otherwise fill the remainder with zero. */
    memset(dp + ofs, 0, sz - ofs);
  }
}

/* Initialize a sub-struct/union with TValues. */
static void cconv_substruct_init(CTState *cts, CType *d, uint8_t *dp,
				 TValue *o, MSize len, MSize *ip)
{
  CTypeID id = d->sib;
  while (id) {
    CType *df = ctype_get(cts, id);
    id = df->sib;
    if (ctype_isfield(df->info) || ctype_isbitfield(df->info)) {
      MSize i = *ip;
      if (!gcref(df->name)) continue;  /* Ignore unnamed fields. */
      if (i >= len) break;
      *ip = i + 1;
      if (ctype_isfield(df->info))
	lj_cconv_ct_tv(cts, ctype_rawchild(cts, df), dp+df->size, o + i, 0);
      else
	lj_cconv_bf_tv(cts, df, dp+df->size, o + i);
      if ((d->info & CTF_UNION)) break;
    } else if (ctype_isxattrib(df->info, CTA_SUBTYPE)) {
      cconv_substruct_init(cts, ctype_rawchild(cts, df),
			   dp+df->size, o, len, ip);
    }  /* Ignore all other entries in the chain. */
  }
}

/* Initialize a struct/union with TValues. */
static void cconv_struct_init(CTState *cts, CType *d, CTSize sz, uint8_t *dp,
			      TValue *o, MSize len)
{
  MSize i = 0;
  memset(dp, 0, sz);  /* Much simpler to clear the struct first. */
  cconv_substruct_init(cts, d, dp, o, len, &i);
  if (i < len)
    cconv_err_initov(cts, d);
}

/* Check whether to use a multi-value initializer.
** This is true if an aggregate is to be initialized with a value.
** Valarrays are treated as values here so ct_tv handles (V|C, I|F).
*/
int lj_cconv_multi_init(CTState *cts, CType *d, TValue *o)
{
  if (!(ctype_isrefarray(d->info) || ctype_isstruct(d->info)))
    return 0;  /* Destination is not an aggregate. */
  if (tvistab(o) || (tvisstr(o) && !ctype_isstruct(d->info)))
    return 0;  /* Initializer is not a value. */
  if (tviscdata(o) && lj_ctype_rawref(cts, cdataV(o)->ctypeid) == d)
    return 0;  /* Source and destination are identical aggregates. */
  return 1;  /* Otherwise the initializer is a value. */
}

/* Initialize C type with TValues. Caveat: expects to get the raw CType! */
void lj_cconv_ct_init(CTState *cts, CType *d, CTSize sz,
		      uint8_t *dp, TValue *o, MSize len)
{
  if (len == 0)
    memset(dp, 0, sz);
  else if (len == 1 && !lj_cconv_multi_init(cts, d, o))
    lj_cconv_ct_tv(cts, d, dp, o, 0);
  else if (ctype_isarray(d->info))  /* Also handles valarray init with len>1. */
    cconv_array_init(cts, d, sz, dp, o, len);
  else if (ctype_isstruct(d->info))
    cconv_struct_init(cts, d, sz, dp, o, len);
  else
    cconv_err_initov(cts, d);
}

#endif
