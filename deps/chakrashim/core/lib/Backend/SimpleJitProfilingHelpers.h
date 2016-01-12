//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    namespace SimpleJitHelpers
    {
        void ProfileParameters(void* framePtr);

        void CleanImplicitCallFlags(FunctionBody* body);

        void ProfileCall_DefaultInlineCacheIndex(void* framePtr, ProfileId profileId, Var retval, JavascriptFunction* callee, CallInfo info);
        void ProfileCall(void* framePtr, ProfileId profileId, InlineCacheIndex inlineCacheIndex, Var retval, Var callee, CallInfo info);
        void ProfileReturnTypeCall(void* framePtr, ProfileId profileId, Var retval, JavascriptFunction*callee, CallInfo info);

        Var ProfiledLdLen_A(FunctionBody* body, DynamicObject * instance, ProfileId profileId);
        Var ProfiledStrictLdThis(Var thisVar, FunctionBody* functionBody);
        Var ProfiledLdThis(Var thisVar, int moduleID, FunctionBody* functionBody);
        Var ProfiledSwitch(FunctionBody* functionBody,ProfileId profileId, Var exp);
        Var ProfiledDivide(FunctionBody* functionBody, ProfileId profileId, Var aLeft, Var aRight);
        Var ProfiledRemainder(FunctionBody* functionBody, ProfileId profileId, Var aLeft, Var aRight);

        void StoreArrayHelper(Var arr, uint32 index, Var value);
        void StoreArraySegHelper(Var arr, uint32 index, Var value);

        LoopEntryPointInfo* GetScheduledEntryPoint(void* framePtr, uint loopnum);
        bool IsLoopCodeGenDone(LoopEntryPointInfo* info);
        void RecordLoopImplicitCallFlags(void* framePtr, uint loopNum, int restoreCallFlags);
    }
}
