//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
class ThreadContextManagerBase
{
protected:

    static void ShutdownThreadContext(ThreadContext* threadContext);
};

class ThreadBoundThreadContextManager : public ThreadContextManagerBase
{
    friend class ThreadContext;

public:
    typedef DListCounted<ThreadContextTLSEntry *, HeapAllocator> EntryList;

    static ThreadContext * EnsureContextForCurrentThread();
    static void DestroyContextAndEntryForCurrentThread();
    static void DestroyAllContexts();
    static void DestroyAllContextsAndEntries();
    static JsUtil::JobProcessor * GetSharedJobProcessor();
    static uint GetActiveThreadContextCount();
    static void ResetMaxNumberActiveThreadContexts();
    static uint s_maxNumberActiveThreadContexts;

private:
    static EntryList entries;
    static JsUtil::BackgroundJobProcessor * s_sharedJobProcessor;
    static CriticalSection s_sharedJobProcessorCreationLock;
};

class RentalThreadContextManager : public ThreadContextManagerBase
{
public:
    static void DestroyThreadContext(ThreadContext* threadContext);

};
