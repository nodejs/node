//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
template <class TBlockAttributes>
class SmallLeafHeapBlockT : public SmallHeapBlockT<TBlockAttributes>
{
    friend class HeapBucketT<SmallLeafHeapBlockT>;
public:
    static const ObjectInfoBits RequiredAttributes = LeafBit;
    typedef TBlockAttributes HeapBlockAttributes;

    SmallLeafHeapBlockT * GetNextBlock() const { return __super::GetNextBlock()->AsLeafBlock<TBlockAttributes>(); }
    void SetNextBlock(SmallLeafHeapBlockT * next) { __super::SetNextBlock(next); }

    void ScanNewImplicitRoots(Recycler * recycler);

    static SmallLeafHeapBlockT * New(HeapBucketT<SmallLeafHeapBlockT> * bucket);
    static void Delete(SmallLeafHeapBlockT * block);
#ifdef RECYCLER_SLOW_CHECK_ENABLED
    virtual bool GetFreeObjectListOnAllocator(FreeObject ** freeObjectList) override;
#endif

    virtual bool FindHeapObject(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject) override sealed
    {
        return FindHeapObjectImpl<SmallLeafHeapBlockT>(objectAddress, recycler, flags, heapObject);
    }
private:
    SmallLeafHeapBlockT(HeapBucketT<SmallLeafHeapBlockT> * bucket, ushort objectSize, ushort objectCount);
};

// Declare the class templates
extern template class SmallLeafHeapBlockT<SmallAllocationBlockAttributes>;
extern template class SmallLeafHeapBlockT<MediumAllocationBlockAttributes>;

typedef SmallLeafHeapBlockT<SmallAllocationBlockAttributes> SmallLeafHeapBlock;
typedef SmallLeafHeapBlockT<MediumAllocationBlockAttributes>   MediumLeafHeapBlock;
}
