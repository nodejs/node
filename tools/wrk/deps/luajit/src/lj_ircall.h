/*
** IR CALL* instruction definitions.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_IRCALL_H
#define _LJ_IRCALL_H

#include "lj_obj.h"
#include "lj_ir.h"
#include "lj_jit.h"

/* C call info for CALL* instructions. */
typedef struct CCallInfo {
  ASMFunction func;		/* Function pointer. */
  uint32_t flags;		/* Number of arguments and flags. */
} CCallInfo;

#define CCI_NARGS(ci)		((ci)->flags & 0xff)	/* Extract # of args. */
#define CCI_NARGS_MAX		32			/* Max. # of args. */

#define CCI_OTSHIFT		16
#define CCI_OPTYPE(ci)		((ci)->flags >> CCI_OTSHIFT)  /* Get op/type. */
#define CCI_OPSHIFT		24
#define CCI_OP(ci)		((ci)->flags >> CCI_OPSHIFT)  /* Get op. */

#define CCI_CALL_N		(IR_CALLN << CCI_OPSHIFT)
#define CCI_CALL_L		(IR_CALLL << CCI_OPSHIFT)
#define CCI_CALL_S		(IR_CALLS << CCI_OPSHIFT)
#define CCI_CALL_FN		(CCI_CALL_N|CCI_CC_FASTCALL)
#define CCI_CALL_FL		(CCI_CALL_L|CCI_CC_FASTCALL)
#define CCI_CALL_FS		(CCI_CALL_S|CCI_CC_FASTCALL)

/* C call info flags. */
#define CCI_L			0x0100	/* Implicit L arg. */
#define CCI_CASTU64		0x0200	/* Cast u64 result to number. */
#define CCI_NOFPRCLOBBER	0x0400	/* Does not clobber any FPRs. */
#define CCI_VARARG		0x0800	/* Vararg function. */

#define CCI_CC_MASK		0x3000	/* Calling convention mask. */
#define CCI_CC_SHIFT		12
/* ORDER CC */
#define CCI_CC_CDECL		0x0000	/* Default cdecl calling convention. */
#define CCI_CC_THISCALL		0x1000	/* Thiscall calling convention. */
#define CCI_CC_FASTCALL		0x2000	/* Fastcall calling convention. */
#define CCI_CC_STDCALL		0x3000	/* Stdcall calling convention. */

/* Helpers for conditional function definitions. */
#define IRCALLCOND_ANY(x)		x

#if LJ_TARGET_X86ORX64
#define IRCALLCOND_FPMATH(x)		NULL
#else
#define IRCALLCOND_FPMATH(x)		x
#endif

#if LJ_SOFTFP
#define IRCALLCOND_SOFTFP(x)		x
#if LJ_HASFFI
#define IRCALLCOND_SOFTFP_FFI(x)	x
#else
#define IRCALLCOND_SOFTFP_FFI(x)	NULL
#endif
#else
#define IRCALLCOND_SOFTFP(x)		NULL
#define IRCALLCOND_SOFTFP_FFI(x)	NULL
#endif

#define LJ_NEED_FP64	(LJ_TARGET_ARM || LJ_TARGET_PPC || LJ_TARGET_MIPS)

#if LJ_HASFFI && (LJ_SOFTFP || LJ_NEED_FP64)
#define IRCALLCOND_FP64_FFI(x)		x
#else
#define IRCALLCOND_FP64_FFI(x)		NULL
#endif

#if LJ_HASFFI
#define IRCALLCOND_FFI(x)		x
#if LJ_32
#define IRCALLCOND_FFI32(x)		x
#else
#define IRCALLCOND_FFI32(x)		NULL
#endif
#else
#define IRCALLCOND_FFI(x)		NULL
#define IRCALLCOND_FFI32(x)		NULL
#endif

#if LJ_SOFTFP
#define ARG1_FP		2	/* Treat as 2 32 bit arguments. */
#else
#define ARG1_FP		1
#endif

#if LJ_32
#define ARG2_64		4	/* Treat as 4 32 bit arguments. */
#else
#define ARG2_64		2
#endif

/* Function definitions for CALL* instructions. */
#define IRCALLDEF(_) \
  _(ANY,	lj_str_cmp,		2,  FN, INT, CCI_NOFPRCLOBBER) \
  _(ANY,	lj_str_new,		3,   S, STR, CCI_L) \
  _(ANY,	lj_strscan_num,		2,  FN, INT, 0) \
  _(ANY,	lj_str_fromint,		2,  FN, STR, CCI_L) \
  _(ANY,	lj_str_fromnum,		2,  FN, STR, CCI_L) \
  _(ANY,	lj_tab_new1,		2,  FS, TAB, CCI_L) \
  _(ANY,	lj_tab_dup,		2,  FS, TAB, CCI_L) \
  _(ANY,	lj_tab_newkey,		3,   S, P32, CCI_L) \
  _(ANY,	lj_tab_len,		1,  FL, INT, 0) \
  _(ANY,	lj_gc_step_jit,		2,  FS, NIL, CCI_L) \
  _(ANY,	lj_gc_barrieruv,	2,  FS, NIL, 0) \
  _(ANY,	lj_mem_newgco,		2,  FS, P32, CCI_L) \
  _(ANY,	lj_math_random_step, 1, FS, NUM, CCI_CASTU64|CCI_NOFPRCLOBBER) \
  _(ANY,	lj_vm_modi,		2,  FN, INT, 0) \
  _(ANY,	sinh,			ARG1_FP,  N, NUM, 0) \
  _(ANY,	cosh,			ARG1_FP,  N, NUM, 0) \
  _(ANY,	tanh,			ARG1_FP,  N, NUM, 0) \
  _(ANY,	fputc,			2,  S, INT, 0) \
  _(ANY,	fwrite,			4,  S, INT, 0) \
  _(ANY,	fflush,			1,  S, INT, 0) \
  /* ORDER FPM */ \
  _(FPMATH,	lj_vm_floor,		ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	lj_vm_ceil,		ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	lj_vm_trunc,		ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	sqrt,			ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	exp,			ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	lj_vm_exp2,		ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	log,			ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	lj_vm_log2,		ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	log10,			ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	sin,			ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	cos,			ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	tan,			ARG1_FP,   N, NUM, 0) \
  _(FPMATH,	lj_vm_powi,		ARG1_FP+1, N, NUM, 0) \
  _(FPMATH,	pow,			ARG1_FP*2, N, NUM, 0) \
  _(FPMATH,	atan2,			ARG1_FP*2, N, NUM, 0) \
  _(FPMATH,	ldexp,			ARG1_FP+1, N, NUM, 0) \
  _(SOFTFP,	lj_vm_tobit,		2,   N, INT, 0) \
  _(SOFTFP,	softfp_add,		4,   N, NUM, 0) \
  _(SOFTFP,	softfp_sub,		4,   N, NUM, 0) \
  _(SOFTFP,	softfp_mul,		4,   N, NUM, 0) \
  _(SOFTFP,	softfp_div,		4,   N, NUM, 0) \
  _(SOFTFP,	softfp_cmp,		4,   N, NIL, 0) \
  _(SOFTFP,	softfp_i2d,		1,   N, NUM, 0) \
  _(SOFTFP,	softfp_d2i,		2,   N, INT, 0) \
  _(SOFTFP_FFI,	softfp_ui2d,		1,   N, NUM, 0) \
  _(SOFTFP_FFI,	softfp_f2d,		1,   N, NUM, 0) \
  _(SOFTFP_FFI,	softfp_d2ui,		2,   N, INT, 0) \
  _(SOFTFP_FFI,	softfp_d2f,		2,   N, FLOAT, 0) \
  _(SOFTFP_FFI,	softfp_i2f,		1,   N, FLOAT, 0) \
  _(SOFTFP_FFI,	softfp_ui2f,		1,   N, FLOAT, 0) \
  _(SOFTFP_FFI,	softfp_f2i,		1,   N, INT, 0) \
  _(SOFTFP_FFI,	softfp_f2ui,		1,   N, INT, 0) \
  _(FP64_FFI,	fp64_l2d,		2,   N, NUM, 0) \
  _(FP64_FFI,	fp64_ul2d,		2,   N, NUM, 0) \
  _(FP64_FFI,	fp64_l2f,		2,   N, FLOAT, 0) \
  _(FP64_FFI,	fp64_ul2f,		2,   N, FLOAT, 0) \
  _(FP64_FFI,	fp64_d2l,		ARG1_FP,   N, I64, 0) \
  _(FP64_FFI,	fp64_d2ul,		ARG1_FP,   N, U64, 0) \
  _(FP64_FFI,	fp64_f2l,		1,   N, I64, 0) \
  _(FP64_FFI,	fp64_f2ul,		1,   N, U64, 0) \
  _(FFI,	lj_carith_divi64,	ARG2_64,   N, I64, CCI_NOFPRCLOBBER) \
  _(FFI,	lj_carith_divu64,	ARG2_64,   N, U64, CCI_NOFPRCLOBBER) \
  _(FFI,	lj_carith_modi64,	ARG2_64,   N, I64, CCI_NOFPRCLOBBER) \
  _(FFI,	lj_carith_modu64,	ARG2_64,   N, U64, CCI_NOFPRCLOBBER) \
  _(FFI,	lj_carith_powi64,	ARG2_64,   N, I64, CCI_NOFPRCLOBBER) \
  _(FFI,	lj_carith_powu64,	ARG2_64,   N, U64, CCI_NOFPRCLOBBER) \
  _(FFI,	lj_cdata_setfin,	2,        FN, P32, CCI_L) \
  _(FFI,	strlen,			1,         L, INTP, 0) \
  _(FFI,	memcpy,			3,         S, PTR, 0) \
  _(FFI,	memset,			3,         S, PTR, 0) \
  _(FFI,	lj_vm_errno,		0,         S, INT, CCI_NOFPRCLOBBER) \
  _(FFI32,	lj_carith_mul64,	ARG2_64,   N, I64, CCI_NOFPRCLOBBER)
  \
  /* End of list. */

typedef enum {
#define IRCALLENUM(cond, name, nargs, kind, type, flags)	IRCALL_##name,
IRCALLDEF(IRCALLENUM)
#undef IRCALLENUM
  IRCALL__MAX
} IRCallID;

LJ_FUNC TRef lj_ir_call(jit_State *J, IRCallID id, ...);

LJ_DATA const CCallInfo lj_ir_callinfo[IRCALL__MAX+1];

/* Soft-float declarations. */
#if LJ_SOFTFP
#if LJ_TARGET_ARM
#define softfp_add __aeabi_dadd
#define softfp_sub __aeabi_dsub
#define softfp_mul __aeabi_dmul
#define softfp_div __aeabi_ddiv
#define softfp_cmp __aeabi_cdcmple
#define softfp_i2d __aeabi_i2d
#define softfp_d2i __aeabi_d2iz
#define softfp_ui2d __aeabi_ui2d
#define softfp_f2d __aeabi_f2d
#define softfp_d2ui __aeabi_d2uiz
#define softfp_d2f __aeabi_d2f
#define softfp_i2f __aeabi_i2f
#define softfp_ui2f __aeabi_ui2f
#define softfp_f2i __aeabi_f2iz
#define softfp_f2ui __aeabi_f2uiz
#define fp64_l2d __aeabi_l2d
#define fp64_ul2d __aeabi_ul2d
#define fp64_l2f __aeabi_l2f
#define fp64_ul2f __aeabi_ul2f
#if LJ_TARGET_IOS
#define fp64_d2l __fixdfdi
#define fp64_d2ul __fixunsdfdi
#define fp64_f2l __fixsfdi
#define fp64_f2ul __fixunssfdi
#else
#define fp64_d2l __aeabi_d2lz
#define fp64_d2ul __aeabi_d2ulz
#define fp64_f2l __aeabi_f2lz
#define fp64_f2ul __aeabi_f2ulz
#endif
#else
#error "Missing soft-float definitions for target architecture"
#endif
extern double softfp_add(double a, double b);
extern double softfp_sub(double a, double b);
extern double softfp_mul(double a, double b);
extern double softfp_div(double a, double b);
extern void softfp_cmp(double a, double b);
extern double softfp_i2d(int32_t a);
extern int32_t softfp_d2i(double a);
#if LJ_HASFFI
extern double softfp_ui2d(uint32_t a);
extern double softfp_f2d(float a);
extern uint32_t softfp_d2ui(double a);
extern float softfp_d2f(double a);
extern float softfp_i2f(int32_t a);
extern float softfp_ui2f(uint32_t a);
extern int32_t softfp_f2i(float a);
extern uint32_t softfp_f2ui(float a);
#endif
#endif

#if LJ_HASFFI && LJ_NEED_FP64 && !(LJ_TARGET_ARM && LJ_SOFTFP)
#ifdef __GNUC__
#define fp64_l2d __floatdidf
#define fp64_ul2d __floatundidf
#define fp64_l2f __floatdisf
#define fp64_ul2f __floatundisf
#define fp64_d2l __fixdfdi
#define fp64_d2ul __fixunsdfdi
#define fp64_f2l __fixsfdi
#define fp64_f2ul __fixunssfdi
#else
#error "Missing fp64 helper definitions for this compiler"
#endif
#endif

#if LJ_HASFFI && (LJ_SOFTFP || LJ_NEED_FP64)
extern double fp64_l2d(int64_t a);
extern double fp64_ul2d(uint64_t a);
extern float fp64_l2f(int64_t a);
extern float fp64_ul2f(uint64_t a);
extern int64_t fp64_d2l(double a);
extern uint64_t fp64_d2ul(double a);
extern int64_t fp64_f2l(float a);
extern uint64_t fp64_f2ul(float a);
#endif

#endif
