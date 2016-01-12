//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

InliningThreshold::InliningThreshold(Js::FunctionBody *topFunc, bool aggressive) : topFunc(topFunc)
{
    if (aggressive)
    {
        SetAggressiveHeuristics();
    }
    else
    {
        SetHeuristics();
    }
}
void InliningThreshold::SetAggressiveHeuristics()
{
    int limit = CONFIG_FLAG(AggressiveInlineThreshold);

    inlineThreshold = limit;
    constructorInlineThreshold = limit;
    outsideLoopInlineThreshold = limit;
    leafInlineThreshold = limit;
    loopInlineThreshold = limit;
    polymorphicInlineThreshold = limit;
    maxNumberOfInlineesWithLoop = CONFIG_FLAG(MaxNumberOfInlineesWithLoop);

    inlineCountMax = CONFIG_FLAG(AggressiveInlineCountMax);
}

void InliningThreshold::Reset()
{
    SetHeuristics();
}

void InliningThreshold::SetHeuristics()
{
    inlineThreshold = CONFIG_FLAG(InlineThreshold);
    // Inline less aggressively in large functions since the register pressure is likely high.
    // Small functions shouldn't be a problem.
    if (topFunc->GetByteCodeWithoutLDACount() > 800)
    {
        inlineThreshold -= CONFIG_FLAG(InlineThresholdAdjustCountInLargeFunction);
    }
    else if (topFunc->GetByteCodeWithoutLDACount() > 200)
    {
        inlineThreshold -= CONFIG_FLAG(InlineThresholdAdjustCountInMediumSizedFunction);
    }
    else if (topFunc->GetByteCodeWithoutLDACount() < 50)
    {
        inlineThreshold += CONFIG_FLAG(InlineThresholdAdjustCountInSmallFunction);
    }

    constructorInlineThreshold = CONFIG_FLAG(ConstructorInlineThreshold);
    outsideLoopInlineThreshold = CONFIG_FLAG(OutsideLoopInlineThreshold);
    leafInlineThreshold = CONFIG_FLAG(LeafInlineThreshold);
    loopInlineThreshold = CONFIG_FLAG(LoopInlineThreshold);
    polymorphicInlineThreshold = CONFIG_FLAG(PolymorphicInlineThreshold);
    maxNumberOfInlineesWithLoop = CONFIG_FLAG(MaxNumberOfInlineesWithLoop);
    constantArgumentInlineThreshold = CONFIG_FLAG(ConstantArgumentInlineThreshold);
    inlineCountMax = CONFIG_FLAG(InlineCountMax);
}

bool InliningHeuristics::CanRecursivelyInline(Js::FunctionBody* inlinee, Js::FunctionBody *inliner, bool allowRecursiveInlining, uint recursiveInlineDepth)
{
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif


    if (!PHASE_OFF(Js::InlineRecursivePhase, inliner)
        && allowRecursiveInlining
        &&  inlinee == inliner
        &&  inlinee->CanInlineRecursively(recursiveInlineDepth) )
    {
        INLINE_TESTTRACE(L"INLINING: Inlined recursively\tInlinee: %s (%s)\tCaller: %s (%s)\tDepth: %d\n",
            inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inliner->GetDisplayName(),
            inliner->GetDebugNumberSet(debugStringBuffer2), recursiveInlineDepth);
        return true;
    }

    if (!inlinee->CanInlineAgain())
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: Do not inline recursive functions\tInlinee: %s (%s)\tCaller: %s (%s)\n",
            inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inliner->GetDisplayName(),
            inliner->GetDebugNumberSet(debugStringBuffer2));
        return false;
    }

    return true;
}

// This function is called from Inlining decider, this only enables collection of the inlinee data, we are much more aggressive here.
// Actual decision of whether something is inlined or not is taken in CommitInlineIntoInliner
bool InliningHeuristics::DeciderInlineIntoInliner(Js::FunctionBody* inlinee, Js::FunctionBody *inliner, bool isConstructorCall, bool isPolymorphicCall, InliningDecider* inliningDecider, uint16 constantArgInfo, uint recursiveInlineDepth, bool allowRecursiveInlining)
{

    if (!CanRecursivelyInline(inlinee, inliner, allowRecursiveInlining, recursiveInlineDepth))
    {
        return false;
    }

    if (PHASE_FORCE(Js::InlinePhase, this->topFunc) ||
        PHASE_FORCE(Js::InlinePhase, inliner) ||
        PHASE_FORCE(Js::InlinePhase, inlinee))
    {
        return true;
    }

    if (PHASE_OFF(Js::InlinePhase, this->topFunc) ||
        PHASE_OFF(Js::InlinePhase, inliner) ||
        PHASE_OFF(Js::InlinePhase, inlinee))
    {
        return false;
    }

    if (PHASE_FORCE(Js::InlineTreePhase, this->topFunc) ||
        PHASE_FORCE(Js::InlineTreePhase, inliner))
    {
        return true;
    }

    if (PHASE_FORCE(Js::InlineAtEveryCallerPhase, inlinee))
    {
        return true;
    }

    if (inlinee->GetIsAsmjsMode() || inliner->GetIsAsmjsMode())
    {
        return false;
    }

    uint inlineeByteCodeCount = inlinee->GetByteCodeWithoutLDACount();

    // Heuristics are hit in the following order (Note *order* is important)
    // 1. Leaf function:  If the inlinee is a leaf (but not a constructor or a polymorphic call) inline threshold is LeafInlineThreshold (60). Also it can have max 1 loop
    // 2. Constant Function Argument: If the inlinee candidate has a constant argument and that argument is used for branching, then the inline threshold is ConstantArgumentInlineThreshold (157)
    // 3. InlineThreshold: If an inlinee candidate exceeds InlineThreshold just don't inline no matter what.

    // Following are additional constraint for an inlinee which meets InlineThreshold (Rule no 3)
    // 4. Rule for inlinee with loops:
    //      4a. Only single loop in inlinee is permitted.
    //      4b. Should not have polymorphic field access.
    //      4c. Should not be a constructor.
    //      4d. Should meet LoopInlineThreshold (25)
    // 5. Rule for polymorphic inlinee:
    //      4a. Should meet PolymorphicInlineThreshold (32)
    // 6. Rule for constructors:
    //       5a. Always inline if inlinee has polymorphic field access (as we have cloned runtime data).
    //       5b. If inlinee is monomorphic, inline only small constructors. They are governed by ConstructorInlineThreshold (21)
    // 7. Rule for inlinee which is not interpreted enough (as we might not have all the profile data):
    //       7a. As of now it is still governed by the InlineThreshold. Plan to play with this in future.
    // 8. Rest should be inlined.

    if (!isPolymorphicCall && !isConstructorCall && IsInlineeLeaf(inlinee) && (inlinee->GetLoopCount() <= 2))
    {
        // Inlinee is a leaf function
        if (inliningDecider->getNumberOfInlineesWithLoop() <= (uint)threshold.maxNumberOfInlineesWithLoop) // Don't inlinee too many inlinees with loops.
        {
            // Negative LeafInlineThreshold disable the threshold
            if (threshold.leafInlineThreshold >= 0 && inlineeByteCodeCount < (uint)threshold.leafInlineThreshold)
            {
                if (inlinee->GetLoopCount())
                {
                    inliningDecider->incrementNumberOfInlineesWithLoop();
                }
                return true;
            }
        }

    }


    uint16 mask = constantArgInfo &  inlinee->m_argUsedForBranch;
    if (mask && inlineeByteCodeCount <  (uint)CONFIG_FLAG(ConstantArgumentInlineThreshold))
    {
        return true;
    }

#if ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer3[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    if (inlineeByteCodeCount > (uint)this->threshold.inlineThreshold)         // Don't inline function too big to inline
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: Bytecode greater than threshold\tInlinee: %s (%s)\tByteCodeCount: %d\tByteCodeInlinedThreshold: %d\tCaller: %s (%s) \tRoot: %s (%s)\n",
            inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inlinee->GetByteCodeCount(), this->threshold.inlineThreshold,
            inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2),
            topFunc->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer3));
        return false;
    }

    if (inlinee->GetHasLoops())
    {
        // Small function with a single loop, go ahead and inline that.
        if (threshold.loopInlineThreshold < 0 ||                                     // Negative LoopInlineThreshold disable inlining with loop
            inlineeByteCodeCount >(uint)threshold.loopInlineThreshold ||
            inliningDecider->getNumberOfInlineesWithLoop()  > (uint)threshold.maxNumberOfInlineesWithLoop || // See if we are inlining too many inlinees with loops.
            (inlinee->GetLoopCount() > 2) ||                                         // Allow at most 2 loops.
            inlinee->GetHasNestedLoop() ||                                           // Nested loops are not a good inlinee candidate
            isConstructorCall ||                                                        // If the function is constructor or has polymorphic fields don't inline.
            PHASE_OFF(Js::InlineFunctionsWithLoopsPhase,this->topFunc))
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Has loops \tBytecode size: %d \tgetNumberOfInlineesWithLoop: %d\tloopCount: %d\thasNestedLoop: %B\tisConstructorCall:%B\tInlinee: %s (%s)\tCaller: %s (%s) \tRoot: %s (%s)\n",
                inlinee->GetByteCodeCount(),
                inliningDecider->getNumberOfInlineesWithLoop(),
                inlinee->GetLoopCount(),
                inlinee->GetHasNestedLoop(),
                isConstructorCall,
                inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer),
                inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2),
                topFunc->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer3));
            // Don't inline function with loops
            return false;
        }
        inliningDecider->incrementNumberOfInlineesWithLoop();
        return true;
    }

    if (isPolymorphicCall)
    {
        if (threshold.polymorphicInlineThreshold < 0 ||                              // Negative PolymorphicInlineThreshold disable inlining
            inlineeByteCodeCount > (uint)threshold.polymorphicInlineThreshold ||     // This is second level check to ensure we don't inline huge polymorphic functions.
            isConstructorCall)
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Polymorphic call under PolymorphicInlineThreshold: %d \tBytecode size: %d\tInlinee: %s (%s)\tCaller: %s (%s) \tRoot: %s (%s)\n",
                threshold.polymorphicInlineThreshold,
                inlinee->GetByteCodeCount(),
                inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer),
                inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2),
                topFunc->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer3));
            return false;
        }
        return true;
    }

    if(isConstructorCall)
    {
#pragma prefast(suppress: 6285, "logical-or of constants is by design")
        if(PHASE_OFF(Js::InlineConstructorsPhase, this->topFunc) ||
            PHASE_OFF(Js::InlineConstructorsPhase, inliner) ||
            PHASE_OFF(Js::InlineConstructorsPhase, inlinee) ||
            !CONFIG_FLAG(CloneInlinedPolymorphicCaches))
        {
            return false;
        }

        if(PHASE_FORCE(Js::InlineConstructorsPhase, this->topFunc) ||
           PHASE_FORCE(Js::InlineConstructorsPhase, inliner) ||
           PHASE_FORCE(Js::InlineConstructorsPhase, inlinee))
        {
            return true;
        }

        if (inlinee->HasDynamicProfileInfo() && inlinee->GetAnyDynamicProfileInfo()->HasPolymorphicFldAccess())
        {
            // As of now this is dependent on bytecodeInlinedThreshold.
            return true;
        }

        // Negative ConstructorInlineThreshold always disable constructor inlining
        if (threshold.constructorInlineThreshold < 0 || inlineeByteCodeCount >(uint)threshold.constructorInlineThreshold)
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Constructor with no polymorphic field access \tBytecode size: %d\tInlinee: %s (%s)\tCaller: %s (%s) \tRoot: %s (%s)\n",
                inlinee->GetByteCodeCount(),
                inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer),
                inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2),
                topFunc->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer3));
            // Don't inline constructor that does not have a polymorphic field access, or if cloning polymorphic inline
            // caches is disabled
            return false;
        }
        return true;
    }

    return true;
}

// Called from background thread to commit inlining.
bool InliningHeuristics::BackendInlineIntoInliner(Js::FunctionBody* inlinee,
                                Js::FunctionBody *inliner,
                                Func *topFunction,
                                Js::ProfileId callSiteId,
                                bool isConstructorCall,
                                bool isFixedMethodCall,                     // Reserved
                                bool isCallOutsideLoopInTopFunc,            // There is a loop for sure and this call is outside loop
                                bool isCallInsideLoop,
                                uint recursiveInlineDepth,
                                uint16 constantArguments
                                )
{
    // We have one piece of additional data in backend, whether  we are outside loop or inside
    // This function decides to inline or not based on that additional data. Most of the filtering is already done by DeciderInlineIntoInliner which is called
    // during work item creation.
    // This is additional filtering during actual inlining phase.

    // Note *order* is important
    // Following are
    // 1. Constructor is always inlined (irrespective of inside or outside)
    // 2. If the inlinee candidate has constant argument and that argument is used for a branch and the inlinee size is within ConstantArgumentInlineThreshold(157) we inline
    // 3. Inside loops:
    //     3a. Leaf function will always get inlined (irrespective of leaf has loop or not)
    //     3b. If the inlinee has loops, don't inline it (Basically avoiding inlining a loop within another loop unless its leaf).
    // 4. Outside loop (inliner has loops):
    //     4a. Only inline small inlinees. Governed by OutsideLoopInlineThreshold (16)
    // 5. Rest are inlined.
#if ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer3[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    bool doBackEndAggressiveInline = (constantArguments & inlinee->m_argUsedForBranch) != 0;

    if (!PHASE_OFF(Js::InlineRecursivePhase, inliner)
        && inlinee == inliner
        && (!inlinee->CanInlineRecursively(recursiveInlineDepth, doBackEndAggressiveInline)))
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline (backend): Recursive inlining\tInlinee: %s (#%s)\tCaller: %s (#%s) \tRoot: %s (#%s) Depth: %d\n",
            inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer),
            inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2),
            topFunc->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer3),
            recursiveInlineDepth);
        return false;
    }

    if(PHASE_FORCE(Js::InlinePhase, this->topFunc) ||
        PHASE_FORCE(Js::InlinePhase, inliner) ||
        PHASE_FORCE(Js::InlinePhase, inlinee))
    {
        return true;
    }

    if (PHASE_FORCE(Js::InlineTreePhase, this->topFunc) ||
        PHASE_FORCE(Js::InlineTreePhase, inliner))
    {
        return true;
    }

    if (PHASE_FORCE(Js::InlineAtEveryCallerPhase, inlinee))
    {
        return true;
    }

    Js::DynamicProfileInfo *dynamicProfile = inliner->GetAnyDynamicProfileInfo();

    bool doConstantArgumentInlining = (dynamicProfile->GetConstantArgInfo(callSiteId) & inlinee->m_argUsedForBranch) != 0;
    if (doConstantArgumentInlining && inlinee->GetByteCodeWithoutLDACount() <  (uint)threshold.constantArgumentInlineThreshold)
    {
        return true;
    }


    if (topFunction->m_workItem->RecyclableData()->JitTimeData()->GetIsAggressiveInliningEnabled())
    {
        return true;
    }

    if (isConstructorCall)
    {
        return true;
    }


    if (isCallInsideLoop && IsInlineeLeaf(inlinee))
    {
        return true;
    }

    if (isCallInsideLoop && inlinee->GetHasLoops() )                            // Don't inline function with loops inside another loop unless it is a leaf
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline (backend): Recursive loop inlining\tInlinee: %s (#%s)\tCaller: %s (#%s) \tRoot: %s (#%s)\n",
            inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer),
            inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2),
            topFunc->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer3));
        return false;
    }
    byte scale = 1;

    if (doBackEndAggressiveInline)
    {
        scale = 2;
    }

    if (isCallOutsideLoopInTopFunc &&
        (threshold.outsideLoopInlineThreshold < 0 ||
        inlinee->GetByteCodeWithoutLDACount() > (uint)threshold.outsideLoopInlineThreshold * scale))
    {
        Assert(!isCallInsideLoop);
        INLINE_TESTTRACE(L"INLINING: Skip Inline (backend): Inlining outside loop doesn't meet OutsideLoopInlineThreshold: %d \tBytecode size: %d\tInlinee: %s (#%s)\tCaller: %s (#%s) \tRoot: %s (#%s)\n",
            threshold.outsideLoopInlineThreshold,
            inlinee->GetByteCodeCount(),
            inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer),
            inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2),
            topFunc->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer3));
        return false;
    }
    return true;
}

bool InliningHeuristics::ContinueInliningUserDefinedFunctions(uint32 bytecodeInlinedCount) const
{
#if ENABLE_DEBUG_CONFIG_OPTIONS
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
    if (PHASE_FORCE(Js::InlinePhase, this->topFunc) || bytecodeInlinedCount <= (uint)threshold.inlineCountMax)
    {
        return true;
    }

    INLINE_TESTTRACE(L"INLINING: Skip Inline: InlineCountMax threshold %d, reached: %s (#%s)\n",
        (uint)threshold.inlineCountMax,
        this->topFunc->GetDisplayName(), this->topFunc->GetDebugNumberSet(debugStringBuffer));

    return false;
}
