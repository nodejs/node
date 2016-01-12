//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#if defined(_M_IX86) || defined(_M_X64)
// For prefetch
#include <mmintrin.h>
#endif


MarkContext::MarkContext(Recycler * recycler, PagePool * pagePool) :
    recycler(recycler),
    pagePool(pagePool),
    markStack(pagePool),
    trackStack(pagePool)
{
}


MarkContext::~MarkContext()
{
#ifdef RECYCLER_MARK_TRACK
    this->markMap = nullptr;
#endif
}

#ifdef RECYCLER_MARK_TRACK
void MarkContext::OnObjectMarked(void* object, void* parent)
{
    if (!this->markMap->ContainsKey(object))
    {
        this->markMap->AddNew(object, parent);
    }
}
#endif

void MarkContext::Init(uint reservedPageCount)
{
    markStack.Init(reservedPageCount);
    trackStack.Init();
}

void MarkContext::Clear()
{
    markStack.Clear();
    trackStack.Clear();
}

void MarkContext::Abort()
{
    markStack.Abort();
    trackStack.Abort();

    pagePool->ReleaseFreePages();
}


void MarkContext::Release()
{
    markStack.Release();
    trackStack.Release();

    pagePool->ReleaseFreePages();
}


uint MarkContext::Split(uint targetCount, __in_ecount(targetCount) MarkContext ** targetContexts)
{
    Assert(targetCount > 0 && targetCount <= PageStack<MarkCandidate>::MaxSplitTargets);
    __analysis_assume(targetCount <= PageStack<MarkCandidate>::MaxSplitTargets);

    PageStack<MarkCandidate> * targetStacks[PageStack<MarkCandidate>::MaxSplitTargets];

    for (uint i = 0; i < targetCount; i++)
    {
        targetStacks[i] = &targetContexts[i]->markStack;
    }

    return this->markStack.Split(targetCount, targetStacks);
}


void MarkContext::ProcessTracked()
{
    if (trackStack.IsEmpty())
    {
        return;
    }

    FinalizableObject * trackedObject;
    while (trackStack.Pop(&trackedObject))
    {
        MarkTrackedObject(trackedObject);
    }

    Assert(trackStack.IsEmpty());

    trackStack.Release();
}



