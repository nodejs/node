/*
** FFI C callback handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#include "lj_obj.h"

#if LJ_HASFFI

#include "lj_gc.h"
#include "lj_err.h"
#include "lj_tab.h"
#include "lj_state.h"
#include "lj_frame.h"
#include "lj_ctype.h"
#include "lj_cconv.h"
#include "lj_ccall.h"
#include "lj_ccallback.h"
#include "lj_target.h"
#include "lj_mcode.h"
#include "lj_trace.h"
#include "lj_vm.h"

/* -- Target-specific handling of callback slots -------------------------- */

#define CALLBACK_MCODE_SIZE	(LJ_PAGESIZE * LJ_NUM_CBPAGE)

#if LJ_OS_NOJIT

/* Disabled callback support. */
#define CALLBACK_SLOT2OFS(slot)	(0*(slot))
#define CALLBACK_OFS2SLOT(ofs)	(0*(ofs))
#define CALLBACK_MAX_SLOT	0

#elif LJ_TARGET_X86ORX64

#define CALLBACK_MCODE_HEAD	(LJ_64 ? 8 : 0)
#define CALLBACK_MCODE_GROUP	(-2+1+2+5+(LJ_64 ? 6 : 5))

#define CALLBACK_SLOT2OFS(slot) \
  (CALLBACK_MCODE_HEAD + CALLBACK_MCODE_GROUP*((slot)/32) + 4*(slot))

static MSize CALLBACK_OFS2SLOT(MSize ofs)
{
  MSize group;
  ofs -= CALLBACK_MCODE_HEAD;
  group = ofs / (32*4 + CALLBACK_MCODE_GROUP);
  return (ofs % (32*4 + CALLBACK_MCODE_GROUP))/4 + group*32;
}

#define CALLBACK_MAX_SLOT \
  (((CALLBACK_MCODE_SIZE-CALLBACK_MCODE_HEAD)/(CALLBACK_MCODE_GROUP+4*32))*32)

#elif LJ_TARGET_ARM

#define CALLBACK_MCODE_HEAD		32
#define CALLBACK_SLOT2OFS(slot)		(CALLBACK_MCODE_HEAD + 8*(slot))
#define CALLBACK_OFS2SLOT(ofs)		(((ofs)-CALLBACK_MCODE_HEAD)/8)
#define CALLBACK_MAX_SLOT		(CALLBACK_OFS2SLOT(CALLBACK_MCODE_SIZE))

#elif LJ_TARGET_PPC

#define CALLBACK_MCODE_HEAD		24
#define CALLBACK_SLOT2OFS(slot)		(CALLBACK_MCODE_HEAD + 8*(slot))
#define CALLBACK_OFS2SLOT(ofs)		(((ofs)-CALLBACK_MCODE_HEAD)/8)
#define CALLBACK_MAX_SLOT		(CALLBACK_OFS2SLOT(CALLBACK_MCODE_SIZE))

#elif LJ_TARGET_MIPS

#define CALLBACK_MCODE_HEAD		24
#define CALLBACK_SLOT2OFS(slot)		(CALLBACK_MCODE_HEAD + 8*(slot))
#define CALLBACK_OFS2SLOT(ofs)		(((ofs)-CALLBACK_MCODE_HEAD)/8)
#define CALLBACK_MAX_SLOT		(CALLBACK_OFS2SLOT(CALLBACK_MCODE_SIZE))

#else

/* Missing support for this architecture. */
#define CALLBACK_SLOT2OFS(slot)	(0*(slot))
#define CALLBACK_OFS2SLOT(ofs)	(0*(ofs))
#define CALLBACK_MAX_SLOT	0

#endif

/* Convert callback slot number to callback function pointer. */
static void *callback_slot2ptr(CTState *cts, MSize slot)
{
  return (uint8_t *)cts->cb.mcode + CALLBACK_SLOT2OFS(slot);
}

/* Convert callback function pointer to slot number. */
MSize lj_ccallback_ptr2slot(CTState *cts, void *p)
{
  uintptr_t ofs = (uintptr_t)((uint8_t *)p -(uint8_t *)cts->cb.mcode);
  if (ofs < CALLBACK_MCODE_SIZE) {
    MSize slot = CALLBACK_OFS2SLOT((MSize)ofs);
    if (CALLBACK_SLOT2OFS(slot) == (MSize)ofs)
      return slot;
  }
  return ~0u;  /* Not a known callback function pointer. */
}

/* Initialize machine code for callback function pointers. */
#if LJ_OS_NOJIT
/* Disabled callback support. */
#define callback_mcode_init(g, p)	UNUSED(p)
#elif LJ_TARGET_X86ORX64
static void callback_mcode_init(global_State *g, uint8_t *page)
{
  uint8_t *p = page;
  uint8_t *target = (uint8_t *)(void *)lj_vm_ffi_callback;
  MSize slot;
#if LJ_64
  *(void **)p = target; p += 8;
#endif
  for (slot = 0; slot < CALLBACK_MAX_SLOT; slot++) {
    /* mov al, slot; jmp group */
    *p++ = XI_MOVrib | RID_EAX; *p++ = (uint8_t)slot;
    if ((slot & 31) == 31 || slot == CALLBACK_MAX_SLOT-1) {
      /* push ebp/rbp; mov ah, slot>>8; mov ebp, &g. */
      *p++ = XI_PUSH + RID_EBP;
      *p++ = XI_MOVrib | (RID_EAX+4); *p++ = (uint8_t)(slot >> 8);
      *p++ = XI_MOVri | RID_EBP;
      *(int32_t *)p = i32ptr(g); p += 4;
#if LJ_64
      /* jmp [rip-pageofs] where lj_vm_ffi_callback is stored. */
      *p++ = XI_GROUP5; *p++ = XM_OFS0 + (XOg_JMP<<3) + RID_EBP;
      *(int32_t *)p = (int32_t)(page-(p+4)); p += 4;
#else
      /* jmp lj_vm_ffi_callback. */
      *p++ = XI_JMP; *(int32_t *)p = target-(p+4); p += 4;
#endif
    } else {
      *p++ = XI_JMPs; *p++ = (uint8_t)((2+2)*(31-(slot&31)) - 2);
    }
  }
  lua_assert(p - page <= CALLBACK_MCODE_SIZE);
}
#elif LJ_TARGET_ARM
static void callback_mcode_init(global_State *g, uint32_t *page)
{
  uint32_t *p = page;
  void *target = (void *)lj_vm_ffi_callback;
  MSize slot;
  /* This must match with the saveregs macro in buildvm_arm.dasc. */
  *p++ = ARMI_SUB|ARMF_D(RID_R12)|ARMF_N(RID_R12)|ARMF_M(RID_PC);
  *p++ = ARMI_PUSH|ARMF_N(RID_SP)|RSET_RANGE(RID_R4,RID_R11+1)|RID2RSET(RID_LR);
  *p++ = ARMI_SUB|ARMI_K12|ARMF_D(RID_R12)|ARMF_N(RID_R12)|CALLBACK_MCODE_HEAD;
  *p++ = ARMI_STR|ARMI_LS_P|ARMI_LS_W|ARMF_D(RID_R12)|ARMF_N(RID_SP)|(CFRAME_SIZE-4*9);
  *p++ = ARMI_LDR|ARMI_LS_P|ARMI_LS_U|ARMF_D(RID_R12)|ARMF_N(RID_PC);
  *p++ = ARMI_LDR|ARMI_LS_P|ARMI_LS_U|ARMF_D(RID_PC)|ARMF_N(RID_PC);
  *p++ = u32ptr(g);
  *p++ = u32ptr(target);
  for (slot = 0; slot < CALLBACK_MAX_SLOT; slot++) {
    *p++ = ARMI_MOV|ARMF_D(RID_R12)|ARMF_M(RID_PC);
    *p = ARMI_B | ((page-p-2) & 0x00ffffffu);
    p++;
  }
  lua_assert(p - page <= CALLBACK_MCODE_SIZE);
}
#elif LJ_TARGET_PPC
static void callback_mcode_init(global_State *g, uint32_t *page)
{
  uint32_t *p = page;
  void *target = (void *)lj_vm_ffi_callback;
  MSize slot;
  *p++ = PPCI_LIS | PPCF_T(RID_TMP) | (u32ptr(target) >> 16);
  *p++ = PPCI_LIS | PPCF_T(RID_R12) | (u32ptr(g) >> 16);
  *p++ = PPCI_ORI | PPCF_A(RID_TMP)|PPCF_T(RID_TMP) | (u32ptr(target) & 0xffff);
  *p++ = PPCI_ORI | PPCF_A(RID_R12)|PPCF_T(RID_R12) | (u32ptr(g) & 0xffff);
  *p++ = PPCI_MTCTR | PPCF_T(RID_TMP);
  *p++ = PPCI_BCTR;
  for (slot = 0; slot < CALLBACK_MAX_SLOT; slot++) {
    *p++ = PPCI_LI | PPCF_T(RID_R11) | slot;
    *p = PPCI_B | (((page-p) & 0x00ffffffu) << 2);
    p++;
  }
  lua_assert(p - page <= CALLBACK_MCODE_SIZE);
}
#elif LJ_TARGET_MIPS
static void callback_mcode_init(global_State *g, uint32_t *page)
{
  uint32_t *p = page;
  void *target = (void *)lj_vm_ffi_callback;
  MSize slot;
  *p++ = MIPSI_SW | MIPSF_T(RID_R1)|MIPSF_S(RID_SP) | 0;
  *p++ = MIPSI_LUI | MIPSF_T(RID_R3) | (u32ptr(target) >> 16);
  *p++ = MIPSI_LUI | MIPSF_T(RID_R2) | (u32ptr(g) >> 16);
  *p++ = MIPSI_ORI | MIPSF_T(RID_R3)|MIPSF_S(RID_R3) |(u32ptr(target)&0xffff);
  *p++ = MIPSI_JR | MIPSF_S(RID_R3);
  *p++ = MIPSI_ORI | MIPSF_T(RID_R2)|MIPSF_S(RID_R2) | (u32ptr(g)&0xffff);
  for (slot = 0; slot < CALLBACK_MAX_SLOT; slot++) {
    *p = MIPSI_B | ((page-p-1) & 0x0000ffffu);
    p++;
    *p++ = MIPSI_LI | MIPSF_T(RID_R1) | slot;
  }
  lua_assert(p - page <= CALLBACK_MCODE_SIZE);
}
#else
/* Missing support for this architecture. */
#define callback_mcode_init(g, p)	UNUSED(p)
#endif

/* -- Machine code management --------------------------------------------- */

#if LJ_TARGET_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#elif LJ_TARGET_POSIX

#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS   MAP_ANON
#endif

#endif

/* Allocate and initialize area for callback function pointers. */
static void callback_mcode_new(CTState *cts)
{
  size_t sz = (size_t)CALLBACK_MCODE_SIZE;
  void *p;
  if (CALLBACK_MAX_SLOT == 0)
    lj_err_caller(cts->L, LJ_ERR_FFI_CBACKOV);
#if LJ_TARGET_WINDOWS
  p = VirtualAlloc(NULL, sz, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
  if (!p)
    lj_err_caller(cts->L, LJ_ERR_FFI_CBACKOV);
#elif LJ_TARGET_POSIX
  p = mmap(NULL, sz, (PROT_READ|PROT_WRITE), MAP_PRIVATE|MAP_ANONYMOUS,
	   -1, 0);
  if (p == MAP_FAILED)
    lj_err_caller(cts->L, LJ_ERR_FFI_CBACKOV);
#else
  /* Fallback allocator. Fails if memory is not executable by default. */
  p = lj_mem_new(cts->L, sz);
#endif
  cts->cb.mcode = p;
  callback_mcode_init(cts->g, p);
  lj_mcode_sync(p, (char *)p + sz);
#if LJ_TARGET_WINDOWS
  {
    DWORD oprot;
    VirtualProtect(p, sz, PAGE_EXECUTE_READ, &oprot);
  }
#elif LJ_TARGET_POSIX
  mprotect(p, sz, (PROT_READ|PROT_EXEC));
#endif
}

/* Free area for callback function pointers. */
void lj_ccallback_mcode_free(CTState *cts)
{
  size_t sz = (size_t)CALLBACK_MCODE_SIZE;
  void *p = cts->cb.mcode;
  if (p == NULL) return;
#if LJ_TARGET_WINDOWS
  VirtualFree(p, 0, MEM_RELEASE);
  UNUSED(sz);
#elif LJ_TARGET_POSIX
  munmap(p, sz);
#else
  lj_mem_free(cts->g, p, sz);
#endif
}

/* -- C callback entry ---------------------------------------------------- */

/* Target-specific handling of register arguments. Similar to lj_ccall.c. */
#if LJ_TARGET_X86

#define CALLBACK_HANDLE_REGARG \
  if (!isfp) {  /* Only non-FP values may be passed in registers. */ \
    if (n > 1) {  /* Anything > 32 bit is passed on the stack. */ \
      if (!LJ_ABI_WIN) ngpr = maxgpr;  /* Prevent reordering. */ \
    } else if (ngpr + 1 <= maxgpr) { \
      sp = &cts->cb.gpr[ngpr]; \
      ngpr += n; \
      goto done; \
    } \
  }

#elif LJ_TARGET_X64 && LJ_ABI_WIN

/* Windows/x64 argument registers are strictly positional (use ngpr). */
#define CALLBACK_HANDLE_REGARG \
  if (isfp) { \
    if (ngpr < maxgpr) { sp = &cts->cb.fpr[ngpr++]; UNUSED(nfpr); goto done; } \
  } else { \
    if (ngpr < maxgpr) { sp = &cts->cb.gpr[ngpr++]; goto done; } \
  }

#elif LJ_TARGET_X64

#define CALLBACK_HANDLE_REGARG \
  if (isfp) { \
    if (nfpr + n <= CCALL_NARG_FPR) { \
      sp = &cts->cb.fpr[nfpr]; \
      nfpr += n; \
      goto done; \
    } \
  } else { \
    if (ngpr + n <= maxgpr) { \
      sp = &cts->cb.gpr[ngpr]; \
      ngpr += n; \
      goto done; \
    } \
  }

#elif LJ_TARGET_ARM

#if LJ_ABI_SOFTFP

#define CALLBACK_HANDLE_REGARG_FP1	UNUSED(isfp);
#define CALLBACK_HANDLE_REGARG_FP2

#else

#define CALLBACK_HANDLE_REGARG_FP1 \
  if (isfp) { \
    if (n == 1) { \
      if (fprodd) { \
	sp = &cts->cb.fpr[fprodd-1]; \
	fprodd = 0; \
	goto done; \
      } else if (nfpr + 1 <= CCALL_NARG_FPR) { \
	sp = &cts->cb.fpr[nfpr++]; \
	fprodd = nfpr; \
	goto done; \
      } \
    } else { \
      if (nfpr + 1 <= CCALL_NARG_FPR) { \
	sp = &cts->cb.fpr[nfpr++]; \
	goto done; \
      } \
    } \
    fprodd = 0;  /* No reordering after the first FP value is on stack. */ \
  } else {

#define CALLBACK_HANDLE_REGARG_FP2	}

#endif

#define CALLBACK_HANDLE_REGARG \
  CALLBACK_HANDLE_REGARG_FP1 \
  if (n > 1) ngpr = (ngpr + 1u) & ~1u;  /* Align to regpair. */ \
  if (ngpr + n <= maxgpr) { \
    sp = &cts->cb.gpr[ngpr]; \
    ngpr += n; \
    goto done; \
  } CALLBACK_HANDLE_REGARG_FP2

#elif LJ_TARGET_PPC

#define CALLBACK_HANDLE_REGARG \
  if (isfp) { \
    if (nfpr + 1 <= CCALL_NARG_FPR) { \
      sp = &cts->cb.fpr[nfpr++]; \
      cta = ctype_get(cts, CTID_DOUBLE);  /* FPRs always hold doubles. */ \
      goto done; \
    } \
  } else {  /* Try to pass argument in GPRs. */ \
    if (n > 1) { \
      lua_assert(ctype_isinteger(cta->info) && n == 2);  /* int64_t. */ \
      ngpr = (ngpr + 1u) & ~1u;  /* Align int64_t to regpair. */ \
    } \
    if (ngpr + n <= maxgpr) { \
      sp = &cts->cb.gpr[ngpr]; \
      ngpr += n; \
      goto done; \
    } \
  }

#define CALLBACK_HANDLE_RET \
  if (ctype_isfp(ctr->info) && ctr->size == sizeof(float)) \
    *(double *)dp = *(float *)dp;  /* FPRs always hold doubles. */

#elif LJ_TARGET_MIPS

#define CALLBACK_HANDLE_REGARG \
  if (isfp && nfpr < CCALL_NARG_FPR) {  /* Try to pass argument in FPRs. */ \
    sp = (void *)((uint8_t *)&cts->cb.fpr[nfpr] + ((LJ_BE && n==1) ? 4 : 0)); \
    nfpr++; ngpr += n; \
    goto done; \
  } else {  /* Try to pass argument in GPRs. */ \
    nfpr = CCALL_NARG_FPR; \
    if (n > 1) ngpr = (ngpr + 1u) & ~1u;  /* Align to regpair. */ \
    if (ngpr + n <= maxgpr) { \
      sp = &cts->cb.gpr[ngpr]; \
      ngpr += n; \
      goto done; \
    } \
  }

#define CALLBACK_HANDLE_RET \
  if (ctype_isfp(ctr->info) && ctr->size == sizeof(float)) \
    ((float *)dp)[1] = *(float *)dp;

#else
#error "Missing calling convention definitions for this architecture"
#endif

/* Convert and push callback arguments to Lua stack. */
static void callback_conv_args(CTState *cts, lua_State *L)
{
  TValue *o = L->top;
  intptr_t *stack = cts->cb.stack;
  MSize slot = cts->cb.slot;
  CTypeID id = 0, rid, fid;
  CType *ct;
  GCfunc *fn;
  MSize ngpr = 0, nsp = 0, maxgpr = CCALL_NARG_GPR;
#if CCALL_NARG_FPR
  MSize nfpr = 0;
#if LJ_TARGET_ARM
  MSize fprodd = 0;
#endif
#endif

  if (slot < cts->cb.sizeid && (id = cts->cb.cbid[slot]) != 0) {
    ct = ctype_get(cts, id);
    rid = ctype_cid(ct->info);
    fn = funcV(lj_tab_getint(cts->miscmap, (int32_t)slot));
  } else {  /* Must set up frame first, before throwing the error. */
    ct = NULL;
    rid = 0;
    fn = (GCfunc *)L;
  }
  o->u32.lo = LJ_CONT_FFI_CALLBACK;  /* Continuation returns from callback. */
  o->u32.hi = rid;  /* Return type. x86: +(spadj<<16). */
  o++;
  setframe_gc(o, obj2gco(fn));
  setframe_ftsz(o, (int)((char *)(o+1) - (char *)L->base) + FRAME_CONT);
  L->top = L->base = ++o;
  if (!ct)
    lj_err_caller(cts->L, LJ_ERR_FFI_BADCBACK);
  if (isluafunc(fn))
    setcframe_pc(L->cframe, proto_bc(funcproto(fn))+1);
  lj_state_checkstack(L, LUA_MINSTACK);  /* May throw. */
  o = L->base;  /* Might have been reallocated. */

#if LJ_TARGET_X86
  /* x86 has several different calling conventions. */
  switch (ctype_cconv(ct->info)) {
  case CTCC_FASTCALL: maxgpr = 2; break;
  case CTCC_THISCALL: maxgpr = 1; break;
  default: maxgpr = 0; break;
  }
#endif

  fid = ct->sib;
  while (fid) {
    CType *ctf = ctype_get(cts, fid);
    if (!ctype_isattrib(ctf->info)) {
      CType *cta;
      void *sp;
      CTSize sz;
      int isfp;
      MSize n;
      lua_assert(ctype_isfield(ctf->info));
      cta = ctype_rawchild(cts, ctf);
      isfp = ctype_isfp(cta->info);
      sz = (cta->size + CTSIZE_PTR-1) & ~(CTSIZE_PTR-1);
      n = sz / CTSIZE_PTR;  /* Number of GPRs or stack slots needed. */

      CALLBACK_HANDLE_REGARG  /* Handle register arguments. */

      /* Otherwise pass argument on stack. */
      if (CCALL_ALIGN_STACKARG && LJ_32 && sz == 8)
	nsp = (nsp + 1) & ~1u;  /* Align 64 bit argument on stack. */
      sp = &stack[nsp];
      nsp += n;

    done:
      if (LJ_BE && cta->size < CTSIZE_PTR)
	sp = (void *)((uint8_t *)sp + CTSIZE_PTR-cta->size);
      lj_cconv_tv_ct(cts, cta, 0, o++, sp);
    }
    fid = ctf->sib;
  }
  L->top = o;
#if LJ_TARGET_X86
  /* Store stack adjustment for returns from non-cdecl callbacks. */
  if (ctype_cconv(ct->info) != CTCC_CDECL)
    (L->base-2)->u32.hi |= (nsp << (16+2));
#endif
}

/* Convert Lua object to callback result. */
static void callback_conv_result(CTState *cts, lua_State *L, TValue *o)
{
  CType *ctr = ctype_raw(cts, (uint16_t)(L->base-2)->u32.hi);
#if LJ_TARGET_X86
  cts->cb.gpr[2] = 0;
#endif
  if (!ctype_isvoid(ctr->info)) {
    uint8_t *dp = (uint8_t *)&cts->cb.gpr[0];
#if CCALL_NUM_FPR
    if (ctype_isfp(ctr->info))
      dp = (uint8_t *)&cts->cb.fpr[0];
#endif
    lj_cconv_ct_tv(cts, ctr, dp, o, 0);
#ifdef CALLBACK_HANDLE_RET
    CALLBACK_HANDLE_RET
#endif
    /* Extend returned integers to (at least) 32 bits. */
    if (ctype_isinteger_or_bool(ctr->info) && ctr->size < 4) {
      if (ctr->info & CTF_UNSIGNED)
	*(uint32_t *)dp = ctr->size == 1 ? (uint32_t)*(uint8_t *)dp :
					   (uint32_t)*(uint16_t *)dp;
      else
	*(int32_t *)dp = ctr->size == 1 ? (int32_t)*(int8_t *)dp :
					  (int32_t)*(int16_t *)dp;
    }
#if LJ_TARGET_X86
    if (ctype_isfp(ctr->info))
      cts->cb.gpr[2] = ctr->size == sizeof(float) ? 1 : 2;
#endif
  }
}

/* Enter callback. */
lua_State * LJ_FASTCALL lj_ccallback_enter(CTState *cts, void *cf)
{
  lua_State *L = cts->L;
  global_State *g = cts->g;
  lua_assert(L != NULL);
  if (gcref(g->jit_L)) {
    setstrV(L, L->top++, lj_err_str(L, LJ_ERR_FFI_BADCBACK));
    if (g->panic) g->panic(L);
    exit(EXIT_FAILURE);
  }
  lj_trace_abort(g);  /* Never record across callback. */
  /* Setup C frame. */
  cframe_prev(cf) = L->cframe;
  setcframe_L(cf, L);
  cframe_errfunc(cf) = -1;
  cframe_nres(cf) = 0;
  L->cframe = cf;
  callback_conv_args(cts, L);
  return L;  /* Now call the function on this stack. */
}

/* Leave callback. */
void LJ_FASTCALL lj_ccallback_leave(CTState *cts, TValue *o)
{
  lua_State *L = cts->L;
  GCfunc *fn;
  TValue *obase = L->base;
  L->base = L->top;  /* Keep continuation frame for throwing errors. */
  if (o >= L->base) {
    /* PC of RET* is lost. Point to last line for result conv. errors. */
    fn = curr_func(L);
    if (isluafunc(fn)) {
      GCproto *pt = funcproto(fn);
      setcframe_pc(L->cframe, proto_bc(pt)+pt->sizebc+1);
    }
  }
  callback_conv_result(cts, L, o);
  /* Finally drop C frame and continuation frame. */
  L->cframe = cframe_prev(L->cframe);
  L->top -= 2;
  L->base = obase;
  cts->cb.slot = 0;  /* Blacklist C function that called the callback. */
}

/* -- C callback management ----------------------------------------------- */

/* Get an unused slot in the callback slot table. */
static MSize callback_slot_new(CTState *cts, CType *ct)
{
  CTypeID id = ctype_typeid(cts, ct);
  CTypeID1 *cbid = cts->cb.cbid;
  MSize top;
  for (top = cts->cb.topid; top < cts->cb.sizeid; top++)
    if (LJ_LIKELY(cbid[top] == 0))
      goto found;
#if CALLBACK_MAX_SLOT
  if (top >= CALLBACK_MAX_SLOT)
#endif
    lj_err_caller(cts->L, LJ_ERR_FFI_CBACKOV);
  if (!cts->cb.mcode)
    callback_mcode_new(cts);
  lj_mem_growvec(cts->L, cbid, cts->cb.sizeid, CALLBACK_MAX_SLOT, CTypeID1);
  cts->cb.cbid = cbid;
  memset(cbid+top, 0, (cts->cb.sizeid-top)*sizeof(CTypeID1));
found:
  cbid[top] = id;
  cts->cb.topid = top+1;
  return top;
}

/* Check for function pointer and supported argument/result types. */
static CType *callback_checkfunc(CTState *cts, CType *ct)
{
  int narg = 0;
  if (!ctype_isptr(ct->info) || (LJ_64 && ct->size != CTSIZE_PTR))
    return NULL;
  ct = ctype_rawchild(cts, ct);
  if (ctype_isfunc(ct->info)) {
    CType *ctr = ctype_rawchild(cts, ct);
    CTypeID fid = ct->sib;
    if (!(ctype_isvoid(ctr->info) || ctype_isenum(ctr->info) ||
	  ctype_isptr(ctr->info) || (ctype_isnum(ctr->info) && ctr->size <= 8)))
      return NULL;
    if ((ct->info & CTF_VARARG))
      return NULL;
    while (fid) {
      CType *ctf = ctype_get(cts, fid);
      if (!ctype_isattrib(ctf->info)) {
	CType *cta;
	lua_assert(ctype_isfield(ctf->info));
	cta = ctype_rawchild(cts, ctf);
	if (!(ctype_isenum(cta->info) || ctype_isptr(cta->info) ||
	      (ctype_isnum(cta->info) && cta->size <= 8)) ||
	    ++narg >= LUA_MINSTACK-3)
	  return NULL;
      }
      fid = ctf->sib;
    }
    return ct;
  }
  return NULL;
}

/* Create a new callback and return the callback function pointer. */
void *lj_ccallback_new(CTState *cts, CType *ct, GCfunc *fn)
{
  ct = callback_checkfunc(cts, ct);
  if (ct) {
    MSize slot = callback_slot_new(cts, ct);
    GCtab *t = cts->miscmap;
    setfuncV(cts->L, lj_tab_setint(cts->L, t, (int32_t)slot), fn);
    lj_gc_anybarriert(cts->L, t);
    return callback_slot2ptr(cts, slot);
  }
  return NULL;  /* Bad conversion. */
}

#endif
