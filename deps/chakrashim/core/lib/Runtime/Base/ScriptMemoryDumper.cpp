//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
#include "Base\ScriptMemoryDumper.h"

ScriptMemoryDumper::ScriptMemoryDumper(Js::ScriptContext* scriptContext)
    :scriptContext(scriptContext)
{
    memset(&current, 0, sizeof(current));
    memset(&total, 0, sizeof(total));
    Init();
}


void ScriptMemoryDumper::Init()
{
    pageCountId = scriptContext->GetOrAddPropertyIdTracked(L"pageCount");
    objectSizeId = scriptContext->GetOrAddPropertyIdTracked(L"objectSize");
    freeObjectCountId = scriptContext->GetOrAddPropertyIdTracked(L"freeObjectCount");
    activeObjectCountId = scriptContext->GetOrAddPropertyIdTracked(L"activeObjectCount");
    totalByteCountId = scriptContext->GetOrAddPropertyIdTracked(L"totalByteCount");
    finalizeCountId = scriptContext->GetOrAddPropertyIdTracked(L"finalizeCount");
    weakReferenceCountId = scriptContext->GetOrAddPropertyIdTracked(L"weakReferenceCount");
    largeObjectsId = scriptContext->GetOrAddPropertyIdTracked(L"largeObjects");
    activeObjectByteSizeId = scriptContext->GetOrAddPropertyIdTracked(L"activeObjectByteSize");
    summaryId = scriptContext->GetOrAddPropertyIdTracked(L"summary");
    dumpObject = scriptContext->GetLibrary()->CreateObject();
}

// Export script related memory to javascript object containing related information.
Js::Var ScriptMemoryDumper::Dump()
{
    Recycler* recycler = scriptContext->GetRecycler();
    HeapInfo* heapInfo = recycler->GetAutoHeap();

    for (uint32 i = 0 ; i < HeapConstants::BucketCount; i++)
    {
        ResetCurrentStats();
        size_t sizeCat = (i + 1) * HeapConstants::ObjectGranularity;
        DumpHeapBucket(i, &heapInfo->GetBucket<LeafBit>(sizeCat));
        DumpHeapBucket(i, &heapInfo->GetBucket<NoBit>(sizeCat));
        DumpHeapBucket(i, (SmallFinalizableHeapBucket *)&heapInfo->GetBucket<FinalizeBit>(sizeCat));
        SaveCurrentAtIndex(i);
        MergeCurrentStats();
    }

#ifdef BUCKETIZE_MEDIUM_ALLOCATIONS
    for (uint32 i = 0 ; i < HeapConstants::MediumBucketCount; i++)
    {
        ResetCurrentStats();
        size_t sizeCat = HeapConstants::MaxSmallObjectSize + ((i + 1) * HeapConstants::ObjectGranularity);

#if SMALLBLOCK_MEDIUM_ALLOC
        DumpHeapBucket(i, &heapInfo->GetMediumBucket<LeafBit>(sizeCat));
        DumpHeapBucket(i, &heapInfo->GetMediumBucket<NoBit>(sizeCat));
        DumpHeapBucket(i, (MediumFinalizableHeapBucket *)&heapInfo->GetMediumBucket<FinalizeBit>(sizeCat));
#else
        DumpLargeBucket(&heapInfo->GetMediumBucket(sizeCat));
#endif
        SaveCurrentAtIndex(i + HeapConstants::BucketCount);
        MergeCurrentStats();
    }
#endif

    ResetCurrentStats();
    DumpLargeBucket(&heapInfo->largeObjectBucket);
    SaveCurrentAsLargeBlock();
    MergeCurrentStats();
    SaveSummary();
    return dumpObject;
}

template <typename TBlockType>
void ScriptMemoryDumper::DumpHeapBucket(uint index, HeapBucketT<TBlockType>* heapBucket)
{
    SmallHeapBlockAllocator<TBlockType> * currentAllocator = heapBucket->GetAllocator();
    do
    {
        DumpSmallHeapBlock(currentAllocator->GetHeapBlock());
        currentAllocator = currentAllocator->GetNext();
    }
    while (currentAllocator != heapBucket->GetAllocator());

    DumpSmallHeapBlockList(heapBucket->fullBlockList);
    DumpSmallHeapBlockList(heapBucket->heapBlockList);
}

template <typename TBlockType, typename TBlockAttributes>
void ScriptMemoryDumper::DumpHeapBucket(uint index, SmallNormalHeapBucketBase<TBlockType>* heapBucket)
{
    DumpHeapBucket(index, (HeapBucketT<TBlockType> *)heapBucket);
    DumpSmallHeapBlockList(((SmallNormalHeapBucketBase<TBlockType, TBlockAttributes> *)heapBucket)->partialHeapBlockList);
    DumpSmallHeapBlockList(((SmallNormalHeapBucketBase<TBlockType, TBlockAttributes> *)heapBucket)->partialSweptHeapBlockList);
}

template <typename TBlockAttributes>
void ScriptMemoryDumper::DumpHeapBucket(uint index, SmallFinalizableHeapBucketT<TBlockAttributes>* heapBucket)
{
    DumpHeapBucket(index, (SmallNormalHeapBucketBase<SmallFinalizableHeapBlockT<TBlockAttributes>> *)heapBucket);
    DumpSmallHeapBlockList(heapBucket->pendingDisposeList);
}

template <class TBlockAttributes>
void ScriptMemoryDumper::DumpSmallHeapBlockList(SmallHeapBlockT<TBlockAttributes>* heapBlockHead)
{
    HeapBlockList::ForEach(heapBlockHead, [this](SmallHeapBlockT<TBlockAttributes> * heapBlock)
    {
        DumpSmallHeapBlock(heapBlock);
    });
}

template <class TBlockAttributes>
void ScriptMemoryDumper::DumpSmallHeapBlock(SmallHeapBlockT<TBlockAttributes>* heapBlock)
{
    if (heapBlock == nullptr)
        return;

    if (current.objectSize == 0)
    {
        current.objectSize = heapBlock->objectSize;
    }
    Assert(current.objectSize == heapBlock->GetObjectSize());
    current.freeObjectCount = heapBlock->freeCount;
    current.activeObjectCount += heapBlock->objectCount - heapBlock->freeCount;
    if (heapBlock->IsAnyFinalizableBlock())
    {
        current.finalizeCount += heapBlock->AsFinalizableBlock<TBlockAttributes>()->finalizeCount;
    }
    current.pageCount += heapBlock->GetPageCount();
    current.totalByteCount += heapBlock->GetPageCount() * AutoSystemInfo::PageSize;
    current.activeObjectByteSize += (heapBlock->objectCount - heapBlock->freeCount)* current.objectSize;
}

void ScriptMemoryDumper::DumpLargeHeapBlockList(LargeHeapBlock* heapBlockHead)
{
    HeapBlockList::ForEach(heapBlockHead, [this](LargeHeapBlock * heapBlock)
    {
        DumpLargeHeapBlock(heapBlock);
    });
}

void ScriptMemoryDumper::DumpLargeBucket(LargeHeapBucket* heapBucket)
{
    DumpLargeHeapBlockList(heapBucket->fullLargeBlockList);
    DumpLargeHeapBlockList(heapBucket->largeBlockList);
#ifdef RECYCLER_PAGE_HEAP
    DumpLargeHeapBlockList(heapBucket->largePageHeapBlockList);
#endif
    DumpLargeHeapBlockList(heapBucket->pendingDisposeLargeBlockList);
    DumpLargeHeapBlockList(heapBucket->pendingSweepLargeBlockList);
#if defined(PARTIAL_GC_ENABLED) && defined(CONCURRENT_GC_ENABLED)
    DumpLargeHeapBlockList(heapBucket->partialSweptLargeBlockList);
#endif
}

struct LargeObjectHeader;
void ScriptMemoryDumper::DumpLargeHeapBlock(LargeHeapBlock* heapBlock)
{
    if (heapBlock == nullptr)
        return;

    current.finalizeCount += heapBlock->finalizeCount;
    current.pageCount += heapBlock->GetPageCount();
    current.totalByteCount += heapBlock->GetPageCount() * AutoSystemInfo::PageSize;

    for (uint32 i = 0; i < heapBlock->allocCount; i++)
    {
        Memory::LargeObjectHeader* heapHeader = heapBlock->GetHeader(i);
        if (heapHeader != nullptr)
        {
            current.activeObjectCount++;
            current.activeObjectByteSize += heapHeader->objectSize;
        }
    }
}

inline void ScriptMemoryDumper::ResetCurrentStats()
{
    memset(&current, 0, sizeof(current));
}

inline void ScriptMemoryDumper::MergeCurrentStats()
{
    total.pageCount += current.pageCount;
    total.activeObjectCount += current.activeObjectCount;
    total.activeObjectByteSize += current.activeObjectByteSize;
    total.finalizeCount += current.finalizeCount;
    total.totalByteCount += current.totalByteCount;
    total.freeObjectCount += current.freeObjectCount;
}

void ScriptMemoryDumper::SaveCurrentAtIndex(uint32 index)
{
    Js::DynamicObject* currentBucket = scriptContext->GetLibrary()->CreateObject();
    FillObjectWithStats(currentBucket, current);
    dumpObject->SetItem(index, currentBucket, Js::PropertyOperation_None);
}

void ScriptMemoryDumper::SaveCurrentAsLargeBlock()
{
    Js::DynamicObject* largeObjectStat = scriptContext->GetLibrary()->CreateObject();
    FillObjectWithStats(largeObjectStat, current);
    dumpObject->SetProperty(largeObjectsId, largeObjectStat, Js::PropertyOperation_None, NULL);
}

void ScriptMemoryDumper::SaveSummary()
{
    Js::DynamicObject* summaryStat = scriptContext->GetLibrary()->CreateObject();
    FillObjectWithStats(summaryStat, total);
    dumpObject->SetProperty(summaryId, summaryStat, Js::PropertyOperation_None, NULL);
}

void ScriptMemoryDumper::FillObjectWithStats(Js::DynamicObject* dynamicObject, HeapStats stats)
{
    dynamicObject->SetProperty(pageCountId, Js::JavascriptUInt64Number::ToVar(stats.pageCount, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(objectSizeId, Js::JavascriptNumber::New(stats.objectSize, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(freeObjectCountId, Js::JavascriptNumber::New(stats.freeObjectCount, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(activeObjectCountId, Js::JavascriptNumber::New(stats.activeObjectCount, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(activeObjectByteSizeId, Js::JavascriptUInt64Number::ToVar(stats.activeObjectByteSize, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(totalByteCountId, Js::JavascriptUInt64Number::ToVar(stats.totalByteCount, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(finalizeCountId, Js::JavascriptNumber::New(stats.finalizeCount, scriptContext),  Js::PropertyOperation_None, NULL);
}

#endif ENABLE_DEBUG_CONFIG_OPTIONS
