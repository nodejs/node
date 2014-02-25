/*
** FFI C call handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#include "lj_obj.h"

#if LJ_HASFFI

#include "lj_gc.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_ctype.h"
#include "lj_cconv.h"
#include "lj_cdata.h"
#include "lj_ccall.h"
#include "lj_trace.h"

/* Target-specific handling of register arguments. */
#if LJ_TARGET_X86
/* -- x86 calling conventions --------------------------------------------- */

#if LJ_ABI_WIN

#define CCALL_HANDLE_STRUCTRET \
  /* Return structs bigger than 8 by reference (on stack only). */ \
  cc->retref = (sz > 8); \
  if (cc->retref) cc->stack[nsp++] = (GPRArg)dp;

#define CCALL_HANDLE_COMPLEXRET CCALL_HANDLE_STRUCTRET

#else

#if LJ_TARGET_OSX

#define CCALL_HANDLE_STRUCTRET \
  /* Return structs of size 1, 2, 4 or 8 in registers. */ \
  cc->retref = !(sz == 1 || sz == 2 || sz == 4 || sz == 8); \
  if (cc->retref) { \
    if (ngpr < maxgpr) \
      cc->gpr[ngpr++] = (GPRArg)dp; \
    else \
      cc->stack[nsp++] = (GPRArg)dp; \
  } else {  /* Struct with single FP field ends up in FPR. */ \
    cc->resx87 = ccall_classify_struct(cts, ctr); \
  }

#define CCALL_HANDLE_STRUCTRET2 \
  if (cc->resx87) sp = (uint8_t *)&cc->fpr[0]; \
  memcpy(dp, sp, ctr->size);

#else

#define CCALL_HANDLE_STRUCTRET \
  cc->retref = 1;  /* Return all structs by reference (in reg or on stack). */ \
  if (ngpr < maxgpr) \
    cc->gpr[ngpr++] = (GPRArg)dp; \
  else \
    cc->stack[nsp++] = (GPRArg)dp;

#endif

#define CCALL_HANDLE_COMPLEXRET \
  /* Return complex float in GPRs and complex double by reference. */ \
  cc->retref = (sz > 8); \
  if (cc->retref) { \
    if (ngpr < maxgpr) \
      cc->gpr[ngpr++] = (GPRArg)dp; \
    else \
      cc->stack[nsp++] = (GPRArg)dp; \
  }

#endif

#define CCALL_HANDLE_COMPLEXRET2 \
  if (!cc->retref) \
    *(int64_t *)dp = *(int64_t *)sp;  /* Copy complex float from GPRs. */

#define CCALL_HANDLE_STRUCTARG \
  ngpr = maxgpr;  /* Pass all structs by value on the stack. */

#define CCALL_HANDLE_COMPLEXARG \
  isfp = 1;  /* Pass complex by value on stack. */

#define CCALL_HANDLE_REGARG \
  if (!isfp) {  /* Only non-FP values may be passed in registers. */ \
    if (n > 1) {  /* Anything > 32 bit is passed on the stack. */ \
      if (!LJ_ABI_WIN) ngpr = maxgpr;  /* Prevent reordering. */ \
    } else if (ngpr + 1 <= maxgpr) { \
      dp = &cc->gpr[ngpr]; \
      ngpr += n; \
      goto done; \
    } \
  }

#elif LJ_TARGET_X64 && LJ_ABI_WIN
/* -- Windows/x64 calling conventions ------------------------------------- */

#define CCALL_HANDLE_STRUCTRET \
  /* Return structs of size 1, 2, 4 or 8 in a GPR. */ \
  cc->retref = !(sz == 1 || sz == 2 || sz == 4 || sz == 8); \
  if (cc->retref) cc->gpr[ngpr++] = (GPRArg)dp;

#define CCALL_HANDLE_COMPLEXRET CCALL_HANDLE_STRUCTRET

#define CCALL_HANDLE_COMPLEXRET2 \
  if (!cc->retref) \
    *(int64_t *)dp = *(int64_t *)sp;  /* Copy complex float from GPRs. */

#define CCALL_HANDLE_STRUCTARG \
  /* Pass structs of size 1, 2, 4 or 8 in a GPR by value. */ \
  if (!(sz == 1 || sz == 2 || sz == 4 || sz == 8)) { \
    rp = cdataptr(lj_cdata_new(cts, did, sz)); \
    sz = CTSIZE_PTR;  /* Pass all other structs by reference. */ \
  }

#define CCALL_HANDLE_COMPLEXARG \
  /* Pass complex float in a GPR and complex double by reference. */ \
  if (sz != 2*sizeof(float)) { \
    rp = cdataptr(lj_cdata_new(cts, did, sz)); \
    sz = CTSIZE_PTR; \
  }

/* Windows/x64 argument registers are strictly positional (use ngpr). */
#define CCALL_HANDLE_REGARG \
  if (isfp) { \
    if (ngpr < maxgpr) { dp = &cc->fpr[ngpr++]; nfpr = ngpr; goto done; } \
  } else { \
    if (ngpr < maxgpr) { dp = &cc->gpr[ngpr++]; goto done; } \
  }

#elif LJ_TARGET_X64
/* -- POSIX/x64 calling conventions --------------------------------------- */

#define CCALL_HANDLE_STRUCTRET \
  int rcl[2]; rcl[0] = rcl[1] = 0; \
  if (ccall_classify_struct(cts, ctr, rcl, 0)) { \
    cc->retref = 1;  /* Return struct by reference. */ \
    cc->gpr[ngpr++] = (GPRArg)dp; \
  } else { \
    cc->retref = 0;  /* Return small structs in registers. */ \
  }

#define CCALL_HANDLE_STRUCTRET2 \
  int rcl[2]; rcl[0] = rcl[1] = 0; \
  ccall_classify_struct(cts, ctr, rcl, 0); \
  ccall_struct_ret(cc, rcl, dp, ctr->size);

#define CCALL_HANDLE_COMPLEXRET \
  /* Complex values are returned in one or two FPRs. */ \
  cc->retref = 0;

#define CCALL_HANDLE_COMPLEXRET2 \
  if (ctr->size == 2*sizeof(float)) {  /* Copy complex float from FPR. */ \
    *(int64_t *)dp = cc->fpr[0].l[0]; \
  } else {  /* Copy non-contiguous complex double from FPRs. */ \
    ((int64_t *)dp)[0] = cc->fpr[0].l[0]; \
    ((int64_t *)dp)[1] = cc->fpr[1].l[0]; \
  }

#define CCALL_HANDLE_STRUCTARG \
  int rcl[2]; rcl[0] = rcl[1] = 0; \
  if (!ccall_classify_struct(cts, d, rcl, 0)) { \
    cc->nsp = nsp; cc->ngpr = ngpr; cc->nfpr = nfpr; \
    if (ccall_struct_arg(cc, cts, d, rcl, o, narg)) goto err_nyi; \
    nsp = cc->nsp; ngpr = cc->ngpr; nfpr = cc->nfpr; \
    continue; \
  }  /* Pass all other structs by value on stack. */

#define CCALL_HANDLE_COMPLEXARG \
  isfp = 2;  /* Pass complex in FPRs or on stack. Needs postprocessing. */

#define CCALL_HANDLE_REGARG \
  if (isfp) {  /* Try to pass argument in FPRs. */ \
    if (nfpr + n <= CCALL_NARG_FPR) { \
      dp = &cc->fpr[nfpr]; \
      nfpr += n; \
      goto done; \
    } \
  } else {  /* Try to pass argument in GPRs. */ \
    /* Note that reordering is explicitly allowed in the x64 ABI. */ \
    if (n <= 2 && ngpr + n <= maxgpr) { \
      dp = &cc->gpr[ngpr]; \
      ngpr += n; \
      goto done; \
    } \
  }

#elif LJ_TARGET_ARM
/* -- ARM calling conventions --------------------------------------------- */

#if LJ_ABI_SOFTFP

#define CCALL_HANDLE_STRUCTRET \
  /* Return structs of size <= 4 in a GPR. */ \
  cc->retref = !(sz <= 4); \
  if (cc->retref) cc->gpr[ngpr++] = (GPRArg)dp;

#define CCALL_HANDLE_COMPLEXRET \
  cc->retref = 1;  /* Return all complex values by reference. */ \
  cc->gpr[ngpr++] = (GPRArg)dp;

#define CCALL_HANDLE_COMPLEXRET2 \
  UNUSED(dp); /* Nothing to do. */

#define CCALL_HANDLE_STRUCTARG \
  /* Pass all structs by value in registers and/or on the stack. */

#define CCALL_HANDLE_COMPLEXARG \
  /* Pass complex by value in 2 or 4 GPRs. */

#define CCALL_HANDLE_REGARG_FP1
#define CCALL_HANDLE_REGARG_FP2

#else

#define CCALL_HANDLE_STRUCTRET \
  cc->retref = !ccall_classify_struct(cts, ctr, ct); \
  if (cc->retref) cc->gpr[ngpr++] = (GPRArg)dp;

#define CCALL_HANDLE_STRUCTRET2 \
  if (ccall_classify_struct(cts, ctr, ct) > 1) sp = (uint8_t *)&cc->fpr[0]; \
  memcpy(dp, sp, ctr->size);

#define CCALL_HANDLE_COMPLEXRET \
  if (!(ct->info & CTF_VARARG)) cc->retref = 0;  /* Return complex in FPRs. */

#define CCALL_HANDLE_COMPLEXRET2 \
  if (!(ct->info & CTF_VARARG)) memcpy(dp, &cc->fpr[0], ctr->size);

#define CCALL_HANDLE_STRUCTARG \
  isfp = (ccall_classify_struct(cts, d, ct) > 1);
  /* Pass all structs by value in registers and/or on the stack. */

#define CCALL_HANDLE_COMPLEXARG \
  isfp = 1;  /* Pass complex by value in FPRs or on stack. */

#define CCALL_HANDLE_REGARG_FP1 \
  if (isfp && !(ct->info & CTF_VARARG)) { \
    if ((d->info & CTF_ALIGN) > CTALIGN_PTR) { \
      if (nfpr + (n >> 1) <= CCALL_NARG_FPR) { \
	dp = &cc->fpr[nfpr]; \
	nfpr += (n >> 1); \
	goto done; \
      } \
    } else { \
      if (sz > 1 && fprodd != nfpr) fprodd = 0; \
      if (fprodd) { \
	if (2*nfpr+n <= 2*CCALL_NARG_FPR+1) { \
	  dp = (void *)&cc->fpr[fprodd-1].f[1]; \
	  nfpr += (n >> 1); \
	  if ((n & 1)) fprodd = 0; else fprodd = nfpr-1; \
	  goto done; \
	} \
      } else { \
	if (2*nfpr+n <= 2*CCALL_NARG_FPR) { \
	  dp = (void *)&cc->fpr[nfpr]; \
	  nfpr += (n >> 1); \
	  if ((n & 1)) fprodd = ++nfpr; else fprodd = 0; \
	  goto done; \
	} \
      } \
    } \
    fprodd = 0;  /* No reordering after the first FP value is on stack. */ \
  } else {

#define CCALL_HANDLE_REGARG_FP2	}

#endif

#define CCALL_HANDLE_REGARG \
  CCALL_HANDLE_REGARG_FP1 \
  if ((d->info & CTF_ALIGN) > CTALIGN_PTR) { \
    if (ngpr < maxgpr) \
      ngpr = (ngpr + 1u) & ~1u;  /* Align to regpair. */ \
  } \
  if (ngpr < maxgpr) { \
    dp = &cc->gpr[ngpr]; \
    if (ngpr + n > maxgpr) { \
      nsp += ngpr + n - maxgpr;  /* Assumes contiguous gpr/stack fields. */ \
      if (nsp > CCALL_MAXSTACK) goto err_nyi;  /* Too many arguments. */ \
      ngpr = maxgpr; \
    } else { \
      ngpr += n; \
    } \
    goto done; \
  } CCALL_HANDLE_REGARG_FP2

#define CCALL_HANDLE_RET \
  if ((ct->info & CTF_VARARG)) sp = (uint8_t *)&cc->gpr[0];

#elif LJ_TARGET_PPC
/* -- PPC calling conventions --------------------------------------------- */

#define CCALL_HANDLE_STRUCTRET \
  cc->retref = 1;  /* Return all structs by reference. */ \
  cc->gpr[ngpr++] = (GPRArg)dp;

#define CCALL_HANDLE_COMPLEXRET \
  /* Complex values are returned in 2 or 4 GPRs. */ \
  cc->retref = 0;

#define CCALL_HANDLE_COMPLEXRET2 \
  memcpy(dp, sp, ctr->size);  /* Copy complex from GPRs. */

#define CCALL_HANDLE_STRUCTARG \
  rp = cdataptr(lj_cdata_new(cts, did, sz)); \
  sz = CTSIZE_PTR;  /* Pass all structs by reference. */

#define CCALL_HANDLE_COMPLEXARG \
  /* Pass complex by value in 2 or 4 GPRs. */

#define CCALL_HANDLE_REGARG \
  if (isfp) {  /* Try to pass argument in FPRs. */ \
    if (nfpr + 1 <= CCALL_NARG_FPR) { \
      dp = &cc->fpr[nfpr]; \
      nfpr += 1; \
      d = ctype_get(cts, CTID_DOUBLE);  /* FPRs always hold doubles. */ \
      goto done; \
    } \
  } else {  /* Try to pass argument in GPRs. */ \
    if (n > 1) { \
      lua_assert(n == 2 || n == 4);  /* int64_t or complex (float). */ \
      if (ctype_isinteger(d->info)) \
	ngpr = (ngpr + 1u) & ~1u;  /* Align int64_t to regpair. */ \
      else if (ngpr + n > maxgpr) \
	ngpr = maxgpr;  /* Prevent reordering. */ \
    } \
    if (ngpr + n <= maxgpr) { \
      dp = &cc->gpr[ngpr]; \
      ngpr += n; \
      goto done; \
    } \
  }

#define CCALL_HANDLE_RET \
  if (ctype_isfp(ctr->info) && ctr->size == sizeof(float)) \
    ctr = ctype_get(cts, CTID_DOUBLE);  /* FPRs always hold doubles. */

#elif LJ_TARGET_PPCSPE
/* -- PPC/SPE calling conventions ----------------------------------------- */

#define CCALL_HANDLE_STRUCTRET \
  cc->retref = 1;  /* Return all structs by reference. */ \
  cc->gpr[ngpr++] = (GPRArg)dp;

#define CCALL_HANDLE_COMPLEXRET \
  /* Complex values are returned in 2 or 4 GPRs. */ \
  cc->retref = 0;

#define CCALL_HANDLE_COMPLEXRET2 \
  memcpy(dp, sp, ctr->size);  /* Copy complex from GPRs. */

#define CCALL_HANDLE_STRUCTARG \
  rp = cdataptr(lj_cdata_new(cts, did, sz)); \
  sz = CTSIZE_PTR;  /* Pass all structs by reference. */

#define CCALL_HANDLE_COMPLEXARG \
  /* Pass complex by value in 2 or 4 GPRs. */

/* PPC/SPE has a softfp ABI. */
#define CCALL_HANDLE_REGARG \
  if (n > 1) {  /* Doesn't fit in a single GPR? */ \
    lua_assert(n == 2 || n == 4);  /* int64_t, double or complex (float). */ \
    if (n == 2) \
      ngpr = (ngpr + 1u) & ~1u;  /* Only align 64 bit value to regpair. */ \
    else if (ngpr + n > maxgpr) \
      ngpr = maxgpr;  /* Prevent reordering. */ \
  } \
  if (ngpr + n <= maxgpr) { \
    dp = &cc->gpr[ngpr]; \
    ngpr += n; \
    goto done; \
  }

#elif LJ_TARGET_MIPS
/* -- MIPS calling conventions -------------------------------------------- */

#define CCALL_HANDLE_STRUCTRET \
  cc->retref = 1;  /* Return all structs by reference. */ \
  cc->gpr[ngpr++] = (GPRArg)dp;

#define CCALL_HANDLE_COMPLEXRET \
  /* Complex values are returned in 1 or 2 FPRs. */ \
  cc->retref = 0;

#define CCALL_HANDLE_COMPLEXRET2 \
  if (ctr->size == 2*sizeof(float)) {  /* Copy complex float from FPRs. */ \
    ((float *)dp)[0] = cc->fpr[0].f; \
    ((float *)dp)[1] = cc->fpr[1].f; \
  } else {  /* Copy complex double from FPRs. */ \
    ((double *)dp)[0] = cc->fpr[0].d; \
    ((double *)dp)[1] = cc->fpr[1].d; \
  }

#define CCALL_HANDLE_STRUCTARG \
  /* Pass all structs by value in registers and/or on the stack. */

#define CCALL_HANDLE_COMPLEXARG \
  /* Pass complex by value in 2 or 4 GPRs. */

#define CCALL_HANDLE_REGARG \
  if (isfp && nfpr < CCALL_NARG_FPR && !(ct->info & CTF_VARARG)) { \
    /* Try to pass argument in FPRs. */ \
    dp = n == 1 ? (void *)&cc->fpr[nfpr].f : (void *)&cc->fpr[nfpr].d; \
    nfpr++; ngpr += n; \
    goto done; \
  } else {  /* Try to pass argument in GPRs. */ \
    nfpr = CCALL_NARG_FPR; \
    if ((d->info & CTF_ALIGN) > CTALIGN_PTR) \
      ngpr = (ngpr + 1u) & ~1u;  /* Align to regpair. */ \
    if (ngpr < maxgpr) { \
      dp = &cc->gpr[ngpr]; \
      if (ngpr + n > maxgpr) { \
	nsp += ngpr + n - maxgpr;  /* Assumes contiguous gpr/stack fields. */ \
	if (nsp > CCALL_MAXSTACK) goto err_nyi;  /* Too many arguments. */ \
	ngpr = maxgpr; \
      } else { \
	ngpr += n; \
      } \
      goto done; \
    } \
  }

#define CCALL_HANDLE_RET \
  if (ctype_isfp(ctr->info) && ctr->size == sizeof(float)) \
    sp = (uint8_t *)&cc->fpr[0].f;

#else
#error "Missing calling convention definitions for this architecture"
#endif

#ifndef CCALL_HANDLE_STRUCTRET2
#define CCALL_HANDLE_STRUCTRET2 \
  memcpy(dp, sp, ctr->size);  /* Copy struct return value from GPRs. */
#endif

/* -- x86 OSX ABI struct classification ----------------------------------- */

#if LJ_TARGET_X86 && LJ_TARGET_OSX

/* Check for struct with single FP field. */
static int ccall_classify_struct(CTState *cts, CType *ct)
{
  CTSize sz = ct->size;
  if (!(sz == sizeof(float) || sz == sizeof(double))) return 0;
  if ((ct->info & CTF_UNION)) return 0;
  while (ct->sib) {
    ct = ctype_get(cts, ct->sib);
    if (ctype_isfield(ct->info)) {
      CType *sct = ctype_rawchild(cts, ct);
      if (ctype_isfp(sct->info)) {
	if (sct->size == sz)
	  return (sz >> 2);  /* Return 1 for float or 2 for double. */
      } else if (ctype_isstruct(sct->info)) {
	if (sct->size)
	  return ccall_classify_struct(cts, sct);
      } else {
	break;
      }
    } else if (ctype_isbitfield(ct->info)) {
      break;
    } else if (ctype_isxattrib(ct->info, CTA_SUBTYPE)) {
      CType *sct = ctype_rawchild(cts, ct);
      if (sct->size)
	return ccall_classify_struct(cts, sct);
    }
  }
  return 0;
}

#endif

/* -- x64 struct classification ------------------------------------------- */

#if LJ_TARGET_X64 && !LJ_ABI_WIN

/* Register classes for x64 struct classification. */
#define CCALL_RCL_INT	1
#define CCALL_RCL_SSE	2
#define CCALL_RCL_MEM	4
/* NYI: classify vectors. */

static int ccall_classify_struct(CTState *cts, CType *ct, int *rcl, CTSize ofs);

/* Classify a C type. */
static void ccall_classify_ct(CTState *cts, CType *ct, int *rcl, CTSize ofs)
{
  if (ctype_isarray(ct->info)) {
    CType *cct = ctype_rawchild(cts, ct);
    CTSize eofs, esz = cct->size, asz = ct->size;
    for (eofs = 0; eofs < asz; eofs += esz)
      ccall_classify_ct(cts, cct, rcl, ofs+eofs);
  } else if (ctype_isstruct(ct->info)) {
    ccall_classify_struct(cts, ct, rcl, ofs);
  } else {
    int cl = ctype_isfp(ct->info) ? CCALL_RCL_SSE : CCALL_RCL_INT;
    lua_assert(ctype_hassize(ct->info));
    if ((ofs & (ct->size-1))) cl = CCALL_RCL_MEM;  /* Unaligned. */
    rcl[(ofs >= 8)] |= cl;
  }
}

/* Recursively classify a struct based on its fields. */
static int ccall_classify_struct(CTState *cts, CType *ct, int *rcl, CTSize ofs)
{
  if (ct->size > 16) return CCALL_RCL_MEM;  /* Too big, gets memory class. */
  while (ct->sib) {
    CTSize fofs;
    ct = ctype_get(cts, ct->sib);
    fofs = ofs+ct->size;
    if (ctype_isfield(ct->info))
      ccall_classify_ct(cts, ctype_rawchild(cts, ct), rcl, fofs);
    else if (ctype_isbitfield(ct->info))
      rcl[(fofs >= 8)] |= CCALL_RCL_INT;  /* NYI: unaligned bitfields? */
    else if (ctype_isxattrib(ct->info, CTA_SUBTYPE))
      ccall_classify_struct(cts, ctype_rawchild(cts, ct), rcl, fofs);
  }
  return ((rcl[0]|rcl[1]) & CCALL_RCL_MEM);  /* Memory class? */
}

/* Try to split up a small struct into registers. */
static int ccall_struct_reg(CCallState *cc, GPRArg *dp, int *rcl)
{
  MSize ngpr = cc->ngpr, nfpr = cc->nfpr;
  uint32_t i;
  for (i = 0; i < 2; i++) {
    lua_assert(!(rcl[i] & CCALL_RCL_MEM));
    if ((rcl[i] & CCALL_RCL_INT)) {  /* Integer class takes precedence. */
      if (ngpr >= CCALL_NARG_GPR) return 1;  /* Register overflow. */
      cc->gpr[ngpr++] = dp[i];
    } else if ((rcl[i] & CCALL_RCL_SSE)) {
      if (nfpr >= CCALL_NARG_FPR) return 1;  /* Register overflow. */
      cc->fpr[nfpr++].l[0] = dp[i];
    }
  }
  cc->ngpr = ngpr; cc->nfpr = nfpr;
  return 0;  /* Ok. */
}

/* Pass a small struct argument. */
static int ccall_struct_arg(CCallState *cc, CTState *cts, CType *d, int *rcl,
			    TValue *o, int narg)
{
  GPRArg dp[2];
  dp[0] = dp[1] = 0;
  /* Convert to temp. struct. */
  lj_cconv_ct_tv(cts, d, (uint8_t *)dp, o, CCF_ARG(narg));
  if (ccall_struct_reg(cc, dp, rcl)) {  /* Register overflow? Pass on stack. */
    MSize nsp = cc->nsp, n = rcl[1] ? 2 : 1;
    if (nsp + n > CCALL_MAXSTACK) return 1;  /* Too many arguments. */
    cc->nsp = nsp + n;
    memcpy(&cc->stack[nsp], dp, n*CTSIZE_PTR);
  }
  return 0;  /* Ok. */
}

/* Combine returned small struct. */
static void ccall_struct_ret(CCallState *cc, int *rcl, uint8_t *dp, CTSize sz)
{
  GPRArg sp[2];
  MSize ngpr = 0, nfpr = 0;
  uint32_t i;
  for (i = 0; i < 2; i++) {
    if ((rcl[i] & CCALL_RCL_INT)) {  /* Integer class takes precedence. */
      sp[i] = cc->gpr[ngpr++];
    } else if ((rcl[i] & CCALL_RCL_SSE)) {
      sp[i] = cc->fpr[nfpr++].l[0];
    }
  }
  memcpy(dp, sp, sz);
}
#endif

/* -- ARM hard-float ABI struct classification ---------------------------- */

#if LJ_TARGET_ARM && !LJ_ABI_SOFTFP

/* Classify a struct based on its fields. */
static unsigned int ccall_classify_struct(CTState *cts, CType *ct, CType *ctf)
{
  CTSize sz = ct->size;
  unsigned int r = 0, n = 0, isu = (ct->info & CTF_UNION);
  if ((ctf->info & CTF_VARARG)) goto noth;
  while (ct->sib) {
    CType *sct;
    ct = ctype_get(cts, ct->sib);
    if (ctype_isfield(ct->info)) {
      sct = ctype_rawchild(cts, ct);
      if (ctype_isfp(sct->info)) {
	r |= sct->size;
	if (!isu) n++; else if (n == 0) n = 1;
      } else if (ctype_iscomplex(sct->info)) {
	r |= (sct->size >> 1);
	if (!isu) n += 2; else if (n < 2) n = 2;
      } else if (ctype_isstruct(sct->info)) {
	goto substruct;
      } else {
	goto noth;
      }
    } else if (ctype_isbitfield(ct->info)) {
      goto noth;
    } else if (ctype_isxattrib(ct->info, CTA_SUBTYPE)) {
      sct = ctype_rawchild(cts, ct);
    substruct:
      if (sct->size > 0) {
	unsigned int s = ccall_classify_struct(cts, sct, ctf);
	if (s <= 1) goto noth;
	r |= (s & 255);
	if (!isu) n += (s >> 8); else if (n < (s >>8)) n = (s >> 8);
      }
    }
  }
  if ((r == 4 || r == 8) && n <= 4)
    return r + (n << 8);
noth:  /* Not a homogeneous float/double aggregate. */
  return (sz <= 4);  /* Return structs of size <= 4 in a GPR. */
}

#endif

/* -- Common C call handling ---------------------------------------------- */

/* Infer the destination CTypeID for a vararg argument. */
CTypeID lj_ccall_ctid_vararg(CTState *cts, cTValue *o)
{
  if (tvisnumber(o)) {
    return CTID_DOUBLE;
  } else if (tviscdata(o)) {
    CTypeID id = cdataV(o)->ctypeid;
    CType *s = ctype_get(cts, id);
    if (ctype_isrefarray(s->info)) {
      return lj_ctype_intern(cts,
	       CTINFO(CT_PTR, CTALIGN_PTR|ctype_cid(s->info)), CTSIZE_PTR);
    } else if (ctype_isstruct(s->info) || ctype_isfunc(s->info)) {
      /* NYI: how to pass a struct by value in a vararg argument? */
      return lj_ctype_intern(cts, CTINFO(CT_PTR, CTALIGN_PTR|id), CTSIZE_PTR);
    } else if (ctype_isfp(s->info) && s->size == sizeof(float)) {
      return CTID_DOUBLE;
    } else {
      return id;
    }
  } else if (tvisstr(o)) {
    return CTID_P_CCHAR;
  } else if (tvisbool(o)) {
    return CTID_BOOL;
  } else {
    return CTID_P_VOID;
  }
}

/* Setup arguments for C call. */
static int ccall_set_args(lua_State *L, CTState *cts, CType *ct,
			  CCallState *cc)
{
  int gcsteps = 0;
  TValue *o, *top = L->top;
  CTypeID fid;
  CType *ctr;
  MSize maxgpr, ngpr = 0, nsp = 0, narg;
#if CCALL_NARG_FPR
  MSize nfpr = 0;
#if LJ_TARGET_ARM
  MSize fprodd = 0;
#endif
#endif

  /* Clear unused regs to get some determinism in case of misdeclaration. */
  memset(cc->gpr, 0, sizeof(cc->gpr));
#if CCALL_NUM_FPR
  memset(cc->fpr, 0, sizeof(cc->fpr));
#endif

#if LJ_TARGET_X86
  /* x86 has several different calling conventions. */
  cc->resx87 = 0;
  switch (ctype_cconv(ct->info)) {
  case CTCC_FASTCALL: maxgpr = 2; break;
  case CTCC_THISCALL: maxgpr = 1; break;
  default: maxgpr = 0; break;
  }
#else
  maxgpr = CCALL_NARG_GPR;
#endif

  /* Perform required setup for some result types. */
  ctr = ctype_rawchild(cts, ct);
  if (ctype_isvector(ctr->info)) {
    if (!(CCALL_VECTOR_REG && (ctr->size == 8 || ctr->size == 16)))
      goto err_nyi;
  } else if (ctype_iscomplex(ctr->info) || ctype_isstruct(ctr->info)) {
    /* Preallocate cdata object and anchor it after arguments. */
    CTSize sz = ctr->size;
    GCcdata *cd = lj_cdata_new(cts, ctype_cid(ct->info), sz);
    void *dp = cdataptr(cd);
    setcdataV(L, L->top++, cd);
    if (ctype_isstruct(ctr->info)) {
      CCALL_HANDLE_STRUCTRET
    } else {
      CCALL_HANDLE_COMPLEXRET
    }
#if LJ_TARGET_X86
  } else if (ctype_isfp(ctr->info)) {
    cc->resx87 = ctr->size == sizeof(float) ? 1 : 2;
#endif
  }

  /* Skip initial attributes. */
  fid = ct->sib;
  while (fid) {
    CType *ctf = ctype_get(cts, fid);
    if (!ctype_isattrib(ctf->info)) break;
    fid = ctf->sib;
  }

  /* Walk through all passed arguments. */
  for (o = L->base+1, narg = 1; o < top; o++, narg++) {
    CTypeID did;
    CType *d;
    CTSize sz;
    MSize n, isfp = 0, isva = 0;
    void *dp, *rp = NULL;

    if (fid) {  /* Get argument type from field. */
      CType *ctf = ctype_get(cts, fid);
      fid = ctf->sib;
      lua_assert(ctype_isfield(ctf->info));
      did = ctype_cid(ctf->info);
    } else {
      if (!(ct->info & CTF_VARARG))
	lj_err_caller(L, LJ_ERR_FFI_NUMARG);  /* Too many arguments. */
      did = lj_ccall_ctid_vararg(cts, o);  /* Infer vararg type. */
      isva = 1;
    }
    d = ctype_raw(cts, did);
    sz = d->size;

    /* Find out how (by value/ref) and where (GPR/FPR) to pass an argument. */
    if (ctype_isnum(d->info)) {
      if (sz > 8) goto err_nyi;
      if ((d->info & CTF_FP))
	isfp = 1;
    } else if (ctype_isvector(d->info)) {
      if (CCALL_VECTOR_REG && (sz == 8 || sz == 16))
	isfp = 1;
      else
	goto err_nyi;
    } else if (ctype_isstruct(d->info)) {
      CCALL_HANDLE_STRUCTARG
    } else if (ctype_iscomplex(d->info)) {
      CCALL_HANDLE_COMPLEXARG
    } else {
      sz = CTSIZE_PTR;
    }
    sz = (sz + CTSIZE_PTR-1) & ~(CTSIZE_PTR-1);
    n = sz / CTSIZE_PTR;  /* Number of GPRs or stack slots needed. */

    CCALL_HANDLE_REGARG  /* Handle register arguments. */

    /* Otherwise pass argument on stack. */
    if (CCALL_ALIGN_STACKARG && !rp && (d->info & CTF_ALIGN) > CTALIGN_PTR) {
      MSize align = (1u << ctype_align(d->info-CTALIGN_PTR)) -1;
      nsp = (nsp + align) & ~align;  /* Align argument on stack. */
    }
    if (nsp + n > CCALL_MAXSTACK) {  /* Too many arguments. */
    err_nyi:
      lj_err_caller(L, LJ_ERR_FFI_NYICALL);
    }
    dp = &cc->stack[nsp];
    nsp += n;
    isva = 0;

  done:
    if (rp) {  /* Pass by reference. */
      gcsteps++;
      *(void **)dp = rp;
      dp = rp;
    }
    lj_cconv_ct_tv(cts, d, (uint8_t *)dp, o, CCF_ARG(narg));
    /* Extend passed integers to 32 bits at least. */
    if (ctype_isinteger_or_bool(d->info) && d->size < 4) {
      if (d->info & CTF_UNSIGNED)
	*(uint32_t *)dp = d->size == 1 ? (uint32_t)*(uint8_t *)dp :
					 (uint32_t)*(uint16_t *)dp;
      else
	*(int32_t *)dp = d->size == 1 ? (int32_t)*(int8_t *)dp :
					(int32_t)*(int16_t *)dp;
    }
#if LJ_TARGET_X64 && LJ_ABI_WIN
    if (isva) {  /* Windows/x64 mirrors varargs in both register sets. */
      if (nfpr == ngpr)
	cc->gpr[ngpr-1] = cc->fpr[ngpr-1].l[0];
      else
	cc->fpr[ngpr-1].l[0] = cc->gpr[ngpr-1];
    }
#else
    UNUSED(isva);
#endif
#if LJ_TARGET_X64 && !LJ_ABI_WIN
    if (isfp == 2 && n == 2 && (uint8_t *)dp == (uint8_t *)&cc->fpr[nfpr-2]) {
      cc->fpr[nfpr-1].d[0] = cc->fpr[nfpr-2].d[1];  /* Split complex double. */
      cc->fpr[nfpr-2].d[1] = 0;
    }
#else
    UNUSED(isfp);
#endif
  }
  if (fid) lj_err_caller(L, LJ_ERR_FFI_NUMARG);  /* Too few arguments. */

#if LJ_TARGET_X64 || LJ_TARGET_PPC
  cc->nfpr = nfpr;  /* Required for vararg functions. */
#endif
  cc->nsp = nsp;
  cc->spadj = (CCALL_SPS_FREE + CCALL_SPS_EXTRA)*CTSIZE_PTR;
  if (nsp > CCALL_SPS_FREE)
    cc->spadj += (((nsp-CCALL_SPS_FREE)*CTSIZE_PTR + 15u) & ~15u);
  return gcsteps;
}

/* Get results from C call. */
static int ccall_get_results(lua_State *L, CTState *cts, CType *ct,
			     CCallState *cc, int *ret)
{
  CType *ctr = ctype_rawchild(cts, ct);
  uint8_t *sp = (uint8_t *)&cc->gpr[0];
  if (ctype_isvoid(ctr->info)) {
    *ret = 0;  /* Zero results. */
    return 0;  /* No additional GC step. */
  }
  *ret = 1;  /* One result. */
  if (ctype_isstruct(ctr->info)) {
    /* Return cdata object which is already on top of stack. */
    if (!cc->retref) {
      void *dp = cdataptr(cdataV(L->top-1));  /* Use preallocated object. */
      CCALL_HANDLE_STRUCTRET2
    }
    return 1;  /* One GC step. */
  }
  if (ctype_iscomplex(ctr->info)) {
    /* Return cdata object which is already on top of stack. */
    void *dp = cdataptr(cdataV(L->top-1));  /* Use preallocated object. */
    CCALL_HANDLE_COMPLEXRET2
    return 1;  /* One GC step. */
  }
  if (LJ_BE && ctype_isinteger_or_bool(ctr->info) && ctr->size < CTSIZE_PTR)
    sp += (CTSIZE_PTR - ctr->size);
#if CCALL_NUM_FPR
  if (ctype_isfp(ctr->info) || ctype_isvector(ctr->info))
    sp = (uint8_t *)&cc->fpr[0];
#endif
#ifdef CCALL_HANDLE_RET
  CCALL_HANDLE_RET
#endif
  /* No reference types end up here, so there's no need for the CTypeID. */
  lua_assert(!(ctype_isrefarray(ctr->info) || ctype_isstruct(ctr->info)));
  return lj_cconv_tv_ct(cts, ctr, 0, L->top-1, sp);
}

/* Call C function. */
int lj_ccall_func(lua_State *L, GCcdata *cd)
{
  CTState *cts = ctype_cts(L);
  CType *ct = ctype_raw(cts, cd->ctypeid);
  CTSize sz = CTSIZE_PTR;
  if (ctype_isptr(ct->info)) {
    sz = ct->size;
    ct = ctype_rawchild(cts, ct);
  }
  if (ctype_isfunc(ct->info)) {
    CCallState cc;
    int gcsteps, ret;
    cc.func = (void (*)(void))cdata_getptr(cdataptr(cd), sz);
    gcsteps = ccall_set_args(L, cts, ct, &cc);
    ct = (CType *)((intptr_t)ct-(intptr_t)cts->tab);
    cts->cb.slot = ~0u;
    lj_vm_ffi_call(&cc);
    if (cts->cb.slot != ~0u) {  /* Blacklist function that called a callback. */
      TValue tv;
      setlightudV(&tv, (void *)cc.func);
      setboolV(lj_tab_set(L, cts->miscmap, &tv), 1);
    }
    ct = (CType *)((intptr_t)ct+(intptr_t)cts->tab);  /* May be reallocated. */
    gcsteps += ccall_get_results(L, cts, ct, &cc, &ret);
#if LJ_TARGET_X86 && LJ_ABI_WIN
    /* Automatically detect __stdcall and fix up C function declaration. */
    if (cc.spadj && ctype_cconv(ct->info) == CTCC_CDECL) {
      CTF_INSERT(ct->info, CCONV, CTCC_STDCALL);
      lj_trace_abort(G(L));
    }
#endif
    while (gcsteps-- > 0)
      lj_gc_check(L);
    return ret;
  }
  return -1;  /* Not a function. */
}

#endif
