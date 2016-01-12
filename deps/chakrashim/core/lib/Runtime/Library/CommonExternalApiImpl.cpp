//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Base\Exception.h"
#include "Base\ThreadContextTLSEntry.h"

void JsUtil::ExternalApi::RecoverUnusedMemory()
{
    Js::Exception::RecoverUnusedMemory();
}

bool JsUtil::ExternalApi::RaiseOnIntOverflow()
{
    ::Math::DefaultOverflowPolicy();
}

bool JsUtil::ExternalApi::RaiseOutOfMemoryIfScriptActive()
{
    return Js::Exception::RaiseIfScriptActive(nullptr, Js::Exception::ExceptionKind_OutOfMemory);
}

bool JsUtil::ExternalApi::RaiseStackOverflowIfScriptActive(Js::ScriptContext * scriptContext, PVOID returnAddress)
{
    return Js::Exception::RaiseIfScriptActive(scriptContext, Js::Exception::ExceptionKind_StackOverflow, returnAddress);
}

ThreadContextId JsUtil::ExternalApi::GetCurrentThreadContextId()
{
    return ThreadContextTLSEntry::GetCurrentThreadContextId();
}

#if DBG || defined(EXCEPTION_CHECK)
BOOL JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext()
{
    return ThreadContext::GetContextForCurrentThread() != nullptr &&
        ThreadContext::GetContextForCurrentThread()->IsScriptActive();
}
#endif

