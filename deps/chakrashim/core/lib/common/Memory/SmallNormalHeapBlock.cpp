//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

template <class TBlockAttributes>
SmallNormalHeapBlockT<TBlockAttributes> *
SmallNormalHeapBlockT<TBlockAttributes>::New(HeapBucketT<SmallNormalHeapBlockT<TBlockAttributes>> * bucket)
{
    CompileAssert(TBlockAttributes::MaxObjectSize <= USHRT_MAX);
    Assert(bucket->sizeCat <= TBlockAttributes::MaxObjectSize);
    Assert((TBlockAttributes::PageCount * AutoSystemInfo::PageSize) / bucket->sizeCat <= USHRT_MAX);

    ushort objectSize = (ushort)bucket->sizeCat;
    ushort objectCount = (ushort)(TBlockAttributes::PageCount * AutoSystemInfo::PageSize) / objectSize;

    HeapBlockType blockType = (TBlockAttributes::IsSmallBlock ? SmallNormalBlockType : MediumNormalBlockType);
    return NoMemProtectHeapNewNoThrowPlusPrefixZ(GetAllocPlusSize(objectCount), SmallNormalHeapBlockT<TBlockAttributes>, bucket, objectSize, objectCount, blockType);
}

#ifdef RECYCLER_WRITE_BARRIER
template <class TBlockAttributes>
SmallNormalWithBarrierHeapBlockT<TBlockAttributes> *
SmallNormalWithBarrierHeapBlockT<TBlockAttributes>::New(HeapBucketT<SmallNormalWithBarrierHeapBlockT<TBlockAttributes>> * bucket)
{
    CompileAssert(TBlockAttributes::MaxObjectSize <= USHRT_MAX);
    Assert(bucket->sizeCat <= TBlockAttributes::MaxObjectSize);
    Assert((TBlockAttributes::PageCount * AutoSystemInfo::PageSize) / bucket->sizeCat <= USHRT_MAX);

    ushort objectSize = (ushort)bucket->sizeCat;
    ushort objectCount = (ushort)(TBlockAttributes::PageCount * AutoSystemInfo::PageSize) / objectSize;

    HeapBlockType blockType = (TBlockAttributes::IsSmallBlock ? SmallNormalBlockWithBarrierType : MediumNormalBlockWithBarrierType);
    return NoMemProtectHeapNewNoThrowPlusPrefixZ(GetAllocPlusSize(objectCount), SmallNormalWithBarrierHeapBlockT<TBlockAttributes>, bucket, objectSize, objectCount, blockType);
}
#endif

template <class TBlockAttributes>
void
SmallNormalHeapBlockT<TBlockAttributes>::Delete(SmallNormalHeapBlockT<TBlockAttributes> * heapBlock)
{
    Assert(heapBlock->IsNormalBlock());
    NoMemProtectHeapDeletePlusPrefix(GetAllocPlusSize(heapBlock->objectCount), heapBlock);
}

#ifdef RECYCLER_WRITE_BARRIER
template <class TBlockAttributes>
void
SmallNormalWithBarrierHeapBlockT<TBlockAttributes>::Delete(SmallNormalWithBarrierHeapBlockT<TBlockAttributes> * heapBlock)
{
    Assert(heapBlock->IsNormalWriteBarrierBlock());
    NoMemProtectHeapDeletePlusPrefix(GetAllocPlusSize(heapBlock->objectCount), heapBlock);
}
#endif

template <class TBlockAttributes>
SmallNormalHeapBlockT<TBlockAttributes>::SmallNormalHeapBlockT(HeapBucket * bucket, ushort objectSize, ushort objectCount, HeapBlockType heapBlockType)
    : SmallHeapBlockT<TBlockAttributes>(bucket, objectSize, objectCount, heapBlockType)
{
}

template <>
SmallNormalHeapBlockT<SmallAllocationBlockAttributes>::SmallNormalHeapBlockT(HeapBucketT<SmallNormalHeapBlockT<SmallAllocationBlockAttributes>> * bucket, ushort objectSize, ushort objectCount)
: SmallHeapBlockT<SmallAllocationBlockAttributes>(bucket, objectSize, objectCount, SmallNormalBlockType)
{
}

template <>
SmallNormalHeapBlockT<MediumAllocationBlockAttributes>::SmallNormalHeapBlockT(HeapBucketT<SmallNormalHeapBlockT<MediumAllocationBlockAttributes>> * bucket, ushort objectSize, ushort objectCount)
: SmallHeapBlockT<MediumAllocationBlockAttributes>(bucket, objectSize, objectCount, MediumNormalBlockType)
{
}

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)

template <class TBlockAttributes>
void
SmallNormalHeapBlockT<TBlockAttributes>::ScanInitialImplicitRoots(Recycler * recycler)
{
    Assert(IsAnyNormalBlock());

    uint const localObjectCount = this->objectCount;
    uint const localObjectSize = this->GetObjectSize();

    // We can use the markCount to optimize the scan because we update it in ResetMark
    if (this->markCount == localObjectCount
#ifdef RECYCLER_DUMP_OBJECT_GRAPH
        && recycler->objectGraphDumper == nullptr
#endif
        )
    {
        // TODO: only interior?
        recycler->ScanObjectInlineInterior((void **)this->GetAddress(), localObjectSize * localObjectCount);
    }
    else if (this->markCount != 0)
    {
        uint const localObjectBitDelta = this->GetObjectBitDelta();
        for (uint i = 0; i < localObjectCount; i++)
        {
            if (this->GetMarkedBitVector()->Test(i * localObjectBitDelta))
            {
                // TODO: only interior?
                void ** address = (void **)(this->GetAddress() + i * localObjectSize);
                DUMP_IMPLICIT_ROOT(recycler, address);
                recycler->ScanObjectInlineInterior(address, localObjectSize);
            }
        }
    }
}

template <class TBlockAttributes>
void
SmallNormalHeapBlockT<TBlockAttributes>::ScanNewImplicitRoots(Recycler * recycler)
{
    Assert(IsAnyNormalBlock());
    __super::ScanNewImplicitRootsBase([recycler](void * objectAddress, size_t objectSize)
    {
        // TODO: only interior?
        recycler->ScanObjectInlineInterior((void **)objectAddress, objectSize);
    });
}

// static
template <class TBlockAttributes>
bool
SmallNormalHeapBlockT<TBlockAttributes>::RescanObject(SmallNormalHeapBlockT<TBlockAttributes>* block,
    __in_ecount(localObjectSize) char * objectAddress, uint localObjectSize,
    uint objectIndex, Recycler * recycler)
{
    // REVIEW: This would be a good assert to have but we don't have the heap block here
    // Assert(block->GetAddressIndex(objectAddress) != SmallHeapBlockT<TBlockAttributes>::InvalidAddressBit);

    if (!recycler->AddMark(objectAddress, localObjectSize))
    {
        // Failed to add to the mark stack due to OOM.
        return false;
    }

    RECYCLER_STATS_INC(recycler, markData.rescanObjectCount);
    RECYCLER_STATS_ADD(recycler, markData.rescanObjectByteCount, localObjectSize);

    return true;
}

// static
template <class TBlockAttributes>
bool
SmallNormalHeapBlockT<TBlockAttributes>::CanRescanFullBlock()
{
    return true;
}

template <class TBlockAttributes>
uint
SmallNormalHeapBlockT<TBlockAttributes>::CalculateMarkCountForPage(SmallHeapBlockBitVector * markBits, uint bucketIndex, uint pageStartBitIndex)
{
    SmallHeapBlockBitVector temp;
    SmallHeapBlockBitVector const* invalid = HeapInfo::GetInvalidBitVectorForBucket<TBlockAttributes>(bucketIndex);

    // Remove any invalid bits from the calculation
    temp.Copy(markBits);
    temp.Minus(invalid);

    Assert(pageStartBitIndex % HeapBlockMap32::PageMarkBitCount == 0);
    uint rescanMarkCount = temp.GetRange<HeapBlockMap32::PageMarkBitCount>(pageStartBitIndex)->Count();

    // If the first object on the page is not at the start of the page, then the object containing
    // the first few bytes of the page is not included in this mark count
    // The caller will have to account for this
    return rescanMarkCount;
}

#endif

#if defined(PARTIAL_GC_ENABLED) && defined(CONCURRENT_GC_ENABLED)
template <class TBlockAttributes>
void
SmallNormalHeapBlockT<TBlockAttributes>::FinishPartialCollect()
{
    // We don't allocate from a partially swept block
    Assert(this->IsFreeBitsValid());

    RECYCLER_SLOW_CHECK(CheckFreeBitVector(true));
}
#endif

#ifdef RECYCLER_SLOW_CHECK_ENABLED
template <class TBlockAttributes>
bool
SmallNormalHeapBlockT<TBlockAttributes>::GetFreeObjectListOnAllocator(FreeObject ** freeObjectList)
{
    return GetFreeObjectListOnAllocatorImpl<SmallNormalHeapBlockT<TBlockAttributes>>(freeObjectList);
}
#endif

template class SmallNormalHeapBlockT<SmallAllocationBlockAttributes>;
template class SmallNormalHeapBlockT<MediumAllocationBlockAttributes>;
#ifdef RECYCLER_WRITE_BARRIER
template class SmallNormalWithBarrierHeapBlockT<SmallAllocationBlockAttributes>;
template class SmallNormalWithBarrierHeapBlockT<MediumAllocationBlockAttributes>;
#endif
