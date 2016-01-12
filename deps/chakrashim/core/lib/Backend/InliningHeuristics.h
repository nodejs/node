//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

struct InliningThreshold
{
    Js::FunctionBody * topFunc;
    int inlineThreshold;
    int constructorInlineThreshold;
    int outsideLoopInlineThreshold;
    int leafInlineThreshold;
    int loopInlineThreshold;
    int polymorphicInlineThreshold;
    int inlineCountMax;
    int maxNumberOfInlineesWithLoop;
    int constantArgumentInlineThreshold;

    InliningThreshold(Js::FunctionBody * topFunc, bool aggressive = false);
    void SetHeuristics();
    void SetAggressiveHeuristics();
    void Reset();
};

class InliningHeuristics
{
    friend class InliningDecider;

    Js::FunctionBody * topFunc;
    InliningThreshold threshold;

public:

public:
    InliningHeuristics(Js::FunctionBody * topFunc) :topFunc(topFunc), threshold(topFunc) {};
    bool DeciderInlineIntoInliner(Js::FunctionBody* inlinee, Js::FunctionBody *inliner, bool isConstructorCall, bool isPolymorphicCall, InliningDecider* inliningDecider, uint16 constantArgInfo, uint recursiveInlineDepth, bool allowRecursiveInlining);
    bool CanRecursivelyInline(Js::FunctionBody* inlinee, Js::FunctionBody *inliner, bool allowRecursiveInlining, uint recursiveInlineDepth);
    bool BackendInlineIntoInliner(Js::FunctionBody* inlinee,
                                Js::FunctionBody *inliner,
                                Func *topFunc,
                                Js::ProfileId,
                                bool isConstructorCall,
                                bool isFixedMethodCall,
                                bool isCallOutsideLoopInTopFunc,
                                bool isCallInsideLoop,
                                uint recursiveInlineDepth,
                                uint16 constantArguments
                                );
    bool ContinueInliningUserDefinedFunctions(uint32 bytecodeInlinedCount) const;
private:
    static bool IsInlineeLeaf(Js::FunctionBody* inlinee)
    {
        return inlinee->HasDynamicProfileInfo()
            && (!PHASE_OFF(Js::InlineBuiltInCallerPhase, inlinee) ? !inlinee->HasNonBuiltInCallee() : inlinee->GetProfiledCallSiteCount() == 0)
            && !inlinee->GetAnyDynamicProfileInfo()->hasLdFldCallSiteInfo();
    }

};


