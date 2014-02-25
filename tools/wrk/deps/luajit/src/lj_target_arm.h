/*
** Definitions for ARM CPUs.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_TARGET_ARM_H
#define _LJ_TARGET_ARM_H

/* -- Registers IDs ------------------------------------------------------- */

#define GPRDEF(_) \
  _(R0) _(R1) _(R2) _(R3) _(R4) _(R5) _(R6) _(R7) \
  _(R8) _(R9) _(R10) _(R11) _(R12) _(SP) _(LR) _(PC)
#if LJ_SOFTFP
#define FPRDEF(_)
#else
#define FPRDEF(_) \
  _(D0) _(D1) _(D2) _(D3) _(D4) _(D5) _(D6) _(D7) \
  _(D8) _(D9) _(D10) _(D11) _(D12) _(D13) _(D14) _(D15)
#endif
#define VRIDDEF(_)

#define RIDENUM(name)	RID_##name,

enum {
  GPRDEF(RIDENUM)		/* General-purpose registers (GPRs). */
  FPRDEF(RIDENUM)		/* Floating-point registers (FPRs). */
  RID_MAX,
  RID_TMP = RID_LR,

  /* Calling conventions. */
  RID_RET = RID_R0,
  RID_RETLO = RID_R0,
  RID_RETHI = RID_R1,
#if LJ_SOFTFP
  RID_FPRET = RID_R0,
#else
  RID_FPRET = RID_D0,
#endif

  /* These definitions must match with the *.dasc file(s): */
  RID_BASE = RID_R9,		/* Interpreter BASE. */
  RID_LPC = RID_R6,		/* Interpreter PC. */
  RID_DISPATCH = RID_R7,	/* Interpreter DISPATCH table. */
  RID_LREG = RID_R8,		/* Interpreter L. */

  /* Register ranges [min, max) and number of registers. */
  RID_MIN_GPR = RID_R0,
  RID_MAX_GPR = RID_PC+1,
  RID_MIN_FPR = RID_MAX_GPR,
#if LJ_SOFTFP
  RID_MAX_FPR = RID_MIN_FPR,
#else
  RID_MAX_FPR = RID_D15+1,
#endif
  RID_NUM_GPR = RID_MAX_GPR - RID_MIN_GPR,
  RID_NUM_FPR = RID_MAX_FPR - RID_MIN_FPR
};

#define RID_NUM_KREF		RID_NUM_GPR
#define RID_MIN_KREF		RID_R0

/* -- Register sets ------------------------------------------------------- */

/* Make use of all registers, except sp, lr and pc. */
#define RSET_GPR		(RSET_RANGE(RID_MIN_GPR, RID_R12+1))
#define RSET_GPREVEN \
  (RID2RSET(RID_R0)|RID2RSET(RID_R2)|RID2RSET(RID_R4)|RID2RSET(RID_R6)| \
   RID2RSET(RID_R8)|RID2RSET(RID_R10))
#define RSET_GPRODD \
  (RID2RSET(RID_R1)|RID2RSET(RID_R3)|RID2RSET(RID_R5)|RID2RSET(RID_R7)| \
   RID2RSET(RID_R9)|RID2RSET(RID_R11))
#if LJ_SOFTFP
#define RSET_FPR		0
#else
#define RSET_FPR		(RSET_RANGE(RID_MIN_FPR, RID_MAX_FPR))
#endif
#define RSET_ALL		(RSET_GPR|RSET_FPR)
#define RSET_INIT		RSET_ALL

/* ABI-specific register sets. lr is an implicit scratch register. */
#define RSET_SCRATCH_GPR_	(RSET_RANGE(RID_R0, RID_R3+1)|RID2RSET(RID_R12))
#ifdef __APPLE__
#define RSET_SCRATCH_GPR	(RSET_SCRATCH_GPR_|RID2RSET(RID_R9))
#else
#define RSET_SCRATCH_GPR	RSET_SCRATCH_GPR_
#endif
#if LJ_SOFTFP
#define RSET_SCRATCH_FPR	0
#else
#define RSET_SCRATCH_FPR	(RSET_RANGE(RID_D0, RID_D7+1))
#endif
#define RSET_SCRATCH		(RSET_SCRATCH_GPR|RSET_SCRATCH_FPR)
#define REGARG_FIRSTGPR		RID_R0
#define REGARG_LASTGPR		RID_R3
#define REGARG_NUMGPR		4
#if LJ_ABI_SOFTFP
#define REGARG_FIRSTFPR		0
#define REGARG_LASTFPR		0
#define REGARG_NUMFPR		0
#else
#define REGARG_FIRSTFPR		RID_D0
#define REGARG_LASTFPR		RID_D7
#define REGARG_NUMFPR		8
#endif

/* -- Spill slots --------------------------------------------------------- */

/* Spill slots are 32 bit wide. An even/odd pair is used for FPRs.
**
** SPS_FIXED: Available fixed spill slots in interpreter frame.
** This definition must match with the *.dasc file(s).
**
** SPS_FIRST: First spill slot for general use. Reserve min. two 32 bit slots.
*/
#define SPS_FIXED	2
#define SPS_FIRST	2

#define SPOFS_TMP	0

#define sps_scale(slot)		(4 * (int32_t)(slot))
#define sps_align(slot)		(((slot) - SPS_FIXED + 1) & ~1)

/* -- Exit state ---------------------------------------------------------- */

/* This definition must match with the *.dasc file(s). */
typedef struct {
#if !LJ_SOFTFP
  lua_Number fpr[RID_NUM_FPR];	/* Floating-point registers. */
#endif
  int32_t gpr[RID_NUM_GPR];	/* General-purpose registers. */
  int32_t spill[256];		/* Spill slots. */
} ExitState;

/* PC after instruction that caused an exit. Used to find the trace number. */
#define EXITSTATE_PCREG		RID_PC
/* Highest exit + 1 indicates stack check. */
#define EXITSTATE_CHECKEXIT	1

#define EXITSTUB_SPACING        4
#define EXITSTUBS_PER_GROUP     32

/* -- Instructions -------------------------------------------------------- */

/* Instruction fields. */
#define ARMF_CC(ai, cc)	(((ai) ^ ARMI_CCAL) | ((cc) << 28))
#define ARMF_N(r)	((r) << 16)
#define ARMF_D(r)	((r) << 12)
#define ARMF_S(r)	((r) << 8)
#define ARMF_M(r)	(r)
#define ARMF_SH(sh, n)	(((sh) << 5) | ((n) << 7))
#define ARMF_RSH(sh, r)	(0x10 | ((sh) << 5) | ARMF_S(r))

typedef enum ARMIns {
  ARMI_CCAL = 0xe0000000,
  ARMI_S = 0x000100000,
  ARMI_K12 = 0x02000000,
  ARMI_KNEG = 0x00200000,
  ARMI_LS_W = 0x00200000,
  ARMI_LS_U = 0x00800000,
  ARMI_LS_P = 0x01000000,
  ARMI_LS_R = 0x02000000,
  ARMI_LSX_I = 0x00400000,

  ARMI_AND = 0xe0000000,
  ARMI_EOR = 0xe0200000,
  ARMI_SUB = 0xe0400000,
  ARMI_RSB = 0xe0600000,
  ARMI_ADD = 0xe0800000,
  ARMI_ADC = 0xe0a00000,
  ARMI_SBC = 0xe0c00000,
  ARMI_RSC = 0xe0e00000,
  ARMI_TST = 0xe1100000,
  ARMI_TEQ = 0xe1300000,
  ARMI_CMP = 0xe1500000,
  ARMI_CMN = 0xe1700000,
  ARMI_ORR = 0xe1800000,
  ARMI_MOV = 0xe1a00000,
  ARMI_BIC = 0xe1c00000,
  ARMI_MVN = 0xe1e00000,

  ARMI_NOP = 0xe1a00000,

  ARMI_MUL = 0xe0000090,
  ARMI_SMULL = 0xe0c00090,

  ARMI_LDR = 0xe4100000,
  ARMI_LDRB = 0xe4500000,
  ARMI_LDRH = 0xe01000b0,
  ARMI_LDRSB = 0xe01000d0,
  ARMI_LDRSH = 0xe01000f0,
  ARMI_LDRD = 0xe00000d0,
  ARMI_STR = 0xe4000000,
  ARMI_STRB = 0xe4400000,
  ARMI_STRH = 0xe00000b0,
  ARMI_STRD = 0xe00000f0,
  ARMI_PUSH = 0xe92d0000,

  ARMI_B = 0xea000000,
  ARMI_BL = 0xeb000000,
  ARMI_BLX = 0xfa000000,
  ARMI_BLXr = 0xe12fff30,

  /* ARMv6 */
  ARMI_REV = 0xe6bf0f30,
  ARMI_SXTB = 0xe6af0070,
  ARMI_SXTH = 0xe6bf0070,
  ARMI_UXTB = 0xe6ef0070,
  ARMI_UXTH = 0xe6ff0070,

  /* ARMv6T2 */
  ARMI_MOVW = 0xe3000000,
  ARMI_MOVT = 0xe3400000,

  /* VFP */
  ARMI_VMOV_D = 0xeeb00b40,
  ARMI_VMOV_S = 0xeeb00a40,
  ARMI_VMOVI_D = 0xeeb00b00,

  ARMI_VMOV_R_S = 0xee100a10,
  ARMI_VMOV_S_R = 0xee000a10,
  ARMI_VMOV_RR_D = 0xec500b10,
  ARMI_VMOV_D_RR = 0xec400b10,

  ARMI_VADD_D = 0xee300b00,
  ARMI_VSUB_D = 0xee300b40,
  ARMI_VMUL_D = 0xee200b00,
  ARMI_VMLA_D = 0xee000b00,
  ARMI_VMLS_D = 0xee000b40,
  ARMI_VNMLS_D = 0xee100b00,
  ARMI_VDIV_D = 0xee800b00,

  ARMI_VABS_D = 0xeeb00bc0,
  ARMI_VNEG_D = 0xeeb10b40,
  ARMI_VSQRT_D = 0xeeb10bc0,

  ARMI_VCMP_D = 0xeeb40b40,
  ARMI_VCMPZ_D = 0xeeb50b40,

  ARMI_VMRS = 0xeef1fa10,

  ARMI_VCVT_S32_F32 = 0xeebd0ac0,
  ARMI_VCVT_S32_F64 = 0xeebd0bc0,
  ARMI_VCVT_U32_F32 = 0xeebc0ac0,
  ARMI_VCVT_U32_F64 = 0xeebc0bc0,
  ARMI_VCVTR_S32_F32 = 0xeebd0a40,
  ARMI_VCVTR_S32_F64 = 0xeebd0b40,
  ARMI_VCVTR_U32_F32 = 0xeebc0a40,
  ARMI_VCVTR_U32_F64 = 0xeebc0b40,
  ARMI_VCVT_F32_S32 = 0xeeb80ac0,
  ARMI_VCVT_F64_S32 = 0xeeb80bc0,
  ARMI_VCVT_F32_U32 = 0xeeb80a40,
  ARMI_VCVT_F64_U32 = 0xeeb80b40,
  ARMI_VCVT_F32_F64 = 0xeeb70bc0,
  ARMI_VCVT_F64_F32 = 0xeeb70ac0,

  ARMI_VLDR_S = 0xed100a00,
  ARMI_VLDR_D = 0xed100b00,
  ARMI_VSTR_S = 0xed000a00,
  ARMI_VSTR_D = 0xed000b00,
} ARMIns;

typedef enum ARMShift {
  ARMSH_LSL, ARMSH_LSR, ARMSH_ASR, ARMSH_ROR
} ARMShift;

/* ARM condition codes. */
typedef enum ARMCC {
  CC_EQ, CC_NE, CC_CS, CC_CC, CC_MI, CC_PL, CC_VS, CC_VC,
  CC_HI, CC_LS, CC_GE, CC_LT, CC_GT, CC_LE, CC_AL,
  CC_HS = CC_CS, CC_LO = CC_CC
} ARMCC;

#endif
