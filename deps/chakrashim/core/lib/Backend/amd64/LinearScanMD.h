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
    BitVector   byteableRegsBv;
    StackSym   *xmmSymTable128[XMM_REGCOUNT];
    StackSym   *xmmSymTable64[XMM_REGCOUNT];
    StackSym   *xmmSymTable32[XMM_REGCOUNT];

public:
    LinearScanMD(Func *func);

    StackSym   *EnsureSpillSymForXmmReg(RegNum reg, Func *func, IRType type);
    BitVector   FilterRegIntSizeConstraints(BitVector regsBv, BitVector sizeUsageBv) const;
    bool        FitRegIntSizeConstraints(RegNum reg, BitVector sizeUsageBv) const;
    bool        FitRegIntSizeConstraints(RegNum reg, IRType type) const;
    bool        IsAllocatable(RegNum reg, Func *func) const { return true; }
    uint        UnAllocatableRegCount(Func *func) const { return 2; /* RSP, RBP */ }
    void        LegalizeDef(IR::Instr * instr) { /* This is a nop for amd64 */ }
    void        LegalizeUse(IR::Instr * instr, IR::Opnd * opnd) { /* A nop for amd64 */ }
    void        LegalizeConstantUse(IR::Instr * instr, IR::Opnd * opnd);
    void        InsertOpHelperSpillAndRestores(SList<OpHelperBlock> *opHelperBlockList);
    void        EndOfHelperBlock(uint32 helperSpilledLiveranges);
    void        GenerateBailOut(IR::Instr * instr,
                                __in_ecount(registerSaveSymsCount) StackSym ** registerSaveSyms,
                                uint registerSaveSymsCount);
    IR::Instr  *GenerateBailInForGeneratorYield(IR::Instr * resumeLabelInstr, BailOutInfo * bailOutInfo);

private:
    static void SaveAllRegisters(BailOutRecord *const bailOutRecord);
public:
    static void SaveAllRegistersAndBailOut(BailOutRecord *const bailOutRecord);
    static void SaveAllRegistersAndBranchBailOut(BranchBailOutRecord *const bailOutRecord, const BOOL condition);

    static uint GetRegisterSaveSlotCount() {
        return RegisterSaveSlotCount;
    }

    static uint GetRegisterSaveIndex(RegNum reg);
    static RegNum GetRegisterFromSaveIndex(uint offset);

    // All regs, including XMMs, plus extra slot space for XMMs (1 XMM = 2 Vars)
    static const uint RegisterSaveSlotCount = RegNumCount + XMM_REGCOUNT;
private:
    void        InsertOpHelperSpillsAndRestores(const OpHelperBlock& opHelperBlock);
};
