//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
template <class TBlockAttributes>
class SmallNormalHeapBlockT : public SmallHeapBlockT<TBlockAttributes>
{
    friend class HeapBucketT<SmallNormalHeapBlockT>;
public:
    typedef TBlockAttributes HeapBlockAttributes;
    static const ObjectInfoBits RequiredAttributes = NoBit;
    static const bool IsLeafOnly = false;

    static SmallNormalHeapBlockT * New(HeapBucketT<SmallNormalHeapBlockT> * bucket);
    static void Delete(SmallNormalHeapBlockT * block);

    SmallNormalHeapBlockT * GetNextBlock() const { return __super::GetNextBlock()->AsNormalBlock<TBlockAttributes>(); }
    void SetNextBlock(SmallNormalHeapBlockT * next) { __super::SetNextBlock(next); }

    void ScanInitialImplicitRoots(Recycler * recycler);
    void ScanNewImplicitRoots(Recycler * recycler);

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    static uint CalculateMarkCountForPage(SmallHeapBlockBitVector* markBits, uint bucketIndex, uint pageStartBitIndex);

    static bool CanRescanFullBlock();
    static bool RescanObject(SmallNormalHeapBlockT<TBlockAttributes> * block, __in_ecount(localObjectSize) char * objectAddress, uint localObjectSize, uint objectIndex, Recycler * recycler);
#endif
#if defined(PARTIAL_GC_ENABLED) && defined(CONCURRENT_GC_ENABLED)
    void FinishPartialCollect();
#endif

#ifdef RECYCLER_SLOW_CHECK_ENABLED
    virtual bool GetFreeObjectListOnAllocator(FreeObject ** freeObjectList) override;
#endif

    virtual bool FindHeapObject(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject) override
    {
        return FindHeapObjectImpl<SmallNormalHeapBlockT>(objectAddress, recycler, flags, heapObject);
    }
protected:
    SmallNormalHeapBlockT(HeapBucket * bucket, ushort objectSize, ushort objectCount, HeapBlockType heapBlockType);

private:
    SmallNormalHeapBlockT(HeapBucketT<SmallNormalHeapBlockT> * bucket, ushort objectSize, ushort objectCount);

};

#ifdef RECYCLER_WRITE_BARRIER
template <class TBlockAttributes>
class SmallNormalWithBarrierHeapBlockT : public SmallNormalHeapBlockT<TBlockAttributes>
{
    friend class HeapBucketT<SmallNormalWithBarrierHeapBlockT>;
public:
    typedef TBlockAttributes HeapBlockAttributes;
    static const ObjectInfoBits RequiredAttributes = WithBarrierBit;
    static const bool IsLeafOnly = false;

    static SmallNormalWithBarrierHeapBlockT * New(HeapBucketT<SmallNormalWithBarrierHeapBlockT> * bucket);
    static void Delete(SmallNormalWithBarrierHeapBlockT * heapBlock);

    SmallNormalWithBarrierHeapBlockT * GetNextBlock() const { return ((SmallHeapBlock*) this)->GetNextBlock()->AsNormalWriteBarrierBlock<TBlockAttributes>(); }

    virtual bool FindHeapObject(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject) override sealed
    {
        return FindHeapObjectImpl<SmallNormalWithBarrierHeapBlockT>(objectAddress, recycler, flags, heapObject);
    }
protected:
    SmallNormalWithBarrierHeapBlockT(HeapBucket * bucket, ushort objectSize, ushort objectCount, HeapBlockType heapBlockType) :
        SmallNormalHeapBlockT(bucket, objectSize, objectCount, heapBlockType)
    {}

};
#endif

extern template class SmallNormalHeapBlockT<SmallAllocationBlockAttributes>;
extern template class SmallNormalHeapBlockT<MediumAllocationBlockAttributes>;

typedef SmallNormalHeapBlockT<SmallAllocationBlockAttributes> SmallNormalHeapBlock;
typedef SmallNormalHeapBlockT<MediumAllocationBlockAttributes> MediumNormalHeapBlock;

#ifdef RECYCLER_WRITE_BARRIER
extern template class SmallNormalWithBarrierHeapBlockT<SmallAllocationBlockAttributes>;
extern template class SmallNormalWithBarrierHeapBlockT<MediumAllocationBlockAttributes>;

typedef SmallNormalWithBarrierHeapBlockT<SmallAllocationBlockAttributes> SmallNormalWithBarrierHeapBlock;
typedef SmallNormalWithBarrierHeapBlockT<MediumAllocationBlockAttributes> MediumNormalWithBarrierHeapBlock;
#endif
}
