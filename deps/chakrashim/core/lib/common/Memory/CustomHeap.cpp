//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"
#ifdef _M_X64
#include "Memory\amd64\XDataAllocator.h"
#elif defined(_M_ARM)
#include "Memory\arm\XDataAllocator.h"
#include <wchar.h>
#elif defined(_M_ARM64)
#include "Memory\arm64\XDataAllocator.h"
#endif
#include "CustomHeap.h"

namespace Memory
{
namespace CustomHeap
{

#pragma region "Constructor and Destructor"

Heap::Heap(AllocationPolicyManager * policyManager, ArenaAllocator * alloc, bool allocXdata):
    auxilliaryAllocator(alloc),
    allocXdata(allocXdata),
#if DBG_DUMP
    freeObjectSize(0),
    totalAllocationSize(0),
    allocationsSinceLastCompact(0),
    freesSinceLastCompact(0),
#endif
#if DBG
    inDtor(false),
#endif
    pageAllocator(policyManager, allocXdata, true /*excludeGuardPages*/),
    preReservedHeapPageAllocator(policyManager, allocXdata, true /*excludeGuardPages*/),
    cs(4000)
{
    for (int i = 0; i < NumBuckets; i++)
    {
        this->buckets[i].Reset();
    }
}

Heap::~Heap()
{
#if DBG
    inDtor = true;
#endif
    this->FreeAll();
}

#pragma endregion

#pragma region "Public routines"
void Heap::FreeAll()
{
    FreeBuckets(false);

    FreeLargeObjects();

    FreeDecommittedBuckets();
    FreeDecommittedLargeObjects();
}

bool Heap::Free(__in Allocation* object)
{
    Assert(object != nullptr);

    if (object == nullptr)
    {
        return false;
    }

    BucketId bucket = (BucketId) GetBucketForSize(object->size);

    if (bucket == BucketId::LargeObjectList)
    {
#if PDATA_ENABLED
        if(!object->xdata.IsFreed())
        {
            FreeXdata(&object->xdata, object->largeObjectAllocation.segment);
        }
#endif
        if (object->largeObjectAllocation.isDecommitted)
        {
            return true;
        }

        return FreeLargeObject<false>(object);
    }
#if PDATA_ENABLED
    if(!object->xdata.IsFreed())
    {
        FreeXdata(&object->xdata, object->page->segment);
    }
#endif

    if (object->page->isDecommitted)
    {
        return true;
    }
    return FreeAllocation(object);
}

bool Heap::Decommit(__in Allocation* object)
{
    // This function doesn't really touch the page allocator data structure.
    // DecommitPages is merely a wrapper for VirtualFree
    // So no need to take the critical section to synchronize
    Assert(object != nullptr);

    if (object == nullptr)
    {
        return false;
    }

    Assert(object->isAllocationUsed);

    BucketId bucket = (BucketId) GetBucketForSize(object->size);

    if (bucket == BucketId::LargeObjectList)
    {
        Assert(!object->largeObjectAllocation.isDecommitted);

        if (!object->largeObjectAllocation.isDecommitted)
        {
#if PDATA_ENABLED
            if(!object->xdata.IsFreed())
            {
                FreeXdata(&object->xdata, object->largeObjectAllocation.segment);
            }
#endif
            this->DecommitPages(object->address, object->GetPageCount(), object->largeObjectAllocation.segment);
            this->largeObjectAllocations.MoveElementTo(object, &this->decommittedLargeObjects);
            object->largeObjectAllocation.isDecommitted = true;
            return true;
        }
    }

    // Skip asserting here- multiple objects could be on the same page
    // Review: should we really decommit here or decommit only when all objects
    // on the page have been decommitted?
    if (!object->page->isDecommitted)
    {
#if PDATA_ENABLED
        if(!object->xdata.IsFreed())
        {
            FreeXdata(&object->xdata, object->page->segment);
        }
#endif
        bucket = object->page->currentBucket;

        this->DecommitPages(object->page->address, 1, object->page->segment);

        if (this->ShouldBeInFullList(object->page))
        {
            this->fullPages[bucket].MoveElementTo(object->page, &this->decommittedPages);
        }
        else
        {
            this->buckets[bucket].MoveElementTo(object->page, &this->decommittedPages);
        }
        object->page->isDecommitted = true;
    }

    return true;
}

bool Heap::IsInRange(__in void* address)
{
    AutoCriticalSection autocs(&this->cs);

    return (this->preReservedHeapPageAllocator.GetVirtualAllocator()->IsInRange(address) || this->pageAllocator.IsAddressFromAllocator(address));
}

/*
 * Algorithm:
 *   - Find bucket
 *   - Check bucket pages - if it has enough free space, allocate that chunk
 *   - Check pages in bigger buckets - if that has enough space, split that page and allocate from that chunk
 *   - Allocate new page
 */
Allocation* Heap::Alloc(size_t bytes, ushort pdataCount, ushort xdataSize, bool canAllocInPreReservedHeapPageSegment, bool isAnyJittedCode, _Inout_ bool* isAllJITCodeInPreReservedRegion)
{
    Assert(bytes > 0);
    Assert((allocXdata || pdataCount == 0) && (!allocXdata || pdataCount > 0));
    Assert(pdataCount > 0 || (pdataCount == 0 && xdataSize == 0));

    // Round up to power of two to allocate, and figure out which bucket to allocate in
    size_t bytesToAllocate = PowerOf2Policy::GetSize(bytes);
    BucketId bucket = (BucketId) GetBucketForSize(bytesToAllocate);
    Allocation* allocation;

    if (bucket == BucketId::LargeObjectList)
    {
        return AllocLargeObject(bytes, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, isAnyJittedCode, isAllJITCodeInPreReservedRegion);
    }

    VerboseHeapTrace(L"Bucket is %d\n", bucket);
    VerboseHeapTrace(L"Requested: %d bytes. Allocated: %d bytes\n", bytes, bytesToAllocate);

    Page* page = nullptr;
    if(!this->buckets[bucket].Empty())
    {
        page = &this->buckets[bucket].Head();
    }
    else
    {
        page = FindPageToSplit(bucket, canAllocInPreReservedHeapPageSegment);
    }

    if(page == nullptr)
    {
        page = AllocNewPage(bucket, canAllocInPreReservedHeapPageSegment, isAnyJittedCode, isAllJITCodeInPreReservedRegion);
    }

    // Out of memory
    if (page == nullptr)
    {
        return nullptr;
    }

    allocation = AllocInPage(page, bytesToAllocate, pdataCount, xdataSize);
    return allocation;
}

BOOL Heap::ProtectAllocation(__in Allocation* allocation, DWORD dwVirtualProtectFlags, __out DWORD* dwOldVirtualProtectFlags, DWORD desiredOldProtectFlag)
{
    Assert(allocation != nullptr);
    Assert(allocation->isAllocationUsed);

    return ProtectAllocationInternal(allocation, nullptr, dwVirtualProtectFlags, dwOldVirtualProtectFlags, desiredOldProtectFlag);
}

BOOL Heap::ProtectAllocationPage(__in Allocation* allocation, __in char* addressInPage, DWORD dwVirtualProtectFlags, __out DWORD* dwOldVirtualProtectFlags, DWORD desiredOldProtectFlag)
{
    Assert(addressInPage != nullptr);
    Assert(allocation != nullptr);
    Assert(addressInPage >= allocation->address);
    Assert(allocation->isAllocationUsed);

    return ProtectAllocationInternal(allocation, addressInPage, dwVirtualProtectFlags, dwOldVirtualProtectFlags, desiredOldProtectFlag);
}

BOOL Heap::ProtectAllocationInternal(__in Allocation* allocation, __in_opt char* addressInPage, DWORD dwVirtualProtectFlags, __out DWORD* dwOldVirtualProtectFlags, DWORD desiredOldProtectFlag)
{
    // Allocate at the page level so that our protections don't
    // transcend allocation page boundaries. Here, allocation->address is page
    // aligned if the object is a large object allocation. If it isn't, in the else
    // branch of the following if statement, we set it to the allocation's page's
    // address. This ensures that the address being protected is always page aligned
    char* address = allocation->address;

#ifdef _CONTROL_FLOW_GUARD
    if (AutoSystemInfo::Data.IsCFGEnabled() &&
        (dwVirtualProtectFlags & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
    {
        AssertMsg(!(dwVirtualProtectFlags & PAGE_EXECUTE_WRITECOPY), "PAGE_EXECUTE_WRITECOPY is not used today. Remove this precondition \
            and add to the if condition above, if this flag is used.");
        dwVirtualProtectFlags |= PAGE_TARGETS_NO_UPDATE;
    }
#endif

    size_t pageCount;
    void * segment;
    if (allocation->IsLargeAllocation())
    {
#if DBG_DUMP || defined(RECYCLER_TRACE)
        if (Js::Configuration::Global.flags.IsEnabled(Js::TraceProtectPagesFlag))
        {
            Output::Print(L"Protecting large allocation\n");
        }
#endif
        segment = allocation->largeObjectAllocation.segment;
        allocation->largeObjectAllocation.isReadWrite = ((dwVirtualProtectFlags & PAGE_READWRITE) == PAGE_READWRITE);

        if (addressInPage != nullptr)
        {
            if (addressInPage >= allocation->address + AutoSystemInfo::PageSize)
            {
                size_t page = (addressInPage - allocation->address) / AutoSystemInfo::PageSize;

                address = allocation->address + (page * AutoSystemInfo::PageSize);
            }
            pageCount = 1;
        }
        else
        {
            pageCount = allocation->GetPageCount();
        }

        VerboseHeapTrace(L"Protecting 0x%p with 0x%x\n", address, dwVirtualProtectFlags);
        return this->ProtectPages(address, pageCount, segment, dwVirtualProtectFlags, dwOldVirtualProtectFlags, desiredOldProtectFlag);
    }
    else
    {
#if DBG_DUMP || defined(RECYCLER_TRACE)
        if (Js::Configuration::Global.flags.IsEnabled(Js::TraceProtectPagesFlag))
        {
            Output::Print(L"Protecting small allocation\n");
        }
#endif
        segment = allocation->page->segment;
        address = allocation->page->address;
        allocation->page->isReadWrite = ((dwVirtualProtectFlags & PAGE_READWRITE) == PAGE_READWRITE);
        pageCount = 1;

        VerboseHeapTrace(L"Protecting 0x%p with 0x%x\n", address, dwVirtualProtectFlags);
        return this->ProtectPages(address, pageCount, segment, dwVirtualProtectFlags, dwOldVirtualProtectFlags, desiredOldProtectFlag);
    }
}

#pragma endregion

#pragma region "Large object methods"
Allocation* Heap::AllocLargeObject(size_t bytes, ushort pdataCount, ushort xdataSize, bool canAllocInPreReservedHeapPageSegment, bool isAnyJittedCode, _Inout_ bool* isAllJITCodeInPreReservedRegion)
{
    size_t pages = GetNumPagesForSize(bytes);

    if (pages == 0)
    {
        return nullptr;
    }

    void * segment = nullptr;
    char* address = nullptr;

#if PDATA_ENABLED
    XDataAllocation xdata;
#endif

    {
        AutoCriticalSection autocs(&this->cs);
        if (canAllocInPreReservedHeapPageSegment)
        {
            address = this->preReservedHeapPageAllocator.Alloc(&pages, (SegmentBase<PreReservedVirtualAllocWrapper>**)(&segment));
        }

        if (address == nullptr)
        {
            if (isAnyJittedCode)
            {
                *isAllJITCodeInPreReservedRegion = false;
            }
            address = this->pageAllocator.Alloc(&pages, (Segment**)&segment);
        }

        // Out of memory
        if (address == nullptr)
        {
            return nullptr;
        }

        FillDebugBreak((BYTE*) address, pages*AutoSystemInfo::PageSize);

#if PDATA_ENABLED
        if(pdataCount > 0)
        {
            if (!this->AllocSecondary(segment, (ULONG_PTR) address, bytes, pdataCount, xdataSize, &xdata))
            {
                AutoCriticalSection autocs(&this->cs);
                this->Release(address, pages, segment);
                return nullptr;
            }
        }
#endif
    }


    Allocation* allocation = this->largeObjectAllocations.PrependNode(this->auxilliaryAllocator);
    if (allocation == nullptr)
    {
        AutoCriticalSection autocs(&this->cs);
        this->Release(address, pages, segment);

#if PDATA_ENABLED
        if(pdataCount > 0)
        {
            this->ReleaseSecondary(xdata, segment);
        }
#endif
        return nullptr;
    }

    allocation->address = address;
    allocation->largeObjectAllocation.segment = segment;
    allocation->largeObjectAllocation.isDecommitted = false;
    allocation->largeObjectAllocation.isReadWrite = true;
    allocation->size = pages * AutoSystemInfo::PageSize;

#if PDATA_ENABLED
    allocation->xdata = xdata;

    if (((Segment*)segment)->GetSecondaryAllocator() != nullptr && !((Segment*)segment)->CanAllocSecondary())
    {
        TransferPages(
            [&](Page* currentPage) -> bool
        {
            bool transfer = currentPage->segment == segment;
            if(transfer)
            {
                VerboseHeapTrace(L"Moving page from bucket %d to full list because no XDATA allocations can be made\n", currentPage->currentBucket);
            }
            return transfer;
        } , this->buckets, this->fullPages);
    }
#endif
    return allocation;
}

void Heap::FreeDecommittedLargeObjects()
{
    // This is only call when the heap is being destroy, so don't need to sync with the background thread.
    Assert(inDtor);
    FOREACH_DLISTBASE_ENTRY_EDITING(Allocation, allocation, &this->decommittedLargeObjects, largeObjectIter)
    {
        VerboseHeapTrace(L"Decommitting large object at address 0x%p of size %u\n", allocation.address, allocation.size);

        this->ReleaseDecommited(allocation.address, allocation.GetPageCount(), allocation.largeObjectAllocation.segment);

        largeObjectIter.RemoveCurrent(this->auxilliaryAllocator);
    }
    NEXT_DLISTBASE_ENTRY_EDITING;
}

template <bool freeAll>
bool Heap::FreeLargeObject(Allocation* address)
{
    AutoCriticalSection autocs(&this->cs);
    FOREACH_DLISTBASE_ENTRY_EDITING(Allocation, allocation, &this->largeObjectAllocations, largeObjectIter)
    {
        if (address == (&allocation) || freeAll)
        {
            EnsureAllocationWriteable(&allocation);
#if PDATA_ENABLED
            Assert(allocation.xdata.IsFreed());
#endif
            this->Release(allocation.address, allocation.GetPageCount(), allocation.largeObjectAllocation.segment);

            largeObjectIter.RemoveCurrent(this->auxilliaryAllocator);
            if (!freeAll) return true;
        }
    }
    NEXT_DLISTBASE_ENTRY_EDITING;

    // If we're not freeing everything, and we hit this point, that means that
    // something that wasn't in the large object list was asked to be free.
    // So, assert that we're freeing everything if we get to this point.
    Assert(freeAll);
    return false;
}

#pragma endregion

#pragma region "Page methods"

Allocation* Heap::AllocInPage(Page* page, size_t bytes, ushort pdataCount, ushort xdataSize)
{
    Assert(Math::IsPow2((int32)bytes));

    uint length = GetChunkSizeForBytes(bytes);
    BVIndex index = GetFreeIndexForPage(page, bytes);
    Assert(index != BVInvalidIndex);
    char* address = page->address + Page::Alignment * index;

#if PDATA_ENABLED
    XDataAllocation xdata;
    if(pdataCount > 0)
    {
        AutoCriticalSection autocs(&this->cs);
        {
            if(!this->AllocSecondary(page->segment, (ULONG_PTR)address, bytes, pdataCount, xdataSize, &xdata))
            {
                return nullptr;
            }
        }
    }
#endif

    Allocation* allocation = AnewNoThrowStruct(this->auxilliaryAllocator, Allocation);
    if (allocation == nullptr)
    {
#if PDATA_ENABLED
        if(pdataCount > 0)
        {
            AutoCriticalSection autocs(&this->cs);
            this->ReleaseSecondary(xdata, page->segment);
        }
#endif
        return nullptr;
    }

#if DBG
    allocation->isAllocationUsed = false;
    allocation->isNotExecutableBecauseOOM = false;
#endif

    allocation->page = page;
    allocation->size = bytes;
    allocation->address = address;

#if DBG_DUMP
    this->allocationsSinceLastCompact += bytes;
    this->freeObjectSize -= bytes;
#endif

    page->freeBitVector.ClearRange(index, length);
    VerboseHeapTrace(L"ChunkSize: %d, Index: %d, Free bit vector in page: ", length, index);

#if VERBOSE_HEAP
    page->freeBitVector.DumpWord();
#endif
    VerboseHeapTrace(L"\n");


    if (this->ShouldBeInFullList(page))
    {
        BucketId bucket = page->currentBucket;
        VerboseHeapTrace(L"Moving page from bucket %d to full list\n", bucket);

        this->buckets[bucket].MoveElementTo(page, &this->fullPages[bucket]);
    }

#if PDATA_ENABLED
    allocation->xdata = xdata;

    if(((Segment*)page->segment)->GetSecondaryAllocator() != nullptr && !((Segment*)page->segment)->CanAllocSecondary())
    {
        TransferPages(
            [&](Page* currentPage) -> bool
        {
            bool transfer = currentPage->segment == page->segment;
            if(transfer)
            {
                VerboseHeapTrace(L"Moving page from bucket %d to full list because no XDATA allocations can be made\n", page->currentBucket);
            }
            return transfer;
        } , this->buckets, this->fullPages);
    }
#endif

    return allocation;
}

char *
Heap::EnsurePreReservedPageAllocation(PreReservedVirtualAllocWrapper * preReservedVirtualAllocator)
{
        AutoCriticalSection autocs(&this->cs);
        Assert(preReservedVirtualAllocator != nullptr);
        Assert(preReservedHeapPageAllocator.GetVirtualAllocator() == preReservedVirtualAllocator);

        char * preReservedRegionStartAddress = (char*)preReservedVirtualAllocator->GetPreReservedStartAddress();
        if (preReservedRegionStartAddress == nullptr)
        {
            preReservedRegionStartAddress = preReservedHeapPageAllocator.InitPageSegment();
        }

        if (preReservedRegionStartAddress == nullptr)
        {
            VerboseHeapTrace(L"PRE-RESERVE: PreReserved Segment CANNOT be allocated \n");
        }
        return preReservedRegionStartAddress;
}

Page* Heap::AllocNewPage(BucketId bucket, bool canAllocInPreReservedHeapPageSegment, bool isAnyJittedCode, _Inout_ bool* isAllJITCodeInPreReservedRegion)
{
    void* pageSegment = nullptr;

    char* address = nullptr;
    {
        AutoCriticalSection autocs(&this->cs);

        if (canAllocInPreReservedHeapPageSegment)
        {
            address = this->preReservedHeapPageAllocator.AllocPages(1, (PageSegmentBase<PreReservedVirtualAllocWrapper>**)&pageSegment);

            if (address == nullptr)
            {
                VerboseHeapTrace(L"PRE-RESERVE: PreReserved Segment CANNOT be allocated \n");
            }
        }

        if (address == nullptr)    // if no space in Pre-reserved Page Segment, then allocate in regular ones.
        {
            if (isAnyJittedCode)
            {
                *isAllJITCodeInPreReservedRegion = false;
            }
            address = this->pageAllocator.AllocPages(1, (PageSegmentBase<VirtualAllocWrapper>**)&pageSegment);
        }
        else
        {
            VerboseHeapTrace(L"PRE-RESERVE: Allocing new page in PreReserved Segment \n");
        }
    }

    if (address == nullptr)
    {
        return nullptr;
    }

    FillDebugBreak((BYTE*) address, AutoSystemInfo::PageSize);

    // Switch to allocating on a list of pages so we can do leak tracking later
    VerboseHeapTrace(L"Allocing new page in bucket %d\n", bucket);
    Page* page = this->buckets[bucket].PrependNode(this->auxilliaryAllocator, address, pageSegment, bucket);

    if (page == nullptr)
    {
        AutoCriticalSection autocs(&this->cs);
        this->ReleasePages(address, 1, pageSegment);
        return nullptr;
    }

#if DBG_DUMP
    this->totalAllocationSize += AutoSystemInfo::PageSize;
    this->freeObjectSize += AutoSystemInfo::PageSize;
#endif

    return page;
}

Page* Heap::AddPageToBucket(Page* page, BucketId bucket, bool wasFull)
{
    Assert(bucket > BucketId::InvalidBucket && bucket < BucketId::NumBuckets);

    BucketId oldBucket = page->currentBucket;

    page->currentBucket = bucket;
    if (wasFull)
    {
        #pragma prefast(suppress: __WARNING_UNCHECKED_LOWER_BOUND_FOR_ENUMINDEX, "targetBucket is always in range >= SmallObjectList, but an __in_range doesn't fix the warning.");
        this->fullPages[oldBucket].MoveElementTo(page, &this->buckets[bucket]);
    }
    else
    {
        #pragma prefast(suppress: __WARNING_UNCHECKED_LOWER_BOUND_FOR_ENUMINDEX, "targetBucket is always in range >= SmallObjectList, but an __in_range doesn't fix the warning.");
        this->buckets[oldBucket].MoveElementTo(page, &this->buckets[bucket]);
    }

    return page;
}

/*
 * This method goes through the buckets greater than the target bucket
 * and if the higher bucket has a page with enough free space to allocate
 * something in the smaller bucket, then we bring the page to the smaller
 * bucket.
 * Note that if we allocate something from a page in the given bucket,
 * and then that page is split into a lower bucket, freeing is still not
 * a problem since the larger allocation is a multiple of the smaller one.
 * This gets more complicated if we can coalesce buckets. In that case,
 * we need to make sure that if a page was coalesced, and an allocation
 * pre-coalescing was freed, the page would need to get split upon free
 * to ensure correctness. For now, we've skipped implementing coalescing.
 * findPreReservedHeapPages - true, if we need to find pages only belonging to PreReservedHeapSegment
 */
Page* Heap::FindPageToSplit(BucketId targetBucket, bool findPreReservedHeapPages)
{
    for (BucketId b = (BucketId)(targetBucket + 1); b < BucketId::NumBuckets; b = (BucketId) (b + 1))
    {
        #pragma prefast(suppress: __WARNING_UNCHECKED_LOWER_BOUND_FOR_ENUMINDEX, "targetBucket is always in range >= SmallObjectList, but an __in_range doesn't fix the warning.");
        FOREACH_DLISTBASE_ENTRY_EDITING(Page, pageInBucket, &this->buckets[b], bucketIter)
        {
            if (findPreReservedHeapPages && !IsPreReservedSegment(pageInBucket.segment))
            {
                //Find only pages that are pre-reserved using preReservedHeapPageAllocator
                continue;
            }

            if (pageInBucket.CanAllocate(targetBucket))
            {
                Page* page = &pageInBucket;
                if (findPreReservedHeapPages)
                {
                    VerboseHeapTrace(L"PRE-RESERVE: Found page for splitting in Pre Reserved Segment\n");
                }
                VerboseHeapTrace(L"Found page to split. Moving from bucket %d to %d\n", b, targetBucket);
                return AddPageToBucket(page, targetBucket);
            }
        }
        NEXT_DLISTBASE_ENTRY_EDITING;
    }

    return nullptr;
}

void Heap::RemovePageFromFullList(Page* pageToRemove)
{
    FOREACH_DLISTBASE_ENTRY_EDITING(Page, page, &this->fullPages[pageToRemove->currentBucket], pageIter)
    {
        if (&page == pageToRemove)
        {
            pageIter.RemoveCurrent(this->auxilliaryAllocator);
            return;
        }
    }
    NEXT_DLISTBASE_ENTRY_EDITING;

    // Page not found- why?
    Assert(false);
}

BVIndex Heap::GetIndexInPage(__in Page* page, __in char* address)
{
    Assert(page->address <= address && address < page->address + AutoSystemInfo::PageSize);

    return (BVIndex) ((address - page->address) / Page::Alignment);
}

#pragma endregion

/**
 * Free List methods
 */
#pragma region "Freeing methods"
bool Heap::FreeAllocation(Allocation* object)
{
    Page* page = object->page;
    void* segment = page->segment;
    size_t pageSize =  AutoSystemInfo::PageSize;

    unsigned int length = GetChunkSizeForBytes(object->size);
    BVIndex index = GetIndexInPage(page, object->address);

#if DBG
    // Make sure that it's not already been freed
    for (BVIndex i = index; i < length; i++)
    {
        Assert(!page->freeBitVector.Test(i));
    }
#endif

    if (this->ShouldBeInFullList(page))
    {
        VerboseHeapTrace(L"Recycling page 0x%p because address 0x%p of size %d was freed\n", page->address, object->address, object->size);

        // If the object being freed is equal to the page size, we're
        // going to remove it anyway so don't add it to a bucket
        if (object->size != pageSize)
        {
            AddPageToBucket(page, page->currentBucket, true);
        }
        else
        {
            EnsureAllocationWriteable(object);

            // Fill the old buffer with debug breaks
            CustomHeap::FillDebugBreak((BYTE *)object->address, object->size);

            void* pageAddress = page->address;

            RemovePageFromFullList(page);

            // The page is not in any bucket- just update the stats, free the allocation
            // and dump the page- we don't need to update free object size since the object
            // size is equal to the page size so they cancel each other out
#if DBG_DUMP
            this->totalAllocationSize -= pageSize;
#endif
            this->auxilliaryAllocator->Free(object, sizeof(Allocation));
            {
                AutoCriticalSection autocs(&this->cs);
                this->ReleasePages(pageAddress, 1, segment);
            }
            VerboseHeapTrace(L"FastPath: freeing page-sized object directly\n");
            return true;
        }
    }

    // If the page is about to become empty then we should not need
    // to set it to executable and we don't expect to restore the
    // previous protection settings.
    if (page->freeBitVector.Count() == BVUnit::BitsPerWord - length)
    {
        EnsureAllocationWriteable(object);
    }
    else
    {
        EnsureAllocationExecuteWriteable(object);
    }

    // Fill the old buffer with debug breaks
    CustomHeap::FillDebugBreak((BYTE *)object->address, object->size);

    VerboseHeapTrace(L"Setting %d bits starting at bit %d, Free bit vector in page was ", length, index);
#if VERBOSE_HEAP
    page->freeBitVector.DumpWord();
#endif
    VerboseHeapTrace(L"\n");

    page->freeBitVector.SetRange(index, length);
    VerboseHeapTrace(L"Free bit vector in page: ", length, index);
#if VERBOSE_HEAP
    page->freeBitVector.DumpWord();
#endif
    VerboseHeapTrace(L"\n");

#if DBG_DUMP
    this->freeObjectSize += object->size;
    this->freesSinceLastCompact += object->size;
#endif

    this->auxilliaryAllocator->Free(object, sizeof(Allocation));

    if (page->IsEmpty())
    {
        // Find the page and remove it from the buckets- the page is going to be freed anyway
        FOREACH_DLISTBASE_ENTRY_EDITING(Page, pageInBucket, &this->buckets[page->currentBucket], pageIter)
        {
            // Templatize this to remove branches/make code more compact?
            if (&pageInBucket == page)
            {
                VerboseHeapTrace(L"Removing page in bucket %d\n", page->currentBucket);
                {
                    AutoCriticalSection autocs(&this->cs);
                    this->ReleasePages(page->address, 1, page->segment);
                }
                pageIter.RemoveCurrent(this->auxilliaryAllocator);

#if DBG_DUMP
                this->freeObjectSize -= pageSize;
                this->totalAllocationSize -= pageSize;
#endif
                return true;
            }
        }
        NEXT_DLISTBASE_ENTRY_EDITING;

        return false;
    }
    else // after freeing part of the page, the page should be in PAGE_EXECUTE_READWRITE protection, and turning to PAGE_EXECUTE
    {
        DWORD dwExpectedFlags = 0;

        this->ProtectPages(page->address, 1, segment, PAGE_EXECUTE, &dwExpectedFlags, PAGE_EXECUTE_READWRITE);

        Assert(!object->isAllocationUsed || dwExpectedFlags == PAGE_EXECUTE_READWRITE);
        page->isReadWrite = false;

        return true;
    }
}

void Heap::FreeDecommittedBuckets()
{
    // This is only call when the heap is being destroy, so don't need to sync with the background thread.
    Assert(inDtor);
    FOREACH_DLISTBASE_ENTRY_EDITING(Page, page, &this->decommittedPages, iter)
    {
        this->TrackDecommitedPages(page.address, 1, page.segment);
        iter.RemoveCurrent(this->auxilliaryAllocator);
    }
    NEXT_DLISTBASE_ENTRY_EDITING;
}

void Heap::FreePage(Page* page)
{
    // This is only call when the heap is being destroy, so don't need to sync with the background thread.
    Assert(inDtor);
    DWORD pageSize = AutoSystemInfo::PageSize;
    EnsurePageWriteable(page);
    size_t freeSpace = page->freeBitVector.Count() * Page::Alignment;

    VerboseHeapTrace(L"Removing page in bucket %d, freeSpace: %d\n", page->currentBucket, freeSpace);
    this->ReleasePages(page->address, 1, page->segment);

#if DBG_DUMP
    this->freeObjectSize -= freeSpace;
    this->totalAllocationSize -= pageSize;
#endif
}

void Heap::FreeBucket(DListBase<Page>* bucket, bool freeOnlyEmptyPages)
{
    // This is only call when the heap is being destroy, so don't need to sync with the background thread.
    Assert(inDtor);
    FOREACH_DLISTBASE_ENTRY_EDITING(Page, page, bucket, pageIter)
    {
        // Templatize this to remove branches/make code more compact?
        if (!freeOnlyEmptyPages || page.IsEmpty())
        {
            FreePage(&page);
            pageIter.RemoveCurrent(this->auxilliaryAllocator);
        }
    }
    NEXT_DLISTBASE_ENTRY_EDITING;
}

void Heap::FreeBuckets(bool freeOnlyEmptyPages)
{
    // This is only call when the heap is being destroy, so don't need to sync with the background thread.
    Assert(inDtor);
    for (int i = 0; i < NumBuckets; i++)
    {
        FreeBucket(&this->buckets[i], freeOnlyEmptyPages);
        FreeBucket(&this->fullPages[i], freeOnlyEmptyPages);
    }

#if DBG_DUMP
    this->allocationsSinceLastCompact = 0;
    this->freesSinceLastCompact = 0;
#endif
}

#if PDATA_ENABLED
void Heap::FreeXdata(XDataAllocation* xdata, void* segment)
{
    Assert(!xdata->IsFreed());

    if(!((Segment*)segment)->CanAllocSecondary())
    {
        this->TransferPages([&](Page* currentPage) -> bool
        {
            bool transfer = currentPage->segment == segment && !currentPage->HasNoSpace();
            if(transfer)
            {
                VerboseHeapTrace(L"Recycling page 0x%p because XDATA was freed\n", currentPage->address);
            }
            return transfer;
        }, this->fullPages, this->buckets);
    }
    {
        AutoCriticalSection autocs(&this->cs);
        this->ReleaseSecondary(*xdata, segment);
        xdata->Free();
    }
}
#endif

#if DBG_DUMP
void Heap::DumpStats()
{
    HeapTrace(L"Total allocation size: %d\n", totalAllocationSize);
    HeapTrace(L"Total free size: %d\n", freeObjectSize);
    HeapTrace(L"Total allocations since last compact: %d\n", allocationsSinceLastCompact);
    HeapTrace(L"Total frees since last compact: %d\n", freesSinceLastCompact);
    HeapTrace(L"Large object count: %d\n", this->largeObjectAllocations.Count());

    HeapTrace(L"Buckets: \n");
    for (int i = 0; i < BucketId::NumBuckets; i++)
    {
        printf("\t%d => %u [", (1 << (i + 7)), buckets[i].Count());

        FOREACH_DLISTBASE_ENTRY_EDITING(Page, page, &this->buckets[i], bucketIter)
        {
            BVUnit usedBitVector = page.freeBitVector;
            usedBitVector.ComplimentAll(); // Get the actual used bit vector
            printf(" %u ", usedBitVector.Count() * Page::Alignment); // Print out the space used in this page
        }

        NEXT_DLISTBASE_ENTRY_EDITING
            printf("] {{%u}}\n", this->fullPages[i].Count());
    }
}
#endif

#pragma endregion

/**
 * Helper methods
 */
#pragma region "Helpers"
inline unsigned int log2(size_t number)
{
    const unsigned int b[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
    const unsigned int S[] = {1, 2, 4, 8, 16};

    unsigned int result = 0;
    for (int i = 4; i >= 0; i--)
    {
        if (number & b[i])
        {
            number >>= S[i];
            result |= S[i];
        }
    }

    return result;
}

inline BucketId GetBucketForSize(size_t bytes)
{
    if (bytes > Page::MaxAllocationSize)
    {
        return BucketId::LargeObjectList;
    }

    BucketId bucket = (BucketId) (log2(bytes) - 7);

    // < 8 => 0
    // 8 => 1
    // 9 => 2 ...
    Assert(bucket < BucketId::LargeObjectList);

    if (bucket < BucketId::SmallObjectList)
    {
        bucket = BucketId::SmallObjectList;
    }

    return bucket;
}

// Fills the specified buffer with "debug break" instruction encoding.
// If there is any space left after that due to alignment, fill it with 0.
// static
void FillDebugBreak(__out_bcount_full(byteCount) BYTE* buffer, __in size_t byteCount)
{
#if defined(_M_ARM)
    // On ARM there is breakpoint instruction (BKPT) which is 0xBEii, where ii (immediate 8) can be any value, 0xBE in particular.
    // While it could be easier to put 0xBE (same way as 0xCC on x86), BKPT is not recommended -- it may cause unexpected side effects.
    // So, use same sequence are C++ compiler uses (0xDEFE), this is recognized by debugger as __debugbreak.
    // This is 2 bytes, and in case there is a gap of 1 byte in the end, fill it with 0 (there is no 1 byte long THUMB instruction).
    CompileAssert(sizeof(wchar_t) == 2);
    wchar_t pattern = 0xDEFE;
    wmemset(reinterpret_cast<wchar_t*>(buffer), pattern, byteCount / 2);
    if (byteCount % 2)
    {
        // Note: this is valid scenario: in JIT mode, we may not be 2-byte-aligned in the end of unwind info.
        *(buffer + byteCount - 1) = 0;  // Fill last remaining byte.
    }
#elif defined(_M_ARM64)
    CompileAssert(sizeof(DWORD) == 4);
    DWORD pattern = 0xd4200000 | (0xf000 << 5);
    for (size_t i = 0; i < byteCount / 4; i++)
    {
        reinterpret_cast<DWORD*>(buffer)[i] = pattern;
    }
    for (size_t i = (byteCount / 4) * 4; i < byteCount; i++)
    {
        // Note: this is valid scenario: in JIT mode, we may not be 2-byte-aligned in the end of unwind info.
        buffer[i] = 0;  // Fill last remaining bytes.
    }
#else
    // On Intel just use "INT 3" instruction which is 0xCC.
    memset(buffer, 0xCC, byteCount);
#endif
}

#pragma endregion
};
}
