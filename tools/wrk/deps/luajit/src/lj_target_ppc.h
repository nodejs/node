/*
** Definitions for PPC CPUs.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_TARGET_PPC_H
#define _LJ_TARGET_PPC_H

/* -- Registers IDs ------------------------------------------------------- */

#define GPRDEF(_) \
  _(R0) _(SP) _(SYS1) _(R3) _(R4) _(R5) _(R6) _(R7) \
  _(R8) _(R9) _(R10) _(R11) _(R12) _(SYS2) _(R14) _(R15) \
  _(R16) _(R17) _(R18) _(R19) _(R20) _(R21) _(R22) _(R23) \
  _(R24) _(R25) _(R26) _(R27) _(R28) _(R29) _(R30) _(R31)
#define FPRDEF(_) \
  _(F0) _(F1) _(F2) _(F3) _(F4) _(F5) _(F6) _(F7) \
  _(F8) _(F9) _(F10) _(F11) _(F12) _(F13) _(F14) _(F15) \
  _(F16) _(F17) _(F18) _(F19) _(F20) _(F21) _(F22) _(F23) \
  _(F24) _(F25) _(F26) _(F27) _(F28) _(F29) _(F30) _(F31)
#define VRIDDEF(_)

#define RIDENUM(name)	RID_##name,

enum {
  GPRDEF(RIDENUM)		/* General-purpose registers (GPRs). */
  FPRDEF(RIDENUM)		/* Floating-point registers (FPRs). */
  RID_MAX,
  RID_TMP = RID_R0,

  /* Calling conventions. */
  RID_RET = RID_R3,
  RID_RETHI = RID_R3,
  RID_RETLO = RID_R4,
  RID_FPRET = RID_F1,

  /* These definitions must match with the *.dasc file(s): */
  RID_BASE = RID_R14,		/* Interpreter BASE. */
  RID_LPC = RID_R16,		/* Interpreter PC. */
  RID_DISPATCH = RID_R17,	/* Interpreter DISPATCH table. */
  RID_LREG = RID_R18,		/* Interpreter L. */
  RID_JGL = RID_R31,		/* On-trace: global_State + 32768. */

  /* Register ranges [min, max) and number of registers. */
  RID_MIN_GPR = RID_R0,
  RID_MAX_GPR = RID_R31+1,
  RID_MIN_FPR = RID_F0,
  RID_MAX_FPR = RID_F31+1,
  RID_NUM_GPR = RID_MAX_GPR - RID_MIN_GPR,
  RID_NUM_FPR = RID_MAX_FPR - RID_MIN_FPR
};

#define RID_NUM_KREF		RID_NUM_GPR
#define RID_MIN_KREF		RID_R0

/* -- Register sets ------------------------------------------------------- */

/* Make use of all registers, except TMP, SP, SYS1, SYS2 and JGL. */
#define RSET_FIXED \
  (RID2RSET(RID_TMP)|RID2RSET(RID_SP)|RID2RSET(RID_SYS1)|\
   RID2RSET(RID_SYS2)|RID2RSET(RID_JGL))
#define RSET_GPR	(RSET_RANGE(RID_MIN_GPR, RID_MAX_GPR) - RSET_FIXED)
#define RSET_FPR	RSET_RANGE(RID_MIN_FPR, RID_MAX_FPR)
#define RSET_ALL	(RSET_GPR|RSET_FPR)
#define RSET_INIT	RSET_ALL

#define RSET_SCRATCH_GPR	(RSET_RANGE(RID_R3, RID_R12+1))
#define RSET_SCRATCH_FPR	(RSET_RANGE(RID_F0, RID_F13+1))
#define RSET_SCRATCH		(RSET_SCRATCH_GPR|RSET_SCRATCH_FPR)
#define REGARG_FIRSTGPR		RID_R3
#define REGARG_LASTGPR		RID_R10
#define REGARG_NUMGPR		8
#define REGARG_FIRSTFPR		RID_F1
#define REGARG_LASTFPR		RID_F8
#define REGARG_NUMFPR		8

/* -- Spill slots --------------------------------------------------------- */

/* Spill slots are 32 bit wide. An even/odd pair is used for FPRs.
**
** SPS_FIXED: Available fixed spill slots in interpreter frame.
** This definition must match with the *.dasc file(s).
**
** SPS_FIRST: First spill slot for general use.
** [sp+12] tmplo word \
** [sp+ 8] tmphi word / tmp dword, parameter area for callee
** [sp+ 4] tmpw, LR of callee
** [sp+ 0] stack chain
*/
#define SPS_FIXED	7
#define SPS_FIRST	4

/* Stack offsets for temporary slots. Used for FP<->int conversions etc. */
#define SPOFS_TMPW	4
#define SPOFS_TMP	8
#define SPOFS_TMPHI	8
#define SPOFS_TMPLO	12

#define sps_scale(slot)		(4 * (int32_t)(slot))
#define sps_align(slot)		(((slot) - SPS_FIXED + 3) & ~3)

/* -- Exit state ---------------------------------------------------------- */

/* This definition must match with the *.dasc file(s). */
typedef struct {
  lua_Number fpr[RID_NUM_FPR];	/* Floating-point registers. */
  int32_t gpr[RID_NUM_GPR];	/* General-purpose registers. */
  int32_t spill[256];		/* Spill slots. */
} ExitState;

/* Highest exit + 1 indicates stack check. */
#define EXITSTATE_CHECKEXIT	1

/* Return the address of a per-trace exit stub. */
static LJ_AINLINE uint32_t *exitstub_trace_addr_(uint32_t *p, uint32_t exitno)
{
  while (*p == 0x60000000) p++;  /* Skip PPCI_NOP. */
  return p + 3 + exitno;
}
/* Avoid dependence on lj_jit.h if only including lj_target.h. */
#define exitstub_trace_addr(T, exitno) \
  exitstub_trace_addr_((MCode *)((char *)(T)->mcode + (T)->szmcode), (exitno))

/* -- Instructions -------------------------------------------------------- */

/* Instruction fields. */
#define PPCF_CC(cc)	((((cc) & 3) << 16) | (((cc) & 4) << 22))
#define PPCF_T(r)	((r) << 21)
#define PPCF_A(r)	((r) << 16)
#define PPCF_B(r)	((r) << 11)
#define PPCF_C(r)	((r) << 6)
#define PPCF_MB(n)	((n) << 6)
#define PPCF_ME(n)	((n) << 1)
#define PPCF_Y		0x00200000
#define PPCF_DOT	0x00000001

typedef enum PPCIns {
  /* Integer instructions. */
  PPCI_MR = 0x7c000378,
  PPCI_NOP = 0x60000000,

  PPCI_LI = 0x38000000,
  PPCI_LIS = 0x3c000000,

  PPCI_ADD = 0x7c000214,
  PPCI_ADDC = 0x7c000014,
  PPCI_ADDO = 0x7c000614,
  PPCI_ADDE = 0x7c000114,
  PPCI_ADDZE = 0x7c000194,
  PPCI_ADDME = 0x7c0001d4,
  PPCI_ADDI = 0x38000000,
  PPCI_ADDIS = 0x3c000000,
  PPCI_ADDIC = 0x30000000,
  PPCI_ADDICDOT = 0x34000000,

  PPCI_SUBF = 0x7c000050,
  PPCI_SUBFC = 0x7c000010,
  PPCI_SUBFO = 0x7c000450,
  PPCI_SUBFE = 0x7c000110,
  PPCI_SUBFZE = 0x7c000190,
  PPCI_SUBFME = 0x7c0001d0,
  PPCI_SUBFIC = 0x20000000,

  PPCI_NEG = 0x7c0000d0,

  PPCI_AND = 0x7c000038,
  PPCI_ANDC = 0x7c000078,
  PPCI_NAND = 0x7c0003b8,
  PPCI_ANDIDOT = 0x70000000,
  PPCI_ANDISDOT = 0x74000000,

  PPCI_OR = 0x7c000378,
  PPCI_NOR = 0x7c0000f8,
  PPCI_ORI = 0x60000000,
  PPCI_ORIS = 0x64000000,

  PPCI_XOR = 0x7c000278,
  PPCI_EQV = 0x7c000238,
  PPCI_XORI = 0x68000000,
  PPCI_XORIS = 0x6c000000,

  PPCI_CMPW = 0x7c000000,
  PPCI_CMPLW = 0x7c000040,
  PPCI_CMPWI = 0x2c000000,
  PPCI_CMPLWI = 0x28000000,

  PPCI_MULLW = 0x7c0001d6,
  PPCI_MULLI = 0x1c000000,
  PPCI_MULLWO = 0x7c0005d6,

  PPCI_EXTSB = 0x7c000774,
  PPCI_EXTSH = 0x7c000734,

  PPCI_SLW = 0x7c000030,
  PPCI_SRW = 0x7c000430,
  PPCI_SRAW = 0x7c000630,
  PPCI_SRAWI = 0x7c000670,

  PPCI_RLWNM = 0x5c000000,
  PPCI_RLWINM = 0x54000000,
  PPCI_RLWIMI = 0x50000000,

  PPCI_B = 0x48000000,
  PPCI_BL = 0x48000001,
  PPCI_BC = 0x40800000,
  PPCI_BCL = 0x40800001,
  PPCI_BCTR = 0x4e800420,
  PPCI_BCTRL = 0x4e800421,

  PPCI_CRANDC = 0x4c000102,
  PPCI_CRXOR = 0x4c000182,
  PPCI_CRAND = 0x4c000202,
  PPCI_CREQV = 0x4c000242,
  PPCI_CRORC = 0x4c000342,
  PPCI_CROR = 0x4c000382,

  PPCI_MFLR = 0x7c0802a6,
  PPCI_MTCTR = 0x7c0903a6,

  PPCI_MCRXR = 0x7c000400,

  /* Load/store instructions. */
  PPCI_LWZ = 0x80000000,
  PPCI_LBZ = 0x88000000,
  PPCI_STW = 0x90000000,
  PPCI_STB = 0x98000000,
  PPCI_LHZ = 0xa0000000,
  PPCI_LHA = 0xa8000000,
  PPCI_STH = 0xb0000000,

  PPCI_STWU = 0x94000000,

  PPCI_LFS = 0xc0000000,
  PPCI_LFD = 0xc8000000,
  PPCI_STFS = 0xd0000000,
  PPCI_STFD = 0xd8000000,

  PPCI_LWZX = 0x7c00002e,
  PPCI_LBZX = 0x7c0000ae,
  PPCI_STWX = 0x7c00012e,
  PPCI_STBX = 0x7c0001ae,
  PPCI_LHZX = 0x7c00022e,
  PPCI_LHAX = 0x7c0002ae,
  PPCI_STHX = 0x7c00032e,

  PPCI_LWBRX = 0x7c00042c,
  PPCI_STWBRX = 0x7c00052c,

  PPCI_LFSX = 0x7c00042e,
  PPCI_LFDX = 0x7c0004ae,
  PPCI_STFSX = 0x7c00052e,
  PPCI_STFDX = 0x7c0005ae,

  /* FP instructions. */
  PPCI_FMR = 0xfc000090,
  PPCI_FNEG = 0xfc000050,
  PPCI_FABS = 0xfc000210,

  PPCI_FRSP = 0xfc000018,
  PPCI_FCTIWZ = 0xfc00001e,

  PPCI_FADD = 0xfc00002a,
  PPCI_FSUB = 0xfc000028,
  PPCI_FMUL = 0xfc000032,
  PPCI_FDIV = 0xfc000024,
  PPCI_FSQRT = 0xfc00002c,

  PPCI_FMADD = 0xfc00003a,
  PPCI_FMSUB = 0xfc000038,
  PPCI_FNMSUB = 0xfc00003c,

  PPCI_FCMPU = 0xfc000000,
  PPCI_FSEL = 0xfc00002e,
} PPCIns;

typedef enum PPCCC {
  CC_GE, CC_LE, CC_NE, CC_NS, CC_LT, CC_GT, CC_EQ, CC_SO
} PPCCC;

#endif
