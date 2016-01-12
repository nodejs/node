//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

typedef unsigned char AssemblyStep;

enum AssemblyStepDefs
{
    STEP_DONE = 32,
    STEP_ILLEGAL,
    STEP_REG,
    STEP_REG_PLUS_1,
    STEP_HREG,
    STEP_DREG,
    STEP_SREG,
    STEP_VMOV_IMM,
    STEP_SWPREG,
    STEP_DUMMY_REG,
    STEP_STACKREG,
    STEP_OPEQ,
    STEP_OPEQ2,
    STEP_NEXTOPN,
    STEP_DST2,
    STEP_CONSTANT,
    STEP_FCONSTANT_Z,
    STEP_T1_SETS_CR0,
    STEP_SHIFTER,
    STEP_DATA,
    STEP_LDRI,
    STEP_LDR,
    STEP_STRI,
    STEP_STR,
    STEP_OPCODE,
    STEP_HBIT,
    STEP_LABEL,
    STEP_CALL,
    STEP_CONDCOD,
    STEP_SBIT,
    STEP_AM1,
    STEP_IMM32,
    STEP_IBIT,
    STEP_IMM5_AM1,
    STEP_IMM5,
    STEP_AM,
    STEP_AM2,
    STEP_INDEXED,
    STEP_BASED,
    STEP_BASEREG,
    STEP_INDEXREG,
    STEP_STACKIMM,
    STEP_IMM12_AM2,
    STEP_AM3,
    STEP_UIMM8_AM3,
    STEP_UIMM16HS,
    STEP_AM4,
    STEP_SBZ,
    STEP_LXN,
    STEP_L24,
    STEP_INDIR,
    STEP_SYM,
    STEP_IMM_W5,
    STEP_IMM,
    STEP_IMM_H5,
    STEP_IMM_W7,
    STEP_IMM_W8,
    STEP_IMM_S8,
    STEP_IMM_DPW8,
    STEP_UIMM32,
    STEP_UIMM8,
    STEP_UIMM16,
    STEP_UIMM24,
    STEP_UIMM5,
    STEP_UIMM_WB5,
    STEP_UIMM3,
    STEP_OFFSET,
    STEP_REGLIST,
    STEP_SHIFTER_TYPE,
    STEP_SHIFTER_CONST,
    STEP_SCALE_CONST,
    STEP_FIXUP,
    STEP_LOFFSET1,
    STEP_LOFFSET2,
    STEP_VALUE,
    STEP_MULL,
    STEP_MRS,
    STEP_NAN,
    STEP_AM5,
    STEP_ZBIT,
    STEP_ACC,
    STEP_OPCODE3,
    STEP_WORD,
    STEP_COPROC,
    STEP_EXTR,
    STEP_IMM8,
    STEP_WLDRI,
    STEP_WSTRI,
    STEP_MASK,
    STEP_UIMM4,
    STEP_BIT,
    STEP_MODCONST_12,
    STEP_T2_IMM_16,
    STEP_T2_IMM_12,
    STEP_T2_SHIFT_IMM_5,
    STEP_T2_IMMSTACK_POS12_NEG8,
    STEP_NOT_CONSTANT,
    STEP_T2_CONST_SHIFT_TYPE,
    STEP_T2_CONST_SHIFT_NUM,
    STEP_T2_CONST_ROR,
    STEP_T2_CONST_ROR_NUM,
    STEP_T2_LSB,
    STEP_T2_xBFX_WIDTH,
    STEP_T2_BFx_WIDTH,
    STEP_T2_MEMIMM_POS12_NEG8,
    STEP_T2_REGLIST,
    STEP_T2_MEM_TYPE,
    STEP_T2_BRANCH20,
    STEP_T2_BRANCH24,
    STEP_T2_LXZ,
    STEP_T2_CBZ,
    STEP_T2_IT,
    STEP_MOVW_reloc,
    STEP_MOVT_reloc,
    STEP_Shift_IMM5,
    STEP_XT_CONST_ROR,
    STEP_XT_CONST_ROR_NUM,
    STEP_DREGLIST,
    STEP_NREG,
    STEP_QREG,
    STEP_FAKEQREG,
    STEP_IMM_OPCODE,
    STEP_NEON_AM5,
    STEP_UIMM16V7,
    STEP_A7_LSB,
    STEP_A7_xBFX_WIDTH,
    STEP_A7_BFx_WIDTH,
    STEP_NOSBIT,
    STEP_T2_INDIRSETUP_PC_OFF,
    STEP_AM_D_T,
    STEP_AM_D_T_STACK,
    STEP_T2_STACKSYM_IMM_12,
    STEP_R12,
};

// used by : ADD, SUB
static const AssemblyStep Steps_T_Add_Sub_dnc [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 3, STEP_NEXTOPN, STEP_CONSTANT, STEP_T1_SETS_CR0,
   STEP_UIMM3, 6, STEP_OPCODE, STEP_DONE
};

// used by : ADD, SUB
static const AssemblyStep Steps_T_Add_Sub_ddc [] =
{
   STEP_REG, 8, STEP_NEXTOPN, STEP_OPEQ, STEP_NEXTOPN, STEP_CONSTANT, STEP_T1_SETS_CR0,
   STEP_UIMM8, STEP_OPCODE, STEP_DONE
};

// used by : ADD, SUB
static const AssemblyStep Steps_T_Add_Sub_dnm [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 3, STEP_NEXTOPN, STEP_REG, 6,
   STEP_OPCODE, STEP_DONE
};

// used by : ADD
static const AssemblyStep Steps_T_Add_High_dm [] =
{
   STEP_HREG, 0, STEP_HBIT, 7, STEP_NEXTOPN, STEP_OPEQ, STEP_NEXTOPN,
   STEP_HREG, 3, STEP_HBIT, 6, STEP_OPCODE, STEP_NOSBIT, STEP_DONE
};

// used by : ADD
static const AssemblyStep Steps_T_Add_SP_or_PC [] =
{
   STEP_REG, 8, STEP_NEXTOPN, STEP_DUMMY_REG, STEP_NEXTOPN, STEP_CONSTANT,
   STEP_IMM_DPW8, STEP_OPCODE, STEP_DONE
};


// used by : ADD, SUB
static const AssemblyStep Steps_T_Add_Sub_SP [] =
{
   STEP_DUMMY_REG, STEP_NEXTOPN, STEP_DUMMY_REG, STEP_NEXTOPN, STEP_CONSTANT,
   STEP_IMM_W7, STEP_OPCODE, STEP_DONE
};

// Thumb2 : used by some ALU instructions (ADD, ADC, ...)
// ALU  rd, rd, rm
static const AssemblyStep Steps_T2_ALU_ddm [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_NEXTOPN, STEP_REG, 16,
   STEP_NEXTOPN, STEP_SBIT, 4,
   STEP_OPCODE, STEP_DONE
};

// Thumb2 : used by ADDW
// ALU  rd, rn, immediate12bits
static const AssemblyStep Steps_T2_ALU_dn_imm12 [] =
{
    STEP_REG, 24, STEP_NEXTOPN, STEP_REG, 0, STEP_NEXTOPN,
    STEP_CONSTANT, STEP_T2_IMM_12, STEP_OPCODE, STEP_DONE
};

// Thumb2: used by : B (unconditional)  <label>
static const AssemblyStep Steps_T2_BranchUncond [] =
{
   STEP_T2_BRANCH24, STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_T2_BranchCond [] =
{
   STEP_T2_BRANCH20, STEP_OPCODE, STEP_DONE
};

// Thumb2: used by : BL
static const AssemblyStep Steps_T2_BL [] =
{
   STEP_CALL, STEP_OPCODE, STEP_DONE
};

// used by : BL
static const AssemblyStep Steps_T_BL [] =
{
   STEP_CALL, STEP_OPCODE, STEP_DONE
};

// used by : BLX
static const AssemblyStep Steps_T_BLX [] =
{
    STEP_LABEL, STEP_OPCODE, STEP_DONE
};

// Thumb2: used by : BLX
static const AssemblyStep Steps_T2_BLX [] =
{
    STEP_CALL, STEP_OPCODE, STEP_DONE
};

// used by : BX
static const AssemblyStep Steps_T_BX [] =
{
   STEP_REG, 3, STEP_OPCODE, STEP_DONE
};

// used by : AND, MUL, BIC, ADC, SUB, SBC
static const AssemblyStep Steps_T_ALU_dm [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_OPEQ, STEP_NEXTOPN, STEP_REG, 3, STEP_T1_SETS_CR0,
   STEP_OPCODE, STEP_DONE
};

// used by : CMP

// used by : CMP(1),
static const AssemblyStep Steps_T_RR_dc [] =
{
   STEP_REG, 8, STEP_NEXTOPN, STEP_CONSTANT,
   STEP_UIMM8, STEP_OPCODE, STEP_DONE
};

// used by : CMP(3),
static const AssemblyStep Steps_T_HRR_ds [] =
{
   STEP_HREG, 0, STEP_HBIT, 7, STEP_NEXTOPN,
   STEP_HREG, 3, STEP_HBIT, 6, STEP_OPCODE, STEP_DONE
};


static const AssemblyStep Steps_T2_n_modc12 [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_CONSTANT,
   STEP_MODCONST_12, STEP_OPCODE, STEP_DONE
};

// used by : LDM
static const AssemblyStep Steps_T_LDM_rspx [] =
{
   STEP_INDIR, STEP_REGLIST, STEP_OFFSET, STEP_OPCODE, STEP_DONE
};

// used by : LDM
static const AssemblyStep Steps_T_LDM_rrx [] =
{
   STEP_INDIR, STEP_REGLIST, STEP_UIMM8, STEP_BASEREG, 8, STEP_OPCODE,
   STEP_DONE
};

// thumb2: used by : LDM
static const AssemblyStep Steps_T2_LDM [] =
{
   STEP_INDIR, STEP_REGLIST, STEP_T2_REGLIST, STEP_BASEREG, 0, STEP_OPCODE, STEP_DONE
};

// thumb2: used by : LDM
static const AssemblyStep Steps_T2_LDM_ONE [] =
{
   STEP_INDIR, STEP_REGLIST, 28, STEP_OPCODE, STEP_DONE
};

// thumb2: used by : STM
static const AssemblyStep Steps_T2_STM_rsp_ONE [] =
{
   STEP_REGLIST, 28, STEP_OPCODE, STEP_DONE
};


// used by : LDRRET (Thumb2)
static const AssemblyStep Steps_T2_LDRRET [] =
{
   STEP_NEXTOPN, STEP_BASED,
   STEP_T2_MEMIMM_POS12_NEG8, 16,
   STEP_OPCODE, STEP_DONE
};

// Thumb2 : MLS Rd, Rn, Rm, Ra -> dst, R12, src2, src1.
static const AssemblyStep Steps_T2_mls_dnma [] =
{
   // dst (Rd)    R12 (Rn)                  src2 (Rm)                   src1 (Ra)
   STEP_REG, 24, STEP_R12, 0, STEP_NEXTOPN, STEP_REG, 16, STEP_NEXTOPN, STEP_REG, 28,
   STEP_OPCODE, STEP_DONE
};

// used by : MULA
static const AssemblyStep Steps_T2_MULA_dnsm [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 16,
   STEP_NEXTOPN, STEP_REG, 28, STEP_OPCODE, STEP_DONE
};

// Thumb2 :  MUL  rd, rn, rm
static const AssemblyStep Steps_T2_ALU_dnm_no_sbit [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 16, STEP_NOSBIT,
   STEP_OPCODE, STEP_DONE
};

// Thumb2 :  SMULL/UMULL RdLo, RdHi, Rn, Rm -> dst, r12, src1, src1
static const AssemblyStep Steps_T2_ALU_mull_no_sbit [] =
{
   // RdLo RdHi 0000 Rm__ | 1111 1011 1000 Rn__
   // dst (RdLo)               src1 (Rn)                  src2 (Rm)
   STEP_REG, 28, STEP_NEXTOPN, STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 16, STEP_R12, 24,
   STEP_NOSBIT, STEP_OPCODE, STEP_DONE
};

// Thumb2 : used by some ALU instructions (MVN, ...)
// ALU  rd, rn ,modified constant 12 bit
static const AssemblyStep Steps_T2_ALU_dm_Shift_c [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_REG, 16, STEP_NEXTOPN, STEP_T2_CONST_SHIFT_TYPE, 20, STEP_NEXTOPN,
   STEP_CONSTANT, STEP_T2_SHIFT_IMM_5, STEP_SBIT, 4, STEP_OPCODE, STEP_DONE
};

// used by : NEG, MVN
static const AssemblyStep Steps_T_Neg_Mvn_dm [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 3, STEP_T1_SETS_CR0, STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_T_Ret_rr [] =
{
   STEP_HREG, 0, STEP_HBIT, 7, STEP_NEXTOPN, STEP_HREG, 3, STEP_HBIT, 6,
   STEP_OPCODE, STEP_DONE
};


// used by : RET
static const AssemblyStep Steps_T_IWRET_dm [] =
{
   STEP_NEXTOPN, STEP_REG, 3, STEP_OPCODE, STEP_DONE
};


// used by : MOV,
static const AssemblyStep Steps_T_ALU_rc [] =
{
   STEP_REG, 8, STEP_NEXTOPN, STEP_CONSTANT, STEP_UIMM8, STEP_T1_SETS_CR0,
   STEP_OPCODE, STEP_DONE
};

// used by : MOV
static const AssemblyStep Steps_T_MovHigh_rr [] =
{
   STEP_HREG, 0, STEP_HBIT, 7, STEP_NEXTOPN, STEP_HREG, 3, STEP_HBIT, 6,
   STEP_OPCODE, STEP_DONE
};

// used by : STM
static const AssemblyStep Steps_T_STM_rspx [] =
{
   STEP_INDIR, STEP_REGLIST, STEP_OFFSET, STEP_OPCODE, STEP_DONE
};

// used by : STM
static const AssemblyStep Steps_T_STM_rrx [] =
{
   STEP_INDIR, STEP_REGLIST, STEP_UIMM8, STEP_BASEREG, 8, STEP_OPCODE, STEP_DONE
};

// used by : STM
static const AssemblyStep Steps_T2_STM_rrx [] =
{
   STEP_INDIR, STEP_REGLIST, STEP_T2_REGLIST, STEP_BASEREG, 0, STEP_OPCODE, STEP_DONE
};

// used by : LDR
static const AssemblyStep Steps_T_LDRN_rcr [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_BASED, STEP_BASEREG, 3,
   STEP_IMM, 6, STEP_FIXUP, STEP_LDRI, STEP_OPCODE, STEP_DONE
};

// used by : LDR
static const AssemblyStep Steps_T_LDRN_rrr [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_INDEXED, STEP_BASEREG, 3, STEP_INDEXREG, 6,
   STEP_LDR, STEP_OPCODE, STEP_DONE
};

// used by : LDR
static const AssemblyStep Steps_T_LDRN_PC_or_SP [] =
{
   STEP_REG, 8, STEP_NEXTOPN, STEP_INDIR, STEP_IMM_W8, STEP_OPCODE,
   STEP_DONE
};

// used by : LDR
static const AssemblyStep Steps_T_LDRN_ri [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_INDIR, STEP_IMM, 6, STEP_OPCODE,
   STEP_DONE
};

// Thumb2        LDR rxf, [rd, +#immediate12]
// Thumb2        LDR rxf, [rd, -#immediate8]
static const AssemblyStep Steps_T2_LDR_OFF [] =
{
   STEP_REG, 28, STEP_NEXTOPN, STEP_BASED, STEP_BASEREG, 0,
   STEP_T2_MEMIMM_POS12_NEG8, 16,
   STEP_T2_MEM_TYPE, STEP_OPCODE, STEP_DONE
};

// Thumb2        LDR rxf, symbol
static const AssemblyStep Steps_T2_LDR_Stack [] =
{
   STEP_REG, 28, STEP_NEXTOPN, STEP_INDIR,
   STEP_T2_IMMSTACK_POS12_NEG8, 16, STEP_T2_MEM_TYPE,
   STEP_OPCODE, STEP_DONE
};

// Thumb2        LDR rxf, [rn, rm {LSL #<shift>}]
static const AssemblyStep Steps_T2_LDR_RegIndir [] =
{
   STEP_REG, 28, STEP_NEXTOPN, STEP_INDEXED, STEP_BASEREG, 0,
   STEP_INDEXREG, 16, STEP_T2_MEM_TYPE, STEP_SCALE_CONST, 20, STEP_OPCODE, STEP_DONE
};

// Thumb2 LEA
static const AssemblyStep Steps_T2_LEA_rrd [] =
{
    STEP_REG, 24, STEP_NEXTOPN, STEP_T2_STACKSYM_IMM_12, 0, STEP_OPCODE, STEP_DONE
};

// used by : LEA
static const AssemblyStep Steps_T_LEA_rrd [] =
{
   STEP_REG, 8, STEP_NEXTOPN, STEP_SYM, STEP_IMM_W8, STEP_OPCODE, STEP_DONE
};

// used by : LEA
static const AssemblyStep Steps_T_LEA_rspd [] =
{
   STEP_REG, 8, STEP_NEXTOPN, STEP_SYM, STEP_IMM_W8, STEP_OPCODE, STEP_DONE
};

// used by : MOV
static const AssemblyStep Steps_T_ALU_dn [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 3,
   STEP_OPCODE, STEP_DONE
};

// used by : STR
static const AssemblyStep Steps_T_STRN_rcr [] =
{
   STEP_BASED, STEP_BASEREG, 3, STEP_IMM, 6, STEP_STRI, STEP_NEXTOPN, STEP_REG, 0,
   STEP_OPCODE, STEP_DONE
};

// used by : STR
static const AssemblyStep Steps_T_STRN_rcr_custom [] =
{
   STEP_BASEREG, 3, STEP_IMM, 6, STEP_STRI, STEP_NEXTOPN, STEP_REG, 0,
   STEP_OPCODE, STEP_DONE
};

// used by : STRWI
static const AssemblyStep Steps_T_STRN_rrr [] =
{
   STEP_INDEXED, STEP_BASEREG, 3, STEP_INDEXREG, 6, STEP_NEXTOPN, STEP_REG,
   0, STEP_STR, STEP_OPCODE, STEP_DONE
};

// used by : STRWI
static const AssemblyStep Steps_T_STRN_ri [] =
{
   STEP_INDIR, STEP_IMM, 6, STEP_NEXTOPN, STEP_REG, 0, STEP_OPCODE,
   STEP_DONE
};

// used by : STR (stack)
static const AssemblyStep Steps_T_STRN_spcr [] =
{
   STEP_INDIR, STEP_IMM_W8, STEP_NEXTOPN, STEP_REG, 8, STEP_OPCODE, STEP_DONE
};

// Thumb2        STR rxf, [rd, +#immediate12]
// Thumb2        STR rxf, [rd, -#immediate8]
static const AssemblyStep Steps_T2_STR_OFF [] =
{
   STEP_BASED, STEP_BASEREG, 0,
   STEP_T2_MEMIMM_POS12_NEG8, 16, STEP_T2_MEM_TYPE,
   STEP_NEXTOPN, STEP_REG, 28,
   STEP_OPCODE, STEP_DONE
};

// Thumb2        STR rxf, [sp, #offset]
static const AssemblyStep Steps_T2_STR_Stack [] =
{
    STEP_INDIR, STEP_T2_IMMSTACK_POS12_NEG8, 16, STEP_T2_MEM_TYPE,
    STEP_NEXTOPN, STEP_REG, 28,
    STEP_OPCODE, STEP_DONE
};

// Thumb2        STR rxf, [rn, rm, LSL #<shift>]
static const AssemblyStep Steps_T2_STR_RegIndir [] =
{
   STEP_INDEXED, STEP_BASEREG, 0, STEP_INDEXREG, 16,
   STEP_SCALE_CONST, 20, STEP_T2_MEM_TYPE, STEP_NEXTOPN, STEP_REG, 28,
   STEP_OPCODE, STEP_DONE
};

// Thumb2 : used by : MOVW,
static const AssemblyStep Steps_T2_ALU_r_imm16 [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_CONSTANT, STEP_T2_IMM_16,
   STEP_OPCODE, STEP_DONE
};

// Thumb2 : used by : MOV_W, ...
static const AssemblyStep Steps_T2_ALU_r_modc12 [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_CONSTANT, STEP_MODCONST_12,
   STEP_OPCODE, STEP_DONE
};

// Thumb2 : used MVN
// ALU  rd, rm
static const AssemblyStep Steps_T2_ALU_dm [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_REG, 16, STEP_SBIT, 4,
   STEP_OPCODE, STEP_DONE
};

// Thumb2 : used by some ALU instructions (ADD, ADC, ...)
// ALU  rd, rn ,modified constant 12 bit
static const AssemblyStep Steps_T2_ALU_dn_modc12 [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_REG, 0, STEP_NEXTOPN, STEP_CONSTANT,
   STEP_MODCONST_12, STEP_SBIT, 4, STEP_OPCODE, STEP_DONE
};

// used by : ASR
static const AssemblyStep Steps_T_Shift_ds [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_OPEQ, STEP_NEXTOPN, STEP_REG, 3, STEP_T1_SETS_CR0,
   STEP_OPCODE, STEP_DONE
};

// used by : ASR
static const AssemblyStep Steps_T_Shift_dmc [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 3, STEP_NEXTOPN, STEP_CONSTANT, STEP_T1_SETS_CR0,
   STEP_UIMM5, 6, STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_T2_Shift_dmc [] =
{
    STEP_REG, 24, STEP_NEXTOPN, STEP_REG, 16, STEP_NEXTOPN,
    STEP_CONSTANT, STEP_T2_SHIFT_IMM_5, STEP_SBIT, 4, STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_T2_ALU_dnm [] =
{
    STEP_REG, 24, STEP_NEXTOPN, STEP_REG, 0, STEP_NEXTOPN,
    STEP_REG, 16, STEP_SBIT, 4,
    STEP_OPCODE, STEP_DONE
};

// Thumbs :  TEQ rn, rn LSL#1 (aka, TIOFLW)
static const AssemblyStep Steps_T2_ALU_nn [] =
{
   STEP_REG, 0, STEP_REG, 16, STEP_OPCODE, STEP_DONE
};

// Thumb2 :  TST  rn, rm
static const AssemblyStep Steps_T2_ALU_nm [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 16,
   STEP_OPCODE, STEP_DONE
};


// Thumb2 : used by some ALU instructions (CMN, CMP, ...)
// ALU  rd, rn, constant shift
static const AssemblyStep Steps_T2_ALU_nm_Shift_c [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 16, STEP_NEXTOPN,
   STEP_T2_CONST_SHIFT_TYPE, 20, STEP_NEXTOPN, STEP_CONSTANT, STEP_T2_SHIFT_IMM_5,
   STEP_OPCODE, STEP_DONE
};

// Thumb2 : used by some ALU instructions (ADD, ADC, ...)
// ALU  rd, rn, rm, constant shift
static const AssemblyStep Steps_T2_ALU_dnm_Shift_c [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 16,
   STEP_NEXTOPN, STEP_T2_CONST_SHIFT_TYPE, 20, STEP_NEXTOPN, STEP_CONSTANT, STEP_T2_SHIFT_IMM_5,
   STEP_SBIT, 4,
   STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_T2_ALU_dnn [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_REG, 0, STEP_REG, 16,
   STEP_OPCODE, STEP_DONE
};

// Thumb2 : used by : MOVW
static const AssemblyStep Steps_T2_MOVW_reloc [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_MOVW_reloc,
   STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_T2_MOVT_reloc [] =
{
   STEP_REG, 24, STEP_NEXTOPN, STEP_MOVT_reloc,
   STEP_OPCODE, STEP_DONE
};

// used by : TST, CMP, CMN
static const AssemblyStep Steps_T_RR_ds [] =
{
   STEP_REG, 0, STEP_NEXTOPN, STEP_REG, 3,
   STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_T_Dbg [] =
{
   STEP_OPCODE, STEP_DONE,
};

static const AssemblyStep Steps_NOP [] =
{
   STEP_OPCODE, STEP_DONE,
};

static const AssemblyStep Steps_DBL_Unary_dm [] =
{
    STEP_DREG, 28, 6, STEP_NEXTOPN,
    STEP_DREG, 16, 21,
    STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_DBL_ALU_dnm [] =
{
   STEP_DREG, 28, 6, STEP_NEXTOPN,
   STEP_DREG, 0, 23, STEP_NEXTOPN,
   STEP_DREG, 16, 21,
   STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_DBL_Cmp_dm [] =
{
    STEP_DREG, 28, 6, STEP_NEXTOPN,
    STEP_DREG, 16, 21,
    STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_FLT_FMRS_flags [] =
{
    STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_FLT_FMRSR_flags [] =
{
    STEP_REG, 28,
    STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_A_LDRN_DBL_Am_rcr [] =
{
    // Dn (dest of load)
    //
    STEP_DREG, 28, 6, STEP_NEXTOPN,

    // [Rn + #offset]
    //
    STEP_BASED, STEP_IMM_S8, STEP_BASEREG, 0,

    STEP_FIXUP, STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_A_LDRN_FLT_Am_rcr [] =
{
    // Sn (dest of load)
    //
    STEP_SREG, 28, 06, STEP_NEXTOPN,

    // [Rn + #offset]
    //
    STEP_BASED, STEP_IMM_S8, STEP_BASEREG, 0,

    STEP_FIXUP, STEP_OPCODE,  STEP_DONE
};


static const AssemblyStep Steps_A_DBL_STRN_Am_rcr [] =
{
    // [Rn + #offset]
    //
    STEP_BASED, STEP_IMM_S8, STEP_BASEREG, 0, STEP_NEXTOPN,

    // Dn (src for store)
    //
    STEP_DREG, 28, 6,

    STEP_FIXUP, STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_A_FLT_STRN_Am_rcr [] =
{
    // [Rn + #offset]
    //
    STEP_BASED, STEP_IMM_S8, STEP_BASEREG, 0, STEP_NEXTOPN,

    // Sn (src for store)
    //
    STEP_SREG, 28, 6,

    STEP_FIXUP, STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_FCVTDS_ds [] =
{
    STEP_DREG, 28, 6, STEP_NEXTOPN,
    STEP_SREG, 16, 21,
    STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_FCVTSD_sd [] =
{
    STEP_SREG, 28, 6, STEP_NEXTOPN,
    STEP_DREG, 16, 21,
    STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_FITOD_ds [] =
{
    STEP_DREG, 28, 6, STEP_NEXTOPN,
    STEP_SREG, 16, 21,
    STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_FDTOI_sd [] =
{
    STEP_SREG, 28, 6, STEP_NEXTOPN,
    STEP_DREG, 16, 21,
    STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_FLT_FMSR_sr [] =
{
    STEP_SREG, 0, 23, STEP_NEXTOPN,
    STEP_REG, 28,
    STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_FLT_FMRS_rs [] =
{
    STEP_REG, 28, STEP_NEXTOPN,
    STEP_SREG, 0, 23,
    STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_T2_PLD_offset [] =
{
   STEP_BASED, STEP_BASEREG, 0,
   STEP_T2_MEMIMM_POS12_NEG8, 16,
   STEP_OPCODE, STEP_DONE
};

// Thumb2        PLD_W [rn, rm]
static const AssemblyStep Steps_T2_PLD_RegIndir [] =
{
   STEP_INDEXED, STEP_BASEREG, 0,
   STEP_INDEXREG, 16,
   STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_A_DBL_LDM[] =
{
    STEP_BASED, STEP_AM5, STEP_BASEREG, 0, STEP_DREGLIST,
    STEP_FIXUP, STEP_OPCODE, STEP_DONE
};

static const AssemblyStep Steps_A_DBL_STM[] =
{
    STEP_BASED, STEP_AM5, STEP_BASEREG, 0, STEP_DREGLIST,
    STEP_FIXUP, STEP_OPCODE, STEP_DONE
};
