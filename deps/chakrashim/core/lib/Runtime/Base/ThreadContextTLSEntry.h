//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class ThreadContextTLSEntry
{
public:
    static bool InitializeProcess();
    static void CleanupProcess();
    static bool IsProcessInitialized();
    static void InitializeThread();
    static void CleanupThread();
    static void Delete(ThreadContextTLSEntry * entry);
    static bool TrySetThreadContext(ThreadContext * threadContext);
    static void SetThreadContext(ThreadContextTLSEntry * entry, ThreadContext * threadContext);
    static bool ClearThreadContext(bool isValid);
    static bool ClearThreadContext(ThreadContextTLSEntry * entry, bool isThreadContextValid, bool force = true);
    static ThreadContextTLSEntry * GetEntryForCurrentThread();
    static ThreadContextTLSEntry * CreateEntryForCurrentThread();
    static ThreadContextId GetThreadContextId(ThreadContext * threadContext);

    static ulong s_tlsSlot;

    ThreadContext * GetThreadContext();

private:
    friend JsUtil::ExternalApi;
    static ThreadContextId GetCurrentThreadContextId();

private:
    ThreadContext * threadContext;
    StackProber prober;

};

class ThreadContextScope
{
public:
    ThreadContextScope(ThreadContext * threadContext)
    {
        if (!threadContext->GetIsThreadBound())
        {
            originalContext = ThreadContextTLSEntry::GetEntryForCurrentThread() ?
                ThreadContextTLSEntry::GetEntryForCurrentThread()->GetThreadContext() : NULL;
            wasInUse = threadContext == originalContext;
            isValid = ThreadContextTLSEntry::TrySetThreadContext(threadContext);
            doCleanup = !wasInUse && isValid;
        }
        else
        {
            Assert(ThreadContext::GetContextForCurrentThread() == threadContext);
            isValid = true;
            wasInUse = true;
            doCleanup = false;
        }
    }

    ~ThreadContextScope()
    {
        if (doCleanup)
        {
            bool cleared = true;

#if DBG
            cleared =
#endif
                ThreadContextTLSEntry::ClearThreadContext(this->isValid);
            Assert(cleared);

            if (originalContext)
            {
                bool canSetback = true;
#if DBG
                canSetback =
#endif
                    ThreadContextTLSEntry::TrySetThreadContext(originalContext);
                Assert(canSetback);
            }
        }
    }

    void Invalidate()
    {
        this->isValid = false;
    }

    bool IsValid() const
    {
        return this->isValid;
    }

    bool WasInUse() const
    {
        return this->wasInUse;
    }

private:
    bool doCleanup;
    bool isValid;
    bool wasInUse;
    ThreadContext* originalContext;
};

