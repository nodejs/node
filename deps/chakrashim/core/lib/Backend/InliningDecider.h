//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class InliningDecider
{
private:
    Js::FunctionBody *const topFunc;
    InliningHeuristics inliningHeuristics;
    bool isLoopBody;     // We don't support inlining on jit loop bodies as of now.
    bool isInDebugMode;

    // These variables capture the temporary state
    uint32 bytecodeInlinedCount;
    uint32 numberOfInlineesWithLoop;

public:
    const ExecutionMode jitMode;      // Disable certain parts for certain JIT modes

public:
    InliningDecider(Js::FunctionBody *const topFunc, bool isLoopBody, bool isInDebugMode, const ExecutionMode jitMode);
    ~InliningDecider();

public:
    bool InlineIntoTopFunc() const;
    bool InlineIntoInliner(Js::FunctionBody *const inliner) const;

    Js::FunctionInfo *Inline(Js::FunctionBody *const inliner, Js::FunctionInfo* functionInfo, bool isConstructorCall, bool isPolymorphicCall, uint16 constantArgInfo, Js::ProfileId callSiteId, uint recursiveInlineDepth, bool allowRecursiveInline);
    Js::FunctionInfo *InlineCallSite(Js::FunctionBody *const inliner, const Js::ProfileId profiledCallSiteId, uint recursiveInlineDepth = 0);
    Js::FunctionInfo *GetCallSiteFuncInfo(Js::FunctionBody *const inliner, const Js::ProfileId profiledCallSiteId, bool* isConstructorCall, bool* isPolymorphicCall);
    uint16 GetConstantArgInfo(Js::FunctionBody *const inliner, const Js::ProfileId profiledCallSiteId);
    bool HasCallSiteInfo(Js::FunctionBody *const inliner, const Js::ProfileId profiledCallSiteId);
    uint InlinePolymorhicCallSite(Js::FunctionBody *const inliner, const Js::ProfileId profiledCallSiteId, Js::FunctionBody** functionBodyArray, uint functionBodyArrayLength, bool* canInlineArray, uint recursiveInlineDepth = 0);
    bool GetIsLoopBody() { return isLoopBody;};

    void SetAggressiveHeuristics() { inliningHeuristics.threshold.SetAggressiveHeuristics(); }
    void ResetInlineHeuristics() { inliningHeuristics.threshold.Reset(); }
    void SetLimitOnInlineesWithLoop(uint countOfInlineesWithLoops)
    {
        // If we have determined in TryAggressiveInlining phase there are too many inlinees with loop, just set the limit such that we don't inline them.
        if ((uint)inliningHeuristics.threshold.maxNumberOfInlineesWithLoop <= countOfInlineesWithLoops)
        {
            inliningHeuristics.threshold.maxNumberOfInlineesWithLoop = 0;
        }
        return;
    }
    void ResetState()
    {
        bytecodeInlinedCount = 0;
        numberOfInlineesWithLoop = 0;
    }
    uint32 getNumberOfInlineesWithLoop() { return numberOfInlineesWithLoop; }
    void incrementNumberOfInlineesWithLoop() { numberOfInlineesWithLoop++; }


    static bool GetBuiltInInfo(
        Js::FunctionInfo *const funcInfo,
        Js::OpCode *const inlineCandidateOpCode,
        ValueType *const returnType,
        Js::ScriptContext *const scriptContext = nullptr);

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    static void TraceInlining(Js::FunctionBody *const inliner, const wchar_t* inlineeName, const wchar_t* inlineeFunctionIdandNumberString, uint inlineeByteCodeCount,
        Js::FunctionBody* topFunc, uint inlinedByteCodeCount, Js::FunctionBody *const inlinee, uint callSiteId, uint builtIn = -1);
#endif

    PREVENT_COPY(InliningDecider)
};

#if ENABLE_DEBUG_CONFIG_OPTIONS
#define INLINE_VERBOSE_TRACE(...) \
    if (Js::Configuration::Global.flags.Verbose && Js::Configuration::Global.flags.Trace.IsEnabled(Js::InlinePhase, this->topFunc->GetSourceContextId(), this->topFunc->GetLocalFunctionId())) \
    { \
    Output::Print(__VA_ARGS__); \
    }
#define INLINE_TRACE(...) \
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::InlinePhase, topFunc->GetSourceContextId(), topFunc->GetLocalFunctionId())) \
    { \
    Output::Print(__VA_ARGS__); \
    }
#define INLINE_TESTTRACE(...) \
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::InlinePhase, topFunc->GetSourceContextId(), topFunc->GetLocalFunctionId())) \
    { \
    Output::Print(__VA_ARGS__); \
    Output::Flush(); \
    }
#define INLINE_TESTTRACE_VERBOSE(...) \
    if (Js::Configuration::Global.flags.Verbose && Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::InlinePhase, topFunc->GetSourceContextId(), topFunc->GetLocalFunctionId())) \
    { \
    Output::Print(__VA_ARGS__); \
    Output::Flush(); \
    }

#define POLYMORPHIC_INLINE_TESTTRACE(...) \
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::PolymorphicInlinePhase)) \
    { \
    Output::Print(__VA_ARGS__); \
    Output::Flush(); \
    }
#define POLYMORPHIC_INLINE_FIXEDMETHODS_TESTTRACE(...) \
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::PolymorphicInlineFixedMethodsPhase)) \
    { \
    Output::Print(__VA_ARGS__); \
    Output::Flush(); \
    }
#define INLINE_FLUSH() \
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::InlinePhase,this->topFunc->GetSourceContextId() ,this->topFunc->GetLocalFunctionId()) || Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::InlinePhase)) \
    { \
    Output::Flush(); \
    }
#else
#define INLINE_VERBOSE_TRACE(...)
#define POLYMORPHIC_INLINE_TESTTRACE(...)
#define POLYMORPHIC_INLINE_FIXEDMETHODS_TESTTRACE(...)
#define INLINE_TRACE(...)
#define INLINE_FLUSH()
#define INLINE_TESTTRACE(...)
#define INLINE_TESTTRACE_VERBOSE(...)
#endif
