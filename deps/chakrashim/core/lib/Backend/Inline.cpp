//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

void
Inline::Optimize()
{
    this->Optimize(this->topFunc);
}

void
Inline::Optimize(Func *func, __in_ecount_opt(callerArgOutCount) IR::Instr *callerArgOuts[], Js::ArgSlot callerArgOutCount, uint recursiveInlineDepth)
{
    if (!func->DoInline() || !topFunc->DoInline() || func->GetJnFunction()->GetIsAsmjsMode()) // disable inlining for asm
    {
        return;
    }

    bool doFixedMethods = !PHASE_OFF(Js::FixedMethodsPhase, func->GetJnFunction());
    const auto inlinerData = func->m_workItem->RecyclableData()->JitTimeData();

    bool doInline = (inlinerData->InlineeCount() > 0 || inlinerData->IsLdFldInlineePresent());
    if (PHASE_OFF(Js::InlinePhase, this->topFunc) ||
        PHASE_OFF(Js::InlinePhase, func->GetJnFunction()) ||
        func->IsJitInDebugMode())
    {
        doInline = false;
    }

    func->actualCount = callerArgOutCount;

    // Keep the caller's "this" symbol (if any).
    StackSym *symThis = nullptr;
    lastStatementBoundary = nullptr;
    IR::LabelInstr* loopTop = nullptr;
    int32 backEdgeCount = 0;

    // Profile data already filter call site outside of loops if the function has loops, so we don't need to detect that here.
    FOREACH_INSTR_EDITING(instr, instrNext, func->m_headInstr)
    {
        bool isInlined = false;
        bool isPolymorphic = false;
        bool isBuiltIn = false;
        bool isCtor = false;
        if (doInline)
        {
            switch (instr->m_opcode)
            {
            case Js::OpCode::StatementBoundary:
                lastStatementBoundary = instr->AsPragmaInstr();
                break;

            case Js::OpCode::Label:
                {
                    if (!loopTop && instr->AsLabelInstr()->m_isLoopTop)
                    {
                        // We only need to know if we are inside loop or not, it doesn't matter how many nested levels we are in.
                        // This is the cheap way of doing so.
                        loopTop = instr->AsLabelInstr();
                        AnalysisAssert(loopTop);
                        this->isInLoop++;
                        backEdgeCount = loopTop->labelRefs.Count();
                    }
                }
                break;

            case Js::OpCode::StFld:
            case Js::OpCode::LdFld:
                {
                    // Try inlining of getter setter
                    if (!inlinerData->IsLdFldInlineePresent())
                    {
                        break;
                    }

                    if (!instr->IsProfiledInstr())
                    {
                        break;
                    }

                    if (!(instr->AsProfiledInstr()->u.FldInfo().flags & Js::FldInfoFlags::FldInfo_FromAccessor))
                    {
                        break;
                    }

                    bool getter = instr->m_opcode == Js::OpCode::LdFld;

                    IR::Opnd *opnd = getter ? instr->GetSrc1() : instr->GetDst();
                    if (!(opnd && opnd->IsSymOpnd()))
                    {
                        break;
                    }

                    IR::SymOpnd* symOpnd = opnd->AsSymOpnd();
                    if (!symOpnd->m_sym->IsPropertySym())
                    {
                        break;
                    }
                    Assert(symOpnd->AsSymOpnd()->IsPropertySymOpnd());

                    const auto inlineCacheIndex = symOpnd->AsPropertySymOpnd()->m_inlineCacheIndex;
                    const auto inlineeData = inlinerData->GetLdFldInlinee(inlineCacheIndex);
                    if (!inlineeData)
                    {
                        break;
                    }

                    Js::FunctionInfo* functionInfo = inlineeData->GetFunctionInfo();
                    if (!functionInfo->GetFunctionBody())
                    {
#ifdef ENABLE_DOM_FAST_PATH
                        Assert(functionInfo->GetLocalFunctionId() == Js::JavascriptBuiltInFunction::DOMFastPathGetter ||
                            functionInfo->GetLocalFunctionId() == Js::JavascriptBuiltInFunction::DOMFastPathSetter);
                        if (PHASE_OFF1(Js::InlineHostCandidatePhase))
                        {
                            break;
                        }
                        this->InlineDOMGetterSetterFunction(instr, inlineeData, inlinerData);
#endif
                        break;
                    }

                    bool isInlinePhaseOff = PHASE_OFF(Js::InlineCandidatePhase, functionInfo->GetFunctionBody()) ||
                                            PHASE_OFF(Js::InlineAccessorsPhase, functionInfo->GetFunctionBody()) ||
                                            (getter && PHASE_OFF(Js::InlineGettersPhase, functionInfo->GetFunctionBody())) ||
                                            (!getter && PHASE_OFF(Js::InlineSettersPhase, functionInfo->GetFunctionBody()));

                    if (isInlinePhaseOff)
                    {
                        break;
                    }

                    this->InlineGetterSetterFunction(instr, inlineeData, symThis, inlineCacheIndex, getter /*isGetter*/, recursiveInlineDepth);

                    break;
                }

            case Js::OpCode::NewScObjArray:
                // We know we're not going to inline these. Just break out and try to do a fixed function check.
                isCtor = true;
                isBuiltIn = true;
                break;

            case Js::OpCode::NewScObject:
                isCtor = true;
                if (PHASE_OFF(Js::InlineConstructorsPhase, this->topFunc))
                {
                    break;
                }
                // fall-through

            case Js::OpCode::CallI:
                {

                    IR::PropertySymOpnd* methodValueOpnd = GetMethodLdOpndForCallInstr(instr);

                    if (this->inlineesProcessed == inlinerData->InlineeCount())
                    {
                        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                        break;
                    }

                    if(!instr->IsProfiledInstr())
                    {
                        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                        break;
                    }

                    const auto profileId = static_cast<Js::ProfileId>(instr->AsProfiledInstr()->u.profileId);
                    if(profileId >= func->GetJnFunction()->GetProfiledCallSiteCount())
                    {
                        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                        break;
                    }

                    const auto inlineeData = inlinerData->GetInlinee(profileId);
                    if(!inlineeData)
                    {
                        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                        break;
                    }

                    if(inlinerData->IsPolymorphicCallSite(profileId))
                    {
                        isPolymorphic = true;
                        if (isCtor ||
                            (PHASE_OFF(Js::PolymorphicInlinePhase, this->topFunc) || PHASE_OFF(Js::PolymorphicInlinePhase, func->GetJnFunction())) ||
                            (this->IsInliningOutSideLoops() && !PHASE_FORCE(Js::InlinePhase, this->topFunc) && !PHASE_FORCE(Js::InlinePhase, func->GetJnFunction())))
                        {
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
                            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
                            POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic): Skip Inline: Inlining polymorphic call site outside loop\tIsConstructorCall: %s \tisTopFunc: %s\tCaller: %s (%s)\n",
                                     (isCtor? L"true": L"false"), (this->topFunc != func? L"true":L"false"),
                                     inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));

                            // TODO: Constructor polymorphic inlining

                            TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                            TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                            break;
                        }
                        if (!PHASE_OFF(Js::FixedMethodsPhase, this->topFunc) && !PHASE_OFF(Js::PolymorphicInlineFixedMethodsPhase, this->topFunc))
                        {
                            instrNext = InlinePolymorphicFunctionUsingFixedMethods(instr, inlinerData, symThis, profileId, methodValueOpnd, &isInlined, recursiveInlineDepth);
                        }
                        else
                        {
                            TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                            TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                            instrNext = InlinePolymorphicFunction(instr, inlinerData, symThis, profileId, &isInlined, recursiveInlineDepth);
                        }
                    }
                    else
                    {
                        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                        Js::FunctionInfo* functionInfo = inlineeData->GetFunctionInfo();
                        Js::OpCode builtInInlineCandidateOpCode;
                        ValueType builtInReturnType;

                        // If the inlinee info is the array constructor, just change the opcode to NewScObjArray
                        // so that we will inline the array allocation in lower
                        if (isCtor && functionInfo == &Js::JavascriptArray::EntryInfo::NewInstance)
                        {
                            isBuiltIn = true;
                            instr->m_opcode = Js::OpCode::NewScObjArray;
                            instr->AsProfiledInstr()->u.profileId = Js::Constants::NoProfileId;
                            break;
                        }

                        isBuiltIn = InliningDecider::GetBuiltInInfo(functionInfo, &builtInInlineCandidateOpCode, &builtInReturnType, func->GetScriptContext());

                        if(!builtInReturnType.IsUninitialized() && instr->GetDst())
                        {
                            Assert(!functionInfo->HasBody());
                            AssertMsg(instr->m_opcode != Js::OpCode::NewScObjArray, "We should have broken out of the switch statement earlier on this opcode.");
                            // Value types for the array built-in calls are pulled from the profile; don't change them here.
                            if ((instr->m_opcode != Js::OpCode::NewScObjArray) ||
                                !instr->GetDst()->GetValueType().IsLikelyNativeArray())
                            {
                                // Assume that this built-in function is not going to be inlined, so the return type cannot be definite
                                instr->GetDst()->SetValueType(builtInReturnType.ToLikely());
                            }
                        }

                        bool isInlinePhaseOff = functionInfo->HasBody() ?
                            PHASE_OFF(Js::InlineCandidatePhase, functionInfo->GetFunctionBody()) :
                            PHASE_OFF1(Js::InlineBuiltInPhase);
                        if (isInlinePhaseOff)
                        {
                            break;
                        }

                        if(!functionInfo->HasBody() && builtInInlineCandidateOpCode == 0)
                        {
                            // This built-in function is not going to be inlined
                            break;
                        }

                        if(!functionInfo->HasBody())
                        {
                            Assert(builtInInlineCandidateOpCode != 0);
                            if(isCtor)
                            {
                                // Inlining a built-in function called as a constructor is currently not supported. Although InliningDecider
                                // already checks for this, profile data matching with a function does not take into account the difference
                                // between a constructor call and a regular function call, so need to check it again.
                                break;
                            }

                            // This built-in function is going to be inlined, so reset the destination's value type
                            if(!builtInReturnType.IsUninitialized())
                            {
                                if(instr->GetDst())
                                {
                                    instr->GetDst()->SetValueType(builtInReturnType);
                                    if(builtInReturnType.IsDefinite())
                                    {
                                        instr->GetDst()->SetValueTypeFixed();
                                    }
                                }
                            }
                        }
                        else
                        {

                            if (!inlineeData->GetFunctionBody()->HasDynamicProfileInfo())        // Don't try to inline a function if it doesn't have profile data
                            {
                                break;
                            }

                            uint16 constantArguments = 0;
                            if (!PHASE_OFF(Js::InlineRecursivePhase, func->GetJnFunction()))
                            {
                                instr->IterateArgInstrs([&](IR::Instr* argInstr) {
                                    IR::Opnd *src1 = argInstr->GetSrc1();
                                    if (!src1->IsRegOpnd())
                                    {
                                        return false;
                                    }
                                    StackSym *sym = src1->AsRegOpnd()->m_sym;
                                    if (sym->IsIntConst())
                                    {
                                        if (argInstr->GetSrc2() && argInstr->GetSrc2()->IsSymOpnd())
                                        {
                                            StackSym *dstSym = argInstr->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
                                            Assert(dstSym->IsSingleDef());
                                            Assert(dstSym->IsArgSlotSym());
                                            Js::ArgSlot argCount = dstSym->GetArgSlotNum() - 1;

                                            if (argCount == Js::Constants::MaximumArgumentCountForConstantArgumentInlining)
                                            {
                                                return true;
                                            }
                                            constantArguments |= (1 << argCount);
                                        }
                                    }
                                    return false;
                                });
                            }

                            if (!inliningHeuristics.BackendInlineIntoInliner(inlineeData->GetFunctionBody(),
                                func->GetJnFunction(), this->topFunc, profileId, isCtor, true /*isFixedMethodCall*/,
                                this->IsInliningOutSideLoops(), this->isInLoop != 0, recursiveInlineDepth, constantArguments))
                            {
                                break;
                            }

                        }


                        instrNext = builtInInlineCandidateOpCode != 0 ?
                            this->InlineBuiltInFunction(instr, functionInfo, builtInInlineCandidateOpCode, inlinerData, symThis, &isInlined, profileId, recursiveInlineDepth) :
                            this->InlineScriptFunction(instr, inlineeData, symThis, profileId, &isInlined, recursiveInlineDepth);

                    }
                    if(++this->inlineesProcessed == inlinerData->InlineeCount())
                    {
                        // getterSetter inline caches are shared and we have no way of knowing how many more are present
                        if (!inlinerData->IsLdFldInlineePresent() && !doFixedMethods)
                        {
                            return ;
                        }
                    }
                    break;
                }

            case Js::OpCode::CallIExtended:
            {
                if (this->inlineesProcessed == inlinerData->InlineeCount())
                {
                    break;
                }

                if (!instr->IsProfiledInstr())
                {
                    break;
                }

                const auto profileId = static_cast<Js::ProfileId>(instr->AsProfiledInstr()->u.profileId);
                if (profileId >= func->GetJnFunction()->GetProfiledCallSiteCount())
                {
                    break;
                }

                const auto inlineeData = inlinerData->GetInlinee(profileId);
                if (!inlineeData)
                {
                    break;
                }

                if (Lowerer::IsSpreadCall(instr))
                {
                    InlineSpread(instr);
                }
                break;
            }

            case Js::OpCode::ArgOut_A:
                InlConstFoldArg(instr, callerArgOuts, callerArgOutCount);
                break;

            case Js::OpCode::LdThis:
                Assert(instr->GetDst() && instr->GetDst()->IsRegOpnd());
                Assert(symThis == nullptr);

                symThis = instr->GetDst()->AsRegOpnd()->m_sym;
                break;

            case Js::OpCode::CheckThis:
                // Is this possible? Can we be walking an inlinee here? Doesn't hurt to support this case...
                Assert(instr->GetSrc1() && instr->GetSrc1()->IsRegOpnd());
                Assert(symThis == nullptr);

                symThis = instr->GetSrc1()->AsRegOpnd()->m_sym;
                break;

            default:
                {
                    if (loopTop && instr->IsBranchInstr())
                    {
                        // Look for the back edge to loopTop.
                        IR::BranchInstr *branch = instr->AsBranchInstr();
                        IR::LabelInstr *labelDestination = branch->GetTarget();
                        if (labelDestination == loopTop) // We found the back edge
                        {
                            backEdgeCount--;
                            if (backEdgeCount == 0) // We have seen all the back edges, hence we are outside loop now.
                            {
                                Assert(this->isInLoop > 0);
                                --this->isInLoop;
                                loopTop = nullptr;
                            }
                        }
                    }
                }

            }
        }

        // If we chose not to inline, let's try to optimize this call if it uses a fixed method
        if (!isInlined)
        {
            switch (instr->m_opcode)
            {
            case Js::OpCode::NewScObject:
            case Js::OpCode::NewScObjArray:
                isCtor = true;
                // intentionally fall through.
            case Js::OpCode::CallI:
                {
                    IR::PropertySymOpnd* methodValueOpnd = GetMethodLdOpndForCallInstr(instr);

                    TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                    TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                    StackSym* originalCallTargetStackSym = instr->GetSrc1()->GetStackSym();
                    bool safeThis = false;
                    if (TryOptimizeCallInstrWithFixedMethod(instr, nullptr, isPolymorphic /*isPolymorphic*/, isBuiltIn /*isBuiltIn*/, isCtor /*isCtor*/, false /*isInlined*/, safeThis /*unused here*/))
                    {
                        Assert(originalCallTargetStackSym != nullptr);

                        // Insert a ByteCodeUsesInstr to make sure the methodValueDstOpnd's constant value is captured by any
                        // bailout that occurs between CheckFixedMethodField and CallI.
                        IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(instr, originalCallTargetStackSym->m_id);
                        instr->InsertBefore(useCallTargetInstr);

                        // Split NewScObject into NewScObjectNoCtor and CallI, but don't touch NewScObjectArray.
                        if (instr->m_opcode == Js::OpCode::NewScObject && !PHASE_OFF(Js::SplitNewScObjectPhase, this->topFunc))
                        {
                            SplitConstructorCall(instr, false, true);
                        }
                    }
                    else if (instr->m_opcode == Js::OpCode::NewScObjArray)
                    {
                        if (instr->GetDst() && instr->GetDst()->GetValueType().IsLikelyNativeArray())
                        {
                            // We expect to create a native array here, so we'll insert a check against the
                            // expected call target, which requires a bailout.
                            instr = instr->ConvertToBailOutInstr(instr, IR::BailOutOnNotNativeArray);
                        }
                    }
                }
                break;
            }
        }
    } NEXT_INSTR_EDITING;

    INLINE_FLUSH();
}

uint Inline::FillInlineesDataArray(
        const Js::FunctionCodeGenJitTimeData* inlineeJitTimeData,
        const Js::FunctionCodeGenRuntimeData* inlineeRuntimeData,
        _Out_writes_to_(inlineesDataArrayLength, (return >= inlineesDataArrayLength ? inlineesDataArrayLength : return)) InlineeData *inlineesDataArray,
        uint inlineesDataArrayLength
        )
{
    uint inlineeCount = 0;
    while(inlineeJitTimeData)
    {
        if (inlineeCount >= inlineesDataArrayLength)
        {
            // Count the actual number of inlinees for logging.
            while (inlineeJitTimeData)
            {
                inlineeCount++;
                inlineeJitTimeData = inlineeJitTimeData->GetNext();
            }
            return inlineeCount;
        }

        Js::FunctionBody *inlineeFunctionBody = inlineeJitTimeData->GetFunctionBody();
        if (!PHASE_OFF(Js::PolymorphicInlinePhase, inlineeFunctionBody))
        {
            const Js::FunctionCodeGenJitTimeData* rightInlineeJitTimeData = inlineeJitTimeData->GetJitTimeDataFromFunctionInfo(inlineeFunctionBody);
            const Js::FunctionCodeGenRuntimeData* rightInlineeRuntimeData = inlineeRuntimeData->GetRuntimeDataFromFunctionInfo(inlineeFunctionBody);

            if (rightInlineeJitTimeData)
            {
                inlineesDataArray[inlineeCount].inlineeJitTimeData = rightInlineeJitTimeData;
                inlineesDataArray[inlineeCount].inlineeRuntimeData = rightInlineeRuntimeData;
                inlineesDataArray[inlineeCount].functionBody = inlineeFunctionBody;
                Assert(rightInlineeJitTimeData->GetFunctionBody() == inlineeFunctionBody);
#ifdef DBG
                for (uint k = 0; k < inlineeCount; k++)
                {
                    if (inlineesDataArray[k].functionBody  == inlineeFunctionBody)
                    {
                        AssertMsg(false, "We should never see duplicate function body here");
                    }
                }
#endif
                inlineeCount++;
            }
            else
            {
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
                POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic): Missing jit time data skipped inlinee\tInlinee: %s (%s)\n",
                            inlineeFunctionBody->GetDisplayName(), inlineeFunctionBody->GetDebugNumberSet(debugStringBuffer));
            }
        }
        inlineeJitTimeData = inlineeJitTimeData->GetNext();
    }

    return inlineeCount;
}

void Inline::FillInlineesDataArrayUsingFixedMethods(
        const Js::FunctionCodeGenJitTimeData* inlineeJitTimeData,
        const Js::FunctionCodeGenRuntimeData* inlineeRuntimeData,
        __inout_ecount(inlineesDataArrayLength) InlineeData *inlineesDataArray,
        uint inlineesDataArrayLength,
        __inout_ecount(cachedFixedInlineeCount) Js::FixedFieldInfo* fixedFieldInfoArray,
        uint16 cachedFixedInlineeCount
        )
{
    AnalysisAssert(cachedFixedInlineeCount <= inlineesDataArrayLength);

    Js::FunctionBody* inlineeFuncBody = nullptr;
    while (inlineeJitTimeData)
    {
        inlineeFuncBody = inlineeJitTimeData->GetFunctionBody();
        if (!PHASE_OFF(Js::PolymorphicInlinePhase, inlineeFuncBody) && !PHASE_OFF(Js::PolymorphicInlineFixedMethodsPhase, inlineeFuncBody))
        {
            const Js::FunctionCodeGenJitTimeData* jitTimeData = inlineeJitTimeData->GetJitTimeDataFromFunctionInfo(inlineeFuncBody);
            if (jitTimeData)
            {
                for (uint16 i = 0; i < cachedFixedInlineeCount; i++)
                {
                    if (inlineeFuncBody == ((Js::JavascriptFunction*)(fixedFieldInfoArray[i].fieldValue))->GetFunctionBody())
                    {
                        inlineesDataArray[i].inlineeJitTimeData = inlineeJitTimeData->GetJitTimeDataFromFunctionInfo(inlineeFuncBody);
                        inlineesDataArray[i].inlineeRuntimeData = inlineeRuntimeData->GetRuntimeDataFromFunctionInfo(inlineeFuncBody);
                        inlineesDataArray[i].functionBody = inlineeFuncBody;
                        break;
                    }
                }
            }
            else
            {
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
                wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
                POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic): Missing jit time data skipped inlinee\tInlinee: %s (%s)\n",
                            inlineeFuncBody->GetDisplayName(), inlineeFuncBody->GetDebugNumberSet(debugStringBuffer));
            }
        }
        inlineeJitTimeData = inlineeJitTimeData->GetNext();
    }
}

IR::Instr *
Inline::InlinePolymorphicFunctionUsingFixedMethods(IR::Instr *callInstr, const Js::FunctionCodeGenJitTimeData* inlinerData, const StackSym *symCallerThis, const Js::ProfileId profileId, IR::PropertySymOpnd* methodValueOpnd, bool* pIsInlined, uint recursiveInlineDepth)
{
    IR::Instr* instrNext = callInstr->m_next;
    *pIsInlined = false;

    const Js::FunctionCodeGenJitTimeData* inlineeJitTimeData = inlinerData->GetInlinee(profileId);
    AnalysisAssert(inlineeJitTimeData);

#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    // Abort conditions:
    if(!inlineeJitTimeData->GetNext())
    {
        POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic): Skip Inline: Missing JitTime data \tInlinee: %s (%s):\tCaller: %s (%s)\n",
                 inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                 inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));

        // There are no multiple codegen jit-time data allocated for this call site, not sure how is this possible, abort
        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
        return instrNext;
    }

    // arguments exceed MaxInlineeArgoutCount
    if (callInstr->GetSrc2() &&
        callInstr->GetSrc2()->IsSymOpnd() &&
        callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum() > Js::InlineeCallInfo::MaxInlineeArgoutCount)
    {
        // This is a hard limit as we only use 4 bits to encode the actual count in the InlineeCallInfo. Although
        // InliningDecider already checks for this, the check is against profile data that may not be accurate since profile
        // data matching does not take into account some types of changes to source code. Need to check this again with current
        // information.
        POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic): Skip Inline: ArgSlot > MaxInlineeArgoutCount\tInlinee: %s (%s)\tArgSlotNum: %d\tMaxInlineeArgoutCount: %d\tCaller: %s (%s)\n",
            inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer) , callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum(),
            Js::InlineeCallInfo::MaxInlineeArgoutCount, inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));

        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
        return instrNext;
    }

    uint inlineeCount = 0;
    const Js::FunctionCodeGenJitTimeData* tmpInlineeJitTimeData = inlineeJitTimeData;
    while(tmpInlineeJitTimeData)
    {
        inlineeCount++;
        tmpInlineeJitTimeData = tmpInlineeJitTimeData->GetNext();
    }

    // Inlinee count too small (<2) or too large (>4)
    if (inlineeCount < 2 || inlineeCount > Js::DynamicProfileInfo::maxPolymorphicInliningSize)
    {
        POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic): Skip Inline: Inlinee count either too small or too large: InlineeCount %d (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n",
                    inlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));

        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
        return instrNext;
    }

    *pIsInlined = true;

    IR::Instr* tmpInstr = callInstr->m_prev;
    while (tmpInstr->m_opcode != Js::OpCode::StartCall)
    {
        if ((tmpInstr->m_opcode != Js::OpCode::ArgOut_A) && (tmpInstr->m_opcode != Js::OpCode::Ld_A))
        {
            POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic; Using Fixed Methods): Skip Inline: ArgOuts may have side effects Inlinee: %s (%s):\tCaller: %s (%s)\n",
                    inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));
            return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
        }
        tmpInstr = tmpInstr->m_prev;
    }

    StackSym* methodValueSym = callInstr->GetSrc1()->AsRegOpnd()->m_sym->AsStackSym();
    if (!methodValueSym->IsSingleDef())
    {
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    IR::Instr* ldMethodFldInstr = methodValueSym->GetInstrDef();
    if (!(ldMethodFldInstr->GetSrc1()->IsSymOpnd() && ldMethodFldInstr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd()))
    {
        POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic; Using Fixed Methods): Skip Inline: Did not find property sym operand for the method load Inlinee: %s (%s):\tCaller: %s (%s)\n",
                    inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    IR::PropertySymOpnd* methodPropertyOpnd = ldMethodFldInstr->GetSrc1()->AsPropertySymOpnd();
    if (!methodPropertyOpnd->HasObjTypeSpecFldInfo())
    {
        POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic; Using Fixed Methods): Skip Inline: no ObjTypeSpecFldInfo to get Fixed Methods from Inlinee: %s (%s):\tCaller: %s (%s)\n",
                    inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    if (!methodPropertyOpnd->HasFixedValue())
    {
        POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic; Using Fixed Methods): Skip Inline: ObjTypeSpecFldInfo doesn't have Fixed Methods for one or some of the inlinees Inlinee: %s (%s):\tCaller: %s (%s)\n",

                    inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    uint16 cachedFixedInlineeCount = methodPropertyOpnd->GetFixedFieldCount();
    if (cachedFixedInlineeCount < 2)
    {
        POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic; Using Fixed Methods): Skip Inline: fixed function count too less %d (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n",
                    cachedFixedInlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    Js::FixedFieldInfo* fixedFunctionInfoArray = methodPropertyOpnd->GetFixedFieldInfoArray();

    // It might so be the case that two objects of different types call the same function (body), for e.g., if they share the prototype on which the function is defined.
    uint uniqueFixedFunctionCount = HandleDifferentTypesSameFunction(fixedFunctionInfoArray, cachedFixedInlineeCount);

    if (uniqueFixedFunctionCount != inlineeCount)
    {
        // inlineeCount obtained from the inlineeJitTimeData is more accurate than cached number of fixed methods for inlinees.
        POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic; Using Fixed Methods): Skip Inline: cached fixed function count (%d) doesn't match inlinee count (%d); (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n",
                    uniqueFixedFunctionCount, inlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    Assert(cachedFixedInlineeCount <= Js::DynamicProfileInfo::maxPolymorphicInliningSize);
    InlineeData inlineesDataArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = {};

    const Js::FunctionCodeGenRuntimeData* inlineeRuntimeData = callInstr->m_func->m_runtimeData ?
                                callInstr->m_func->m_runtimeData->GetInlinee(profileId) :
                             this->topFunc->GetJnFunction()->GetInlineeCodeGenRuntimeData(profileId);

    FillInlineesDataArrayUsingFixedMethods(inlineeJitTimeData, inlineeRuntimeData, inlineesDataArray, Js::DynamicProfileInfo::maxPolymorphicInliningSize, fixedFunctionInfoArray, cachedFixedInlineeCount);

    for (uint i = 0; i < cachedFixedInlineeCount; i++)
    {
        if(!inlineesDataArray[i].functionBody)
        {
            POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic; Using Fixed Methods): Skip Inline: One of the inlinees doesn't have the corresponding object/prototype's type cached\tCaller: %s (%s)\n",
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
            return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
        }
#if DBG
        if(inlineesDataArray[i].functionBody && inlineesDataArray[i].functionBody != methodPropertyOpnd->GetFieldValueAsFixedFunction(i)->GetFunctionBody())
        {
            AssertMsg(false, "inlineesDataArray and fixedfunctionInfoArray should be aligned with each other at this point");
        }
#endif
        while (fixedFunctionInfoArray[i].nextHasSameFixedField)
        {
            i++;
        }
    }

    bool safeThis = true; // Eliminate CheckThis for inlining.
    for (uint i = 0; i < cachedFixedInlineeCount; i++)
    {
        if (!methodPropertyOpnd->GetFieldValue(i))
        {
            POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic; Using Fixed Methods): Skip Inline: no fixed method for one of the inlinees; Inlinee: %s (%s):\tCaller: %s (%s)\n",
                    inlineesDataArray[i].functionBody->GetDisplayName(), inlineesDataArray[i].functionBody->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));
            return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
        }
        if (i == 0)
        {
            // Do all the general, non-function-object-specific checks just once.
            if (!TryOptimizeCallInstrWithFixedMethod(callInstr, (Js::FunctionInfo*)(inlineesDataArray[i].functionBody), true, false, false, true /*isInlined*/, safeThis, true /*dontOptimizeJustCheck*/, i))
            {
                POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic; Using Fixed Methods): Skip Inline: can't optimize using Fixed Methods %d (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n",
                    inlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));
                return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
            }
        }
        else
        {
            if (methodPropertyOpnd->GetFieldValueAsFixedFunction(i) &&
                methodPropertyOpnd->GetFieldValueAsFixedFunction(i)->GetFunctionInfo() != (Js::FunctionInfo*)(inlineesDataArray[i].functionBody))
            {
                POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic; Using Fixed Methods): Skip Inline: can't optimize using Fixed Methods %d (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n",
                    inlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));
                return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth,  true);
            }
        }
        Js::TypeId typeId = methodPropertyOpnd->GetTypeId(i);
        if(!(typeId > Js::TypeIds_LastJavascriptPrimitiveType && typeId <= Js::TypeIds_LastTrueJavascriptObjectType))
        {
            // Don't eliminate CheckThis if it cannot be done for any one of the inlinees
            safeThis = false;
        }
        while (fixedFunctionInfoArray[i].nextHasSameFixedField)
        {
            i++;
        }
    }

    Assert(methodPropertyOpnd->IsPoly());

    // emit property guard check for the method load, and load type
    IR::RegOpnd *typeOpnd = IR::RegOpnd::New(TyVar, callInstr->m_func);
    IR::Instr* propertyGuardCheckInstr = IR::Instr::New(Js::OpCode::CheckPropertyGuardAndLoadType, typeOpnd, ldMethodFldInstr->GetSrc1(), callInstr->m_func);
    ldMethodFldInstr->InsertBefore(propertyGuardCheckInstr);
    propertyGuardCheckInstr->SetByteCodeOffset(ldMethodFldInstr);
    propertyGuardCheckInstr = propertyGuardCheckInstr->ConvertToBailOutInstr(ldMethodFldInstr, IR::BailOutFailedFixedFieldCheck);

    POLYMORPHIC_INLINE_TESTTRACE(L"------------------------------------------------\n");
    for (uint i = 0; i < cachedFixedInlineeCount; i++)
    {
        Js::FunctionBody *inlineeFunctionBody = inlineesDataArray[i].functionBody;
        POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic; Using Fixed Methods): Start inlining: \tInlinee: %s (%s):\tCaller: %s (%s)\n",
                    inlineeFunctionBody->GetDisplayName(), inlineeFunctionBody->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));

        while (fixedFunctionInfoArray[i].nextHasSameFixedField)
        {
            i++;
        }
    }
    POLYMORPHIC_INLINE_TESTTRACE(L"------------------------------------------------\n");

    IR::RegOpnd * returnValueOpnd;
    if (callInstr->GetDst())
    {
        returnValueOpnd = callInstr->UnlinkDst()->AsRegOpnd();
    }
    else
    {
        returnValueOpnd = nullptr;
    }

    callInstr->MoveArgs(/*generateByteCodeCapture*/ true);

    callInstr->m_opcode = Js::OpCode::CallIFixed;

    // iterate over inlineesDataArray to emit each inlinee
    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func, false);
    IR::Instr* dispatchStartLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func, false);
    callInstr->InsertBefore(dispatchStartLabel);
    for(uint i=0; i < cachedFixedInlineeCount; i++)
    {
        IR::LabelInstr* inlineeStartLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func);
        callInstr->InsertBefore(inlineeStartLabel);

        IR::AddrOpnd * constMethodValueOpnd = IR::AddrOpnd::New(methodPropertyOpnd->GetFieldValue(i), IR::AddrOpndKind::AddrOpndKindDynamicVar, callInstr->m_func);
        constMethodValueOpnd->m_isFunction = true;

        InsertOneInlinee(callInstr, returnValueOpnd, constMethodValueOpnd, inlineesDataArray[i], doneLabel, symCallerThis, safeThis, recursiveInlineDepth);
        while (fixedFunctionInfoArray[i].nextHasSameFixedField)
        {
            dispatchStartLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::BrAddr_A, inlineeStartLabel, typeOpnd, IR::AddrOpnd::New(methodPropertyOpnd->GetType(i),
                IR::AddrOpndKindDynamicType, dispatchStartLabel->m_func), dispatchStartLabel->m_func));
            this->topFunc->PinTypeRef(methodPropertyOpnd->GetType(i)); // Keep the types alive as the types may not be equivalent and, hence, won't be kept alive by EquivalentTypeCache
            i++;
        }

        dispatchStartLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::BrAddr_A, inlineeStartLabel,
            typeOpnd, IR::AddrOpnd::New(methodPropertyOpnd->GetType(i), IR::AddrOpndKindDynamicType, dispatchStartLabel->m_func), dispatchStartLabel->m_func));
        this->topFunc->PinTypeRef(methodPropertyOpnd->GetType(i)); // Keep the types alive as the types may not be equivalent and, hence, won't be kept alive by EquivalentTypeCache
    }

    ldMethodFldInstr->Unlink();
    ldMethodFldInstr->m_opcode = Js::OpCode::LdMethodFldPolyInlineMiss;
    Assert(cachedFixedInlineeCount > 0);
    CompletePolymorphicInlining(callInstr, returnValueOpnd, doneLabel, dispatchStartLabel, ldMethodFldInstr, IR::BailOutOnFailedPolymorphicInlineTypeCheck);

    this->topFunc->SetHasInlinee();
    InsertStatementBoundary(instrNext);

    return instrNext;
}

void Inline::CloneCallSequence(IR::Instr* callInstr, IR::Instr* clonedCallInstr)
{
    IR::Instr* previousArg = nullptr;
    IR::Instr* previousClonedArg = clonedCallInstr;
    callInstr->IterateArgInstrs([&](IR::Instr* argInstr){
        IR::Instr* cloneArg = IR::Instr::New(argInstr->m_opcode,
            IR::SymOpnd::New(callInstr->m_func->m_symTable->GetArgSlotSym(argInstr->GetDst()->GetStackSym()->GetArgSlotNum()), 0, TyMachPtr, callInstr->m_func),
            argInstr->GetSrc1(), callInstr->m_func);
        cloneArg->SetByteCodeOffset(callInstr);
        cloneArg->GetDst()->GetStackSym()->m_isArgCaptured = true;
        previousClonedArg->SetSrc2(cloneArg->GetDst());
        previousClonedArg->InsertBefore(cloneArg);
        previousArg = argInstr;
        previousClonedArg = cloneArg;
        return false;
    });
    IR::Instr* startCall = previousArg->GetSrc2()->GetStackSym()->GetInstrDef();
    previousClonedArg->SetSrc2(startCall->GetDst());

}

IR::Instr *
Inline::InlinePolymorphicFunction(IR::Instr *callInstr, const Js::FunctionCodeGenJitTimeData* inlinerData, const StackSym *symCallerThis, const Js::ProfileId profileId, bool* pIsInlined, uint recursiveInlineDepth, bool triedUsingFixedMethods)
{
    IR::Instr* instrNext = callInstr->m_next;
    *pIsInlined = false;


    if (triedUsingFixedMethods)
    {
        if (callInstr->GetSrc1()->AsRegOpnd()->m_sym->AsStackSym()->IsSingleDef())
        {
            IR::Instr* ldMethodFldInstr = callInstr->GetSrc1()->AsRegOpnd()->m_sym->AsStackSym()->GetInstrDef();
            if (ldMethodFldInstr->GetSrc1()->IsSymOpnd() && ldMethodFldInstr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
            {
                TryResetObjTypeSpecFldInfoOn(ldMethodFldInstr->GetSrc1()->AsPropertySymOpnd());
                TryDisableRuntimePolymorphicCacheOn(ldMethodFldInstr->GetSrc1()->AsPropertySymOpnd());
            }
        }
    }

    const Js::FunctionCodeGenJitTimeData* inlineeJitTimeData = inlinerData->GetInlinee(profileId);
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    if (!triedUsingFixedMethods) // We would have done the following two checks when we tried to inline using fixed methods
    {
        if(!inlineeJitTimeData->GetNext())
        {
            POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic): Skip Inline: Missing JitTime data \tInlinee: %s (%s):\tCaller: %s (%s)\n",
                    inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                     inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));

            //There are no multiple codegen jit-time data allocated for this call site, not sure how is this possible, abort
            return instrNext;
        }

        if (callInstr->GetSrc2() &&
            callInstr->GetSrc2()->IsSymOpnd() &&
            callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum() > Js::InlineeCallInfo::MaxInlineeArgoutCount)
        {
            // This is a hard limit as we only use 4 bits to encode the actual count in the InlineeCallInfo. Although
            // InliningDecider already checks for this, the check is against profile data that may not be accurate since profile
            // data matching does not take into account some types of changes to source code. Need to check this again with current
            // information.
            POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic): Skip Inline: ArgSlot > MaxInlineeArgoutCount\tInlinee: %s (%s)\tArgSlotNum: %d\tMaxInlineeArgoutCount: %d\tCaller: %s (%s)\n",
                inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer) , callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum(),
                Js::InlineeCallInfo::MaxInlineeArgoutCount, inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));

            return instrNext;
        }
    }

    InlineeData inlineesDataArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize];

    const Js::FunctionCodeGenRuntimeData* inlineeRuntimeData = callInstr->m_func->m_runtimeData ?
                                callInstr->m_func->m_runtimeData->GetInlinee(profileId) :
                             this->topFunc->GetJnFunction()->GetInlineeCodeGenRuntimeData(profileId);


    uint inlineeCount = FillInlineesDataArray(inlineeJitTimeData, inlineeRuntimeData, inlineesDataArray, Js::DynamicProfileInfo::maxPolymorphicInliningSize);
    if (inlineeCount < 2 || inlineeCount > Js::DynamicProfileInfo::maxPolymorphicInliningSize)
    {
        POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic): Skip Inline: Inlinee count either too small or too large %d (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n",
                    inlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetFunctionBody()->GetDisplayName(), inlineeJitTimeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));

        return instrNext;
    }

    // Begin inlining.
    POLYMORPHIC_INLINE_TESTTRACE(L"------------------------------------------------\n");
    for (uint i = 0; i < inlineeCount; i++)
    {
        Js::FunctionBody *inlineeFunctionBody = inlineesDataArray[i].functionBody;
        POLYMORPHIC_INLINE_TESTTRACE(L"INLINING (Polymorphic): Start inlining: \tInlinee: %s (%s):\tCaller: %s (%s)\n",
                    inlineeFunctionBody->GetDisplayName(), inlineeFunctionBody->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2));
    }
    POLYMORPHIC_INLINE_TESTTRACE(L"------------------------------------------------\n");

    *pIsInlined = true;

    // This function is recursive, so when jitting in the foreground, probe the stack
    if (!this->topFunc->IsBackgroundJIT())
    {
        PROBE_STACK(this->topFunc->GetScriptContext(), Js::Constants::MinStackDefault);
    }

    IR::RegOpnd * returnValueOpnd;
    Js::RegSlot returnRegSlot;
    if (callInstr->GetDst())
    {
        returnValueOpnd = callInstr->UnlinkDst()->AsRegOpnd();
        returnRegSlot = returnValueOpnd->m_sym->GetByteCodeRegSlot();
    }
    else
    {
        returnValueOpnd = nullptr;
        returnRegSlot = Js::Constants::NoRegister;
    }

    Assert(inlineeCount >= 2);

    // Shared bailout point for all the guard check bailouts.
    InsertJsFunctionCheck(callInstr, callInstr, IR::BailOutOnPolymorphicInlineFunction);

    callInstr->MoveArgs(/*generateByteCodeCapture*/ true);

    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func, false);
    IR::Instr* dispatchStartLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func, false);
    callInstr->InsertBefore(dispatchStartLabel);
    for (uint i = 0; i < inlineeCount; i++)
    {
        IR::LabelInstr* inlineeStartLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func);
        callInstr->InsertBefore(inlineeStartLabel);

        InsertOneInlinee(callInstr, returnValueOpnd, callInstr->GetSrc1(), inlineesDataArray[i], doneLabel, symCallerThis, /*fixedFunctionSafeThis*/ false, recursiveInlineDepth);

        IR::RegOpnd* functionObject = callInstr->GetSrc1()->AsRegOpnd();
        dispatchStartLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::BrAddr_A, inlineeStartLabel,
            IR::IndirOpnd::New(functionObject, Js::JavascriptFunction::GetOffsetOfFunctionInfo(), TyMachPtr, dispatchStartLabel->m_func),
            IR::AddrOpnd::New(inlineesDataArray[i].functionBody, IR::AddrOpndKindDynamicFunctionBody, dispatchStartLabel->m_func), dispatchStartLabel->m_func));
    }

    CompletePolymorphicInlining(callInstr, returnValueOpnd, doneLabel, dispatchStartLabel, /*ldMethodFldInstr*/nullptr, IR::BailOutOnPolymorphicInlineFunction);

    this->topFunc->SetHasInlinee();
    InsertStatementBoundary(instrNext);

    return instrNext;

}

void Inline::CompletePolymorphicInlining(IR::Instr* callInstr, IR::RegOpnd* returnValueOpnd, IR::LabelInstr* doneLabel, IR::Instr* dispatchStartLabel, IR::Instr* ldMethodFldInstr, IR::BailOutKind bailoutKind)
{
    // Label $bailout:
    // LdMethodFldPolyInlineMiss
    // BailOnNotPolymorphicInlinee $callOutBytecodeOffset - BailOutOnFailedPolymorphicInlineTypeCheck
    // ByteCoudeUses
    // BytecodeArgoutUses
    // returnValueOpnd = EndCallForPolymorphicInlinee actualsCount
    IR::LabelInstr* bailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func, /*helperLabel*/ true);
    callInstr->InsertBefore(bailOutLabel);
    dispatchStartLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::Br, bailOutLabel, callInstr->m_func));

    // Only fixed function inlining requires a ldMethodFldInstr
    if (ldMethodFldInstr)
    {
        callInstr->InsertBefore(ldMethodFldInstr);
    }

    callInstr->InsertBefore(IR::BailOutInstr::New(Js::OpCode::BailOnNotPolymorphicInlinee, bailoutKind, callInstr, callInstr->m_func));

    uint actualsCount = 0;
    callInstr->IterateArgInstrs([&](IR::Instr* argInstr) {
        IR::Instr* bytecodeArgOutUse = IR::Instr::New(Js::OpCode::BytecodeArgOutUse, callInstr->m_func);
        bytecodeArgOutUse->SetByteCodeOffset(callInstr);
        bytecodeArgOutUse->SetSrc1(argInstr->GetSrc1());
        callInstr->InsertBefore(bytecodeArgOutUse);
        actualsCount++;
        // Remove the original args
        argInstr->Remove();
        return false;
    });

    callInstr->InsertBefore(IR::ByteCodeUsesInstr::New(callInstr, callInstr->GetSrc1()->GetStackSym()->m_id));

    IR::Instr* endCallInstr = IR::Instr::New(Js::OpCode::EndCallForPolymorphicInlinee, callInstr->m_func);
    endCallInstr->SetSrc1(IR::IntConstOpnd::New(actualsCount + Js::Constants::InlineeMetaArgCount, TyInt32, callInstr->m_func, /*dontEncode*/ true));
    if (returnValueOpnd)
    {
        StackSym* returnValueSym = returnValueOpnd->m_sym->AsStackSym();
        IR::Opnd* dstOpnd = IR::RegOpnd::New(returnValueSym, returnValueSym->GetType(), callInstr->m_func);
        dstOpnd->SetValueType(returnValueOpnd->GetValueType());
        endCallInstr->SetDst(dstOpnd);
    }
    callInstr->InsertBefore(endCallInstr);
    callInstr->InsertBefore(doneLabel);
    callInstr->Remove(); // We don't need callInstr anymore.
}

//
// Inlines a function if it is a polymorphic inlining candidate.
// otherwise introduces a call to it.
// The IR for the args & calls is cloned to do this
//
void Inline::InsertOneInlinee(IR::Instr* callInstr, IR::RegOpnd* returnValueOpnd, IR::Opnd* methodOpnd,
    const InlineeData& inlineeData, IR::LabelInstr* doneLabel, const StackSym* symCallerThis, bool fixedFunctionSafeThis, uint recursiveInlineDepth)
{
    bool isInlined = inlineeData.inlineeJitTimeData->GetIsInlined();

    IR::Instr* currentCallInstr;
    if (isInlined)
    {
        currentCallInstr = IR::Instr::New(Js::OpCode::InlineeStart, IR::RegOpnd::New(TyVar, callInstr->m_func), methodOpnd, callInstr->m_func);
    }
    else
    {
        currentCallInstr = IR::Instr::New(callInstr->m_opcode, callInstr->m_func);
        currentCallInstr->SetSrc1(methodOpnd);
        if (returnValueOpnd)
        {
            currentCallInstr->SetDst(returnValueOpnd);
        }
    }
    currentCallInstr->SetIsCloned(true);
    callInstr->InsertBefore(currentCallInstr);
    this->CloneCallSequence(callInstr, currentCallInstr);

    if (isInlined)
    {
        Js::FunctionBody *funcBody = inlineeData.functionBody;
        Func *inlinee = BuildInlinee(funcBody, inlineeData, returnValueOpnd ? returnValueOpnd->m_sym->GetByteCodeRegSlot() : Js::Constants::NoRegister, callInstr, recursiveInlineDepth);

        IR::Instr *argOuts[Js::InlineeCallInfo::MaxInlineeArgoutCount];
#if DBG
        memset(argOuts, 0xFE, sizeof(argOuts));
#endif
        bool stackArgsArgOutExpanded = false;
        Js::ArgSlot actualCount = MapActuals(currentCallInstr, argOuts, Js::InlineeCallInfo::MaxInlineeArgoutCount, inlinee, (Js::ProfileId)callInstr->AsProfiledInstr()->u.profileId, &stackArgsArgOutExpanded);
        Assert(actualCount > 0);
        MapFormals(inlinee, argOuts, funcBody->GetInParamsCount(), actualCount, returnValueOpnd, currentCallInstr->GetSrc1(), symCallerThis, stackArgsArgOutExpanded, fixedFunctionSafeThis, argOuts);
        currentCallInstr->m_func = inlinee;

        // Put the meta arguments that the stack walker expects to find on the stack.
        // As all the argouts are shared among the inlinees, do this only once.
        SetupInlineeFrame(inlinee, currentCallInstr, actualCount, currentCallInstr->GetSrc1());

        IR::Instr* inlineeEndInstr = IR::Instr::New(Js::OpCode::InlineeEnd, inlinee);
        inlineeEndInstr->SetByteCodeOffset(inlinee->m_tailInstr->GetPrevRealInstr());
        inlineeEndInstr->SetSrc1(IR::IntConstOpnd::New(actualCount + Js::Constants::InlineeMetaArgCount, TyInt32, inlinee));
        inlineeEndInstr->SetSrc2(currentCallInstr->GetDst());
        inlinee->m_tailInstr->InsertBefore(inlineeEndInstr);

        // JMP to done at the end
        IR::Instr* doneInstr = IR::BranchInstr::New(Js::OpCode::Br, doneLabel, currentCallInstr->m_func);
        inlinee->m_tailInstr->InsertBefore(doneInstr);
        currentCallInstr->InsertRangeAfter(inlinee->m_headInstr->m_next, inlinee->m_tailInstr->m_prev);

        inlinee->m_headInstr->Free();
        inlinee->m_tailInstr->Free();
    }
    else
    {
        callInstr->InsertBefore(IR::BranchInstr::New(Js::OpCode::Br, doneLabel, callInstr->m_func));
    }
}

uint
Inline::HandleDifferentTypesSameFunction(__inout_ecount(cachedFixedInlineeCount) Js::FixedFieldInfo* fixedFunctionInfoArray, uint16 cachedFixedInlineeCount)
{
    uint16 uniqueCount = cachedFixedInlineeCount;
    uint16 swapIndex;
    for (uint16 i = 0; i < cachedFixedInlineeCount; i++)
    {
        swapIndex = i+1;
        for (uint16 j = i+1; j < cachedFixedInlineeCount; j++)
        {
            if (fixedFunctionInfoArray[i].fieldValue == fixedFunctionInfoArray[j].fieldValue)
            {
                Js::FixedFieldInfo tmpInfo = fixedFunctionInfoArray[j];
                fixedFunctionInfoArray[j] = fixedFunctionInfoArray[swapIndex];
                fixedFunctionInfoArray[swapIndex] = tmpInfo;
                fixedFunctionInfoArray[swapIndex - 1].nextHasSameFixedField = true;
                swapIndex++;
                uniqueCount--;
            }
        }
        i = swapIndex-1;
    }
    return uniqueCount;
}

void
Inline::SetInlineeFrameStartSym(Func *inlinee, uint actualCount)
{
    StackSym    *stackSym = inlinee->m_symTable->GetArgSlotSym((Js::ArgSlot)actualCount + 1);
    stackSym->m_isInlinedArgSlot = true;
    this->topFunc->SetArgOffset(stackSym, (currentInlineeFrameSlot) * MachPtr);
    inlinee->SetInlineeFrameStartSym(stackSym);
}

Func *
Inline::BuildInlinee(Js::FunctionBody* funcBody, const InlineeData& inlineeData, Js::RegSlot returnRegSlot, IR::Instr *callInstr, uint recursiveInlineDepth)
{
    Assert(callInstr->IsProfiledInstr());
    Js::ProfileId callSiteId = static_cast<Js::ProfileId>(callInstr->AsProfiledInstr()->u.profileId);
    Assert(callSiteId >= 0);

    Js::ProxyEntryPointInfo *defaultEntryPointInfo = funcBody->GetDefaultEntryPointInfo();
    Assert(defaultEntryPointInfo->IsFunctionEntryPointInfo());
    Js::FunctionEntryPointInfo *functionEntryPointInfo = static_cast<Js::FunctionEntryPointInfo*>(defaultEntryPointInfo);

    JsFunctionCodeGen *workItem = JitAnew(this->topFunc->m_alloc, JsFunctionCodeGen,
        funcBody->GetScriptContext()->GetNativeCodeGenerator(), funcBody, functionEntryPointInfo, this->topFunc->IsJitInDebugMode());
    workItem->SetRecyclableData(JitAnew(this->topFunc->m_alloc, Js::CodeGenRecyclableData, inlineeData.inlineeJitTimeData));
    workItem->SetJitMode(this->topFunc->m_workItem->GetJitMode());

    const auto profileInfo =
        JitAnew(
            this->topFunc->m_alloc,
            Js::ReadOnlyDynamicProfileInfo,
            funcBody->HasDynamicProfileInfo() ? funcBody->GetAnyDynamicProfileInfo() : nullptr,
            this->topFunc->IsBackgroundJIT() ? this->topFunc->m_alloc : nullptr);

    Js::EntryPointPolymorphicInlineCacheInfo * entryPointPolymorphicInlineCacheInfo = this->topFunc->m_workItem->GetEntryPoint()->GetPolymorphicInlineCacheInfo();
    Func *inlinee = JitAnew(this->topFunc->m_alloc,
                            Func,
                            this->topFunc->m_alloc,
                            workItem,
                            inlineeData.inlineeRuntimeData,
                            entryPointPolymorphicInlineCacheInfo ? entryPointPolymorphicInlineCacheInfo->GetInlineeInfo(funcBody) : nullptr,
                            this->topFunc->GetCodeGenAllocators(),
                            this->topFunc->GetNumberAllocator(),
                            profileInfo,
                            this->topFunc->GetCodeGenProfiler(),
                            this->topFunc->IsBackgroundJIT(),
                            callInstr->m_func,
                            callInstr->m_next->GetByteCodeOffset(),
                            returnRegSlot,
                            false,
                            callSiteId,
                            false);

    BuildIRForInlinee(inlinee, funcBody, callInstr, false, recursiveInlineDepth);
    return inlinee;
}

void
Inline::BuildIRForInlinee(Func *inlinee, Js::FunctionBody *funcBody, IR::Instr *callInstr, bool isApplyTarget, uint recursiveInlineDepth)
{
    Js::ArgSlot actualsCount = 0;
    IR::Instr *argOuts[Js::InlineeCallInfo::MaxInlineeArgoutCount];
#if DBG
    memset(argOuts, 0xFE, sizeof(argOuts));
#endif

    callInstr->IterateArgInstrs([&](IR::Instr* argInstr){
        StackSym *argSym = argInstr->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
        argOuts[argSym->GetArgSlotNum() - 1] = argInstr;
        actualsCount++;
        return false;
    });

    inlinee->actualCount = actualsCount;

    inlinee->m_symTable = this->topFunc->m_symTable;

    inlinee->m_symTable->SetIDAdjustment();
    inlinee->m_symTable->IncreaseStartingID(funcBody->GetLocalsCount());

    BEGIN_CODEGEN_PHASE(this->topFunc, Js::IRBuilderPhase);

    IRBuilder irBuilder(inlinee);
    irBuilder.Build();

    END_CODEGEN_PHASE_NO_DUMP(this->topFunc, Js::IRBuilderPhase);

    inlinee->m_symTable->ClearIDAdjustment();

    Inline recursiveInliner(this->topFunc, this->inliningHeuristics, this->isInLoop, currentInlineeFrameSlot + Js::Constants::InlineeMetaArgCount + actualsCount, isApplyTarget);
    recursiveInliner.Optimize(inlinee, argOuts, actualsCount, inlinee->GetJnFunction() == callInstr->m_func->GetJnFunction() ? recursiveInlineDepth + 1 : 0);

#ifdef DBG
    Js::ArgSlot formalCount = funcBody->GetInParamsCount();

    if (formalCount > Js::InlineeCallInfo::MaxInlineeArgoutCount)
    {
        Fatal();
    }
#endif
}

bool
Inline::TryOptimizeCallInstrWithFixedMethod(IR::Instr *callInstr, Js::FunctionInfo* functionInfo, bool isPolymorphic, bool isBuiltIn, bool isCtor, bool isInlined, bool &safeThis,
                                            bool dontOptimizeJustCheck, uint i /*i-th inlinee at a polymorphic call site*/)
{
    Assert(!callInstr->m_func->GetJnFunction()->GetHasTry());

    if (PHASE_OFF(Js::FixedMethodsPhase, callInstr->m_func->GetJnFunction()))
    {
        return false;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
#define TRACE_FIXED_FIELDS 1
#endif

#if TRACE_FIXED_FIELDS
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    bool printFixedFieldsTrace =
        ((PHASE_TRACE(Js::FixedMethodsPhase, callInstr->m_func->GetJnFunction()) || PHASE_TESTTRACE(Js::FixedMethodsPhase, callInstr->m_func->GetJnFunction()) ||
        (isCtor && PHASE_TRACE(Js::FixedNewObjPhase, callInstr->m_func->GetJnFunction()) || PHASE_TESTTRACE(Js::FixedNewObjPhase, callInstr->m_func->GetJnFunction()))) && !dontOptimizeJustCheck);

    if (printFixedFieldsTrace)
    {
        Js::FunctionBody* callerFunctionBody = callInstr->m_func->GetJnFunction();
        Js::FunctionBody* calleeFunctionBody = functionInfo != nullptr && functionInfo->HasBody() ? functionInfo->GetFunctionBody() : nullptr;
        const wchar_t* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : L"<unknown>";

        Output::Print(L"FixedFields: function %s (%s): considering method <unknown> (%s %s): polymorphic = %d, built-in = %d, ctor = %d, inlined = %d, functionInfo = %p.\n",
            callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer), calleeName,
            calleeFunctionBody ? calleeFunctionBody->GetDebugNumberSet(debugStringBuffer2) : L"(null)",
            isPolymorphic, isBuiltIn, isCtor, isInlined, functionInfo);
        Output::Flush();
    }
#endif

    if (isPolymorphic && isInlined)
    {
        Assert(dontOptimizeJustCheck);
    }

    StackSym* methodValueSym = callInstr->GetSrc1()->AsRegOpnd()->m_sym->AsStackSym();
    if (!methodValueSym->IsSingleDef())
    {
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {
            Js::FunctionBody* callerFunctionBody = callInstr->m_func->GetJnFunction();
            Js::FunctionBody* calleeFunctionBody = functionInfo != nullptr && functionInfo->HasBody() ? functionInfo->GetFunctionBody() : nullptr;
            const wchar_t* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : L"<unknown>";

            Output::Print(L"FixedFields: function %s (%s): %s non-fixed method <unknown> (%s %s), because callee is not single def.\n",
                callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer),
                functionInfo != nullptr ? L"inlining" : L"calling", calleeName,
                calleeFunctionBody ? calleeFunctionBody->GetDebugNumberSet(debugStringBuffer2) : L"(null)");
            Output::Flush();
        }
#endif
        return false;
    }

    IR::Instr* ldMethodFldInstr = methodValueSym->GetInstrDef();
    if (ldMethodFldInstr->m_opcode != Js::OpCode::ScopedLdMethodFld
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdRootMethodFld
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdMethodFld
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdRootFld
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdFld
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdFldForCallApplyTarget
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdMethodFromFlags)
    {
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {
            Js::FunctionBody* callerFunctionBody = callInstr->m_func->GetJnFunction();
            Js::FunctionBody* calleeFunctionBody = functionInfo != nullptr && functionInfo->HasBody() ? functionInfo->GetFunctionBody() : nullptr;
            const wchar_t* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : L"<unknown>";

            Output::Print(L"FixedFields: function %s (%s): %s non-fixed method <unknown> (%s %s), because callee does not come from LdMethodFld.\n",
                callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer),
                functionInfo != nullptr ? L"inlining" : L"calling", calleeName,
                calleeFunctionBody ? calleeFunctionBody->GetDebugNumberSet(debugStringBuffer2) : L"(null)");
            Output::Flush();
        }
#endif
        return false;
    }

    IR::PropertySymOpnd* methodPropertyOpnd = ldMethodFldInstr->GetSrc1()->AsPropertySymOpnd();

    if ((isCtor &&
            ((isInlined && PHASE_OFF(Js::FixedCtorInliningPhase, callInstr->m_func->GetJnFunction())) ||
            (!isInlined && PHASE_OFF(Js::FixedCtorCallsPhase, callInstr->m_func->GetJnFunction())) ||
            (methodPropertyOpnd->UsesAccessor()))) ||
        (!isCtor &&
            ((isBuiltIn &&
                ((isInlined && PHASE_OFF(Js::FixedBuiltInMethodInliningPhase, callInstr->m_func->GetJnFunction())) ||
                (!isInlined && PHASE_OFF(Js::FixedBuiltInMethodCallsPhase, callInstr->m_func->GetJnFunction())))) ||
            (!isBuiltIn &&
                ((isInlined && PHASE_OFF(Js::FixedScriptMethodInliningPhase, callInstr->m_func->GetJnFunction())) ||
                (!isInlined && !PHASE_ON(Js::FixedScriptMethodCallsPhase, callInstr->m_func->GetJnFunction()))))))
       )
    {
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {
            Js::FunctionBody* callerFunctionBody = callInstr->m_func->GetJnFunction();
            Js::FunctionBody* calleeFunctionBody = functionInfo != nullptr && functionInfo->HasBody() ? functionInfo->GetFunctionBody() : nullptr;
            const wchar_t* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : L"<unknown>";

            Js::PropertyId methodPropertyId = callerFunctionBody->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callerFunctionBody->GetScriptContext()->GetPropertyNameLocked(methodPropertyId);

            Output::Print(L"FixedFields: function %s (#%u): %s non-fixed method %s (%s #%u) (cache id: %d), because %s fixed %s %s is disabled.\n",
                callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer),
                functionInfo != nullptr ? L"inlining" : L"calling", methodPropertyRecord->GetBuffer(), calleeName,
                calleeFunctionBody ? calleeFunctionBody->GetDebugNumberSet(debugStringBuffer2) : L"(null)",
                methodPropertyOpnd->m_inlineCacheIndex, isInlined ? L"inlining" : L"calling", isBuiltIn ? L"built-in" : L"script",
                isCtor ? L"ctors" : L"methods");
            Output::Flush();
        }
#endif
        return false;
    }

    if (!methodPropertyOpnd->IsObjTypeSpecCandidate() && !methodPropertyOpnd->IsRootObjectNonConfigurableFieldLoad())
    {
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {
            Js::FunctionBody* callerFunctionBody = callInstr->m_func->GetJnFunction();
            Js::FunctionBody* calleeFunctionBody = functionInfo != nullptr && functionInfo->HasBody() ? functionInfo->GetFunctionBody() : nullptr;
            const wchar_t* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : L"<unknown>";
            Js::PropertyId methodPropertyId = callerFunctionBody->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callerFunctionBody->GetScriptContext()->GetPropertyNameLocked(methodPropertyId);

            Output::Print(L"FixedFields: function %s (%s): %s non-fixed method %s (%s %s) (cache id: %d), because inline cache has no cached type.\n",
                callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer),
                functionInfo != nullptr ? L"inlining" : L"calling", methodPropertyRecord->GetBuffer(), calleeName,
                calleeFunctionBody ? calleeFunctionBody->GetDebugNumberSet(debugStringBuffer2) : L"(null)",
                methodPropertyOpnd->m_inlineCacheIndex);
            Output::Flush();
        }
#endif
        return false;
    }

    Js::JavascriptFunction const * functionObject = nullptr;
    if (!isPolymorphic)
    {
        functionObject = methodPropertyOpnd->HasFixedValue() ? methodPropertyOpnd->GetFieldValueAsFixedFunction() : nullptr;
    }
    else if (isPolymorphic && isInlined)
    {
        functionObject = methodPropertyOpnd->HasFixedValue() ? methodPropertyOpnd->GetFieldValueAsFixedFunction(i) : nullptr;
    }

    if (!functionObject)
    {
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {
            Js::FunctionBody* callerFunctionBody = callInstr->m_func->GetJnFunction();
            Js::FunctionBody* calleeFunctionBody = functionInfo != nullptr && functionInfo->HasBody() ? functionInfo->GetFunctionBody() : nullptr;
            const wchar_t* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : L"<unknown>";

            Js::PropertyId methodPropertyId = callerFunctionBody->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callerFunctionBody->GetScriptContext()->GetPropertyNameLocked(methodPropertyId);

            Output::Print(L"FixedFields: function %s (%s): %s non-fixed method %s (%s %s) (cache id: %d, layout: %s), because inline cache has no fixed function object.\n",
                callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer),
                functionInfo != nullptr ? L"inlining" : L"calling", methodPropertyRecord->GetBuffer(), calleeName,
                calleeFunctionBody ? calleeFunctionBody->GetDebugNumberSet(debugStringBuffer2) : L"(null)",
                methodPropertyOpnd->m_inlineCacheIndex,
                methodPropertyOpnd->IsLoadedFromProto() ? L"proto" : methodPropertyOpnd->UsesAccessor() ? L"accessor" : L"local");
            Output::Flush();
        }
#endif
        return false;
    }

    // Certain built-ins that we decide not to inline will get a fast path emitted by the lowerer.
    // The lowering code cannot handle a call with a fixed function target, because it needs access to
    // the original property sym. Turn off fixed method calls for these cases.
    if (functionInfo == nullptr && Func::IsBuiltInInlinedInLowerer(callInstr->GetSrc1()))
    {
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {
            Js::FunctionBody* callerFunctionBody = callInstr->m_func->GetJnFunction();
            Js::FunctionBody* calleeFunctionBody = functionInfo != nullptr && functionInfo->HasBody() ? functionInfo->GetFunctionBody() : nullptr;
            const wchar_t* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : L"<unknown>";

            Js::PropertyId methodPropertyId = callerFunctionBody->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callerFunctionBody->GetScriptContext()->GetPropertyNameLocked(methodPropertyId);

            Output::Print(L"FixedFields: function %s (%s): %s non-fixed method %s (%s %s) (cache id: %d, layout: %s), because callee is a built-in with fast path in lowerer.\n",
                callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer),
                functionInfo != nullptr ? L"inlining" : L"calling", methodPropertyRecord->GetBuffer(), calleeName,
                calleeFunctionBody ? calleeFunctionBody->GetDebugNumberSet(debugStringBuffer2) : L"(null)",
                methodPropertyOpnd->m_inlineCacheIndex,
                methodPropertyOpnd->IsLoadedFromProto() ? L"proto" : methodPropertyOpnd->UsesAccessor() ? L"accessor" : L"local");
            Output::Flush();
        }
#endif
        return false;
    }

    if (functionInfo != nullptr && functionObject->GetFunctionInfo() != functionInfo)
    {
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {
            wchar_t debugStringBuffer3[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Js::FunctionBody* callerFunctionBody = callInstr->m_func->GetJnFunction();
            Js::PropertyId methodPropertyId = callerFunctionBody->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callerFunctionBody->GetScriptContext()->GetPropertyNameLocked(methodPropertyId);
            bool isProto = methodPropertyOpnd->IsLoadedFromProto();
            bool isAccessor = methodPropertyOpnd->UsesAccessor();
            Js::DynamicObject* protoObject = isProto ? methodPropertyOpnd->GetProtoObject() : nullptr;
            Js::FunctionBody* fixedFunctionBody   = functionObject->GetFunctionInfo()->GetFunctionBody();
            const wchar_t* fixedFunctionNumbers   = fixedFunctionBody ? fixedFunctionBody->GetDebugNumberSet(debugStringBuffer2) : L"(null)";
            Js::FunctionBody* profileFunctionBody = functionInfo->GetFunctionBody();
            const wchar_t* profileFunctionName    = profileFunctionBody != nullptr ? profileFunctionBody->GetDisplayName() : L"<unknown>";
            const wchar_t* profileFunctionNumbers = profileFunctionBody ? profileFunctionBody->GetDebugNumberSet(debugStringBuffer3) : L"(null)";

            if (PHASE_TRACE(Js::FixedMethodsPhase, callInstr->m_func->GetJnFunction()))
            {
                Output::Print(L"FixedFields: function %s (#%s): function body mismatch for inlinee: %s (%s) 0x%p->0x%p != %s (%s) 0x%p (cache id: %d, layout: %s, type: 0x%p, proto: 0x%p, proto type: 0x%p).\n",
                    callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer),
                    methodPropertyRecord->GetBuffer(), fixedFunctionNumbers, functionObject, functionObject->GetFunctionInfo(),
                    profileFunctionName, profileFunctionNumbers, functionInfo,
                    methodPropertyOpnd->m_inlineCacheIndex, isProto ? L"proto" : isAccessor ? L"accessor" : L"local",
                    methodPropertyOpnd->GetType(), protoObject, protoObject != nullptr ? protoObject->GetType() : nullptr);
            }
            if (PHASE_TESTTRACE(Js::FixedMethodsPhase, callInstr->m_func->GetJnFunction()))
            {
                Output::Print(L"FixedFields: function %s (%s): function body mismatch for inlinee: %s (%s) != %s (%s) (cache id: %d, layout: %s).\n",
                    callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer),
                    methodPropertyRecord->GetBuffer(), fixedFunctionNumbers, profileFunctionName, profileFunctionNumbers,
                    methodPropertyOpnd->m_inlineCacheIndex, isProto ? L"proto" : isAccessor ? L"accessor" : L"local");
            }
            Output::Flush();
        }
#endif
        // It appears that under certain bailout and re-JIT conditions we may end up with an updated
        // inline cache pointing to a new function object, while the call site profile info still
        // holds the old function body.  If the two don't match, let's fall back on the regular LdMethodFld.
        return false;
    }
    else
    {
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {
            Js::FunctionBody* callerFunctionBody = callInstr->m_func->GetJnFunction();
            Js::FunctionBody* calleeFunctionBody = functionInfo != nullptr && functionInfo->HasBody() ? functionInfo->GetFunctionBody() : nullptr;
            Js::PropertyId methodPropertyId = callerFunctionBody->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callerFunctionBody->GetScriptContext()->GetPropertyNameLocked(methodPropertyId);
            const wchar_t* fixedFunctionName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : L"<unknown>";
            Js::FunctionBody* fixedFunctionBody = functionObject->GetFunctionInfo()->GetFunctionBody();
            const wchar_t* fixedFunctionNumbers = fixedFunctionBody ? fixedFunctionBody->GetDebugNumberSet(debugStringBuffer2) : L"(null)";

            Output::Print(L"FixedFields: function %s (%s): %s fixed method %s (%s %s) (cache id: %d, layout: %s).\n",
                callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer),
                functionInfo != nullptr ? L"inlining" : L"calling",
                methodPropertyRecord->GetBuffer(), fixedFunctionName, fixedFunctionNumbers,
                methodPropertyOpnd->m_inlineCacheIndex,
                methodPropertyOpnd->IsLoadedFromProto() ? L"proto" : methodPropertyOpnd->UsesAccessor() ? L"accessor" : L"local");
            Output::Flush();
        }
#endif
    }

#undef TRACE_FIXED_FIELDS

    if (dontOptimizeJustCheck)
    {
        return true;
    }

    // Change Ld[Root]MethodFld, LdMethodFromFlags to CheckFixedFld, which doesn't need a dst.
    if(ldMethodFldInstr->m_opcode == Js::OpCode::LdMethodFromFlags)
    {
        Assert(ldMethodFldInstr->HasBailOutInfo());
        ldMethodFldInstr->ClearBailOutInfo();
    }
    ldMethodFldInstr->m_opcode = Js::OpCode::CheckFixedFld;
    IR::Opnd * methodValueDstOpnd = ldMethodFldInstr->UnlinkDst();
    IR::Instr * chkMethodFldInstr = ldMethodFldInstr->ConvertToBailOutInstr(ldMethodFldInstr,
        !methodPropertyOpnd->HasEquivalentTypeSet() ? IR::BailOutFailedFixedFieldTypeCheck : IR::BailOutFailedEquivalentFixedFieldTypeCheck);
    chkMethodFldInstr->GetBailOutInfo()->polymorphicCacheIndex = methodPropertyOpnd->m_inlineCacheIndex;

    Assert(chkMethodFldInstr->GetSrc1()->IsSymOpnd());
    if (chkMethodFldInstr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
    {
        Assert(chkMethodFldInstr->m_opcode == Js::OpCode::CheckFixedFld);
        IR::PropertySymOpnd* chkMethodFldOpnd = chkMethodFldInstr->GetSrc1()->AsPropertySymOpnd();
        // For polymorphic field loads we only support fixed functions on prototypes. This helps keep the equivalence check helper simple.
        Assert(chkMethodFldOpnd->IsMono() || chkMethodFldOpnd->IsLoadedFromProto() || chkMethodFldOpnd->UsesAccessor());
        chkMethodFldOpnd->SetUsesFixedValue(true);
    }

    if (isCtor)
    {
        Js::JitTimeConstructorCache* constructorCache = methodPropertyOpnd->GetCtorCache();
        if (constructorCache != nullptr && callInstr->IsProfiledInstr())
        {

#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (PHASE_TRACE(Js::FixedNewObjPhase, callInstr->m_func->GetJnFunction()) || PHASE_TESTTRACE(Js::FixedNewObjPhase, callInstr->m_func->GetJnFunction()))
            {
                Js::FunctionBody* callerFunctionBody = callInstr->m_func->GetJnFunction();
                Js::FunctionBody* calleeFunctionBody = functionInfo != nullptr && functionInfo->HasBody() ? functionInfo->GetFunctionBody() : nullptr;
                Js::PropertyId methodPropertyId = callerFunctionBody->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
                Js::PropertyRecord const * const methodPropertyRecord = callerFunctionBody->GetScriptContext()->GetPropertyNameLocked(methodPropertyId);
                const wchar_t* fixedFunctionName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : L"<unknown>";
                Js::FunctionBody* fixedFunctionBody = functionObject->GetFunctionInfo()->GetFunctionBody();
                const wchar_t* fixedFunctionNumbers = fixedFunctionBody ? fixedFunctionBody->GetDebugNumberSet(debugStringBuffer2) : L"(null)";

                Output::Print(L"FixedNewObj: function %s (%s): fixed new object for %s with %s ctor %s (%s %s)%s\n",
                    callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer), Js::OpCodeUtil::GetOpCodeName(callInstr->m_opcode),
                    functionInfo != nullptr ? L"inlined" : L"called",
                    methodPropertyRecord->GetBuffer(), fixedFunctionName, fixedFunctionNumbers,
                    constructorCache->skipNewScObject ? L" skip default object" : L"");
                Output::Flush();
            }
#endif

            // The profile ID's hung from array ctor opcodes don't match up with normal profiled call sites.
            if (callInstr->m_opcode != Js::OpCode::NewScObjArray)
            {
                // Because we are storing flow sensitive info in the cache (guarded property operations),
                // we must make sure the same cache cannot be used multiple times in the flow.
                if (constructorCache->isUsed)
                {
                    // It's okay to allocate a JitTimeConstructorCache from the func's allocator (rather than recycler),
                    // because we only use these during JIT. We use the underlying runtime cache as a guard that must
                    // live after JIT, and these are added to the EntryPointInfo during work item creation and thus kept alive.
                    constructorCache = constructorCache->Clone(this->topFunc->m_alloc);
                }
                Assert(!constructorCache->isUsed);
                constructorCache->isUsed = true;
                callInstr->m_func->SetConstructorCache(static_cast<Js::ProfileId>(callInstr->AsProfiledInstr()->u.profileId), constructorCache);
            }
        }
        else
        {
#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (PHASE_TRACE(Js::FixedNewObjPhase, callInstr->m_func->GetJnFunction()) || PHASE_TESTTRACE(Js::FixedNewObjPhase, callInstr->m_func->GetJnFunction()))
            {
                Js::FunctionBody* callerFunctionBody = callInstr->m_func->GetJnFunction();
                Js::FunctionBody* calleeFunctionBody = functionInfo != nullptr && functionInfo->HasBody() ? functionInfo->GetFunctionBody() : nullptr;
                Js::PropertyId methodPropertyId = callerFunctionBody->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
                Js::PropertyRecord const * const methodPropertyRecord = callerFunctionBody->GetScriptContext()->GetPropertyNameLocked(methodPropertyId);
                const wchar_t* fixedFunctionName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : L"<unknown>";
                Js::FunctionBody* fixedFunctionBody = functionObject->GetFunctionInfo()->GetFunctionBody();
                const wchar_t* fixedFunctionNumbers = fixedFunctionBody ? fixedFunctionBody->GetDebugNumberSet(debugStringBuffer2) : L"(null)";

                Output::Print(L"FixedNewObj: function %s (%s): non-fixed new object for %s with %s ctor %s (%s %s), because %s.\n",
                    callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer), Js::OpCodeUtil::GetOpCodeName(callInstr->m_opcode),
                    functionInfo != nullptr ? L"inlined" : L"called",
                    methodPropertyRecord->GetBuffer(), fixedFunctionName, fixedFunctionNumbers,
                    constructorCache == nullptr ? L"constructor cache hasn't been cloned" : L"instruction isn't profiled");
                Output::Flush();
            }
#endif
        }
    }

    // Insert a load instruction to place the constant address in methodOpnd (the Ld[Root]MethodFld's original dst).
    IR::AddrOpnd * constMethodValueOpnd = IR::AddrOpnd::New((Js::Var)functionObject, IR::AddrOpndKind::AddrOpndKindDynamicVar, callInstr->m_func);
    constMethodValueOpnd->m_isFunction = true;
    IR::Instr * ldMethodValueInstr = IR::Instr::New(Js::OpCode::Ld_A, methodValueDstOpnd, constMethodValueOpnd, callInstr->m_func);
    StackSym* methodSym = methodValueDstOpnd->AsRegOpnd()->m_sym;
    if (methodSym->IsSingleDef())
    {
        methodSym->SetIsConst();
    }
    methodValueDstOpnd->SetValueType(ValueType::FromObject((Js::RecyclableObject* const)functionObject));
    chkMethodFldInstr->InsertAfter(ldMethodValueInstr);
    callInstr->ReplaceSrc1(constMethodValueOpnd);
    if (callInstr->m_opcode == Js::OpCode::CallI || callInstr->CallsAccessor(methodPropertyOpnd))
    {
        callInstr->m_opcode = Js::OpCode::CallIFixed;
    }
    else
    {
        // We patch later for constructor inlining.
        Assert(
            callInstr->m_opcode == Js::OpCode::NewScObject ||
            callInstr->m_opcode == Js::OpCode::NewScObjArray);
    }

    if (!isBuiltIn && isInlined)
    {
        // We eliminate CheckThis for fixed method inlining. Assert here that our assumption is true.
        Js::TypeId typeId = methodPropertyOpnd->IsRootObjectNonConfigurableField() ?
            Js::TypeIds_GlobalObject : methodPropertyOpnd->GetTypeId();
        if(typeId > Js::TypeIds_LastJavascriptPrimitiveType && typeId <= Js::TypeIds_LastTrueJavascriptObjectType)
        {
            // Eliminate CheckThis for inlining.
            safeThis = true;
        }
    }
    return true;
}

Js::Var
Inline::TryOptimizeInstrWithFixedDataProperty(IR::Instr *&instr)
{
    if (PHASE_OFF(Js::UseFixedDataPropsPhase, instr->m_func->GetJnFunction()) ||
        PHASE_OFF(Js::UseFixedDataPropsInInlinerPhase, instr->m_func->GetJnFunction()))
    {
        return nullptr;
    }
    if (!instr->IsProfiledInstr() ||
        !instr->GetSrc1()->IsSymOpnd() || !instr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
    {
        return nullptr;
    }
    if (!OpCodeAttr::CanLoadFixedFields(instr->m_opcode))
    {
        return nullptr;
    }
    return instr->TryOptimizeInstrWithFixedDataProperty(&instr, nullptr);
}

// Inline a built-in/math function call, such as Math.sin(x).
// Main idea on what happens with IR during different stages.
// 1) Copy args from ArgOuts into inline instr.
// 2) Change opcode: ArgOut_A -> ArgOut_A_InlineBuiltIn (aka BIA).
// 3) Notes:
//    - General logic is similar to inlining regular functions, except that:
//      - There are no inner instructions to inline.
//      - We don't need to support arguments object inside the inlinee - don't need inlinee meta frame, etc.
//    - ArgOuts are linked through src2->m_sym->m_instrDef.
//    - ArgOuts are not needed for the inlined call itself, but we can't remove them because they are needed for bailout.
//      We convert them to ArgOut_A_InlineBuiltIn.
// Example for Math.pow(x, y), x86 case.
// Original:
//     instrS: dstS = StartCall <N=count>, NULL -- N is actual number of parameters, including "this".
//     instr0: arg0 = ArgOut t, link(->instrS)  -- "this" arg
//     instr1: arg1 = ArgOut x, link(->instr0)  -- src1
//     instr2: arg2 = ArgOut y, link(->instr1)  -- src2
//     instr3: dstC = CallI fn, link(->instr2)  -- links to instr2, etc.
// After Inline:
//     instrS: dstS = StartCall <N=count>, NULL -- N is actual number of parameters, including "this".
//             tmpt = BytecodeArgOutCapture t                                     -- create assigns to temps to snapshot argout values in case they are modified later before the call
//             tmpx = BytecodeArgOutCapture x
//             tmpy = BytecodeArgOutCapture y
//     instr1: arg1 = ArgOut_InlineBuiltIn tmpx, link(->instr0)  -- src1
//     instr0: arg0 = ArgOut_InlineBuiltIn tmpt, link(->instrS)  -- "this" arg    -- Change ArgOut_a to ArgOut_A_InlineBuiltIn
//     instr2: arg2 = ArgOut_InlineBuiltIn tmpy, link(->instr1)  -- src2
//             NULL = InlineBuiltInStart fn, link(->instr2)
//             dstC = InlineMathPow, tmpx, tmpy       -- actual native math call.
//             NULL = InlineBuiltInEnd <N=count>, link(->instr2)
// After Globopt:
//     instrS: dstS = StartCall <N=count>, NULL -- N is actual number of parameters, including "this".
//             tmpt = BytecodeArgOutCapture t                                     -- create assigns to temps to snapshot argout values in case they are modified later before the call
//             tmpx = BytecodeArgOutCapture x
//             Bailout 1
//             tmpy = BytecodeArgOutCapture y
//             Bailout 2
//     instr1: arg1 = ArgOut_InlineBuiltIn tmpx, link(->instr0)  -- src1
//     instr0: arg0 = ArgOut_InlineBuiltIn tmpt, link(->instrS)  -- "this" arg    -- Change ArgOut_a to ArgOut_A_InlineBuiltIn
//     instr2: arg2 = ArgOut_InlineBuiltIn tmpy, link(->instr1)  -- src2
//                    ...
//             NULL = InlineBuiltInStart fn, link(->instr2) -- Note that InlineBuiltInStart is after last bailout.
//                                                             This is important so that fn used for bailout is after last bailout.
//             dstC = InlineMathPow, tmpx, tmpy       -- actual native math call.
//             NULL = InlineBuiltInEnd <N=count>, link(->instr2)
// After Lowerer:
//                    ...
//         s1(XMM0) = MOVSD tmpx
//         s2(XMM1) = MOVSD tmpy
//         s1(XMM0) = CALL pow                  -- actual native math call.
//             dstC = MOVSD s1(XMM0)

IR::Instr *
Inline::InlineBuiltInFunction(IR::Instr *callInstr, Js::FunctionInfo *funcInfo, Js::OpCode inlineCallOpCode, const Js::FunctionCodeGenJitTimeData* inlinerData, const StackSym *symCallerThis, bool* pIsInlined, uint profileId, uint recursiveInlineDepth)
{
    Assert(callInstr);
    Assert(funcInfo);
    Assert(inlinerData);
    Assert(inlineCallOpCode != 0);

    // We may still decide not to inline.
    *pIsInlined = false;

    // Inlining is profile-based, so get the built-in function from profile rather than from the callInstr's opnd.
    Js::BuiltinFunction builtInId = Js::JavascriptLibrary::GetBuiltInForFuncInfo(funcInfo, callInstr->m_func->GetScriptContext());

#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
    if(inlineCallOpCode == Js::OpCode::InlineMathFloor || inlineCallOpCode == Js::OpCode::InlineMathCeil || inlineCallOpCode == Js::OpCode::InlineMathRound)
    {
#if defined(_M_IX86) || defined(_M_X64)
        if (!AutoSystemInfo::Data.SSE4_1Available())
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: SSE4.1 not available\tInlinee: %s (#%d)\tCaller: %s\n", Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId, inlinerData->GetFunctionBody()->GetDisplayName());
            return callInstr->m_next;
        }
#endif
        if(callInstr->m_func->GetTopFunc()->GetProfileInfo()->IsFloorInliningDisabled())
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Floor Inlining Disabled\tInlinee: %s (#%d)\tCaller: %s\n", Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId, inlinerData->GetFunctionBody()->GetDisplayName());
            return callInstr->m_next;
        }
    }

    if (callInstr->GetSrc2() &&
        callInstr->GetSrc2()->IsSymOpnd() &&
        callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum() > Js::InlineeCallInfo::MaxInlineeArgoutCount)
    {
        // This is a hard limit as we only use 4 bits to encode the actual count in the InlineeCallInfo. Although
        // InliningDecider already checks for this, the check is against profile data that may not be accurate since profile
        // data matching does not take into account some types of changes to source code. Need to check this again with current
        // information.
        INLINE_TESTTRACE(L"INLINING: Skip Inline: ArgSlot > MaxInlineeArgoutCount\tInlinee: %s (#%d)\tArgSlotNum: %d\tMaxInlineeArgoutCount: %d\tCaller: %s (#%d)\n",
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId, callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum(),
            Js::InlineeCallInfo::MaxInlineeArgoutCount, inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    Js::BuiltInFlags builtInFlags = Js::JavascriptLibrary::GetFlagsForBuiltIn(builtInId);

    bool isAnyArgFloat = (builtInFlags & Js::BuiltInFlags::BIF_TypeSpecAllToFloat) != 0;
    if (isAnyArgFloat && !GlobOpt::DoFloatTypeSpec(this->topFunc))
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: float type spec is off\tInlinee: %s (#%d)\tCaller: %s (%s)\n",
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    bool canDstBeFloat = (builtInFlags & Js::BuiltInFlags::BIF_TypeSpecDstToFloat) != 0;
    if (canDstBeFloat && !Js::JavascriptLibrary::CanFloatPreferenceFunc(builtInId) && inlineCallOpCode != Js::OpCode::InlineArrayPop)
    {
        // Note that for Math.abs that means that even though it can potentially be type-spec'd to int, we won't inline it.
        // Some built-in functions, such as atan2, are disabled for float-pref.
        INLINE_TESTTRACE(L"INLINING: Skip Inline: Cannot float-type-spec the inlinee\tInlinee: %s (#%d)\tCaller: %s (%s)\n",
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId, // Get the _value (cause operator _E) to avoid using struct directly.
            inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    bool isAnyArgInt = (builtInFlags & (Js::BuiltInFlags::BIF_TypeSpecDstToInt | Js::BuiltInFlags::BIF_TypeSpecSrc1ToInt | Js::BuiltInFlags::BIF_TypeSpecSrc2ToInt)) != 0;
    if (isAnyArgInt && !GlobOpt::DoAggressiveIntTypeSpec(this->topFunc))
    {
        // Note that for Math.abs that means that even though it can potentially be type-spec'd to float, we won't inline it.
        INLINE_TESTTRACE(L"INLINING: Skip Inline: int type spec is off\tInlinee: %s (#%d)\tCaller: %s (%s)\n",
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    if(inlineCallOpCode == Js::OpCode::InlineMathImul && !GlobOpt::DoLossyIntTypeSpec(topFunc))
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: lossy int type spec is off, it's required for Math.imul to do | 0 on src opnds\tInlinee: %s (#%d)\tCaller: %s (%s)\n",
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    if(inlineCallOpCode == Js::OpCode::InlineMathClz32 && !GlobOpt::DoLossyIntTypeSpec(topFunc))
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: lossy int type spec is off, it's required for Math.clz32 to do | 0 on src opnds\tInlinee: %s (#%d)\tCaller: %s (%s)\n",
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    if (inlineCallOpCode == Js::OpCode::InlineFunctionApply && (!callInstr->m_func->GetHasStackArgs() || this->topFunc->GetJnFunction()->IsInlineApplyDisabled()))
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: stack args of inlining is off\tInlinee: %s (#%d)\tCaller: %s (%s)\n",
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    // TODO: when adding support for other type spec args (array, string) do appropriate check as well.

    Assert(callInstr->GetSrc1());
    Assert(callInstr->GetSrc1()->IsRegOpnd());
    Assert(callInstr->GetSrc1()->AsRegOpnd()->m_sym);

    if (!(builtInFlags & Js::BuiltInFlags::BIF_IgnoreDst) && callInstr->GetDst() == nullptr && inlineCallOpCode != Js::OpCode::InlineArrayPop)
    {
        // Is seems that it's not worth optimizing odd cases where the result is unused.
        INLINE_TESTTRACE(L"INLINING: Skip Inline: inlinee's return value is not assigned to anything\tInlinee: %s (#%d)\tCaller: %s (%s)\n",
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    // Number of arguments, not including "this".
    IntConstType requiredInlineCallArgCount = (IntConstType)Js::JavascriptLibrary::GetArgCForBuiltIn(builtInId);

    IR::Opnd* linkOpnd = callInstr->GetSrc2();
    Js::ArgSlot actualCount = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum();

    // Check for missing actuals:
    // if number of passed params to built-in function is not what it needs, don't inline.
    int inlineCallArgCount = (int)((builtInFlags & Js::BuiltInFlags::BIF_UseSrc0) != 0 ? actualCount : actualCount - 1);
    Assert(inlineCallArgCount >= 0);

    if (linkOpnd->IsSymOpnd())
    {
#if ENABLE_DEBUG_CONFIG_OPTIONS
        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
        if((builtInFlags & Js::BuiltInFlags::BIF_VariableArgsNumber) != 0)
        {
            if(inlineCallArgCount > requiredInlineCallArgCount)
            {
                INLINE_TESTTRACE(L"INLINING: Skip Inline: parameter count exceeds the maximum number of parameters allowed\tInlinee: %s (#%d)\tCaller: %s (%s)\n",
                    Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
                    inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
                return callInstr->m_next;
            }
        }
        else if(inlineCallArgCount != requiredInlineCallArgCount)
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: parameter count doesn't match dynamic profile\tInlinee: %s (#%d)\tCaller: %s (%s)\n",
                Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
                inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
            return callInstr->m_next;
        }
    }

    IR::Instr *inlineBuiltInEndInstr = nullptr;
    if (inlineCallOpCode ==  Js::OpCode::InlineFunctionApply)
    {
       inlineBuiltInEndInstr = InlineApply(callInstr, funcInfo, inlinerData, symCallerThis, pIsInlined, profileId, recursiveInlineDepth);
       return inlineBuiltInEndInstr->m_next;
    }

    if (inlineCallOpCode ==  Js::OpCode::InlineFunctionCall)
    {
       inlineBuiltInEndInstr = InlineCall(callInstr, funcInfo, inlinerData, symCallerThis, pIsInlined, profileId, recursiveInlineDepth);
       return inlineBuiltInEndInstr->m_next;
    }


#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    InliningDecider::TraceInlining(inlinerData->GetFunctionBody(), Js::JavascriptLibrary::GetNameForBuiltIn(builtInId),
        nullptr, 0, this->topFunc->m_workItem->GetFunctionBody(), 0, nullptr, profileId, builtInId);
#endif

    // From now on we are committed to inlining.
    *pIsInlined = true;

    // Save off the call target operand (function object) so we can extend its lifetime as needed, even if
    // the call instruction gets transformed to CallIFixed.
    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();

    // We are committed to inlining, optimize the call instruction for fixed fields now and don't attempt it later.
    bool safeThis = false;
    if (TryOptimizeCallInstrWithFixedMethod(callInstr, funcInfo, false /*isPolymorphic*/, true /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/, safeThis /*unused here*/))
    {
        Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
        Assert(callInstr->GetFixedFunction()->GetFunctionInfo() == funcInfo);
    }
    else
    {
        // FunctionObject check for built-ins
        IR::BailOutInstr * bailOutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNotBuiltIn, IR::BailOutOnInlineFunction, callInstr, callInstr->m_func);
        InsertFunctionObjectCheck(callInstr, callInstr, bailOutInstr, funcInfo);
    }

    // To push function object for cases when we have to make calls to helper method to assist in inlining
    if(inlineCallOpCode == Js::OpCode::CallDirect)
    {
        IR::Instr* argoutInstr;
        StackSym *dstSym = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(1));
        argoutInstr = IR::Instr::New(Js::OpCode::ArgOut_A_InlineSpecialized, IR::SymOpnd::New(dstSym, 0, TyMachPtr, callInstr->m_func), callInstr->UnlinkSrc1(), callInstr->UnlinkSrc2(), callInstr->m_func);
        argoutInstr->SetByteCodeOffset(callInstr);
        callInstr->GetInsertBeforeByteCodeUsesInstr()->InsertBefore(argoutInstr);

        Js::BuiltinFunction builtInId = Js::JavascriptLibrary::GetBuiltInForFuncInfo(funcInfo, callInstr->m_func->GetScriptContext());


        callInstr->m_opcode = inlineCallOpCode;
        SetupInlineInstrForCallDirect(builtInId, callInstr, argoutInstr);

        // Generate ByteCodeArgOutCaptures and move the ArgOut_A/ArgOut_A_Inline close to the call instruction
        callInstr->MoveArgs(/*generateByteCodeCapture*/ true);

        WrapArgsOutWithCoerse(builtInId, callInstr);

        inlineBuiltInEndInstr = callInstr;
    }
    else
    {
        inlineBuiltInEndInstr = InsertInlineeBuiltInStartEndTags(callInstr, actualCount);

        // InlineArrayPop - TrackCalls Need to be done at InlineArrayPop and not at the InlineBuiltInEnd
        // Hence we use a new opcode, to detect that it is a InlineArrayPop and we don't track the call during End of inlineBuiltInCall sequence
        if(inlineCallOpCode == Js::OpCode::InlineArrayPop)
        {
            inlineBuiltInEndInstr->m_opcode = Js::OpCode::InlineNonTrackingBuiltInEnd;
        }
    }

    // Insert a byteCodeUsesInstr to make sure the function object's lifetime is extended beyond the last bailout point
    // at which we may need to call the inlinee again in the interpreter.
    IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(callInstr, originalCallTargetStackSym->m_id);
    callInstr->InsertBefore(useCallTargetInstr);

    if(Js::JavascriptLibrary::IsTypeSpecRequired(builtInFlags)
// SIMD_JS
        || IsSimd128Opcode(inlineCallOpCode)
//
        )
    {
        // Emit byteCodeUses for function object
        IR::Instr * inlineBuilitInStartInstr = inlineBuiltInEndInstr;
        while(inlineBuilitInStartInstr->m_opcode != Js::OpCode::InlineBuiltInStart)
        {
            inlineBuilitInStartInstr = inlineBuilitInStartInstr->m_prev;
        }

        IR::Opnd * tmpDst = nullptr;
        IR::Opnd * callInstrDst = callInstr->GetDst();

        if(callInstrDst && inlineCallOpCode != Js::OpCode::InlineArrayPop)
        {
            StackSym * tmpSym = StackSym::New(callInstr->GetDst()->GetType(), callInstr->m_func);
            tmpDst = IR::RegOpnd::New(tmpSym, tmpSym->GetType(), callInstr->m_func);

            callInstrDst = callInstr->UnlinkDst();
            callInstr->SetDst(tmpDst);
        }
        else
        {
            AssertMsg(inlineCallOpCode == Js::OpCode::InlineArrayPush || inlineCallOpCode == Js::OpCode::InlineArrayPop || Js::IsSimd128Opcode(inlineCallOpCode),
                "Currently Dst can be null only for InlineArrayPush/InlineArrayPop");
        }

        // Insert a byteCodeUsesInstr to make sure the function object's lifetime is extended beyond the last bailout point
        // at which we may need to call the inlinee again in the interpreter.
        IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(callInstr->GetPrevRealInstrOrLabel(), originalCallTargetStackSym->m_id);

        if(inlineCallOpCode == Js::OpCode::InlineArrayPop)
        {
           callInstr->InsertBefore(useCallTargetInstr);
        }
        else
        {
            inlineBuiltInEndInstr->InsertBefore(useCallTargetInstr);
        }

        if(tmpDst)
        {
            IR::Instr * ldInstr = IR::Instr::New(Js::OpCode::Ld_A, callInstrDst, tmpDst, callInstr->m_func);
            inlineBuiltInEndInstr->InsertBefore(ldInstr);
        }

        // Set srcs of the callInstr, and process ArgOuts.
        callInstr->UnlinkSrc1();
        callInstr->UnlinkSrc2();
        callInstr->m_opcode = inlineCallOpCode;

        int argIndex = inlineCallArgCount;    // We'll use it to fill call instr srcs from upper to lower.


        IR::ByteCodeUsesInstr * byteCodeUsesInstr = IR::ByteCodeUsesInstr::New(callInstr->m_func);
        byteCodeUsesInstr->SetByteCodeOffset(callInstr);
        byteCodeUsesInstr->byteCodeUpwardExposedUsed = JitAnew(callInstr->m_func->m_alloc, BVSparse<JitArenaAllocator>, callInstr->m_func->m_alloc);
        IR::Instr *argInsertInstr = inlineBuilitInStartInstr;

// SIMD_JS
        IR::Instr *eaInsertInstr = callInstr;
        IR::Opnd *eaLinkOpnd = nullptr;
        ThreadContext::SimdFuncSignature simdFuncSignature;
        if (IsSimd128Opcode(callInstr->m_opcode))
        {
            callInstr->m_func->GetScriptContext()->GetThreadContext()->GetSimdFuncSignatureFromOpcode(callInstr->m_opcode, simdFuncSignature);
            Assert(simdFuncSignature.valid);
        }
//
        inlineBuiltInEndInstr->IterateArgInstrs([&](IR::Instr* argInstr) {
            StackSym *linkSym = linkOpnd->GetStackSym();
            linkSym->m_isInlinedArgSlot = true;
            linkSym->m_allocated = true;

            // We are going to replace the use on the call (below), insert byte code use if necessary
            if (OpCodeAttr::BailOutRec(inlineCallOpCode) || Js::IsSimd128Opcode(inlineCallOpCode))
            {
                StackSym * sym = argInstr->GetSrc1()->GetStackSym();
                if (!sym->m_isSingleDef || !sym->m_instrDef->GetSrc1() || !sym->m_instrDef->GetSrc1()->IsConstOpnd())
                {
                    if (!sym->IsFromByteCodeConstantTable())
                    {
                        byteCodeUsesInstr->byteCodeUpwardExposedUsed->Set(sym->m_id);
                    }
                }
            }

            // Convert the arg out to built in arg out, and get the src of the arg out
            IR::Opnd * argOpnd = ConvertToInlineBuiltInArgOut(argInstr);

            // SIMD_JS
            if (inlineCallArgCount > 2 && argIndex != 0 /* don't include 'this' */)
            {
                Assert(IsSimd128Opcode(callInstr->m_opcode));
                // Insert ExtendedArgs

                IR::Instr *eaInstr;

                // inliner sets the dst type of the ExtendedArg to the expected arg type for the operation. The globOpt uses this info to know the type-spec target for each ExtendedArg.
                eaInstr = IR::Instr::New(Js::OpCode::ExtendArg_A, callInstr->m_func);
                eaInstr->SetByteCodeOffset(callInstr);
                if (argIndex == inlineCallArgCount)
                {
                    // fix callInstr
                    eaLinkOpnd = IR::RegOpnd::New(TyVar, callInstr->m_func);
                    eaLinkOpnd->GetStackSym()->m_isInlinedArgSlot = true;
                    eaLinkOpnd->GetStackSym()->m_allocated = true;

                    Assert(callInstr->GetSrc1() == nullptr && callInstr->GetSrc2() == nullptr);
                    callInstr->SetSrc1(eaLinkOpnd);
                }
                Assert(eaLinkOpnd);
                eaInstr->SetDst(eaLinkOpnd);
                eaInstr->SetSrc1(argInstr->GetSrc1());

                // insert link opnd, except for first ExtendedArg
                if (argIndex > 1)
                {
                    eaInstr->SetSrc2(IR::RegOpnd::New(TyVar, callInstr->m_func));
                    eaLinkOpnd = eaInstr->GetSrc2();
                    eaLinkOpnd->GetStackSym()->m_isInlinedArgSlot = true;
                    eaLinkOpnd->GetStackSym()->m_allocated = true;
                }

                eaInstr->GetDst()->SetValueType(simdFuncSignature.args[argIndex - 1]);

                eaInsertInstr->InsertBefore(eaInstr);
                eaInsertInstr = eaInstr;
            }
            else
            {
                // Use parameter to the inline call to tempDst.
                if (argIndex == 2)
                {
                    callInstr->SetSrc2(argOpnd);
                    // Prevent inserting ByteCodeUses instr during globopt, as we already track the src in ArgOut.
                    callInstr->GetSrc2()->SetIsJITOptimizedReg(true);
                }
                else if (argIndex == 1)
                {
                    callInstr->SetSrc1(argOpnd);
                    // Prevent inserting ByteCodeUses instr during globopt, as we already track the src in ArgOut.
                    callInstr->GetSrc1()->SetIsJITOptimizedReg(true);
                }
            }


            argIndex--;

            linkOpnd = argInstr->GetSrc2();

            // Move the arguments next to the call.
            argInstr->Move(argInsertInstr);
            argInsertInstr = argInstr;
            return false;
        });
        if(inlineCallOpCode == Js::OpCode::InlineMathImul || inlineCallOpCode == Js::OpCode::InlineMathClz32)
        {
            // Convert:
            //     s1 = InlineMathImul s2, s3
            // Into:
            //     s4 = Or_A s2, 0
            //     s5 = Or_A s3, 0
            //     s1 = InlineMathImul s4, s5

            Func *const func = callInstr->m_func;
            IR::AddrOpnd *const zeroOpnd = IR::AddrOpnd::NewFromNumber(0, func, true);

            IR::RegOpnd *const s4 = IR::RegOpnd::New(TyVar, func);
            s4->SetIsJITOptimizedReg(true);
            IR::Instr *orInstr = IR::Instr::New(Js::OpCode::Or_A, s4, callInstr->UnlinkSrc1(), zeroOpnd, func);
            orInstr->SetByteCodeOffset(callInstr);
            callInstr->InsertBefore(orInstr);
            callInstr->SetSrc1(s4);
            if (inlineCallOpCode == Js::OpCode::InlineMathImul)
            {
                if (callInstr->GetSrc2()->IsEqual(callInstr->GetSrc1()))
                {
                    callInstr->ReplaceSrc2(s4);
                }
                else
                {
                    IR::RegOpnd *const s5 = IR::RegOpnd::New(TyVar, func);
                    s5->SetIsJITOptimizedReg(true);
                    orInstr = IR::Instr::New(Js::OpCode::Or_A, s5, callInstr->UnlinkSrc2(), zeroOpnd, func);
                    orInstr->SetByteCodeOffset(callInstr);
                    callInstr->InsertBefore(orInstr);
                    callInstr->SetSrc2(s5);
                }
            }
        }

        if(OpCodeAttr::BailOutRec(inlineCallOpCode))
        {
            inlineBuiltInEndInstr->InsertBefore(byteCodeUsesInstr);
        }

        Assert(linkOpnd->AsRegOpnd()->m_sym->GetInstrDef()->m_opcode == Js::OpCode::StartCall);
        Assert(linkOpnd->AsRegOpnd()->m_sym->GetInstrDef()->GetArgOutCount(/*getInterpreterArgOutCount*/ false) == actualCount);

        // Mark the StartCall's dst as an inlined arg slot as well so we know this is an inlined start call
        // and not adjust the stack height on x86
        linkOpnd->AsRegOpnd()->m_sym->m_isInlinedArgSlot = true;

        if(OpCodeAttr::BailOutRec(inlineCallOpCode))
        {
            callInstr = callInstr->ConvertToBailOutInstr(callInstr, IR::BailOutOnFloor);
        }
    }
    return inlineBuiltInEndInstr->m_next;
}

IR::Instr* Inline::InsertInlineeBuiltInStartEndTags(IR::Instr* callInstr, uint actualCount, IR::Instr** builtinStartInstr)
{
    IR::Instr* inlineBuiltInStartInstr = IR::Instr::New(Js::OpCode::InlineBuiltInStart, callInstr->m_func);
    inlineBuiltInStartInstr->SetSrc1(callInstr->GetSrc1());
    inlineBuiltInStartInstr->SetSrc2(callInstr->GetSrc2());
    inlineBuiltInStartInstr->SetByteCodeOffset(callInstr);
    callInstr->InsertBefore(inlineBuiltInStartInstr);
    if (builtinStartInstr)
    {
        *builtinStartInstr = inlineBuiltInStartInstr;
    }

    IR::Instr* inlineBuiltInEndInstr = IR::Instr::New(Js::OpCode::InlineBuiltInEnd, callInstr->m_func);
    inlineBuiltInEndInstr->SetSrc1(IR::IntConstOpnd::New(actualCount, TyInt32, callInstr->m_func));
    inlineBuiltInEndInstr->SetSrc2(callInstr->GetSrc2());
    inlineBuiltInEndInstr->SetByteCodeOffset(callInstr->GetNextRealInstrOrLabel());
    callInstr->InsertAfter(inlineBuiltInEndInstr);
    return inlineBuiltInEndInstr;
}

IR::Instr* Inline::GetDefInstr(IR::Opnd* linkOpnd)
{
    StackSym *linkSym = linkOpnd->AsSymOpnd()->m_sym->AsStackSym();
    Assert(linkSym->m_isSingleDef);
    Assert(linkSym->IsArgSlotSym());

    return linkSym->m_instrDef;
}

IR::Instr* Inline::InlineApply(IR::Instr *callInstr, Js::FunctionInfo *funcInfo, const Js::FunctionCodeGenJitTimeData* inlinerData, const StackSym *symCallerThis, bool* pIsInlined, uint callSiteId, uint recursiveInlineDepth)
{
    // We may still decide not to inline.
    *pIsInlined = false;

    Js::BuiltinFunction builtInId = Js::JavascriptLibrary::GetBuiltInForFuncInfo(funcInfo, callInstr->m_func->GetScriptContext());
    const Js::FunctionCodeGenJitTimeData * inlineeData = nullptr;

    IR::SymOpnd* linkOpnd = callInstr->GetSrc2()->AsSymOpnd();
    StackSym *arrayArgsym = linkOpnd->AsSymOpnd()->m_sym->AsStackSym();
    Assert(arrayArgsym->m_isSingleDef);
    Assert(arrayArgsym->IsArgSlotSym());

    IR::Instr* arrayArgInstr = arrayArgsym->m_instrDef;
    IR::Opnd *arrayArgOpnd = arrayArgInstr->GetSrc1();

    // if isArrayOpndArgumentsObject == false, the array opnd can still be the arguments object; we just can't say that for sure
    bool isArrayOpndArgumentsObject = arrayArgOpnd->IsArgumentsObject();

    IR::Instr * returnInstr = nullptr;
    if (!PHASE_OFF(Js::InlineApplyTargetPhase, this->topFunc))
    {
        if (isArrayOpndArgumentsObject && InlineApplyTarget(callInstr, inlinerData, &inlineeData, funcInfo, symCallerThis, &returnInstr, recursiveInlineDepth))
        {
            *pIsInlined = true;
            Assert(returnInstr);
            return returnInstr;
        }
    }

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    InliningDecider::TraceInlining(inlinerData->GetFunctionBody(), Js::JavascriptLibrary::GetNameForBuiltIn(builtInId),
        nullptr, 0, this->topFunc->m_workItem->GetFunctionBody(), 0, nullptr, callSiteId, builtInId);
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    if (!isArrayOpndArgumentsObject)
    {
        if (inlineeData && inlineeData->GetFunctionBody() == nullptr)
        {
            *pIsInlined = true;
            Assert((inlineeData->GetFunctionInfo()->GetAttributes() & Js::FunctionInfo::Attributes::BuiltInInlinableAsLdFldInlinee) != 0);
            return InlineApplyWithArray(callInstr, funcInfo, Js::JavascriptLibrary::GetBuiltInForFuncInfo(inlineeData->GetFunctionInfo(), callInstr->m_func->GetScriptContext()));
        }
        else
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Supporting inlining func.apply(this, array) or func.apply(this, arguments) with formals in the parent function only when func is a built-in inlineable as apply target \tCaller: %s (%s)\n",
                inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer));
            return callInstr;
        }
    }

    *pIsInlined = true;
    return InlineApplyWithArgumentsObject(callInstr, arrayArgInstr, funcInfo);
}

IR::Instr * Inline::InlineApplyWithArgumentsObject(IR::Instr * callInstr, IR::Instr * argsObjectArgInstr, Js::FunctionInfo * funcInfo)
{
    IR::Instr* ldHeapArguments = argsObjectArgInstr->GetSrc1()->GetStackSym()->GetInstrDef();
    IR::RegOpnd* argumentsObj = IR::RegOpnd::New(TyVar, callInstr->m_func);
    IR::Instr *assignInstr = IR::Instr::New(Js::OpCode::LdArgumentsFromStack, argumentsObj, ldHeapArguments->GetDst(), callInstr->m_func);
    assignInstr->SetByteCodeOffset(argsObjectArgInstr);
    argsObjectArgInstr->InsertBefore(assignInstr);
    argsObjectArgInstr->ReplaceSrc1(ldHeapArguments->GetDst());

    IR::Opnd * linkOpnd = callInstr->GetSrc2()->AsSymOpnd();
    IR::Instr * explicitThisArgOut = nullptr;
    IR::Instr * implicitThisArgOut = nullptr;
    callInstr->IterateArgInstrs([&](IR::Instr* argInstr) {
        explicitThisArgOut = implicitThisArgOut;
        implicitThisArgOut = argInstr;

        linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_isInlinedArgSlot = true;
        linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_allocated = true;
        ConvertToInlineBuiltInArgOut(argInstr);

        linkOpnd = argInstr->GetSrc2();
        return false;
    });

    //      BailOnNotEqual  s4.var                  ---------------New additional BAILOUT if not stack args or actuals exceed 16 at runtime.
    //      Bailout: #004e (BailOutOnInlineFunction)
    //      linkOpnd      Argout_FromStackArgs s4.var
    //      linkOpnd1     ArgOut_A_Dynamic  s3.var, linkOpnd
    //                    CallI_Dynamic     s6.var,  linkOpnd1

    IR::Instr* bailOutOnNotStackArgs;
    IR::Instr* bailOutOnNotStackArgsInsertionPoint = callInstr;

    // Save off the call target operand (function object) so we can extend its lifetime as needed, even if
    // the call instruction gets transformed to CallIFixed.
    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();

    // If we optimized the call instruction for a fixed function we will have bailed out earlier if the function
    // wasn't what we expected or was not a function at all.  However, we must still check and bail out on heap arguments.
    bool safeThis = false;
    if (TryOptimizeCallInstrWithFixedMethod(callInstr, funcInfo, false /*isPolymorphic*/, true /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/, safeThis /*unused here*/))
    {
        Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
        bailOutOnNotStackArgs = IR::BailOutInstr::New(Js::OpCode::BailOnNotStackArgs, IR::BailOutOnInlineFunction, callInstr, callInstr->m_func);
    }
    else
    {
        IR::Instr *primaryBailoutInstr = PrepareInsertionPoint(callInstr, funcInfo, callInstr);
        bailOutOnNotStackArgs = IR::BailOutInstr::New(Js::OpCode::BailOnNotStackArgs, IR::BailOutOnInlineFunction, primaryBailoutInstr->GetBailOutInfo(), callInstr->m_func);
        bailOutOnNotStackArgsInsertionPoint = primaryBailoutInstr;
    }

    bailOutOnNotStackArgs->SetSrc1(argumentsObj);
    bailOutOnNotStackArgs->SetSrc2(IR::AddrOpnd::NewNull(callInstr->m_func));
    bailOutOnNotStackArgsInsertionPoint->InsertBefore(bailOutOnNotStackArgs);

    // If we optimized the call instruction for a fixed function, we must extend the function object's lifetime until after
    // the bailout on non-stack arguments.
    if (callInstr->m_opcode == Js::OpCode::CallIFixed)
    {
        IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(callInstr, originalCallTargetStackSym->m_id);
        callInstr->InsertBefore(useCallTargetInstr);
    }

    // Optimize .init.apply(this, arguments);
    IR::Instr* builtInStartInstr;
    InsertInlineeBuiltInStartEndTags(callInstr, 3, &builtInStartInstr); //3 args (implicit this + explicit this + arguments = 3)

    // Move argouts close to call
    IR::Instr* argInsertInstr = builtInStartInstr;
    builtInStartInstr->IterateArgInstrs([&](IR::Instr* argInstr) {
        argInstr->Move(argInsertInstr);
        argInsertInstr = argInstr;
        return false;
    });

    IR::Instr *startCall = IR::Instr::New(Js::OpCode::StartCall, callInstr->m_func);
    startCall->SetDst(IR::RegOpnd::New(TyVar, callInstr->m_func));
    startCall->SetSrc1(IR::IntConstOpnd::New(2, TyInt32, callInstr->m_func)); //2 args (this pointer & ArgOut_A_From_StackArgs for this direct call to init

    callInstr->InsertBefore(startCall);

    StackSym *symDst = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(2));
    IR::SymOpnd* linkOpnd1 = IR::SymOpnd::New(symDst, 0, TyMachPtr, callInstr->m_func);

    symDst = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(1));
    IR::Opnd *linkOpnd2 = IR::SymOpnd::New(symDst, 0, TyMachPtr, callInstr->m_func);

    // This keeps the stack args alive for bailout to recover
    IR::Instr* argout = IR::Instr::New(Js::OpCode::ArgOut_A_FromStackArgs, linkOpnd1, ldHeapArguments->GetDst(), startCall->GetDst(), callInstr->m_func);
    callInstr->InsertBefore(argout);

    callInstr->ReplaceSrc1(implicitThisArgOut->GetSrc1());
    callInstr->ReplaceSrc2(linkOpnd2);
    callInstr->m_opcode = Js::OpCode::CallIDynamic;

    argout = IR::Instr::New(Js::OpCode::ArgOut_A_Dynamic, linkOpnd2, explicitThisArgOut->GetSrc1(), linkOpnd1, callInstr->m_func); // push explicit this as this pointer
    callInstr->InsertBefore(argout);
    return callInstr;
}

IR::Instr * Inline::InlineApplyWithArray(IR::Instr * callInstr, Js::FunctionInfo * funcInfo, Js::BuiltinFunction builtInId)
{
    IR::Opnd * linkOpnd = callInstr->GetSrc2()->AsSymOpnd();
    IR::Instr * argInsertInstr = callInstr;
    IR::Instr * arrayArgOut = nullptr;
    IR::Instr * explicitThisArgOut = nullptr;
    IR::Instr * implicitThisArgOut = nullptr;
    callInstr->IterateArgInstrs([&](IR::Instr* argInstr) {
        arrayArgOut = explicitThisArgOut;
        explicitThisArgOut = implicitThisArgOut;
        implicitThisArgOut = argInstr;

        linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_isInlinedArgSlot = true;
        linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_allocated = true;
        ConvertToInlineBuiltInArgOut(argInstr);

        argInstr->Move(argInsertInstr);
        argInsertInstr = argInstr;

        linkOpnd = argInstr->GetSrc2();
        return false;
    });

    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();

    // If we optimized the call instruction for a fixed function we will have bailed out earlier if the function
    // wasn't what we expected or was not a function at all.  However, we must still check and bail out on heap arguments.
    bool safeThis = false;
    if (TryOptimizeCallInstrWithFixedMethod(callInstr, funcInfo, false /*isPolymorphic*/, true /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/, safeThis /*unused here*/))
    {
        Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
    }
    else
    {
        PrepareInsertionPoint(callInstr, funcInfo, callInstr);
    }

    // If we optimized the call instruction for a fixed function, we must extend the function object's lifetime until after the last bailout before the call.
    if (callInstr->m_opcode == Js::OpCode::CallIFixed)
    {
        IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(callInstr, originalCallTargetStackSym->m_id);
        callInstr->InsertBefore(useCallTargetInstr);
    }

    IR::Instr* builtInEndInstr = InsertInlineeBuiltInStartEndTags(callInstr, 3); // 3 args (implicit this + explicit this + array = 3)
    builtInEndInstr->m_opcode = Js::OpCode::InlineNonTrackingBuiltInEnd; // We will call EndTrackCall when we see CallDirect for reasons explained in GlobOpt::TrackCalls

    IR::Instr * startCall = IR::Instr::New(Js::OpCode::StartCall,
                                           IR::RegOpnd::New(TyVar, callInstr->m_func),
                                           IR::IntConstOpnd::New(2, TyInt32, callInstr->m_func),
                                           callInstr->m_func);
    callInstr->InsertBefore(startCall);

    StackSym * sym = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(1));
    linkOpnd = IR::SymOpnd::New(sym, 0, TyMachPtr, callInstr->m_func);
    IR::Instr * argOut = IR::Instr::New(Js::OpCode::ArgOut_A, linkOpnd, explicitThisArgOut->GetSrc1(), startCall->GetDst(), callInstr->m_func);
    callInstr->InsertBefore(argOut);

    sym = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(2));
    linkOpnd = IR::SymOpnd::New(sym, 0, TyMachPtr, callInstr->m_func);
    argOut = IR::Instr::New(Js::OpCode::ArgOut_A, linkOpnd, arrayArgOut->GetSrc1(), argOut->GetDst(), callInstr->m_func);
    callInstr->InsertBefore(argOut);

    linkOpnd = IR::SymOpnd::New(callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(1)), 0, TyMachPtr, callInstr->m_func);
    argOut = IR::Instr::New(Js::OpCode::ArgOut_A_InlineSpecialized, linkOpnd, implicitThisArgOut->GetSrc1(), argOut->GetDst(), callInstr->m_func);
    callInstr->InsertBefore(argOut);

    IR::HelperCallOpnd * helperCallOpnd = nullptr;
    switch (builtInId)
    {
    case Js::BuiltinFunction::Math_Max:
        helperCallOpnd = IR::HelperCallOpnd::New(IR::HelperOp_MaxInAnArray, callInstr->m_func);
        break;

    case Js::BuiltinFunction::Math_Min:
        helperCallOpnd = IR::HelperCallOpnd::New(IR::HelperOp_MinInAnArray, callInstr->m_func);
        break;

    default:
        Assert(false);
        __assume(UNREACHED);
    }
    callInstr->m_opcode = Js::OpCode::CallDirect;
    callInstr->ReplaceSrc1(helperCallOpnd);
    callInstr->ReplaceSrc2(argOut->GetDst());

    return callInstr;
}

bool Inline::InlineApplyTarget(IR::Instr *callInstr, const Js::FunctionCodeGenJitTimeData* inlinerData, const Js::FunctionCodeGenJitTimeData** pInlineeData, Js::FunctionInfo *applyFuncInfo,
                            const StackSym *symCallerThis, IR::Instr ** returnInstr, uint recursiveInlineDepth)
{
#if ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    if (this->isApplyTargetInliningInProgress)
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: Skipping apply target inlining, Recursive apply inlining is not supported \tCaller: %s\t(%s) \tTop Func:%s\t(%s)\n", inlinerData->GetFunctionBody()->GetDisplayName(),
                                inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer), this->topFunc->GetJnFunction()->GetDisplayName(), this->topFunc->GetJnFunction()->GetDebugNumberSet(debugStringBuffer2));
        return false;
    }

    // Begin inlining apply target

    IR::Opnd* applyOpnd = callInstr->GetSrc1();
    Assert(applyOpnd->IsRegOpnd());
    StackSym* applySym = applyOpnd->AsRegOpnd()->m_sym->AsStackSym();
    if (!applySym->IsSingleDef())
    {
        return false;
    }
    IR::Instr* applyLdInstr = applySym->GetInstrDef();
    IR::Instr* applyTargetLdInstr = applyLdInstr->m_prev;

    if(applyTargetLdInstr->m_opcode != Js::OpCode::LdFldForCallApplyTarget)
    {
        return false;
    }

    IR::Opnd *applyTargetLdOpnd = applyTargetLdInstr->GetSrc1();
    if (!applyTargetLdOpnd->IsSymOpnd() || !applyTargetLdOpnd->AsSymOpnd()->IsPropertySymOpnd())
    {
        return false;
    }

    const auto inlineCacheIndex = applyTargetLdOpnd->AsPropertySymOpnd()->m_inlineCacheIndex;
    const auto inlineeData = inlinerData->GetLdFldInlinee(inlineCacheIndex);

    if (SkipCallApplyTargetInlining_Shared(callInstr, inlinerData, inlineeData, /*isApplyTarget*/ true, /*isCallTarget*/ false))
    {
        *pInlineeData = inlineeData;
        return false;
    }

    if (callInstr->m_func->IsTopFunc())
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: Skipping apply target inlining in top func\tCaller: %s\t(%s) \tTop Func:%s\t(%s)\n", inlinerData->GetFunctionBody()->GetDisplayName(),
            inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer), this->topFunc->GetJnFunction()->GetDisplayName(), this->topFunc->GetJnFunction()->GetDebugNumberSet(debugStringBuffer2));
        return false;
    }

    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    bool safeThis = false;
    if (!TryGetFixedMethodsForBuiltInAndTarget(callInstr, inlinerData, inlineeData, applyFuncInfo, applyLdInstr, applyTargetLdInstr, safeThis, /*isApplyTarget*/ true))
    {
        return false;
    }

    // o.foo.apply(obj, arguments)
    //
    // StartCall
    // ArgOut_A         <-- implicit "this" (foo) argout
    // ArgOut_A         <-- explicit "this" (obj) argout
    // ArgOut_A         <-- arguments object argout
    // CallIFixed

    IR::Instr* implicitThisArgOut = nullptr;
    IR::Instr* explicitThisArgOut = nullptr;
    IR::Instr* argumentsObjArgOut = nullptr;
    callInstr->IterateArgInstrs([&](IR::Instr* argInstr)
    {
        argumentsObjArgOut = explicitThisArgOut;
        explicitThisArgOut = implicitThisArgOut;
        implicitThisArgOut = argInstr;

        argInstr->GenerateBytecodeArgOutCapture(); // Generate BytecodeArgOutCapture here to capture the implicit "this" (to be removed) and arguments object (to be expanded) argouts,
                                                   // so that any bailout in the call sequence restores the argouts stack as the interpreter would expect it to be.
        argInstr->GetDst()->AsSymOpnd()->GetStackSym()->DecrementArgSlotNum(); // We will be removing implicit "this" argout
        return false;
    });

    if (safeThis)
    {
        IR::Instr * byteCodeArgOutCapture = explicitThisArgOut->GetBytecodeArgOutCapture();
        Assert(byteCodeArgOutCapture->GetSrc1()->IsRegOpnd());

        if (byteCodeArgOutCapture->GetSrc1()->AsRegOpnd()->GetStackSym() != symCallerThis)
        {
            safeThis = false;
        }
    }

    IR::Opnd *src1 = argumentsObjArgOut->GetSrc1();
    IR::Instr* ldHeapArguments = src1->AsRegOpnd()->m_sym->m_instrDef;

    IR::RegOpnd* argumentsObj = IR::RegOpnd::New(TyVar, callInstr->m_func);
    IR::Instr *assignInstr = IR::Instr::New(Js::OpCode::LdArgumentsFromStack, argumentsObj, ldHeapArguments->GetDst(), callInstr->m_func);
    assignInstr->SetByteCodeOffset(argumentsObjArgOut);
    argumentsObjArgOut->InsertBefore(assignInstr);

    IR::Instr* argObjByteCodeArgoutCapture = argumentsObjArgOut->GetBytecodeArgOutCapture();
    argObjByteCodeArgoutCapture->GetDst()->GetStackSym()->m_nonEscapingArgObjAlias = true;

    argumentsObjArgOut->m_opcode = Js::OpCode::ArgOut_A_FromStackArgs;

    Assert(implicitThisArgOut->GetSrc2()->IsRegOpnd());
    IR::Instr * startCall = implicitThisArgOut->GetSrc2()->AsRegOpnd()->m_sym->AsStackSym()->GetInstrDef();
    Assert(startCall->m_opcode == Js::OpCode::StartCall);

    IR::Instr *  bailOutOnNotStackArgs = IR::BailOutInstr::New(Js::OpCode::BailOnNotStackArgs, IR::BailOutOnInlineFunction,
        callInstr, callInstr->m_func);
    bailOutOnNotStackArgs->SetSrc1(argumentsObj);
    bailOutOnNotStackArgs->SetSrc2(IR::AddrOpnd::NewNull(callInstr->m_func));
    argumentsObjArgOut->InsertBefore(bailOutOnNotStackArgs);

    IR::Instr* byteCodeArgOutUse = IR::Instr::New(Js::OpCode::BytecodeArgOutUse, callInstr->m_func);
    byteCodeArgOutUse->SetSrc1(implicitThisArgOut->GetSrc1());
    byteCodeArgOutUse->SetSrc2(argumentsObjArgOut->GetSrc1());
    callInstr->InsertBefore(byteCodeArgOutUse);

    // don't need the implicit "this" anymore
    explicitThisArgOut->ReplaceSrc2(startCall->GetDst());
    implicitThisArgOut->Remove();

    startCall->SetSrc2(IR::IntConstOpnd::New(startCall->GetArgOutCount(/*getInterpreterArgOutCount*/ false), TyUint32, startCall->m_func));
    startCall->GetSrc1()->AsIntConstOpnd()->IncrValue(-1); // update the count of argouts as seen by JIT, in the start call instruction

    *returnInstr = InlineCallApplyTarget_Shared(callInstr, originalCallTargetStackSym, inlineeData->GetFunctionInfo(), inlineeData, inlineCacheIndex,
                                                safeThis, /*isApplyTarget*/ true, /*isCallTarget*/ false, recursiveInlineDepth);
    return true;
}

IR::Instr *
Inline::InlineCallApplyTarget_Shared(IR::Instr *callInstr, StackSym* originalCallTargetStackSym, Js::FunctionInfo *funcInfo, const Js::FunctionCodeGenJitTimeData *const inlineeData,
                                        uint inlineCacheIndex, bool safeThis, bool isApplyTarget, bool isCallTarget, uint recursiveInlineDepth)
{
    Assert(isApplyTarget ^ isCallTarget);

    // function body
    Js::FunctionBody* funcBody = funcInfo->GetFunctionBody();

    // returnValueOpnd
    IR::RegOpnd * returnValueOpnd;
    Js::RegSlot returnRegSlot;
    if (callInstr->GetDst())
    {
        returnValueOpnd = callInstr->UnlinkDst()->AsRegOpnd();
        returnRegSlot = returnValueOpnd->m_sym->GetByteCodeRegSlot();
    }
    else
    {
        returnValueOpnd = nullptr;
        returnRegSlot = Js::Constants::NoRegister;
    }

    Assert(callInstr->IsProfiledInstr());
    Js::ProfileId callSiteId = static_cast<Js::ProfileId>(callInstr->AsProfiledInstr()->u.profileId);
    Assert(callSiteId >= 0);

    // inlinee
    Js::ProxyEntryPointInfo *defaultEntryPointInfo = funcBody->GetDefaultEntryPointInfo();
    Assert(defaultEntryPointInfo->IsFunctionEntryPointInfo());
    Js::FunctionEntryPointInfo *functionEntryPointInfo = static_cast<Js::FunctionEntryPointInfo*>(defaultEntryPointInfo);
    JsFunctionCodeGen *workItem = JitAnew(this->topFunc->m_alloc, JsFunctionCodeGen,
        funcBody->GetScriptContext()->GetNativeCodeGenerator(), funcBody, functionEntryPointInfo, this->topFunc->IsJitInDebugMode());
    workItem->SetRecyclableData(JitAnew(this->topFunc->m_alloc, Js::CodeGenRecyclableData, inlineeData));
    workItem->SetJitMode(this->topFunc->m_workItem->GetJitMode());

    const auto profileInfo =
        JitAnew(
            this->topFunc->m_alloc,
            Js::ReadOnlyDynamicProfileInfo,
            funcBody->HasDynamicProfileInfo() ? funcBody->GetAnyDynamicProfileInfo() : nullptr,
            this->topFunc->IsBackgroundJIT() ? this->topFunc->m_alloc : nullptr);

    Js::EntryPointPolymorphicInlineCacheInfo * entryPointPolymorphicInlineCacheInfo = this->topFunc->m_workItem->GetEntryPoint()->GetPolymorphicInlineCacheInfo();
    Func *inlinee = JitAnew(this->topFunc->m_alloc,
                         Func,
                         this->topFunc->m_alloc,
                         workItem,
                         callInstr->m_func->m_runtimeData ?
                            callInstr->m_func->m_runtimeData->GetLdFldInlinee(inlineCacheIndex) :
                            this->topFunc->GetJnFunction()->GetLdFldInlineeCodeGenRuntimeData(inlineCacheIndex),
                         entryPointPolymorphicInlineCacheInfo ? entryPointPolymorphicInlineCacheInfo->GetInlineeInfo(funcBody) : nullptr,
                         this->topFunc->GetCodeGenAllocators(),
                         this->topFunc->GetNumberAllocator(),
                         profileInfo,
                         this->topFunc->GetCodeGenProfiler(),
                         this->topFunc->IsBackgroundJIT(),
                         callInstr->m_func,
                         callInstr->m_next->GetByteCodeOffset(),
                         returnRegSlot,
                         false,
                         callSiteId,
                         false);

    // instrNext
    IR::Instr* instrNext = callInstr->m_next;

    return InlineFunctionCommon(callInstr, originalCallTargetStackSym, funcBody, inlinee, instrNext, returnValueOpnd, callInstr, nullptr, recursiveInlineDepth, safeThis, isApplyTarget);
}

IR::Opnd *
Inline::ConvertToInlineBuiltInArgOut(IR::Instr * argInstr)
{
    argInstr->m_opcode = Js::OpCode::ArgOut_A_InlineBuiltIn;
    argInstr->GenerateBytecodeArgOutCapture();
    return argInstr->GetSrc1();
}

IR::Instr*
Inline::InlineCall(IR::Instr *callInstr, Js::FunctionInfo *funcInfo, const Js::FunctionCodeGenJitTimeData* inlinerData, const StackSym *symCallerThis, bool* pIsInlined, uint callSiteId, uint recursiveInlineDepth)
{
    Js::BuiltinFunction builtInId = Js::JavascriptLibrary::GetBuiltInForFuncInfo(funcInfo, callInstr->m_func->GetScriptContext());
    Func *func = callInstr->m_func;

    *pIsInlined = false;
    if (PHASE_OFF(Js::InlineCallPhase, this->topFunc) || PHASE_OFF(Js::InlineCallPhase, func->GetJnFunction())
        || !this->topFunc->GetJnFunction()->GetInParamsCount())
    {
        return callInstr;
    }

    // Convert all the current ARG_OUT to  ArgOut_A_InlineBuiltIn
    IR::Opnd *linkOpnd = callInstr->GetSrc2();

    if (!GetDefInstr(linkOpnd)->GetSrc2()->IsSymOpnd())
    {
        // There is no benefit of inlining.call() with no arguments.
        return callInstr;
    }

    *pIsInlined = true;
    const Js::FunctionCodeGenJitTimeData * inlineeData = nullptr;

    IR::Instr * returnInstr = nullptr;
    if (!PHASE_OFF(Js::InlineCallTargetPhase, this->topFunc))
    {
        if (InlineCallTarget(callInstr, inlinerData, &inlineeData, funcInfo, symCallerThis, &returnInstr, recursiveInlineDepth))
        {
            Assert(returnInstr);
            return returnInstr;
        }
    }

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    InliningDecider::TraceInlining(inlinerData->GetFunctionBody(), Js::JavascriptLibrary::GetNameForBuiltIn(builtInId),
        nullptr, 0, this->topFunc->m_workItem->GetFunctionBody(), 0, nullptr, callSiteId, builtInId);
#endif

    uint actualCount = 0;
    Assert(linkOpnd->IsSymOpnd());

    // We are trying to optimize this.superConstructor.call(this, a, b,c);
    // argImplicitInstr represents this.superConstructor which we need to call directly.
    IR::Instr *argImplicitInstr;
    IR::Instr* argInsertInstr = callInstr;
    callInstr->IterateArgInstrs([&](IR::Instr* argInstr) {
        argImplicitInstr = argInstr;
        ++actualCount;
        linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_isInlinedArgSlot = true;
        linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_allocated = true;
        ConvertToInlineBuiltInArgOut(argInstr);
        // Move the arguments next to the call.
        argInstr->Move(argInsertInstr);
        argInsertInstr = argInstr;
        linkOpnd = argInstr->GetSrc2();
        return false;
    });
    linkOpnd->AsRegOpnd()->m_sym->m_isInlinedArgSlot = true;

    IR::SymOpnd* orgLinkOpnd = callInstr->GetSrc2()->AsSymOpnd();

    // Save off the call target operand (function object) so we can extend its lifetime as needed, even if
    // the call instruction gets transformed to CallIFixed.
    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    bool safeThis = false;
    if (!TryOptimizeCallInstrWithFixedMethod(callInstr, funcInfo, false /*isPolymorphic*/, true /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/, safeThis))
    {
        PrepareInsertionPoint(callInstr, funcInfo, callInstr);
    }
    else
    {
        Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
        // If we optimized the call instruction for a fixed function, we must extend the function object's lifetime until after
        // the bailout on non-stack arguments.
        IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(callInstr, originalCallTargetStackSym->m_id);
        callInstr->InsertBefore(useCallTargetInstr);
    }

    InsertInlineeBuiltInStartEndTags(callInstr, actualCount);

    uint actualCountToInlinedCall = actualCount - 1;

    IR::Instr *startCall = IR::Instr::New(Js::OpCode::StartCall, func);
    startCall->SetDst(IR::RegOpnd::New(TyVar, func));
    startCall->SetSrc1(IR::IntConstOpnd::New(actualCountToInlinedCall, TyInt32, func)); // New call will have one less parameter.

    callInstr->InsertBefore(startCall);

    callInstr->ReplaceSrc1(argImplicitInstr->GetSrc1());
    callInstr->UnlinkSrc2();
    callInstr->m_opcode = Js::OpCode::CallI;

    IR::Instr* insertBeforeInstr = callInstr;
    IR::Instr* clonedArgout = nullptr;
    IR::Instr* orgArgout = nullptr;

    for (uint i = actualCountToInlinedCall ; i > 0; i--)
    {
        orgArgout = GetDefInstr(orgLinkOpnd);
        orgLinkOpnd = orgArgout->GetSrc2()->AsSymOpnd();
        IR::Opnd *orgSrc1 = orgArgout->GetSrc1();

        // Change ArgOut to use temp as src1.
        StackSym * stackSym = StackSym::New(orgSrc1->GetStackSym()->GetType(), argImplicitInstr->m_func);
        IR::Opnd* tempDst = IR::RegOpnd::New(stackSym, orgSrc1->GetType(), argImplicitInstr->m_func);
        IR::Instr *assignInstr = IR::Instr::New(Js::OpCode::Ld_A, tempDst, orgSrc1, argImplicitInstr->m_func);
        assignInstr->SetByteCodeOffset(orgArgout);
        tempDst->SetIsJITOptimizedReg(true);
        orgArgout->InsertBefore(assignInstr);

        StackSym *symDst = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(i));
        IR::SymOpnd* newLinkOpnd = IR::SymOpnd::New(symDst, 0, TyMachPtr, func);

        clonedArgout = IR::Instr::New(Js::OpCode::ArgOut_A, newLinkOpnd, tempDst, func);
        insertBeforeInstr->SetSrc2(newLinkOpnd);

        insertBeforeInstr->InsertBefore(clonedArgout);
        insertBeforeInstr = clonedArgout;
    }
    clonedArgout->SetSrc2(startCall->GetDst());
    Assert(GetDefInstr(orgLinkOpnd) == argImplicitInstr);
    return callInstr;
}

bool
Inline::InlineCallTarget(IR::Instr *callInstr, const Js::FunctionCodeGenJitTimeData* inlinerData, const Js::FunctionCodeGenJitTimeData** pInlineeData, Js::FunctionInfo *callFuncInfo,
                            const StackSym *symCallerThis, IR::Instr ** returnInstr, uint recursiveInlineDepth)
{
    IR::Opnd* src1 = callInstr->GetSrc1();
    Assert(src1->IsRegOpnd());
    StackSym* sym = src1->AsRegOpnd()->GetStackSym();
    if (!sym->IsSingleDef())
    {
        return false;
    }
    IR::Instr* callLdInstr = sym->GetInstrDef();
    Assert(callLdInstr);

    IR::Instr* callTargetLdInstr = callLdInstr->m_prev;
    if (callTargetLdInstr->m_opcode != Js::OpCode::LdFldForCallApplyTarget)
    {
        return false;
    }

    IR::Opnd* callTargetLdOpnd = callTargetLdInstr->GetSrc1();
    if (!callTargetLdOpnd->IsSymOpnd() || !callTargetLdOpnd->AsSymOpnd()->IsPropertySymOpnd())
    {
        return false;
    }

    const auto inlineCacheIndex = callTargetLdOpnd->AsPropertySymOpnd()->m_inlineCacheIndex;
    const auto inlineeData = inlinerData->GetLdFldInlinee(inlineCacheIndex);

    if (SkipCallApplyTargetInlining_Shared(callInstr, inlinerData, inlineeData, /*isApplyTarget*/ false, /*isCallTarget*/ true))
    {
        *pInlineeData = inlineeData;
        return false;
    }

    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    bool safeThis = false;
    if (!TryGetFixedMethodsForBuiltInAndTarget(callInstr, inlinerData, inlineeData, callFuncInfo, callLdInstr, callTargetLdInstr, safeThis, /*isApplyTarget*/ false))
    {
        return false;
    }

    IR::Instr* implicitThisArgOut = nullptr;
    IR::Instr* explicitThisArgOut = nullptr;

    callInstr->IterateArgInstrs([&] (IR::Instr* argInstr)
    {
        explicitThisArgOut = implicitThisArgOut;
        implicitThisArgOut = argInstr;

        argInstr->GenerateBytecodeArgOutCapture(); // Generate BytecodeArgOutCapture here to capture the implicit "this" argout (which will be removed) as well,
                                                   // so that any bailout in the call sequence restores the argouts stack as the interpreter would expect it to be.
        argInstr->GetDst()->AsSymOpnd()->GetStackSym()->DecrementArgSlotNum(); // We will be removing implicit "this" argout
        return false;
    });

    Assert(explicitThisArgOut);
    Assert(explicitThisArgOut->HasByteCodeArgOutCapture());
    if (safeThis)
    {
        IR::Instr * byteCodeArgOutCapture = explicitThisArgOut->GetBytecodeArgOutCapture();
        Assert(byteCodeArgOutCapture->GetSrc1()->IsRegOpnd());

        if (byteCodeArgOutCapture->GetSrc1()->AsRegOpnd()->GetStackSym() != symCallerThis)
        {
            safeThis = false;
        }
    }

    IR::Opnd* linkOpnd = implicitThisArgOut->GetSrc2();
    Assert(linkOpnd->IsRegOpnd() && linkOpnd->AsRegOpnd()->GetStackSym()->IsSingleDef());
    Assert(linkOpnd->AsRegOpnd()->GetStackSym()->GetInstrDef()->m_opcode == Js::OpCode::StartCall);

    IR::Instr* startCall = linkOpnd->AsRegOpnd()->GetStackSym()->GetInstrDef();

    explicitThisArgOut->ReplaceSrc2(startCall->GetDst());

    IR::Instr * bytecodeArgOutUse = IR::Instr::New(Js::OpCode::BytecodeArgOutUse, callInstr->m_func);
    bytecodeArgOutUse->SetSrc1(implicitThisArgOut->GetSrc1());
    callInstr->InsertBefore(bytecodeArgOutUse); // Need to keep the implicit "this" argout live till the call instruction for it to be captured by any bailout in the call sequence.
    implicitThisArgOut->Remove();

    startCall->SetSrc2(IR::IntConstOpnd::New(startCall->GetArgOutCount(/*getInterpreterArgOutCount*/ false), TyUint32, startCall->m_func));
    startCall->GetSrc1()->AsIntConstOpnd()->SetValue(startCall->GetSrc1()->AsIntConstOpnd()->GetValue() - 1);

    *returnInstr = InlineCallApplyTarget_Shared(callInstr, originalCallTargetStackSym, inlineeData->GetFunctionInfo(), inlineeData, inlineCacheIndex,
                                                safeThis, /*isApplyTarget*/ false, /*isCallTarget*/ true, recursiveInlineDepth);

    return true;
}

bool
Inline::SkipCallApplyTargetInlining_Shared(IR::Instr *callInstr, const Js::FunctionCodeGenJitTimeData* inlinerData, const Js::FunctionCodeGenJitTimeData* inlineeData, bool isApplyTarget, bool isCallTarget)
{
#if ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer3[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    Assert(isApplyTarget ^ isCallTarget);

    if (PHASE_OFF(Js::FixedMethodsPhase, callInstr->m_func->GetJnFunction()))
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: Skipping %s target inlining, Fixed Methods turned off\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n", isApplyTarget ? L"apply" : L"call" ,
            inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
            this->topFunc->GetJnFunction()->GetDisplayName(), this->topFunc->GetJnFunction()->GetDebugNumberSet(debugStringBuffer2));
        return true;
    }

    if (!inlineeData)
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: Skipping %s target inlining, inlineeData not present\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n", isApplyTarget ? L"apply" : L"call",
            inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
            this->topFunc->GetJnFunction()->GetDisplayName(), this->topFunc->GetJnFunction()->GetDebugNumberSet(debugStringBuffer2));
        return true;
    }

    if (!inlineeData->GetFunctionBody())
    {
        if (isCallTarget)
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Skipping .call inlining, target is a built-in\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n",
                inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                this->topFunc->GetJnFunction()->GetDisplayName(), this->topFunc->GetJnFunction()->GetDebugNumberSet(debugStringBuffer2));
        }
        return true;
    }

    if (!inlinerData->IsLdFldInlineePresent())
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: Skipping %s target inlining, not registered as a LdFld inlinee \tInlinee: %s (#%d)\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n", isApplyTarget ? L"apply" : L"call",
            inlineeData->GetFunctionBody()->GetDisplayName(), inlineeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
            inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2),
            this->topFunc->GetJnFunction()->GetDisplayName(), this->topFunc->GetJnFunction()->GetDebugNumberSet(debugStringBuffer3));
        return true;
    }

    return false;
}

bool
Inline::TryGetFixedMethodsForBuiltInAndTarget(IR::Instr *callInstr, const Js::FunctionCodeGenJitTimeData* inlinerData, const Js::FunctionCodeGenJitTimeData* inlineeData, Js::FunctionInfo *builtInFuncInfo,
                                              IR::Instr* builtInLdInstr, IR::Instr* targetLdInstr, bool& safeThis, bool isApplyTarget)
{
#if ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer3[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif



    Assert(isApplyTarget || (Js::JavascriptLibrary::GetBuiltInForFuncInfo(builtInFuncInfo, callInstr->m_func->GetScriptContext()) == Js::BuiltinFunction::Function_Call));

    Js::OpCode originalCallOpCode = callInstr->m_opcode;
    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();

    IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(callInstr->m_func);
    useCallTargetInstr->SetByteCodeOffset(callInstr);
    useCallTargetInstr->byteCodeUpwardExposedUsed = JitAnew(callInstr->m_func->m_alloc, BVSparse<JitArenaAllocator>, callInstr->m_func->m_alloc);

    Js::FunctionInfo* targetFunctionInfo = inlineeData->GetFunctionInfo();
    safeThis = false;
    // Check if we can get fixed method for call
    if (TryOptimizeCallInstrWithFixedMethod(callInstr, builtInFuncInfo/*funcinfo for call*/, false /*isPolymorphic*/, false /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/,
        safeThis /*unused here*/, true /*dontOptimizeJustCheck*/))
    {
        Assert(callInstr->m_opcode == originalCallOpCode); // check that we didn't change the opcode to CallIFixed.
        callInstr->ReplaceSrc1(targetLdInstr->GetDst());
        safeThis = false;
        // Check if we can get fixed method for call target
        if (!TryOptimizeCallInstrWithFixedMethod(callInstr, targetFunctionInfo, false /*isPolymorphic*/, false /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/,
            safeThis /*unused here*/, true /*dontOptimizeJustCheck*/))
        {
            callInstr->ReplaceSrc1(builtInLdInstr->GetDst());
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Skipping %s target inlining, did not get fixed method for %s target \tInlinee: %s (#%d)\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n", isApplyTarget ? L"apply" : L"call", isApplyTarget ? L"apply" : L"call",
                inlineeData->GetFunctionBody()->GetDisplayName(), inlineeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2),
                this->topFunc->GetJnFunction()->GetDisplayName(), this->topFunc->GetJnFunction()->GetDebugNumberSet(debugStringBuffer3));
            return false;
        }
    }
    else
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: Skipping %s target inlining, did not get fixed method for %s \tInlinee: %s (#%d)\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n", isApplyTarget ? L"apply" : L"call", isApplyTarget ? L"apply" : L"call",
            inlineeData->GetFunctionBody()->GetDisplayName(), inlineeData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer),
            inlinerData->GetFunctionBody()->GetDisplayName(), inlinerData->GetFunctionBody()->GetDebugNumberSet(debugStringBuffer2),
            this->topFunc->GetJnFunction()->GetDisplayName(), this->topFunc->GetJnFunction()->GetDebugNumberSet(debugStringBuffer3));
        return false;
    }

    if (isApplyTarget)
    {
        callInstr->m_func->SetHasApplyTargetInlining();
    }

    Assert(callInstr->m_opcode == originalCallOpCode);
    callInstr->ReplaceSrc1(builtInLdInstr->GetDst());

    // Emit Fixed Method check for apply/call
    safeThis = false;
    TryOptimizeCallInstrWithFixedMethod(callInstr, builtInFuncInfo/*funcinfo for apply/call */, false /*isPolymorphic*/, false /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/, safeThis /*unused here*/);

    // If we optimized the call instruction for a fixed function, we must extend the function object's lifetime until after
    // the bailout on non-stack arguments.
    Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
    useCallTargetInstr->byteCodeUpwardExposedUsed->Set(originalCallTargetStackSym->m_id);

    // Make the target of apply/call as the target of the call instruction
    callInstr->ReplaceSrc1(targetLdInstr->GetDst());
    callInstr->m_opcode = originalCallOpCode;

    //Emit Fixed Method check for apply/call target
    originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    safeThis = false;
    TryOptimizeCallInstrWithFixedMethod(callInstr, targetFunctionInfo, false /*isPolymorphic*/, false /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/, safeThis /*unused here*/);

    // If we optimized the call instruction for a fixed function, we must extend the function object's lifetime until after
    // the bailout on non-stack arguments.
    Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
    useCallTargetInstr->byteCodeUpwardExposedUsed->Set(originalCallTargetStackSym->m_id);

    callInstr->InsertBefore(useCallTargetInstr);

    return true;
}

void
Inline::SetupInlineInstrForCallDirect(Js::BuiltinFunction builtInId, IR::Instr* callInstr, IR::Instr* argoutInstr)
{
    switch(builtInId)
    {
    case Js::BuiltinFunction::Array_Concat:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Concat, callInstr->m_func));
        break;

    case Js::BuiltinFunction::Array_IndexOf:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_IndexOf, callInstr->m_func));
        break;

    case Js::BuiltinFunction::Array_Includes:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Includes, callInstr->m_func));
        break;

    case Js::BuiltinFunction::Array_Join:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Join, callInstr->m_func));
        break;

    case Js::BuiltinFunction::Array_LastIndexOf:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_LastIndexOf, callInstr->m_func));
        break;

    case Js::BuiltinFunction::Array_Reverse:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Reverse, callInstr->m_func));
        break;

    case Js::BuiltinFunction::Array_Shift:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Shift, callInstr->m_func));
        break;

    case Js::BuiltinFunction::Array_Slice:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Slice, callInstr->m_func));
        break;

    case Js::BuiltinFunction::Array_Splice:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Splice, callInstr->m_func));
        break;

    case Js::BuiltinFunction::Array_Unshift:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Unshift, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_Concat:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Concat, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_CharCodeAt:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_CharCodeAt, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_CharAt:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_CharAt, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_FromCharCode:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_FromCharCode, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_FromCodePoint:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_FromCodePoint, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_IndexOf:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_IndexOf, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_LastIndexOf:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_LastIndexOf, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_Link:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Link, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_LocaleCompare:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_LocaleCompare, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_Match:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Match, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_Replace:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Replace, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_Search:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Search, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_Slice:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Slice, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_Split:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Split, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_Substr:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Substr, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_Substring:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Substring, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_ToLocaleLowerCase:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_ToLocaleLowerCase, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_ToLocaleUpperCase:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_ToLocaleUpperCase, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_ToLowerCase:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_ToLowerCase, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_ToUpperCase:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_ToUpperCase, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_Trim:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Trim, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_TrimLeft:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_TrimLeft, callInstr->m_func));
        break;

    case Js::BuiltinFunction::String_TrimRight:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_TrimRight, callInstr->m_func));
        break;


    case Js::BuiltinFunction::GlobalObject_ParseInt:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperGlobalObject_ParseInt, callInstr->m_func));
        break;

    case Js::BuiltinFunction::RegExp_Exec:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperRegExp_Exec, callInstr->m_func));
        break;

    };
    callInstr->SetSrc2(argoutInstr->GetDst());
    return;
}

void
Inline::WrapArgsOutWithCoerse(Js::BuiltinFunction builtInId, IR::Instr* callInstr)
{
    switch (builtInId)
    {
    case Js::BuiltinFunction::String_Match:
        callInstr->ForEachCallDirectArgOutInstrBackward([&](IR::Instr *argOutInstr, uint argNum)
        {
            IR::Instr * newInstr = nullptr;
            bool isPreOpBailOutNeeded = false;
            if (argNum == 0)
            {
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Coerse_Str);
                isPreOpBailOutNeeded = true;
                newInstr->GetDst()->SetValueType(ValueType::String);
                newInstr->SetSrc2(IR::AddrOpnd::New(L"String.prototype.match", IR::AddrOpndKindSz, newInstr->m_func));
                argOutInstr->GetSrc1()->SetValueType(ValueType::String);
            }
            else if (argNum == 1)
            {
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Coerse_Regex);
                isPreOpBailOutNeeded = true;
            }
            if (isPreOpBailOutNeeded)
            {
                newInstr->SetByteCodeOffset(argOutInstr);
                newInstr->forcePreOpBailOutIfNeeded = true;
            }
            return false;
        }, 2);
        break;

    case Js::BuiltinFunction::String_Replace:
        callInstr->ForEachCallDirectArgOutInstrBackward([&](IR::Instr *argOutInstr, uint argNum)
        {
            IR::Instr * newInstr = nullptr;
            bool isPreOpBailOutNeeded = false;
            if (argNum == 0)
            {
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Coerse_Str);
                isPreOpBailOutNeeded = true;
                newInstr->GetDst()->SetValueType(ValueType::String);
                newInstr->SetSrc2(IR::AddrOpnd::New(L"String.prototype.replace", IR::AddrOpndKindSz, newInstr->m_func));
                argOutInstr->GetSrc1()->SetValueType(ValueType::String);
            }
            if (argNum == 1)
            {
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Coerse_StrOrRegex);
                isPreOpBailOutNeeded = true;
            }
            if (isPreOpBailOutNeeded)
            {
                newInstr->SetByteCodeOffset(argOutInstr);
                newInstr->forcePreOpBailOutIfNeeded = true;
            }
            return false;
        }, 3);
        break;
    case Js::BuiltinFunction::RegExp_Exec:
        callInstr->ForEachCallDirectArgOutInstrBackward([&](IR::Instr *argOutInstr, uint argNum)
        {
            IR::Instr * newInstr = nullptr;
            bool isPreOpBailOutNeeded = false;
            if (argNum == 0)
            {
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Coerse_Regex);
                isPreOpBailOutNeeded = true;
            }
            else if (argNum == 1)
            {
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Conv_Str);
                newInstr->GetDst()->SetValueType(ValueType::String);
                argOutInstr->GetSrc1()->SetValueType(ValueType::String);
                isPreOpBailOutNeeded = true;
            }
            if (isPreOpBailOutNeeded)
            {
                newInstr->SetByteCodeOffset(argOutInstr);
                newInstr->forcePreOpBailOutIfNeeded = true;
            }
            return false;
        }, 2);
        break;
    }
}

IR::Instr *
Inline::SimulateCallForGetterSetter(IR::Instr *accessorInstr, IR::Instr* insertInstr, IR::PropertySymOpnd* methodOpnd, bool isGetter)
{
    Assert(methodOpnd->UsesAccessor());

    IntConstType argOutCount = isGetter ? 1 : 2; // A setter would have an additional ArgOut in the form of the value being set.

    IR::Instr *ldMethodFld = IR::Instr::New(Js::OpCode::LdMethodFromFlags, IR::RegOpnd::New(TyVar, accessorInstr->m_func), methodOpnd, accessorInstr->m_func);
    insertInstr->InsertBefore(ldMethodFld);
    ldMethodFld = ldMethodFld->ConvertToBailOutInstr(accessorInstr, IR::BailOutFailedInlineTypeCheck);
    ldMethodFld->SetByteCodeOffset(accessorInstr);

    IR::Instr *startCall = IR::Instr::New(Js::OpCode::StartCall, accessorInstr->m_func);
    startCall->SetDst(IR::RegOpnd::New(TyVar, accessorInstr->m_func));
    startCall->SetSrc1(IR::IntConstOpnd::New(argOutCount, TyInt32, accessorInstr->m_func));
    insertInstr->InsertBefore(startCall);
    startCall->SetByteCodeOffset(accessorInstr);

    PropertySym * fieldSym = methodOpnd->AsSymOpnd()->m_sym->AsPropertySym();
    IR::RegOpnd * instanceOpnd = IR::RegOpnd::New(fieldSym->m_stackSym, TyVar, accessorInstr->m_func);

    IR::Instr *argOutThis = IR::Instr::New(Js::OpCode::ArgOut_A, accessorInstr->m_func);

    StackSym *symDst = accessorInstr->m_func->m_symTable->GetArgSlotSym((uint16)(1));
    argOutThis->SetDst(IR::SymOpnd::New(symDst, 0, TyVar, accessorInstr->m_func));

    argOutThis->SetSrc1(instanceOpnd);
    argOutThis->SetSrc2(startCall->GetDst());
    insertInstr->InsertBefore(argOutThis);

    IR::Instr * argOut = nullptr;
    if(!isGetter)
    {
        // Set the src1 of the StFld to be the second ArgOut.
        argOut = IR::Instr::New(Js::OpCode::ArgOut_A, accessorInstr->m_func);
        symDst = accessorInstr->m_func->m_symTable->GetArgSlotSym((uint16)(2));

        argOut->SetDst(IR::SymOpnd::New(symDst, 0, TyVar, accessorInstr->m_func));
        argOut->SetSrc1(accessorInstr->GetSrc1());
        argOut->SetSrc2(argOutThis->GetDst());

        insertInstr->InsertBefore(argOut);
    }

    accessorInstr->ReplaceSrc1(ldMethodFld->GetDst());
    isGetter ? accessorInstr->SetSrc2(argOutThis->GetDst()) : accessorInstr->SetSrc2(argOut->GetDst());

    if(!isGetter)
    {
        accessorInstr->UnlinkDst();
    }

    return startCall;
}

IR::Instr *
Inline::InlineGetterSetterFunction(IR::Instr *accessorInstr, const Js::FunctionCodeGenJitTimeData *const inlineeData, const StackSym *symCallerThis, const uint inlineCacheIndex, bool isGetter, uint recursiveInlineDepth)
{
    // This function is recursive, so when jitting in the foreground, probe the stack
    if (!this->topFunc->IsBackgroundJIT())
    {
        PROBE_STACK(this->topFunc->GetScriptContext(), Js::Constants::MinStackDefault);
    }

    IR::Instr *instrNext = accessorInstr->m_next;

    Js::FunctionBody *funcCaller = accessorInstr->m_func->GetJnFunction();
    Js::FunctionBody *funcBody = inlineeData->GetFunctionBody();

    Assert(!accessorInstr->GetSrc2());

    JS_ETW(EventWriteJSCRIPT_BACKEND_INLINE(
        funcCaller->GetFunctionNumber(), funcBody->GetFunctionNumber(),
        funcCaller->GetExternalDisplayName(), funcBody->GetExternalDisplayName()));

    IR::Instr *inlineBailoutChecksBeforeInstr = accessorInstr;

    Js::ProxyEntryPointInfo *defaultEntryPointInfo = funcBody->GetDefaultEntryPointInfo();
    Assert(defaultEntryPointInfo->IsFunctionEntryPointInfo());
    Js::FunctionEntryPointInfo *functionEntryPointInfo = static_cast<Js::FunctionEntryPointInfo*>(defaultEntryPointInfo);
    JsFunctionCodeGen *workItem = JitAnew(this->topFunc->m_alloc, JsFunctionCodeGen,
        funcBody->GetScriptContext()->GetNativeCodeGenerator(), funcBody, functionEntryPointInfo, this->topFunc->IsJitInDebugMode());
    workItem->SetRecyclableData(JitAnew(this->topFunc->m_alloc, Js::CodeGenRecyclableData, inlineeData));
    workItem->SetJitMode(this->topFunc->m_workItem->GetJitMode());

    IR::RegOpnd * returnValueOpnd;
    Js::RegSlot returnRegSlot;
    if (isGetter && accessorInstr->GetDst())
    {
        returnValueOpnd = accessorInstr->UnlinkDst()->AsRegOpnd();
        returnRegSlot = returnValueOpnd->m_sym->GetByteCodeRegSlot();
    }
    else
    {
        returnValueOpnd = nullptr;
        returnRegSlot = Js::Constants::NoRegister;
    }

    const auto profileInfo =
        JitAnew(
            this->topFunc->m_alloc,
            Js::ReadOnlyDynamicProfileInfo,
            funcBody->HasDynamicProfileInfo() ? funcBody->GetAnyDynamicProfileInfo() : nullptr,
            this->topFunc->IsBackgroundJIT() ? this->topFunc->m_alloc : nullptr);

    Js::EntryPointPolymorphicInlineCacheInfo * entryPointPolymorphicInlineCacheInfo = this->topFunc->m_workItem->GetEntryPoint()->GetPolymorphicInlineCacheInfo();
    Func *inlinee = JitAnew(this->topFunc->m_alloc,
                         Func,
                         this->topFunc->m_alloc,
                         workItem,
                         accessorInstr->m_func->m_runtimeData ?
                            accessorInstr->m_func->m_runtimeData->GetLdFldInlinee(inlineCacheIndex) :
                            this->topFunc->GetJnFunction()->GetLdFldInlineeCodeGenRuntimeData(inlineCacheIndex),
                         entryPointPolymorphicInlineCacheInfo ? entryPointPolymorphicInlineCacheInfo->GetInlineeInfo(funcBody) : nullptr,
                         this->topFunc->GetCodeGenAllocators(),
                         this->topFunc->GetNumberAllocator(),
                         profileInfo,
                         this->topFunc->GetCodeGenProfiler(),
                         this->topFunc->IsBackgroundJIT(),
                         accessorInstr->m_func,
                         accessorInstr->m_next->GetByteCodeOffset(),
                         returnRegSlot,
                         false,
                         UINT16_MAX,
                         true);

    // funcBody->GetInParamsCount() can be greater than one even if it is all undefined. Example defineProperty(a,"foo", {get:function(a,b,c){}});

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::InlinePhase) ||
        Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::InlineAccessorsPhase) || Js::Configuration::Global.flags.Trace.IsEnabled(Js::InlineAccessorsPhase))
    {
        wchar_t debugStringBuffer [MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        PropertySym *propertySym = isGetter ? accessorInstr->GetSrc1()->AsSymOpnd()->m_sym->AsPropertySym() : accessorInstr->GetDst()->AsSymOpnd()->m_sym->AsPropertySym();
        Js::ScriptContext* scriptContext = propertySym->GetFunc()->GetScriptContext();

        Output::Print(L"INLINING: %s: \tInlinee: %s (%s)\tCaller: %s (%s)\t fieldName: %s\n", isGetter ? L"Getter" : L"Setter",
        funcBody->GetDisplayName(), funcBody->GetDebugNumberSet(debugStringBuffer), funcCaller->GetDisplayName(), funcCaller->GetDebugNumberSet(debugStringBuffer2),
                scriptContext->GetPropertyNameLocked(propertySym->m_propertyId)->GetBuffer());
        Output::Flush();
    }
#endif
    IR::Opnd * methodOpnd = isGetter ? accessorInstr->GetSrc1() : accessorInstr->GetDst();
    Assert(methodOpnd->IsSymOpnd() && methodOpnd->AsSymOpnd()->IsPropertySymOpnd());

    inlineBailoutChecksBeforeInstr = SimulateCallForGetterSetter(accessorInstr, accessorInstr, methodOpnd->AsPropertySymOpnd(), isGetter);

    bool safeThis = false;
    TryOptimizeCallInstrWithFixedMethod(accessorInstr, inlineeData->GetFunctionInfo(), false, false, false, true, safeThis);

    return InlineFunctionCommon(accessorInstr, nullptr, funcBody, inlinee, instrNext, returnValueOpnd, inlineBailoutChecksBeforeInstr, symCallerThis, recursiveInlineDepth, safeThis);
}

IR::Instr *
Inline::InlineFunctionCommon(IR::Instr *callInstr, StackSym* originalCallTargetStackSym, Js::FunctionBody *funcBody, Func *inlinee, IR::Instr *instrNext,
                                IR::RegOpnd * returnValueOpnd, IR::Instr *inlineBailoutChecksBeforeInstr, const StackSym *symCallerThis, uint recursiveInlineDepth, bool safeThis, bool isApplyTarget)
{
    BuildIRForInlinee(inlinee, funcBody, callInstr, isApplyTarget, recursiveInlineDepth);

    Js::ArgSlot formalCount = funcBody->GetInParamsCount();
    IR::Instr *argOuts[Js::InlineeCallInfo::MaxInlineeArgoutCount];
#if DBG
    memset(argOuts, 0xFE, sizeof(argOuts));
#endif
    if (callInstr->m_opcode == Js::OpCode::CallIFixed)
    {
        Assert(callInstr->GetFixedFunction()->GetFunctionInfo() == funcBody);
    }
    else
    {
        PrepareInsertionPoint(callInstr, funcBody, inlineBailoutChecksBeforeInstr);
    }

    Assert(formalCount <= Js::InlineeCallInfo::MaxInlineeArgoutCount);
    __analysis_assume(formalCount <= Js::InlineeCallInfo::MaxInlineeArgoutCount);

    IR::Instr *argOutsExtra[Js::InlineeCallInfo::MaxInlineeArgoutCount];
#if DBG
    memset(argOutsExtra, 0xFE, sizeof(argOutsExtra));
#endif

    bool stackArgsArgOutExpanded = false;
    Js::ArgSlot actualCount = MapActuals(callInstr, argOuts, formalCount, inlinee, (Js::ProfileId)callInstr->AsProfiledInstr()->u.profileId, &stackArgsArgOutExpanded, argOutsExtra);
    inlinee->actualCount = actualCount;
    Assert(actualCount > 0);

#if DBG
    if(safeThis)
    {
        Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
    }
#endif

    MapFormals(inlinee, argOuts, formalCount, actualCount, returnValueOpnd, callInstr->GetSrc1(), symCallerThis, stackArgsArgOutExpanded, safeThis, argOutsExtra);

    if (callInstr->m_opcode == Js::OpCode::CallIFixed && !inlinee->isGetterSetter)
    {
        Assert(originalCallTargetStackSym != nullptr);

        // Insert a ByteCodeUsesInstr to make sure the function object's lifetimes is extended beyond the last bailout point
        // at which we may have to call the function again in the interpreter.
        // Don't need to do this for a getter/setter inlinee as, upon bailout, the execution will start in the interpreter at the LdFld/StFld itself.
        callInstr->InsertBefore(IR::ByteCodeUsesInstr::New(callInstr, originalCallTargetStackSym->m_id));
    }

    // InlineeStart indicate the beginning of the inlinee, and we need the stack arg for the inlinee until InlineeEnd
    callInstr->m_opcode = Js::OpCode::InlineeStart;

    // Set it to belong to the inlinee, so that we can use the actual count when lowering InlineeStart
    callInstr->m_func = inlinee;
    callInstr->SetDst(IR::RegOpnd::New(TyVar, inlinee));
    // Put the meta arguments that the stack walker expects to find on the stack.
    SetupInlineeFrame(inlinee, callInstr, actualCount, callInstr->GetSrc1());

    // actualCount + MetaArgCount to include the meta arguments to pop from the inlinee argout stack.
    IR::Instr *inlineeEndInstr = IR::Instr::New(Js::OpCode::InlineeEnd, inlinee);
    inlineeEndInstr->SetByteCodeOffset(inlinee->m_tailInstr->GetPrevRealInstr());
    inlineeEndInstr->SetSrc1(IR::IntConstOpnd::New(actualCount + Js::Constants::InlineeMetaArgCount, TyInt32, callInstr->m_func));
    inlineeEndInstr->SetSrc2(callInstr->GetDst()); // Link the inlinee end to the inlinee Start
    callInstr->InsertAfter(inlineeEndInstr);

    // Move the ArgOut_A_Inlines close to the InlineeStart
    callInstr->MoveArgs();

    inlineeEndInstr->InsertRangeBefore(inlinee->m_headInstr->m_next, inlinee->m_tailInstr->m_prev);
    inlinee->m_headInstr->Free();
    inlinee->m_tailInstr->Free();

    this->topFunc->SetHasInlinee();

    InsertStatementBoundary(instrNext);

    return instrNext;
}

#ifdef ENABLE_DOM_FAST_PATH
// we have LdFld, src1 obj, src2: null; dest: return value
// We need to convert it to inlined method call.
// We cannot do CallDirect as it requires ArgOut and that cannot be hoisted/copyprop'd
// Create a new OpCode, DOMFastPathGetter. The OpCode takes three arguments:
// The function object, the "this" instance object, and the helper routine as we have one for each index
// A functionInfo->Index# table is created in scriptContext (and potentially movable to threadContext if WS is not a concern).
// we use the table to identify the helper that needs to be lowered.
// At lower time we create the call to helper, which is function entrypoint at this time.
IR::Instr * Inline::InlineDOMGetterSetterFunction(IR::Instr *ldFldInstr, const Js::FunctionCodeGenJitTimeData *const inlineeData, const Js::FunctionCodeGenJitTimeData *const inlinerData)
{
    Js::FunctionInfo* functionInfo = inlineeData->GetFunctionInfo();

    Assert(ldFldInstr->GetSrc1()->IsSymOpnd() && ldFldInstr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd());

    Assert(ldFldInstr->GetSrc1()->AsPropertySymOpnd()->HasObjTypeSpecFldInfo());
    Assert(ldFldInstr->GetSrc1()->AsPropertySymOpnd()->GetObjTypeSpecInfo()->UsesAccessor());

    // Find the helper routine for this functionInfo.
    Js::ScriptContext* scriptContext = this->topFunc->GetScriptContext();
    IR::JnHelperMethod helperMethod;
    bool found = scriptContext->EnsureDOMFastPathIRHelperMap()->TryGetValue(functionInfo, &helperMethod);
    Assert(found);

    // Find the instance object (External object).
    PropertySym * fieldSym = ldFldInstr->GetSrc1()->AsSymOpnd()->m_sym->AsPropertySym();
    IR::RegOpnd * instanceOpnd = IR::RegOpnd::New(fieldSym->m_stackSym, TyMachPtr, ldFldInstr->m_func);

    // Find the function object from getter inline cache. Need bailout to verify.
    IR::Instr *ldMethodFld = IR::Instr::New(Js::OpCode::LdMethodFromFlags, IR::RegOpnd::New(TyVar, ldFldInstr->m_func), ldFldInstr->GetSrc1(), ldFldInstr->m_func);
    ldFldInstr->InsertBefore(ldMethodFld);
    ldMethodFld = ldMethodFld->ConvertToBailOutInstr(ldFldInstr, IR::BailOutFailedInlineTypeCheck);

    ldFldInstr->ReplaceSrc1(ldMethodFld->GetDst());
    ldMethodFld->SetByteCodeOffset(ldFldInstr);

    // generate further object/type bailout
    PrepareInsertionPoint(ldFldInstr, functionInfo, ldFldInstr);

    // We have three arguments to pass to the OpCode. Create a new ExtendArg_A opcode to chain up the argument. It is similar to ArgOut chain
    // except that it is not argout.
    // The Opcode sequence is like:
    // (dst)helpArg1: ExtendArg_A (src1)thisObject (src2)null
    // (dst)helpArg2: ExtendArg_A (src1)funcObject (src2)helpArg1
    // method: DOMFastPathGetter (src1)HelperCall (src2)helpArg2
    IR::Instr* extendArg0 = IR::Instr::New(Js::OpCode::ExtendArg_A, IR::RegOpnd::New(TyVar, ldFldInstr->m_func), instanceOpnd, ldFldInstr->m_func);
    ldFldInstr->InsertBefore(extendArg0);
    IR::Instr* extendArg1 = IR::Instr::New(Js::OpCode::ExtendArg_A, IR::RegOpnd::New(TyVar, ldFldInstr->m_func), ldMethodFld->GetDst(), extendArg0->GetDst(), ldFldInstr->m_func);
    ldFldInstr->InsertBefore(extendArg1);
    ldFldInstr->ReplaceSrc1(IR::HelperCallOpnd::New(helperMethod, ldFldInstr->m_func));
    ldFldInstr->SetSrc2(extendArg1->GetDst());
    ldFldInstr->m_opcode = Js::OpCode::DOMFastPathGetter;

    StackSym * tmpSym = StackSym::New(ldFldInstr->GetDst()->GetType(), ldFldInstr->m_func);
    IR::Opnd * tmpDst = IR::RegOpnd::New(tmpSym, tmpSym->GetType(), ldFldInstr->m_func);

    IR::Opnd * callInstrDst = ldFldInstr->UnlinkDst();
    ldFldInstr->SetDst(tmpDst);

    IR::Instr * ldInstr = IR::Instr::New(Js::OpCode::Ld_A, callInstrDst, tmpDst, ldFldInstr->m_func);
    ldFldInstr->InsertAfter(ldInstr);

    this->topFunc->SetHasInlinee();

    InsertStatementBoundary(ldInstr->m_next);

    return ldInstr->m_next;
}
#endif
void
Inline::InsertStatementBoundary(IR::Instr * instrNext)
{
    if (lastStatementBoundary)
    {
        Assert(lastStatementBoundary->m_func == instrNext->m_func);
        IR::PragmaInstr * pragmaInstr = IR::PragmaInstr::New(Js::OpCode::StatementBoundary,
            lastStatementBoundary->m_statementIndex,
            lastStatementBoundary->m_func);
        pragmaInstr->SetByteCodeOffset(instrNext);
        instrNext->InsertBefore(pragmaInstr);
    }
}

IR::Instr *
Inline::InlineScriptFunction(IR::Instr *callInstr, const Js::FunctionCodeGenJitTimeData *const inlineeData, const StackSym *symCallerThis, const Js::ProfileId profileId, bool* pIsInlined, uint recursiveInlineDepth)
{
    *pIsInlined = false;

    // This function is recursive, so when jitting in the foreground, probe the stack
    if (!this->topFunc->IsBackgroundJIT())
    {
        PROBE_STACK(this->topFunc->GetScriptContext(), Js::Constants::MinStackDefault);
    }

    IR::Instr *instrNext = callInstr->m_next;

    Js::FunctionBody *funcCaller = callInstr->m_func->GetJnFunction();
    Js::FunctionBody *funcBody = inlineeData->GetFunctionBody();

    if (callInstr->GetSrc2() &&
        callInstr->GetSrc2()->IsSymOpnd() &&
        callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum() > Js::InlineeCallInfo::MaxInlineeArgoutCount)
    {
#if ENABLE_DEBUG_CONFIG_OPTIONS
        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
        // This is a hard limit as we only use 4 bits to encode the actual count in the InlineeCallInfo. Although
        // InliningDecider already checks for this, the check is against profile data that may not be accurate since profile
        // data matching does not take into account some types of changes to source code. Need to check this again with current
        // information.
        INLINE_TESTTRACE(L"INLINING: Skip Inline: ArgSlot > MaxInlineeArgoutCount\tInlinee: %s (%s)\tArgSlotNum: %d\tMaxInlineeArgoutCount: %d\tCaller: %s (%s)\n",
            funcBody->GetDisplayName(), funcBody->GetDebugNumberSet(debugStringBuffer), callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum(),
            Js::InlineeCallInfo::MaxInlineeArgoutCount, funcCaller->GetDisplayName(), funcCaller->GetDebugNumberSet(debugStringBuffer2));
        return instrNext;
    }

    *pIsInlined = true;

    // Save off the call target operand (function object) so we can extend its lifetime as needed, even if
    // the call instruction gets transformed to CallIFixed.
    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();

    // We are committed to inlining, optimize the call instruction for fixed fields now and don't attempt it later.
    bool isFixed = false;

    JS_ETW(EventWriteJSCRIPT_BACKEND_INLINE(
        funcCaller->GetFunctionNumber(), funcBody->GetFunctionNumber(),
        funcCaller->GetExternalDisplayName(), funcBody->GetExternalDisplayName()));

    bool isCtor = false;
    bool safeThis = false;
    IR::Instr *inlineBailoutChecksBeforeInstr;
    if (callInstr->m_opcode == Js::OpCode::NewScObject || callInstr->m_opcode == Js::OpCode::NewScObjArray)
    {
        isCtor = true;
        isFixed = TryOptimizeCallInstrWithFixedMethod(callInstr, inlineeData->GetFunctionInfo(),
            false /*isPolymorphic*/, false /*isBuiltIn*/, isCtor /*isCtor*/, true /*isInlined*/, safeThis /*&safeThis*/);
        bool split = SplitConstructorCall(callInstr, true, isFixed, &inlineBailoutChecksBeforeInstr);
        Assert(split && inlineBailoutChecksBeforeInstr != nullptr);
    }
    else
    {
        isFixed = TryOptimizeCallInstrWithFixedMethod(callInstr, inlineeData->GetFunctionInfo(),
            false /*isPolymorphic*/, false /*isBuiltIn*/, isCtor /*isCtor*/, true /*isInlined*/, safeThis /*&safeThis*/);
        inlineBailoutChecksBeforeInstr = callInstr;
    }

    Assert(callInstr->IsProfiledInstr());
    Js::ProfileId callSiteId = static_cast<Js::ProfileId>(callInstr->AsProfiledInstr()->u.profileId);
    Assert(callSiteId >= 0);

    Js::ProxyEntryPointInfo *defaultEntryPointInfo = funcBody->GetDefaultEntryPointInfo();
    Assert(defaultEntryPointInfo->IsFunctionEntryPointInfo());
    Js::FunctionEntryPointInfo *functionEntryPointInfo = static_cast<Js::FunctionEntryPointInfo*>(defaultEntryPointInfo);

    JsFunctionCodeGen *workItem = JitAnew(this->topFunc->m_alloc, JsFunctionCodeGen,
        funcBody->GetScriptContext()->GetNativeCodeGenerator(), funcBody, functionEntryPointInfo, this->topFunc->IsJitInDebugMode());
    workItem->SetRecyclableData(JitAnew(this->topFunc->m_alloc, Js::CodeGenRecyclableData, inlineeData));
    workItem->SetJitMode(this->topFunc->m_workItem->GetJitMode());

    IR::RegOpnd * returnValueOpnd;
    Js::RegSlot returnRegSlot;
    if (callInstr->GetDst())
    {
        returnValueOpnd = callInstr->UnlinkDst()->AsRegOpnd();
        returnRegSlot = returnValueOpnd->m_sym->GetByteCodeRegSlot();
    }
    else
    {
        returnValueOpnd = nullptr;
        returnRegSlot = Js::Constants::NoRegister;
    }

    const auto profileInfo =
        JitAnew(
        this->topFunc->m_alloc,
        Js::ReadOnlyDynamicProfileInfo,
        funcBody->HasDynamicProfileInfo() ? funcBody->GetAnyDynamicProfileInfo() : nullptr,
        this->topFunc->IsBackgroundJIT() ? this->topFunc->m_alloc : nullptr);

    Js::EntryPointPolymorphicInlineCacheInfo * entryPointPolymorphicInlineCacheInfo = this->topFunc->m_workItem->GetEntryPoint()->GetPolymorphicInlineCacheInfo();
    Func *inlinee = JitAnew(this->topFunc->m_alloc,
                         Func,
                         this->topFunc->m_alloc,
                         workItem,
                         callInstr->m_func->m_runtimeData ?
                            callInstr->m_func->m_runtimeData->GetInlineeForTargetInlinee(profileId, funcBody) :
                            this->topFunc->GetJnFunction()->GetInlineeCodeGenRuntimeDataForTargetInlinee(profileId, funcBody),
                         entryPointPolymorphicInlineCacheInfo ? entryPointPolymorphicInlineCacheInfo->GetInlineeInfo(funcBody) : nullptr,
                         this->topFunc->GetCodeGenAllocators(),
                         this->topFunc->GetNumberAllocator(),
                         profileInfo,
                         this->topFunc->GetCodeGenProfiler(),
                         this->topFunc->IsBackgroundJIT(),
                         callInstr->m_func,
                         callInstr->m_next->GetByteCodeOffset(),
                         returnRegSlot,
                         isCtor,
                         callSiteId,
                         false);

    return InlineFunctionCommon(callInstr, originalCallTargetStackSym, funcBody, inlinee, instrNext, returnValueOpnd, inlineBailoutChecksBeforeInstr, symCallerThis, recursiveInlineDepth, safeThis);
}

bool
Inline::SplitConstructorCall(IR::Instr *const newObjInstr, const bool isInlined, const bool isFixed, IR::Instr** createObjInstrOut, IR::Instr** callCtorInstrOut) const
{
    Assert(newObjInstr);
    Assert(newObjInstr->m_opcode == Js::OpCode::NewScObject);
    Assert(newObjInstr->GetSrc1());
    Assert(newObjInstr->GetSrc2());

    this->topFunc->SetHasTempObjectProducingInstr(true);

    return
        SplitConstructorCallCommon(
            newObjInstr,
            newObjInstr->GetSrc2(),
            Js::OpCode::NewScObjectNoCtor,
            isInlined,
            isFixed,
            createObjInstrOut,
            callCtorInstrOut);
}

bool
Inline::SplitConstructorCallCommon(
    IR::Instr *const newObjInstr,
    IR::Opnd *const lastArgOpnd,
    const Js::OpCode newObjOpCode,
    const bool isInlined,
    const bool isFixed,
    IR::Instr** createObjInstrOut,
    IR::Instr** callCtorInstrOut) const
{
    Assert(newObjInstr);
    Assert(newObjInstr->GetSrc1());
    Assert(lastArgOpnd);
    Assert(isInlined || isFixed);

    const auto callerFunc = newObjInstr->m_func;

    // Call the NoCtor version of NewScObject

    // Use a temporary register for the newly allocated object (before the call to ctor) - even if we know we'll return this
    // object from the whole operation.  That's so that we don't trash the bytecode register if we need to bail out at
    // object allocation (bytecode instruction has the form [Profiled]NewScObject R6 = R6).
    IR::RegOpnd* createObjDst = nullptr;
    IR::Instr* createObjInstr = nullptr;
    const Js::JitTimeConstructorCache* constructorCache;
    bool returnCreatedObject = false;
    bool skipNewScObj = false;

    if (newObjInstr->IsProfiledInstr())
    {
        Js::ProfileId profiledCallSiteId = static_cast<Js::ProfileId>(newObjInstr->AsProfiledInstr()->u.profileId);
        constructorCache = newObjInstr->m_func->GetConstructorCache(profiledCallSiteId);
        returnCreatedObject = constructorCache != nullptr && constructorCache->ctorHasNoExplicitReturnValue;
        skipNewScObj = constructorCache != nullptr && constructorCache->skipNewScObject;
        if (!skipNewScObj)
        {
            createObjDst = IR::RegOpnd::New(TyVar, callerFunc);
            createObjInstr = IR::ProfiledInstr::New(newObjOpCode, createObjDst, newObjInstr->GetSrc1(), callerFunc);
            createObjInstr->AsProfiledInstr()->u.profileId = profiledCallSiteId;
        }
    }
    else
    {
        constructorCache = nullptr;
        createObjDst = IR::RegOpnd::New(TyVar, callerFunc);
        createObjInstr = IR::Instr::New(newObjOpCode, createObjDst, newObjInstr->GetSrc1(), callerFunc);
    }

    Assert(!isInlined || !skipNewScObj);
    Assert(isFixed || !skipNewScObj);

    // For new Object() and new Array() we have special fast helpers.  We'll let the lowerer convert this instruction directly
    // into a call to one of these helpers.
    if (skipNewScObj)
    {
        Js::JavascriptFunction* ctor = newObjInstr->GetFixedFunction();
        Js::FunctionInfo* ctorInfo = ctor->GetFunctionInfo();
        if ((ctorInfo == &Js::JavascriptObject::EntryInfo::NewInstance || ctorInfo == &Js::JavascriptArray::EntryInfo::NewInstance) &&
            newObjInstr->HasEmptyArgOutChain())
        {
            return false;
        }
    }

    IR::Opnd* thisPtrOpnd;
    if (createObjInstr != nullptr)
    {
        createObjInstr->SetByteCodeOffset(newObjInstr);
        createObjInstr->GetSrc1()->SetIsJITOptimizedReg(true);
        newObjInstr->InsertBefore(createObjInstr);

        createObjDst->SetValueType(ValueType::GetObject(ObjectType::UninitializedObject));
        thisPtrOpnd = createObjDst;
    }
    else
    {
        thisPtrOpnd = IR::AddrOpnd::NewNull(newObjInstr->m_func);
    }

    // Pass the new object to the constructor function with an ArgOut
    const auto thisArgOpnd = IR::SymOpnd::New(callerFunc->m_symTable->GetArgSlotSym(1), TyVar, callerFunc);
    auto instr = IR::Instr::New(Js::OpCode::ArgOut_A, thisArgOpnd, thisPtrOpnd, lastArgOpnd, callerFunc);
    instr->SetByteCodeOffset(newObjInstr);
    instr->GetDst()->SetIsJITOptimizedReg(true);
    instr->GetSrc2()->SetIsJITOptimizedReg(true);
    newObjInstr->InsertBefore(instr);

    // Call the constructor using CallI with isCtorCall set.  If we inline the constructor, and the inlined constructor
    // bails out, the interpreter would be entered with CallFlags_Value as well. If the interpreter starts using the
    // call flags, the proper call flags will need to be specified here by using a different op code specific to constructors.
    if (isFixed)
    {
        newObjInstr->m_opcode = Js::OpCode::CallIFixed;
    }
    else
    {
        newObjInstr->m_opcode = Js::OpCode::CallI;
    }

    newObjInstr->isCtorCall = true;

    if(newObjInstr->GetSrc2())
    {
        newObjInstr->FreeSrc2();
    }
    newObjInstr->SetSrc2(thisArgOpnd);

    const auto insertBeforeInstr = newObjInstr->m_next;
    Assert(insertBeforeInstr);
    const auto nextByteCodeOffsetInstr = newObjInstr->GetNextRealInstrOrLabel();

    // Determine which object to use as the final result of NewScObject, the object passed into the constructor as 'this', or
    // the object returned by the constructor.  We only need this if we don't have a hard-coded constructor cache, or if the
    // constructor returns something explicitly.  Otherwise, we simply return the object we allocated and passed to the constructor.
    if (returnCreatedObject)
    {
        instr = IR::Instr::New(Js::OpCode::Ld_A, newObjInstr->GetDst(), createObjDst, callerFunc);
        instr->SetByteCodeOffset(nextByteCodeOffsetInstr);
        instr->GetDst()->SetIsJITOptimizedReg(true);
        instr->GetSrc1()->SetIsJITOptimizedReg(true);
        insertBeforeInstr->InsertBefore(instr);
    }
    else if (!skipNewScObj)
    {
        Assert(createObjDst != newObjInstr->GetDst());

        // Since we're not returning the default new object, the constructor must be returning something explicitly.  We don't
        // know at this point whether it's an object or not.  If the constructor is later inlined, the value type will be determined
        // from the flow in glob opt.  Otherwise, we'll need to emit an object check.
        newObjInstr->GetDst()->SetValueType(ValueType::Uninitialized);

        instr = IR::Instr::New(Js::OpCode::GetNewScObject, newObjInstr->GetDst(), newObjInstr->GetDst(), createObjDst, callerFunc);
        instr->SetByteCodeOffset(nextByteCodeOffsetInstr);
        instr->GetDst()->SetIsJITOptimizedReg(true);
        instr->GetSrc1()->SetIsJITOptimizedReg(true);
        insertBeforeInstr->InsertBefore(instr);
    }

    // Update the NewScObject cache, but only if we don't have a hard-coded constructor cache.  We only clone caches that
    // don't require update, and once updated a cache never requires an update again.
    if (constructorCache == nullptr)
    {
        instr = IR::Instr::New(Js::OpCode::UpdateNewScObjectCache, callerFunc);
        instr->SetSrc1(newObjInstr->GetSrc1()); // constructor function
        instr->SetSrc2(newObjInstr->GetDst());  // the new object
        instr->SetByteCodeOffset(nextByteCodeOffsetInstr);
        instr->GetSrc1()->SetIsJITOptimizedReg(true);
        instr->GetSrc2()->SetIsJITOptimizedReg(true);
        insertBeforeInstr->InsertBefore(instr);
    }

    if (createObjInstrOut != nullptr)
    {
        *createObjInstrOut = createObjInstr;
    }

    if (callCtorInstrOut != nullptr)
    {
        *callCtorInstrOut = newObjInstr;
    }

    return true;
}

void
Inline::InsertObjectCheck(IR::Instr *callInstr, IR::Instr* insertBeforeInstr, IR::Instr*bailOutIfNotObject)
{
    // Bailout if 'functionRegOpnd' is not an object.
    bailOutIfNotObject->SetSrc1(callInstr->GetSrc1()->AsRegOpnd());
    bailOutIfNotObject->SetByteCodeOffset(insertBeforeInstr);
    insertBeforeInstr->InsertBefore(bailOutIfNotObject);
}

void
Inline::InsertFunctionTypeIdCheck(IR::Instr *callInstr, IR::Instr* insertBeforeInstr, IR::Instr* bailOutIfNotJsFunction)
{
    // functionTypeRegOpnd = Ld functionRegOpnd->type
    IR::IndirOpnd *functionTypeIndirOpnd = IR::IndirOpnd::New(callInstr->GetSrc1()->AsRegOpnd(), Js::RecyclableObject::GetOffsetOfType(), TyMachPtr, callInstr->m_func);
    IR::RegOpnd *functionTypeRegOpnd = IR::RegOpnd::New(TyVar, this->topFunc);
    IR::Instr *instr = IR::Instr::New(Js::OpCode::Ld_A, functionTypeRegOpnd, functionTypeIndirOpnd, callInstr->m_func);
    if(instr->m_func->HasByteCodeOffset())
    {
        instr->SetByteCodeOffset(insertBeforeInstr);
    }
    insertBeforeInstr->InsertBefore(instr);

    CompileAssert(sizeof(Js::TypeId) == sizeof(int32));
    // if (functionTypeRegOpnd->typeId != TypeIds_Function) goto $noInlineLabel
    // BrNeq_I4 $noInlineLabel, functionTypeRegOpnd->typeId, TypeIds_Function
    IR::IndirOpnd *functionTypeIdIndirOpnd = IR::IndirOpnd::New(functionTypeRegOpnd, Js::Type::GetOffsetOfTypeId(), TyInt32, callInstr->m_func);
    IR::IntConstOpnd *typeIdFunctionConstOpnd = IR::IntConstOpnd::New(Js::TypeIds_Function, TyInt32, callInstr->m_func);
    bailOutIfNotJsFunction->SetSrc1(functionTypeIdIndirOpnd);
    bailOutIfNotJsFunction->SetSrc2(typeIdFunctionConstOpnd);
    insertBeforeInstr->InsertBefore(bailOutIfNotJsFunction);
}

void
Inline::InsertJsFunctionCheck(IR::Instr *callInstr, IR::Instr *insertBeforeInstr, IR::BailOutKind bailOutKind)
{
    // This function only inserts bailout for tagged int & TypeIds_Function.
    // As of now this is only used for polymorphic inlining.
    Assert(bailOutKind == IR::BailOutOnPolymorphicInlineFunction);

    Assert(insertBeforeInstr);
    Assert(insertBeforeInstr->m_func == callInstr->m_func);

    // bailOutIfNotFunction is primary bailout instruction
    IR::Instr* bailOutIfNotFunction = IR::BailOutInstr::New(Js::OpCode::BailOnNotEqual, bailOutKind, insertBeforeInstr, callInstr->m_func);

    IR::Instr *bailOutIfNotObject = IR::BailOutInstr::New(Js::OpCode::BailOnNotObject, bailOutKind, bailOutIfNotFunction->GetBailOutInfo(),callInstr->m_func);
    InsertObjectCheck(callInstr, insertBeforeInstr, bailOutIfNotObject);

    InsertFunctionTypeIdCheck(callInstr, insertBeforeInstr, bailOutIfNotFunction);

}

void
Inline::InsertFunctionBodyCheck(IR::Instr *callInstr, IR::Instr *insertBeforeInstr, IR::Instr* bailoutInstr, Js::FunctionInfo *funcInfo)
{
    // if (JavascriptFunction::FromVar(r1)->functionInfo != funcInfo) goto noInlineLabel
    // BrNeq_I4 noInlineLabel, r1->functionInfo, funcInfo
    IR::IndirOpnd* funcBody = IR::IndirOpnd::New(callInstr->GetSrc1()->AsRegOpnd(), Js::JavascriptFunction::GetOffsetOfFunctionInfo(), TyMachPtr, callInstr->m_func);
    IR::AddrOpnd* inlinedFuncBody = IR::AddrOpnd::New(funcInfo, IR::AddrOpndKindDynamicFunctionBody, callInstr->m_func);
    bailoutInstr->SetSrc1(funcBody);
    bailoutInstr->SetSrc2(inlinedFuncBody);

    insertBeforeInstr->InsertBefore(bailoutInstr);
}

void
Inline::InsertFunctionObjectCheck(IR::Instr *callInstr, IR::Instr *insertBeforeInstr, IR::Instr *bailOutInstr, Js::FunctionInfo *funcInfo)
{
     Js::BuiltinFunction index = Js::JavascriptLibrary::GetBuiltInForFuncInfo(funcInfo, callInstr->m_func->GetScriptContext());
    AssertMsg(index < Js::BuiltinFunction::Count, "Invalid built-in index on a call target marked as built-in");

    bailOutInstr->SetSrc1(callInstr->GetSrc1()->AsRegOpnd());
    bailOutInstr->SetSrc2(IR::IntConstOpnd::New(index, TyInt32, callInstr->m_func));
    insertBeforeInstr->InsertBefore(bailOutInstr);
}

IR::Instr *
Inline::PrepareInsertionPoint(IR::Instr *callInstr, Js::FunctionInfo *funcInfo, IR::Instr *insertBeforeInstr, IR::BailOutKind bailOutKind)
{
    Assert(insertBeforeInstr);
    Assert(insertBeforeInstr->m_func == callInstr->m_func);
    Assert(bailOutKind == IR::BailOutOnInlineFunction);

    // FunctionBody check is the primary bailout instruction, create it first
    IR::BailOutInstr* primaryBailOutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNotEqual, bailOutKind, insertBeforeInstr, callInstr->m_func);

    // 1. Bailout if function object is not an object.
    IR::Instr *bailOutIfNotObject = IR::BailOutInstr::New(Js::OpCode::BailOnNotObject,
                                                          bailOutKind,
                                                          primaryBailOutInstr->GetBailOutInfo(),
                                                          callInstr->m_func);
    InsertObjectCheck(callInstr, insertBeforeInstr, bailOutIfNotObject);

    // 2. Bailout if function object is not a TypeId_Function
    IR::Instr* bailOutIfNotJsFunction = IR::BailOutInstr::New(Js::OpCode::BailOnNotEqual, bailOutKind, primaryBailOutInstr->GetBailOutInfo(), callInstr->m_func);
    InsertFunctionTypeIdCheck(callInstr, insertBeforeInstr, bailOutIfNotJsFunction);

    // 3. Bailout if function body doesn't match funcInfo
    InsertFunctionBodyCheck(callInstr, insertBeforeInstr, primaryBailOutInstr, funcInfo);

    return primaryBailOutInstr;
}

uint Inline::CountActuals(IR::Instr *callInstr)
{
    IR::Opnd *linkOpnd = callInstr->GetSrc2();
    uint actualCount = 0;
    if (linkOpnd->IsSymOpnd())
    {
        IR::Instr *argInstr;
        do
        {
            Assert(linkOpnd->IsSymOpnd());
            StackSym *sym = linkOpnd->AsSymOpnd()->m_sym->AsStackSym();
            Assert(sym->m_isSingleDef);
            Assert(sym->IsArgSlotSym());
            argInstr = sym->m_instrDef;
            ++actualCount;
            linkOpnd = argInstr->GetSrc2();
        }
        while (linkOpnd->IsSymOpnd());
    }
    return actualCount;
}

bool Inline::InlConstFoldArg(IR::Instr *instr, __in_ecount_opt(callerArgOutCount) IR::Instr *callerArgOuts[], Js::ArgSlot callerArgOutCount)
{
    Assert(instr->m_opcode == Js::OpCode::ArgOut_A);

    if (PHASE_OFF(Js::InlinerConstFoldPhase, instr->m_func->GetTopFunc()))
    {
        return false;
    }

    IR::Opnd *src1 = instr->GetSrc1();
    IntConstType value;

    if (!src1->IsRegOpnd())
    {
        return false;
    }

    StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;

    if (!sym->IsSingleDef())
    {
        return false;
    }

    IR::Instr *instrDef = sym->GetInstrDef();
    if (!this->InlConstFold(instrDef, &value, callerArgOuts, callerArgOutCount))
    {
        return false;
    }
    return true;
}

bool Inline::InlConstFold(IR::Instr *instr, IntConstType *pValue, __in_ecount_opt(callerArgOutCount) IR::Instr *callerArgOuts[], Js::ArgSlot callerArgOutCount)
{
    IR::Opnd *src1 = instr->GetSrc1();

    if (!src1)
    {
        return false;
    }

    switch (src1->GetKind())
    {
    case IR::OpndKindReg:
        // Walk the tree below
        break;

    case IR::OpndKindIntConst:
        if (instr->m_opcode == Js::OpCode::LdC_A_I4)
        {
            // Found a constant
            *pValue = src1->AsIntConstOpnd()->GetValue();
            return true;
        }
        return false;

    case IR::OpndKindSym:
        if (callerArgOuts && instr->m_opcode == Js::OpCode::ArgIn_A)
        {
            // We have an ArgIn.  Walk the caller's ArgOut tree to see if a constant
            // is passed in to the inlinee.

            Assert(callerArgOuts && callerArgOutCount != (Js::ArgSlot) - 1);
            Assert(src1->AsSymOpnd()->m_sym->AsStackSym()->IsParamSlotSym());
            Js::ArgSlot paramSlot = src1->AsSymOpnd()->m_sym->AsStackSym()->GetParamSlotNum();
            if (paramSlot <= callerArgOutCount)
            {
                IR::Instr *argOut = callerArgOuts[paramSlot - 1];
                IR::Opnd *argOutSrc1 = argOut->GetSrc1();

                if (!argOutSrc1->IsRegOpnd())
                {
                    return false;
                }

                StackSym *sym = argOutSrc1->AsRegOpnd()->m_sym;

                if (!sym->IsSingleDef())
                {
                    return false;
                }

                IR::Instr *instrDef = sym->GetInstrDef();
                // Walk the caller
                return InlConstFold(instrDef, pValue, nullptr, (Js::ArgSlot) - 1);
            }
        }
        else if (src1->AsSymOpnd()->IsPropertySymOpnd())
        {
            // See if we have a LdFld of a fixed field.
            Js::Var var = TryOptimizeInstrWithFixedDataProperty(instr);
            if (!Js::TaggedInt::Is(var))
            {
                return false;
            }
            else
            {
                *pValue = Js::TaggedInt::ToInt32(var);
                return true;
            }
        }
        return false;

    default:
        return false;
    }

    // All that is left is RegOpnds
    Assert(src1->IsRegOpnd());

    StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;

    if (!sym->IsSingleDef())
    {
        return false;
    }

    if (!src1 || !src1->IsRegOpnd() || !src1->AsRegOpnd()->m_sym->IsSingleDef())
    {
        return false;
    }

    IR::Opnd *src2 = instr->GetSrc2();

    if (src2)
    {
        if (!src2->IsRegOpnd() || !src2->AsRegOpnd()->m_sym->IsSingleDef())
        {
            return false;
        }
    }

    // See if src1 can be folded to a constant
    if (!InlConstFold(src1->AsRegOpnd()->m_sym->GetInstrDef(), pValue, callerArgOuts, callerArgOutCount))
    {
        return false;
    }
    IntConstType src1Constant = *pValue;

    // See if src2 (unless it is unary) can be folded to a constant
    if (src2 && !InlConstFold(src2->AsRegOpnd()->m_sym->GetInstrDef(), pValue, callerArgOuts, callerArgOutCount))
    {
        return false;
    }

    // Now let's try to constant fold the current instruction
    if (src2)
    {
        IntConstType src2Constant = *pValue;

        if (!instr->BinaryCalculator(src1Constant, src2Constant, pValue)
            || !Math::FitsInDWord(*pValue))
        {
            return false;
        }

        // Success
        IR::ByteCodeUsesInstr * byteCodeInstr = IR::ByteCodeUsesInstr::New(instr->m_func);
        byteCodeInstr->SetByteCodeOffset(instr);
        StackSym *src1Sym = src1->AsRegOpnd()->m_sym;
        StackSym *src2Sym = src2->AsRegOpnd()->m_sym;

        if (src1Sym->HasByteCodeRegSlot() || src2Sym->HasByteCodeRegSlot())
        {
            if (src1Sym->HasByteCodeRegSlot())
            {
                byteCodeInstr->Set(src1Sym->m_id);
            }
            if (src2Sym->HasByteCodeRegSlot())
            {
                byteCodeInstr->Set(src2Sym->m_id);
            }
            instr->InsertBefore(byteCodeInstr);
        }

#if DBG_DUMP
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::InlinerConstFoldPhase, this->topFunc->GetSourceContextId(), this->topFunc->GetLocalFunctionId()))
        {
            Output::Print(L"Constant folding to %d\n", *pValue);
            instr->Dump();
        }
#endif
        instr->m_opcode = Js::OpCode::LdC_A_I4;
        instr->ReplaceSrc1(IR::IntConstOpnd::New(*pValue, TyInt32, instr->m_func));
        instr->GetDst()->AsRegOpnd()->m_sym->SetIsConst();
        instr->FreeSrc2();
    }
    else
    {
        if (!instr->UnaryCalculator(src1Constant, pValue)
            || !Math::FitsInDWord(*pValue))
        {
            // Skip over BytecodeArgOutCapture
            if (instr->m_opcode == Js::OpCode::BytecodeArgOutCapture)
            {
                return true;
            }
            return false;
        }
        // Success
        StackSym *src1Sym = src1->AsRegOpnd()->m_sym;
        if (src1Sym->HasByteCodeRegSlot())
        {
            IR::ByteCodeUsesInstr * byteCodeInstr = IR::ByteCodeUsesInstr::New(instr->m_func);
            byteCodeInstr->SetByteCodeOffset(instr);
            byteCodeInstr->Set(src1Sym->m_id);
            instr->InsertBefore(byteCodeInstr);
        }

#if DBG_DUMP
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::InlinerConstFoldPhase, this->topFunc->GetSourceContextId(), this->topFunc->GetLocalFunctionId()))
        {
            Output::Print(L"Constant folding to %d\n", *pValue);
            instr->Dump();
        }
#endif
        instr->m_opcode = Js::OpCode::LdC_A_I4;
        instr->ReplaceSrc1(IR::IntConstOpnd::New(*pValue, TyInt32, instr->m_func));
        instr->GetDst()->AsRegOpnd()->m_sym->SetIsConst();
    }

    return true;
}


Js::ArgSlot
Inline::MapActuals(IR::Instr *callInstr, __out_ecount(maxParamCount) IR::Instr *argOuts[],
    Js::ArgSlot formalCount,
    Func* inlinee,
    Js::ProfileId callSiteId,
    bool *stackArgsArgOutExpanded,
    IR::Instr *argOutsExtra[],
    Js::ArgSlot maxParamCount /* = Js::InlineeCallInfo::MaxInlineeArgoutCount*/)
{
    AnalysisAssert(formalCount <= maxParamCount);

    IR::Opnd *linkOpnd = callInstr->GetSrc2();
    Js::ArgSlot actualCount = 0;

    *stackArgsArgOutExpanded = false;
    uint inlineeFrameSlot = currentInlineeFrameSlot + (Js::Constants::InlineeMetaArgCount - 1);
    uint fixupArgoutCount = 0;

    if (inlinee)
    {
        bool hasArgumentsAccess = this->GetInlineeHasArgumentObject(inlinee);
        inlinee->SetHasUnoptimizedArgumentsAccess(hasArgumentsAccess);
    }

    if (linkOpnd->IsSymOpnd())
    {
        IR::Instr *argInstr;
        do
        {
            Assert(linkOpnd->IsSymOpnd());
            StackSym *sym = linkOpnd->AsSymOpnd()->m_sym->AsStackSym();

            Assert(sym->m_isSingleDef);
            Assert(sym->IsArgSlotSym());
            sym->m_isInlinedArgSlot = true;
            this->topFunc->SetArgOffset(sym, (inlineeFrameSlot + sym->GetArgSlotNum()) * MachPtr);
            argInstr = sym->m_instrDef;
            if (argInstr->m_opcode == Js::OpCode::ArgOut_A)
            {
                if(inlinee)
                {
                    if (!inlinee->GetHasUnoptimizedArgumentsAcccess())
                    {
                        // This allows us to markTemp the argOut source.
                        argInstr->m_opcode = Js::OpCode::ArgOut_A_Inline;
                    }
                }
                argInstr->GenerateBytecodeArgOutCapture();
            }

            // Expand
            //
            // s31 ArgOut_A s32
            // s30 ArgOut_A_FromStackArgs s31
            //
            // to
            //
            // s31 ArgOut_A(_Inline) s32
            // sXX ArgOut_A_FixupForStackArgs s31
            // .
            // .
            // s34 ArgOut_A_FixupForStackArgs sXX
            // s30 ArgOut_A_FromStackArgs s34

            if (inlinee && argInstr->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs)
            {
                IR::Instr * argFixupInstr;
                for(uint currentFormal = 1; currentFormal < formalCount; currentFormal++)
                {
                    StackSym* newStackSym = StackSym::NewArgSlotSym(sym->GetArgSlotNum(), argInstr->m_func);
                    newStackSym->m_isInlinedArgSlot = true;

                    IR::SymOpnd * linkOpnd = IR::SymOpnd::New(newStackSym, sym->GetType(), argInstr->m_func);
                    IR::Opnd * undefined = IR::AddrOpnd::New(this->topFunc->GetScriptContext()->GetLibrary()->GetUndefined(),
                                                IR::AddrOpndKindDynamicVar, this->topFunc, true);
                    undefined->SetValueType(ValueType::Undefined);

                    argFixupInstr = IR::Instr::New(Js::OpCode::ArgOut_A_FixupForStackArgs, linkOpnd, undefined, argInstr->GetSrc2(), argInstr->m_func);
                    argInstr->InsertBefore(argFixupInstr);
                    argInstr->ReplaceSrc2(argFixupInstr->GetDst());
                    sym->IncrementArgSlotNum();
                    argInstr->m_func->SetArgOffset(sym, (inlineeFrameSlot + sym->GetArgSlotNum()) * MachPtr);

                    argFixupInstr->GenerateArgOutSnapshot();
                    fixupArgoutCount++;
                }
                // Now that the arguments object has been expanded, we don't require the sym corresponding to it.
                IR::IntConstOpnd* callSiteIdOpnd = IR::IntConstOpnd::New(callSiteId, TyUint16, argInstr->m_func);
                argInstr->ReplaceSrc1(callSiteIdOpnd);
                // Don't count ArgOut_A_FromStackArgs as an actual, when it has been expanded
                --actualCount;
                *stackArgsArgOutExpanded = true;
            }
            ++actualCount;
            const Js::ArgSlot currentActual = sym->GetArgSlotNum() - 1;
            if (currentActual < formalCount)
            {
                Assert(currentActual < Js::InlineeCallInfo::MaxInlineeArgoutCount);
                argOuts[currentActual] = argInstr;
            }

            // We don't want to treat ArgOut_A_FromStackArgs as an actual arg.
            else if (argInstr->m_opcode != Js::OpCode::ArgOut_A_FromStackArgs)
            {
                Assert(currentActual <= Js::InlineeCallInfo::MaxInlineeArgoutCount);
                if(argOutsExtra)
                {
                    argOutsExtra[currentActual] = argInstr;
                    if (currentActual < maxParamCount)
                    {
                        __analysis_assume(currentActual < Js::InlineeCallInfo::MaxInlineeArgoutCount);
                        argOuts[currentActual] = nullptr;
                    }
                }
            }
            linkOpnd = argInstr->GetSrc2();
        }
        while (linkOpnd->IsSymOpnd());
#if DBG
        Assert(actualCount <= Js::InlineeCallInfo::MaxInlineeArgoutCount);
        for(Js::ArgSlot i = 0; i < min(actualCount, formalCount); ++i)
        {
#pragma prefast(suppress:6001)
            Assert(argOuts[i]);
        }
#endif
    }

    Assert(linkOpnd->IsRegOpnd());
    Assert(linkOpnd->AsRegOpnd()->m_sym->m_isSingleDef);
    Js::OpCode startCallOpCode = linkOpnd->AsRegOpnd()->m_sym->m_instrDef->m_opcode;
    Assert(startCallOpCode == Js::OpCode::StartCall);

    // Update the count in StartCall to reflect
    //  1. ArgOut_A_FromStackArgs is not an actual once it has been expanded.
    //  2. The expanded argouts (from ArgOut_A_FromStackArgs).
    //
    // Note that the StartCall will reflect the formal count only as of now; the actual count would be set during MapFormals
    if(*stackArgsArgOutExpanded)
    {
        // TODO: Is an underflow here intended, it triggers on test\inlining\OS_2733280.js
        IR::IntConstOpnd * countOpnd = linkOpnd->AsRegOpnd()->m_sym->m_instrDef->GetSrc1()->AsIntConstOpnd();
        int32 count = countOpnd->AsInt32();
        count += fixupArgoutCount - 1;
        countOpnd->SetValue(count);

        callInstr->m_func->EnsureCallSiteToArgumentsOffsetFixupMap();
        Assert(!(callInstr->m_func->callSiteToArgumentsOffsetFixupMap->ContainsKey(callSiteId)));
        callInstr->m_func->callSiteToArgumentsOffsetFixupMap->Add(callSiteId, fixupArgoutCount - 1);
    }

    Assert(linkOpnd->AsRegOpnd()->m_sym->m_instrDef->GetArgOutCount(/*getInterpreterArgOutCount*/ false) == actualCount);

    // Mark the StartCall's dst as an inlined arg slot as well so we know this is an inlined start call
    // and not adjust the stack height on x86
    linkOpnd->AsRegOpnd()->m_sym->m_isInlinedArgSlot = true;

    // Missing arguments...
    for (Js::ArgSlot i = actualCount; i < formalCount; i++)
    {
        argOuts[i] = nullptr;
    }

    // We may not know the exact number of actuals that "b" gets in a.b.apply just yet, since we have expanded the ArgOut_A_FromStackArgs based on the number of formals "b" accepts.
    // So, return the actualCount stored on the func if the ArgOut_A_FromStackArgs was expanded (and thus, the expanded argouts were accounted for in calculating the local actualCount)
    return *stackArgsArgOutExpanded ? callInstr->m_func->actualCount : actualCount;
}

void
Inline::MapFormals(Func *inlinee,
    __in_ecount(formalCount) IR::Instr *argOuts[],
    uint formalCount,
    uint actualCount,
    IR::RegOpnd *retOpnd,
    IR::Opnd * funcObjOpnd,
    const StackSym *symCallerThis,
    bool stackArgsArgOutExpanded,
    bool fixedFunctionSafeThis,
    IR::Instr *argOutsExtra[])
{
    IR::SymOpnd *formalOpnd;
    uint argIndex;
    uint formalCountForInlinee;
    IR::Instr * argInstr;
    IR::Opnd * linkOpnd;

    bool fUsesSafeThis = false;
    bool fUsesConstThis = false;
    StackSym *symThis = nullptr;
    Js::Var thisConstVar = nullptr;

    FOREACH_INSTR_EDITING(instr, instrNext, inlinee->m_headInstr)
    {
        switch (instr->m_opcode)
        {
        case Js::OpCode::ArgIn_Rest:
        {
            // We only currently support a statically known number of actuals.
            if (stackArgsArgOutExpanded)
            {
                break;
            }

            IR::Opnd *restDst = instr->GetDst();

            Assert(actualCount < 1 << 24 && formalCount < 1 << 24); // 24 bits for arg count (see CallInfo.h)
            int excess = actualCount - formalCount;

            if (excess < 0)
            {
                excess = 0;
            }

            // Set the type info about the destination so the array offsets get calculated properly.
            restDst->SetValueType(
                ValueType::GetObject(ObjectType::Array)
                .SetHasNoMissingValues(true)
                .SetArrayTypeId(Js::TypeIds_Array));
            restDst->SetValueTypeFixed();

            // Create the array and assign the elements.
            IR::Instr *newArrInstr = IR::Instr::New(Js::OpCode::NewScArray, restDst, IR::IntConstOpnd::New(excess, TyUint32, inlinee), inlinee);
            instr->InsertBefore(newArrInstr);

            for (uint i = formalCount; i < actualCount; ++i)
            {
                IR::IndirOpnd *arrayLocOpnd = IR::IndirOpnd::New(restDst->AsRegOpnd(), i - formalCount, TyVar, inlinee);
                IR::Instr *stElemInstr = IR::Instr::New(Js::OpCode::StElemC, arrayLocOpnd, argOutsExtra[i]->GetBytecodeArgOutCapture()->GetDst(), inlinee);
                instr->InsertBefore(stElemInstr);
            }

            instr->Remove();

            break;
        }

        case Js::OpCode::ArgIn_A:
            formalOpnd = instr->UnlinkSrc1()->AsSymOpnd();
            argIndex = formalOpnd->m_sym->AsStackSym()->GetParamSlotNum() - 1;
            if (argIndex >= formalCount)
            {
                Fatal();
            }
            formalOpnd->Free(this->topFunc);
            if (argOuts[argIndex])
            {
                IR::Instr *argOut = argOuts[argIndex];
                IR::Instr* instrDef;
                if (argOut->HasByteCodeArgOutCapture())
                {
                    instrDef = argOut->GetBytecodeArgOutCapture();
                }
                else
                {
                    Assert(argOut->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs);
                    instrDef = argOut->GetArgOutSnapshot();
                }

                instr->SetSrc1(instrDef->GetDst());
                instr->m_opcode = Js::OpCode::Ld_A;
                IR::Opnd* dst = instr->GetDst();
                IR::Opnd* src = instrDef->GetSrc1();

                if (argIndex == 0)
                {
                    // Look at the "this" argument source.
                    // If it's known to be a normal object (the caller has already guaranteed that, or
                    // it was defined by an instruction that produces normal objects), we'll omit CheckThis.
                    // If it's a constant value, we'll do the mapping at jit time and copy the final value.
                    if (src->IsRegOpnd())
                    {
                        symThis = dst->AsRegOpnd()->m_sym;

                        StackSym *symSrc = src->AsRegOpnd()->m_sym;
                        if (symSrc == symCallerThis ||
                            symSrc->m_isSafeThis ||
                            inlinee->IsInlinedConstructor())
                        {
                            fUsesSafeThis = true;
                        }
                        else if (symSrc->m_isSingleDef && symSrc->IsConst() && !symSrc->IsIntConst() && !symSrc->IsFloatConst())
                        {
                            thisConstVar = symSrc->GetConstAddress();
                            fUsesConstThis = true;
                        }
                        else if(fixedFunctionSafeThis)
                        {
                            // Note this need to come after we determined that this pointer is not const (undefined/null)
                            fUsesSafeThis = true;
                        }
                    }
                }
            }
            else
            {
                instr->SetSrc1(IR::AddrOpnd::New(this->topFunc->GetScriptContext()->GetLibrary()->GetUndefined(),
                    IR::AddrOpndKindDynamicVar, this->topFunc, true));
                instr->GetSrc1()->SetValueType(ValueType::Undefined);
                instr->m_opcode = Js::OpCode::Ld_A;
            }
            break;

        case Js::OpCode::ArgOut_A_FromStackArgs:
            {
                linkOpnd = instr->GetSrc2();
                if(!linkOpnd->IsSymOpnd())
                {
                    break;
                }

                Assert(instr->GetSrc1()->IsIntConstOpnd());
                Js::ProfileId callSiteId = static_cast<Js::ProfileId>(instr->GetSrc1()->AsIntConstOpnd()->GetValue());

                argInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                while(linkOpnd->IsSymOpnd())
                {
                    argInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                    linkOpnd = argInstr->GetSrc2();
                }

                Assert(linkOpnd->IsRegOpnd());
                Js::OpCode startCallOpCode = linkOpnd->AsRegOpnd()->m_sym->AsStackSym()->GetInstrDef()->m_opcode;
                Assert(startCallOpCode == Js::OpCode::StartCall);
                IR::Instr* startCallForInlinee = linkOpnd->AsRegOpnd()->m_sym->AsStackSym()->GetInstrDef();
                formalCountForInlinee = startCallForInlinee->GetArgOutCount(false); // As of now, StartCall has the formal count

                if(actualCount < formalCountForInlinee)
                {
                    RemoveExtraFixupArgouts(instr, formalCountForInlinee - actualCount, callSiteId);
                    startCallForInlinee->GetSrc1()->AsIntConstOpnd()->DecrValue(formalCountForInlinee - actualCount); //account for the extra formals
                }

                linkOpnd = instr->GetSrc2();
                argInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                argIndex = ((actualCount < formalCountForInlinee) ? actualCount : formalCountForInlinee) - 1;
                for ( ; argIndex > 0; argIndex--)
                {
                    if(argInstr->m_opcode != Js::OpCode::ArgOut_A_FixupForStackArgs)
                    {
                        break;
                    }

                    Assert(!argInstr->HasByteCodeArgOutCapture()); // ArgOut_A_FixupForStackArgs should not be restored on bailout, so we don't generate ByteCodeArgOutCapture for these argouts.

                    IR::Instr* currentArgOutInstr = nullptr;
                    if(argOuts[argIndex])
                    {
                        currentArgOutInstr = argOuts[argIndex];
                    }
                    else if(argOutsExtra && argOutsExtra[argIndex])
                    {
                        currentArgOutInstr = argOutsExtra[argIndex];
                    }
                    if(currentArgOutInstr)
                    {
                        Assert(currentArgOutInstr->m_opcode == Js::OpCode::ArgOut_A || currentArgOutInstr->m_opcode == Js::OpCode::ArgOut_A_Inline);

                        IR::Instr* bytecodeArgoutCapture = currentArgOutInstr->GetBytecodeArgOutCapture();
                        IR::Instr* formalArgOutUse = argInstr->GetArgOutSnapshot();

                        Assert(formalArgOutUse->m_opcode == Js::OpCode::Ld_A);
                        Assert(formalArgOutUse->GetSrc1()->AsAddrOpnd()->m_address == this->topFunc->GetScriptContext()->GetLibrary()->GetUndefined());

                        formalArgOutUse->ReplaceSrc1(bytecodeArgoutCapture->GetDst());

                        linkOpnd = argInstr->GetSrc2();
                        argInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                    }
                }

                if (formalCountForInlinee < actualCount)
                {
                    FixupExtraActualParams(instr, argOuts, argOutsExtra, formalCountForInlinee, actualCount, callSiteId);
                    startCallForInlinee->GetSrc1()->AsIntConstOpnd()->IncrValue(actualCount - formalCountForInlinee); //account for the extra actuals
                }

                break;
            }

        case Js::OpCode::InlineeStart:
            {
                linkOpnd = instr->GetSrc2();
                if(!linkOpnd->IsSymOpnd())
                {
                    break;
                }
                IR::Instr* stackArgsInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                if (stackArgsInstr->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs)
                {
                    linkOpnd = stackArgsInstr->GetSrc2();
                    argInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                    Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A_Inline || argInstr->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs || argInstr->m_opcode == Js::OpCode::ArgOut_A);
                    stackArgsInstr->Remove();
                    Assert(argInstr->GetDst()->IsSymOpnd());
                    instr->ReplaceSrc2(argInstr->GetDst());
                }
                break;
            }

        case Js::OpCode::LdEnv:
            if (instr->m_func == inlinee)
            {
                // Need to give the inlinee's function to load the environment
                if (funcObjOpnd->IsAddrOpnd())
                {
                    instr->m_opcode = Js::OpCode::Ld_A;
                    instr->SetSrc1(IR::AddrOpnd::New(Js::ScriptFunction::FromVar(funcObjOpnd->AsAddrOpnd()->m_address)->GetEnvironment(),
                        IR::AddrOpndKindDynamicFrameDisplay, instr->m_func));
                }
                else
                {
                    instr->SetSrc1(funcObjOpnd);
                }
            }
            else
            {
                Assert(instr->GetSrc1() != nullptr);
            }
            break;

        case Js::OpCode::LdNewTarget:
            if (instr->m_func == inlinee)
            {
                if (instr->m_func->IsInlinedConstructor())
                {
                    instr->SetSrc1(funcObjOpnd);
                }
                else
                {
                    instr->SetSrc1(IR::AddrOpnd::New(this->topFunc->GetScriptContext()->GetLibrary()->GetUndefined(),
                        IR::AddrOpndKindDynamicVar, this->topFunc, true));
                    instr->GetSrc1()->SetValueType(ValueType::Undefined);
                }
                instr->m_opcode = Js::OpCode::Ld_A;
            }
            break;

        case Js::OpCode::ChkNewCallFlag:
            if (instr->m_func == inlinee)
            {
                if (instr->m_func->IsInlinedConstructor())
                {
                    instr->Remove();
                }
                else
                {
                    // InliningDecider::Inline should have decided not to inline this since we are going to end up throwing anyway
                    Assert(false);
                }
            }
            break;

        case Js::OpCode::LdSuper:
        case Js::OpCode::LdSuperCtor:
            if (instr->m_func == inlinee)
            {
                instr->SetSrc1(funcObjOpnd);
            }
            else
            {
                Assert(instr->GetSrc1() != nullptr);
            }
            break;


        case Js::OpCode::LdThis:
        case Js::OpCode::StrictLdThis:
            // Optimization of LdThis may be possible.
            // Verify that this is a use of the "this" passed by the caller (not a nested function).
            if (instr->GetSrc1()->AsRegOpnd()->m_sym == symThis)
            {
                if (fUsesSafeThis)
                {
                    // No need for any "this" mapping.
                    instrNext = this->RemoveLdThis(instr);
                    break;
                }
                else if (fUsesConstThis)
                {
                    // "this" is a constant, so map it now.
                    // Don't bother mapping if it's not an object, though, since we'd have to create a
                    // boxed value at JIT time, and that case doesn't seem worth it.
                    Js::TypeId typeId = Js::JavascriptOperators::GetTypeId(thisConstVar);
                    if (Js::JavascriptOperators::IsObjectType(typeId) ||
                        Js::JavascriptOperators::IsUndefinedOrNullType(typeId))
                    {
                        Js::ScriptContext *scriptContext = inlinee->GetScriptContext();
                        if (instr->m_opcode == Js::OpCode::LdThis)
                        {
                            thisConstVar = Js::JavascriptOperators::OP_GetThis(
                                thisConstVar, instr->GetSrc2()->AsIntConstOpnd()->AsInt32(), scriptContext);
                            instr->FreeSrc2();
                        }
                        else
                        {
                            thisConstVar = Js::JavascriptOperators::OP_StrictGetThis(thisConstVar, scriptContext);
                        }
                        IR::Opnd *thisOpnd = IR::AddrOpnd::New(thisConstVar, IR::AddrOpndKindDynamicVar, inlinee, true);

                        instr->m_opcode = Js::OpCode::Ld_A;
                        instr->ReplaceSrc1(thisOpnd);
                        break;
                    }
                }
            }

            // Couldn't eliminate the execution-time "this" mapping. Try to change it to a check.
            instrNext = this->DoCheckThisOpt(instr);
            break;

        case Js::OpCode::Throw:
            instr->m_opcode = Js::OpCode::InlineThrow;
            instr->m_func->SetHasImplicitCallsOnSelfAndParents();
            break;

        case Js::OpCode::RuntimeTypeError:
            instr->m_opcode = Js::OpCode::InlineRuntimeTypeError;
            instr->m_func->SetHasImplicitCallsOnSelfAndParents();
            break;

        case Js::OpCode::RuntimeReferenceError:
            instr->m_opcode = Js::OpCode::InlineRuntimeReferenceError;
            instr->m_func->SetHasImplicitCallsOnSelfAndParents();
            break;

        case Js::OpCode::Ret:
            if (!retOpnd)
            {
                instr->Remove();
            }
            else
            {
                instr->m_opcode = Js::OpCode::Ld_A;
                instr->SetDst(retOpnd);
            }
            break;
        }
    } NEXT_INSTR_EDITING;
}

void
Inline::SetupInlineeFrame(Func *inlinee, IR::Instr *inlineeStart, Js::ArgSlot actualCount, IR::Opnd *functionObject)
{
    Js::ArgSlot argSlots[Js::Constants::InlineeMetaArgCount] = {
        actualCount + 1, /* argc */
        actualCount + 2, /* function object */
        actualCount + 3  /* arguments object slot */
    };

    IR::Opnd *srcs[Js::Constants::InlineeMetaArgCount] = {
        IR::AddrOpnd::New((Js::Var)actualCount, IR::AddrOpndKindConstant, inlinee, true /*dontEncode*/),

        /*
         * Don't initialize this slot with the function object yet. In compat mode we evaluate
         * the target only after evaluating all arguments. Having this SymOpnd here ensures it gets
         * the correct slot in the frame. Lowerer fills this slot with the function object just
         * before entering the inlinee when we're sure we've evaluated the target in all modes.
         */
        nullptr,

        IR::AddrOpnd::NewNull(inlinee)
    };

    const IRType types[Js::Constants::InlineeMetaArgCount] = {
        TyMachReg,
        TyVar,
        TyMachReg
    };

    for (unsigned instrIndex = 0; instrIndex < Js::Constants::InlineeMetaArgCount; instrIndex++)
    {
        StackSym    *stackSym = inlinee->m_symTable->GetArgSlotSym(argSlots[instrIndex]);
        stackSym->m_isInlinedArgSlot = true;
        this->topFunc->SetArgOffset(stackSym, (currentInlineeFrameSlot + instrIndex) * MachPtr);
        IR::SymOpnd *symOpnd  = IR::SymOpnd::New(stackSym, 0, types[instrIndex], inlinee);

        IR::Instr   *instr    = IR::Instr::New(Js::OpCode::InlineeMetaArg, inlinee);
        instr->SetDst(symOpnd);
        if (srcs[instrIndex])
        {
            instr->SetSrc1(srcs[instrIndex]);
        }
        inlineeStart->InsertBefore(instr);

        if (instrIndex == 0)
        {
            inlinee->SetInlineeFrameStartSym(stackSym);
        }
    }
}

void
Inline::FixupExtraActualParams(IR::Instr * instr, IR::Instr *argOuts[], IR::Instr *argOutsExtra[], uint index, uint actualCount, Js::ProfileId callSiteId)
{
    Assert(instr->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs);

    int offsetFixup;
    Assert(instr->m_func->callSiteToArgumentsOffsetFixupMap->ContainsKey(callSiteId));
    instr->m_func->callSiteToArgumentsOffsetFixupMap->TryGetValue(callSiteId, &offsetFixup);

    StackSym *sym = instr->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
    while (index < actualCount)
    {
        IR::Instr* argOutToMapTo = argOuts[index] ? argOuts[index] : argOutsExtra[index];
        StackSym* newStackSym = StackSym::NewArgSlotSym(sym->GetArgSlotNum(), instr->m_func);
        newStackSym->m_isInlinedArgSlot = true;
        this->topFunc->SetArgOffset(newStackSym, sym->m_offset);
        sym->IncrementArgSlotNum();
        this->topFunc->SetArgOffset(sym, sym->m_offset + MachPtr);

        IR::SymOpnd * linkOpnd = IR::SymOpnd::New(newStackSym, sym->GetType(), instr->m_func);

        IR::Instr * extraActualParamInstr = IR::Instr::New(Js::OpCode::ArgOut_A_FixupForStackArgs, linkOpnd, argOutToMapTo->GetSrc1(), instr->GetSrc2(), instr->m_func);
        instr->InsertBefore(extraActualParamInstr);
        extraActualParamInstr->GenerateArgOutSnapshot();

        instr->m_func->callSiteToArgumentsOffsetFixupMap->Item(callSiteId, ++offsetFixup);

        instr->ReplaceSrc2(extraActualParamInstr->GetDst());

        index++;
    }
}

void
Inline::RemoveExtraFixupArgouts(IR::Instr* instr, uint argoutRemoveCount, Js::ProfileId callSiteId)
{
    Assert(instr->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs);

    int offsetFixup;
    Assert(instr->m_func->callSiteToArgumentsOffsetFixupMap->ContainsKey(callSiteId));
    instr->m_func->callSiteToArgumentsOffsetFixupMap->TryGetValue(callSiteId, &offsetFixup);

    StackSym* argSym = instr->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
    IR::Instr* argInstr = instr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
    for(uint argIndex = 0; argIndex < argoutRemoveCount; argIndex++)
    {
        Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs);
        Assert(!argInstr->HasByteCodeArgOutCapture()); // ArgOut_A_FixupForStackArgs should not be restored on bailout, so we don't generate ByteCodeArgOutCapture for these argouts.

        instr->ReplaceSrc2(argInstr->GetSrc2());
        argSym->DecrementArgSlotNum();
        argSym->m_offset -= MachPtr;
        argSym->m_allocated = true;
        argInstr->Remove();

        instr->m_func->callSiteToArgumentsOffsetFixupMap->Item(callSiteId, --offsetFixup);

        argInstr = instr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
    }
}

IR::Instr *
Inline::DoCheckThisOpt(IR::Instr * instr)
{
    IR::Instr * instrNext = instr->m_next;

    if (PHASE_OFF(Js::CheckThisPhase, instr->m_func->GetTopFunc()))
    {
        return instrNext;
    }

    if (!PHASE_FORCE(Js::CheckThisPhase, instr->m_func->GetTopFunc()))
    {
        if (!instr->m_func->HasProfileInfo())
        {
            return instrNext;
        }

        if (instr->m_func->GetProfileInfo()->GetThisInfo().thisType != Js::ThisType_Simple)
        {
            return instrNext;
        }

        if (instr->m_func->GetProfileInfo()->IsCheckThisDisabled())
        {
            return instrNext;
        }
    }

    // If the instr is an inlined LdThis, try to replace it with a CheckThis
    // that will bail out if a helper call is required to get the real "this" pointer.

    Assert(instr->m_opcode == Js::OpCode::LdThis || instr->m_opcode == Js::OpCode::StrictLdThis);
    Assert(instr->IsInlined());

    // Create the CheckThis. The target is the original offset, i.e., the LdThis still has to be executed.
    if(instr->m_opcode == Js::OpCode::LdThis)
    {
        instr->FreeSrc2();
    }
    IR::Instr *newInstr =
        IR::BailOutInstr::New( instr->m_opcode == Js::OpCode::LdThis ? Js::OpCode::CheckThis : Js::OpCode::StrictCheckThis, IR::BailOutCheckThis, instr, instr->m_func);
    // Just re-use the original src1 since the LdThis will usually be deleted.
    newInstr->SetSrc1(instr->GetSrc1());
    newInstr->SetByteCodeOffset(instr);
    instr->InsertBefore(newInstr);

    return this->RemoveLdThis(instr);
}

IR::Instr *
Inline::RemoveLdThis(IR::Instr *instr)
{
    // Replace the original instr with a copy, if needed.
    if (instr->GetDst()->IsEqual(instr->GetSrc1()))
    {
        // The copy would be a nop, so just delete.
        IR::Instr *instrNext = instr->m_next;
        instr->Remove();
        return instrNext;
    }
    else
    {
        instr->m_opcode = Js::OpCode::Ld_A;
        return instr;
    }
}

bool
Inline::IsArgumentsOpnd(IR::Opnd* opnd, SymID argumentsSymId)
{
    if (opnd->IsRegOpnd())
    {
        return argumentsSymId ==  opnd->AsRegOpnd()->m_sym->m_id;
    }
    else if (opnd->IsSymOpnd())
    {
        Sym *sym = opnd->AsSymOpnd()->m_sym;
        if (sym && sym->IsPropertySym())
        {
            PropertySym *propertySym = sym->AsPropertySym();
            return argumentsSymId == propertySym->m_stackSym->m_id;
        }
        return false;
    }
    else if (opnd->IsIndirOpnd())
    {
        IR::RegOpnd *indexOpnd = opnd->AsIndirOpnd()->GetIndexOpnd();
        IR::RegOpnd *baseOpnd = opnd->AsIndirOpnd()->GetBaseOpnd();
        return  (argumentsSymId == baseOpnd->m_sym->m_id) || (indexOpnd && indexOpnd->m_sym->m_id == argumentsSymId);
    }
    AssertMsg(false, "Unknown type");
    return false;
}


bool
Inline::HasArgumentsAccess(IR::Opnd *opnd, SymID argumentsSymId)
{
    // We should look at dst last to correctly handle cases where it's the same as one of the src operands.
    if (opnd)
    {
        if (opnd->IsRegOpnd() || opnd->IsSymOpnd() || opnd->IsIndirOpnd())
        {
            if (IsArgumentsOpnd(opnd, argumentsSymId))
            {
                return true;
            }
        }
    }
    return false;
}

bool
Inline::HasArgumentsAccess(IR::Instr * instr, SymID argumentsSymId)
{
    IR::Opnd* dst = instr->GetDst();
    IR::Opnd* src1 = instr->GetSrc1();
    IR::Opnd* src2 = instr->GetSrc2();

    // Super conservative here, if we see the arguments or any of its alias being used in any
    // other opcode just don't do this optimization.
    if (HasArgumentsAccess(src1, argumentsSymId) || HasArgumentsAccess(src2, argumentsSymId))
    {
        return true;
    }

    if (dst)
    {
        // For dst no need to check for RegOpnd
        if (dst->IsSymOpnd() || dst->IsIndirOpnd())
        {
            if (IsArgumentsOpnd(dst, argumentsSymId))
            {
                return true;
            }
        }
    }

    return false;
}

bool
Inline::GetInlineeHasArgumentObject(Func * inlinee)
{
    if (!inlinee->GetHasArgumentObject())
    {
        // If inlinee has no arguments access return false
        return false;
    }

    // Inlinee has arguments access

    if (!inlinee->GetHasApplyTargetInlining())
    {
        // There is no apply target inlining (this.init.apply(this, arguments))
        // So arguments access continues to exist
        return true;
    }

    // Its possible there is no more arguments access after we inline apply target validate the same.
    // This sounds expensive, but we are only walking inlinee which has apply target inlining optimization enabled.
    // Also we walk only instruction in that inlinee and not nested inlinees. So it is not expensive.
    SymID argumentsSymId = 0;
    FOREACH_INSTR_IN_FUNC(instr, inlinee)
    {
        if (instr->m_func != inlinee)
        {
            // Skip nested inlinees
            continue;
        }

        if (instr->m_opcode == Js::OpCode::LdHeapArguments || instr->m_opcode == Js::OpCode::LdLetHeapArguments)
        {
            argumentsSymId = instr->GetDst()->AsRegOpnd()->m_sym->m_id;
        }
        else if (argumentsSymId != 0)
        {
            // Once we find the arguments object i.e. argumentsSymId is set
            // Make sure no one refers to it.
            switch (instr->m_opcode)
            {
                case Js::OpCode::InlineBuiltInStart:
                    {
                        IR::Opnd* builtInOpnd = instr->GetSrc1();
                        if (builtInOpnd->IsAddrOpnd())
                        {
                            Assert(builtInOpnd->AsAddrOpnd()->m_isFunction);
                            Js::BuiltinFunction builtinFunction = Js::JavascriptLibrary::GetBuiltInForFuncInfo(((Js::JavascriptFunction*)builtInOpnd->AsAddrOpnd()->m_address)->GetFunctionInfo(), inlinee->GetScriptContext());
                            if (builtinFunction == Js::BuiltinFunction::Function_Apply)
                            {
                                this->SetIsInInlinedApplyCall(true);
                            }
                        }
                        else if (builtInOpnd->IsRegOpnd())
                        {
                            if (builtInOpnd->AsRegOpnd()->m_sym->m_builtInIndex == Js::BuiltinFunction::Function_Apply)
                            {
                                this->SetIsInInlinedApplyCall(true);
                            }
                        }
                        break;
                    }

                case Js::OpCode::InlineBuiltInEnd:
                    {
                        if(this->GetIsInInlinedApplyCall())
                        {
                            this->SetIsInInlinedApplyCall(false);
                        }
                        break;
                    }

                case Js::OpCode::BailOnNotStackArgs:
                case Js::OpCode::LdArgumentsFromStack:
                case Js::OpCode::ArgOut_A_InlineBuiltIn:
                case Js::OpCode::BytecodeArgOutCapture:
                case Js::OpCode::BytecodeArgOutUse:
                    // These are part of arguments optimization and we are fine if they access stack args.
                    break;

                case Js::OpCode::ArgOut_A_FromStackArgs:
                    {
                        // If ArgOut_A_FromStackArgs is part of the call sequence for apply built-in inlining (as opposed to apply target inlining),
                        // then arguments access continues to exist.
                        if (this->GetIsInInlinedApplyCall() && HasArgumentsAccess(instr, argumentsSymId))
                        {
                            return true;
                        }
                        break;
                    }

                default:
                    {
                        if (HasArgumentsAccess(instr, argumentsSymId))
                        {
                            return true;
                        }
                    }
            }
        }
    }
    NEXT_INSTR_IN_FUNC;
    return false;
}

IR::Instr *
Inline::InlineSpread(IR::Instr *spreadCall)
{
    Assert(Lowerer::IsSpreadCall(spreadCall));

    if (spreadCall->m_func->GetJnFunction()->IsInlineSpreadDisabled()
        || this->topFunc->GetJnFunction()->IsInlineSpreadDisabled())
    {
        return spreadCall;
    }

    IR::Instr *spreadIndicesInstr = Lowerer::GetLdSpreadIndicesInstr(spreadCall);

    IR::Opnd *spreadIndicesOpnd = spreadIndicesInstr->GetSrc1();
    Js::AuxArray<uint32>* spreadIndices = static_cast<Js::AuxArray<uint32>*>(spreadIndicesOpnd->AsAddrOpnd()->m_address);

    Assert(spreadIndices->count > 0);
    IR::Instr *argInstr = spreadIndicesInstr;

    IR::SymOpnd *argLinkOpnd = argInstr->GetSrc2()->AsSymOpnd();
    StackSym *argLinkSym  = argLinkOpnd->m_sym->AsStackSym();
    argInstr = argLinkSym->m_instrDef;

    // We only support one spread argument for inlining.
    if (argLinkSym->GetArgSlotNum() > 2)
    {
        return spreadCall;
    }

    // We are now committed to inlining spread. Remove the LdSpreadIndices instr
    // and convert the spread and 'this' ArgOuts.
    spreadCall->ReplaceSrc2(argLinkOpnd);
    spreadIndicesInstr->Remove();

    // Insert the bailout before the array ArgOut
    IR::Opnd *arrayOpnd = argInstr->GetSrc1();
    argInstr->m_opcode = Js::OpCode::ArgOut_A_SpreadArg;
    IR::Instr *bailoutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNotSpreadable, IR::BailOutOnInlineFunction, argInstr, argInstr->m_func);
    bailoutInstr->SetSrc1(arrayOpnd);
    argInstr->InsertBefore(bailoutInstr);

    argLinkOpnd = argInstr->GetSrc2()->AsSymOpnd();
    argLinkSym  = argLinkOpnd->m_sym->AsStackSym();
    argInstr = argLinkSym->m_instrDef;
    argInstr->m_opcode = Js::OpCode::ArgOut_A_Dynamic;

    IR::RegOpnd *startCallDstOpnd = argInstr->GetSrc2()->AsRegOpnd();
    argLinkSym = startCallDstOpnd->m_sym->AsStackSym();
    argInstr = argLinkSym->m_instrDef;
    Assert(argInstr->m_opcode == Js::OpCode::StartCall);

    spreadCall->m_opcode = Js::OpCode::CallIDynamicSpread;

    return spreadCall;
}

void
Inline::TryResetObjTypeSpecFldInfoOn(IR::PropertySymOpnd* propertySymOpnd)
{
    // if an objTypeSpecFldInfo was created just for the purpose of polymorphic inlining but didn't get used for the same (for some reason or the other), and the polymorphic cache it was created from, wasn't equivalent,
    // we should null out this info on the propertySymOpnd so that assumptions downstream around equivalent object type spec still hold.
    if (propertySymOpnd)
    {
        propertySymOpnd->TryResetObjTypeSpecFldInfo();
    }
}

void
Inline::TryDisableRuntimePolymorphicCacheOn(IR::PropertySymOpnd* propertySymOpnd)
{
    if (propertySymOpnd)
    {
        propertySymOpnd->TryDisableRuntimePolymorphicCache();
    }
}

IR::PropertySymOpnd*
Inline::GetMethodLdOpndForCallInstr(IR::Instr* callInstr)
{
    IR::Opnd* methodOpnd = callInstr->GetSrc1();
    if (methodOpnd->IsRegOpnd())
    {
        if (methodOpnd->AsRegOpnd()->m_sym->IsStackSym())
        {
            if (methodOpnd->AsRegOpnd()->m_sym->AsStackSym()->IsSingleDef())
            {
                IR::Instr* defInstr = methodOpnd->AsRegOpnd()->m_sym->AsStackSym()->GetInstrDef();
                if (defInstr->GetSrc1() && defInstr->GetSrc1()->IsSymOpnd() && defInstr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
                {
                    return defInstr->GetSrc1()->AsSymOpnd()->AsPropertySymOpnd();
                }
                return nullptr;
            }
            return nullptr;
        }
        return nullptr;
    }
    return nullptr;
}
