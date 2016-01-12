//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

InliningDecider::InliningDecider(Js::FunctionBody *const topFunc, bool isLoopBody, bool isInDebugMode, const ExecutionMode jitMode)
    : topFunc(topFunc), isLoopBody(isLoopBody), isInDebugMode(isInDebugMode), jitMode(jitMode), bytecodeInlinedCount(0), numberOfInlineesWithLoop (0), inliningHeuristics(topFunc)
{
    Assert(topFunc);
}

InliningDecider::~InliningDecider()
{
    INLINE_FLUSH();
}

bool InliningDecider::InlineIntoTopFunc() const
{
    if (this->jitMode == ExecutionMode::SimpleJit ||
        PHASE_OFF(Js::InlinePhase, this->topFunc) ||
        PHASE_OFF(Js::GlobOptPhase, this->topFunc))
    {
        return false;
    }

    if (this->topFunc->GetHasTry())
    {
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
        wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
        INLINE_TESTTRACE(L"INLINING: Skip Inline: Has try\tCaller: %s (%s)\n", this->topFunc->GetDisplayName(),
            this->topFunc->GetDebugNumberSet(debugStringBuffer));
        // Glob opt doesn't run on function with try, so we can't generate bailout for it
        return false;
    }

    return InlineIntoInliner(topFunc);
}

bool InliningDecider::InlineIntoInliner(Js::FunctionBody *const inliner) const
{
    Assert(inliner);
    Assert(this->jitMode == ExecutionMode::FullJit);

#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    if (PHASE_OFF(Js::InlinePhase, inliner) ||
        PHASE_OFF(Js::GlobOptPhase, inliner))
    {
        return false;
    }

    if (!inliner->HasDynamicProfileInfo())
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: No dynamic profile info\tCaller: %s (%s)\n", inliner->GetDisplayName(),
            inliner->GetDebugNumberSet(debugStringBuffer));
        return false;
    }

    if (inliner->GetProfiledCallSiteCount() == 0 && !inliner->GetAnyDynamicProfileInfo()->hasLdFldCallSiteInfo())
    {
        INLINE_TESTTRACE_VERBOSE(L"INLINING: Skip Inline: Leaf function\tCaller: %s (%s)\n", inliner->GetDisplayName(),
            inliner->GetDebugNumberSet(debugStringBuffer));
        // Nothing to do
        return false;
    }

    if (!inliner->GetAnyDynamicProfileInfo()->HasCallSiteInfo(inliner))
    {
        INLINE_TESTTRACE(L"INLINING: Skip Inline: No call site info\tCaller: %s (#%d)\n", inliner->GetDisplayName(),
            inliner->GetDebugNumberSet(debugStringBuffer));
        return false;
    }

    return true;
}

Js::FunctionInfo *InliningDecider::GetCallSiteFuncInfo(Js::FunctionBody *const inliner, const Js::ProfileId profiledCallSiteId, bool* isConstructorCall, bool* isPolymorphicCall)
{
    Assert(inliner);
    Assert(profiledCallSiteId < inliner->GetProfiledCallSiteCount());

    const auto profileData = inliner->GetAnyDynamicProfileInfo();
    Assert(profileData);

    return profileData->GetCallSiteInfo(inliner, profiledCallSiteId, isConstructorCall, isPolymorphicCall);
}

uint16 InliningDecider::GetConstantArgInfo(Js::FunctionBody *const inliner, const Js::ProfileId profiledCallSiteId)
{
    Assert(inliner);
    Assert(profiledCallSiteId < inliner->GetProfiledCallSiteCount());

    const auto profileData = inliner->GetAnyDynamicProfileInfo();
    Assert(profileData);

    return profileData->GetConstantArgInfo(profiledCallSiteId);
}


bool InliningDecider::HasCallSiteInfo(Js::FunctionBody *const inliner, const Js::ProfileId profiledCallSiteId)
{
    Assert(inliner);
    Assert(profiledCallSiteId < inliner->GetProfiledCallSiteCount());

    const auto profileData = inliner->GetAnyDynamicProfileInfo();
    Assert(profileData);

    return profileData->HasCallSiteInfo(inliner, profiledCallSiteId);
}


Js::FunctionInfo *InliningDecider::InlineCallSite(Js::FunctionBody *const inliner, const Js::ProfileId profiledCallSiteId, uint recursiveInlineDepth)
{
    bool isConstructorCall;
    bool isPolymorphicCall;
    Js::FunctionInfo *functionInfo = GetCallSiteFuncInfo(inliner, profiledCallSiteId, &isConstructorCall, &isPolymorphicCall);
    if (functionInfo)
    {
        return Inline(inliner, functionInfo, isConstructorCall, false, GetConstantArgInfo(inliner, profiledCallSiteId), profiledCallSiteId, recursiveInlineDepth, true);
    }
    return nullptr;
}

uint InliningDecider::InlinePolymorhicCallSite(Js::FunctionBody *const inliner, const Js::ProfileId profiledCallSiteId,
    Js::FunctionBody** functionBodyArray, uint functionBodyArrayLength, bool* canInlineArray, uint recursiveInlineDepth)
{
    Assert(inliner);
    Assert(profiledCallSiteId < inliner->GetProfiledCallSiteCount());
    Assert(functionBodyArray);

    const auto profileData = inliner->GetAnyDynamicProfileInfo();
    Assert(profileData);

    bool isConstructorCall;
    if (!profileData->GetPolymorphicCallSiteInfo(inliner, profiledCallSiteId, &isConstructorCall, functionBodyArray, functionBodyArrayLength))
    {
        return false;
    }

    uint inlineeCount = 0;
    uint actualInlineeCount  = 0;

    for (inlineeCount = 0; inlineeCount < functionBodyArrayLength; inlineeCount++)
    {
        if (!functionBodyArray[inlineeCount])
        {
            AssertMsg(inlineeCount >= 2, "There are at least two polymorphic call site");
            break;
        }
        if (Inline(inliner, functionBodyArray[inlineeCount], isConstructorCall, true /*isPolymorphicCall*/, 0, profiledCallSiteId, recursiveInlineDepth, false))
        {
            canInlineArray[inlineeCount] = true;
            actualInlineeCount++;
        }
    }
    if (inlineeCount != actualInlineeCount)
    {
        // We generate polymorphic dispatch and call even if there are no inlinees as it's seen to provide a perf boost
        // Skip loop bodies for now as we do not handle re-jit scenarios for the bailouts from them
        if (!PHASE_OFF(Js::PartialPolymorphicInlinePhase, inliner) && !this->isLoopBody)
        {
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
            wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
            INLINE_TESTTRACE(L"Partial inlining of polymorphic call: %s (%s)\tCaller: %s (%s)\n",
                functionBodyArray[inlineeCount - 1]->GetDisplayName(), functionBodyArray[inlineeCount - 1]->GetDebugNumberSet(debugStringBuffer),
                inliner->GetDisplayName(),
                inliner->GetDebugNumberSet(debugStringBuffer2));
        }
        else
        {
            return 0;
        }
    }
    return inlineeCount;
}

Js::FunctionInfo *InliningDecider::Inline(Js::FunctionBody *const inliner, Js::FunctionInfo* functionInfo,
    bool isConstructorCall, bool isPolymorphicCall, uint16 constantArgInfo, Js::ProfileId callSiteId, uint recursiveInlineDepth, bool allowRecursiveInlining)
{
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
    Js::FunctionProxy * proxy = functionInfo->GetFunctionProxy();
    if (proxy && proxy->IsFunctionBody())
    {
        if (isLoopBody && PHASE_OFF(Js::InlineInJitLoopBodyPhase, this->topFunc))
        {
            INLINE_TESTTRACE_VERBOSE(L"INLINING: Skip Inline: Jit loop body: %s (%s)\n", this->topFunc->GetDisplayName(),
                this->topFunc->GetDebugNumberSet(debugStringBuffer));
            return nullptr;
        }

        // Note: disable inline for debugger, as we can't bailout at return from function.
        // Alternative can be generate this bailout as part of inline, which can be done later as perf improvement.
        const auto inlinee = proxy->GetFunctionBody();
        Assert(this->jitMode == ExecutionMode::FullJit);
        if (PHASE_OFF(Js::InlinePhase, inlinee) ||
            PHASE_OFF(Js::GlobOptPhase, inlinee) ||
            !inliningHeuristics.ContinueInliningUserDefinedFunctions(this->bytecodeInlinedCount) ||
            this->isInDebugMode)
        {
            return nullptr;
        }

        if (functionInfo->IsDeferred() || inlinee->GetByteCode() == nullptr)
        {
            // DeferredParse...
            INLINE_TESTTRACE(L"INLINING: Skip Inline: No bytecode\tInlinee: %s (%s)\tCaller: %s (%s)\n",
                inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inliner->GetDisplayName(),
                inliner->GetDebugNumberSet(debugStringBuffer2));
            return nullptr;
        }

        if (inlinee->GetHasTry())
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Has try\tInlinee: %s (%s)\tCaller: %s (%s)\n",
                inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inliner->GetDisplayName(),
                inliner->GetDebugNumberSet(debugStringBuffer2));
            return nullptr;
        }

        // This is a hard limit as the argOuts array is statically sized.
        if (inlinee->GetInParamsCount() > Js::InlineeCallInfo::MaxInlineeArgoutCount)
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Params count greater then MaxInlineeArgoutCount\tInlinee: %s (%s)\tParamcount: %d\tMaxInlineeArgoutCount: %d\tCaller: %s (%s)\n",
                inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inlinee->GetInParamsCount(), Js::InlineeCallInfo::MaxInlineeArgoutCount,
                inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2));
            return nullptr;
        }

        if (inlinee->GetInParamsCount() == 0)
        {
            // Inline candidate has no params, not even a this pointer.  This can only be the global function,
            // which we shouldn't inline.
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Params count is zero!\tInlinee: %s (%s)\tParamcount: %d\tCaller: %s (%s)\n",
                inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inlinee->GetInParamsCount(),
                inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2));
            return nullptr;
        }

        if (inlinee->GetDontInline())
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Do not inline\tInlinee: %s (%s)\tCaller: %s (%s)\n",
                inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inliner->GetDisplayName(),
                inliner->GetDebugNumberSet(debugStringBuffer2));
            return nullptr;
        }

        // Do not inline a call to a class constructor if it isn't part of a new expression since the call will throw a TypeError anyway.
        if (inlinee->IsClassConstructor() && !isConstructorCall)
        {
            INLINE_TESTTRACE(L"INLINING: Skip Inline: Class constructor without new keyword\tInlinee: %s (%s)\tCaller: %s (%s)\n",
                inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inliner->GetDisplayName(),
                inliner->GetDebugNumberSet(debugStringBuffer2));
            return nullptr;
        }

        if (!inliningHeuristics.DeciderInlineIntoInliner(inlinee, inliner, isConstructorCall, isPolymorphicCall, this, constantArgInfo, recursiveInlineDepth, allowRecursiveInlining))
        {
            return nullptr;
        }

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
        TraceInlining(inliner, inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inlinee->GetByteCodeCount(), this->topFunc, this->bytecodeInlinedCount, inlinee, callSiteId);
#endif

        this->bytecodeInlinedCount += inlinee->GetByteCodeCount();
        return inlinee;
    }

    Js::OpCode builtInInlineCandidateOpCode;
    ValueType builtInReturnType;
    GetBuiltInInfo(functionInfo, &builtInInlineCandidateOpCode, &builtInReturnType, inliner->GetScriptContext());

    if(builtInInlineCandidateOpCode == 0 && builtInReturnType.IsUninitialized())
    {
        return nullptr;
    }

    Assert(this->jitMode == ExecutionMode::FullJit);
    if (builtInInlineCandidateOpCode != 0 &&
        (
            PHASE_OFF(Js::InlinePhase, inliner) ||
            PHASE_OFF(Js::GlobOptPhase, inliner) ||
            isConstructorCall
        ))
    {
        return nullptr;
    }

    // Note: for built-ins at this time we don't have enough data (the instr) to decide whether it's going to be inlined.
    return functionInfo;
}

bool InliningDecider::GetBuiltInInfo(
    Js::FunctionInfo *const funcInfo,
    Js::OpCode *const inlineCandidateOpCode,
    ValueType *const returnType,
    Js::ScriptContext *const scriptContext /* = nullptr*/
    )
{
    Assert(funcInfo);
    Assert(inlineCandidateOpCode);
    Assert(returnType);

    *inlineCandidateOpCode = (Js::OpCode)0;
    *returnType = ValueType::Uninitialized;

    if(funcInfo->HasBody())
    {
        return false;
    }

    // TODO: consider adding another column to JavascriptBuiltInFunctionList.h/LibraryFunction.h
    // and getting helper method from there instead of multiple switch labels. And for return value types too.
    switch (funcInfo->GetLocalFunctionId())
    {
    case Js::JavascriptBuiltInFunction::Math_Abs:
        *inlineCandidateOpCode = Js::OpCode::InlineMathAbs;
        break;

    case Js::JavascriptBuiltInFunction::Math_Acos:
        *inlineCandidateOpCode = Js::OpCode::InlineMathAcos;
        break;

    case Js::JavascriptBuiltInFunction::Math_Asin:
        *inlineCandidateOpCode = Js::OpCode::InlineMathAsin;
        break;

    case Js::JavascriptBuiltInFunction::Math_Atan:
        *inlineCandidateOpCode = Js::OpCode::InlineMathAtan;
        break;

    case Js::JavascriptBuiltInFunction::Math_Atan2:
        *inlineCandidateOpCode = Js::OpCode::InlineMathAtan2;
        break;

    case Js::JavascriptBuiltInFunction::Math_Cos:
        *inlineCandidateOpCode = Js::OpCode::InlineMathCos;
        break;

    case Js::JavascriptBuiltInFunction::Math_Exp:
        *inlineCandidateOpCode = Js::OpCode::InlineMathExp;
        break;

    case Js::JavascriptBuiltInFunction::Math_Log:
        *inlineCandidateOpCode = Js::OpCode::InlineMathLog;
        break;

    case Js::JavascriptBuiltInFunction::Math_Pow:
        *inlineCandidateOpCode = Js::OpCode::InlineMathPow;
        break;

    case Js::JavascriptBuiltInFunction::Math_Sin:
        *inlineCandidateOpCode = Js::OpCode::InlineMathSin;
        break;

    case Js::JavascriptBuiltInFunction::Math_Sqrt:
        *inlineCandidateOpCode = Js::OpCode::InlineMathSqrt;
        break;

    case Js::JavascriptBuiltInFunction::Math_Tan:
        *inlineCandidateOpCode = Js::OpCode::InlineMathTan;
        break;

    case Js::JavascriptBuiltInFunction::Math_Floor:
        *inlineCandidateOpCode = Js::OpCode::InlineMathFloor;
        break;

    case Js::JavascriptBuiltInFunction::Math_Ceil:
        *inlineCandidateOpCode = Js::OpCode::InlineMathCeil;
        break;

    case Js::JavascriptBuiltInFunction::Math_Round:
        *inlineCandidateOpCode = Js::OpCode::InlineMathRound;
        break;

    case Js::JavascriptBuiltInFunction::Math_Min:
        *inlineCandidateOpCode = Js::OpCode::InlineMathMin;
        break;

    case Js::JavascriptBuiltInFunction::Math_Max:
        *inlineCandidateOpCode = Js::OpCode::InlineMathMax;
        break;

    case Js::JavascriptBuiltInFunction::Math_Imul:
        *inlineCandidateOpCode = Js::OpCode::InlineMathImul;
        break;

    case Js::JavascriptBuiltInFunction::Math_Clz32:
        *inlineCandidateOpCode = Js::OpCode::InlineMathClz32;
        break;

    case Js::JavascriptBuiltInFunction::Math_Random:
        *inlineCandidateOpCode = Js::OpCode::InlineMathRandom;
        break;

    case Js::JavascriptBuiltInFunction::Math_Fround:
        *inlineCandidateOpCode = Js::OpCode::InlineMathFround;
        *returnType = ValueType::Float;
        break;

    case Js::JavascriptBuiltInFunction::JavascriptArray_Push:
        *inlineCandidateOpCode = Js::OpCode::InlineArrayPush;
        break;

    case Js::JavascriptBuiltInFunction::JavascriptArray_Pop:
        *inlineCandidateOpCode = Js::OpCode::InlineArrayPop;
        break;

    case Js::JavascriptBuiltInFunction::JavascriptArray_Concat:
    case Js::JavascriptBuiltInFunction::JavascriptArray_Reverse:
    case Js::JavascriptBuiltInFunction::JavascriptArray_Shift:
    case Js::JavascriptBuiltInFunction::JavascriptArray_Slice:
    case Js::JavascriptBuiltInFunction::JavascriptArray_Splice:

    case Js::JavascriptBuiltInFunction::JavascriptString_Link:
    case Js::JavascriptBuiltInFunction::JavascriptString_LocaleCompare:
        goto CallDirectCommon;

    case Js::JavascriptBuiltInFunction::JavascriptArray_Join:
    case Js::JavascriptBuiltInFunction::JavascriptString_CharAt:
    case Js::JavascriptBuiltInFunction::JavascriptString_Concat:
    case Js::JavascriptBuiltInFunction::JavascriptString_FromCharCode:
    case Js::JavascriptBuiltInFunction::JavascriptString_FromCodePoint:
    case Js::JavascriptBuiltInFunction::JavascriptString_Replace:
    case Js::JavascriptBuiltInFunction::JavascriptString_Slice:
    case Js::JavascriptBuiltInFunction::JavascriptString_Substr:
    case Js::JavascriptBuiltInFunction::JavascriptString_Substring:
    case Js::JavascriptBuiltInFunction::JavascriptString_ToLocaleLowerCase:
    case Js::JavascriptBuiltInFunction::JavascriptString_ToLocaleUpperCase:
    case Js::JavascriptBuiltInFunction::JavascriptString_ToLowerCase:
    case Js::JavascriptBuiltInFunction::JavascriptString_ToUpperCase:
    case Js::JavascriptBuiltInFunction::JavascriptString_Trim:
    case Js::JavascriptBuiltInFunction::JavascriptString_TrimLeft:
    case Js::JavascriptBuiltInFunction::JavascriptString_TrimRight:
        *returnType = ValueType::String;
        goto CallDirectCommon;

    case Js::JavascriptBuiltInFunction::JavascriptArray_Includes:
        *returnType = ValueType::Boolean;
        goto CallDirectCommon;

    case Js::JavascriptBuiltInFunction::JavascriptArray_IndexOf:
    case Js::JavascriptBuiltInFunction::JavascriptArray_LastIndexOf:
    case Js::JavascriptBuiltInFunction::JavascriptArray_Unshift:
    case Js::JavascriptBuiltInFunction::JavascriptString_CharCodeAt:
    case Js::JavascriptBuiltInFunction::JavascriptString_IndexOf:
    case Js::JavascriptBuiltInFunction::JavascriptString_LastIndexOf:
    case Js::JavascriptBuiltInFunction::JavascriptString_Search:
    case Js::JavascriptBuiltInFunction::GlobalObject_ParseInt:
        *returnType = ValueType::GetNumberAndLikelyInt(true);
        goto CallDirectCommon;

    case Js::JavascriptBuiltInFunction::JavascriptString_Split:
        *returnType = ValueType::GetObject(ObjectType::Array).SetHasNoMissingValues(true).SetArrayTypeId(Js::TypeIds_Array);
        goto CallDirectCommon;

    case Js::JavascriptBuiltInFunction::JavascriptString_Match:
    case Js::JavascriptBuiltInFunction::JavascriptRegExp_Exec:
        *returnType =
            ValueType::GetObject(ObjectType::Array).SetHasNoMissingValues(true).SetArrayTypeId(Js::TypeIds_Array)
                .Merge(ValueType::Null);
        goto CallDirectCommon;

    CallDirectCommon:
        *inlineCandidateOpCode = Js::OpCode::CallDirect;
        break;

    case Js::JavascriptBuiltInFunction::JavascriptFunction_Apply:
        *inlineCandidateOpCode = Js::OpCode::InlineFunctionApply;
        break;
    case Js::JavascriptBuiltInFunction::JavascriptFunction_Call:
        *inlineCandidateOpCode = Js::OpCode::InlineFunctionCall;
        break;

    // The following are not currently inlined, but are tracked for their return type
    // TODO: Add more built-ins that return objects. May consider tracking all built-ins.

    case Js::JavascriptBuiltInFunction::JavascriptArray_NewInstance:
        *returnType = ValueType::GetObject(ObjectType::Array).SetHasNoMissingValues(true).SetArrayTypeId(Js::TypeIds_Array);
        break;

    case Js::JavascriptBuiltInFunction::Int8Array_NewInstance:
#ifdef _M_X64
        *returnType = (!PHASE_OFF1(Js::TypedArrayVirtualPhase)) ? ValueType::GetObject(ObjectType::Int8MixedArray) : ValueType::GetObject(ObjectType::Int8Array);
#else
        *returnType = ValueType::GetObject(ObjectType::Int8Array);
#endif
        break;

    case Js::JavascriptBuiltInFunction::Uint8Array_NewInstance:
#ifdef _M_X64
        *returnType = (!PHASE_OFF1(Js::TypedArrayVirtualPhase)) ? ValueType::GetObject(ObjectType::Uint8MixedArray) : ValueType::GetObject(ObjectType::Uint8Array);
#else
        *returnType = ValueType::GetObject(ObjectType::Uint8Array);
#endif
        break;

    case Js::JavascriptBuiltInFunction::Uint8ClampedArray_NewInstance:
#ifdef _M_X64
        *returnType = (!PHASE_OFF1(Js::TypedArrayVirtualPhase)) ? ValueType::GetObject(ObjectType::Uint8ClampedMixedArray) : ValueType::GetObject(ObjectType::Uint8ClampedArray);
#else
        *returnType = ValueType::GetObject(ObjectType::Uint8ClampedArray);
#endif
        break;

    case Js::JavascriptBuiltInFunction::Int16Array_NewInstance:
#ifdef _M_X64
        *returnType = (!PHASE_OFF1(Js::TypedArrayVirtualPhase)) ? ValueType::GetObject(ObjectType::Int16MixedArray) : ValueType::GetObject(ObjectType::Int16Array);
#else
        *returnType = ValueType::GetObject(ObjectType::Int16Array);
#endif
        break;

    case Js::JavascriptBuiltInFunction::Uint16Array_NewInstance:
#ifdef _M_X64
        *returnType = (!PHASE_OFF1(Js::TypedArrayVirtualPhase)) ? ValueType::GetObject(ObjectType::Uint16MixedArray) : ValueType::GetObject(ObjectType::Uint16Array);
#else
        *returnType = ValueType::GetObject(ObjectType::Uint16Array);
#endif
        break;

    case Js::JavascriptBuiltInFunction::Int32Array_NewInstance:
#ifdef _M_X64
        *returnType = (!PHASE_OFF1(Js::TypedArrayVirtualPhase)) ? ValueType::GetObject(ObjectType::Int32MixedArray) : ValueType::GetObject(ObjectType::Int32Array);
#else
        *returnType = ValueType::GetObject(ObjectType::Int32Array);
#endif
        break;

    case Js::JavascriptBuiltInFunction::Uint32Array_NewInstance:
#ifdef _M_X64
        *returnType = (!PHASE_OFF1(Js::TypedArrayVirtualPhase)) ? ValueType::GetObject(ObjectType::Uint32MixedArray) : ValueType::GetObject(ObjectType::Uint32Array);
#else
        *returnType = ValueType::GetObject(ObjectType::Uint32Array);
#endif
        break;

    case Js::JavascriptBuiltInFunction::Float32Array_NewInstance:
#ifdef _M_X64
        *returnType = (!PHASE_OFF1(Js::TypedArrayVirtualPhase)) ? ValueType::GetObject(ObjectType::Float32MixedArray) : ValueType::GetObject(ObjectType::Float32Array);
#else
        *returnType = ValueType::GetObject(ObjectType::Float32Array);
#endif
        break;

    case Js::JavascriptBuiltInFunction::Float64Array_NewInstance:
#ifdef _M_X64
        *returnType = (!PHASE_OFF1(Js::TypedArrayVirtualPhase)) ? ValueType::GetObject(ObjectType::Float64MixedArray) : ValueType::GetObject(ObjectType::Float64Array);
#else
        *returnType = ValueType::GetObject(ObjectType::Float64Array);
#endif
        break;

    case Js::JavascriptBuiltInFunction::Int64Array_NewInstance:
        *returnType = ValueType::GetObject(ObjectType::Int64Array);
        break;

    case Js::JavascriptBuiltInFunction::Uint64Array_NewInstance:
        *returnType = ValueType::GetObject(ObjectType::Uint64Array);
        break;

    case Js::JavascriptBuiltInFunction::BoolArray_NewInstance:
        *returnType = ValueType::GetObject(ObjectType::BoolArray);
        break;

    case Js::JavascriptBuiltInFunction::CharArray_NewInstance:
        *returnType = ValueType::GetObject(ObjectType::CharArray);
        break;

#ifdef ENABLE_DOM_FAST_PATH
    case Js::JavascriptBuiltInFunction::DOMFastPathGetter:
        *inlineCandidateOpCode = Js::OpCode::DOMFastPathGetter;
        break;
#endif

    // SIMD_JS
    // we only inline, and hence type-spec on IA
#if defined(_M_X64) || defined(_M_IX86)
    default:
    {
        // inline only if simdjs and simd128 type-spec is enabled.
        if (scriptContext->GetConfig()->IsSimdjsEnabled() && SIMD128_TYPE_SPEC_FLAG)
        {
            *inlineCandidateOpCode = scriptContext->GetThreadContext()->GetSimdOpcodeFromFuncInfo(funcInfo);
        }
        else
        {
            return false;
        }
    }
#endif
    }
    return true;
}

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
// static
void InliningDecider::TraceInlining(Js::FunctionBody *const inliner, const wchar_t* inlineeName, const wchar_t* inlineeFunctionIdandNumberString, uint inlineeByteCodeCount,
    Js::FunctionBody* topFunc, uint inlinedByteCodeCount, Js::FunctionBody *const inlinee, uint callSiteId, uint builtIn)
{
    wchar_t debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    wchar_t debugStringBuffer3[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    if (inlineeName == nullptr)
    {

        int len = swprintf_s(debugStringBuffer3, MAX_FUNCTION_BODY_DEBUG_STRING_SIZE, L"built In Id: %u", builtIn);
        Assert(len > 14);
        inlineeName = debugStringBuffer3;
    }
    INLINE_TESTTRACE(L"INLINING: Inlinee: %s (%s)\tSize: %d\tCaller: %s (%s)\tSize: %d\tInlineCount: %d\tRoot: %s (%s)\tSize: %d\tCallSiteId: %d\n",
        inlineeName, inlineeFunctionIdandNumberString, inlineeByteCodeCount,
        inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer), inliner->GetByteCodeCount(), inlinedByteCodeCount,
        topFunc->GetDisplayName(),
        topFunc->GetDebugNumberSet(debugStringBuffer2), topFunc->GetByteCodeCount(),
        callSiteId
        );

    INLINE_TRACE(L"INLINING:\n\tInlinee: size: %4d  %s\n\tCaller: size: %4d  %-25s (%s)  InlineCount: %d\tRoot:  size: %4d  %s  (%s) CallSiteId %d\n",
        inlineeByteCodeCount, inlineeName,
        inliner->GetByteCodeCount(), inliner->GetDisplayName(),
        inliner->GetDebugNumberSet(debugStringBuffer), inlinedByteCodeCount,
        topFunc->GetByteCodeCount(),
        topFunc->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer2),
        callSiteId
        );

    // Now Trace inlining across files cases

    if (builtIn != -1)  // built-in functions
    {
        return;
    }

    Assert(inliner && inlinee);

    if (inliner->GetSourceContextId() != inlinee->GetSourceContextId())
    {
        INLINE_TESTTRACE(L"INLINING_ACROSS_FILES: Inlinee: %s (%s)\tSize: %d\tCaller: %s (%s)\tSize: %d\tInlineCount: %d\tRoot: %s (%s)\tSize: %d\n",
            inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inlinee->GetByteCodeCount(),
            inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2), inliner->GetByteCodeCount(), inlinedByteCodeCount,
            topFunc->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer3), topFunc->GetByteCodeCount()
            );

        INLINE_TRACE(L"INLINING_ACROSS_FILES: Inlinee: %s (%s)\tSize: %d\tCaller: %s (%s)\tSize: %d\tInlineCount: %d\tRoot: %s (%s)\tSize: %d\n",
            inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inlinee->GetByteCodeCount(),
            inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2), inliner->GetByteCodeCount(), inlinedByteCodeCount,
            topFunc->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer3), topFunc->GetByteCodeCount()
            );
    }

}
#endif
