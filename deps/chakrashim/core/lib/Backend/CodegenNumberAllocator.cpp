//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"


CodeGenNumberThreadAllocator::CodeGenNumberThreadAllocator(Recycler * recycler)
    : recycler(recycler), currentNumberSegment(nullptr), currentChunkSegment(nullptr),
    numberSegmentEnd(nullptr), currentNumberBlockEnd(nullptr), nextNumber(nullptr), chunkSegmentEnd(nullptr),
    currentChunkBlockEnd(nullptr), nextChunk(nullptr), hasNewNumberBlock(nullptr), hasNewChunkBlock(nullptr),
    pendingIntegrationNumberSegmentCount(0), pendingIntegrationChunkSegmentCount(0),
    pendingIntegrationNumberSegmentPageCount(0), pendingIntegrationChunkSegmentPageCount(0)
{
}

CodeGenNumberThreadAllocator::~CodeGenNumberThreadAllocator()
{
    pendingIntegrationNumberSegment.Clear(&NoThrowNoMemProtectHeapAllocator::Instance);
    pendingIntegrationChunkSegment.Clear(&NoThrowNoMemProtectHeapAllocator::Instance);
    pendingIntegrationNumberBlock.Clear(&NoThrowHeapAllocator::Instance);
    pendingIntegrationChunkBlock.Clear(&NoThrowHeapAllocator::Instance);
    pendingFlushNumberBlock.Clear(&NoThrowHeapAllocator::Instance);
    pendingFlushChunkBlock.Clear(&NoThrowHeapAllocator::Instance);
    pendingReferenceNumberBlock.Clear(&NoThrowHeapAllocator::Instance);
}

size_t
CodeGenNumberThreadAllocator::GetNumberAllocSize()
{
#ifdef RECYCLER_MEMORY_VERIFY
    if (recycler->VerifyEnabled())
    {
        return HeapInfo::GetAlignedSize(AllocSizeMath::Add(sizeof(Js::JavascriptNumber) + sizeof(size_t), recycler->verifyPad));
    }
#endif
    return HeapInfo::GetAlignedSizeNoCheck(sizeof(Js::JavascriptNumber));
}


size_t
CodeGenNumberThreadAllocator::GetChunkAllocSize()
{
#ifdef RECYCLER_MEMORY_VERIFY
    if (recycler->VerifyEnabled())
    {
        return HeapInfo::GetAlignedSize(AllocSizeMath::Add(sizeof(CodeGenNumberChunk) + sizeof(size_t), recycler->verifyPad));
    }
#endif
    return HeapInfo::GetAlignedSizeNoCheck(sizeof(CodeGenNumberChunk));
}

Js::JavascriptNumber *
CodeGenNumberThreadAllocator::AllocNumber()
{
    AutoCriticalSection autocs(&cs);
    size_t sizeCat = GetNumberAllocSize();
    if (nextNumber + sizeCat > currentNumberBlockEnd)
    {
        AllocNewNumberBlock();
    }
    Js::JavascriptNumber * newNumber = (Js::JavascriptNumber *)nextNumber;
#ifdef RECYCLER_MEMORY_VERIFY
    recycler->FillCheckPad(newNumber, sizeof(Js::JavascriptNumber), sizeCat);
#endif

    nextNumber += sizeCat;
    return newNumber;
}

CodeGenNumberChunk *
CodeGenNumberThreadAllocator::AllocChunk()
{
    AutoCriticalSection autocs(&cs);
    size_t sizeCat = GetChunkAllocSize();
    if (nextChunk + sizeCat > currentChunkBlockEnd)
    {
        AllocNewChunkBlock();
    }
    CodeGenNumberChunk * newChunk = (CodeGenNumberChunk *)nextChunk;
#ifdef RECYCLER_MEMORY_VERIFY
    recycler->FillCheckPad(nextChunk, sizeof(CodeGenNumberChunk), sizeCat);
#endif

    memset(newChunk, 0, sizeof(CodeGenNumberChunk));
    nextChunk += sizeCat;
    return newChunk;
}

void
CodeGenNumberThreadAllocator::AllocNewNumberBlock()
{
    Assert(cs.IsLocked());
    Assert(nextNumber + GetNumberAllocSize() > currentNumberBlockEnd);
    if (hasNewNumberBlock)
    {
        if (!pendingReferenceNumberBlock.PrependNode(&NoThrowHeapAllocator::Instance,
            currentNumberBlockEnd - BlockSize, currentNumberSegment))
        {
            Js::Throw::OutOfMemory();
        }
        hasNewNumberBlock = false;
    }

    if (currentNumberBlockEnd == numberSegmentEnd)
    {
        Assert(cs.IsLocked());
        // Reserve the segment, but not committing it
        currentNumberSegment = PageAllocator::AllocPageSegment(pendingIntegrationNumberSegment, this->recycler->GetRecyclerLeafPageAllocator(), true);
        if (currentNumberSegment == nullptr)
        {
            currentNumberBlockEnd = nullptr;
            numberSegmentEnd = nullptr;
            nextNumber = nullptr;
            Js::Throw::OutOfMemory();
        }
        pendingIntegrationNumberSegmentCount++;
        pendingIntegrationNumberSegmentPageCount += currentNumberSegment->GetPageCount();
        currentNumberBlockEnd = currentNumberSegment->GetAddress();
        numberSegmentEnd = currentNumberSegment->GetEndAddress();
    }

    // Commit the page.
    if (!::VirtualAlloc(currentNumberBlockEnd, BlockSize, MEM_COMMIT, PAGE_READWRITE))
    {
        Js::Throw::OutOfMemory();
    }
    nextNumber = currentNumberBlockEnd;
    currentNumberBlockEnd += BlockSize;
    hasNewNumberBlock = true;
    this->recycler->GetRecyclerLeafPageAllocator()->FillAllocPages(nextNumber, 1);
}

void
CodeGenNumberThreadAllocator::AllocNewChunkBlock()
{
    Assert(cs.IsLocked());
    Assert(nextChunk + GetChunkAllocSize() > currentChunkBlockEnd);
    if (hasNewChunkBlock)
    {
        if (!pendingFlushChunkBlock.PrependNode(&NoThrowHeapAllocator::Instance,
            currentChunkBlockEnd - BlockSize, currentChunkSegment))
        {
            Js::Throw::OutOfMemory();
        }
        // All integrated pages' object are all live initially, so don't need to rescan them
        ::ResetWriteWatch(currentChunkBlockEnd - BlockSize, BlockSize);
        pendingReferenceNumberBlock.MoveTo(&pendingFlushNumberBlock);
        hasNewChunkBlock = false;
    }

    if (currentChunkBlockEnd == chunkSegmentEnd)
    {
        Assert(cs.IsLocked());
        // Reserve the segment, but not committing it
        currentChunkSegment = PageAllocator::AllocPageSegment(pendingIntegrationChunkSegment, this->recycler->GetRecyclerPageAllocator(), true);
        if (currentChunkSegment == nullptr)
        {
            currentChunkBlockEnd = nullptr;
            chunkSegmentEnd = nullptr;
            nextChunk = nullptr;
            Js::Throw::OutOfMemory();
        }
        pendingIntegrationChunkSegmentCount++;
        pendingIntegrationChunkSegmentPageCount += currentChunkSegment->GetPageCount();
        currentChunkBlockEnd = currentChunkSegment->GetAddress();
        chunkSegmentEnd = currentChunkSegment->GetEndAddress();
    }

    // Commit the page.
    if (!::VirtualAlloc(currentChunkBlockEnd, BlockSize, MEM_COMMIT, PAGE_READWRITE))
    {
        Js::Throw::OutOfMemory();
    }

    nextChunk = currentChunkBlockEnd;
    currentChunkBlockEnd += BlockSize;
    hasNewChunkBlock = true;
    this->recycler->GetRecyclerLeafPageAllocator()->FillAllocPages(nextChunk, 1);
}

void
CodeGenNumberThreadAllocator::Integrate()
{
    AutoCriticalSection autocs(&cs);
    PageAllocator * leafPageAllocator = this->recycler->GetRecyclerLeafPageAllocator();
    leafPageAllocator->IntegrateSegments(pendingIntegrationNumberSegment, pendingIntegrationNumberSegmentCount, pendingIntegrationNumberSegmentPageCount);
    PageAllocator * recyclerPageAllocator = this->recycler->GetRecyclerPageAllocator();
    recyclerPageAllocator->IntegrateSegments(pendingIntegrationChunkSegment, pendingIntegrationChunkSegmentCount, pendingIntegrationChunkSegmentPageCount);
    pendingIntegrationNumberSegmentCount = 0;
    pendingIntegrationChunkSegmentCount = 0;
    pendingIntegrationNumberSegmentPageCount = 0;
    pendingIntegrationChunkSegmentPageCount = 0;

#ifdef TRACK_ALLOC
    TrackAllocData oldAllocData = recycler->nextAllocData;
    recycler->nextAllocData.Clear();
#endif
    while (!pendingIntegrationNumberBlock.Empty())
    {
        TRACK_ALLOC_INFO(recycler, Js::JavascriptNumber, Recycler, 0, (size_t)-1);

        BlockRecord& record = pendingIntegrationNumberBlock.Head();
        if (!recycler->IntegrateBlock<LeafBit>(record.blockAddress, record.segment, GetNumberAllocSize(), sizeof(Js::JavascriptNumber)))
        {
            Js::Throw::OutOfMemory();
        }
        pendingIntegrationNumberBlock.RemoveHead(&NoThrowHeapAllocator::Instance);
    }


    while (!pendingIntegrationChunkBlock.Empty())
    {
        TRACK_ALLOC_INFO(recycler, CodeGenNumberChunk, Recycler, 0, (size_t)-1);

        BlockRecord& record = pendingIntegrationChunkBlock.Head();
        if (!recycler->IntegrateBlock<NoBit>(record.blockAddress, record.segment, GetChunkAllocSize(), sizeof(CodeGenNumberChunk)))
        {
            Js::Throw::OutOfMemory();
        }
        pendingIntegrationChunkBlock.RemoveHead(&NoThrowHeapAllocator::Instance);
    }
#ifdef TRACK_ALLOC
    Assert(recycler->nextAllocData.IsEmpty());
    recycler->nextAllocData = oldAllocData;
#endif
}

void
CodeGenNumberThreadAllocator::FlushAllocations()
{
    AutoCriticalSection autocs(&cs);
    pendingFlushNumberBlock.MoveTo(&pendingIntegrationNumberBlock);
    pendingFlushChunkBlock.MoveTo(&pendingIntegrationChunkBlock);
}

CodeGenNumberAllocator::CodeGenNumberAllocator(CodeGenNumberThreadAllocator * threadAlloc, Recycler * recycler) :
    threadAlloc(threadAlloc), recycler(recycler), chunk(nullptr), chunkTail(nullptr), currentChunkNumberCount(CodeGenNumberChunk::MaxNumberCount)
{
#if DBG
    finalized = false;
#endif
}

// We should never call this function if we are using tagged float
#if !FLOATVAR
Js::JavascriptNumber *
CodeGenNumberAllocator::Alloc()
{
    Assert(!finalized);
    if (currentChunkNumberCount == CodeGenNumberChunk::MaxNumberCount)
    {
        CodeGenNumberChunk * newChunk = threadAlloc? threadAlloc->AllocChunk()
            : RecyclerNewStructZ(recycler, CodeGenNumberChunk);
        // Need to always put the new chunk last, as when we flush
        // pages, new chunk's page might not be full yet, and won't
        // be flushed, and we will have a broken link in the link list.
        newChunk->next = nullptr;
        if (this->chunkTail != nullptr)
        {
            this->chunkTail->next = newChunk;
        }
        else
        {
            this->chunk = newChunk;
        }
        this->chunkTail = newChunk;
        this->currentChunkNumberCount = 0;
    }
    Js::JavascriptNumber * newNumber = threadAlloc? threadAlloc->AllocNumber()
        : Js::JavascriptNumber::NewUninitialized(recycler);
    this->chunkTail->numbers[this->currentChunkNumberCount++] = newNumber;
    return newNumber;
}
#endif

CodeGenNumberChunk *
CodeGenNumberAllocator::Finalize()
{
    Assert(!finalized);
#if DBG
    finalized = true;
#endif
    CodeGenNumberChunk * finalizedChunk = this->chunk;
    this->chunk = nullptr;
    this->chunkTail = nullptr;
    this->currentChunkNumberCount = 0;
    return finalizedChunk;
}
