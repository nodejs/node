//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"
#include "SCCLiveness.h"

extern const IRType RegTypes[RegNumCount];

LinearScanMD::LinearScanMD(Func *func)
    : helperSpillSlots(nullptr),
      maxOpHelperSpilledLiveranges(0),
      func(func)
{
}

void
LinearScanMD::Init(LinearScan *linearScan)
{
    LinearScanMDShared::Init(linearScan);
    Func *func = linearScan->func;
    RegNum localsReg = func->GetLocalsPointer();
    if (localsReg != RegSP)
    {
        func->m_regsUsed.Set(localsReg);
    }

    memset(this->vfpSymTable, 0, sizeof(this->vfpSymTable));
}

StackSym *
LinearScanMD::EnsureSpillSymForVFPReg(RegNum reg, Func *func)
{
    Assert(REGNUM_ISVFPREG(reg));

    __analysis_assume(reg - RegD0 < VFP_REGCOUNT);
    StackSym *sym = this->vfpSymTable[reg - RegD0];

    if (sym == nullptr)
    {
        sym = StackSym::New(TyFloat64, func);
        func->StackAllocate(sym, MachRegDouble);

        __analysis_assume(reg - RegD0 < VFP_REGCOUNT);
        this->vfpSymTable[reg - RegD0] = sym;
    }

    return sym;
}


bool
LinearScanMD::IsAllocatable(RegNum reg, Func *func) const
{
    return reg != func->GetLocalsPointer();
}

BitVector
LinearScanMD::FilterRegIntSizeConstraints(BitVector regsBv, BitVector sizeUsageBv) const
{
    return regsBv;
}

bool
LinearScanMD::FitRegIntSizeConstraints(RegNum reg, BitVector sizeUsageBv) const
{
    return true;
}

bool
LinearScanMD::FitRegIntSizeConstraints(RegNum reg, IRType type) const
{
    return true;
}

void
LinearScanMD::InsertOpHelperSpillAndRestores(SList<OpHelperBlock> *opHelperBlockList)
{
    if (maxOpHelperSpilledLiveranges)
    {
        Assert(!helperSpillSlots);
        helperSpillSlots = AnewArrayZ(linearScan->GetTempAlloc(), StackSym *, maxOpHelperSpilledLiveranges);
    }

    FOREACH_SLIST_ENTRY(OpHelperBlock, opHelperBlock, opHelperBlockList)
    {
        InsertOpHelperSpillsAndRestores(opHelperBlock);
    }
    NEXT_SLIST_ENTRY;
}

void
LinearScanMD::InsertOpHelperSpillsAndRestores(const OpHelperBlock& opHelperBlock)
{
    uint32 index = 0;

    FOREACH_SLIST_ENTRY(OpHelperSpilledLifetime, opHelperSpilledLifetime, &opHelperBlock.spilledLifetime)
    {
        // Use the original sym as spill slot if this is an inlinee arg
        StackSym* sym = nullptr;
        if (opHelperSpilledLifetime.spillAsArg)
        {
            sym = opHelperSpilledLifetime.lifetime->sym;
            AnalysisAssert(sym);
            Assert(sym->IsAllocated());
        }

        if (RegTypes[opHelperSpilledLifetime.reg] == TyFloat64)
        {
            IR::RegOpnd * regOpnd = IR::RegOpnd::New(nullptr, opHelperSpilledLifetime.reg, TyMachDouble, this->func);

            if (!sym)
            {
                sym = EnsureSpillSymForVFPReg(regOpnd->GetReg(), this->func);
            }
            IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::VSTR, IR::SymOpnd::New(sym, TyMachDouble, this->func), regOpnd, this->func);
            opHelperBlock.opHelperLabel->InsertAfter(pushInstr);
            pushInstr->CopyNumber(opHelperBlock.opHelperLabel);
            if (opHelperSpilledLifetime.reload)
            {
                IR::Instr * popInstr = IR::Instr::New(Js::OpCode::VLDR, regOpnd, IR::SymOpnd::New(sym, TyMachDouble, this->func), this->func);
                opHelperBlock.opHelperEndInstr->InsertBefore(popInstr);
                popInstr->CopyNumber(opHelperBlock.opHelperEndInstr);
            }
        }
        else
        {
            Assert(helperSpillSlots);
            Assert(index < maxOpHelperSpilledLiveranges);

            if (!sym)
            {
                // Lazily allocate only as many slots as we really need.
                if (!helperSpillSlots[index])
                {
                    helperSpillSlots[index] = StackSym::New(TyMachReg, func);
                }

                sym = helperSpillSlots[index];
                index++;

                Assert(sym);
                func->StackAllocate(sym, MachRegInt);
            }

            IR::RegOpnd * regOpnd = IR::RegOpnd::New(sym, opHelperSpilledLifetime.reg, sym->GetType(), func);
            IR::Instr * saveInstr = IR::Instr::New(Js::OpCode::STR, IR::SymOpnd::New(sym, sym->GetType(), func), regOpnd, func);
            opHelperBlock.opHelperLabel->InsertAfter(saveInstr);
            saveInstr->CopyNumber(opHelperBlock.opHelperLabel);
            this->LegalizeDef(saveInstr);

            if (opHelperSpilledLifetime.reload)
            {
                IR::Instr * restoreInstr = IR::Instr::New(Js::OpCode::LDR, regOpnd, IR::SymOpnd::New(sym, sym->GetType(), func), func);
                opHelperBlock.opHelperEndInstr->InsertBefore(restoreInstr);
                restoreInstr->CopyNumber(opHelperBlock.opHelperEndInstr);
                this->LegalizeUse(restoreInstr, restoreInstr->GetSrc1());
            }
        }
    }
    NEXT_SLIST_ENTRY;
}

void
LinearScanMD::EndOfHelperBlock(uint32 helperSpilledLiveranges)
{
    if (helperSpilledLiveranges > maxOpHelperSpilledLiveranges)
    {
        maxOpHelperSpilledLiveranges = helperSpilledLiveranges;
    }
}

void
LinearScanMD::LegalizeDef(IR::Instr * instr)
{
    if (instr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn)
    {
        // ArgOut_A_InlineBuiltIn pseudo instruction is kept through register allocator only to use for bailout as is,
        // and thus it must not be changed here by legalization.
        // It is removed in peeps, so only place to special case it is in register allocator.
        return;
    }

    // Legalize opcodes, etc., but do not expand symbol/indirs with large offsets
    // because we can't safely do this until all loads and stores are in place.
    LegalizeMD::LegalizeDst(instr, false);
}

void
LinearScanMD::LegalizeUse(IR::Instr * instr, IR::Opnd * opnd)
{
    if (instr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn)
    {
        // ArgOut_A_InlineBuiltIn pseudo instruction is kept through register allocator only to use for bailout as is,
        // and thus it must not be changed here by legalization.
        // It is removed in peeps, so only place to special case it is in register allocator.
        return;
    }

    // Legalize opcodes, etc., but do not expand symbol/indirs with large offsets
    // because we can't safely do this until all loads and stores are in place.
    if (opnd == instr->GetSrc1())
    {
        LegalizeMD::LegalizeSrc(instr, opnd, 1, false);
    }
    else
    {
        LegalizeMD::LegalizeSrc(instr, opnd, 2, false);
    }
}

void
LinearScanMD::GenerateBailOut(
    IR::Instr * instr,
    __in_ecount(registerSaveSymsCount) StackSym ** registerSaveSyms,
    uint registerSaveSymsCount)
{
    Func *const func = instr->m_func;
    BailOutInfo *const bailOutInfo = instr->GetBailOutInfo();
    IR::Instr *firstInstr = instr->m_prev;
    Js::Var *const registerSaveSpace = func->GetScriptContext()->GetThreadContext()->GetBailOutRegisterSaveSpace();

    const auto LoadRegSaveSpaceIntoScratch = [&](const RegNum reg)
    {
        // Load the register save space address for the specified register into the scratch register:
        //     ldimm SCRATCH_REG, regSaveSpace
        LinearScan::InsertMove(
            IR::RegOpnd::New(nullptr, SCRATCH_REG, TyMachPtr, func),
            IR::AddrOpnd::New(&registerSaveSpace[reg - 1], IR::AddrOpndKindDynamicMisc, func),
            instr);
    };

    const auto SaveReg = [&](const RegNum reg)
    {
        Assert(registerSaveSyms[reg - 1]);

        //     LoadRegSaveSpaceIntoScratch(reg)
        //     mov  [SCRATCH_REG], reg
        LoadRegSaveSpaceIntoScratch(reg);
        const IRType regType = RegTypes[reg];
        LinearScan::InsertMove(
            IR::IndirOpnd::New(
                IR::RegOpnd::New(nullptr, SCRATCH_REG, TyMachPtr, func),
                0,
                regType,
                func),
            IR::RegOpnd::New(registerSaveSyms[reg - 1], reg, regType, func),
            instr);
    };

    // Save registers used for parameters, and lr, if necessary, into the register save space
    if(bailOutInfo->branchConditionOpnd && registerSaveSyms[RegR1 - 1] && registerSaveSyms[RegR0 - 1])
    {
        // Save r1 and r0 with one push:
        //     LoadRegSaveSpaceIntoScratch(RegR2)
        //     push [SCRATCH_REG], {r0 - r1}
        LoadRegSaveSpaceIntoScratch(RegR2);
        IR::Instr *instrPush = IR::Instr::New(
            Js::OpCode::PUSH,
            IR::IndirOpnd::New(
            IR::RegOpnd::New(nullptr, SCRATCH_REG, TyMachPtr, func),
            0,
            TyMachReg,
            func),
            IR::RegBVOpnd::New(BVUnit32((1 << RegR1) - 1), TyMachReg, func),
            func);

        instr->InsertBefore(instrPush);
        instrPush->CopyNumber(instr);
    }
    else if(bailOutInfo->branchConditionOpnd && registerSaveSyms[RegR1 - 1])
    {
        SaveReg(RegR1);
    }
    else if(registerSaveSyms[RegR0 - 1])
    {
        SaveReg(RegR0);
    }
    if(registerSaveSyms[RegLR - 1])
    {
        SaveReg(RegLR);
    }

    if(bailOutInfo->branchConditionOpnd)
    {
        // Pass in the branch condition
        //     mov  r1, condition
        IR::Instr *const newInstr =
            LinearScan::InsertMove(
                IR::RegOpnd::New(nullptr, RegR1, bailOutInfo->branchConditionOpnd->GetType(), func),
                bailOutInfo->branchConditionOpnd,
                instr);
        linearScan->SetSrcRegs(newInstr);
    }

    // Pass in the bailout record
    //     ldimm r0, bailOutRecord
    LinearScan::InsertMove(
        IR::RegOpnd::New(nullptr, RegR0, TyMachPtr, func),
        IR::AddrOpnd::New(bailOutInfo->bailOutRecord, IR::AddrOpndKindDynamicBailOutRecord, func, true),
        instr);

    firstInstr = firstInstr->m_next;
    for(uint i = 0; i < registerSaveSymsCount; i++)
    {
        StackSym *const stackSym = registerSaveSyms[i];
        if(!stackSym)
        {
            continue;
        }

        // Record the use on the lifetime in case it spilled afterwards. Spill loads will be inserted before 'firstInstr', that
        // is, before the register saves are done.
        this->linearScan->RecordUse(stackSym->scratch.linearScan.lifetime, firstInstr, nullptr, true);
    }

    // Load the bailout target into lr
    //     ldimm lr, BailOut
    //     blx  lr
    Assert(instr->GetSrc1()->IsHelperCallOpnd());
    LinearScan::InsertMove(IR::RegOpnd::New(nullptr, RegLR, TyMachPtr, func), instr->GetSrc1(), instr);
    instr->ReplaceSrc1(IR::RegOpnd::New(nullptr, RegLR, TyMachPtr, func));
}

IR::Instr *
LinearScanMD::GenerateBailInForGeneratorYield(IR::Instr * resumeLabelInstr, BailOutInfo * bailOutInfo)
{
    Js::Throw::NotImplemented();
}

uint LinearScanMD::GetRegisterSaveIndex(RegNum reg)
{
    if (RegTypes[reg] == TyFloat64)
    {
        Assert(reg+1 >= RegD0);
        return (reg - RegD0) * 2 + RegD0;
    }
    else
    {
        return reg;
    }
}

// static
RegNum LinearScanMD::GetRegisterFromSaveIndex(uint offset)
{
    return (RegNum)(offset >= RegD0 ? (offset - RegD0) / 2  + RegD0 : offset);
}
