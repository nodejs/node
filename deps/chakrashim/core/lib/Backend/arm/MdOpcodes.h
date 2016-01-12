//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// Instruction encoding macros.  Used by ARM instruction encoder.
//
#define INSTR_TYPE(x) (x)

//            opcode
//           /          layout
//          /          /         attrib             byte2
//         /          /         /                   /         form
//        /          /         /                   /         /          opbyte
//       /          /         /                   /         /          /
//      /          /         /                   /         /          /                  dope
//     /          /         /                   /         /          /                  /

MACRO(ADD,     Reg3,       0,              0,  LEGAL_ADDSUB,   INSTR_TYPE(Forms_ADD), D___C)
MACRO(ADDS,    Reg3,       OpSideEffect,   0,  LEGAL_ALU3,     INSTR_TYPE(Forms_ADD),  D__SC)
MACRO(ADDW,    Reg3,       0,              0,  LEGAL_ADDSUBW,  INSTR_TYPE(Forms_ADDW), D___C)
MACRO(AND,     Reg3,       0,              0,  LEGAL_ALU3,     INSTR_TYPE(Forms_AND),  D___C)
MACRO(ASR,     Reg3,       0,              0,  LEGAL_SHIFT,    INSTR_TYPE(Forms_ASR),  D___C)
MACRO(ASRS,    Reg3,       OpSideEffect,   0,  LEGAL_SHIFT,    INSTR_TYPE(Forms_ASR),  D__SC)

MACRO(B,       Br,         OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_B),    DQ__C)
MACRO(BX,      Br,         OpSideEffect,   0,  LEGAL_BREG,     INSTR_TYPE(Forms_BX),   D___C)
MACRO(BL,      CallI,      OpSideEffect,   0,  LEGAL_CALL,     INSTR_TYPE(Forms_BL),   D___C)
MACRO(BLX,     CallI,      OpSideEffect,   0,  LEGAL_CALLREG,  INSTR_TYPE(Forms_BLX),  D___C)

MACRO(BIC,     Reg3,       OpSideEffect,   0,  LEGAL_ALU3,     INSTR_TYPE(Forms_BIC),  D___C)

// B<C>: explicit and resolved in encoder
MACRO(BEQ,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BEQ),    DQ__C)
MACRO(BNE,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BNE),    DQ__C)
MACRO(BLT,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BLT),    DQ__C)
MACRO(BLE,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BLE),    DQ__C)
MACRO(BGT,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BGT),    DQ__C)
MACRO(BGE,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BGE),    DQ__C)

MACRO(BCS,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BCS),    DQ__C)
MACRO(BCC,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BCC),    DQ__C)
MACRO(BHI,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BHI),    DQ__C)
MACRO(BLS,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BLS),    DQ__C)
MACRO(BMI,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BMI),    DQ__C)
MACRO(BPL,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BPL),    DQ__C)
MACRO(BVS,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BVS),    DQ__C)
MACRO(BVC,     BrReg2,     OpSideEffect,   0,  LEGAL_BLAB,     INSTR_TYPE(Forms_BVC),    DQ__C)
MACRO(DEBUGBREAK,  Reg1,   OpSideEffect,   0,  LEGAL_NONE,     INSTR_TYPE(Forms_DBGBRK), D____)

MACRO(CLZ,     Reg2,       0,              0,  LEGAL_REG2,     INSTR_TYPE(Forms_CLZ),  D___C)
MACRO(CMP,     Reg1,       OpSideEffect,   0,  LEGAL_CMP,      INSTR_TYPE(Forms_CMP),  D___C)
MACRO(CMN,     Reg1,       OpSideEffect,   0,  LEGAL_CMN,      INSTR_TYPE(Forms_CMN),  D___C)
// CMP src1, src, ASR #31
MACRO(CMP_ASR31,Reg1,      OpSideEffect,   0,  LEGAL_CMP_SH,   INSTR_TYPE(Forms_CMP_ASR31),D___C)

MACRO(EOR,     Reg3,       0,              0,  LEGAL_ALU3,     INSTR_TYPE(Forms_EOR),  D___C)
MACRO(EOR_ASR31,Reg3,      0,              0,  LEGAL_ALU3,     INSTR_TYPE(Forms_EOR_ASR31),D___C)

// LDIMM: Load Immediate
MACRO(LDIMM,   Reg2,       0,              0,  LEGAL_LDIMM,    INSTR_TYPE(Forms_LDIMM), DM__C)

// Forms of LDM/STM that update the base reg (i.e., associate the update/sub/etc. with the opcode rather than indir)
MACRO(PUSH,    Reg2,       OpSideEffect,   0,  LEGAL_PUSHPOP,  INSTR_TYPE(Forms_STM), DSUS_C)
MACRO(POP,     Reg2,       OpSideEffect,   0,  LEGAL_PUSHPOP,  INSTR_TYPE(Forms_LDM), DLUP_C)

// LDR: Load register from memory
MACRO(LDR,     Reg2,       0,              0,  LEGAL_LOAD,     INSTR_TYPE(Forms_LDRN), DL__C)
// Wide LDR encoding (Thumb2)
MACRO(LDR_W,   Reg2,       0,              0,  LEGAL_LOAD,     INSTR_TYPE(Forms_LDR_W),DL__C)

// LDRRET: pseudo-op for "ldr pc, [sp], #imm"
MACRO(LDRRET,  Reg2,       OpSideEffect,   0,  LEGAL_LOAD,     INSTR_TYPE(Forms_LDRRET), DLUPQC)

// LEA: Load Effective Address
MACRO(LEA,     Reg3,       0,              0,  LEGAL_LEA,      INSTR_TYPE(Forms_LEA),  D___C)

// Shifter operands
MACRO(LSL,     Reg2,       0,              0,  LEGAL_SHIFT,    INSTR_TYPE(Forms_LSL), D___C)
MACRO(LSR,     Reg2,       0,              0,  LEGAL_SHIFT,    INSTR_TYPE(Forms_LSR), D___C)

// MOV(P): Pseudo-op which can expands into ADD r2, r1, #0 (for MOVL)
MACRO(MOV_W,   Reg2,       0,              0,  LEGAL_ALU2,     INSTR_TYPE(Forms_MOV_W),DM__C)

MACRO(MOV,     Reg2,       0,              0,  LEGAL_ALU2,     INSTR_TYPE(Forms_MOV), DM__C)

MACRO(MOVT,    Reg2,       0,              0,  LEGAL_MOVIMM16, INSTR_TYPE(Forms_MOVT), DM__C)
MACRO(MOVW,    Reg2,       0,              0,  LEGAL_MOVIMM16, INSTR_TYPE(Forms_MOVW), DM__C)

MACRO(MUL,     Reg3,       0,              0,  LEGAL_REG3,     INSTR_TYPE(Forms_MUL),  D___C)

// SMULL dst, r12, src1, src2.
MACRO(SMULL,     Reg3,       0,            0,  LEGAL_REG3,     INSTR_TYPE(Forms_SMULL),D___C)

// SMLAL dst, r12, src1, src2.  The add source comes from r12:s1. Dst is 64 bit and is in r12:s1.
MACRO(SMLAL,     Reg3,       0,            0,  LEGAL_REG3,     INSTR_TYPE(Forms_SMLAL),D___C)

// MLS dst, src1, src2: Multiply and Subtract. We use 3 registers: dst = src1 - src2 * dst
MACRO(MLS,     Reg3,       0,              0,  LEGAL_REG3,     INSTR_TYPE(Forms_MLS),  D___C)

// MVN: Move NOT (register); Format 4
MACRO(MVN,     Reg2,       0,              0,  LEGAL_ALU2,     INSTR_TYPE(Forms_MVN),  D___C)

// NOP: No-op
MACRO(NOP,     Empty,      0,              0,  LEGAL_NONE,     INSTR_TYPE(Forms_NOP),   D___C)
MACRO(NOP_W,   Empty,      0,              0,  LEGAL_NONE,     INSTR_TYPE(Forms_NOP_W), D___C)

MACRO(ORR,     Reg3,       0,              0,  LEGAL_ALU3,     INSTR_TYPE(Forms_ORR),  D___C)

MACRO(PLD,     Reg2,       0,              0,  LEGAL_LOAD,     INSTR_TYPE(Forms_PLD),  DL___)

// RET: pseudo-op for function return
MACRO(RET,     Reg2,       OpSideEffect,   0,  LEGAL_REG2,     INSTR_TYPE(Forms_RET),  D___C)

// REM: pseudo-op
MACRO(REM,     Reg3,       OpSideEffect,   0,  LEGAL_REG3,     INSTR_TYPE(Forms_REM),  D___C)

// RSB: reserve subtract (i.e., NEG)
MACRO(RSB,     Reg2,       0,              0,  LEGAL_ALU3,     INSTR_TYPE(Forms_RSB),  D___C)
MACRO(RSBS,    Reg2,       OpSideEffect,   0,  LEGAL_ALU3,     INSTR_TYPE(Forms_RSB),  D__SC)

// SDIV: Signed divide
MACRO(SDIV,    Reg3,       0,              0,  LEGAL_REG3,     INSTR_TYPE(Forms_SDIV),  D___C)

// STR: Store register to memory
MACRO(STR,     Reg2,       0,              0,  LEGAL_STORE,    INSTR_TYPE(Forms_STRN), DS__C)
// Wide STR encoding (Thumb2)
MACRO(STR_W,   Reg2,       0,              0,  LEGAL_STORE,    INSTR_TYPE(Forms_STR_W),DS__C)

MACRO(SUB,     Reg3,       0,              0,  LEGAL_ADDSUB,   INSTR_TYPE(Forms_SUB), D___C)
MACRO(SUBS,    Reg3,       OpSideEffect,   0,  LEGAL_ALU3,     INSTR_TYPE(Forms_SUB), D__SC)
MACRO(SUBW,    Reg2,       0,              0,  LEGAL_ADDSUBW,  INSTR_TYPE(Forms_SUBW), D___C)

MACRO(TIOFLW,  Reg1,       OpSideEffect,   0,  LEGAL_CMP1,     INSTR_TYPE(Forms_TIOFLW), D___C)
MACRO(TST,     Reg2,       OpSideEffect,   0,  LEGAL_CMP,      INSTR_TYPE(Forms_TST),  D___C)

// Pseudo-op that loads the size of the arg out area. A special op with no src is used so that the
// actual arg out size can be fixed up by the encoder.
MACRO(LDARGOUTSZ,Reg1,     0,              0,  LEGAL_REG1,     INSTR_TYPE(Forms_LDIMM), D___C)

// Pseudo-op: dst = EOR src, src ASR #31
MACRO(CLRSIGN, Reg2,       0,              0,  LEGAL_REG2,     INSTR_TYPE(Forms_CLRSIGN), D___C)

// Pseudo-op: dst = SUB src1, src2 ASR #31
MACRO(SBCMPLNT, Reg3,      0,              0,  LEGAL_REG3,     INSTR_TYPE(Forms_SBCMPLNT), D___C)


//VFP instructions:
MACRO(VABS,     Reg2,      0,              0,  LEGAL_REG2,      INSTR_TYPE(Forms_VABS),   D___C)
MACRO(VADDF64,  Reg3,      0,              0,  LEGAL_REG3,      INSTR_TYPE(Forms_VADDF64),   D___C)
MACRO(VCMPF64,  Reg1,      OpSideEffect,   0,  LEGAL_CMP_SH,    INSTR_TYPE(Forms_VCMPF64),   D___C)
MACRO(VCVTF64F32, Reg2,    0,              0,  LEGAL_REG2,      INSTR_TYPE(Forms_VCVTF64F32),   D___C)
MACRO(VCVTF32F64, Reg2,    0,              0,  LEGAL_REG2,      INSTR_TYPE(Forms_VCVTF32F64),   D___C)
MACRO(VCVTF64S32, Reg2,    0,              0,  LEGAL_REG2,      INSTR_TYPE(Forms_VCVTF64S32),   D___C)
MACRO(VCVTF64U32, Reg2,    0,              0,  LEGAL_REG2,      INSTR_TYPE(Forms_VCVTF64U32),   D___C)
MACRO(VCVTS32F64, Reg2,    0,              0,  LEGAL_REG2,      INSTR_TYPE(Forms_VCVTS32F64),   D___C)
MACRO(VCVTRS32F64, Reg2,   0,              0,  LEGAL_REG2,      INSTR_TYPE(Forms_VCVTRS32F64),   D___C)
MACRO(VDIVF64,  Reg3,      0,              0,  LEGAL_REG3,      INSTR_TYPE(Forms_VDIVF64),   D___C)
MACRO(VLDR,     Reg2,      0,              0,  LEGAL_VLOAD,     INSTR_TYPE(Forms_VLDR),   DL__C)
MACRO(VLDR32,   Reg2,      0,              0,  LEGAL_VLOAD,     INSTR_TYPE(Forms_VLDR32), DL__C) //single precision float load
MACRO(VMOV,     Reg2,      0,              0,  LEGAL_REG2,      INSTR_TYPE(Forms_VMOV),   DM__C)
MACRO(VMOVARMVFP, Reg2,    0,              0,  LEGAL_REG2,      INSTR_TYPE(Forms_VMOVARMVFP),   DM__C)
MACRO(VMRS,     Empty,     OpSideEffect,   0,  LEGAL_NONE,      INSTR_TYPE(Forms_VMRS),   D___C)
MACRO(VMRSR,    Reg1,      OpSideEffect,   0,  LEGAL_NONE,      INSTR_TYPE(Forms_VMRSR),   D___C)
MACRO(VMSR,     Reg1,      OpSideEffect,   0,  LEGAL_NONE,      INSTR_TYPE(Forms_VMSR),   D___C)
MACRO(VMULF64,  Reg3,      0,              0,  LEGAL_REG3,      INSTR_TYPE(Forms_VMULF64),   D___C)
MACRO(VNEGF64,  Reg2,      0,              0,  LEGAL_REG2,      INSTR_TYPE(Forms_VNEGF64),   D___C)
MACRO(VPUSH,    Reg2,      OpSideEffect,   0,  LEGAL_VPUSHPOP,  INSTR_TYPE(Forms_VPUSH), DSUS_C)
MACRO(VPOP,     Reg2,      OpSideEffect,   0,  LEGAL_VPUSHPOP,  INSTR_TYPE(Forms_VPOP), DLUP_C)
MACRO(VSUBF64,  Reg3,      0,              0,  LEGAL_REG3,      INSTR_TYPE(Forms_VSUBF64),   D___C)
MACRO(VSQRT,    Reg2,      0,              0,  LEGAL_REG2,      INSTR_TYPE(Forms_VSQRT),   D___C)
MACRO(VSTR,     Reg2,      0,              0,  LEGAL_VSTORE,    INSTR_TYPE(Forms_VSTR),   DS__C)
MACRO(VSTR32,   Reg2,      0,              0,  LEGAL_VSTORE,    INSTR_TYPE(Forms_VSTR32), DS__C) //single precision float store
