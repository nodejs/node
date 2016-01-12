//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "JavascriptNativeOperators.h"

namespace Js
{
#if ENABLE_NATIVE_CODEGEN
    void * JavascriptNativeOperators::Op_SwitchStringLookUp(JavascriptString* str, Js::BranchDictionaryWrapper<JavascriptString*>* branchTargets, uintptr funcStart, uintptr funcEnd)
    {
        void* defaultTarget = branchTargets->defaultTarget;
        Js::BranchDictionaryWrapper<JavascriptString*>::BranchDictionary& stringDictionary = branchTargets->dictionary;
        void* target = stringDictionary.Lookup(str, defaultTarget);
        uintptr utarget = (uintptr)target;

        if ((utarget - funcStart) > (funcEnd - funcStart))
        {
            AssertMsg(false, "Switch string dictionary jump target outside of function");
            Throw::FatalInternalError();
        }
        return target;
    }
#endif
};
