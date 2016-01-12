//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#define ASSERT_THREAD() AssertMsg(this->pageAllocator->ValidThreadAccess(), "Arena allocation should only be used by a single thread")

template __forceinline BVSparseNode * BVSparse<JitArenaAllocator>::NodeFromIndex(BVIndex i, BVSparseNode *** prevNextFieldOut, bool create);

ArenaData::ArenaData(PageAllocator * pageAllocator) :
    pageAllocator(pageAllocator),
    bigBlocks(nullptr),
    mallocBlocks(nullptr),
    fullBlocks(nullptr),
    cacheBlockCurrent(nullptr),
    lockBlockList(false)
{
}

void ArenaData::UpdateCacheBlock() const
{
    if (bigBlocks != nullptr)
    {
        size_t currentByte = (cacheBlockCurrent - bigBlocks->GetBytes());
        // Avoid writing to the page unnecessary, it might be write watched
        if (currentByte != bigBlocks->currentByte)
        {
            bigBlocks->currentByte = currentByte;
        }
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
ArenaAllocatorBase(__in LPCWSTR name, PageAllocator * pageAllocator, void(*outOfMemoryFunc)(), void(*recoverMemoryFunc)()) :
    Allocator(outOfMemoryFunc, recoverMemoryFunc),
    ArenaData(pageAllocator),
#ifdef ARENA_ALLOCATOR_FREE_LIST_SIZE
    freeListSize(0),
#endif
    freeList(nullptr),
    largestHole(0),
    cacheBlockEnd(nullptr),
    blockState(0)
{
#ifdef PROFILE_MEM
    this->name = name;
    LogBegin();
#endif
    ArenaMemoryTracking::ArenaCreated(this, name);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
~ArenaAllocatorBase()
{
    Assert(!lockBlockList);
    ArenaMemoryTracking::ReportFreeAll(this);
    ArenaMemoryTracking::ArenaDestroyed(this);

    if (!pageAllocator->IsClosed())
    {
        ReleasePageMemory();
    }
    ReleaseHeapMemory();
    TFreeListPolicy::Release(this->freeList);
#ifdef PROFILE_MEM
    LogEnd();
#endif
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
Move(ArenaAllocatorBase *srcAllocator)
{
    Assert(!lockBlockList);
    Assert(srcAllocator != nullptr);
    Allocator::Move(srcAllocator);

    Assert(this->pageAllocator == srcAllocator->pageAllocator);
    AllocatorFieldMove(this, srcAllocator, bigBlocks);
    AllocatorFieldMove(this, srcAllocator, largestHole);
    AllocatorFieldMove(this, srcAllocator, cacheBlockCurrent);
    AllocatorFieldMove(this, srcAllocator, cacheBlockEnd);
    AllocatorFieldMove(this, srcAllocator, mallocBlocks);
    AllocatorFieldMove(this, srcAllocator, fullBlocks);
    AllocatorFieldMove(this, srcAllocator, blockState);
    AllocatorFieldMove(this, srcAllocator, freeList);

#ifdef PROFILE_MEM
    this->name = srcAllocator->name;
    srcAllocator->name = nullptr;
    AllocatorFieldMove(this, srcAllocator, memoryData);
#endif
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
size_t
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
AllocatedSize(ArenaMemoryBlock * blockList)
{
    ArenaMemoryBlock * memoryBlock = blockList;
    size_t totalBytes = 0;
    while (memoryBlock != NULL)
    {
        totalBytes += memoryBlock->nbytes;
        memoryBlock = memoryBlock->next;
    }
    return totalBytes;
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
size_t
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
AllocatedSize()
{
    UpdateCacheBlock();
    return AllocatedSize(this->fullBlocks) + AllocatedSize(this->bigBlocks) + AllocatedSize(this->mallocBlocks);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
size_t
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
Size()
{
    UpdateCacheBlock();
    return Size(this->fullBlocks) + Size(this->bigBlocks) + AllocatedSize(this->mallocBlocks);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
size_t
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
Size(BigBlock * blockList)
{
    BigBlock * memoryBlock = blockList;
    size_t totalBytes = 0;
    while (memoryBlock != NULL)
    {
        totalBytes += memoryBlock->currentByte;
        memoryBlock = (BigBlock *)memoryBlock->next;
    }
    return totalBytes;
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
RealAlloc(size_t nbytes)
{
    return RealAllocInlined(nbytes);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
RealAllocInlined(size_t nbytes)
{
    Assert(nbytes != 0);
    Assert((nbytes & (ObjectAlignment - 1)) == 0);

#ifdef ARENA_MEMORY_VERIFY
    if (Js::Configuration::Global.flags.ArenaUseHeapAlloc)
    {
        return AllocFromHeap<true>(nbytes);
    }
#endif

    Assert(cacheBlockEnd >= cacheBlockCurrent);
    char * p = cacheBlockCurrent;
    if ((size_t)(cacheBlockEnd - p) >= nbytes)
    {
        Assert(cacheBlockEnd == bigBlocks->GetBytes() + bigBlocks->nbytes);
        Assert(bigBlocks->GetBytes() <= cacheBlockCurrent && cacheBlockCurrent <= cacheBlockEnd);
        cacheBlockCurrent = p + nbytes;

        ArenaMemoryTracking::ReportAllocation(this, p, nbytes);

        return(p);
    }

    return SnailAlloc(nbytes);

}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
SetCacheBlock(BigBlock * newCacheBlock)
{
    if (bigBlocks != nullptr)
    {
        Assert(cacheBlockEnd == bigBlocks->GetBytes() + bigBlocks->nbytes);
        Assert(bigBlocks->GetBytes() <= cacheBlockCurrent && cacheBlockCurrent <= cacheBlockEnd);

        bigBlocks->currentByte = (cacheBlockCurrent - bigBlocks->GetBytes());
        uint cacheBlockRemainBytes = (uint)(cacheBlockEnd - cacheBlockCurrent);
        if (cacheBlockRemainBytes < ObjectAlignment && !lockBlockList)
        {
            BigBlock * cacheBlock = bigBlocks;
            bigBlocks = bigBlocks->nextBigBlock;
            cacheBlock->next = fullBlocks;
            fullBlocks = cacheBlock;
        }
        else
        {
            largestHole = max(largestHole, static_cast<size_t>(cacheBlockRemainBytes));
        }
    }
    cacheBlockCurrent = newCacheBlock->GetBytes() + newCacheBlock->currentByte;
    cacheBlockEnd = newCacheBlock->GetBytes() + newCacheBlock->nbytes;
    newCacheBlock->nextBigBlock = bigBlocks;
    bigBlocks = newCacheBlock;
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
SnailAlloc(size_t nbytes)
{
    BigBlock* blockp = NULL;
    size_t currentLargestHole = 0;

    if (nbytes <= largestHole)
    {
        Assert(bigBlocks != nullptr);
        Assert(cacheBlockEnd == bigBlocks->GetBytes() + bigBlocks->nbytes);
        Assert(bigBlocks->GetBytes() <= cacheBlockCurrent && cacheBlockCurrent <= cacheBlockEnd);

        BigBlock * cacheBlock = bigBlocks;
        BigBlock** pPrev= &(bigBlocks->nextBigBlock);
        blockp = bigBlocks->nextBigBlock;
        int giveUpAfter = 10;
        do
        {
            size_t remainingBytes = blockp->nbytes - blockp->currentByte;
            if (remainingBytes >= nbytes)
            {
                char *p = blockp->GetBytes() + blockp->currentByte;
                blockp->currentByte += nbytes;
                if (remainingBytes == largestHole || currentLargestHole > largestHole)
                {
                    largestHole = currentLargestHole;
                }
                remainingBytes -= nbytes;
                if (remainingBytes > cacheBlock->nbytes - cacheBlock->currentByte)
                {
                    *pPrev = blockp->nextBigBlock;
                    SetCacheBlock(blockp);
                }
                else if (remainingBytes < ObjectAlignment && !lockBlockList)
                {
                    *pPrev = blockp->nextBigBlock;
                    blockp->nextBigBlock = fullBlocks;
                    fullBlocks = blockp;
                }

                ArenaMemoryTracking::ReportAllocation(this, p, nbytes);

                return(p);
            }
            currentLargestHole = max(currentLargestHole, remainingBytes);
            if (--giveUpAfter == 0)
            {
                break;
            }
            pPrev = &(blockp->nextBigBlock);
            blockp = blockp->nextBigBlock;
        }
        while (blockp != nullptr);
    }

    blockp = AddBigBlock(nbytes);
    if (blockp == nullptr)
    {
        return AllocFromHeap<false>(nbytes);    // Passing DoRecoverMemory=false as we already tried recovering memory in AddBigBlock, and it is costly.
    }

    this->blockState++;
    SetCacheBlock(blockp);
    char *p = cacheBlockCurrent;
    Assert(p + nbytes <= cacheBlockEnd);
    cacheBlockCurrent += nbytes;

    ArenaMemoryTracking::ReportAllocation(this, p, nbytes);

    return(p);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
template <bool DoRecoverMemory>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
AllocFromHeap(size_t requestBytes)
{
    size_t allocBytes = AllocSizeMath::Add(requestBytes, sizeof(ArenaMemoryBlock));

    ARENA_FAULTINJECT_MEMORY(this->name, requestBytes);

    char * buffer = HeapNewNoThrowArray(char, allocBytes);

    if (buffer == nullptr)
    {
        if (DoRecoverMemory && recoverMemoryFunc)
        {
            // Try to recover some memory and see if after that we can allocate.
            recoverMemoryFunc();
            buffer = HeapNewNoThrowArray(char, allocBytes);
        }

        if (buffer == nullptr)
        {
            if (outOfMemoryFunc)
            {
                outOfMemoryFunc();
            }
            return nullptr;
        }
    }

    ArenaMemoryBlock * memoryBlock = (ArenaMemoryBlock *)buffer;
    memoryBlock->nbytes = requestBytes;
    memoryBlock->next = this->mallocBlocks;
    this->mallocBlocks = memoryBlock;
    this->blockState = 2;                       // set the block state to 2 to disable the reset fast path.

    ArenaMemoryTracking::ReportAllocation(this, buffer + sizeof(ArenaMemoryBlock), requestBytes);

    return buffer + sizeof(ArenaMemoryBlock);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
BigBlock *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
AddBigBlock(size_t requestBytes)
{

    FAULTINJECT_MEMORY_NOTHROW(this->name, requestBytes);

    size_t allocBytes = AllocSizeMath::Add(requestBytes, sizeof(BigBlock));

    PageAllocation * allocation = this->GetPageAllocator()->AllocPagesForBytes(allocBytes);

    if (allocation == nullptr)
    {
        // Try to recover some memory and see if after that we can allocate.
        if (recoverMemoryFunc)
        {
            recoverMemoryFunc();
            allocation = this->GetPageAllocator()->AllocPagesForBytes(allocBytes);
        }
        if (allocation == nullptr)
        {
            return nullptr;
        }
    }
    BigBlock * blockp = (BigBlock *)allocation->GetAddress();
    blockp->allocation = allocation;
    blockp->nbytes = allocation->GetSize() - sizeof(BigBlock);
    blockp->currentByte = 0;

#ifdef PROFILE_MEM
    LogRealAlloc(allocation->GetSize() + sizeof(PageAllocation));
#endif
    return(blockp);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
FullReset()
{
    BigBlock * initBlock = this->bigBlocks;
    if (initBlock != nullptr)
    {
        this->bigBlocks = initBlock->nextBigBlock;
    }
    Clear();
    if (initBlock != nullptr)
    {
        this->blockState = 1;
        initBlock->currentByte = 0;
        SetCacheBlock(initBlock);
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
ReleaseMemory()
{
    ReleasePageMemory();
    ReleaseHeapMemory();
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
ReleasePageMemory()
{
    pageAllocator->SuspendIdleDecommit();
#ifdef ARENA_MEMORY_VERIFY
    bool reenableDisablePageReuse = false;
    if (Js::Configuration::Global.flags.ArenaNoPageReuse)
    {
        reenableDisablePageReuse = !pageAllocator->DisablePageReuse();
    }
#endif
    BigBlock *blockp = bigBlocks;
    while (blockp != NULL)
    {
        PageAllocation * allocation = blockp->allocation;
        blockp = blockp->nextBigBlock;
        GetPageAllocator()->ReleaseAllocationNoSuspend(allocation);
    }

    blockp = fullBlocks;
    while (blockp != NULL)
    {
        PageAllocation * allocation = blockp->allocation;
        blockp = blockp->nextBigBlock;
        GetPageAllocator()->ReleaseAllocationNoSuspend(allocation);
    }

#ifdef ARENA_MEMORY_VERIFY
    if (reenableDisablePageReuse)
    {
        pageAllocator->ReenablePageReuse();
    }
#endif

    pageAllocator->ResumeIdleDecommit();
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
ReleaseHeapMemory()
{
    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {
        ArenaMemoryBlock * next = memoryBlock->next;
        HeapDeleteArray(memoryBlock->nbytes + sizeof(ArenaMemoryBlock), (char *)memoryBlock);
        memoryBlock = next;
    }
}

template __forceinline char *ArenaAllocatorBase<InPlaceFreeListPolicy, 0, 0, 0>::AllocInternal(size_t requestedBytes);
template __forceinline char *ArenaAllocatorBase<InPlaceFreeListPolicy, 3, 0, 0>::AllocInternal(size_t requestedBytes);

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
AllocInternal(size_t requestedBytes)
{
    Assert(requestedBytes != 0);

    if (MaxObjectSize > 0)
    {
        Assert(requestedBytes <= MaxObjectSize);
    }

    if (RequireObjectAlignment)
    {
        Assert(requestedBytes % ObjectAlignment == 0);
    }

    // If out of memory function is set, that means that the caller is a throwing allocation
    // routine, so we can throw from here. Otherwise, we shouldn't throw.
    ARENA_FAULTINJECT_MEMORY(this->name, requestedBytes);

    ASSERT_THREAD();

    size_t nbytes;
    if (freeList != nullptr && requestedBytes > 0 && requestedBytes <= ArenaAllocatorBase::MaxSmallObjectSize)
    {
        // We have checked the size requested, so no integer overflow check
        nbytes = Math::Align(requestedBytes, ArenaAllocator::ObjectAlignment);
        Assert(nbytes <= ArenaAllocator::MaxSmallObjectSize);
#ifdef PROFILE_MEM
        LogAlloc(requestedBytes, nbytes);
#endif
        void * freeObject = TFreeListPolicy::Allocate(this->freeList, nbytes);

        if (freeObject != nullptr)
        {
#ifdef ARENA_MEMORY_VERIFY
            TFreeListPolicy::VerifyFreeObjectIsFreeMemFilled(freeObject, nbytes);
#endif

#ifdef ARENA_ALLOCATOR_FREE_LIST_SIZE
            this->freeListSize -= nbytes;
#endif

#ifdef PROFILE_MEM
            LogReuse(nbytes);
#endif
            ArenaMemoryTracking::ReportAllocation(this, freeObject, nbytes);
            return (char *)freeObject;
        }
    }
    else
    {
        nbytes = AllocSizeMath::Align(requestedBytes, ArenaAllocator::ObjectAlignment);
#ifdef PROFILE_MEM
        LogAlloc(requestedBytes, nbytes);
#endif
    }
    // TODO: Support large object free listing
    return ArenaAllocatorBase::RealAllocInlined(nbytes);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
Free(void * buffer, size_t byteSize)
{
    ASSERT_THREAD();
    Assert(byteSize != 0);

    if (MaxObjectSize > 0)
    {
        Assert(byteSize <= MaxObjectSize);
    }

    if (RequireObjectAlignment)
    {
        Assert(byteSize % ObjectAlignment == 0);
    }

    // Since we successfully allocated, we shouldn't have integer overflow here
    size_t size = Math::Align(byteSize, ArenaAllocator::ObjectAlignment);
    Assert(size >= byteSize);

    ArenaMemoryTracking::ReportFree(this, buffer, byteSize);

#ifdef ARENA_MEMORY_VERIFY
    if (Js::Configuration::Global.flags.ArenaNoFreeList)
    {
        return;
    }
#endif
    if (buffer == cacheBlockCurrent - byteSize)
    {
#ifdef PROFILE_MEM
        LogFree(byteSize);
#endif
        cacheBlockCurrent = (char *)buffer;
        return;
    }
    else if (this->pageAllocator->IsClosed())
    {
        return;
    }
    else if (size <= ArenaAllocator::MaxSmallObjectSize)
    {
        // If we plan to free-list this object, we must prepare (typically, debug pattern fill) its memory here, in case we fail to allocate the free list because we're out of memory (see below),
        // and we never get to call TFreeListPolicy::Free.
        TFreeListPolicy::PrepareFreeObject(buffer, size);

        if (freeList == nullptr)
        {
            // Caution: TFreeListPolicy::New may fail silently if we're out of memory.
            freeList = TFreeListPolicy::New(this);

            if (freeList == nullptr)
            {
                return;
            }
        }

        this->freeList = TFreeListPolicy::Free(this->freeList, buffer, size);

#ifdef ARENA_ALLOCATOR_FREE_LIST_SIZE
        this->freeListSize += size;
#endif

#ifdef PROFILE_MEM
        LogFree(byteSize);
#endif
        return;
    }

    // TODO: Free list bigger objects
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
Realloc(void* buffer, size_t existingBytes, size_t requestedBytes)
{
    ASSERT_THREAD();

    if (existingBytes == 0)
    {
        Assert(buffer == nullptr);
        return AllocInternal(requestedBytes);
    }

    if (MaxObjectSize > 0)
    {
        Assert(requestedBytes <= MaxObjectSize);
    }

    if (RequireObjectAlignment)
    {
        Assert(requestedBytes % ObjectAlignment == 0);
    }

    size_t nbytes = AllocSizeMath::Align(requestedBytes, ArenaAllocator::ObjectAlignment);

    // Since we successfully allocated, we shouldn't have integer overflow here
    size_t nbytesExisting = Math::Align(existingBytes, ArenaAllocator::ObjectAlignment);
    Assert(nbytesExisting >= existingBytes);

    if (nbytes == nbytesExisting)
    {
        return (char *)buffer;
    }

    if (nbytes < nbytesExisting)
    {
        ArenaMemoryTracking::ReportReallocation(this, buffer, nbytesExisting, nbytes);

        Free(((char *)buffer) + nbytes, nbytesExisting - nbytes);
        return (char *)buffer;
    }

    char* replacementBuf = nullptr;
    if (requestedBytes > 0)
    {
        replacementBuf = AllocInternal(requestedBytes);
        if (replacementBuf != nullptr)
        {
            js_memcpy_s(replacementBuf, requestedBytes, buffer, existingBytes);
        }
    }

    if (nbytesExisting > 0)
    {
        Free(buffer, nbytesExisting);
    }

    return replacementBuf;
}

#ifdef PROFILE_MEM
template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogBegin()
{
    memoryData = MemoryProfiler::Begin(this->name);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogReset()
{
    if (memoryData)
    {
        MemoryProfiler::Reset(this->name, memoryData);
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogEnd()
{
    if (memoryData)
    {
        MemoryProfiler::End(this->name, memoryData);
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogAlloc(size_t requestedBytes, size_t allocateBytes)
{
    if (memoryData)
    {
        memoryData->requestCount++;
        memoryData->requestBytes += requestedBytes;
        memoryData->alignmentBytes += allocateBytes - requestedBytes;
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogRealAlloc(size_t size)
{
    if (memoryData)
    {
        memoryData->allocatedBytes += size;
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogFree(size_t size)
{
    if (memoryData)
    {
        memoryData->freelistBytes += size;
        memoryData->freelistCount++;
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogReuse(size_t size)
{
    if (memoryData)
    {
        memoryData->reuseCount++;
        memoryData->reuseBytes += size;
    }
}
#endif

void * InPlaceFreeListPolicy::New(ArenaAllocatorBase<InPlaceFreeListPolicy> * allocator)
{
    return AllocatorNewNoThrowNoRecoveryArrayZ(ArenaAllocator, allocator, FreeObject *, buckets);
}

void * InPlaceFreeListPolicy::Allocate(void * policy, size_t size)
{
    Assert(policy);

    FreeObject ** freeObjectLists = reinterpret_cast<FreeObject **>(policy);
    size_t index = (size >> ArenaAllocator::ObjectAlignmentBitShift) - 1;
    FreeObject * freeObject = freeObjectLists[index];

    if (NULL != freeObject)
    {
        freeObjectLists[index] = freeObject->next;

#ifdef ARENA_MEMORY_VERIFY
        // Make sure the next pointer bytes are also DbgFreeMemFill-ed.
        memset(freeObject, DbgFreeMemFill, sizeof(freeObject->next));
#endif
    }

    return freeObject;
}
void * InPlaceFreeListPolicy::Free(void * policy, void * object, size_t size)
{
    Assert(policy);

    FreeObject ** freeObjectLists = reinterpret_cast<FreeObject **>(policy);
    FreeObject * freeObject = reinterpret_cast<FreeObject *>(object);
    size_t index = (size >> ArenaAllocator::ObjectAlignmentBitShift) - 1;

    freeObject->next = freeObjectLists[index];
    freeObjectLists[index] = freeObject;
    return policy;
}

void * InPlaceFreeListPolicy::Reset(void * policy)
{
    return NULL;
}

#ifdef ARENA_MEMORY_VERIFY
void InPlaceFreeListPolicy::VerifyFreeObjectIsFreeMemFilled(void * object, size_t size)
{
    unsigned char * bytes = reinterpret_cast<unsigned char*>(object);
    for (size_t i = 0; i < size; i++)
    {
        Assert(bytes[i] == InPlaceFreeListPolicy::DbgFreeMemFill);
    }
}
#endif

template class ArenaAllocatorBase<InPlaceFreeListPolicy>;

void * StandAloneFreeListPolicy::New(ArenaAllocatorBase<StandAloneFreeListPolicy> * /*allocator*/)
{
    return NewInternal(InitialEntries);
}

void * StandAloneFreeListPolicy::Allocate(void * policy, size_t size)
{
    Assert(policy);

    StandAloneFreeListPolicy * _this = reinterpret_cast<StandAloneFreeListPolicy *>(policy);
    size_t index = (size >> ArenaAllocator::ObjectAlignmentBitShift) - 1;
    void * object = NULL;

    uint * freeObjectList = &_this->freeObjectLists[index];
    if (0 != *freeObjectList)
    {
        FreeObjectListEntry * entry = &_this->entries[*freeObjectList - 1];
        uint oldFreeList = _this->freeList;

        _this->freeList = *freeObjectList;
        *freeObjectList = entry->next;
        object = entry->object;
        Assert(object != NULL);
        entry->next = oldFreeList;
        entry->object = NULL;
    }

    return object;
}

void * StandAloneFreeListPolicy::Free(void * policy, void * object, size_t size)
{
    Assert(policy);

    StandAloneFreeListPolicy * _this = reinterpret_cast<StandAloneFreeListPolicy *>(policy);
    size_t index = (size >> ArenaAllocator::ObjectAlignmentBitShift) - 1;

    if (TryEnsureFreeListEntry(_this))
    {
        Assert(_this->freeList != 0);

        uint * freeObjectList = &_this->freeObjectLists[index];
        FreeObjectListEntry * entry = &_this->entries[_this->freeList - 1];
        uint oldFreeObjectList = *freeObjectList;

        *freeObjectList = _this->freeList;
        _this->freeList = entry->next;
        entry->object = object;
        entry->next = oldFreeObjectList;
    }

    return _this;
}

void * StandAloneFreeListPolicy::Reset(void * policy)
{
    Assert(policy);

    StandAloneFreeListPolicy * _this = reinterpret_cast<StandAloneFreeListPolicy *>(policy);
    HeapDeletePlus(GetPlusSize(_this), _this);

    return NULL;
}

#ifdef ARENA_MEMORY_VERIFY
void StandAloneFreeListPolicy::VerifyFreeObjectIsFreeMemFilled(void * object, size_t size)
{
    char * bytes = reinterpret_cast<char*>(object);
    for (size_t i = 0; i < size; i++)
    {
        Assert(bytes[i] == StandAloneFreeListPolicy::DbgFreeMemFill);
    }
}
#endif

void StandAloneFreeListPolicy::Release(void * policy)
{
    if (NULL != policy)
    {
        Reset(policy);
    }
}

StandAloneFreeListPolicy * StandAloneFreeListPolicy::NewInternal(uint entries)
{
    size_t plusSize = buckets * sizeof(uint) + entries * sizeof(FreeObjectListEntry);
    StandAloneFreeListPolicy * _this = HeapNewNoThrowPlusZ(plusSize, StandAloneFreeListPolicy);

    if (NULL != _this)
    {
        _this->allocated = entries;
        _this->freeObjectLists = (uint *)(_this + 1);
        _this->entries = (FreeObjectListEntry *)(_this->freeObjectLists + buckets);
    }

    return _this;
}

bool StandAloneFreeListPolicy::TryEnsureFreeListEntry(StandAloneFreeListPolicy *& _this)
{
    if (0 == _this->freeList)
    {
        if (_this->used < _this->allocated)
        {
            _this->used++;
            _this->freeList = _this->used;
        }
        else
        {
            Assert(_this->used == _this->allocated);

            StandAloneFreeListPolicy * oldThis = _this;
            uint entries = oldThis->allocated + min(oldThis->allocated, MaxEntriesGrowth);
            StandAloneFreeListPolicy * newThis = NewInternal(entries);
            if (NULL != newThis)
            {
                uint sizeInBytes = buckets * sizeof(uint);
                js_memcpy_s(newThis->freeObjectLists, sizeInBytes, oldThis->freeObjectLists, sizeInBytes);
                js_memcpy_s(newThis->entries, newThis->allocated * sizeof(FreeObjectListEntry), oldThis->entries, oldThis->used * sizeof(FreeObjectListEntry));
                newThis->used = oldThis->used + 1;
                newThis->freeList = newThis->used;
                _this = newThis;
                HeapDeletePlus(GetPlusSize(oldThis), oldThis);
            }
            else
            {
                return false;
            }
        }
    }

    return true;
}

template class ArenaAllocatorBase<StandAloneFreeListPolicy>;

#ifdef PERSISTENT_INLINE_CACHES

void * InlineCacheFreeListPolicy::New(ArenaAllocatorBase<InlineCacheAllocatorTraits> * allocator)
{
    return NewInternal();
}

InlineCacheFreeListPolicy * InlineCacheFreeListPolicy::NewInternal()
{
    InlineCacheFreeListPolicy * _this = HeapNewNoThrowZ(InlineCacheFreeListPolicy);
    return _this;
}

InlineCacheFreeListPolicy::InlineCacheFreeListPolicy()
{
    Assert(AreFreeListBucketsEmpty());
}

bool InlineCacheFreeListPolicy::AreFreeListBucketsEmpty()
{
    for (int b = 0; b < bucketCount; b++)
    {
        if (this->freeListBuckets[b] != 0) return false;
    }
    return true;
}

void * InlineCacheFreeListPolicy::Allocate(void * policy, size_t size)
{
    Assert(policy);

    FreeObject ** freeObjectLists = reinterpret_cast<FreeObject **>(policy);
    size_t index = (size >> InlineCacheAllocatorInfo::ObjectAlignmentBitShift) - 1;
    FreeObject * freeObject = freeObjectLists[index];

    if (NULL != freeObject)
    {
        freeObjectLists[index] = reinterpret_cast<FreeObject *>(reinterpret_cast<intptr>(freeObject->next) & ~InlineCacheFreeListTag);

#ifdef ARENA_MEMORY_VERIFY
        // Make sure the next pointer bytes are also DbgFreeMemFill-ed, before we give them out.
        memset(&freeObject->next, DbgFreeMemFill, sizeof(freeObject->next));
#endif
    }

    return freeObject;
}

void * InlineCacheFreeListPolicy::Free(void * policy, void * object, size_t size)
{
    Assert(policy);

    FreeObject ** freeObjectLists = reinterpret_cast<FreeObject **>(policy);
    FreeObject * freeObject = reinterpret_cast<FreeObject *>(object);
    size_t index = (size >> InlineCacheAllocatorInfo::ObjectAlignmentBitShift) - 1;

    freeObject->next = reinterpret_cast<FreeObject *>(reinterpret_cast<intptr>(freeObjectLists[index]) | InlineCacheFreeListTag);
    freeObjectLists[index] = freeObject;
    return policy;
}

void * InlineCacheFreeListPolicy::Reset(void * policy)
{
    Assert(policy);

    InlineCacheFreeListPolicy * _this = reinterpret_cast<InlineCacheFreeListPolicy *>(policy);
    HeapDelete(_this);

    return NULL;
}

#ifdef ARENA_MEMORY_VERIFY
void InlineCacheFreeListPolicy::VerifyFreeObjectIsFreeMemFilled(void * object, size_t size)
{
    unsigned char * bytes = reinterpret_cast<unsigned char*>(object);
    for (size_t i = 0; i < size; i++)
    {
        // We must allow for zero-filled free listed objects (at least their weakRefs/blankSlots bytes), because during garbage collection, we may zero out
        // some of the weakRefs (those that have become unreachable), and this is NOT a sign of "use after free" problem.  It would be nice if during collection
        // we could reliably distinguish free-listed objects from live caches, but that's not possible because caches can be allocated and freed in batches
        // (see more on that in comments inside InlineCacheFreeListPolicy::PrepareFreeObject).
        Assert(bytes[i] == NULL || bytes[i] == InlineCacheFreeListPolicy::DbgFreeMemFill);
    }
}
#endif

void InlineCacheFreeListPolicy::Release(void * policy)
{
    if (NULL != policy)
    {
        Reset(policy);
    }
}

template class ArenaAllocatorBase<InlineCacheAllocatorTraits>;

#if DBG
bool InlineCacheAllocator::IsAllZero()
{
    UpdateCacheBlock();

    // See InlineCacheAllocator::ZeroAll for why we ignore the strongRef slot of the CacheLayout.

    BigBlock *bigBlock = this->bigBlocks;
    while (bigBlock != NULL)
    {
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            unsigned char* weakRefBytes = (unsigned char *)cache->weakRefs;
            for (size_t i = 0; i < sizeof(cache->weakRefs); i++)
            {
                // If we're verifying arena memory (in debug builds) caches on the free list
                // will be debug pattern filled (specifically, at least their weak reference slots).
                // All other caches must be zeroed out (again, at least their weak reference slots).
#ifdef ARENA_MEMORY_VERIFY
                if (weakRefBytes[i] != NULL && weakRefBytes[i] != InlineCacheFreeListPolicy::DbgFreeMemFill)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#else
                if (weakRefBytes[i] != NULL)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#endif
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    bigBlock = this->fullBlocks;
    while (bigBlock != NULL)
    {
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            char* weakRefBytes = (char *)cache->weakRefs;
            for (size_t i = 0; i < sizeof(cache->weakRefs); i++)
            {
                // If we're verifying arena memory (in debug builds) caches on the free list
                // will be debug pattern filled (specifically, their weak reference slots).
                // All other caches must be zeroed out (again, their weak reference slots).
#ifdef ARENA_MEMORY_VERIFY
                if (weakRefBytes[i] != NULL && weakRefBytes[i] != InlineCacheFreeListPolicy::DbgFreeMemFill)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#else
                if (weakRefBytes[i] != NULL)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#endif
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {
        Assert(memoryBlock->nbytes % sizeof(CacheLayout) == 0);
        ArenaMemoryBlock * next = memoryBlock->next;
        CacheLayout* endPtr = (CacheLayout*)(memoryBlock->GetBytes() + memoryBlock->nbytes);
        for (CacheLayout* cache = (CacheLayout*)memoryBlock->GetBytes(); cache < endPtr; cache++)
        {
            unsigned char* weakRefBytes = (unsigned char *)cache->weakRefs;
            for (size_t i = 0; i < sizeof(cache->weakRefs); i++)
            {
#ifdef ARENA_MEMORY_VERIFY
                if (weakRefBytes[i] != NULL && weakRefBytes[i] != InlineCacheFreeListPolicy::DbgFreeMemFill)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#else
                if (weakRefBytes[i] != NULL)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#endif
            }
        }
        memoryBlock = next;
    }

    return true;
}
#endif

void InlineCacheAllocator::ZeroAll()
{
    UpdateCacheBlock();

    // We zero the weakRefs part of each cache in the arena unconditionally.  The strongRef slot is zeroed only
    // if it isn't tagged with InlineCacheFreeListTag.  That's so we don't lose our free list, which is
    // formed by caches linked via their strongRef slot tagged with InlineCacheFreeListTag.  On the other hand,
    // inline caches that require invalidation use the same slot as a pointer (untagged) to the cache's address
    // in the invalidation list.  Hence, we must zero the strongRef slot when untagged to ensure the cache
    // doesn't appear registered for invalidation when it's actually blank (which would trigger asserts in InlineCache::VerifyRegistrationForInvalidation).

    BigBlock *bigBlock = this->bigBlocks;
    while (bigBlock != NULL)
    {
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            memset(cache->weakRefs, 0, sizeof(cache->weakRefs));
            // We want to preserve the free list, whose next pointers are tagged with InlineCacheFreeListTag.
            if ((cache->strongRef & InlineCacheFreeListTag) == 0) cache->strongRef = 0;

            if (cache->weakRefs[0] != NULL || cache->weakRefs[1] != NULL || cache->weakRefs[2] != NULL)
            {
                AssertMsg(false, "Inline cache arena is not zeroed!");
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    bigBlock = this->fullBlocks;
    while (bigBlock != NULL)
    {
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            memset(cache->weakRefs, 0, sizeof(cache->weakRefs));
            // We want to preserve the free list, whose next pointers are tagged with InlineCacheFreeListTag.
            if ((cache->strongRef & InlineCacheFreeListTag) == 0) cache->strongRef = 0;

            if (cache->weakRefs[0] != NULL || cache->weakRefs[1] != NULL || cache->weakRefs[2] != NULL)
            {
                AssertMsg(false, "Inline cache arena is not zeroed!");
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {
        Assert(memoryBlock->nbytes % sizeof(CacheLayout) == 0);
        ArenaMemoryBlock * next = memoryBlock->next;
        CacheLayout* endPtr = (CacheLayout*)(memoryBlock->GetBytes() + memoryBlock->nbytes);
        for (CacheLayout* cache = (CacheLayout*)memoryBlock->GetBytes(); cache < endPtr; cache++)
        {
            memset(cache->weakRefs, 0, sizeof(cache->weakRefs));
            // We want to preserve the free list, whose next pointers are tagged with InlineCacheFreeListTag.
            if ((cache->strongRef & InlineCacheFreeListTag) == 0) cache->strongRef = 0;

            if (cache->weakRefs[0] != NULL || cache->weakRefs[1] != NULL || cache->weakRefs[2] != NULL)
            {
                AssertMsg(false, "Inline cache arena is not zeroed!");
            }
        }
        memoryBlock = next;
    }
}

bool InlineCacheAllocator::IsDeadWeakRef(Recycler* recycler, void* ptr)
{
    return recycler->IsObjectMarked(ptr);
}

bool InlineCacheAllocator::CacheHasDeadWeakRefs(Recycler* recycler, CacheLayout* cache)
{
    for (intptr* curWeakRefPtr = cache->weakRefs; curWeakRefPtr < &cache->strongRef; curWeakRefPtr++)
    {
        intptr curWeakRef = *curWeakRefPtr;

        if (curWeakRef == 0)
        {
            continue;
        }

        curWeakRef &= ~(intptr)InlineCacheAuxSlotTypeTag;

        if ((curWeakRef & (HeapConstants::ObjectGranularity - 1)) != 0)
        {
            continue;
        }


        if (!recycler->IsObjectMarked((void*)curWeakRef))
        {
            return true;
        }
    }

    return false;
}

bool InlineCacheAllocator::HasNoDeadWeakRefs(Recycler* recycler)
{
    UpdateCacheBlock();

    BigBlock *bigBlock = this->bigBlocks;
    while (bigBlock != NULL)
    {
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            if (CacheHasDeadWeakRefs(recycler, cache))
            {
                return false;
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }
    bigBlock = this->fullBlocks;
    while (bigBlock != NULL)
    {
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            if (CacheHasDeadWeakRefs(recycler, cache))
            {
                return false;
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {
        Assert(memoryBlock->nbytes % sizeof(CacheLayout) == 0);
        ArenaMemoryBlock * next = memoryBlock->next;
        CacheLayout* endPtr = (CacheLayout*)(memoryBlock->GetBytes() + memoryBlock->nbytes);
        for (CacheLayout* cache = (CacheLayout*)memoryBlock->GetBytes(); cache < endPtr; cache++)
        {
            if (CacheHasDeadWeakRefs(recycler, cache))
            {
                return false;
            }
        }
        memoryBlock = next;
    }

    return true;
}

void InlineCacheAllocator::ClearCacheIfHasDeadWeakRefs(Recycler* recycler, CacheLayout* cache)
{
    for (intptr* curWeakRefPtr = cache->weakRefs; curWeakRefPtr < &cache->strongRef; curWeakRefPtr++)
    {
        intptr curWeakRef = *curWeakRefPtr;

        if (curWeakRef == 0)
        {
            continue;
        }

        curWeakRef &= ~(intptr)InlineCacheAuxSlotTypeTag;

        if ((curWeakRef & (HeapConstants::ObjectGranularity - 1)) != 0)
        {
            continue;
        }

        if (!recycler->IsObjectMarked((void*)curWeakRef))
        {
            cache->weakRefs[0] = 0;
            cache->weakRefs[1] = 0;
            cache->weakRefs[2] = 0;
            break;
        }
    }
}

void InlineCacheAllocator::ClearCachesWithDeadWeakRefs(Recycler* recycler)
{
    UpdateCacheBlock();

    BigBlock *bigBlock = this->bigBlocks;
    while (bigBlock != NULL)
    {
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            ClearCacheIfHasDeadWeakRefs(recycler, cache);
        }
        bigBlock = bigBlock->nextBigBlock;
    }
    bigBlock = this->fullBlocks;
    while (bigBlock != NULL)
    {
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            ClearCacheIfHasDeadWeakRefs(recycler, cache);
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {
        Assert(memoryBlock->nbytes % sizeof(CacheLayout) == 0);
        ArenaMemoryBlock * next = memoryBlock->next;
        CacheLayout* endPtr = (CacheLayout*)(memoryBlock->GetBytes() + memoryBlock->nbytes);
        for (CacheLayout* cache = (CacheLayout*)memoryBlock->GetBytes(); cache < endPtr; cache++)
        {
            ClearCacheIfHasDeadWeakRefs(recycler, cache);
        }
        memoryBlock = next;
    }
}

#else

template class ArenaAllocatorBase<InlineCacheAllocatorTraits>;

#if DBG
bool InlineCacheAllocator::IsAllZero()
{
    UpdateCacheBlock();
    BigBlock *blockp = this->bigBlocks;
    while (blockp != NULL)
    {
        for (size_t i = 0; i < blockp->currentByte; i++)
        {
            if (blockp->GetBytes()[i] != 0)
            {
                return false;
            }
        }

        blockp = blockp->nextBigBlock;
    }
    blockp = this->fullBlocks;
    while (blockp != NULL)
    {
        for (size_t i = 0; i < blockp->currentByte; i++)
        {
            if (blockp->GetBytes()[i] != 0)
            {
                return false;
            }
        }
        blockp = blockp->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {
        ArenaMemoryBlock * next = memoryBlock->next;
        for (size_t i = 0; i < memoryBlock->nbytes; i++)
        {
            if (memoryBlock->GetBytes()[i] != 0)
            {
                return false;
            }
        }
        memoryBlock = next;
    }
    return true;
}
#endif

void InlineCacheAllocator::ZeroAll()
{
    UpdateCacheBlock();
    BigBlock *blockp = this->bigBlocks;
    while (blockp != NULL)
    {
        memset(blockp->GetBytes(), 0, blockp->currentByte);
        blockp = blockp->nextBigBlock;
    }
    blockp = this->fullBlocks;
    while (blockp != NULL)
    {
        memset(blockp->GetBytes(), 0, blockp->currentByte);
        blockp = blockp->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {
        ArenaMemoryBlock * next = memoryBlock->next;
        memset(memoryBlock->GetBytes(), 0, memoryBlock->nbytes);
        memoryBlock = next;
    }
}

#endif

template class ArenaAllocatorBase<IsInstInlineCacheAllocatorTraits>;

#if DBG
bool IsInstInlineCacheAllocator::IsAllZero()
{
    UpdateCacheBlock();
    BigBlock *blockp = this->bigBlocks;
    while (blockp != NULL)
    {
        for (size_t i = 0; i < blockp->currentByte; i++)
        {
            if (blockp->GetBytes()[i] != 0)
            {
                return false;
            }
        }

        blockp = blockp->nextBigBlock;
    }
    blockp = this->fullBlocks;
    while (blockp != NULL)
    {
        for (size_t i = 0; i < blockp->currentByte; i++)
        {
            if (blockp->GetBytes()[i] != 0)
            {
                return false;
            }
        }
        blockp = blockp->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {
        ArenaMemoryBlock * next = memoryBlock->next;
        for (size_t i = 0; i < memoryBlock->nbytes; i++)
        {
            if (memoryBlock->GetBytes()[i] != 0)
            {
                return false;
            }
        }
        memoryBlock = next;
    }
    return true;
}
#endif

void IsInstInlineCacheAllocator::ZeroAll()
{
    UpdateCacheBlock();
    BigBlock *blockp = this->bigBlocks;
    while (blockp != NULL)
    {
        memset(blockp->GetBytes(), 0, blockp->currentByte);
        blockp = blockp->nextBigBlock;
    }
    blockp = this->fullBlocks;
    while (blockp != NULL)
    {
        memset(blockp->GetBytes(), 0, blockp->currentByte);
        blockp = blockp->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {
        ArenaMemoryBlock * next = memoryBlock->next;
        memset(memoryBlock->GetBytes(), 0, memoryBlock->nbytes);
        memoryBlock = next;
    }
}

#undef ASSERT_TRHEAD
