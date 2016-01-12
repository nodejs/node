//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Memory
{
template <class TBlockAttributes> class SmallFinalizableHeapBucketT;
#ifdef RECYCLER_WRITE_BARRIER
template <class TBlockAttributes> class SmallFinalizableWithBarrierHeapBlockT;
#endif

template <class TBlockAttributes>
class SmallFinalizableHeapBlockT : public SmallNormalHeapBlockT<TBlockAttributes>
{
    friend class HeapBucketT<SmallFinalizableHeapBlockT>;
public:
    typedef TBlockAttributes HeapBlockAttributes;

    static const ObjectInfoBits RequiredAttributes = FinalizeBit;
    static SmallFinalizableHeapBlockT * New(HeapBucketT<SmallFinalizableHeapBlockT> * bucket);
    static void Delete(SmallFinalizableHeapBlockT * block);

    SmallFinalizableHeapBlockT * GetNextBlock() const { return SmallHeapBlockT<TBlockAttributes>::GetNextBlock()->AsFinalizableBlock<TBlockAttributes>(); }
    void SetNextBlock(SmallFinalizableHeapBlockT * next) { __super::SetNextBlock(next); }

    void ProcessMarkedObject(void* candidate, MarkContext * markContext);

    void SetAttributes(void * address, unsigned char attributes);

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    static bool CanRescanFullBlock();
    static bool RescanObject(SmallFinalizableHeapBlockT<TBlockAttributes> * block, __in_ecount(localObjectSize) char * objectAddress, uint localObjectSize, uint objectIndex, Recycler * recycler);
    bool RescanTrackedObject(FinalizableObject * object, uint objectIndex,  Recycler * recycler);
#endif
    template<bool pageheap>
    SweepState Sweep(RecyclerSweep& sweepeData, bool queuePendingSweep, bool allocable);
    void DisposeObjects();
    void TransferDisposedObjects();

    bool HasPendingDisposeObjects() const
    {
        return (this->pendingDisposeCount != 0);
    }

    void AddPendingDisposeObject()
    {
        this->pendingDisposeCount++;
        Assert(this->pendingDisposeCount <= this->objectCount);
    }

    bool HasDisposedObjects() const { return this->disposedObjectList != nullptr; }
    bool HasAnyDisposeObjects() const { return this->HasPendingDisposeObjects() || this->HasDisposedObjects(); }

    template <typename Fn>
    void ForEachPendingDisposeObject(Fn fn)
    {
        if (this->HasPendingDisposeObjects())
        {
            for (uint i = 0; i < this->objectCount; i++)
            {
                if ((ObjectInfo(i) & PendingDisposeBit) != 0)
                {
                    // When pending dispose, exactly the PendingDisposeBits should be set
                    Assert(ObjectInfo(i) == PendingDisposeObjectBits);

                    fn(i);
                }
            }
        }
        else
        {
#if DBG
            for (uint i = 0; i < this->objectCount; i++)
            {
                Assert((ObjectInfo(i) & PendingDisposeBit) == 0);
            }
#endif
        }
    }

    ushort AddDisposedObjectFreeBitVector(SmallHeapBlockBitVector * free);
#ifdef RECYCLER_SLOW_CHECK_ENABLED
    uint CheckDisposedObjectFreeBitVector();
    virtual bool GetFreeObjectListOnAllocator(FreeObject ** freeObjectList) override;
#endif
#if DBG
#ifdef PARTIAL_GC_ENABLED
    void FinishPartialCollect();
#endif
    bool IsPendingDispose() const { return isPendingDispose; }
    void SetIsPendingDispose() { isPendingDispose = true; }
#endif
    void FinalizeAllObjects();

#ifdef DUMP_FRAGMENTATION_STATS
    ushort GetFinalizeCount() {
        return finalizeCount;
    }
#endif

    virtual bool FindHeapObject(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject) override
    {
        return FindHeapObjectImpl<SmallFinalizableHeapBlockT>(objectAddress, recycler, flags, heapObject);
    }
protected:
    SmallFinalizableHeapBlockT(HeapBucketT<SmallFinalizableHeapBlockT>  * bucket, ushort objectSize, ushort objectCount);
#ifdef RECYCLER_WRITE_BARRIER
    SmallFinalizableHeapBlockT(HeapBucketT<SmallFinalizableWithBarrierHeapBlockT<TBlockAttributes>> * bucket, ushort objectSize, ushort objectCount, HeapBlockType blockType);
#endif

#if DBG
    void Init(ushort objectSize, ushort objectCount);
    bool isPendingDispose;
#endif
    ushort finalizeCount;
    ushort pendingDisposeCount;
    FreeObject* disposedObjectList;
    FreeObject* disposedObjectListTail;

    friend class ScriptMemoryDumper;
#ifdef RECYCLER_MEMORY_VERIFY
    friend void SmallHeapBlockT<TBlockAttributes>::Verify(bool pendingDispose);
#endif
};

#ifdef RECYCLER_WRITE_BARRIER
template <class TBlockAttributes>
class SmallFinalizableWithBarrierHeapBlockT : public SmallFinalizableHeapBlockT<TBlockAttributes>
{
    friend class HeapBucketT<SmallFinalizableWithBarrierHeapBlockT>;
public:
    typedef TBlockAttributes HeapBlockAttributes;

    static const ObjectInfoBits RequiredAttributes = FinalizableWithBarrierBit;
    static const bool IsLeafOnly = false;

    static SmallFinalizableWithBarrierHeapBlockT * New(HeapBucketT<SmallFinalizableWithBarrierHeapBlockT> * bucket);
    static void Delete(SmallFinalizableWithBarrierHeapBlockT * block);

    SmallFinalizableWithBarrierHeapBlockT * GetNextBlock() const { return ((SmallHeapBlock*) this)->GetNextBlock()->AsFinalizableWriteBarrierBlock<TBlockAttributes>(); }

    virtual bool FindHeapObject(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject) override sealed
    {
        return FindHeapObjectImpl<SmallFinalizableWithBarrierHeapBlockT>(objectAddress, recycler, flags, heapObject);
    }
protected:
    SmallFinalizableWithBarrierHeapBlockT(HeapBucketT<SmallFinalizableWithBarrierHeapBlockT> * bucket, ushort objectSize, ushort objectCount)
        : SmallFinalizableHeapBlockT(bucket, objectSize, objectCount, TBlockAttributes::IsSmallBlock ? SmallFinalizableBlockWithBarrierType : MediumFinalizableBlockWithBarrierType)
    {
    }
};
#endif

extern template class SmallFinalizableHeapBlockT<SmallAllocationBlockAttributes>;
extern template class SmallFinalizableHeapBlockT<MediumAllocationBlockAttributes>;

typedef SmallFinalizableHeapBlockT<SmallAllocationBlockAttributes>  SmallFinalizableHeapBlock;
typedef SmallFinalizableHeapBlockT<MediumAllocationBlockAttributes>    MediumFinalizableHeapBlock;

#ifdef RECYCLER_WRITE_BARRIER
extern template class SmallFinalizableWithBarrierHeapBlockT<SmallAllocationBlockAttributes>;
extern template class SmallFinalizableWithBarrierHeapBlockT<MediumAllocationBlockAttributes>;

typedef SmallFinalizableWithBarrierHeapBlockT<SmallAllocationBlockAttributes>   SmallFinalizableWithBarrierHeapBlock;
typedef SmallFinalizableWithBarrierHeapBlockT<MediumAllocationBlockAttributes>     MediumFinalizableWithBarrierHeapBlock;
#endif
}
