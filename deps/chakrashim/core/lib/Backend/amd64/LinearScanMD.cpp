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
    this->byteableRegsBv.ClearAll();

    FOREACH_REG(reg)
    {
        if (LinearScan::GetRegAttribs(reg) & RA_BYTEABLE)
        {
            this->byteableRegsBv.Set(reg);
        }
    } NEXT_REG;

    memset(this->xmmSymTable128, 0, sizeof(this->xmmSymTable128));
    memset(this->xmmSymTable64, 0, sizeof(this->xmmSymTable64));
    memset(this->xmmSymTable32, 0, sizeof(this->xmmSymTable32));
}

BitVector
LinearScanMD::FilterRegIntSizeConstraints(BitVector regsBv, BitVector sizeUsageBv) const
{
    // Requires byte-able reg?
    if (sizeUsageBv.Test(1))
    {
        regsBv.And(this->byteableRegsBv);
    }

    return regsBv;
}

bool
LinearScanMD::FitRegIntSizeConstraints(RegNum reg, BitVector sizeUsageBv) const
{
    // Requires byte-able reg?
    return !sizeUsageBv.Test(1) || this->byteableRegsBv.Test(reg);
}

bool
LinearScanMD::FitRegIntSizeConstraints(RegNum reg, IRType type) const
{
    // Requires byte-able reg?
    return TySize[type] != 1 || this->byteableRegsBv.Test(reg);
}

StackSym *
LinearScanMD::EnsureSpillSymForXmmReg(RegNum reg, Func *func, IRType type)
{
    Assert(REGNUM_ISXMMXREG(reg));

    __analysis_assume(reg - FIRST_XMM_REG < XMM_REGCOUNT);
    StackSym *sym;
    if (type == TyFloat32)
    {
        sym = this->xmmSymTable32[reg - FIRST_XMM_REG];
    }
    else if (type == TyFloat64)
    {
        sym = this->xmmSymTable64[reg - FIRST_XMM_REG];
    }
    else
    {
        Assert(IRType_IsSimd128(type));
        sym = this->xmmSymTable128[reg - FIRST_XMM_REG];
    }

    if (sym == nullptr)
    {
        sym = StackSym::New(type, func);
        func->StackAllocate(sym, TySize[type]);

        __analysis_assume(reg - FIRST_XMM_REG < XMM_REGCOUNT);

        if (type == TyFloat32)
        {
            this->xmmSymTable32[reg - FIRST_XMM_REG] = sym;
        }
        else if (type == TyFloat64)
        {
            this->xmmSymTable64[reg - FIRST_XMM_REG] = sym;
        }
        else
        {
            Assert(IRType_IsSimd128(type));
            this->xmmSymTable128[reg - FIRST_XMM_REG] = sym;
        }
    }

    return sym;
}

void
LinearScanMD::LegalizeConstantUse(IR::Instr * instr, IR::Opnd * opnd)
{
    Assert(opnd->IsAddrOpnd() || opnd->IsIntConstOpnd());
    intptr value = opnd->IsAddrOpnd() ? (intptr)opnd->AsAddrOpnd()->m_address : opnd->AsIntConstOpnd()->GetValue();
    if (value == 0
        && instr->m_opcode == Js::OpCode::MOV
        && !instr->GetDst()->IsRegOpnd()
        && TySize[opnd->GetType()] >= 4)
    {
        Assert(this->linearScan->instrUseRegs.IsEmpty());

        // MOV doesn't have a imm8 encoding for 32-bit/64-bit assignment, so if we have a register available,
        // we should hoist it and generate xor reg, reg and MOV dst, reg
        BitVector regsBv;
        regsBv.Copy(this->linearScan->activeRegs);
        regsBv.ComplimentAll();
        regsBv.And(this->linearScan->int32Regs);
        regsBv.Minus(this->linearScan->tempRegs);       // Avoid tempRegs
        BVIndex regIndex = regsBv.GetNextBit();
        if (regIndex != BVInvalidIndex)
        {
            instr->HoistSrc1(Js::OpCode::MOV, (RegNum)regIndex);
            this->linearScan->instrUseRegs.Set(regIndex);
            this->func->m_regsUsed.Set(regIndex);

            // If we are in a loop, we need to mark the register being used by the loop so that
            // reload to that register will not be hoisted out of the loop
            this->linearScan->RecordLoopUse(nullptr, (RegNum)regIndex);
        }
    }
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
            IRType type = opHelperSpilledLifetime.lifetime->sym->GetType();
            IR::RegOpnd *regOpnd = IR::RegOpnd::New(nullptr, opHelperSpilledLifetime.reg, type, this->func);

            if (!sym)
            {
                sym = EnsureSpillSymForXmmReg(regOpnd->GetReg(), this->func, type);
            }

            IR::Instr   *pushInstr = IR::Instr::New(LowererMDArch::GetAssignOp(type), IR::SymOpnd::New(sym, type, this->func), regOpnd, this->func);
            opHelperBlock.opHelperLabel->InsertAfter(pushInstr);
            pushInstr->CopyNumber(opHelperBlock.opHelperLabel);
            if (opHelperSpilledLifetime.reload)
            {
                IR::Instr   *popInstr = IR::Instr::New(LowererMDArch::GetAssignOp(type), regOpnd, IR::SymOpnd::New(sym, type, this->func), this->func);
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
            IR::RegOpnd * regOpnd = IR::RegOpnd::New(nullptr, opHelperSpilledLifetime.reg, sym->GetType(), func);
            LowererMD::CreateAssign(IR::SymOpnd::New(sym, sym->GetType(), func), regOpnd, opHelperBlock.opHelperLabel->m_next);
            if (opHelperSpilledLifetime.reload)
            {
                LowererMD::CreateAssign(regOpnd, IR::SymOpnd::New(sym, sym->GetType(), func), opHelperBlock.opHelperEndInstr);
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
LinearScanMD::GenerateBailOut(IR::Instr * instr, __in_ecount(registerSaveSymsCount) StackSym ** registerSaveSyms, uint registerSaveSymsCount)
{
    Func *const func = instr->m_func;
    BailOutInfo *const bailOutInfo = instr->GetBailOutInfo();
    IR::Instr *firstInstr = instr->m_prev;

    // Save registers used for parameters, and rax, if necessary, into the shadow space allocated for register parameters:
    //     mov  [rsp + 16], rdx
    //     mov  [rsp + 8], rcx
    //     mov  [rsp], rax
    for(RegNum reg = bailOutInfo->branchConditionOpnd ? RegRDX : RegRCX;
        reg != RegNOREG;
        reg = static_cast<RegNum>(reg - 1))
    {
        StackSym *const stackSym = registerSaveSyms[reg - 1];
        if(!stackSym)
        {
            continue;
        }

        const IRType regType = RegTypes[reg];
        Lowerer::InsertMove(
            IR::SymOpnd::New(func->m_symTable->GetArgSlotSym(static_cast<Js::ArgSlot>(reg)), regType, func),
            IR::RegOpnd::New(stackSym, reg, regType, func),
            instr);
    }

    if(bailOutInfo->branchConditionOpnd)
    {
        // Pass in the branch condition
        //     mov  rdx, condition
        IR::Instr *const newInstr =
            Lowerer::InsertMove(
                IR::RegOpnd::New(nullptr, RegRDX, bailOutInfo->branchConditionOpnd->GetType(), func),
                bailOutInfo->branchConditionOpnd,
                instr);
        linearScan->SetSrcRegs(newInstr);
    }

    // Pass in the bailout record
    //     mov  rcx, bailOutRecord
    Lowerer::InsertMove(
        IR::RegOpnd::New(nullptr, RegRCX, TyMachPtr, func),
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

    // Load the bailout target into rax
    //     mov  rax, BailOut
    //     call rax
    Assert(instr->GetSrc1()->IsHelperCallOpnd());
    Lowerer::InsertMove(IR::RegOpnd::New(nullptr, RegRAX, TyMachPtr, func), instr->GetSrc1(), instr);
    instr->ReplaceSrc1(IR::RegOpnd::New(nullptr, RegRAX, TyMachPtr, func));
}

// Gets the InterpreterStackFrame pointer into RAX.
// Restores the live stack locations followed by the live registers from
// the interpreter's register slots.
// RecordDefs each live register that is restored.
//
// Generates the following code:
//
// MOV rax, param0
// MOV rax, [rax + JavascriptGenerator::GetFrameOffset()]
//
// for each live stack location, sym
//
//   MOV rcx, [rax + regslot offset]
//   MOV sym(stack location), rcx
//
// for each live register, sym (rax is restore last if it is live)
//
//   MOV sym(register), [rax + regslot offset]
//
IR::Instr *
LinearScanMD::GenerateBailInForGeneratorYield(IR::Instr * resumeLabelInstr, BailOutInfo * bailOutInfo)
{
    IR::Instr * instrAfter = resumeLabelInstr->m_next;

    IR::RegOpnd * raxRegOpnd = IR::RegOpnd::New(nullptr, RegRAX, TyMachPtr, this->func);
    IR::RegOpnd * rcxRegOpnd = IR::RegOpnd::New(nullptr, RegRCX, TyVar, this->func);

    StackSym * sym = StackSym::NewParamSlotSym(1, this->func);
    this->func->SetArgOffset(sym, LowererMD::GetFormalParamOffset() * MachPtr);
    IR::SymOpnd * symOpnd = IR::SymOpnd::New(sym, TyMachPtr, this->func);
    LinearScan::InsertMove(raxRegOpnd, symOpnd, instrAfter);

    IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(raxRegOpnd, Js::JavascriptGenerator::GetFrameOffset(), TyMachPtr, this->func);
    LinearScan::InsertMove(raxRegOpnd, indirOpnd, instrAfter);


    // rax points to the frame, restore stack syms and registers except rax, restore rax last

    IR::Instr * raxRestoreInstr = nullptr;
    IR::Instr * instrInsertStackSym = instrAfter;
    IR::Instr * instrInsertRegSym = instrAfter;

    Assert(bailOutInfo->capturedValues.constantValues.Empty());
    Assert(bailOutInfo->capturedValues.copyPropSyms.Empty());
    Assert(bailOutInfo->liveLosslessInt32Syms->IsEmpty());
    Assert(bailOutInfo->liveFloat64Syms->IsEmpty());

    auto restoreSymFn = [this, &raxRegOpnd, &rcxRegOpnd, &raxRestoreInstr, &instrInsertStackSym, &instrInsertRegSym](Js::RegSlot regSlot, StackSym* stackSym)
    {
        Assert(stackSym->IsVar());

        int32 offset = regSlot * sizeof(Js::Var) + Js::InterpreterStackFrame::GetOffsetOfLocals();

        IR::Opnd * srcOpnd = IR::IndirOpnd::New(raxRegOpnd, offset, stackSym->GetType(), this->func);
        Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;

        if (lifetime->isSpilled)
        {
            // stack restores require an extra register since we can't move an indir directly to an indir on amd64
            IR::SymOpnd * dstOpnd = IR::SymOpnd::New(stackSym, stackSym->GetType(), this->func);
            LinearScan::InsertMove(rcxRegOpnd, srcOpnd, instrInsertStackSym);
            LinearScan::InsertMove(dstOpnd, rcxRegOpnd, instrInsertStackSym);
        }
        else
        {
            // register restores must come after stack restores so that we have RAX and RCX free to
            // use for stack restores and further RAX must be restored last since it holds the
            // pointer to the InterpreterStackFrame from which we are restoring values.
            // We must also track these restores using RecordDef in case the symbols are spilled.

            IR::RegOpnd * dstRegOpnd = IR::RegOpnd::New(stackSym, stackSym->GetType(), this->func);
            dstRegOpnd->SetReg(lifetime->reg);

            IR::Instr * instr = LinearScan::InsertMove(dstRegOpnd, srcOpnd, instrInsertRegSym);

            if (instrInsertRegSym == instrInsertStackSym)
            {
                // this is the first register sym, make sure we don't insert stack stores
                // after this instruction so we can ensure rax and rcx remain free to use
                // for restoring spilled stack syms.
                instrInsertStackSym = instr;
            }

            if (lifetime->reg == RegRAX)
            {
                // ensure rax is restored last
                Assert(instrInsertRegSym != instrInsertStackSym);

                instrInsertRegSym = instr;

                if (raxRestoreInstr != nullptr)
                {
                    AssertMsg(false, "this is unexpected until copy prop is enabled");
                    // rax was mapped to multiple bytecode registers.  Obviously only the first
                    // restore we do will work so change all following stores to `mov rax, rax`.
                    // We still need to keep them around for RecordDef in case the corresponding
                    // dst sym is spilled later on.
                    raxRestoreInstr->FreeSrc1();
                    raxRestoreInstr->SetSrc1(raxRegOpnd);
                }

                raxRestoreInstr = instr;
            }

            this->linearScan->RecordDef(lifetime, instr, 0);
        }
    };

    FOREACH_BITSET_IN_SPARSEBV(symId, bailOutInfo->byteCodeUpwardExposedUsed)
    {
        StackSym* stackSym = this->func->m_symTable->FindStackSym(symId);
        restoreSymFn(stackSym->GetByteCodeRegSlot(), stackSym);
    }
    NEXT_BITSET_IN_SPARSEBV;

    if (bailOutInfo->capturedValues.argObjSyms)
    {
        FOREACH_BITSET_IN_SPARSEBV(symId, bailOutInfo->capturedValues.argObjSyms)
        {
            StackSym* stackSym = this->func->m_symTable->FindStackSym(symId);
            restoreSymFn(stackSym->GetByteCodeRegSlot(), stackSym);
        }
        NEXT_BITSET_IN_SPARSEBV;
    }

    Js::RegSlot localsCount = this->func->GetJnFunction()->GetLocalsCount();
    bailOutInfo->IterateArgOutSyms([localsCount, &restoreSymFn](uint, uint argOutSlotOffset, StackSym* sym) {
        restoreSymFn(localsCount + argOutSlotOffset, sym);
    });

    return instrAfter;
}

uint LinearScanMD::GetRegisterSaveIndex(RegNum reg)
{
    if (RegTypes[reg] == TyFloat64)
    {
        // make room for maximum XMM reg size
        Assert(reg >= RegXMM0);
        return (reg - RegXMM0) * (sizeof(SIMDValue) / sizeof(Js::Var)) + RegXMM0;
    }
    else
    {
        return reg;
    }
}

RegNum LinearScanMD::GetRegisterFromSaveIndex(uint offset)
{
    return (RegNum)(offset >= RegXMM0 ? (offset - RegXMM0) / (sizeof(SIMDValue) / sizeof(Js::Var)) + RegXMM0 : offset);
}
