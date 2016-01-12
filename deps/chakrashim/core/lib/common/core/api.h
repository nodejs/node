//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#pragma intrinsic(memcpy)
extern void __stdcall js_memcpy_s(__bcount(sizeInBytes) void *dst, size_t sizeInBytes, __in_bcount(count) const void *src, size_t count);
extern void __stdcall js_wmemcpy_s(__ecount(sizeInWords) wchar_t *dst, size_t sizeInWords, __in_ecount(count) const wchar_t *src, size_t count);

// A virtualized thread id. The physical thread on which an instance of the runtime is executed can change but a
// ThreadContextId should be invariant.
// Many parts of the runtime expect to only be called by the execution, or "main", thread. Hosts have the prerogative
// of allocating physical execution threads, thus the physical execution thread is not invariant. Hosts also own the
// virtualization of the thread id and are responsible for making ThreadContextIds invariant.
typedef void * ThreadContextId;
#define NoThreadContextId (ThreadContextId)NULL

// Functions that need to be implemented by user of Common library
namespace Js
{
    // Forward declaration
    class ScriptContext;
};

namespace JsUtil
{
    struct ExternalApi
    {
        // Returns the current execution ThreadContextId
        static ThreadContextId GetCurrentThreadContextId();

        static bool RaiseOutOfMemoryIfScriptActive();
        static bool RaiseStackOverflowIfScriptActive(Js::ScriptContext * scriptContext, PVOID returnAddress);
        static bool RaiseOnIntOverflow();
        static void RecoverUnusedMemory();
#if DBG || defined(EXCEPTION_CHECK)
        static BOOL IsScriptActiveOnCurrentThreadContext();
#endif

        // By default, implemented in Dll\Jscript\ScriptEngine.cpp
        // Anyone who statically links with jscript.common.common.lib has to implement this
        // This is used to determine which regkey we should read while loading the configuration
        static LPWSTR GetFeatureKeyName();
    };
};

// Just an alias to ExternalApi::GetCurrentThreadContextId
inline ThreadContextId GetCurrentThreadContextId()
{
    return JsUtil::ExternalApi::GetCurrentThreadContextId();
}
