//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#ifndef TBlockTypeAttributes
#error Need to define block type attributes before including this file
#endif

template void SmallHeapBlockT<TBlockTypeAttributes>::ReleasePages<true>(Recycler * recycler);
template void SmallHeapBlockT<TBlockTypeAttributes>::ReleasePages<false>(Recycler * recycler);
template void SmallHeapBlockT<TBlockTypeAttributes>::BackgroundReleasePagesSweep<true>(Recycler* recycler);
template void SmallHeapBlockT<TBlockTypeAttributes>::BackgroundReleasePagesSweep<false>(Recycler* recycler);
template void SmallHeapBlockT<TBlockTypeAttributes>::ReleasePagesSweep<true>(Recycler * recycler);
template void SmallHeapBlockT<TBlockTypeAttributes>::ReleasePagesSweep<false>(Recycler * recycler);
template BOOL SmallHeapBlockT<TBlockTypeAttributes>::ReassignPages<true>(Recycler * recycler);
template BOOL SmallHeapBlockT<TBlockTypeAttributes>::ReassignPages<false>(Recycler * recycler);
template BOOL SmallHeapBlockT<TBlockTypeAttributes>::SetPage<true>(__in_ecount_pagesize char * baseAddress, PageSegment * pageSegment, Recycler * recycler);
template BOOL SmallHeapBlockT<TBlockTypeAttributes>::SetPage<false>(__in_ecount_pagesize char * baseAddress, PageSegment * pageSegment, Recycler * recycler);
template const uint SmallHeapBlockT<TBlockTypeAttributes>::GetPageHeapModePageCount<true>() const;
template const uint SmallHeapBlockT<TBlockTypeAttributes>::GetPageHeapModePageCount<false>() const;
template SweepState SmallHeapBlockT<TBlockTypeAttributes>::Sweep<true>(RecyclerSweep& recyclerSweep, bool queuePendingSweep, bool allocable, ushort finalizeCount, bool hasPendingDispose);
template SweepState SmallHeapBlockT<TBlockTypeAttributes>::Sweep<false>(RecyclerSweep& recyclerSweep, bool queuePendingSweep, bool allocable, ushort finalizeCount, bool hasPendingDispose);

template SmallNormalHeapBlockT<TBlockTypeAttributes>* HeapBlock::AsNormalBlock<TBlockTypeAttributes>();
template SmallLeafHeapBlockT<TBlockTypeAttributes>* HeapBlock::AsLeafBlock<TBlockTypeAttributes>();
template SmallFinalizableHeapBlockT<TBlockTypeAttributes>* HeapBlock::AsFinalizableBlock<TBlockTypeAttributes>();
#ifdef RECYCLER_WRITE_BARRIER
template SmallNormalWithBarrierHeapBlockT<TBlockTypeAttributes>* HeapBlock::AsNormalWriteBarrierBlock<TBlockTypeAttributes>();
template SmallFinalizableWithBarrierHeapBlockT<TBlockTypeAttributes>* HeapBlock::AsFinalizableWriteBarrierBlock<TBlockTypeAttributes>();
#endif

template bool SmallHeapBlockT<TBlockTypeAttributes>::FindHeapObjectImpl<SmallLeafHeapBlockT<TBlockTypeAttributes>>(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject);
template bool SmallHeapBlockT<TBlockTypeAttributes>::FindHeapObjectImpl<SmallNormalHeapBlockT<TBlockTypeAttributes>>(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject);
template bool SmallHeapBlockT<TBlockTypeAttributes>::FindHeapObjectImpl<SmallFinalizableHeapBlockT<TBlockTypeAttributes>>(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject);
#ifdef RECYCLER_WRITE_BARRIER
template bool SmallHeapBlockT<TBlockTypeAttributes>::FindHeapObjectImpl<SmallNormalWithBarrierHeapBlockT<TBlockTypeAttributes>>(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject);
template bool SmallHeapBlockT<TBlockTypeAttributes>::FindHeapObjectImpl<SmallFinalizableWithBarrierHeapBlockT<TBlockTypeAttributes>>(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject);
#endif

#ifdef RECYCLER_SLOW_CHECK_ENABLED
template bool SmallHeapBlockT<TBlockTypeAttributes>::GetFreeObjectListOnAllocatorImpl<SmallNormalHeapBlockT<TBlockTypeAttributes>>(FreeObject ** freeObjectList);
template bool SmallHeapBlockT<TBlockTypeAttributes>::GetFreeObjectListOnAllocatorImpl<SmallLeafHeapBlockT<TBlockTypeAttributes>>(FreeObject ** freeObjectList);
template bool SmallHeapBlockT<TBlockTypeAttributes>::GetFreeObjectListOnAllocatorImpl<SmallFinalizableHeapBlockT<TBlockTypeAttributes>>(FreeObject ** freeObjectList);
#ifdef RECYCLER_WRITE_BARRIER
template bool SmallHeapBlockT<TBlockTypeAttributes>::GetFreeObjectListOnAllocatorImpl<SmallNormalWithBarrierHeapBlockT<TBlockTypeAttributes>>(FreeObject ** freeObjectList);
template bool SmallHeapBlockT<TBlockTypeAttributes>::GetFreeObjectListOnAllocatorImpl<SmallFinalizableWithBarrierHeapBlockT<TBlockTypeAttributes>>(FreeObject ** freeObjectList);
#endif
#endif

// template const SmallHeapBlockT<TBlockTypeAttributes>::SmallHeapBlockBitVector * HeapInfo::ValidPointersMap<TBlockTypeAttributes>::GetInvalidBitVector(uint index) const;

// Explicit instantiate all the sweep mode
template void SmallHeapBlockT<TBlockTypeAttributes>::SweepObjects<true, SweepMode_InThread>(Recycler * recycler);
template void SmallHeapBlockT<TBlockTypeAttributes>::SweepObjects<false, SweepMode_InThread>(Recycler * recycler);
#ifdef CONCURRENT_GC_ENABLED
template <>
template <>
void
SmallHeapBlockT<TBlockTypeAttributes>::SweepObject<SweepMode_Concurrent>(Recycler * recycler, uint i, void * addr)
{
    AssertMsg(!(ObjectInfo(i) & FinalizeBit), "Finalize object should not be concurrent swept");
    EnqueueProcessedObject(&freeObjectList, addr, i);
}
// Explicit instantiate all the sweep mode
template void SmallHeapBlockT<TBlockTypeAttributes>::SweepObjects<false, SweepMode_Concurrent>(Recycler * recycler);
#ifdef PARTIAL_GC_ENABLED
template <>
template <>
void
SmallHeapBlockT<TBlockTypeAttributes>::SweepObject<SweepMode_ConcurrentPartial>(Recycler * recycler, uint i, void * addr)
{
    Assert(recycler->inPartialCollectMode);
    AssertMsg(!this->IsLeafBlock(), "Leaf pages should not do partial sweep");
    AssertMsg(!(ObjectInfo(i) & FinalizeBit), "Finalize object should not be concurrent swept");

    // This is a partial swept block; i.e. we're not reusing it.
    // Just leave the object as-is; we will collect it in a future Sweep.

    // However, we do clear out the ObjectInfo
    // this keeps us from getting confused in certain situations, e.g. function enumeration for the debugger.
    ObjectInfo(i) = 0;
}

// Explicit instantiate all the sweep mode
template void SmallHeapBlockT<TBlockTypeAttributes>::SweepObjects<false, SweepMode_ConcurrentPartial>(Recycler * recycler);
#endif
#endif

template <>
template <>
void
SmallHeapBlockT<TBlockTypeAttributes>::SweepObject<SweepMode_InThread>(Recycler * recycler, uint i, void * addr)
{
    if (ObjectInfo(i) & FinalizeBit)
    {
        Assert(this->IsAnyFinalizableBlock());

#ifdef CONCURRENT_GC_ENABLED
        Assert(!recycler->IsConcurrentExecutingState());
#endif

        // Call prepare finalize to do clean up that needs to be done immediately
        // (e.g. Clear the ITrackable alias reference, so it can't be revived during
        // other finalizers or concurrent sweep)
        ((FinalizableObject *)addr)->Finalize(false);

        // Set ObjectInfo to indicate a pending dispose object

        ObjectInfo(i) = PendingDisposeObjectBits;
        this->AsFinalizableBlock<TBlockTypeAttributes>()->AddPendingDisposeObject();

#ifdef RECYCLER_FINALIZE_CHECK
        recycler->autoHeap.pendingDisposableObjectCount++;
#endif
    }
    else
    {
        EnqueueProcessedObject(&freeObjectList, addr, i);
    }
}
