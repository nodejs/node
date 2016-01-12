//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"
#include "Base\ThreadContextTLSEntry.h"

ulong ThreadContextTLSEntry::s_tlsSlot = TLS_OUT_OF_INDEXES;

bool ThreadContextTLSEntry::InitializeProcess()
{
    Assert(s_tlsSlot == TLS_OUT_OF_INDEXES);
    s_tlsSlot = TlsAlloc();
    return (s_tlsSlot != TLS_OUT_OF_INDEXES);
}

void ThreadContextTLSEntry::CleanupProcess()
{
    Assert(s_tlsSlot != TLS_OUT_OF_INDEXES);
    TlsFree(s_tlsSlot);
    s_tlsSlot = TLS_OUT_OF_INDEXES;
}

bool ThreadContextTLSEntry::IsProcessInitialized()
{
    return s_tlsSlot != TLS_OUT_OF_INDEXES;
}

void ThreadContextTLSEntry::InitializeThread()
{
    Assert(s_tlsSlot != TLS_OUT_OF_INDEXES);
    Assert(!TlsGetValue(s_tlsSlot));
    TlsSetValue(s_tlsSlot, NULL);
}

void ThreadContextTLSEntry::CleanupThread()
{
    Assert(s_tlsSlot != TLS_OUT_OF_INDEXES);
    ThreadContextTLSEntry * entry = GetEntryForCurrentThread();

    if (entry != NULL)
    {
        HeapDelete(entry);
        TlsSetValue(s_tlsSlot, NULL);
    }
}

bool ThreadContextTLSEntry::TrySetThreadContext(ThreadContext * threadContext)
{
    Assert(threadContext != NULL);
    Assert(s_tlsSlot != TLS_OUT_OF_INDEXES);

    DWORD threadContextThreadId = threadContext->GetCurrentThreadId();

    // If a thread context is current on another thread, then you cannot set it to current on this one.
    if (threadContextThreadId != ThreadContext::NoThread && threadContextThreadId != ::GetCurrentThreadId())
    {
        // the thread doesn't support rental thread and try to set on a different thread???
        Assert(!threadContext->GetIsThreadBound());
        return false;
    }

    ThreadContextTLSEntry * entry = GetEntryForCurrentThread();

    if (entry == NULL)
    {
        Assert(!threadContext->GetIsThreadBound());
        entry = CreateEntryForCurrentThread();
    }
    else if (entry->threadContext != NULL && entry->threadContext != threadContext)
    {
        // If the thread has an active thread context and either that thread context is thread
        // bound (in which case it cannot be moved off this thread), or if the thread context
        // is running script, you cannot move it off this thread.
        if (entry->threadContext->GetIsThreadBound() || entry->threadContext->IsInScript())
        {
            return false;
        }

        ClearThreadContext(entry, true);
    }

    SetThreadContext(entry, threadContext);

    return true;
}

void ThreadContextTLSEntry::SetThreadContext(ThreadContextTLSEntry * entry, ThreadContext * threadContext)
{
    entry->threadContext = threadContext;
    threadContext->SetStackProber(&entry->prober);
    threadContext->SetCurrentThreadId(::GetCurrentThreadId());
}

bool ThreadContextTLSEntry::ClearThreadContext(bool isValid)
{
    return ClearThreadContext(GetEntryForCurrentThread(), isValid, false);
}

bool ThreadContextTLSEntry::ClearThreadContext(ThreadContextTLSEntry * entry, bool isThreadContextValid, bool force)
{
    Assert(s_tlsSlot != TLS_OUT_OF_INDEXES);

    if (entry != NULL)
    {
        if (entry->threadContext != NULL && isThreadContextValid)
        {
            // If the thread has an active thread context and either that thread context is thread
            // bound (in which case it cannot be moved off this thread), or if the thread context
            // is running script, you cannot move it off this thread.
            if (!force && (entry->threadContext->GetIsThreadBound() || entry->threadContext->IsInScript()))
            {
                return false;
            }
            entry->threadContext->SetCurrentThreadId(ThreadContext::NoThread);
            entry->threadContext->SetStackProber(NULL);
        }

        entry->threadContext = NULL;
    }

    return true;
}

void ThreadContextTLSEntry::Delete(ThreadContextTLSEntry * entry)
{
    HeapDelete(entry);
}

__inline ThreadContextTLSEntry * ThreadContextTLSEntry::GetEntryForCurrentThread()
{
    Assert(s_tlsSlot != TLS_OUT_OF_INDEXES);
    return reinterpret_cast<ThreadContextTLSEntry *>(TlsGetValue(s_tlsSlot));
}

ThreadContextTLSEntry * ThreadContextTLSEntry::CreateEntryForCurrentThread()
{
    Assert(s_tlsSlot != TLS_OUT_OF_INDEXES);
    Assert(TlsGetValue(s_tlsSlot) == NULL);

    ThreadContextTLSEntry * entry = HeapNewStructZ(ThreadContextTLSEntry);
#pragma prefast(suppress:6001, "Memory from HeapNewStructZ are zero initialized")
    entry->prober.Initialize();
    TlsSetValue(s_tlsSlot, entry);

    return entry;
}

__inline ThreadContext * ThreadContextTLSEntry::GetThreadContext()
{
    return this->threadContext;
}

ThreadContextId ThreadContextTLSEntry::GetCurrentThreadContextId()
{
    ThreadContextTLSEntry * entry = GetEntryForCurrentThread();
    if (entry != NULL && entry->GetThreadContext() != NULL)
    {
        return (ThreadContextId)entry->GetThreadContext();
    }

    return NoThreadContextId;
}

ThreadContextId ThreadContextTLSEntry::GetThreadContextId(ThreadContext * threadContext)
{
    return threadContext;
}
