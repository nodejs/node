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
public:
    LinearScanMD(Func *func) { }

    void        Init(LinearScan *linearScan) { __debugbreak(); }

    BitVector   FilterRegIntSizeConstraints(BitVector regsBv, BitVector sizeUsageBv) const { __debugbreak(); return 0; }
    bool        FitRegIntSizeConstraints(RegNum reg, BitVector sizeUsageBv) const { __debugbreak(); return 0; }
    void        InsertOpHelperSpillAndRestores(SList<OpHelperBlock> *opHelperBlockList) { __debugbreak(); }
    void        EndOfHelperBlock(uint32 helperSpilledLiveranges) { __debugbreak(); }

    uint        UnAllocatableRegCount(Func *func) const
              {
                  __debugbreak(); return 0;
//                  return func->GetLocalsPointer() != RegSP ? 5 : 4; //r11(Frame Pointer),r12,sp,pc
              }

    StackSym   *EnsureSpillSymForVFPReg(RegNum reg, Func *func) { __debugbreak(); return 0; }

    void        LegalizeDef(IR::Instr * instr) { __debugbreak(); }
    void        LegalizeUse(IR::Instr * instr, IR::Opnd * opnd) { __debugbreak(); }
    void        LegalizeConstantUse(IR::Instr * instr, IR::Opnd * opnd) { __debugbreak(); }

    void        GenerateBailOut(IR::Instr * instr, StackSym ** registerSaveSyms, uint registerSaveSymsCount) { __debugbreak(); }
    IR::Instr  *GenerateBailInForGeneratorYield(IR::Instr * resumeLabelInstr, BailOutInfo * bailOutInfo) { __debugbreak(); return nullptr; }

public:
    static void SaveAllRegistersAndBailOut(BailOutRecord *const bailOutRecord) { __debugbreak(); }
    static void SaveAllRegistersAndBranchBailOut(BranchBailOutRecord *const bailOutRecord, const BOOL condition) { __debugbreak(); }

    bool        IsAllocatable(RegNum reg, Func *func) const { __debugbreak(); return 0; }
    static uint GetRegisterSaveSlotCount() {
        return RegisterSaveSlotCount ;
    }
    static uint GetRegisterSaveIndex(RegNum reg) { __debugbreak(); return 0; }
    static RegNum GetRegisterFromSaveIndex(uint offset) { __debugbreak(); return RegNum(0); }

    static const uint RegisterSaveSlotCount = RegNumCount + VFP_REGCOUNT;
};
