//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

IdleDecommitPageAllocator::IdleDecommitPageAllocator(AllocationPolicyManager * policyManager, PageAllocatorType type,
#ifndef JD_PRIVATE
    Js::ConfigFlagsTable& flagTable,
#endif
    uint maxFreePageCount, uint maxIdleFreePageCount,
    bool zeroPages, BackgroundPageQueue *  backgroundPageQueue, uint maxAllocPageCount) :
#ifdef IDLE_DECOMMIT_ENABLED
    idleDecommitTryEnterWaitFactor(0),
    hasDecommitTimer(false),
    hadDecommitTimer(false),
#endif
    PageAllocator(policyManager,
#ifndef JD_PRIVATE
        flagTable,
#endif
    type, maxFreePageCount, zeroPages, backgroundPageQueue, maxAllocPageCount),
    maxIdleDecommitFreePageCount(maxIdleFreePageCount),
    maxNonIdleDecommitFreePageCount(maxFreePageCount)
{
    // if maxIdle is the same as max free, disable idleDecommit but setting the entry count to 1
    this->idleDecommitEnterCount = (maxIdleFreePageCount == maxFreePageCount);
#ifdef IDLE_DECOMMIT_ENABLED
#if DBG_DUMP
    idleDecommitCount = 0;
#endif
#endif
}


void
IdleDecommitPageAllocator::EnterIdleDecommit()
{
    this->idleDecommitEnterCount++;
    if (this->idleDecommitEnterCount != 1)
    {
        return;
    }
#ifdef IDLE_DECOMMIT_ENABLED
    cs.Enter();

    this->isUsed = false;
    this->hadDecommitTimer = hasDecommitTimer;
    PAGE_ALLOC_VERBOSE_TRACE(L"EnterIdleDecommit");
    if (hasDecommitTimer)
    {
        // Cancel the decommit timer
        Assert(this->maxFreePageCount == maxIdleDecommitFreePageCount);
        hasDecommitTimer = false;
        PAGE_ALLOC_TRACE(L"Cancel Decommit Timer");
    }
    else
    {
        // Switch to maxIdleDecommitFreePageCount
        Assert(this->maxFreePageCount == maxNonIdleDecommitFreePageCount);
        Assert(minFreePageCount == 0);
        this->maxFreePageCount = maxIdleDecommitFreePageCount;
    }

    cs.Leave();

    Assert(!hasDecommitTimer);
#else
    Assert(this->maxFreePageCount == maxNonIdleDecommitFreePageCount);
    this->maxFreePageCount = maxIdleDecommitFreePageCount;
#endif
}

IdleDecommitSignal
IdleDecommitPageAllocator::LeaveIdleDecommit(bool allowTimer)
{

    Assert(this->idleDecommitEnterCount > 0);
    Assert(this->maxFreePageCount == maxIdleDecommitFreePageCount);

#ifdef IDLE_DECOMMIT_ENABLED
    Assert(!hasDecommitTimer);
#endif

    this->idleDecommitEnterCount--;
    if (this->idleDecommitEnterCount != 0)
    {
        return IdleDecommitSignal_None;
    }

#ifdef IDLE_DECOMMIT_ENABLED
    if (allowTimer)
    {
        cs.Enter();

        PAGE_ALLOC_VERBOSE_TRACE(L"LeaveIdleDecommit");
        Assert(maxIdleDecommitFreePageCount != maxNonIdleDecommitFreePageCount);

        IdleDecommitSignal idleDecommitSignal = IdleDecommitSignal_None;
        if (freePageCount == 0 && !isUsed && !hadDecommitTimer)
        {
            Assert(minFreePageCount == 0);
            Assert(minFreePageCount == debugMinFreePageCount);

            // Nothing to decommit, it isn't used, and there was no timer before.
            // Just switch it back to non idle decommit mode
            this->maxFreePageCount = maxNonIdleDecommitFreePageCount;
        }
        else
        {
            UpdateMinFreePageCount();

            hasDecommitTimer = true;
            idleDecommitSignal = IdleDecommitSignal_NeedTimer;

            if (isUsed)
            {
                // Reschedule the timer
                decommitTime = ::GetTickCount() + IdleDecommitTimeout;
                PAGE_ALLOC_TRACE( L"Schedule idle decommit at %d (%d)", decommitTime, IdleDecommitTimeout);
            }
            else
            {
                int timeDiff = (int)decommitTime - ::GetTickCount();
                if (timeDiff < 20)
                {
                    idleDecommitSignal = IdleDecommitSignal_NeedSignal;
                }

                PAGE_ALLOC_TRACE(L"Reschedule idle decommit at %d (%d)", decommitTime, decommitTime - ::GetTickCount());
            }

        }
        cs.Leave();
        return idleDecommitSignal;
    }
#endif
    this->maxFreePageCount = maxNonIdleDecommitFreePageCount;
    __super::DecommitNow();
    ClearMinFreePageCount();
    return IdleDecommitSignal_None;
}

#ifdef IDLE_DECOMMIT_ENABLED
void
IdleDecommitPageAllocator::DecommitNow(bool all)
{
    SuspendIdleDecommit();

    // If we are in non-idle-decommit mode, then always decommit all.
    // Otherwise, we will end up with some un-decommitted pages and get confused later.
    if (maxFreePageCount == maxNonIdleDecommitFreePageCount)
        all = true;

    __super::DecommitNow(all);

    if (all)
    {
        if (this->hasDecommitTimer)
        {
            Assert(idleDecommitEnterCount == 0);
            Assert(this->maxFreePageCount == maxIdleDecommitFreePageCount);
            this->hasDecommitTimer = false;
            this->maxFreePageCount = maxNonIdleDecommitFreePageCount;
        }
        else
        {
            Assert((idleDecommitEnterCount > 0? maxIdleDecommitFreePageCount : maxNonIdleDecommitFreePageCount)
                == this->maxFreePageCount);
        }
        ClearMinFreePageCount();
    }
    else
    {
        ResetMinFreePageCount();
    }

    ResumeIdleDecommit();
}

DWORD
IdleDecommitPageAllocator::IdleDecommit()
{
    // We can check hasDecommitTimer outside of the lock because when it change to true
    // the Recycler::concurrentIdleDecommitEvent will signal and we try to IdleDecommit again
    // If it change to false, we check again when we acquired the lock
    if (!hasDecommitTimer)
    {
        return INFINITE;
    }
    if (!cs.TryEnter())
    {
        // Failed to acquire the lock, wait for a variable time.
        PAGE_ALLOC_TRACE(L"IdleDecommit Retry");

        // Varies the wait time between 11 - 99
        idleDecommitTryEnterWaitFactor++;
        if (idleDecommitTryEnterWaitFactor >= 10)
        {
            idleDecommitTryEnterWaitFactor = 1;
        }
        DWORD waitTime = 11 * idleDecommitTryEnterWaitFactor;
        return waitTime;      // Retry time
    }
    idleDecommitTryEnterWaitFactor = 0;
    DWORD waitTime = INFINITE;
    if (hasDecommitTimer)
    {
        Assert(this->maxFreePageCount == maxIdleDecommitFreePageCount);
        int timediff = (int)(decommitTime - ::GetTickCount());
        if (timediff >= 20)   // Ignore time diff is it is < 20 since the system timer doesn't have that high of precision anyways
        {
            waitTime = (DWORD)timediff;
        }
        else
        {
            // Do the decommit in normal priority so that we don't block the main thread for too long
            PAGE_ALLOC_TRACE(L"IdleDecommit");
#if DBG_DUMP
            idleDecommitCount++;
#endif
            __super::DecommitNow();
            hasDecommitTimer = false;
            ClearMinFreePageCount();
            this->maxFreePageCount = maxNonIdleDecommitFreePageCount;
        }
    }
    cs.Leave();
    return waitTime;
}

#endif

void
IdleDecommitPageAllocator::Prime(uint primePageCount)
{
    while (this->freePageCount < primePageCount)
    {
        PageSegment * segment = AddPageSegment(emptySegments);
        if (segment == nullptr)
        {
            return;
        }
        segment->Prime();
    }
}


#if DBG
bool
IdleDecommitPageAllocator::HasMultiThreadAccess() const
{
#ifdef IDLE_DECOMMIT_ENABLED
    return this->hasDecommitTimer && !cs.IsLocked();
#else
    return false;
#endif
}

void
IdleDecommitPageAllocator::ShutdownIdleDecommit()
{
    // The recycler thread should have died already
    // Just set the state
    idleDecommitEnterCount = 1;
#ifdef IDLE_DECOMMIT_ENABLED
    hasDecommitTimer = false;
#endif
}
#endif

#ifdef IDLE_DECOMMIT_ENABLED
#if DBG_DUMP
void
IdleDecommitPageAllocator::DumpStats() const
{
    __super::DumpStats();
    Output::Print(L"  Idle Decommit Count       : %4d\n",
        this->idleDecommitCount);
}
#endif
#endif
