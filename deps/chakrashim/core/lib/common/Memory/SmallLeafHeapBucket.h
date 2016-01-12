//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Memory
{
template <class TBlockAttributes>
class SmallLeafHeapBucketT : public HeapBucketT<SmallLeafHeapBlockT<TBlockAttributes>>
{
    typedef HeapBucketT<SmallLeafHeapBlockT<TBlockAttributes>> BaseT;
protected:
    friend class HeapBucket;
    template <class TBlockAttributes>
    friend class HeapBucketGroup;

    template<bool pageheap>
    void Sweep(RecyclerSweep& recyclerSweep);

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
    size_t GetNonEmptyHeapBlockCount(bool checkCount) const;
#endif
#ifdef RECYCLER_SLOW_CHECK_ENABLED
    size_t Check();
    friend class HeapBucketT<SmallLeafHeapBlockT<TBlockAttributes>>;
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    void Verify();
#endif
#ifdef RECYCLER_VERIFY_MARK
    void VerifyMark();
#endif
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    friend class ScriptMemoryDumper;
#endif
};

typedef SmallLeafHeapBucketT<SmallAllocationBlockAttributes>  SmallLeafHeapBucket;
typedef SmallLeafHeapBucketT<MediumAllocationBlockAttributes> MediumLeafHeapBucket;
}
