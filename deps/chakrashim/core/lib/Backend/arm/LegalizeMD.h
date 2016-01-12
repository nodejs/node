//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

enum LegalForms
{
    L_None =    0,

    L_Reg  =    1,
    L_RegMask = L_Reg,

    L_ImmModC12 =  0x10,
    L_ImmU12 =     0x20,
    L_ImmU5 =      0x40,
    L_ImmU16 =     0x80,
    L_Imm =        0x100,
    L_ImmMask =    (L_ImmModC12 | L_ImmU12 | L_ImmU5 | L_ImmU16 | L_Imm),

    L_IndirI8 =    0x1000,
    L_IndirU12I8 = 0x2000,
    L_IndirU12 =   0x4000,
    L_VIndirI11 =  0x8000,
    L_IndirMask =  (L_IndirI8 | L_IndirU12I8 | L_IndirU12 | L_VIndirI11),

    L_SymU12I8 =   0x10000,
    L_SymU12 =     0x20000,
    L_VSymI11 =    0x40000,
    L_SymMask =    (L_SymU12I8 | L_SymU12 | L_VSymI11),

    L_Lab20 =      0x100000,

    L_RegBV =      0x1000000,
};

struct LegalInstrForms
{
    LegalForms dst, src[2];
};

#define LEGAL_NONE     { L_None,    { L_None,    L_None } }
#define LEGAL_ADDSUB   { L_Reg,     { L_Reg,     (LegalForms)(L_Reg | L_ImmModC12 | L_ImmU12) } }
#define LEGAL_ADDSUBW  { L_Reg,     { L_Reg,     (LegalForms)(L_Reg | L_ImmU12) } }
#define LEGAL_ALU2     { L_Reg,     { (LegalForms)(L_Reg | L_ImmModC12), L_None } }
#define LEGAL_ALU3     { L_Reg,     { L_Reg,     (LegalForms)(L_Reg | L_ImmModC12) } }
#define LEGAL_SHIFT    { L_Reg,     { L_Reg,     (LegalForms)(L_Reg | L_ImmU5) } }
#define LEGAL_BLAB     LEGAL_NONE
#define LEGAL_BREG     { L_None,    { L_Reg,     L_None } }
#define LEGAL_CALL     { L_Reg,     { L_Lab20 ,  L_None } } // Not currently generated, offset check is missing
#define LEGAL_CALLREG  { L_Reg,     { L_Reg,     L_None } }
#define LEGAL_CMP      { L_None,    { L_Reg,     (LegalForms)(L_Reg | L_ImmModC12) } }
#define LEGAL_CMP_SH   { L_None,    { L_Reg,     L_Reg } }
#define LEGAL_CMP1     { L_None,    { L_Reg,     L_None } }
#define LEGAL_CMN      { L_None,    { L_Reg,     (LegalForms)(L_Reg | L_ImmModC12) } }
#define LEGAL_LDIMM    { L_Reg,     { L_Imm,     L_None } }
#define LEGAL_LEA      { L_Reg,     { (LegalForms)(L_SymU12 | L_IndirU12),  L_None } }
#define LEGAL_LOAD     { L_Reg,     { (LegalForms)(L_IndirU12I8 | L_SymU12I8), L_None } }
#define LEGAL_MOVIMM16 { L_Reg,     { L_ImmU16,  L_None } }
#define LEGAL_PUSHPOP  { L_IndirI8, { L_RegBV,   L_None } }
#define LEGAL_REG1     { L_Reg,     { L_None,    L_None } }
#define LEGAL_REG2     { L_Reg,     { L_Reg,     L_None } }
#define LEGAL_REG3     { L_Reg,     { L_Reg,     L_Reg  } }
#define LEGAL_STORE    { (LegalForms)(L_IndirU12I8 | L_SymU12I8), { L_Reg, L_None } }

#define LEGAL_VSTORE   { (LegalForms)(L_VSymI11 | L_VIndirI11), { L_Reg, L_None } }
#define LEGAL_VLOAD    { L_Reg,     { (LegalForms)(L_VSymI11 | L_VIndirI11), L_None } }
#define LEGAL_VPUSHPOP  { L_IndirI8, { L_RegBV,   L_None } }

class LegalizeMD
{
public:
    static void LegalizeInstr(IR::Instr * instr, bool fPostRegAlloc);
    static void LegalizeDst(IR::Instr * instr, bool fPostRegAlloc);
    static void LegalizeSrc(IR::Instr * instr, IR::Opnd * opnd, uint opndNum, bool fPostRegAlloc);

    static bool LegalizeDirectBranch(IR::BranchInstr *instr, uint32 branchOffset); // DirectBranch has no src & dst operands.
    //Returns IndexOpnd which is removed from VFP indirect operand
    static void LegalizeIndirOpndForVFP(IR::Instr* insertInstr, IR::IndirOpnd *indirOpnd, bool fPostRegAlloc);

private:
    static IR::Instr *LegalizeStore(IR::Instr *instr, LegalForms forms, bool fPostRegAlloc);
    static IR::Instr *LegalizeLoad(IR::Instr *instr, uint opndNum, LegalForms forms, bool fPostRegAlloc);
    static void LegalizeIndirOffset(IR::Instr * instr, IR::IndirOpnd * indirOpnd, LegalForms forms, bool fPostRegAlloc);
    static void LegalizeSymOffset(IR::Instr * instr, IR::SymOpnd * indirOpnd, LegalForms forms, bool fPostRegAlloc);
    static void LegalizeImmed(IR::Instr * instr, IR::Opnd * opnd, uint opndNum, IntConstType immed, LegalForms forms, bool fPostRegAlloc);
    static void LegalizeLabelOpnd(IR::Instr * instr, IR::Opnd * opnd, uint opndNum, bool fPostRegAlloc);

    static void LegalizeLDIMM(IR::Instr * instr, IntConstType immed);
    static void LegalizeLdLabel(IR::Instr * instr, IR::Opnd * opnd);
    static IR::Instr * GenerateLDIMM(IR::Instr * instr, uint opndNum, RegNum scratchReg);

    static void ObfuscateLDIMM(IR::Instr * instrMov, IR::Instr * instrMovt);
    static void EmitRandomNopBefore(IR::Instr * instrMov, UINT_PTR rand, RegNum targetReg);

#ifdef DBG
    static void IllegalInstr(IR::Instr * instr, const wchar_t * msg, ...);
#endif
};
