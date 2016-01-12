//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

void
GlobOpt::CaptureValue(BasicBlock *block, StackSym * stackSym, Value * value, BailOutInfo * bailOutInfo)
{
    if (!this->func->DoGlobOptsForGeneratorFunc())
    {
        // TODO[generators][ianhall]: Enable constprop and copyprop for generator functions; see GlobOpt::CopyProp()
        // Even though CopyProp is disabled for generator functions we must also not put the copy-prop sym into the
        // bailOutInfo so that the bailOutInfo keeps track of the key sym in its byteCodeUpwardExposed list.
        return;
    }

    ValueInfo * valueInfo = value->GetValueInfo();
    Assert(stackSym->HasByteCodeRegSlot() || stackSym->HasArgSlotNum());
    Assert(!stackSym->IsTypeSpec());
    int32 intConstantValue;
    if (valueInfo->TryGetIntConstantValue(&intConstantValue))
    {
        BailoutConstantValue constValue;
        constValue.InitIntConstValue(intConstantValue);
        bailOutInfo->capturedValues.constantValues.PrependNode(this->func->m_alloc, stackSym, constValue);
    }
    else if (valueInfo->IsVarConstant())
    {
        BailoutConstantValue constValue;
        constValue.InitVarConstValue(valueInfo->AsVarConstant()->VarValue());
        bailOutInfo->capturedValues.constantValues.PrependNode(this->func->m_alloc, stackSym, constValue);
    }
    else
    {
        StackSym * copyPropSym = this->GetCopyPropSym(block, stackSym, value);
        if (copyPropSym)
        {
            bailOutInfo->capturedValues.copyPropSyms.PrependNode(this->func->m_alloc, stackSym, copyPropSym);
        }
    }
}

void
GlobOpt::CaptureValues(BasicBlock *block, BailOutInfo * bailOutInfo)
{
    FOREACH_GLOBHASHTABLE_ENTRY(bucket, block->globOptData.symToValueMap)
    {
        Value* value = bucket.element;
        ValueInfo * valueInfo = value->GetValueInfo();

        if (valueInfo->GetSymStore() == nullptr && !valueInfo->HasIntConstantValue())
        {
            continue;
        }
        Sym * sym = bucket.value;
        if (sym == nullptr || !sym->IsStackSym() || !sym->AsStackSym()->HasByteCodeRegSlot())
        {
            continue;
        }
        this->CaptureValue(block, sym->AsStackSym(), value, bailOutInfo);
    }
    NEXT_GLOBHASHTABLE_ENTRY;
}

void
GlobOpt::CaptureArguments(BasicBlock *block, BailOutInfo * bailOutInfo, JitArenaAllocator *allocator)
{
    FOREACH_BITSET_IN_SPARSEBV(id, this->blockData.argObjSyms)
    {
        StackSym * stackSym = this->func->m_symTable->FindStackSym(id);
        Assert(stackSym != nullptr);
        if (!stackSym->HasByteCodeRegSlot())
        {
            continue;
        }

        if (!bailOutInfo->capturedValues.argObjSyms)
        {
            bailOutInfo->capturedValues.argObjSyms = JitAnew(allocator, BVSparse<JitArenaAllocator>, allocator);
        }

        bailOutInfo->capturedValues.argObjSyms->Set(id);
        // Add to BailOutInfo
    }
    NEXT_BITSET_IN_SPARSEBV
}

void
GlobOpt::TrackByteCodeSymUsed(IR::Instr * instr, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed, PropertySym **pPropertySym)
{
    IR::Opnd * src = instr->GetSrc1();
    if (src)
    {
        TrackByteCodeSymUsed(src, instrByteCodeStackSymUsed, pPropertySym);
        src = instr->GetSrc2();
        if (src)
        {
            TrackByteCodeSymUsed(src, instrByteCodeStackSymUsed, pPropertySym);
        }
    }

#if DBG
    // There should be no more than one property sym used.
    PropertySym *propertySymFromSrc = *pPropertySym;
#endif

    IR::Opnd * dst = instr->GetDst();
    if (dst)
    {
        StackSym *stackSym = dst->GetStackSym();

        // We want stackSym uses: IndirOpnd and SymOpnds of propertySyms.
        // RegOpnd and SymOPnd of StackSyms are stack sym defs.
        if (stackSym == NULL)
        {
            TrackByteCodeSymUsed(dst, instrByteCodeStackSymUsed, pPropertySym);
        }
    }

#if DBG
    AssertMsg(propertySymFromSrc == NULL || propertySymFromSrc == *pPropertySym,
              "Lost a property sym use?");
#endif
}

void
GlobOpt::TrackByteCodeSymUsed(IR::RegOpnd * regOpnd, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed)
{
    // Check JITOptimizedReg to catch case where baseOpnd of indir was optimized.
    if (!regOpnd->GetIsJITOptimizedReg())
    {
        TrackByteCodeSymUsed(regOpnd->m_sym, instrByteCodeStackSymUsed);
    }
}

void
GlobOpt::TrackByteCodeSymUsed(IR::Opnd * opnd, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed, PropertySym **pPropertySym)
{
    if (opnd->GetIsJITOptimizedReg())
    {
        AssertMsg(!opnd->IsIndirOpnd(), "TrackByteCodeSymUsed doesn't expect IndirOpnd with IsJITOptimizedReg turned on");
        return;
    }

    switch(opnd->GetKind())
    {
    case IR::OpndKindReg:
        TrackByteCodeSymUsed(opnd->AsRegOpnd(), instrByteCodeStackSymUsed);
        break;
    case IR::OpndKindSym:
        {
            Sym * sym = opnd->AsSymOpnd()->m_sym;
            if (sym->IsStackSym())
            {
                TrackByteCodeSymUsed(sym->AsStackSym(), instrByteCodeStackSymUsed);
            }
            else
            {
                TrackByteCodeSymUsed(sym->AsPropertySym()->m_stackSym, instrByteCodeStackSymUsed);
                *pPropertySym = sym->AsPropertySym();
            }
        }
        break;
    case IR::OpndKindIndir:
        TrackByteCodeSymUsed(opnd->AsIndirOpnd()->GetBaseOpnd(), instrByteCodeStackSymUsed);
        {
            IR::RegOpnd * indexOpnd = opnd->AsIndirOpnd()->GetIndexOpnd();
            if (indexOpnd)
            {
                TrackByteCodeSymUsed(indexOpnd, instrByteCodeStackSymUsed);
            }
        }
        break;
    }
}

void
GlobOpt::TrackByteCodeSymUsed(StackSym * sym, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed)
{
    // We only care about stack sym that has a corresponding byte code register
    if (sym->HasByteCodeRegSlot())
    {
        if (sym->IsTypeSpec())
        {
            // It has to have a var version for byte code regs
            sym = sym->GetVarEquivSym(nullptr);
        }
        instrByteCodeStackSymUsed->Set(sym->m_id);
    }
}

void
GlobOpt::MarkNonByteCodeUsed(IR::Instr * instr)
{
    IR::Opnd * dst = instr->GetDst();
    if (dst)
    {
        MarkNonByteCodeUsed(dst);
    }

    IR::Opnd * src1 = instr->GetSrc1();
    if (src1)
    {
        MarkNonByteCodeUsed(src1);
        IR::Opnd * src2 = instr->GetSrc2();
        if (src2)
        {
            MarkNonByteCodeUsed(src2);
        }
    }
}

void
GlobOpt::MarkNonByteCodeUsed(IR::Opnd * opnd)
{
    switch(opnd->GetKind())
    {
    case IR::OpndKindReg:
        opnd->AsRegOpnd()->SetIsJITOptimizedReg(true);
        break;
    case IR::OpndKindIndir:
        opnd->AsIndirOpnd()->GetBaseOpnd()->SetIsJITOptimizedReg(true);
        {
            IR::RegOpnd * indexOpnd = opnd->AsIndirOpnd()->GetIndexOpnd();
            if (indexOpnd)
            {
                indexOpnd->SetIsJITOptimizedReg(true);
            }
        }
        break;
    }
}

void
GlobOpt::CaptureByteCodeSymUses(IR::Instr * instr)
{
    if (this->byteCodeUses)
    {
        // We already captured it before.
        return;
    }
    Assert(this->propertySymUse == NULL);
    this->byteCodeUses = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
    GlobOpt::TrackByteCodeSymUsed(instr, this->byteCodeUses, &this->propertySymUse);

    AssertMsg(this->byteCodeUses->Equal(this->byteCodeUsesBeforeOpt),
        "Instruction edited before capturing the byte code use");
}

void
GlobOpt::TrackCalls(IR::Instr * instr)
{
    // Keep track of out params for bailout
    switch (instr->m_opcode)
    {
    case Js::OpCode::StartCall:
        Assert(!this->isCallHelper);
        Assert(instr->GetDst()->IsRegOpnd());
        Assert(instr->GetDst()->AsRegOpnd()->m_sym->m_isSingleDef);

        if (this->blockData.callSequence == nullptr)
        {
            this->blockData.callSequence = JitAnew(this->alloc, SListBase<IR::Opnd *>);
            this->currentBlock->globOptData.callSequence = this->blockData.callSequence;
        }
        this->blockData.callSequence->Prepend(this->alloc, instr->GetDst());

        this->currentBlock->globOptData.totalOutParamCount += instr->GetArgOutCount(/*getInterpreterArgOutCount*/ true);
        this->currentBlock->globOptData.startCallCount++;

        break;
    case Js::OpCode::BytecodeArgOutCapture:
        {
            this->blockData.callSequence->Prepend(this->alloc, instr->GetDst());
            this->currentBlock->globOptData.argOutCount++;
            break;
        }
    case Js::OpCode::ArgOut_A:
    case Js::OpCode::ArgOut_A_Inline:
    case Js::OpCode::ArgOut_A_FixupForStackArgs:
    case Js::OpCode::ArgOut_A_InlineBuiltIn:
    case Js::OpCode::ArgOut_A_Dynamic:
    case Js::OpCode::ArgOut_A_FromStackArgs:
    case Js::OpCode::ArgOut_A_SpreadArg:
    {
        IR::Opnd * opnd = instr->GetDst();
        if (opnd->IsSymOpnd())
        {
            Assert(!this->isCallHelper);
            Assert(!this->blockData.callSequence->Empty());
            StackSym* stackSym = opnd->AsSymOpnd()->m_sym->AsStackSym();

            // These scenarios are already tracked using BytecodeArgOutCapture,
            // and we don't want to be tracking ArgOut_A_FixupForStackArgs as these are only visible to the JIT and we should not be restoring them upon bailout.
            if (!stackSym->m_isArgCaptured && instr->m_opcode != Js::OpCode::ArgOut_A_FixupForStackArgs)
            {
                this->blockData.callSequence->Prepend(this->alloc, instr->GetDst());
                this->currentBlock->globOptData.argOutCount++;
            }
            Assert(stackSym->IsArgSlotSym());
            if (stackSym->m_isInlinedArgSlot)
            {
                this->currentBlock->globOptData.inlinedArgOutCount++;
                // We want to update the offsets only once: don't do in prepass.
                if (!this->IsLoopPrePass() && stackSym->m_offset >= 0)
                {
                    Func * currentFunc = instr->m_func;
                    stackSym->FixupStackOffset(currentFunc);
                }
            }
        }
        else
        {
            // It is a reg opnd if it is a helper call
            // It should be all ArgOut until the CallHelper instruction
            Assert(opnd->IsRegOpnd());
            this->isCallHelper = true;
        }

        if (instr->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs && !this->IsLoopPrePass())
        {
            instr->m_opcode = Js::OpCode::ArgOut_A_Inline;
        }
        break;
    }

    case Js::OpCode::InlineeStart:
        Assert(instr->m_func->GetParentFunc() == this->blockData.curFunc);
        Assert(instr->m_func->GetParentFunc());
        this->blockData.curFunc = instr->m_func;
        this->currentBlock->globOptData.curFunc = instr->m_func;

        this->EndTrackCall(instr);

        if (DoInlineArgsOpt(instr->m_func))
        {
            instr->m_func->m_hasInlineArgsOpt = true;
            InlineeFrameInfo* frameInfo = InlineeFrameInfo::New(func->m_alloc);
            instr->m_func->frameInfo = frameInfo;
            frameInfo->floatSyms = currentBlock->globOptData.liveFloat64Syms->CopyNew(this->alloc);
            frameInfo->intSyms = currentBlock->globOptData.liveInt32Syms->MinusNew(currentBlock->globOptData.liveLossyInt32Syms, this->alloc);

            // SIMD_JS
            frameInfo->simd128F4Syms = currentBlock->globOptData.liveSimd128F4Syms->CopyNew(this->alloc);
            frameInfo->simd128I4Syms = currentBlock->globOptData.liveSimd128I4Syms->CopyNew(this->alloc);
        }
        break;

    case Js::OpCode::EndCallForPolymorphicInlinee:
        // Have this opcode mimic the functions of both InlineeStart and InlineeEnd in the bailout block of a polymorphic call inlined using fixed methods.
        this->EndTrackCall(instr);
        break;

    case Js::OpCode::CallHelper:
    case Js::OpCode::IsInst:
        Assert(this->isCallHelper);
        this->isCallHelper = false;
        break;

    case Js::OpCode::BailOnNoProfile:
    case Js::OpCode::InlineThrow:
    case Js::OpCode::InlineRuntimeTypeError:
    case Js::OpCode::InlineRuntimeReferenceError:
        //We are not going to see an  inlinee end
        this->func->UpdateMaxInlineeArgOutCount(this->currentBlock->globOptData.inlinedArgOutCount);
        break;

    case Js::OpCode::InlineeEnd:
        if (instr->m_func->m_hasInlineArgsOpt)
        {
            RecordInlineeFrameInfo(instr);
        }
        EndTrackingOfArgObjSymsForInlinee();

        Assert(this->currentBlock->globOptData.inlinedArgOutCount >= instr->GetArgOutCount(/*getInterpreterArgOutCount*/ false));
        this->func->UpdateMaxInlineeArgOutCount(this->currentBlock->globOptData.inlinedArgOutCount);
        this->currentBlock->globOptData.inlinedArgOutCount -= instr->GetArgOutCount(/*getInterpreterArgOutCount*/ false);
        break;

    case Js::OpCode::InlineeMetaArg:
    {
        Assert(instr->GetDst()->IsSymOpnd());
        StackSym * stackSym = instr->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
        Assert(stackSym->IsArgSlotSym());

        // InlineeMetaArg has the m_func set as the "inlinee" and not the "inliner"
        // TODO: Review this and fix the m_func of InlineeMetaArg to be "inliner" (as for the rest of the ArgOut's)
        // We want to update the offsets only once: don't do in prepass.
        if (!this->IsLoopPrePass())
        {
            Func * currentFunc = instr->m_func->GetParentFunc();
            stackSym->FixupStackOffset(currentFunc);
        }
        this->currentBlock->globOptData.inlinedArgOutCount++;
        break;
    }

    case Js::OpCode::InlineBuiltInStart:
        this->inInlinedBuiltIn = true;
        break;

    case Js::OpCode::InlineNonTrackingBuiltInEnd:
    case Js::OpCode::InlineBuiltInEnd:
    {
        // If extra bailouts were added for the InlineMathXXX call itself,
        // move InlineeBuiltInStart just above the InlineMathXXX.
        // This is needed so that the function argument has lifetime after all bailouts for InlineMathXXX,
        // otherwise when we bailout we would get wrong function.
        IR::Instr* inlineBuiltInStartInstr = instr->m_prev;
        while (inlineBuiltInStartInstr->m_opcode != Js::OpCode::InlineBuiltInStart)
        {
            inlineBuiltInStartInstr = inlineBuiltInStartInstr->m_prev;
        }

        IR::Instr *byteCodeUsesInstr = inlineBuiltInStartInstr->m_prev;
        IR::Instr * insertBeforeInstr = instr->m_prev;
        IR::Instr * tmpInstr = insertBeforeInstr;
        while(tmpInstr->m_opcode != Js::OpCode::InlineBuiltInStart )
        {
            if(tmpInstr->m_opcode == Js::OpCode::ByteCodeUses)
            {
                insertBeforeInstr = tmpInstr;
            }
            tmpInstr = tmpInstr->m_prev;
        }
        inlineBuiltInStartInstr->Unlink();
        if(insertBeforeInstr == instr->m_prev)
        {
            insertBeforeInstr->InsertBefore(inlineBuiltInStartInstr);
        }

        else
        {
            insertBeforeInstr->m_prev->InsertBefore(inlineBuiltInStartInstr);
        }

        // Need to move the byte code uses instructions associated with inline built-in start instruction as well. For instance,
        // copy-prop may have replaced the function sym and inserted a byte code uses for the original sym holding the function.
        // That byte code uses instruction needs to appear after bailouts inserted for the InlinMathXXX instruction since the
        // byte code register holding the function object needs to be restored on bailout.
        IR::Instr *const insertByteCodeUsesAfterInstr = inlineBuiltInStartInstr->m_prev;
        if(byteCodeUsesInstr != insertByteCodeUsesAfterInstr)
        {
            // The InlineBuiltInStart instruction was moved, look for its ByteCodeUses instructions that also need to be moved
            while(
                byteCodeUsesInstr->IsByteCodeUsesInstr() &&
                byteCodeUsesInstr->AsByteCodeUsesInstr()->GetByteCodeOffset() == inlineBuiltInStartInstr->GetByteCodeOffset())
            {
                IR::Instr *const instrToMove = byteCodeUsesInstr;
                byteCodeUsesInstr = byteCodeUsesInstr->m_prev;
                instrToMove->Unlink();
                insertByteCodeUsesAfterInstr->InsertAfter(instrToMove);
            }
        }

        // The following code makes more sense to be processed when we hit InlineeBuiltInStart,
        // but when extra bailouts are added for the InlineMathXXX and InlineArrayPop instructions itself, those bailouts
        // need to know about current bailout record, but since they are added after TrackCalls is called
        // for InlineeBuiltInStart, we can't clear current record when got InlineeBuiltInStart

        // Do not track calls for InlineNonTrackingBuiltInEnd, as it is already tracked for InlineArrayPop
        if(instr->m_opcode == Js::OpCode::InlineBuiltInEnd)
        {
            this->EndTrackCall(instr);
        }

        Assert(this->currentBlock->globOptData.inlinedArgOutCount >= instr->GetArgOutCount(/*getInterpreterArgOutCount*/ false));
        this->currentBlock->globOptData.inlinedArgOutCount -= instr->GetArgOutCount(/*getInterpreterArgOutCount*/ false);

        this->inInlinedBuiltIn = false;
        break;
    }

    case Js::OpCode::InlineArrayPop:
    {
        // EndTrackCall should be called here as the Post-op BailOutOnImplicitCalls will bail out to the instruction after the Pop function call instr.
        // This bailout shouldn't be tracking the call sequence as it will then erroneously reserve stack space for arguments when the call would have already happened
        // Can't wait till InlineBuiltinEnd like we do for other InlineMathXXX because by then we would have filled bailout info for the BailOutOnImplicitCalls for InlineArrayPop.
        this->EndTrackCall(instr);
        break;
    }

    default:
        if (OpCodeAttr::CallInstr(instr->m_opcode))
        {
            this->EndTrackCall(instr);
            if (this->inInlinedBuiltIn && instr->m_opcode == Js::OpCode::CallDirect)
            {
                // We can end up in this situation when a built-in apply target is inlined to a CallDirect. We have the following IR:
                //
                // StartCall
                // ArgOut_InlineBuiltIn
                // ArgOut_InlineBuiltIn
                // ArgOut_InlineBuiltIn
                // InlineBuiltInStart
                //      ArgOut_A_InlineSpecialized
                //      ArgOut_A
                //      ArgOut_A
                //      CallDirect
                // InlineNonTrackingBuiltInEnd
                //
                // We need to call EndTrackCall twice for CallDirect in this case. The CallDirect may get a BailOutOnImplicitCalls later,
                // but it should not be tracking the call sequence for the apply call as it is a post op bailout and the call would have
                // happened when we bail out.
                // Can't wait till InlineBuiltinEnd like we do for other InlineMathXXX because by then we would have filled bailout info for the BailOutOnImplicitCalls for CallDirect.
                this->EndTrackCall(instr);
            }
        }
        break;
    }
}

void GlobOpt::RecordInlineeFrameInfo(IR::Instr* inlineeEnd)
{
    if (this->IsLoopPrePass())
    {
        return;
    }
    InlineeFrameInfo* frameInfo = inlineeEnd->m_func->frameInfo;
    if (frameInfo->isRecorded)
    {
        Assert(frameInfo->function.type != InlineeFrameInfoValueType_None);
        // Due to Cmp peeps in flow graph - InlineeEnd can be cloned.
        return;
    }
    inlineeEnd->IterateArgInstrs([=] (IR::Instr* argInstr)
    {
        if (argInstr->m_opcode == Js::OpCode::InlineeStart)
        {
            Assert(frameInfo->function.type == InlineeFrameInfoValueType_None);
            IR::RegOpnd* functionObject = argInstr->GetSrc1()->AsRegOpnd();
            if (functionObject->m_sym->IsConst())
            {
                frameInfo->function = InlineFrameInfoValue(functionObject->m_sym->GetConstValueForBailout());
            }
            else
            {
                frameInfo->function = InlineFrameInfoValue(functionObject->m_sym);
            }
        }
        else
        {
            Js::ArgSlot argSlot = argInstr->GetDst()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum();
            IR::Opnd* argOpnd = argInstr->GetSrc1();
            InlineFrameInfoValue frameInfoValue;
            StackSym* argSym = argOpnd->GetStackSym();
            if (!argSym)
            {
                frameInfoValue = InlineFrameInfoValue(argOpnd->GetConstValue());
            }
            else if (argSym->IsConst())
            {
                frameInfoValue = InlineFrameInfoValue(argSym->GetConstValueForBailout());
            }
            else
            {
                if (PHASE_ON(Js::CopyPropPhase, func))
                {
                    Value* value = FindValue(argSym);

                    StackSym * copyPropSym = this->GetCopyPropSym(this->currentBlock, argSym, value);
                    if (copyPropSym)
                    {
                        argSym = copyPropSym;
                    }
                }

                GlobOptBlockData& globOptData = this->currentBlock->globOptData;

                if (frameInfo->intSyms->TestEmpty() && frameInfo->intSyms->Test(argSym->m_id))
                {
                    // Var version of the sym is not live, use the int32 version
                    argSym = argSym->GetInt32EquivSym(nullptr);
                    Assert(argSym);
                }
                else if (frameInfo->floatSyms->TestEmpty() && frameInfo->floatSyms->Test(argSym->m_id))
                {
                    // Var/int32 version of the sym is not live, use the float64 version
                    argSym = argSym->GetFloat64EquivSym(nullptr);
                    Assert(argSym);
                }
                // SIMD_JS
                else if (frameInfo->simd128F4Syms->TestEmpty() && frameInfo->simd128F4Syms->Test(argSym->m_id))
                {
                    argSym = argSym->GetSimd128F4EquivSym(nullptr);
                }
                else if (frameInfo->simd128I4Syms->TestEmpty() && frameInfo->simd128I4Syms->Test(argSym->m_id))
                {
                    argSym = argSym->GetSimd128I4EquivSym(nullptr);
                }
                else
                {
                    Assert(globOptData.liveVarSyms->Test(argSym->m_id));
                }

                if (argSym->IsConst())
                {
                    frameInfoValue = InlineFrameInfoValue(argSym->GetConstValueForBailout());
                }
                else
                {
                    frameInfoValue = InlineFrameInfoValue(argSym);
                }
            }
            Assert(argSlot >= 1);
            frameInfo->arguments->SetItem(argSlot - 1, frameInfoValue);
        }
        return false;
    });

    JitAdelete(this->alloc, frameInfo->intSyms);
    frameInfo->intSyms = nullptr;
    JitAdelete(this->alloc, frameInfo->floatSyms);
    frameInfo->floatSyms = nullptr;

    // SIMD_JS
    JitAdelete(this->alloc, frameInfo->simd128F4Syms);
    frameInfo->simd128F4Syms = nullptr;
    JitAdelete(this->alloc, frameInfo->simd128I4Syms);
    frameInfo->simd128I4Syms = nullptr;

    frameInfo->isRecorded = true;
}

void GlobOpt::EndTrackingOfArgObjSymsForInlinee()
{
    Assert(this->blockData.curFunc->GetParentFunc());
    if (this->blockData.curFunc->argObjSyms && TrackArgumentsObject())
    {
        BVSparse<JitArenaAllocator> * tempBv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
        tempBv->Minus(this->blockData.curFunc->argObjSyms, this->blockData.argObjSyms);
        if(!tempBv->IsEmpty())
        {
            // This means there are arguments object symbols in the current function which are not in the current block.
            // This could happen when one of the blocks has a throw and arguments object aliased in it and other blocks don't see it.
            // Rare case, abort stack arguments optimization in this case.
            CannotAllocateArgumentsObjectOnStack();
        }
        else
        {
            Assert(this->blockData.argObjSyms->OrNew(this->blockData.curFunc->argObjSyms)->Equal(this->blockData.argObjSyms));
            this->blockData.argObjSyms->Minus(this->blockData.curFunc->argObjSyms);
        }
        JitAdelete(this->tempAlloc, tempBv);
    }
    this->blockData.curFunc = this->blockData.curFunc->GetParentFunc();
    this->currentBlock->globOptData.curFunc = this->blockData.curFunc;
}

void GlobOpt::EndTrackCall(IR::Instr* instr)
{
    Assert(instr);
    Assert(OpCodeAttr::CallInstr(instr->m_opcode) || instr->m_opcode == Js::OpCode::InlineeStart || instr->m_opcode == Js::OpCode::InlineBuiltInEnd
        || instr->m_opcode == Js::OpCode::InlineArrayPop || instr->m_opcode == Js::OpCode::EndCallForPolymorphicInlinee);

    Assert(!this->isCallHelper);
    Assert(!this->blockData.callSequence->Empty());


#if DBG
    uint origArgOutCount = this->currentBlock->globOptData.argOutCount;
#endif
    while (this->blockData.callSequence->Head()->GetStackSym()->HasArgSlotNum())
    {
        this->currentBlock->globOptData.argOutCount--;
        this->blockData.callSequence->RemoveHead(this->alloc);
    }
    StackSym * sym = this->blockData.callSequence->Head()->AsRegOpnd()->m_sym->AsStackSym();
    this->blockData.callSequence->RemoveHead(this->alloc);

#if DBG
    Assert(sym->m_isSingleDef);
    Assert(sym->m_instrDef->m_opcode == Js::OpCode::StartCall);

    // Number of argument set should be the same as indicated at StartCall
    // except NewScObject has an implicit arg1
    Assert((uint)sym->m_instrDef->GetArgOutCount(/*getInterpreterArgOutCount*/ true) ==
        origArgOutCount - this->currentBlock->globOptData.argOutCount +
           (instr->m_opcode == Js::OpCode::NewScObject || instr->m_opcode == Js::OpCode::NewScObjArray
           || instr->m_opcode == Js::OpCode::NewScObjectSpread || instr->m_opcode == Js::OpCode::NewScObjArraySpread));

#endif

    this->currentBlock->globOptData.totalOutParamCount -= sym->m_instrDef->GetArgOutCount(/*getInterpreterArgOutCount*/ true);
    this->currentBlock->globOptData.startCallCount--;
}

void
GlobOpt::FillBailOutInfo(BasicBlock *block, BailOutInfo * bailOutInfo)
{
    AssertMsg(!this->isCallHelper, "Bail out can't be inserted the middle of CallHelper sequence");

    bailOutInfo->liveVarSyms = block->globOptData.liveVarSyms->CopyNew(this->func->m_alloc);
    bailOutInfo->liveFloat64Syms = block->globOptData.liveFloat64Syms->CopyNew(this->func->m_alloc);
    // SIMD_JS
    bailOutInfo->liveSimd128F4Syms = block->globOptData.liveSimd128F4Syms->CopyNew(this->func->m_alloc);
    bailOutInfo->liveSimd128I4Syms = block->globOptData.liveSimd128I4Syms->CopyNew(this->func->m_alloc);

    // The live int32 syms in the bailout info are only the syms resulting from lossless conversion to int. If the int32 value
    // was created from a lossy conversion to int, the original var value cannot be re-materialized from the int32 value. So, the
    // int32 version is considered to be not live for the purposes of bailout, which forces the var or float versions to be used
    // directly for restoring the value during bailout. Otherwise, bailout may try to re-materialize the var value by converting
    // the lossily-converted int value back into a var, restoring the wrong value.
    bailOutInfo->liveLosslessInt32Syms =
        block->globOptData.liveInt32Syms->MinusNew(block->globOptData.liveLossyInt32Syms, this->func->m_alloc);

    // Save the stack literal init field count so we can null out the uninitialized fields
    StackLiteralInitFldDataMap * stackLiteralInitFldDataMap = block->globOptData.stackLiteralInitFldDataMap;
    if (stackLiteralInitFldDataMap != nullptr)
    {
        uint stackLiteralInitFldDataCount = stackLiteralInitFldDataMap->Count();
        if (stackLiteralInitFldDataCount != 0)
        {
            auto stackLiteralBailOutInfo = AnewArray(this->func->m_alloc,
                BailOutInfo::StackLiteralBailOutInfo, stackLiteralInitFldDataCount);
            uint i = 0;
            stackLiteralInitFldDataMap->Map(
                [stackLiteralBailOutInfo, stackLiteralInitFldDataCount, &i](StackSym * stackSym, StackLiteralInitFldData const& data)
            {
                Assert(i < stackLiteralInitFldDataCount);
                stackLiteralBailOutInfo[i].stackSym = stackSym;
                stackLiteralBailOutInfo[i].initFldCount = data.currentInitFldCount;
                i++;
            });

            Assert(i == stackLiteralInitFldDataCount);
            bailOutInfo->stackLiteralBailOutInfoCount = stackLiteralInitFldDataCount;
            bailOutInfo->stackLiteralBailOutInfo = stackLiteralBailOutInfo;
        }
    }

    // Save the constant values that we know so we can restore them directly.
    // This allows us to dead store the constant value assign.
    this->CaptureValues(block, bailOutInfo);

    if (TrackArgumentsObject())
    {
        this->CaptureArguments(block, bailOutInfo, this->func->m_alloc);
    }

    if (block->globOptData.callSequence && !block->globOptData.callSequence->Empty())
    {
        uint currentArgOutCount = 0;
        uint startCallNumber = block->globOptData.startCallCount;

        bailOutInfo->startCallInfo = JitAnewArray(this->func->m_alloc, BailOutInfo::StartCallInfo, startCallNumber);
        bailOutInfo->startCallCount = startCallNumber;

        // Save the start call's func to identify the function (inlined) that the call sequence is for
        // We might not have any arg out yet to get the function from
        bailOutInfo->startCallFunc = JitAnewArray(this->func->m_alloc, Func *, startCallNumber);
#ifdef _M_IX86
        bailOutInfo->inlinedStartCall = BVFixed::New(startCallNumber, this->func->m_alloc, false);
#endif
        uint totalOutParamCount = block->globOptData.totalOutParamCount;
        bailOutInfo->totalOutParamCount = totalOutParamCount;
        bailOutInfo->argOutSyms = JitAnewArrayZ(this->func->m_alloc, StackSym *, totalOutParamCount);

        uint argRestoreAdjustCount = 0;
        FOREACH_SLISTBASE_ENTRY(IR::Opnd *, opnd, block->globOptData.callSequence)
        {
            if(opnd->GetStackSym()->HasArgSlotNum())
            {
                StackSym * sym;
                if(opnd->IsSymOpnd())
                {
                    sym = opnd->AsSymOpnd()->m_sym->AsStackSym();
                    Assert(sym->IsArgSlotSym());
                    Assert(sym->m_isSingleDef);
                    Assert(sym->m_instrDef->m_opcode == Js::OpCode::ArgOut_A
                        || sym->m_instrDef->m_opcode == Js::OpCode::ArgOut_A_Inline
                        || sym->m_instrDef->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn
                        || sym->m_instrDef->m_opcode == Js::OpCode::ArgOut_A_SpreadArg
                        || sym->m_instrDef->m_opcode == Js::OpCode::ArgOut_A_Dynamic);
                }
                else
                {
                    sym = opnd->GetStackSym();
                    Value* val = FindValue(sym);
                    Assert(val);
                    CaptureValue(block, sym, val, bailOutInfo);
                }

                Assert(totalOutParamCount != 0);
                Assert(totalOutParamCount > currentArgOutCount);
                currentArgOutCount++;
#pragma prefast(suppress:26000, "currentArgOutCount is never 0");
                bailOutInfo->argOutSyms[totalOutParamCount - currentArgOutCount] = sym;
                // Note that there could be ArgOuts below current bailout instr that belong to current call (currentArgOutCount < argOutCount),
                // in which case we will have nulls in argOutSyms[] in start of section for current call, because we fill from tail.
                // Example: StartCall 3, ArgOut1,.. ArgOut2, Bailout,.. Argout3 -> [NULL, ArgOut1, ArgOut2].
            }
            else
            {
                Assert(opnd->IsRegOpnd());
                StackSym * sym = opnd->AsRegOpnd()->m_sym;
                Assert(!sym->IsArgSlotSym());
                Assert(sym->m_isSingleDef);
                Assert(sym->m_instrDef->m_opcode == Js::OpCode::StartCall);

                Assert(startCallNumber != 0);
                startCallNumber--;

                bailOutInfo->startCallFunc[startCallNumber] = sym->m_instrDef->m_func;
#ifdef _M_IX86
                if (this->currentRegion && this->currentRegion->GetType() == RegionTypeTry)
                {
                    // For a bailout in argument evaluation from an EH region, the esp is offset by the TryCatch helper’s frame. So, the argouts are not actually pushed at the
                    // offsets stored in the bailout record, which are relative to ebp. Need to restore the argouts from the actual value of esp before calling the Bailout helper.
                    // For nested calls, argouts for the outer call need to be restored from an offset of stack-adjustment-done-by-the-inner-call from esp.
                    if (startCallNumber + 1 == bailOutInfo->startCallCount)
                    {
                        argRestoreAdjustCount = 0;
                    }
                    else
                    {
                        argRestoreAdjustCount = bailOutInfo->startCallInfo[startCallNumber + 1].argRestoreAdjustCount + bailOutInfo->startCallInfo[startCallNumber + 1].argCount;
                        if ((Math::Align<int32>(bailOutInfo->startCallInfo[startCallNumber + 1].argCount * MachPtr, MachStackAlignment) - (bailOutInfo->startCallInfo[startCallNumber + 1].argCount * MachPtr)) != 0)
                        {
                            argRestoreAdjustCount++;
                        }
                    }
                }

                if (sym->m_isInlinedArgSlot)
                {
                    bailOutInfo->inlinedStartCall->Set(startCallNumber);
                }
#endif
                uint argOutCount = sym->m_instrDef->GetArgOutCount(/*getInterpreterArgOutCount*/ true);
                Assert(totalOutParamCount >= argOutCount);
                Assert(argOutCount >= currentArgOutCount);

                bailOutInfo->RecordStartCallInfo(startCallNumber, argRestoreAdjustCount, sym->m_instrDef);
                totalOutParamCount -= argOutCount;
                currentArgOutCount = 0;

            }
        }
        NEXT_SLISTBASE_ENTRY;

        Assert(totalOutParamCount == 0);
        Assert(startCallNumber == 0);
        Assert(currentArgOutCount == 0);
    }
}

IR::ByteCodeUsesInstr *
GlobOpt::InsertByteCodeUses(IR::Instr * instr, bool includeDef)
{
    IR::ByteCodeUsesInstr * byteCodeUsesInstr = nullptr;
    Assert(this->byteCodeUses);
    IR::RegOpnd * dstOpnd = nullptr;
    if (includeDef)
    {
        IR::Opnd * opnd = instr->GetDst();
        if (opnd && opnd->IsRegOpnd())
        {
            dstOpnd = opnd->AsRegOpnd();
            if (dstOpnd->GetIsJITOptimizedReg() || !dstOpnd->m_sym->HasByteCodeRegSlot())
            {
                dstOpnd = nullptr;
            }
        }
    }
    if (!this->byteCodeUses->IsEmpty() || this->propertySymUse || dstOpnd != nullptr)
    {
        byteCodeUsesInstr = IR::ByteCodeUsesInstr::New(instr->m_func);
        byteCodeUsesInstr->SetByteCodeOffset(instr);
        if (!this->byteCodeUses->IsEmpty())
        {
            byteCodeUsesInstr->byteCodeUpwardExposedUsed = byteCodeUses->CopyNew(instr->m_func->m_alloc);
        }
        if (dstOpnd != nullptr)
        {
            byteCodeUsesInstr->SetFakeDst(dstOpnd);
        }
        if (this->propertySymUse)
        {
            byteCodeUsesInstr->propertySymUse = this->propertySymUse;
        }
        instr->InsertBefore(byteCodeUsesInstr);
    }

    JitAdelete(this->alloc, this->byteCodeUses);
    this->byteCodeUses = nullptr;
    this->propertySymUse = nullptr;
    return byteCodeUsesInstr;
}

IR::ByteCodeUsesInstr *
GlobOpt::ConvertToByteCodeUses(IR::Instr * instr)
{
#if DBG
    PropertySym *propertySymUseBefore = NULL;
    Assert(this->byteCodeUses == nullptr);
    this->byteCodeUsesBeforeOpt->ClearAll();
    GlobOpt::TrackByteCodeSymUsed(instr, this->byteCodeUsesBeforeOpt, &propertySymUseBefore);
#endif
    this->CaptureByteCodeSymUses(instr);
    IR::ByteCodeUsesInstr * byteCodeUsesInstr = this->InsertByteCodeUses(instr, true);
    instr->Remove();
    return byteCodeUsesInstr;
}

bool
GlobOpt::MayNeedBailOut(Loop * loop) const
{
    Assert(this->IsLoopPrePass());
    return loop->CanHoistInvariants() ||
        this->DoFieldCopyProp(loop) || (this->DoFieldHoisting(loop) && !loop->fieldHoistCandidates->IsEmpty());
}

bool
GlobOpt::MayNeedBailOnImplicitCall(IR::Opnd * opnd, Value *val, bool callsToPrimitive)
{
    switch (opnd->GetKind())
    {
    case IR::OpndKindAddr:
    case IR::OpndKindFloatConst:
    case IR::OpndKindIntConst:
        return false;
    case IR::OpndKindReg:
        // Only need implicit call if the operation will call ToPrimitive and we haven't prove
        // that it is already a primitive
        return callsToPrimitive &&
            !(val && val->GetValueInfo()->IsPrimitive()) &&
            !opnd->AsRegOpnd()->GetValueType().IsPrimitive() &&
            !opnd->AsRegOpnd()->m_sym->IsInt32() &&
            !opnd->AsRegOpnd()->m_sym->IsFloat64() &&
            !opnd->AsRegOpnd()->m_sym->IsFloatConst() &&
            !opnd->AsRegOpnd()->m_sym->IsIntConst();
    case IR::OpndKindSym:
        if (opnd->AsSymOpnd()->IsPropertySymOpnd())
        {
            IR::PropertySymOpnd* propertySymOpnd = opnd->AsSymOpnd()->AsPropertySymOpnd();
            if (!propertySymOpnd->MayHaveImplicitCall())
            {
                return false;
            }
        }
        return true;
    default:
        return true;
    };
}

bool
GlobOpt::IsImplicitCallBailOutCurrentlyNeeded(IR::Instr * instr, Value *src1Val, Value *src2Val)
{
    Assert(!this->IsLoopPrePass());

    return this->IsImplicitCallBailOutCurrentlyNeeded(instr, src1Val, src2Val, this->currentBlock,
        (!this->blockData.liveFields->IsEmpty()), !this->currentBlock->IsLandingPad(), true);
}

bool
GlobOpt::IsImplicitCallBailOutCurrentlyNeeded(IR::Instr * instr, Value *src1Val, Value *src2Val, BasicBlock * block, bool hasLiveFields, bool mayNeedImplicitCallBailOut, bool isForwardPass)
{
    if (mayNeedImplicitCallBailOut &&
        !instr->CallsAccessor() &&
        (
            NeedBailOnImplicitCallForLiveValues(block, isForwardPass) ||
            NeedBailOnImplicitCallForCSE(block, isForwardPass) ||
            NeedBailOnImplicitCallWithFieldOpts(block->loop, hasLiveFields) ||
            NeedBailOnImplicitCallForArrayCheckHoist(block, isForwardPass)
        ) &&
        (!instr->HasTypeCheckBailOut() && MayNeedBailOnImplicitCall(instr, src1Val, src2Val)))
    {
        return true;
    }

#if DBG
    if (Js::Configuration::Global.flags.IsEnabled(Js::BailOutAtEveryImplicitCallFlag) &&
        !instr->HasBailOutInfo() && MayNeedBailOnImplicitCall(instr, nullptr, nullptr))
    {
        // always add implicit call bailout even if we don't need it, but only on opcode that supports it
        return true;
    }
#endif

    return false;
}

bool
GlobOpt::IsTypeCheckProtected(const IR::Instr * instr)
{
#if DBG
    IR::Opnd* dst = instr->GetDst();
    IR::Opnd* src1 = instr->GetSrc1();
    IR::Opnd* src2 = instr->GetSrc2();
    AssertMsg(!dst || !dst->IsSymOpnd() || !dst->AsSymOpnd()->IsPropertySymOpnd() ||
        !src1 || !src1->IsSymOpnd() || !src1->AsSymOpnd()->IsPropertySymOpnd(), "No instruction should have a src1 and dst be a PropertySymOpnd.");
    AssertMsg(!src2 || !src2->IsSymOpnd() || !src2->AsSymOpnd()->IsPropertySymOpnd(), "No instruction should have a src2 be a PropertySymOpnd.");
#endif

    IR::Opnd * opnd = instr->GetDst();
    if (opnd && opnd->IsSymOpnd() && opnd->AsSymOpnd()->IsPropertySymOpnd())
    {
        return opnd->AsPropertySymOpnd()->IsTypeCheckProtected();
    }
    opnd = instr->GetSrc1();
    if (opnd && opnd->IsSymOpnd() && opnd->AsSymOpnd()->IsPropertySymOpnd())
    {
        return opnd->AsPropertySymOpnd()->IsTypeCheckProtected();
    }
    return false;
}

bool
GlobOpt::NeedsTypeCheckBailOut(const IR::Instr *instr, IR::PropertySymOpnd *propertySymOpnd, bool isStore, bool* pIsTypeCheckProtected, IR::BailOutKind *pBailOutKind)
{
    if (instr->m_opcode == Js::OpCode::CheckPropertyGuardAndLoadType || instr->m_opcode == Js::OpCode::LdMethodFldPolyInlineMiss)
    {
        return false;
    }
    // CheckFixedFld always requires a type check and bailout either at the instruction or upstream.
    Assert(instr->m_opcode != Js::OpCode::CheckFixedFld || (propertySymOpnd->UsesFixedValue() && propertySymOpnd->MayNeedTypeCheckProtection()));

    if (propertySymOpnd->MayNeedTypeCheckProtection())
    {
        bool isCheckFixedFld = instr->m_opcode == Js::OpCode::CheckFixedFld;
        AssertMsg(!isCheckFixedFld || !PHASE_OFF(Js::FixedMethodsPhase, instr->m_func->GetJnFunction()) ||
            !PHASE_OFF(Js::UseFixedDataPropsPhase, instr->m_func->GetJnFunction()), "CheckFixedFld with fixed method/data phase disabled?");
        Assert(!isStore || !isCheckFixedFld);
        // We don't share caches between field loads and stores.  We should never have a field store involving a proto cache.
        Assert(!isStore || !propertySymOpnd->IsLoadedFromProto());

        if (propertySymOpnd->NeedsTypeCheckAndBailOut())
        {
            *pBailOutKind = propertySymOpnd->HasEquivalentTypeSet() && !propertySymOpnd->MustDoMonoCheck() ?
                (isCheckFixedFld ? IR::BailOutFailedEquivalentFixedFieldTypeCheck : IR::BailOutFailedEquivalentTypeCheck) :
                (isCheckFixedFld ? IR::BailOutFailedFixedFieldTypeCheck : IR::BailOutFailedTypeCheck);
            return true;
        }
        else
        {
            *pIsTypeCheckProtected = propertySymOpnd->IsTypeCheckProtected();
            *pBailOutKind = IR::BailOutInvalid;
            return false;
        }
    }
    else
    {
        Assert(instr->m_opcode != Js::OpCode::CheckFixedFld);
        *pBailOutKind = IR::BailOutInvalid;
        return false;
    }
}

bool
GlobOpt::MayNeedBailOnImplicitCall(const IR::Instr * instr, Value *src1Val, Value *src2Val)
{
    if (!instr->HasAnyImplicitCalls())
    {
        return false;
    }

    bool isLdElem = false;
    switch (instr->m_opcode)
    {
    case Js::OpCode::LdLen_A:
    {
        const ValueType baseValueType(instr->GetSrc1()->GetValueType());
        return
            !(
                baseValueType.IsString() ||
                baseValueType.IsAnyArray() && baseValueType.GetObjectType() != ObjectType::ObjectWithArray ||
                instr->HasBailOutInfo() && instr->GetBailOutKindNoBits() == IR::BailOutOnIrregularLength // guarantees no implicit calls
            );
    }

    case Js::OpCode::LdElemI_A:
    case Js::OpCode::LdMethodElem:
    case Js::OpCode::InlineArrayPop:
        isLdElem = true;
        // fall-through

    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
    case Js::OpCode::InlineArrayPush:
    {
        if(!instr->HasBailOutInfo())
        {
            return true;
        }

        // The following bailout kinds already prevent implicit calls from happening. Any conditions that could trigger an
        // implicit call result in a pre-op bailout.
        const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
        return
            !(
                (bailOutKind & ~IR::BailOutKindBits) == IR::BailOutConventionalTypedArrayAccessOnly ||
                bailOutKind & IR::BailOutOnArrayAccessHelperCall ||
                isLdElem && bailOutKind & IR::BailOutConventionalNativeArrayAccessOnly
            );
    }

    default:
        break;
    }

    IR::Opnd * opnd = instr->GetSrc1();

    bool callsToPrimitive = OpCodeAttr::CallsValueOf(instr->m_opcode);
    if (opnd != nullptr && MayNeedBailOnImplicitCall(opnd, src1Val, callsToPrimitive))
    {
        return true;
    }
    opnd = instr->GetSrc2();
    if (opnd != nullptr && MayNeedBailOnImplicitCall(opnd, src2Val, callsToPrimitive))
    {
        return true;
    }

    opnd = instr->GetDst();

    if (opnd)
    {
        switch (opnd->GetKind())
        {
        case IR::OpndKindReg:
            return false;

        case IR::OpndKindSym:
            // No implicit call if we are just storing to a stack sym. Note that stores to non-configurable root
            // object fields may still need implicit call bailout. That's because a non-configurable field may still
            // become read-only and thus the store field will not take place (or throw in strict mode). Hence, we
            // can't optimize (e.g. copy prop) across such field stores.
            if (opnd->AsSymOpnd()->m_sym->IsStackSym())
            {
                return false;
            }

            if (opnd->AsSymOpnd()->IsPropertySymOpnd())
            {
                IR::PropertySymOpnd* propertySymOpnd = opnd->AsSymOpnd()->AsPropertySymOpnd();
                if (!propertySymOpnd->MayHaveImplicitCall())
                {
                    return false;
                }
            }

            return true;

        case IR::OpndKindIndir:
            return true;

        default:
            Assume(UNREACHED);
        }
    }

    return false;
}

void
GlobOpt::GenerateBailAfterOperation(IR::Instr * *const pInstr, IR::BailOutKind kind)
{
    Assert(pInstr);

    IR::Instr* instr = *pInstr;
    Assert(instr);

    IR::Instr * nextInstr = instr->GetNextRealInstrOrLabel();
    uint32 currentOffset = instr->GetByteCodeOffset();
    while (nextInstr->GetByteCodeOffset() == Js::Constants::NoByteCodeOffset ||
        nextInstr->GetByteCodeOffset() == currentOffset)
    {
        nextInstr = nextInstr->GetNextRealInstrOrLabel();
    }
    IR::Instr * bailOutInstr = instr->ConvertToBailOutInstr(nextInstr, kind);
    if (this->currentBlock->GetLastInstr() == instr)
    {
        this->currentBlock->SetLastInstr(bailOutInstr);
    }
    FillBailOutInfo(this->currentBlock, bailOutInstr->GetBailOutInfo());
    *pInstr = bailOutInstr;
}

void
GlobOpt::GenerateBailAtOperation(IR::Instr * *const pInstr, const IR::BailOutKind bailOutKind)
{
    Assert(pInstr);

    IR::Instr * instr = *pInstr;
    Assert(instr);
    Assert(instr->GetByteCodeOffset() != Js::Constants::NoByteCodeOffset);
    Assert(bailOutKind != IR::BailOutInvalid);

    IR::Instr * bailOutInstr = instr->ConvertToBailOutInstr(instr, bailOutKind);
    if (this->currentBlock->GetLastInstr() == instr)
    {
        this->currentBlock->SetLastInstr(bailOutInstr);
    }
    FillBailOutInfo(currentBlock, bailOutInstr->GetBailOutInfo());
    *pInstr = bailOutInstr;
}

IR::Instr *
GlobOpt::EnsureBailTarget(Loop * loop)
{
    BailOutInfo * bailOutInfo = loop->bailOutInfo;
    IR::Instr * bailOutInstr = bailOutInfo->bailOutInstr;
    if (bailOutInstr == nullptr)
    {
        bailOutInstr = IR::BailOutInstr::New(Js::OpCode::BailTarget, IR::BailOutShared, bailOutInfo, bailOutInfo->bailOutFunc);
        loop->landingPad->InsertAfter(bailOutInstr);
    }
    return bailOutInstr;
}
