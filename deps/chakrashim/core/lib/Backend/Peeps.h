//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "PeepsMD.h"

#if defined(_M_IX86) || defined(_M_X64)
#include "AgenPeeps.h"
#endif

class Peeps
{
    friend class PeepsMD;
private:
    Func *      func;
#if defined(_M_IX86) || defined(_M_X64)
    AgenPeeps   peepsAgen;
#endif
    PeepsMD     peepsMD;
    StackSym*   regMap[RegNumCount];
    void        ClearRegMap();

public:
#if defined(_M_IX86) || defined(_M_X64)
    Peeps(Func *func) : func(func), peepsMD(func), peepsAgen(func) {}
#else
    Peeps(Func *func) : func(func), peepsMD(func) {}
#endif

    void                 PeepFunc();
    IR::Instr *          PeepAssign(IR::Instr *assign);
    static IR::LabelInstr *RetargetBrToBr(IR::BranchInstr *branchInstr, IR::LabelInstr * targetInstr);
    static IR::Instr *   PeepBranch(IR::BranchInstr *branchInstr, bool *const peepedRef = nullptr);
    static IR::Instr *   PeepUnreachableLabel(IR::LabelInstr *deadLabel, const bool isInHelper, bool *const peepedRef = nullptr);
    static IR::Instr *   CleanupLabel(IR::LabelInstr * instr, IR::LabelInstr * instrNext);

private:
    void        SetReg(RegNum reg, StackSym *sym);
    void        ClearReg(RegNum reg);
    static IR::Instr * RemoveDeadBlock(IR::Instr *instr, bool* wasStmtBoundaryKeptInDeadBlock = nullptr);

#if defined(_M_IX86) || defined(_M_X64)
    IR::Instr * PeepRedundant(IR::Instr *instr);
    IR::Instr * PeepCondMove(IR::LabelInstr *labelInstr, IR::Instr *nextInstr, const bool isInHelper);
    static bool IsJccOrShiftInstr(IR::Instr *instr);
    IR::Instr * HoistSameInstructionAboveSplit(IR::BranchInstr *branchInstr, IR::Instr *instrNext);
#endif


};
