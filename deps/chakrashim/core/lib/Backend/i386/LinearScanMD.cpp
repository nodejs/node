//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"
#include "SCCLiveness.h"

extern const IRType RegTypes[RegNumCount];

LinearScanMD::LinearScanMD(Func *func)
    : func(func)
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

    __analysis_assume(reg - RegXMM0 >= 0 && reg - RegXMM0 < XMM_REGCOUNT);
    StackSym *sym;
    if (type == TyFloat32)
    {
        sym = this->xmmSymTable32[reg - RegXMM0];
    }
    else if (type == TyFloat64)
    {
        sym = this->xmmSymTable64[reg - RegXMM0];
    }
    else
    {
        Assert(IRType_IsSimd128(type));
        sym = this->xmmSymTable128[reg - RegXMM0];
    }

    if (sym == NULL)
    {
        sym = StackSym::New(type, func);
        func->StackAllocate(sym, TySize[type]);

        __analysis_assume(reg - RegXMM0 < XMM_REGCOUNT);

        if (type == TyFloat32)
        {
            this->xmmSymTable32[reg - RegXMM0] = sym;
        }
        else if (type == TyFloat64)
        {
            this->xmmSymTable64[reg - RegXMM0] = sym;
        }
        else
        {
            Assert(IRType_IsSimd128(type));
            this->xmmSymTable128[reg - RegXMM0] = sym;
        }
    }

    return sym;
}

void
LinearScanMD::GenerateBailOut(IR::Instr * instr, __in_ecount(registerSaveSymsCount) StackSym ** registerSaveSyms, uint registerSaveSymsCount)
{
    Func *const func = instr->m_func;
    BailOutInfo *const bailOutInfo = instr->GetBailOutInfo();
    IR::Instr *firstInstr = instr->m_prev;
    if(bailOutInfo->branchConditionOpnd)
    {
        // Pass in the branch condition
        //     push condition
        IR::Instr *const newInstr = IR::Instr::New(Js::OpCode::PUSH, func);
        newInstr->SetSrc1(bailOutInfo->branchConditionOpnd);
        instr->InsertBefore(newInstr);
        newInstr->CopyNumber(instr);
        linearScan->SetSrcRegs(newInstr);
    }

    // Pass in the bailout record
    //     push bailOutRecord
    {
        IR::Instr *const newInstr = IR::Instr::New(Js::OpCode::PUSH, func);
        newInstr->SetSrc1(IR::AddrOpnd::New(bailOutInfo->bailOutRecord, IR::AddrOpndKindDynamicBailOutRecord, func, true));
        instr->InsertBefore(newInstr);
        newInstr->CopyNumber(instr);
    }

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
}

IR::Instr *
LinearScanMD::GenerateBailInForGeneratorYield(IR::Instr * resumeLabelInstr, BailOutInfo * bailOutInfo)
{
    BailOutRecord * bailOutRecord = bailOutInfo->bailOutRecord;
    IR::Instr * instrAfter = resumeLabelInstr->m_next;
    IR::Instr * newInstr;

    // if (argOuts) {
    //     sub esp, numArgOutsTimesMachSizeMachAligned
    // }
    // mov eax, prm1
    // mov eax, [eax + offset of JavascriptGenerator::frame]
    // <restore live stack syms, out params, and registers directly from InterpreterStackFrame registers (use ecx as temporary for mov IndirOpnd, IndirOpnd before restoring registers)>

    IntConstType stackAdjustSize = 0;
    bailOutRecord->MapStartCallParamCounts([&stackAdjustSize](uint startCallParamCount) {
        IntConstType sizeValue = startCallParamCount;
        int32 stackAlignment = Math::Align<int32>(sizeValue*MachPtr, MachStackAlignment) - sizeValue*MachPtr;
        if (stackAlignment != 0)
        {
            sizeValue += 1;
        }
        sizeValue *= MachPtr;
        stackAdjustSize += sizeValue;
    });

    if (stackAdjustSize != 0)
    {
        if ((uint32)stackAdjustSize > AutoSystemInfo::PageSize)
        {
            //     mov eax, sizeOpnd->m_value
            //     call _chkstk
            IR::RegOpnd *eaxOpnd = IR::RegOpnd::New(nullptr, RegEAX, TyMachReg, this->func);
            newInstr = IR::Instr::New(Js::OpCode::MOV, eaxOpnd, IR::IntConstOpnd::New(stackAdjustSize, TyInt32, this->func), this->func);
            instrAfter->InsertBefore(newInstr);

            newInstr = IR::Instr::New(Js::OpCode::CALL, this->func);
            newInstr->SetSrc1(IR::HelperCallOpnd::New(IR::HelperCRT_chkstk, this->func));
            instrAfter->InsertBefore(newInstr);
        }
        else
        {
            //     lea esp, [esp - sizeValue]
            IR::RegOpnd * espOpnd = IR::RegOpnd::New(nullptr, LowererMD::GetRegStackPointer(), TyMachReg, this->func);
            newInstr = IR::Instr::New(Js::OpCode::LEA, espOpnd, IR::IndirOpnd::New(espOpnd, -stackAdjustSize, TyMachReg, this->func), this->func);
            instrAfter->InsertBefore(newInstr);
        }
    }

    IR::RegOpnd * eaxRegOpnd = IR::RegOpnd::New(nullptr, RegEAX, TyMachPtr, this->func);
    IR::RegOpnd * ecxRegOpnd = IR::RegOpnd::New(nullptr, RegECX, TyVar, this->func);

    StackSym * sym = StackSym::NewParamSlotSym(1, this->func);
    this->func->SetArgOffset(sym, LowererMD::GetFormalParamOffset() * MachPtr);
    IR::SymOpnd * symOpnd = IR::SymOpnd::New(sym, TyMachPtr, this->func);
    newInstr = IR::Instr::New(Js::OpCode::MOV, eaxRegOpnd, symOpnd, this->func);
    instrAfter->InsertBefore(newInstr);

    IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(eaxRegOpnd, Js::JavascriptGenerator::GetFrameOffset(), TyMachPtr, this->func);
    newInstr = IR::Instr::New(Js::OpCode::MOV, eaxRegOpnd, indirOpnd, this->func);
    instrAfter->InsertBefore(newInstr);


    // eax points to the frame, restore stack syms and registers except eax, restore eax last

    IR::Instr * eaxRestoreInstr = nullptr;
    IR::Instr * instrInsertStackSym = instrAfter;
    IR::Instr * instrInsertRegSym = instrAfter;

    Assert(bailOutInfo->capturedValues.constantValues.Empty());
    Assert(bailOutInfo->capturedValues.copyPropSyms.Empty());

    auto restoreSymFn = [this, &eaxRegOpnd, &ecxRegOpnd, &eaxRestoreInstr, &instrInsertStackSym, &instrInsertRegSym](SymID symId)
    {
        StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
        Assert(stackSym->IsVar());

        Assert(stackSym->HasByteCodeRegSlot());
        Js::RegSlot byteCodeReg = stackSym->GetByteCodeRegSlot();
        int32 offset = byteCodeReg * sizeof(Js::Var) + Js::InterpreterStackFrame::GetOffsetOfLocals();

        IR::Opnd * srcOpnd = IR::IndirOpnd::New(eaxRegOpnd, offset, stackSym->GetType(), this->func);
        Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;

        if (lifetime->isSpilled)
        {
            // stack restores require an extra register since we can't move an indir directly to an indir on x86
            IR::Instr * instr = IR::Instr::New(Js::OpCode::MOV, ecxRegOpnd, srcOpnd, this->func);
            instrInsertStackSym->InsertBefore(instr);

            IR::SymOpnd * dstOpnd = IR::SymOpnd::New(stackSym, stackSym->GetType(), this->func);
            instr = IR::Instr::New(Js::OpCode::MOV, dstOpnd, ecxRegOpnd, this->func);
            instrInsertStackSym->InsertBefore(instr);
        }
        else
        {
            // register restores must come after stack restores so that we have EAX and ECX free to
            // use for stack restores and further EAX must be restored last since it holds the
            // pointer to the InterpreterStackFrame from which we are restoring values.
            // We must also track these restores using RecordDef in case the symbols are spilled.

            IR::RegOpnd * dstRegOpnd = IR::RegOpnd::New(stackSym, stackSym->GetType(), this->func);
            dstRegOpnd->SetReg(lifetime->reg);

            IR::Instr * instr = IR::Instr::New(Js::OpCode::MOV, dstRegOpnd, srcOpnd, this->func);
            instrInsertRegSym->InsertBefore(instr);

            if (instrInsertRegSym == instrInsertStackSym)
            {
                // this is the first register sym, make sure we don't insert stack stores
                // after this instruction so we can ensure eax and ecx remain free to use
                // for restoring spilled stack syms.
                instrInsertStackSym = instr;
            }

            if (lifetime->reg == RegEAX)
            {
                // ensure eax is restored last
                Assert(instrInsertRegSym != instrInsertStackSym);

                instrInsertRegSym = instr;

                if (eaxRestoreInstr != nullptr)
                {
                    AssertMsg(false, "this is unexpected until copy prop is enabled");
                    // eax was mapped to multiple bytecode registers.  Obviously only the first
                    // restore we do will work so change all following stores to `mov eax, eax`.
                    // We still need to keep them around for RecordDef in case the corresponding
                    // dst sym is spilled later on.
                    eaxRestoreInstr->FreeSrc1();
                    eaxRestoreInstr->SetSrc1(eaxRegOpnd);
                }

                eaxRestoreInstr = instr;
            }

            this->linearScan->RecordDef(lifetime, instr, 0);
        }
    };

    FOREACH_BITSET_IN_SPARSEBV(symId, bailOutInfo->byteCodeUpwardExposedUsed)
    {
        restoreSymFn(symId);
    }
    NEXT_BITSET_IN_SPARSEBV;

    if (bailOutInfo->capturedValues.argObjSyms)
    {
        FOREACH_BITSET_IN_SPARSEBV(symId, bailOutInfo->capturedValues.argObjSyms)
        {
            restoreSymFn(symId);
        }
        NEXT_BITSET_IN_SPARSEBV;
    }

    // Restore out params.
    // Would be nice to use restoreSymFn on a walk of the SymIds where the walk matches
    // the logic in LinearScan::FillBailOutRecord, but the walk is very complicated and
    // requires state to enumerate the exact syms that are actually mapped in the bail
    // out record.  So instead, since we have disabled most of GlobOpt for the time
    // being, just enumerate the argouts from the BailOutRecord and ignore the syms.
    // This may need to be improved to use the syms when the optimizations are brought
    // online.
    bailOutRecord->MapArgOutOffsets([this, &eaxRegOpnd, &ecxRegOpnd, &instrInsertStackSym](Js::RegSlot regSlot, int32 stackOffset) {
        // mov ecx, [eax + bytecode reg offset]
        // mov [ebp + native stack offset], ecx
        int32 regSlotOffset = Js::InterpreterStackFrame::GetOffsetOfLocals() + (this->func->GetJnFunction()->GetLocalsCount() + regSlot) * sizeof(Js::Var);
        IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(eaxRegOpnd, regSlotOffset, TyVar, this->func);
        IR::Instr * instr = IR::Instr::New(Js::OpCode::MOV, ecxRegOpnd, indirOpnd, this->func);
        instrInsertStackSym->InsertBefore(instr);

        IR::RegOpnd* ebpRegOpnd = IR::RegOpnd::New(nullptr, RegEBP, TyMachPtr, this->func);
        indirOpnd = IR::IndirOpnd::New(ebpRegOpnd, stackOffset, TyVar, this->func);
        instr = IR::Instr::New(Js::OpCode::RestoreOutParam, indirOpnd, ecxRegOpnd, this->func);
        instrInsertStackSym->InsertBefore(instr);
    });

    return instrAfter;
}

__declspec(naked) void LinearScanMD::SaveAllRegisters(BailOutRecord *const bailOutRecord)
{
    __asm
    {
        // [esp + 0 * 4] == return address
        // [esp + 1 * 4] == caller's return address
        // [esp + 2 * 4] == bailOutRecord

        push eax
        mov eax, [esp + 3 * 4] // bailOutRecord
        mov eax, [eax] // bailOutRecord->globalBailOutRecordDataTable
        mov eax, [eax] // bailOutRecord->globalBailOutRecordDataTable->registerSaveSpace

        mov [eax + (RegECX - 1) * 4], ecx
        pop ecx // saved eax
        mov [eax + (RegEAX - 1) * 4], ecx
        mov [eax + (RegEDX - 1) * 4], edx
        mov [eax + (RegEBX - 1) * 4], ebx
        // mov [rax + (RegESP - 1) * 4], esp // the stack pointer is not used by bailout, the frame pointer is used instead
        mov [eax + (RegEBP - 1) * 4], ebp
        mov [eax + (RegESI - 1) * 4], esi
        mov [eax + (RegEDI - 1) * 4], edi

        movups[eax + (RegXMM0 - 1) * 4], xmm0
        movups[eax + (RegXMM0 - 1) * 4 + (RegXMM1 - RegXMM0) * 16], xmm1
        movups[eax + (RegXMM0 - 1) * 4 + (RegXMM2 - RegXMM0) * 16], xmm2
        movups[eax + (RegXMM0 - 1) * 4 + (RegXMM3 - RegXMM0) * 16], xmm3
        movups[eax + (RegXMM0 - 1) * 4 + (RegXMM4 - RegXMM0) * 16], xmm4
        movups[eax + (RegXMM0 - 1) * 4 + (RegXMM5 - RegXMM0) * 16], xmm5
        movups[eax + (RegXMM0 - 1) * 4 + (RegXMM6 - RegXMM0) * 16], xmm6
        movups[eax + (RegXMM0 - 1) * 4 + (RegXMM7 - RegXMM0) * 16], xmm7

        // Don't pop parameters, the caller will redirect into another function call
        ret
    }
}

__declspec(naked) void LinearScanMD::SaveAllRegistersNoSse2(BailOutRecord *const bailOutRecord)
{
    __asm
    {
        // [esp + 0 * 4] == return address
        // [esp + 1 * 4] == caller's return address
        // [esp + 2 * 4] == bailOutRecord

        push eax
        mov eax, [esp + 3 * 4] // bailOutRecord
        mov eax, [eax] // bailOutRecord->globalBailOutRecordDataTable
        mov eax, [eax] // bailOutRecord->globalBailOutRecordDataTable->registerSaveSpace

        mov [eax + (RegECX - 1) * 4], ecx
        pop ecx // saved eax
        mov [eax + (RegEAX - 1) * 4], ecx
        mov [eax + (RegEDX - 1) * 4], edx
        mov [eax + (RegEBX - 1) * 4], ebx
        // mov [rax + (RegESP - 1) * 4], esp // the stack pointer is not used by bailout, the frame pointer is used instead
        mov [eax + (RegEBP - 1) * 4], ebp
        mov [eax + (RegESI - 1) * 4], esi
        mov [eax + (RegEDI - 1) * 4], edi

        // Don't pop parameters, the caller will redirect into another function call
        ret
    }
}

__declspec(naked) void LinearScanMD::SaveAllRegistersAndBailOut(BailOutRecord *const bailOutRecord)
{
    __asm
    {
        // [esp + 0 * 4] == return address
        // [esp + 1 * 4] == bailOutRecord

        call SaveAllRegisters
        jmp BailOutRecord::BailOut
    }
}

__declspec(naked) void LinearScanMD::SaveAllRegistersNoSse2AndBailOut(BailOutRecord *const bailOutRecord)
{
    __asm
    {
        // [esp + 0 * 4] == return address
        // [esp + 1 * 4] == bailOutRecord

        call SaveAllRegistersNoSse2
        jmp BailOutRecord::BailOut
    }
}

__declspec(naked) void LinearScanMD::SaveAllRegistersAndBranchBailOut(
    BranchBailOutRecord *const bailOutRecord,
    const BOOL condition)
{
    __asm
    {
        // [esp + 0 * 4] == return address
        // [esp + 1 * 4] == bailOutRecord
        // [esp + 2 * 4] == condition

        call SaveAllRegisters
        jmp BranchBailOutRecord::BailOut
    }
}

__declspec(naked) void LinearScanMD::SaveAllRegistersNoSse2AndBranchBailOut(
    BranchBailOutRecord *const bailOutRecord,
    const BOOL condition)
{
    __asm
    {
        // [esp + 0 * 4] == return address
        // [esp + 1 * 4] == bailOutRecord
        // [esp + 2 * 4] == condition

        call SaveAllRegistersNoSse2
        jmp BranchBailOutRecord::BailOut
    }
}

void
LinearScanMD::InsertOpHelperSpillAndRestores(SList<OpHelperBlock> *opHelperBlockList)
{
    FOREACH_SLIST_ENTRY(OpHelperBlock, opHelperBlock, opHelperBlockList)
    {
        this->InsertOpHelperSpillsAndRestores(opHelperBlock);
    }
    NEXT_SLIST_ENTRY;
}

void
LinearScanMD::InsertOpHelperSpillsAndRestores(OpHelperBlock const& opHelperBlock)
{
    FOREACH_SLIST_ENTRY(OpHelperSpilledLifetime, opHelperSpilledLifetime, &opHelperBlock.spilledLifetime)
    {
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
            IR::RegOpnd * regOpnd = IR::RegOpnd::New(NULL, opHelperSpilledLifetime.reg, type, this->func);
            if (!sym)
            {
                sym = EnsureSpillSymForXmmReg(regOpnd->GetReg(), this->func, type);
            }
            IR::Instr * pushInstr = IR::Instr::New(LowererMDArch::GetAssignOp(type), IR::SymOpnd::New(sym, type, this->func), regOpnd, this->func);
            opHelperBlock.opHelperLabel->InsertAfter(pushInstr);
            pushInstr->CopyNumber(opHelperBlock.opHelperLabel);

            if (opHelperSpilledLifetime.reload)
            {
                IR::Instr * popInstr = IR::Instr::New(LowererMDArch::GetAssignOp(type), regOpnd, IR::SymOpnd::New(sym, type, this->func), this->func);
                opHelperBlock.opHelperEndInstr->InsertBefore(popInstr);
                popInstr->CopyNumber(opHelperBlock.opHelperEndInstr);
            }
        }
        else
        {
            IR::RegOpnd * regOpnd;
            if (!sym)
            {
                regOpnd = IR::RegOpnd::New(NULL, opHelperSpilledLifetime.reg, TyMachPtr, this->func);
                IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::PUSH, this->func);
                pushInstr->SetSrc1(regOpnd);
                opHelperBlock.opHelperLabel->InsertAfter(pushInstr);
                pushInstr->CopyNumber(opHelperBlock.opHelperLabel);
                if (opHelperSpilledLifetime.reload)
                {
                     IR::Instr * popInstr = IR::Instr::New(Js::OpCode::POP, regOpnd, this->func);
                    opHelperBlock.opHelperEndInstr->InsertBefore(popInstr);
                    popInstr->CopyNumber(opHelperBlock.opHelperEndInstr);
                }
            }
            else
            {
                regOpnd = IR::RegOpnd::New(NULL, opHelperSpilledLifetime.reg, sym->GetType(), this->func);
                IR::Instr* instr = LowererMD::CreateAssign(IR::SymOpnd::New(sym, sym->GetType(), func), regOpnd, opHelperBlock.opHelperLabel->m_next);
                instr->CopyNumber(opHelperBlock.opHelperLabel);
                if (opHelperSpilledLifetime.reload)
                {
                    instr = LowererMD::CreateAssign(regOpnd, IR::SymOpnd::New(sym, sym->GetType(), func), opHelperBlock.opHelperEndInstr);
                    instr->CopyNumber(opHelperBlock.opHelperEndInstr);
                }
            }
        }

    }
    NEXT_SLIST_ENTRY;
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
