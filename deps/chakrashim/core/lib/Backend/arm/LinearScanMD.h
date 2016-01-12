//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
class OpHelperBlock;
class LinearScan;
class BailOutRecord;
class BranchBailOutRecord;

class LinearScanMD : public LinearScanMDShared
{
private:
    StackSym ** helperSpillSlots;
    Func      * func;
    uint32      maxOpHelperSpilledLiveranges;
    StackSym   *vfpSymTable[VFP_REGCOUNT];

public:
    LinearScanMD(Func *func);

    void        Init(LinearScan *linearScan);

    BitVector   FilterRegIntSizeConstraints(BitVector regsBv, BitVector sizeUsageBv) const;
    bool        FitRegIntSizeConstraints(RegNum reg, BitVector sizeUsageBv) const;
    bool        FitRegIntSizeConstraints(RegNum reg, IRType type) const;
    void        InsertOpHelperSpillAndRestores(SList<OpHelperBlock> *opHelperBlockList);
    void        EndOfHelperBlock(uint32 helperSpilledLiveranges);

    uint        UnAllocatableRegCount(Func *func) const
                {
                    return func->GetLocalsPointer() != RegSP ? 5 : 4; //r11(Frame Pointer),r12,sp,pc
                }

    StackSym   *EnsureSpillSymForVFPReg(RegNum reg, Func *func);

    void        LegalizeDef(IR::Instr * instr);
    void        LegalizeUse(IR::Instr * instr, IR::Opnd * opnd);
    void        LegalizeConstantUse(IR::Instr * instr, IR::Opnd * opnd) { /* no op for arm */ }

    void        GenerateBailOut(IR::Instr * instr, __in_ecount(registerSaveSymsCount) StackSym ** registerSaveSyms, uint registerSaveSymsCount);
    IR::Instr  *GenerateBailInForGeneratorYield(IR::Instr * resumeLabelInstr, BailOutInfo * bailOutInfo);
private:
    static void SaveAllRegisters(BailOutRecord *const bailOutRecord);
public:
    static void SaveAllRegistersAndBailOut(BailOutRecord *const bailOutRecord);
    static void SaveAllRegistersAndBranchBailOut(BranchBailOutRecord *const bailOutRecord, const BOOL condition);

    bool        IsAllocatable(RegNum reg, Func *func) const;
    static uint GetRegisterSaveSlotCount() {
        return RegisterSaveSlotCount ;
    }
    static uint GetRegisterSaveIndex(RegNum reg);
    static RegNum GetRegisterFromSaveIndex(uint offset);

    static const uint RegisterSaveSlotCount = RegNumCount + VFP_REGCOUNT;

private:
    bool        LegalizeOffset(IR::Instr * instr, IR::Opnd * opnd);
    bool        LegalizeLEA(IR::Instr * instr, IR::SymOpnd * opnd);
    void        InsertOpHelperSpillsAndRestores(const OpHelperBlock& opHelperBlock);
};
