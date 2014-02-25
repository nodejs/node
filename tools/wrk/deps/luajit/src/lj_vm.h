/*
** Assembler VM interface definitions.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_VM_H
#define _LJ_VM_H

#include "lj_obj.h"

/* Entry points for ASM parts of VM. */
LJ_ASMF void lj_vm_call(lua_State *L, TValue *base, int nres1);
LJ_ASMF int lj_vm_pcall(lua_State *L, TValue *base, int nres1, ptrdiff_t ef);
typedef TValue *(*lua_CPFunction)(lua_State *L, lua_CFunction func, void *ud);
LJ_ASMF int lj_vm_cpcall(lua_State *L, lua_CFunction func, void *ud,
			 lua_CPFunction cp);
LJ_ASMF int lj_vm_resume(lua_State *L, TValue *base, int nres1, ptrdiff_t ef);
LJ_ASMF_NORET void LJ_FASTCALL lj_vm_unwind_c(void *cframe, int errcode);
LJ_ASMF_NORET void LJ_FASTCALL lj_vm_unwind_ff(void *cframe);
LJ_ASMF void lj_vm_unwind_c_eh(void);
LJ_ASMF void lj_vm_unwind_ff_eh(void);
#if LJ_TARGET_X86ORX64
LJ_ASMF void lj_vm_unwind_rethrow(void);
#endif

/* Miscellaneous functions. */
#if LJ_TARGET_X86ORX64
LJ_ASMF int lj_vm_cpuid(uint32_t f, uint32_t res[4]);
#endif
#if LJ_TARGET_PPC
void lj_vm_cachesync(void *start, void *end);
#endif
LJ_ASMF double lj_vm_foldarith(double x, double y, int op);
#if LJ_HASJIT
LJ_ASMF double lj_vm_foldfpm(double x, int op);
#endif
#if !LJ_ARCH_HASFPU
/* Declared in lj_obj.h: LJ_ASMF int32_t lj_vm_tobit(double x); */
#endif

/* Dispatch targets for recording and hooks. */
LJ_ASMF void lj_vm_record(void);
LJ_ASMF void lj_vm_inshook(void);
LJ_ASMF void lj_vm_rethook(void);
LJ_ASMF void lj_vm_callhook(void);

/* Trace exit handling. */
LJ_ASMF void lj_vm_exit_handler(void);
LJ_ASMF void lj_vm_exit_interp(void);

/* Internal math helper functions. */
#if LJ_TARGET_X86ORX64 || LJ_TARGET_PPC
#define lj_vm_floor	floor
#define lj_vm_ceil	ceil
#else
LJ_ASMF double lj_vm_floor(double);
LJ_ASMF double lj_vm_ceil(double);
#if LJ_TARGET_ARM
LJ_ASMF double lj_vm_floor_sf(double);
LJ_ASMF double lj_vm_ceil_sf(double);
#endif
#endif
#if defined(LUAJIT_NO_LOG2) || LJ_TARGET_X86ORX64
LJ_ASMF double lj_vm_log2(double);
#else
#define lj_vm_log2	log2
#endif

#if LJ_HASJIT
#if LJ_TARGET_X86ORX64
LJ_ASMF void lj_vm_floor_sse(void);
LJ_ASMF void lj_vm_ceil_sse(void);
LJ_ASMF void lj_vm_trunc_sse(void);
LJ_ASMF void lj_vm_exp_x87(void);
LJ_ASMF void lj_vm_exp2_x87(void);
LJ_ASMF void lj_vm_pow_sse(void);
LJ_ASMF void lj_vm_powi_sse(void);
#else
#if LJ_TARGET_PPC
#define lj_vm_trunc	trunc
#else
LJ_ASMF double lj_vm_trunc(double);
#if LJ_TARGET_ARM
LJ_ASMF double lj_vm_trunc_sf(double);
#endif
#endif
LJ_ASMF double lj_vm_powi(double, int32_t);
#ifdef LUAJIT_NO_EXP2
LJ_ASMF double lj_vm_exp2(double);
#else
#define lj_vm_exp2	exp2
#endif
#endif
LJ_ASMF int32_t LJ_FASTCALL lj_vm_modi(int32_t, int32_t);
#if LJ_HASFFI
LJ_ASMF int lj_vm_errno(void);
#endif
#endif

/* Continuations for metamethods. */
LJ_ASMF void lj_cont_cat(void);  /* Continue with concatenation. */
LJ_ASMF void lj_cont_ra(void);  /* Store result in RA from instruction. */
LJ_ASMF void lj_cont_nop(void);  /* Do nothing, just continue execution. */
LJ_ASMF void lj_cont_condt(void);  /* Branch if result is true. */
LJ_ASMF void lj_cont_condf(void);  /* Branch if result is false. */
LJ_ASMF void lj_cont_hook(void);  /* Continue from hook yield. */

enum { LJ_CONT_TAILCALL, LJ_CONT_FFI_CALLBACK };  /* Special continuations. */

/* Start of the ASM code. */
LJ_ASMF char lj_vm_asm_begin[];

/* Bytecode offsets are relative to lj_vm_asm_begin. */
#define makeasmfunc(ofs)	((ASMFunction)(lj_vm_asm_begin + (ofs)))

#endif
