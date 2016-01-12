//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"
#include "Base\ThreadContextTLSEntry.h"
#include "Base\ThreadBoundThreadContextManager.h"

ThreadBoundThreadContextManager::EntryList ThreadBoundThreadContextManager::entries(&HeapAllocator::Instance);
JsUtil::BackgroundJobProcessor * ThreadBoundThreadContextManager::s_sharedJobProcessor = NULL;
CriticalSection ThreadBoundThreadContextManager::s_sharedJobProcessorCreationLock;
uint ThreadBoundThreadContextManager::s_maxNumberActiveThreadContexts = 0;

ThreadContext * ThreadBoundThreadContextManager::EnsureContextForCurrentThread()
{
    AutoCriticalSection lock(ThreadContext::GetCriticalSection());

    ThreadContextTLSEntry * entry = ThreadContextTLSEntry::GetEntryForCurrentThread();

    if (entry == NULL)
    {
        ThreadContextTLSEntry::CreateEntryForCurrentThread();
        entry = ThreadContextTLSEntry::GetEntryForCurrentThread();
        entries.Prepend(entry);
    }

    ThreadContext * threadContext = entry->GetThreadContext();

    // An existing TLS entry may have a null ThreadContext
    // DllCanUnload may have cleaned out all the TLS entry when the module lock count is 0,
    // but the library didn't get unloaded because someone is holding onto ref count via LoadLibrary.
    // Just reinitialize the thread context.
    if (threadContext == nullptr)
    {
        threadContext = HeapNew(ThreadContext);
        threadContext->SetIsThreadBound();
        if (!ThreadContextTLSEntry::TrySetThreadContext(threadContext))
        {
            HeapDelete(threadContext);
            return NULL;
        }
    }

    Assert(threadContext != NULL);

    s_maxNumberActiveThreadContexts = max(s_maxNumberActiveThreadContexts, GetActiveThreadContextCount());

    return threadContext;
}

void ThreadBoundThreadContextManager::DestroyContextAndEntryForCurrentThread()
{
    AutoCriticalSection lock(ThreadContext::GetCriticalSection());

    ThreadContextTLSEntry * entry = ThreadContextTLSEntry::GetEntryForCurrentThread();

    if (entry == NULL)
    {
        return;
    }

    ThreadContext * threadContext = static_cast<ThreadContext *>(entry->GetThreadContext());
    entries.Remove(entry);

    if (threadContext != NULL && threadContext->GetIsThreadBound())
    {
        ShutdownThreadContext(threadContext);
    }

    ThreadContextTLSEntry::CleanupThread();
}

void ThreadBoundThreadContextManager::DestroyAllContexts()
{
    JsUtil::BackgroundJobProcessor * jobProcessor = NULL;

    {
        AutoCriticalSection lock(ThreadContext::GetCriticalSection());

        ThreadContextTLSEntry * currentEntry = ThreadContextTLSEntry::GetEntryForCurrentThread();

        if (currentEntry == NULL)
        {
            // We need a current thread entry so that we can use it to release any thread contexts
            // we find below.
            try
            {
                AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_OutOfMemory);
                currentEntry = ThreadContextTLSEntry::CreateEntryForCurrentThread();
                entries.Prepend(currentEntry);
            }
            catch (Js::OutOfMemoryException)
            {
                return;
            }
        }
        else
        {
            // We need to clear out the current thread entry so that we can use it to release any
            // thread contexts we find below.
            ThreadContext * threadContext = static_cast<ThreadContext *>(currentEntry->GetThreadContext());

            if (threadContext != NULL)
            {
                if (threadContext->GetIsThreadBound())
                {
                    ShutdownThreadContext(threadContext);
                    ThreadContextTLSEntry::ClearThreadContext(currentEntry, false);
                }
                else
                {
                    ThreadContextTLSEntry::ClearThreadContext(currentEntry, true);
                }
            }
        }

        EntryList::Iterator iter(&entries);

        while (iter.Next())
        {
            ThreadContextTLSEntry * entry = iter.Data();
            ThreadContext * threadContext =  static_cast<ThreadContext *>(entry->GetThreadContext());

            if (threadContext != nullptr)
            {
                // Found a thread context. Remove it from the containing entry.
                ThreadContextTLSEntry::ClearThreadContext(entry, true);
                // Now set it to our thread's entry.
                ThreadContextTLSEntry::SetThreadContext(currentEntry, threadContext);
                // Clear it out.
                ShutdownThreadContext(threadContext);
                // Now clear it out of our entry.
                ThreadContextTLSEntry::ClearThreadContext(currentEntry, false);
            }
        }

        // We can only clean up our own TLS entry, so we're going to go ahead and do that here.
        entries.Remove(currentEntry);
        ThreadContextTLSEntry::CleanupThread();

        if (s_sharedJobProcessor != NULL)
        {
            jobProcessor = s_sharedJobProcessor;
            s_sharedJobProcessor = NULL;

            jobProcessor->Close();
        }
    }

    if (jobProcessor != NULL)
    {
        HeapDelete(jobProcessor);
    }
}

void ThreadBoundThreadContextManager::DestroyAllContextsAndEntries()
{
    AutoCriticalSection lock(ThreadContext::GetCriticalSection());

    while (!entries.Empty())
    {
        ThreadContextTLSEntry * entry = entries.Head();
        ThreadContext * threadContext =  static_cast<ThreadContext *>(entry->GetThreadContext());

        entries.RemoveHead();

        if (threadContext != nullptr)
        {
#if DBG
            PageAllocator* pageAllocator = threadContext->GetPageAllocator();
            if (pageAllocator)
            {
                pageAllocator->SetConcurrentThreadId(::GetCurrentThreadId());
            }
#endif

            threadContext->ShutdownThreads();

            HeapDelete(threadContext);
        }

        ThreadContextTLSEntry::Delete(entry);
    }

    if (s_sharedJobProcessor != NULL)
    {
        s_sharedJobProcessor->Close();

        HeapDelete(s_sharedJobProcessor);
        s_sharedJobProcessor = NULL;
    }
}

JsUtil::JobProcessor * ThreadBoundThreadContextManager::GetSharedJobProcessor()
{
    if (s_sharedJobProcessor == NULL)
    {
        // Don't use ThreadContext::GetCriticalSection() because it's also locked during thread detach while the loader lock is
        // held, and that may prevent the background job processor's thread from being started due to contention on the loader
        // lock, leading to a deadlock
        AutoCriticalSection lock(&s_sharedJobProcessorCreationLock);

        if (s_sharedJobProcessor == NULL)
        {
            // We don't need to have allocation policy manager for web worker.
            s_sharedJobProcessor = HeapNew(JsUtil::BackgroundJobProcessor, NULL, NULL, false /*disableParallelThreads*/);
        }
    }

    return s_sharedJobProcessor;
}

void RentalThreadContextManager::DestroyThreadContext(ThreadContext* threadContext)
{
    ShutdownThreadContext(threadContext);
}

void ThreadContextManagerBase::ShutdownThreadContext(ThreadContext* threadContext)
{

#if DBG
    PageAllocator* pageAllocator = threadContext->GetPageAllocator();
    if (pageAllocator)
    {
        pageAllocator->SetConcurrentThreadId(::GetCurrentThreadId());
    }
#endif
    threadContext->ShutdownThreads();

    HeapDelete(threadContext);
}

uint ThreadBoundThreadContextManager::GetActiveThreadContextCount()
{
    return entries.Count();
}

void ThreadBoundThreadContextManager::ResetMaxNumberActiveThreadContexts()
{
    s_maxNumberActiveThreadContexts = GetActiveThreadContextCount();
}
