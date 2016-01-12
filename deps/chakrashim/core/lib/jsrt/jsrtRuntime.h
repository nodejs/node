//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "chakracommon.h"

#include "JsrtThreadService.h"

class JsrtRuntime
{
    friend class JsrtContext;

public:
    JsrtRuntime(ThreadContext * threadContext, bool useIdle, bool dispatchExceptions);
    ~JsrtRuntime();

    ThreadContext * GetThreadContext() { return this->threadContext; }

    JsRuntimeHandle ToHandle() { return static_cast<JsRuntimeHandle>(this); }
    static JsrtRuntime * FromHandle(JsRuntimeHandle runtimeHandle)
    {
        JsrtRuntime * runtime = static_cast<JsrtRuntime *>(runtimeHandle);
        runtime->threadContext->ValidateThreadContext();
        return runtime;
    }
    static void Uninitialize();

    bool UseIdle() const { return useIdle; }
    unsigned int Idle();

    bool DispatchExceptions() const { return dispatchExceptions; }

    void CloseContexts();
    void SetBeforeCollectCallback(JsBeforeCollectCallback beforeCollectCallback, void * callbackContext);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void SetSerializeByteCodeForLibrary(bool set) { serializeByteCodeForLibrary = set; }
    bool IsSerializeByteCodeForLibrary() const { return serializeByteCodeForLibrary; }
#endif

private:
    static void __cdecl RecyclerCollectCallbackStatic(void * context, RecyclerCollectCallBackFlags flags);

private:
    ThreadContext * threadContext;
    AllocationPolicyManager* allocationPolicyManager;
    JsrtContext * contextList;
    ThreadContext::CollectCallBack * collectCallback;
    JsBeforeCollectCallback beforeCollectCallback;
    JsrtThreadService threadService;
    void * callbackContext;
    bool useIdle;
    bool dispatchExceptions;
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    bool serializeByteCodeForLibrary;
#endif
};


