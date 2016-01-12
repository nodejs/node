//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#define UpdateMinimum(dst, src) if (dst > src) { dst = src; }

//=============================================================================================================
// Segment
//=============================================================================================================

template<typename T>
SegmentBase<T>::SegmentBase(PageAllocatorBase<T> * allocator, size_t pageCount) :
allocator(allocator),
address(nullptr),
trailingGuardPageCount(0),
leadingGuardPageCount(0),
secondaryAllocPageCount(allocator->secondaryAllocPageCount),
secondaryAllocator(nullptr)
#if defined(_M_X64_OR_ARM64) && defined(RECYCLER_WRITE_BARRIER)
, isWriteBarrierAllowed(false)
#endif
{
    this->segmentPageCount = pageCount + secondaryAllocPageCount;
}

template<typename T>
SegmentBase<T>::~SegmentBase()
{
    Assert(this->allocator != nullptr);

    if (this->address)
    {
        char* originalAddress = this->address - (leadingGuardPageCount * AutoSystemInfo::PageSize);
        allocator->GetVirtualAllocator()->Free(originalAddress, GetPageCount() * AutoSystemInfo::PageSize, MEM_RELEASE);
        allocator->ReportFree(this->segmentPageCount * AutoSystemInfo::PageSize); //Note: We reported the guard pages free when we decommitted them during segment initialization
#if defined(_M_X64_OR_ARM64) && defined(RECYCLER_WRITE_BARRIER_BYTE)
        RecyclerWriteBarrierManager::OnSegmentFree(this->address, this->segmentPageCount);
#endif
    }

    if(this->secondaryAllocator)
    {
        this->secondaryAllocator->Delete();
        this->secondaryAllocator = nullptr;
    }
}

template<typename T>
bool SegmentBase<T>::IsInPreReservedHeapPageAllocator() const
{
    return this->allocator->GetVirtualAllocator() != nullptr;
}

template<typename T>
bool
SegmentBase<T>::Initialize(DWORD allocFlags, bool excludeGuardPages)
{
    Assert(this->address == nullptr);
    char* originalAddress = nullptr;
    bool addGuardPages = false;
    if (!excludeGuardPages)
    {
        addGuardPages = (this->segmentPageCount * AutoSystemInfo::PageSize) > VirtualAllocThreshold;
#if _M_IX86_OR_ARM32
        unsigned int randomNumber = static_cast<unsigned int>(Math::Rand());
        addGuardPages = addGuardPages && (randomNumber % 4 == 1);
#endif
#if DEBUG
        addGuardPages = addGuardPages || Js::Configuration::Global.flags.ForceGuardPages;
#endif
        if (addGuardPages)
        {
            unsigned int randomNumber = static_cast<unsigned int>(Math::Rand());
            this->leadingGuardPageCount = randomNumber % maxGuardPages + minGuardPages;
            this->trailingGuardPageCount = minGuardPages;
        }
    }

    // We can only allocate with this granularity using VirtualAlloc
    size_t totalPages = Math::Align<size_t>(this->segmentPageCount + leadingGuardPageCount + trailingGuardPageCount, AutoSystemInfo::Data.GetAllocationGranularityPageCount());
    this->segmentPageCount = totalPages - (leadingGuardPageCount + trailingGuardPageCount);

#ifdef FAULT_INJECTION
    if(Js::FaultInjection::Global.ShouldInjectFault(Js::FaultInjection::Global.NoThrow))
    {
        this->address = nullptr;
        return(address != nullptr);
    }
#endif

    if (!this->allocator->RequestAlloc(totalPages * AutoSystemInfo::PageSize))
    {
        return nullptr;
    }

    this->address = (char *) GetAllocator()->GetVirtualAllocator()->Alloc(NULL, totalPages * AutoSystemInfo::PageSize, MEM_RESERVE | allocFlags, PAGE_READWRITE, this->IsInCustomHeapAllocator());

    originalAddress = this->address;

    if (originalAddress != nullptr)
    {
        if (addGuardPages)
        {
#if DBG_DUMP
            GUARD_PAGE_TRACE(L"Number of Leading Guard Pages: %d\n", leadingGuardPageCount);
            GUARD_PAGE_TRACE(L"Starting address of Leading Guard Pages: 0x%p\n", address);
            GUARD_PAGE_TRACE(L"Offset of Segment Start address: 0x%p\n", this->address + (leadingGuardPageCount*AutoSystemInfo::PageSize));
            GUARD_PAGE_TRACE(L"Starting address of Trailing Guard Pages: 0x%p\n", address + ((leadingGuardPageCount + this->segmentPageCount)*AutoSystemInfo::PageSize));
#endif
#pragma warning(suppress: 6250)
            GetAllocator()->GetVirtualAllocator()->Free(address, leadingGuardPageCount*AutoSystemInfo::PageSize, MEM_DECOMMIT);
#pragma warning(suppress: 6250)
            GetAllocator()->GetVirtualAllocator()->Free(address + ((leadingGuardPageCount + this->segmentPageCount)*AutoSystemInfo::PageSize), trailingGuardPageCount*AutoSystemInfo::PageSize, MEM_DECOMMIT);
            this->allocator->ReportFree((leadingGuardPageCount + trailingGuardPageCount)*AutoSystemInfo::PageSize);
            this->address = this->address + (leadingGuardPageCount*AutoSystemInfo::PageSize);
        }

        if (!allocator->CreateSecondaryAllocator(this, &this->secondaryAllocator))
        {
            GetAllocator()->GetVirtualAllocator()->Free(originalAddress, GetPageCount() * AutoSystemInfo::PageSize, MEM_RELEASE);
            this->allocator->ReportFree(totalPages * AutoSystemInfo::PageSize);
            this->address = nullptr;
        }
#if defined(_M_X64_OR_ARM64) && defined(RECYCLER_WRITE_BARRIER_BYTE)
        else if (!RecyclerWriteBarrierManager::OnSegmentAlloc(this->address, this->segmentPageCount))
        {
            GetAllocator()->GetVirtualAllocator()->Free(originalAddress, GetPageCount() * AutoSystemInfo::PageSize, MEM_RELEASE);
            this->allocator->ReportFree(totalPages * AutoSystemInfo::PageSize);
            this->address = nullptr;
        }
        else
        {
            this->isWriteBarrierAllowed = true;
        }
#endif
    }

    if (this->address == nullptr)
    {
        this->allocator->ReportFailure(totalPages * AutoSystemInfo::PageSize);
    }

    return (this->address != nullptr);
}

//=============================================================================================================
// PageSegment
//=============================================================================================================

template<typename T>
PageSegmentBase<T>::PageSegmentBase(PageAllocatorBase<T> * allocator, bool external) :
    SegmentBase(allocator, allocator->maxAllocPageCount), decommitPageCount(0)
{
    Assert(this->segmentPageCount == allocator->maxAllocPageCount + allocator->secondaryAllocPageCount);

    if (external)
    {
        this->freePageCount = 0;
        this->ClearAllInFreePagesBitVector();
    }
    else
    {

        this->freePageCount = this->GetAvailablePageCount();
        uint maxPageCount = GetMaxPageCount();

        this->SetRangeInFreePagesBitVector(0, this->freePageCount);
        if (this->freePageCount != maxPageCount)
        {
            this->ClearRangeInFreePagesBitVector(this->freePageCount, (maxPageCount - this->freePageCount));
        }

        Assert(this->GetCountOfFreePages() == this->freePageCount);
    }
}

#ifdef PAGEALLOCATOR_PROTECT_FREEPAGE
template<typename T>
bool
PageSegmentBase<T>::Initialize(DWORD allocFlags, bool excludeGuardPages)
{
    Assert(freePageCount + allocator->secondaryAllocPageCount == this->segmentPageCount || freePageCount == 0);
    if (__super::Initialize(allocFlags, excludeGuardPages))
    {
        if (freePageCount != 0)
        {
            DWORD oldProtect;
            BOOL vpresult = ::VirtualProtect(this->address, this->GetAvailablePageCount() * AutoSystemInfo::PageSize, PAGE_NOACCESS, &oldProtect);
            Assert(vpresult && oldProtect == PAGE_READWRITE);
        }
        return true;
    }
    return false;
}
#endif

template<typename T>
void
PageSegmentBase<T>::Prime()
{
#ifndef PAGEALLOCATOR_PROTECT_FREEPAGE
    for (uint i = 0; i < this->GetAvailablePageCount(); i++)
    {
        this->address[i * AutoSystemInfo::PageSize] = NULL;
    }
#endif
}

template<typename T>
bool
PageSegmentBase<T>::IsAllocationPageAligned(__in char* address, size_t pageCount, PageHeapMode pageHeapFlags)
{
#ifdef RECYCLER_PAGE_HEAP
    if (pageHeapFlags != PageHeapMode::PageHeapModeOff)
    {
        // In PageHeap mode, we should ensure that if the guard page
        // is in the front, the page after the guard page is aligned
        if ((pageHeapFlags == PageHeapMode::PageHeapModeBlockStart))
        {
            address += AutoSystemInfo::PageSize;
        }

        // We don't care about whether the guard pages themselves are aligned
        // or fit in the chunk, so don't count the guard page for the purposes
        // of alignment
        pageCount--;
    }
#endif

    // Require that allocations are aligned at a boundary
    // corresponding to the page count
    // REVIEW: This might actually lead to additional address space fragmentation
    // because of the leading guard pages feature in the page allocator
    // We can restrict the guard pages to be an even number to improve the chances
    // of having the first allocation be aligned but that reduces the effectiveness
    // of having a random number of guard pages
    uintptr_t mask = (pageCount * AutoSystemInfo::PageSize) - 1;
    if ((reinterpret_cast<uintptr_t>(address)& mask) == 0)
    {
        return true;
    }

    return false;
}

template<typename T>
template <bool notPageAligned>
char *
PageSegmentBase<T>::AllocPages(uint pageCount, PageHeapMode pageHeapFlags)
{
    Assert(freePageCount != 0);
    Assert(freePageCount == (uint)this->GetCountOfFreePages());
    if (freePageCount < pageCount)
    {
        return nullptr;
    }
    Assert(!IsFull());

#pragma prefast(push)
#pragma prefast(suppress:__WARNING_LOOP_INDEX_UNDERFLOW, "Prefast about overflow when multiplying index.")
    uint index = this->GetNextBitInFreePagesBitVector(0);
    while (index != -1)
    {
        Assert(index < allocator->GetMaxAllocPageCount());

        if (GetAvailablePageCount() - index < pageCount)
        {
            break;
        }
        if (pageCount == 1 || this->TestRangeInFreePagesBitVector(index, pageCount))
        {
            char * allocAddress = this->address + index * AutoSystemInfo::PageSize;

            if (pageCount > 1 && !notPageAligned)
            {
                if (!IsAllocationPageAligned(allocAddress, pageCount, pageHeapFlags))
                {
                    index = this->freePages.GetNextBit(index + 1);
                    continue;
                }
            }

            this->ClearRangeInFreePagesBitVector(index, pageCount);
            freePageCount -= pageCount;
            Assert(freePageCount == (uint)this->GetCountOfFreePages());

#ifdef PAGEALLOCATOR_PROTECT_FREEPAGE
                DWORD oldProtect;
                BOOL vpresult = ::VirtualProtect(allocAddress, pageCount * AutoSystemInfo::PageSize, PAGE_READWRITE, &oldProtect);
                Assert(vpresult && oldProtect == PAGE_NOACCESS);
#endif
            return allocAddress;
        }
        index = this->GetNextBitInFreePagesBitVector(index + 1);
    }
#pragma prefast(pop)
    return nullptr;
}

#pragma prefast(push)
#pragma prefast(suppress:__WARNING_LOOP_INDEX_UNDERFLOW, "Prefast about overflow when multiplying index.")
template<typename TVirtualAlloc>
template<typename T, bool notPageAligned>
char *
PageSegmentBase<TVirtualAlloc>::AllocDecommitPages(uint pageCount, T freePages, T decommitPages, PageHeapMode pageHeapFlags)
{
    Assert(freePageCount == (uint)this->GetCountOfFreePages());
    Assert(decommitPageCount ==  (uint)this->GetCountOfDecommitPages());
    Assert(decommitPageCount != 0);
    if (freePageCount + decommitPageCount < pageCount)
    {
        return nullptr;
    }
    Assert(secondaryAllocator == nullptr || secondaryAllocator->CanAllocate());

    T freeAndDecommitPages = freePages;

    freeAndDecommitPages.Or(&decommitPages);

    uint oldFreePageCount = freePageCount;
    uint index = freeAndDecommitPages.GetNextBit(0);
    while (index != -1)
    {
        Assert(index < allocator->GetMaxAllocPageCount());

        if (GetAvailablePageCount() - index < pageCount)
        {
            break;
        }
        if (pageCount == 1 || freeAndDecommitPages.TestRange(index, pageCount))
        {
            char * pages = this->address + index * AutoSystemInfo::PageSize;

            if (!notPageAligned)
            {
                if (!IsAllocationPageAligned(pages, pageCount, pageHeapFlags))
                {
                    index = freeAndDecommitPages.GetNextBit(index + 1);
                    continue;
                }
            }

            void * ret = GetAllocator()->GetVirtualAllocator()->Alloc(pages, pageCount * AutoSystemInfo::PageSize, MEM_COMMIT, PAGE_READWRITE, this->IsInCustomHeapAllocator());
            if (ret != nullptr)
            {
                Assert(ret == pages);

                this->ClearRangeInFreePagesBitVector(index, pageCount);
                this->ClearRangeInDecommitPagesBitVector(index, pageCount);

                uint newFreePageCount = this->GetCountOfFreePages();
                freePageCount = freePageCount - oldFreePageCount + newFreePageCount;
                decommitPageCount -= pageCount - (oldFreePageCount - newFreePageCount);

                Assert(freePageCount == (uint)this->GetCountOfFreePages());
                Assert(decommitPageCount == (uint)this->GetCountOfDecommitPages());

                return pages;
            }
            else if (pageCount == 1)
            {
                // if we failed to commit one page, we should just give up.
                return nullptr;
            }
        }
        index = freeAndDecommitPages.GetNextBit(index + 1);
    }

    return nullptr;
}
#pragma prefast(pop)

template<typename T>
void
PageSegmentBase<T>::ReleasePages(__in void * address, uint pageCount)
{
    Assert(address >= this->address);
    Assert(pageCount <= allocator->maxAllocPageCount);
    Assert(((uint)(((char *)address) - this->address)) <= (allocator->maxAllocPageCount - pageCount) *  AutoSystemInfo::PageSize);
    Assert(!IsFreeOrDecommitted(address, pageCount));

    uint base = this->GetBitRangeBase(address);
    this->SetRangeInFreePagesBitVector(base, pageCount);
    this->freePageCount += pageCount;
    Assert(freePageCount == (uint)this->GetCountOfFreePages());

#ifdef PAGEALLOCATOR_PROTECT_FREEPAGE
    DWORD oldProtect;
    BOOL vpresult = ::VirtualProtect(address, pageCount * AutoSystemInfo::PageSize, PAGE_NOACCESS, &oldProtect);
    Assert(vpresult && oldProtect == PAGE_READWRITE);
#endif

}

template<typename T>
void
PageSegmentBase<T>::ChangeSegmentProtection(DWORD protectFlags, DWORD expectedOldProtectFlags)
{
    // TODO: There is a discrepancy in PageSegmentBase
    // The segment page count is initialized in PageSegmentBase::Initialize. It takes into account
    // the guard pages + any additional pages for alignment.
    // However, the free page count is calculated for the segment before initialize is called.
    // In practice, what happens is the following. The initial segment page count is 256. This
    // ends up being the free page count too. When initialize is called, we allocate the guard
    // pages and the alignment pages, which causes the total page count to be 272. The segment
    // page count is then calculated as total - guard, which means 256 <= segmentPageCount < totalPageCount
    // The code in PageSegment's constructor will mark the pages between 256 and 272 as in use,
    // which is why it generally works. However, it breaks in the case where we want to know the end
    // address of the page. It should really be address + 256 * 4k but this->GetEndAddress will return
    // a value greater than that. Need to do a pass through the counts and make sure that it's rational.
    // For now, simply calculate the end address from the allocator's page count
    char* segmentEndAddress = this->address + (this->allocator->GetMaxAllocPageCount() * AutoSystemInfo::PageSize);

    for (char* address = this->address; address < segmentEndAddress; address += AutoSystemInfo::PageSize)
    {
        if (!IsFreeOrDecommitted(address))
        {
            char* endAddress = address;
            do
            {
                endAddress += AutoSystemInfo::PageSize;
            } while (endAddress < segmentEndAddress && !IsFreeOrDecommitted(endAddress));

            Assert(((uintptr_t)(endAddress - address)) < UINT_MAX);
            DWORD regionSize = (DWORD) (endAddress - address);
            DWORD oldProtect = 0;

#if DBG
            MEMORY_BASIC_INFORMATION info = { 0 };
            ::VirtualQuery(address, &info, sizeof(MEMORY_BASIC_INFORMATION));
            Assert(info.Protect == expectedOldProtectFlags);
#endif

            BOOL fSuccess = ::VirtualProtect(address, regionSize, protectFlags, &oldProtect);
            Assert(fSuccess == TRUE);
            Assert(oldProtect == expectedOldProtectFlags);

            address = endAddress;
        }
    }
}

template<typename T>
template <bool onlyUpdateState>
void
PageSegmentBase<T>::DecommitPages(__in void * address, uint pageCount)
{
    Assert(address >= this->address);
    Assert(pageCount <= allocator->maxAllocPageCount);
    Assert(((uint)(((char *)address) - this->address)) <= (allocator->maxAllocPageCount - pageCount) * AutoSystemInfo::PageSize);

    Assert(!IsFreeOrDecommitted(address, pageCount));
    uint base = this->GetBitRangeBase(address);

    this->SetRangeInDecommitPagesBitVector(base, pageCount);
    this->decommitPageCount += pageCount;

    if (!onlyUpdateState)
    {
#pragma warning(suppress: 6250)
        GetAllocator()->GetVirtualAllocator()->Free(address, pageCount * AutoSystemInfo::PageSize, MEM_DECOMMIT);
    }

    Assert(decommitPageCount == (uint)this->GetCountOfDecommitPages());
}

template<typename T>
size_t
PageSegmentBase<T>::DecommitFreePages(size_t pageToDecommit)
{
    Assert(pageToDecommit != 0);
    char * currentAddress = this->address;

    uint decommitCount = 0;
    for (uint i = 0; i < this->GetAvailablePageCount(); i++)
    {
        if (this->TestInFreePagesBitVector(i))
        {
            this->ClearBitInFreePagesBitVector(i);
            this->SetBitInDecommitPagesBitVector(i);
#pragma warning(suppress: 6250)
            GetAllocator()->GetVirtualAllocator()->Free(currentAddress, AutoSystemInfo::PageSize, MEM_DECOMMIT);
            decommitCount++;
        }
        currentAddress += AutoSystemInfo::PageSize;
        if (decommitCount == pageToDecommit)
        {
            break;
        }
    }
    Assert(decommitCount <= this->freePageCount);
    this->decommitPageCount += decommitCount;
    this->freePageCount -= decommitCount;
    return decommitCount;
}

//=============================================================================================================
// PageAllocator
//=============================================================================================================
#if DBG
#define ASSERT_THREAD() AssertMsg(ValidThreadAccess(), "Page allocation should only be used by a single thread");
#else
#define ASSERT_THREAD()
#endif

/*
 * Global counter to keep track of the total used bytes by the page allocator
 * per process for performance tooling. This is reported through the
 * JSCRIPT_PAGE_ALLOCATOR_USED_SIZE ETW event.
 */

static size_t totalUsedBytes = 0;
static size_t maxUsedBytes = 0;


template<typename T>
size_t PageAllocatorBase<T>::GetAndResetMaxUsedBytes()
{
    size_t value = maxUsedBytes;
    maxUsedBytes = 0;
    return value;
}

template<typename T>
size_t
PageAllocatorBase<T>::GetProcessUsedBytes()
{
    return totalUsedBytes;
}

template<typename T>
PageAllocatorBase<T>::BackgroundPageQueue::BackgroundPageQueue()
{
    ::InitializeSListHead(&freePageList);
    DebugOnly(this->isZeroPageQueue = false);
}

template<typename T>
PageAllocatorBase<T>::ZeroPageQueue::ZeroPageQueue()
{
    ::InitializeSListHead(&pendingZeroPageList);
    DebugOnly(this->isZeroPageQueue = true);
}

template<typename T>
uint
PageAllocatorBase<T>::GetMaxAllocPageCount()
{
    return maxAllocPageCount;
}

template<typename T>
PageAllocatorBase<T>::PageAllocatorBase(AllocationPolicyManager * policyManager,
#ifndef JD_PRIVATE
    Js::ConfigFlagsTable& flagTable,
#endif
    PageAllocatorType type,
    uint maxFreePageCount, bool zeroPages,  BackgroundPageQueue * backgroundPageQueue, uint maxAllocPageCount, uint secondaryAllocPageCount,
    bool stopAllocationOnOutOfMemory, bool excludeGuardPages) :
    policyManager(policyManager),
#ifndef JD_PRIVATE
    pageAllocatorFlagTable(flagTable),
#endif
    maxFreePageCount(maxFreePageCount),
    freePageCount(0),
    allocFlags(0),
    zeroPages(zeroPages),
    queueZeroPages(false),
    hasZeroQueuedPages(false),
    backgroundPageQueue(backgroundPageQueue),
    minFreePageCount(0),
    isUsed(false),
    idleDecommitEnterCount(1),
    isClosed(false),
    stopAllocationOnOutOfMemory(stopAllocationOnOutOfMemory),
    disableAllocationOutOfMemory(false),
    secondaryAllocPageCount(secondaryAllocPageCount),
    excludeGuardPages(excludeGuardPages),
    virtualAllocator(nullptr),
    type(type)
    , reservedBytes(0)
    , committedBytes(0)
    , usedBytes(0)
    , numberOfSegments(0)
{
    AssertMsg(Math::IsPow2(maxAllocPageCount + secondaryAllocPageCount), "Illegal maxAllocPageCount: Why is this not a power of 2 aligned?");

    this->maxAllocPageCount = maxAllocPageCount;

#if DBG
    // By default, a page allocator is not associated with any thread context
    // Any host which wishes to associate it with a thread context must do so explicitly
    this->threadContextHandle = NULL;
    this->concurrentThreadId = (DWORD)-1;
#endif
#if DBG
    this->disableThreadAccessCheck = false;
    this->debugMinFreePageCount = 0;
#endif
#if DBG_DUMP
    this->decommitPageCount = 0;
    this->debugName = nullptr;
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    this->verifyEnabled = false;
    this->disablePageReuse = false;
#endif

#ifdef PROFILE_MEM
    this->memoryData = MemoryProfiler::GetPageMemoryData(type);
#endif

    PageTracking::PageAllocatorCreated((PageAllocator*)this);
}

template<typename T>
PageAllocatorBase<T>::~PageAllocatorBase()
{
    AssertMsg(this->ValidThreadAccess(), "Page allocator tear-down should only happen on the owning thread");

#if DBG
    Assert(!HasMultiThreadAccess());
#endif

    SubUsedBytes(usedBytes);

    SubCommittedBytes(committedBytes);
    SubReservedBytes(reservedBytes);

    ReleaseSegmentList(&segments);
    ReleaseSegmentList(&fullSegments);
    ReleaseSegmentList(&emptySegments);
    ReleaseSegmentList(&decommitSegments);
    ReleaseSegmentList(&largeSegments);

    PageTracking::PageAllocatorDestroyed((PageAllocator*)this);
}

template<typename T>
void
PageAllocatorBase<T>::StartQueueZeroPage()
{
    Assert(HasZeroPageQueue());
    Assert(!queueZeroPages);
    queueZeroPages = true;
}

template<typename T>
void
PageAllocatorBase<T>::StopQueueZeroPage()
{
    Assert(HasZeroPageQueue());
    Assert(queueZeroPages);
    queueZeroPages = false;
}

template<typename T>
bool
PageAllocatorBase<T>::HasZeroPageQueue() const
{
    bool hasZeroPageQueue = (ZeroPages() && this->backgroundPageQueue != nullptr);
    Assert(backgroundPageQueue == nullptr || hasZeroPageQueue == backgroundPageQueue->isZeroPageQueue);
    return hasZeroPageQueue;
}

#if DBG
template<typename T>
bool
PageAllocatorBase<T>::HasZeroQueuedPages() const
{
    Assert(!HasZeroPageQueue() || hasZeroQueuedPages ||
        ::QueryDepthSList(&(((ZeroPageQueue *)this->backgroundPageQueue)->pendingZeroPageList)) == 0);
    return hasZeroQueuedPages;
}
#endif

template<typename T>
PageAllocation *
PageAllocatorBase<T>::AllocPagesForBytes(size_t requestBytes)
{
    Assert(!isClosed);
    ASSERT_THREAD();

    uint pageSize = AutoSystemInfo::PageSize;
    uint addSize = sizeof(PageAllocation) + pageSize - 1;   // this shouldn't overflow
    // overflow check
    size_t allocSize = AllocSizeMath::Add(requestBytes, addSize);
    if (allocSize == (size_t)-1)
    {
        return nullptr;
    }

    size_t pages = allocSize / pageSize;

    return this->AllocAllocation(pages);
}

template<typename T>
PageSegmentBase<T> *
PageAllocatorBase<T>::AllocPageSegment(DListBase<PageSegmentBase<T>>& segmentList, PageAllocatorBase<T> * pageAllocator, bool external)
{
    PageSegmentBase<T> * segment = segmentList.PrependNode(&NoThrowNoMemProtectHeapAllocator::Instance, pageAllocator, external);

    if (segment == nullptr)
    {
        return nullptr;
    }

    if (!segment->Initialize((external ? 0 : MEM_COMMIT) | pageAllocator->allocFlags, pageAllocator->excludeGuardPages))
    {
        segmentList.RemoveHead(&NoThrowNoMemProtectHeapAllocator::Instance);
        return nullptr;
    }
    return segment;
}

template<typename T>
PageSegmentBase<T> *
PageAllocatorBase<T>::AddPageSegment(DListBase<PageSegmentBase<T>>& segmentList)
{
    Assert(!HasMultiThreadAccess());

    PageSegmentBase<T> * segment = AllocPageSegment(segmentList, this, false);

    if (segment != nullptr)
    {
        LogAllocSegment(segment);
        this->AddFreePageCount(maxAllocPageCount);
    }
    return segment;
}

template<>
char *
HeapPageAllocator<PreReservedVirtualAllocWrapper>::InitPageSegment()
{
    Assert(virtualAllocator);
    PageSegmentBase<PreReservedVirtualAllocWrapper> * firstPreReservedSegment = AddPageSegment(emptySegments);
    if (firstPreReservedSegment == nullptr)
    {
        return nullptr;
    }
    return firstPreReservedSegment->GetAddress();
}

template<>
char *
HeapPageAllocator<VirtualAllocWrapper>::InitPageSegment()
{
    Assert(false);
    return nullptr;
}

template<typename T>
PageSegmentBase<T> *
HeapPageAllocator<T>::AddPageSegment(DListBase<PageSegmentBase<T>>& segmentList)
{
    Assert(!HasMultiThreadAccess());

    PageSegmentBase<T> * segment = AllocPageSegment(segmentList, this, false);

    if (segment != nullptr)
    {
        LogAllocSegment(segment);
        this->AddFreePageCount(maxAllocPageCount);
    }
    return segment;
}

template<typename T>
template <bool notPageAligned>
char *
PageAllocatorBase<T>::TryAllocFreePages(uint pageCount, PageSegmentBase<T> ** pageSegment, PageHeapMode pageHeapFlags)
{
    Assert(!HasMultiThreadAccess());
    if (this->freePageCount < pageCount)
    {
        return nullptr;
    }

    FAULTINJECT_MEMORY_NOTHROW(this->debugName, pageCount*4096);
    DListBase<PageSegmentBase<T>>::EditingIterator i(&segments);

    while (i.Next())
    {
        PageSegmentBase<T> * freeSegment = &i.Data();

        char * pages = freeSegment->AllocPages<notPageAligned>(pageCount, pageHeapFlags);
        if (pages != nullptr)
        {
            LogAllocPages(pageCount);
            if (freeSegment->GetFreePageCount() == 0)
            {
                i.MoveCurrentTo(&fullSegments);
            }

            this->freePageCount -= pageCount;
            *pageSegment = freeSegment;

#if DBG
            UpdateMinimum(this->debugMinFreePageCount, this->freePageCount);
#endif
            this->FillAllocPages(pages, pageCount);
            return pages;
        }
    }

    if (pageCount == 1 && backgroundPageQueue != nullptr)
    {
        FreePageEntry * freePage = (FreePageEntry *)::InterlockedPopEntrySList(&backgroundPageQueue->freePageList);
        if (freePage != nullptr)
        {
#if DBG
            UpdateMinimum(this->debugMinFreePageCount, this->freePageCount);
#endif
            *pageSegment = freePage->segment;
            char * pages;
            if (freePage->pageCount != 1)
            {
                uint pageIndex = --freePage->pageCount;
                ::InterlockedPushEntrySList(&backgroundPageQueue->freePageList, freePage);
                pages = (char *)freePage + pageIndex * AutoSystemInfo::PageSize;
            }
            else
            {
                pages = (char *)freePage;
                memset(pages, 0, sizeof(FreePageEntry));
            }
            this->FillAllocPages(pages, pageCount);
            return (char *)pages;
        }
    }

    return nullptr;
}

template<typename T>
void
PageAllocatorBase<T>::FillAllocPages(__in void * address, uint pageCount)
{

#if DBG
#ifdef RECYCLER_ZERO_MEM_CHECK
    for (size_t i = 0; i < AutoSystemInfo::PageSize * pageCount; i++)
    {
        // new pages are filled with zeros, old pages are filled with DbgMemFill
        Assert(((byte *)address)[i] == 0 || ((byte *)address)[i] == DbgMemFill);
    }
#endif
#endif

#ifdef RECYCLER_MEMORY_VERIFY
    if (verifyEnabled)
    {
        memset(address, Recycler::VerifyMemFill, AutoSystemInfo::PageSize * pageCount);
        return;
    }
#endif

#if DBG
    if (ZeroPages())
    {
        // for release build, the page is zeroed in ReleasePages
        memset(address, 0, AutoSystemInfo::PageSize * pageCount);
    }
#endif
}

template<typename T>
void
PageAllocatorBase<T>::FillFreePages(__in void * address, uint pageCount)
{
#if DBG
    memset(address, DbgMemFill, AutoSystemInfo::PageSize * pageCount);
#else
#ifdef RECYCLER_MEMORY_VERIFY
    if (verifyEnabled)
    {
        return;
    }
#endif
    if (ZeroPages())
    {
        memset(address, 0, AutoSystemInfo::PageSize * pageCount);
    }
#endif

}

template<typename T>
template <bool notPageAligned>
char *
PageAllocatorBase<T>::TryAllocDecommitedPages(uint pageCount, PageSegmentBase<T> ** pageSegment, PageHeapMode pageHeapFlags)
{
    Assert(!HasMultiThreadAccess());

    DListBase<PageSegmentBase<T>>::EditingIterator i(&decommitSegments);

    while (i.Next())
    {
        PageSegmentBase<T> * freeSegment = &i.Data();
        uint oldFreePageCount = freeSegment->GetFreePageCount();
        uint oldDecommitPageCount = freeSegment->GetDecommitPageCount();
        char * pages = freeSegment->DoAllocDecommitPages<notPageAligned>(pageCount, pageHeapFlags);
        if (pages != nullptr)
        {
            this->freePageCount = this->freePageCount - oldFreePageCount + freeSegment->GetFreePageCount();

#if DBG_DUMP
            this->decommitPageCount = this->decommitPageCount - oldDecommitPageCount + freeSegment->GetDecommitPageCount();
#endif
#if DBG
            UpdateMinimum(this->debugMinFreePageCount, this->freePageCount);
#endif
            uint recommitPageCount = pageCount - (oldFreePageCount - freeSegment->GetFreePageCount());
            LogRecommitPages(recommitPageCount);
            LogAllocPages(pageCount);

            if (freeSegment->GetDecommitPageCount() == 0)
            {
                auto toList = GetSegmentList(freeSegment);
                i.MoveCurrentTo(toList);
            }

            *pageSegment = freeSegment;
            return pages;
        }
    }
    return nullptr;
}

template<typename T>
PageAllocation *
PageAllocatorBase<T>::AllocAllocation(size_t pageCount)
{
    PageAllocation * pageAllocation;
    SegmentBase<T> * segment;
    if (pageCount > this->maxAllocPageCount)
    {
        // We need some space reserved for secondary allocations
        segment = AllocSegment(pageCount);
        if (segment == nullptr)
        {
            return nullptr;
        }
        pageAllocation = (PageAllocation *)segment->GetAddress();
        pageAllocation->pageCount = segment->GetAvailablePageCount();
    }
    else
    {
        Assert(pageCount <= UINT_MAX);
        pageAllocation = (PageAllocation *)AllocPages((uint)pageCount, (PageSegmentBase<T> **)&segment);
        if (pageAllocation == nullptr)
        {
            return nullptr;
        }
        pageAllocation->pageCount = pageCount;
    }
    pageAllocation->segment = segment;

    return pageAllocation;
}

template<typename T>
SegmentBase<T> *
PageAllocatorBase<T>::AllocSegment(size_t pageCount)
{
    Assert(!isClosed);
    ASSERT_THREAD();

    // Even though we don't idle decommit large segments, we still need to consider these allocations
    // as using the page allocator
    this->isUsed = true;

    SegmentBase<T> * segment = largeSegments.PrependNode(&NoThrowNoMemProtectHeapAllocator::Instance, this, pageCount);
    if (segment == nullptr)
    {
        return nullptr;
    }
    if (!segment->Initialize(MEM_COMMIT | allocFlags, excludeGuardPages))
    {
        largeSegments.RemoveHead(&NoThrowNoMemProtectHeapAllocator::Instance);
        return nullptr;
    }

    LogAllocSegment(segment);
    LogAllocPages(segment->GetPageCount());

    PageTracking::ReportAllocation((PageAllocator*)this, segment->GetAddress(), AutoSystemInfo::PageSize * segment->GetPageCount());
#ifdef RECYCLER_MEMORY_VERIFY
    if (verifyEnabled)
    {
        memset(segment->GetAddress(), Recycler::VerifyMemFill, AutoSystemInfo::PageSize * segment->GetPageCount());
    }
#endif

    return segment;
}

char *
PageAllocatorBase<VirtualAllocWrapper>::Alloc(size_t * pageCount, SegmentBase<VirtualAllocWrapper> ** segment)
{
    Assert(virtualAllocator == nullptr);
    return AllocInternal<false>(pageCount, segment);
}

char *
PageAllocatorBase<PreReservedVirtualAllocWrapper>::Alloc(size_t * pageCount, SegmentBase<PreReservedVirtualAllocWrapper> ** segment)
{
    Assert(virtualAllocator);
    if (virtualAllocator->IsPreReservedRegionPresent())
    {
        return AllocInternal<false>(pageCount, segment);
    }
    else
    {
        return nullptr;
    }
}

template<typename T>
template <bool doPageAlign>
char *
PageAllocatorBase<T>::AllocInternal(size_t * pageCount, SegmentBase<T> ** segment)
{
    char * addr = nullptr;

    if (*pageCount > this->maxAllocPageCount)
    {
        // Don't bother trying to do single chunk allocation here
        // We're allocating a new segment. If the segment size is
        // within a single chunk, great, otherwise, doesn't matter

        // We need some space reserved for secondary allocations
        SegmentBase<T> * newSegment = this->AllocSegment(*pageCount);
        if (newSegment != nullptr)
        {
            addr = newSegment->GetAddress();
            *pageCount = newSegment->GetAvailablePageCount();
            *segment = newSegment;
        }
    }
    else
    {
        Assert(*pageCount <= UINT_MAX);
        PageSegmentBase<T> * pageSegment;

        if (doPageAlign)
        {
            // TODO: Remove this entire codepath since doPageAlign is not being used anymore
            addr = this->AllocPagesPageAligned((uint)*pageCount, &pageSegment, PageHeapMode::PageHeapModeOff);
        }
        else
        {
            addr = this->AllocPages((uint) *pageCount, &pageSegment);
        }

        if (addr != nullptr)
        {
            *segment = pageSegment;
        }
    }
    return addr;
}

template<typename T>
void PageAllocatorBase<T>::UpdateMinFreePageCount()
{
    UpdateMinimum(minFreePageCount, freePageCount);
    Assert(debugMinFreePageCount == minFreePageCount);
}

template<typename T>
void PageAllocatorBase<T>::ResetMinFreePageCount()
{
    minFreePageCount = freePageCount;
#if DBG
    debugMinFreePageCount = freePageCount;
#endif
}

template<typename T>
void PageAllocatorBase<T>::ClearMinFreePageCount()
{
    minFreePageCount = 0;
#if DBG
    debugMinFreePageCount = 0;
#endif
}

template<>
char *
PageAllocatorBase<VirtualAllocWrapper>::AllocPages(uint pageCount, PageSegmentBase<VirtualAllocWrapper> ** pageSegment)
{
    Assert(virtualAllocator == nullptr);
    return AllocPagesInternal<true /* noPageAligned */>(pageCount, pageSegment);
}

template<>
char *
PageAllocatorBase<PreReservedVirtualAllocWrapper>::AllocPages(uint pageCount, PageSegmentBase<PreReservedVirtualAllocWrapper> ** pageSegment)
{
    Assert(virtualAllocator);
    if (virtualAllocator->IsPreReservedRegionPresent())
    {
        return AllocPagesInternal<true /* noPageAligned */>(pageCount, pageSegment);
    }
    else
    {
        return nullptr;
    }
}

template<typename T>
char *
PageAllocatorBase<T>::AllocPagesPageAligned(uint pageCount, PageSegmentBase<T> ** pageSegment, PageHeapMode pageHeapFlags)
{
    return AllocPagesInternal<false /* noPageAligned */>(pageCount, pageSegment, pageHeapFlags);
}

template<typename T>
template <bool notPageAligned>
char *
PageAllocatorBase<T>::AllocPagesInternal(uint pageCount, PageSegmentBase<T> ** pageSegment, PageHeapMode pageHeapModeFlags)
{
    Assert(!isClosed);
    ASSERT_THREAD();
    Assert(pageCount <= this->maxAllocPageCount);

    this->isUsed = true;

    SuspendIdleDecommit();
    char * allocation = TryAllocFreePages<notPageAligned>(pageCount, pageSegment, pageHeapModeFlags);
    if (allocation == nullptr)
    {
        allocation = SnailAllocPages<notPageAligned>(pageCount, pageSegment, pageHeapModeFlags);
    }
    ResumeIdleDecommit();

    PageTracking::ReportAllocation((PageAllocator*)this, allocation, AutoSystemInfo::PageSize * pageCount);

    return allocation;
}

template<typename T>
void
PageAllocatorBase<T>::OnAllocFromNewSegment(uint pageCount, __in void* pages, SegmentBase<T>* newSegment)
{
    DListBase<PageSegmentBase<T>>* targetSegmentList = (pageCount == maxAllocPageCount) ? &fullSegments : &segments;
    LogAllocPages(pageCount);

    this->FillAllocPages(pages, pageCount);
    this->freePageCount -= pageCount;
#if DBG
    UpdateMinimum(this->debugMinFreePageCount, this->freePageCount);
#endif

    Assert(targetSegmentList != nullptr);
    emptySegments.MoveHeadTo(targetSegmentList);
}

template<typename T>
template <bool notPageAligned>
char *
PageAllocatorBase<T>::SnailAllocPages(uint pageCount, PageSegmentBase<T> ** pageSegment, PageHeapMode pageHeapFlags)
{
    Assert(!HasMultiThreadAccess());

    char * pages = nullptr;
    PageSegmentBase<T> * newSegment = nullptr;

    if (!emptySegments.Empty())
    {
        newSegment = &emptySegments.Head();

        if (!notPageAligned && !PageSegmentBase<T>::IsAllocationPageAligned(newSegment->GetAddress(), pageCount, pageHeapFlags))
        {
            newSegment = nullptr;

            // Scan through the empty segments for a segment that can fit this allocation
            FOREACH_DLISTBASE_ENTRY_EDITING(PageSegmentBase<T>, emptySegment, &this->emptySegments, iter)
            {
                if (PageSegmentBase<T>::IsAllocationPageAligned(emptySegment.GetAddress(), pageCount, pageHeapFlags))
                {
                    iter.MoveCurrentTo(&this->emptySegments);
                    newSegment = &emptySegment;
                    break;
                }
            }
            NEXT_DLISTBASE_ENTRY_EDITING
        }

        if (newSegment != nullptr)
        {
            pages = newSegment->AllocPages<notPageAligned>(pageCount, pageHeapFlags);
            if (pages != nullptr)
            {
                OnAllocFromNewSegment(pageCount, pages, newSegment);
                *pageSegment = newSegment;
                return pages;
            }
        }
    }

    pages = TryAllocDecommitedPages<notPageAligned>(pageCount, pageSegment, pageHeapFlags);
    if (pages != nullptr)
    {
        // TryAllocDecommitedPages may give out a mix of free pages and decommitted pages.
        // Free pages are filled with 0xFE in debug build, so we need to zero them
        // out before giving it out. In release build, free page is already zeroed
        // in ReleasePages
        this->FillAllocPages(pages, pageCount);
        return pages;
    }

    Assert(pages == nullptr);

    // At this point, we haven't been able to allocate either from the
    // decommitted pages, or from the empty segment list, so we'll
    // try allocating a segment. In a page allocator with a pre-reserved segment,
    // we're not allowed to allocate additional segments so return here.
    // Otherwise, add a new segment and allocate from it

    newSegment = AddPageSegment(emptySegments);
    if (newSegment == nullptr)
    {
        return nullptr;
    }

    pages = newSegment->AllocPages<notPageAligned>(pageCount, pageHeapFlags);
    if (notPageAligned)
    {
        // REVIEW: Is this true for single-chunk allocations too? Are new segments guaranteed to
        // allow for single-chunk allocations to succeed?
        Assert(pages != nullptr);
    }

    if (pages != nullptr)
    {
        OnAllocFromNewSegment(pageCount, pages, newSegment);
        *pageSegment = newSegment;
    }

    return pages;
}

template<typename T>
DListBase<PageSegmentBase<T>> *
PageAllocatorBase<T>::GetSegmentList(PageSegmentBase<T> * segment)
{
    Assert(!HasMultiThreadAccess());

    return
        (segment->IsAllDecommitted()) ? nullptr :
        (segment->IsFull()) ? &fullSegments :
        (segment->ShouldBeInDecommittedList()) ? &decommitSegments :
        (segment->IsEmpty()) ? &emptySegments :
        &segments;
}

template<typename T>
void
PageAllocatorBase<T>::ReleaseAllocation(PageAllocation * allocation)
{
    SuspendIdleDecommit();
    ReleaseAllocationNoSuspend(allocation);
    ResumeIdleDecommit();
}

template<typename T>
void
PageAllocatorBase<T>::ReleaseAllocationNoSuspend(PageAllocation * allocation)
{
    this->Release((char *)allocation, allocation->pageCount, allocation->segment);
}

template<typename T>
void
PageAllocatorBase<T>::Release(void * address, size_t pageCount, void * segmentParam)
{
    SegmentBase<T> * segment = (SegmentBase<T>*)segmentParam;
    Assert(!HasMultiThreadAccess());
    Assert(segment->GetAllocator() == this);
    if (pageCount > this->maxAllocPageCount)
    {
        Assert(address == segment->GetAddress());
        Assert(pageCount == segment->GetAvailablePageCount());
        this->ReleaseSegment(segment);
    }
    else
    {
        Assert(pageCount <= UINT_MAX);
        this->ReleasePages(address, static_cast<uint>(pageCount), (PageSegmentBase<T> *)segment);
    }
}

template<typename T>
void
PageAllocatorBase<T>::ReleaseSegment(SegmentBase<T> * segment)
{
    ASSERT_THREAD();
#if defined(RECYCLER_MEMORY_VERIFY) || defined(ARENA_MEMORY_VERIFY)
    if (disablePageReuse)
    {
        DWORD oldProtect;
        BOOL vpresult = ::VirtualProtect(segment->GetAddress(), segment->GetPageCount() * AutoSystemInfo::PageSize, PAGE_NOACCESS, &oldProtect);
        Assert(vpresult && oldProtect == PAGE_READWRITE);
        return;
    }
#endif
    PageTracking::ReportFree((PageAllocator*)this, segment->GetAddress(), AutoSystemInfo::PageSize * segment->GetPageCount());
    LogFreePages(segment->GetPageCount());
    LogFreeSegment(segment);
    largeSegments.RemoveElement(&NoThrowNoMemProtectHeapAllocator::Instance, segment);
}

template<typename T>
void
PageAllocatorBase<T>::AddFreePageCount(uint pageCount)
{
    // minFreePageCount is only updated on release of a page or before decommit
    // so that we don't have to update it on every page allocation.
    UpdateMinFreePageCount();
    this->freePageCount += pageCount;
}

template<typename T>
void
PageAllocatorBase<T>::ReleasePages(__in void * address, uint pageCount, __in void * segmentParam)
{
    Assert(pageCount <= this->maxAllocPageCount);
    PageSegmentBase<T> * segment = (PageSegmentBase<T>*) segmentParam;
    ASSERT_THREAD();
    Assert(!HasMultiThreadAccess());

#if defined(RECYCLER_MEMORY_VERIFY) || defined(ARENA_MEMORY_VERIFY)
    if (disablePageReuse)
    {
        DWORD oldProtect;
        BOOL vpresult = ::VirtualProtect(address, pageCount * AutoSystemInfo::PageSize, PAGE_NOACCESS, &oldProtect);
        Assert(vpresult && oldProtect == PAGE_READWRITE);
        return;
    }
#endif

    PageTracking::ReportFree((PageAllocator*)this, address, AutoSystemInfo::PageSize * pageCount);

    DListBase<PageSegmentBase<T>> * fromSegmentList = GetSegmentList(segment);
    Assert(fromSegmentList != nullptr);

    /**
     * The logic here is as follows:
     * - If we have sufficient pages already, such that the newly free pages are going
     *   to cause us to exceed the threshold of free pages we want:
     *     - First check and see if we have empty segments. If we do, just release that
     *       entire segment back to the operating system, and add the current segments
     *       free pages to our free page pool
     *     - Otherwise, if there are no empty segments (i.e our memory is fragmented),
     *       decommit the pages that are being released so that they don't count towards
     *       our working set
     * - If we don't have enough pages:
     *    - If we're in the free page queuing mode where we have a "pages to zero out" queue
     *      put it in that queue and we're done
     *    - Otherwise, zero it out, and add it to the free page pool
     *  Now that we've either decommited or freed the pages in the segment,
     *  move the segment to the right segment list
     */
    if (this->freePageCount + pageCount > maxFreePageCount)
    {
        // Release a whole segment if possible to reduce the number of VirtualFree and fragmentation
        if (!ZeroPages() && !emptySegments.Empty())
        {
            Assert(emptySegments.Head().GetDecommitPageCount() == 0);
            LogFreeSegment(&emptySegments.Head());
            emptySegments.RemoveHead(&NoThrowNoMemProtectHeapAllocator::Instance);
            this->freePageCount -= maxAllocPageCount;

#if DBG
            UpdateMinimum(this->debugMinFreePageCount, this->freePageCount);
            memset(address, DbgMemFill, AutoSystemInfo::PageSize * pageCount);
#endif
            segment->ReleasePages(address, pageCount);
            LogFreePages(pageCount);
            this->AddFreePageCount(pageCount);

        }
        else
        {
            segment->DecommitPages<false>(address, pageCount);
            LogFreePages(pageCount);
            LogDecommitPages(pageCount);
#if DBG_DUMP
            this->decommitPageCount += pageCount;
#endif
        }
    }
    else
    {
        if (QueueZeroPages())
        {
            Assert(HasZeroPageQueue());
            AddPageToZeroQueue(address, pageCount, segment);
            return;
        }

        this->FillFreePages((char *)address, pageCount);
        segment->ReleasePages(address, pageCount);
        LogFreePages(pageCount);
        this->AddFreePageCount(pageCount);
    }

    TransferSegment(segment, fromSegmentList);
}

template<class T>
typename PageAllocatorBase<T>::FreePageEntry *
PageAllocatorBase<T>::PopPendingZeroPage()
{
    Assert(HasZeroPageQueue());
    return (PageAllocatorBase<T>::FreePageEntry *)::InterlockedPopEntrySList(&(((PageAllocatorBase<T>::ZeroPageQueue *) backgroundPageQueue)->pendingZeroPageList));
}

template<typename T>
void
PageAllocatorBase<T>::AddPageToZeroQueue(__in void * address, uint pageCount, __in PageSegmentBase<T> * pageSegment)
{
    Assert(HasZeroPageQueue());
    Assert(pageSegment->GetAllocator() == this);
    FreePageEntry * entry = (FreePageEntry *)address;
    entry->segment = pageSegment;
    entry->pageCount = pageCount;
    ::InterlockedPushEntrySList(&(((ZeroPageQueue *)backgroundPageQueue)->pendingZeroPageList), entry);
    this->hasZeroQueuedPages = true;
}

template<typename T>
void
PageAllocatorBase<T>::TransferSegment(PageSegmentBase<T> * segment, DListBase<PageSegmentBase<T>> * fromSegmentList)
{
    DListBase<PageSegmentBase<T>> * toSegmentList = GetSegmentList(segment);

    if (fromSegmentList != toSegmentList)
    {
        if (toSegmentList)
        {
            AssertMsg(segment->GetSecondaryAllocator() == nullptr  || fromSegmentList != &fullSegments || segment->GetSecondaryAllocator()->CanAllocate(),
                "If it's being moved from a full segment it should be able to do secondary allocations");
            fromSegmentList->MoveElementTo(segment, toSegmentList);
        }
        else
        {
            LogFreePartiallyDecommitedPageSegment(segment);
            fromSegmentList->RemoveElement(&NoThrowNoMemProtectHeapAllocator::Instance, segment);
#if DBG_DUMP
            this->decommitPageCount -= maxAllocPageCount;
#endif
        }
    }
}

template<typename T>
void
PageAllocatorBase<T>::BackgroundZeroQueuedPages()
{
    Assert(HasZeroPageQueue());
    AutoCriticalSection autocs(&backgroundPageQueue->backgroundPageQueueCriticalSection);
    ZeroQueuedPages();
}

template<typename T>
void
PageAllocatorBase<T>::ZeroQueuedPages()
{
    Assert(HasZeroPageQueue());

    while (true)
    {
        FreePageEntry * freePageEntry = PopPendingZeroPage();
        if (freePageEntry == nullptr)
        {
            break;
        }
        PageSegmentBase<T> * segment = freePageEntry->segment;
        uint pageCount = freePageEntry->pageCount;
        memset(freePageEntry, 0, pageCount * AutoSystemInfo::PageSize);
        QueuePages(freePageEntry, pageCount, segment);
    }
    this->hasZeroQueuedPages = false;
}

template<typename T>
void
PageAllocatorBase<T>::BackgroundReleasePages(void * address, uint pageCount, PageSegmentBase<T> * segment)
{
    FillFreePages(address, pageCount);
    QueuePages(address, pageCount, segment);
}

template<typename T>
void
PageAllocatorBase<T>::QueuePages(void * address, uint pageCount, PageSegmentBase<T> * segment)
{
    Assert(backgroundPageQueue);
    FreePageEntry * freePageEntry = (FreePageEntry *)address;
    freePageEntry->segment = segment;
    freePageEntry->pageCount = pageCount;
    ::InterlockedPushEntrySList(&backgroundPageQueue->freePageList, freePageEntry);
}

template<typename T>
void
PageAllocatorBase<T>::FlushBackgroundPages()
{
    Assert(!HasMultiThreadAccess());
    Assert(backgroundPageQueue);

    // We can have additional pages queued up to be zeroed out here
    // and that's okay since they'll eventually be zeroed out before being flushed

    uint newFreePages = 0;

    while (true)
    {
        FreePageEntry * freePageEntry = (FreePageEntry *)::InterlockedPopEntrySList(&backgroundPageQueue->freePageList);
        if (freePageEntry == nullptr)
        {
            break;
        }
        PageSegmentBase<T> * segment = freePageEntry->segment;
        uint pageCount = freePageEntry->pageCount;

        DListBase<PageSegmentBase<T>> * fromSegmentList = GetSegmentList(segment);
        Assert(fromSegmentList != nullptr);

        memset(freePageEntry, 0, sizeof(FreePageEntry));

        segment->ReleasePages(freePageEntry, pageCount);
        newFreePages += pageCount;

        TransferSegment(segment, fromSegmentList);
    }

    LogFreePages(newFreePages);

    PAGE_ALLOC_VERBOSE_TRACE(L"New free pages: %d\n", newFreePages);
    this->AddFreePageCount(newFreePages);
}

template<typename T>
void
PageAllocatorBase<T>::SuspendIdleDecommit()
{
#ifdef IDLE_DECOMMIT_ENABLED
    if (this->idleDecommitEnterCount != 0)
    {
        return;
    }
    Assert(this->IsIdleDecommitPageAllocator());
    ((IdleDecommitPageAllocator *)this)->cs.Enter();
    PAGE_ALLOC_VERBOSE_TRACE(L"SuspendIdleDecommit");
#endif
}

template<typename T>
void
PageAllocatorBase<T>::ResumeIdleDecommit()
{
#ifdef IDLE_DECOMMIT_ENABLED
    if (this->idleDecommitEnterCount != 0)
    {
        return;
    }
    Assert(this->IsIdleDecommitPageAllocator());
    PAGE_ALLOC_VERBOSE_TRACE(L"ResumeIdleDecommit");
    ((IdleDecommitPageAllocator *)this)->cs.Leave();
#endif
}

template<typename T>
void
PageAllocatorBase<T>::DecommitNow(bool all)
{
    Assert(!HasMultiThreadAccess());

#if DBG_DUMP
    size_t deleteCount = 0;
#endif
    // First, drain the zero page queue.
    // This will cause the free page count to be accurate
    if (HasZeroPageQueue())
    {
        int numZeroPagesFreed = 0;

        // There might be queued zero pages.  Drain them first

        while (true)
        {
            FreePageEntry * freePageEntry = PopPendingZeroPage();
            if (freePageEntry == nullptr)
            {
                break;
            }
            PAGE_ALLOC_TRACE_AND_STATS(L"Freeing page from zero queue");
            PageSegmentBase<T> * segment = freePageEntry->segment;
            uint pageCount = freePageEntry->pageCount;

            numZeroPagesFreed += pageCount;

            DListBase<PageSegmentBase<T>> * fromSegmentList = GetSegmentList(segment);
            Assert(fromSegmentList != nullptr);

            // Check for all here, since the actual free page count can't be determined
            // until we've flushed the zeroed page queue
            if (all)
            {
                // Decommit them immediately if we are decommitting all pages.
                segment->DecommitPages<false>(freePageEntry, pageCount);
                LogFreePages(pageCount);
                LogDecommitPages(pageCount);

                if (segment->IsAllDecommitted())
                {
                    LogFreePartiallyDecommitedPageSegment(segment);
                    fromSegmentList->RemoveElement(&NoThrowNoMemProtectHeapAllocator::Instance, segment);
#if DBG_DUMP
                    deleteCount += maxAllocPageCount;
#endif
                    continue;
                }
            }
            else
            {
                // Zero them and release them in case we don't decommit them.
                memset(freePageEntry, 0, pageCount * AutoSystemInfo::PageSize);
                segment->ReleasePages(freePageEntry, pageCount);
                LogFreePages(pageCount);
            }

            TransferSegment(segment, fromSegmentList);
        }

        // Take the lock to make sure the recycler thread has finished zeroing out the pages after
        // we drained the queue
        backgroundPageQueue->backgroundPageQueueCriticalSection.Enter();
        this->hasZeroQueuedPages = false;
        Assert(!this->HasZeroQueuedPages());
        backgroundPageQueue->backgroundPageQueueCriticalSection.Leave();

        FlushBackgroundPages();
    }

    if (this->freePageCount == 0)
    {
        Assert(debugMinFreePageCount == 0);
        return;
    }

    PAGE_ALLOC_TRACE_AND_STATS(L"Decommit now");

    // minFreePageCount is not updated on every page allocate,
    // so we have to do a final update here.
    UpdateMinFreePageCount();

    size_t newFreePageCount;

    if (all)
    {
        newFreePageCount = this->GetFreePageLimit();

        PAGE_ALLOC_TRACE_AND_STATS(L"Full decommit");
    }
    else
    {
        // Decommit half the min free page count since last partial decommit
        Assert(this->minFreePageCount <= this->freePageCount);
        newFreePageCount = this->freePageCount - (this->minFreePageCount / 2);

        // Ensure we don't decommit down to fewer than our partial decommit minimum
        newFreePageCount = max(newFreePageCount, static_cast<size_t>(MinPartialDecommitFreePageCount));

        PAGE_ALLOC_TRACE_AND_STATS(L"Partial decommit");
    }

    if (newFreePageCount >= this->freePageCount)
    {
        PAGE_ALLOC_TRACE_AND_STATS(L"No pages to decommit");
        return;
    }

    size_t pageToDecommit = this->freePageCount - newFreePageCount;

    PAGE_ALLOC_TRACE_AND_STATS(L"Decommit page count = %d", pageToDecommit);
    PAGE_ALLOC_TRACE_AND_STATS(L"Free page count = %d", this->freePageCount);
    PAGE_ALLOC_TRACE_AND_STATS(L"New free page count = %d", newFreePageCount);

#if DBG_DUMP
    size_t decommitCount = 0;
#endif

    // decommit from page that already has other decommitted page already
    {
        DListBase<PageSegmentBase<T>>::EditingIterator i(&decommitSegments);

        while (pageToDecommit > 0  && i.Next())
        {
            size_t pageDecommited = i.Data().DecommitFreePages(pageToDecommit);
            LogDecommitPages(pageDecommited);
#if DBG_DUMP
            decommitCount += pageDecommited;
#endif
            if (i.Data().GetDecommitPageCount() == maxAllocPageCount)
            {
                LogFreePartiallyDecommitedPageSegment(&i.Data());
                i.RemoveCurrent(&NoThrowNoMemProtectHeapAllocator::Instance);
#if DBG_DUMP
                deleteCount += maxAllocPageCount;
#endif
            }
            pageToDecommit -= pageDecommited;
        }
    }

    // decommit pages that are empty

    while (pageToDecommit > 0 && !emptySegments.Empty())
    {
        if (pageToDecommit >= maxAllocPageCount)
        {
            Assert(emptySegments.Head().GetDecommitPageCount() == 0);
            LogFreeSegment(&emptySegments.Head());
            emptySegments.RemoveHead(&NoThrowNoMemProtectHeapAllocator::Instance);

            pageToDecommit -= maxAllocPageCount;
#if DBG_DUMP
            decommitCount += maxAllocPageCount;
            deleteCount += maxAllocPageCount;
#endif
        }
        else
        {
            size_t pageDecommited = emptySegments.Head().DecommitFreePages(pageToDecommit);
            LogDecommitPages(pageDecommited);
#if DBG_DUMP
            decommitCount += pageDecommited;
#endif
            Assert(pageDecommited == pageToDecommit);
            emptySegments.MoveHeadTo(&decommitSegments);
            pageToDecommit = 0;
        }
    }

    {
        DListBase<PageSegmentBase<T>>::EditingIterator i(&segments);

        while (pageToDecommit > 0  && i.Next())
        {
            size_t pageDecommited = i.Data().DecommitFreePages(pageToDecommit);
            LogDecommitPages(pageDecommited);
#if DBG_DUMP
            decommitCount += pageDecommited;
#endif
            Assert(i.Data().GetDecommitPageCount() != 0);
            Assert(i.Data().GetDecommitPageCount() <= maxAllocPageCount);
            i.MoveCurrentTo(&decommitSegments);
            pageToDecommit -= pageDecommited;

        }
    }


    Assert(pageToDecommit == 0);

#if DBG_DUMP
    Assert(this->freePageCount == newFreePageCount + decommitCount);
#endif

    this->freePageCount = newFreePageCount;

#if DBG
    UpdateMinimum(this->debugMinFreePageCount, this->freePageCount);
    Check();
#endif
#if DBG_DUMP
    this->decommitPageCount += (decommitCount - deleteCount);
    if (CUSTOM_PHASE_TRACE1(this->pageAllocatorFlagTable, Js::PageAllocatorPhase))
    {
        if (CUSTOM_PHASE_STATS1(this->pageAllocatorFlagTable, Js::PageAllocatorPhase))
        {
            Output::Print(L" After decommit now:\n");
            this->DumpStats();
        }
        Output::Flush();
    }
#endif
}

template<typename T>
void
PageAllocatorBase<T>::AddReservedBytes(size_t bytes)
{
    reservedBytes += bytes;
#ifdef PERF_COUNTERS
    GetReservedSizeCounter() += bytes;
    GetTotalReservedSizeCounter() += bytes;
#endif
}

template<typename T>
void
PageAllocatorBase<T>::SubReservedBytes(size_t bytes)
{
    reservedBytes -= bytes;
#ifdef PERF_COUNTERS
    GetReservedSizeCounter() -= bytes;
    GetTotalReservedSizeCounter() -= bytes;
#endif
}

template<typename T>
void
PageAllocatorBase<T>::AddCommittedBytes(size_t bytes)
{
    committedBytes += bytes;
#ifdef PERF_COUNTERS
    GetCommittedSizeCounter() += bytes;
    GetTotalCommittedSizeCounter() += bytes;
#endif
}

template<typename T>
void
PageAllocatorBase<T>::SubCommittedBytes(size_t bytes)
{
    committedBytes -= bytes;
#ifdef PERF_COUNTERS
    GetCommittedSizeCounter() -= bytes;
    GetTotalCommittedSizeCounter() -= bytes;
#endif
}

template<typename T>
void
PageAllocatorBase<T>::AddUsedBytes(size_t bytes)
{
    usedBytes += bytes;
#if defined(_M_X64_OR_ARM64)
    size_t lastTotalUsedBytes = ::InterlockedExchangeAdd64((volatile LONG64 *)&totalUsedBytes, bytes);
#else
    DWORD lastTotalUsedBytes = ::InterlockedExchangeAdd(&totalUsedBytes, bytes);
#endif

    if (totalUsedBytes > maxUsedBytes)
    {
        maxUsedBytes = totalUsedBytes;
    }

    // ETW events from different threads may be reported out of order, producing an
    // incorrect representation of current used bytes in the process. We've determined that this is an
    // acceptable issue, which will be mitigated at the level of the application consuming the event.
    JS_ETW(EventWriteJSCRIPT_PAGE_ALLOCATOR_USED_SIZE(lastTotalUsedBytes + bytes));
#ifndef ENABLE_JS_ETW
    Unused(lastTotalUsedBytes);
#endif

#ifdef PERF_COUNTERS
    GetUsedSizeCounter() += bytes;
    GetTotalUsedSizeCounter() += bytes;
#endif
}

template<typename T>
void
PageAllocatorBase<T>::SubUsedBytes(size_t bytes)
{
    Assert(bytes <= usedBytes);
    Assert(bytes <= totalUsedBytes);

    usedBytes -= bytes;

#if defined(_M_X64_OR_ARM64)
    size_t lastTotalUsedBytes = ::InterlockedExchangeAdd64((volatile LONG64 *)&totalUsedBytes, -(LONG64)bytes);
#else
    DWORD lastTotalUsedBytes = ::InterlockedExchangeSubtract(&totalUsedBytes, bytes);
#endif

    // ETW events from different threads may be reported out of order, producing an
    // incorrect representation of current used bytes in the process. We've determined that this is an
    // acceptable issue, which will be mitigated at the level of the application consuming the event.
    JS_ETW(EventWriteJSCRIPT_PAGE_ALLOCATOR_USED_SIZE(lastTotalUsedBytes - bytes));
#ifndef ENABLE_JS_ETW
    Unused(lastTotalUsedBytes);
#endif

#ifdef PERF_COUNTERS
    GetUsedSizeCounter() -= bytes;
    GetTotalUsedSizeCounter() -= bytes;
#endif
}

template<typename T>
void
PageAllocatorBase<T>::AddNumberOfSegments(size_t segmentCount)
{
    numberOfSegments += segmentCount;
}

template<typename T>
void
PageAllocatorBase<T>::SubNumberOfSegments(size_t segmentCount)
{
    numberOfSegments -= segmentCount;
}

template<typename T>
void
PageAllocatorBase<T>::IntegrateSegments(DListBase<PageSegmentBase<T>>& segmentList, uint segmentCount, size_t pageCount)
{
#if DBG
    size_t debugPageCount = 0;
    uint debugSegmentCount = 0;
    DListBase<PageSegmentBase<T>>::Iterator i(&segmentList);
    while (i.Next())
    {
        Assert(i.Data().GetAllocator() == this);
        debugSegmentCount++;
        debugPageCount += i.Data().GetPageCount();
    }
    Assert(debugSegmentCount == segmentCount);
    Assert(debugPageCount == pageCount);
#endif
    LogAllocSegment(segmentCount, pageCount);
    LogAllocPages(pageCount);

    this->SuspendIdleDecommit();
    segmentList.MoveTo(&this->fullSegments);
    this->ResumeIdleDecommit();
}

template<typename T>
void
PageAllocatorBase<T>::LogAllocSegment(SegmentBase<T> * segment)
{
    LogAllocSegment(1, segment->GetPageCount());
}

template<typename T>
void
PageAllocatorBase<T>::LogAllocSegment(uint segmentCount, size_t pageCount)
{
    size_t bytes = pageCount * AutoSystemInfo::PageSize;
    AddReservedBytes(bytes);
    AddCommittedBytes(bytes);
    AddNumberOfSegments(segmentCount);
#ifdef PROFILE_MEM
    if (this->memoryData)
    {
        this->memoryData->allocSegmentCount += segmentCount;
        this->memoryData->allocSegmentBytes += pageCount * AutoSystemInfo::PageSize;

        this->memoryData->currentCommittedPageCount += pageCount;
        this->memoryData->peakCommittedPageCount = max(this->memoryData->peakCommittedPageCount, this->memoryData->currentCommittedPageCount);
    }
#endif
}

template<typename T>
void
PageAllocatorBase<T>::LogFreeSegment(SegmentBase<T> * segment)
{
    size_t bytes = segment->GetPageCount() * AutoSystemInfo::PageSize;
    SubCommittedBytes(bytes);
    SubReservedBytes(bytes);
    SubNumberOfSegments(1);
#ifdef PROFILE_MEM
    if (this->memoryData)
    {
        this->memoryData->releaseSegmentCount++;
        this->memoryData->releaseSegmentBytes += segment->GetPageCount() * AutoSystemInfo::PageSize;
        this->memoryData->currentCommittedPageCount -= segment->GetPageCount();
    }
#endif
}

template<typename T>
void
PageAllocatorBase<T>::LogFreeDecommittedSegment(SegmentBase<T> * segment)
{
    SubReservedBytes(segment->GetPageCount() * AutoSystemInfo::PageSize);
    SubNumberOfSegments(1);
#ifdef PROFILE_MEM
    if (this->memoryData)
    {
        this->memoryData->releaseSegmentCount++;
        this->memoryData->releaseSegmentBytes += segment->GetPageCount() * AutoSystemInfo::PageSize;
    }
#endif
}

template<typename T>
void
PageAllocatorBase<T>::LogFreePages(size_t pageCount)
{
    SubUsedBytes(pageCount * AutoSystemInfo::PageSize);
#ifdef PROFILE_MEM
    if (this->memoryData)
    {
        this->memoryData->releasePageCount += pageCount;
    }
#endif
}

template<typename T>
void
PageAllocatorBase<T>::LogFreePartiallyDecommitedPageSegment(PageSegmentBase<T> * pageSegment)
{
    AddCommittedBytes(pageSegment->GetDecommitPageCount() * AutoSystemInfo::PageSize);
#ifdef PROFILE_MEM
    if (this->memoryData)
    {
        this->memoryData->currentCommittedPageCount += pageSegment->GetDecommitPageCount();
    }
#endif
    LogFreeSegment(pageSegment);
}

template<typename T>
void
PageAllocatorBase<T>::LogAllocPages(size_t pageCount)
{
    AddUsedBytes(pageCount * AutoSystemInfo::PageSize);
#ifdef PROFILE_MEM
    if (this->memoryData)
    {
        this->memoryData->allocPageCount += pageCount;
    }
#endif
}

template<typename T>
void
PageAllocatorBase<T>::LogRecommitPages(size_t pageCount)
{
#ifdef PROFILE_MEM
    if (this->memoryData)
    {
        this->memoryData->recommitPageCount += pageCount;
    }
#endif
    LogCommitPages(pageCount);
}

template<typename T>
void
PageAllocatorBase<T>::LogCommitPages(size_t pageCount)
{
    AddCommittedBytes(pageCount * AutoSystemInfo::PageSize);
#ifdef PROFILE_MEM
    if (this->memoryData)
    {
        this->memoryData->currentCommittedPageCount += pageCount;
        this->memoryData->peakCommittedPageCount = max(this->memoryData->peakCommittedPageCount, this->memoryData->currentCommittedPageCount);
    }
#endif
}

template<typename T>
void
PageAllocatorBase<T>::LogDecommitPages(size_t pageCount)
{
    SubCommittedBytes(pageCount * AutoSystemInfo::PageSize);
#ifdef PROFILE_MEM
    if (this->memoryData)
    {
        this->memoryData->decommitPageCount += pageCount;
        this->memoryData->currentCommittedPageCount -= pageCount;
    }
#endif
}

#if DBG_DUMP
template<typename T>
void
PageAllocatorBase<T>::DumpStats() const
{
    Output::Print(L"  Full/Partial/Empty/Decommit/Large Segments: %4d %4d %4d %4d %4d\n",
        fullSegments.Count(), segments.Count(), emptySegments.Count(), decommitSegments.Count(), largeSegments.Count());

    Output::Print(L"  Free/Decommit/Min Free Pages              : %4d %4d %4d\n",
        this->freePageCount, this->decommitPageCount, this->minFreePageCount);
}
#endif

#if DBG
template<typename T>
void
PageAllocatorBase<T>::Check()
{
    Assert(!this->HasZeroQueuedPages());
    size_t currentFreePageCount = 0;

    DListBase<PageSegmentBase<T>>::Iterator segmentsIterator(&segments);
    while (segmentsIterator.Next())
    {
        currentFreePageCount += segmentsIterator.Data().GetFreePageCount();
    }

    DListBase<PageSegmentBase<T>>::Iterator fullSegmentsIterator(&fullSegments);
    while (fullSegmentsIterator.Next())
    {
        currentFreePageCount += fullSegmentsIterator.Data().GetFreePageCount();
    }

    DListBase<PageSegmentBase<T>>::Iterator emptySegmentsIterator(&emptySegments);
    while (emptySegmentsIterator.Next())
    {
        currentFreePageCount += emptySegmentsIterator.Data().GetFreePageCount();
    }

    DListBase<PageSegmentBase<T>>::Iterator decommitSegmentsIterator(&decommitSegments);
    while (decommitSegmentsIterator.Next())
    {
        currentFreePageCount += decommitSegmentsIterator.Data().GetFreePageCount();
    }

    Assert(freePageCount == currentFreePageCount);
}
#endif

template<typename T>
HeapPageAllocator<T>::HeapPageAllocator(AllocationPolicyManager * policyManager, bool allocXdata, bool excludeGuardPages) :
    PageAllocatorBase(policyManager,
        Js::Configuration::Global.flags,
        PageAllocatorType_CustomHeap,
        /*maxFreePageCount*/ 0,
        /*zeroPages*/ false,
        /*zeroPageQueue*/ nullptr,
        /*maxAllocPageCount*/ allocXdata ? (DefaultMaxAllocPageCount - XDATA_RESERVE_PAGE_COUNT) : DefaultMaxAllocPageCount,
        /*secondaryAllocPageCount=*/ allocXdata ? XDATA_RESERVE_PAGE_COUNT : 0,
        /*stopAllocationOnOutOfMemory*/ false,
        excludeGuardPages),
    allocXdata(allocXdata)
{
}

template<typename T>
void
HeapPageAllocator<T>::ReleaseDecommited(void * address, size_t pageCount, __in void *  segmentParam)
{
    SegmentBase<T> * segment = (SegmentBase<T>*) segmentParam;
    if (pageCount > this->maxAllocPageCount)
    {
        Assert(address == segment->GetAddress());
        Assert(pageCount == segment->GetAvailablePageCount());
        this->ReleaseDecommitedSegment(segment);
    }
    else
    {
        Assert(pageCount <= UINT_MAX);
        this->TrackDecommitedPages(address, (uint)pageCount, (PageSegment *)segment);
    }
}

template<typename T>
void
HeapPageAllocator<T>::ReleaseDecommitedSegment(__in SegmentBase<T>* segment)
{
    ASSERT_THREAD();

    LogFreeDecommittedSegment(segment);
    largeSegments.RemoveElement(&NoThrowNoMemProtectHeapAllocator::Instance, segment);
}

// decommit the page but don't release it
template<typename T>
void
HeapPageAllocator<T>::DecommitPages(__in char* address, size_t pageCount = 1)
{
    Assert(pageCount <= MAXUINT32);
#pragma prefast(suppress:__WARNING_WIN32UNRELEASEDVADS, "The remainder of the clean-up is done later.");
    virtualAllocator->Free(address, pageCount * AutoSystemInfo::PageSize, MEM_DECOMMIT);
    LogFreePages(pageCount);
    LogDecommitPages(pageCount);
}

template<typename TVirtualAlloc>
template <typename T>
void PageAllocatorBase<TVirtualAlloc>::ReleaseSegmentList(DListBase<T> * segmentList)
{
    segmentList->Clear(&NoThrowNoMemProtectHeapAllocator::Instance);
}

template<typename T>
BOOL
HeapPageAllocator<T>::ProtectPages(__in char* address, size_t pageCount, __in void* segmentParam, DWORD dwVirtualProtectFlags, DWORD* dwOldVirtualProtectFlags, DWORD desiredOldProtectFlag)
{
    SegmentBase<T> * segment = (SegmentBase<T>*)segmentParam;
#if DBG
    Assert(address >= segment->GetAddress());
    Assert(((uint)(((char *)address) - segment->GetAddress()) <= (segment->GetPageCount() - pageCount) * AutoSystemInfo::PageSize));
    Assert(dwOldVirtualProtectFlags != NULL);

    if (IsPageSegment(segment))
    {
        PageSegmentBase<T> * pageSegment = static_cast<PageSegmentBase<T>*>(segment);
        AssertMsg(pageCount <= MAXUINT32, "PageSegment should always be smaller than 4G pages");
        Assert(!pageSegment->IsFreeOrDecommitted(address, static_cast<uint>(pageCount)));
    }
#endif

#if DBG_DUMP || defined(RECYCLER_TRACE)
    if (this->pageAllocatorFlagTable.IsEnabled(Js::TraceProtectPagesFlag))
    {
        Output::Print(L"VirtualProtect(0x%p, %d, %d, %d)\n", address, pageCount, pageCount * AutoSystemInfo::PageSize, dwVirtualProtectFlags);
    }
#endif

    // check address alignment, and that the address is in correct range
    if (((uintptr_t)address & (AutoSystemInfo::PageSize - 1)) != 0
        || address < segment->GetAddress()
        || ((uint)(((char *)address) - segment->GetAddress()) > (segment->GetPageCount() - pageCount) * AutoSystemInfo::PageSize))
    {
        CustomHeap_BadPageState_fatal_error((ULONG_PTR)this);
        return FALSE;
    }

    MEMORY_BASIC_INFORMATION memBasicInfo;

    // check old protection on all pages about to change, ensure the fidelity
    size_t bytes = VirtualQuery(address, &memBasicInfo, sizeof(memBasicInfo));
    if (bytes == 0
        || memBasicInfo.RegionSize < pageCount * AutoSystemInfo::PageSize
        || desiredOldProtectFlag != memBasicInfo.Protect
        )
    {
        CustomHeap_BadPageState_fatal_error((ULONG_PTR)this);
        return FALSE;
    }
    *dwOldVirtualProtectFlags = memBasicInfo.Protect;

    DWORD oldProtect; // this is only for first page
    BOOL retVal = ::VirtualProtect(address, pageCount * AutoSystemInfo::PageSize, dwVirtualProtectFlags, &oldProtect);
    Assert(oldProtect == *dwOldVirtualProtectFlags);

    return retVal;
}

template<typename T>
void
HeapPageAllocator<T>::TrackDecommitedPages(void * address, uint pageCount, __in void* segmentParam)
{
    PageSegmentBase<T> * segment = (PageSegmentBase<T>*)segmentParam;
    ASSERT_THREAD();
    Assert(!HasMultiThreadAccess());
    Assert(pageCount <= this->maxAllocPageCount);

    DListBase<PageSegmentBase<T>> * fromSegmentList = GetSegmentList(segment);

    // Update the state of the segment with the decommitted pages
    segment->DecommitPages<true>(address, pageCount);

    // Move the segment to its appropriate list
    TransferSegment(segment, fromSegmentList);
}

template<typename T>
bool HeapPageAllocator<T>::AllocSecondary(void* segmentParam, ULONG_PTR functionStart, DWORD functionSize, ushort pdataCount, ushort xdataSize, SecondaryAllocation* allocation)
{
    SegmentBase<T> * segment = (SegmentBase<T> *)segmentParam;
    Assert(segment->GetSecondaryAllocator());

    bool success;
    if (IsPageSegment(segment))
    {
        PageSegmentBase<T>* pageSegment = static_cast<PageSegmentBase<T>*>(segment);

        // We should get the segment list BEFORE xdata allocation happens.
        DListBase<PageSegmentBase<T>> * fromSegmentList = GetSegmentList(pageSegment);

        success = segment->GetSecondaryAllocator()->Alloc(functionStart, functionSize, pdataCount, xdataSize, allocation);

        // If no more XDATA allocations can take place.
        if (success && !pageSegment->CanAllocSecondary() && fromSegmentList != &fullSegments)
        {
            AssertMsg(GetSegmentList(pageSegment) == &fullSegments, "This segment should now be in the full list if it can't allocate secondary");

            OUTPUT_TRACE(Js::EmitterPhase, L"XDATA Wasted pages:%u\n", pageSegment->GetFreePageCount());
            this->freePageCount -= pageSegment->GetFreePageCount();
            fromSegmentList->MoveElementTo(pageSegment, &fullSegments);
#if DBG
            UpdateMinimum(this->debugMinFreePageCount, this->freePageCount);
#endif
        }
    }
    else
    {
        // A large segment should always be able to do secondary allocations
        Assert(segment->CanAllocSecondary());
        success = segment->GetSecondaryAllocator()->Alloc(functionStart, functionSize, pdataCount, xdataSize, allocation);
    }

#ifdef _M_X64
    // In ARM it's OK to have xdata size be 0
    AssertMsg(allocation->address != nullptr, "All segments that cannot allocate xdata should have been already moved to full segments list");
#endif
    return success;
}

template<typename T>
void HeapPageAllocator<T>::ReleaseSecondary(const SecondaryAllocation& allocation, void* segmentParam)
{
    SegmentBase<T> * segment = (SegmentBase<T>*)segmentParam;
    Assert(allocation.address != nullptr);
    Assert(segment->GetSecondaryAllocator());

    if (IsPageSegment(segment))
    {
        PageSegmentBase<T>* pageSegment = static_cast<PageSegmentBase<T>*>(segment);
        auto fromList = GetSegmentList(pageSegment);

        pageSegment->GetSecondaryAllocator()->Release(allocation);

        auto toList = GetSegmentList(pageSegment);

        if (fromList != toList)
        {
            OUTPUT_TRACE(Js::EmitterPhase, L"XDATA reclaimed pages:%u\n", pageSegment->GetFreePageCount());
            fromList->MoveElementTo(pageSegment, toList);

            AssertMsg(fromList == &fullSegments, "Releasing a secondary allocator should make a state change only if the segment was originally in the full list");
            AssertMsg(pageSegment->CanAllocSecondary(), "It should be allocate secondary now");
            this->AddFreePageCount(pageSegment->GetFreePageCount());
        }
    }
    else
    {
        Assert(segment->CanAllocSecondary());
        segment->GetSecondaryAllocator()->Release(allocation);
    }
}

template<typename T>
bool
HeapPageAllocator<T>::IsAddressFromAllocator(__in void* address)
{
    DListBase<PageSegmentBase<T>>::Iterator segmentsIterator(&segments);
    while (segmentsIterator.Next())
    {
        if (IsAddressInSegment(address, segmentsIterator.Data()))
        {
            return true;
        }
    }

    DListBase<PageSegmentBase<T>>::Iterator fullSegmentsIterator(&fullSegments);
    while (fullSegmentsIterator.Next())
    {
        if (IsAddressInSegment(address, fullSegmentsIterator.Data()))
        {
            return true;
        }
    }

    DListBase<SegmentBase<T>>::Iterator largeSegmentsIterator(&largeSegments);
    while (largeSegmentsIterator.Next())
    {
        if (IsAddressInSegment(address, largeSegmentsIterator.Data()))
        {
            return true;
        }
    }

    DListBase<PageSegmentBase<T>>::Iterator decommitSegmentsIterator(&decommitSegments);
    while (decommitSegmentsIterator.Next())
    {
        if (IsAddressInSegment(address, decommitSegmentsIterator.Data()))
        {
            return true;
        }
    }

    return false;
}

template<typename T>
bool
PageAllocatorBase<T>::IsAddressInSegment(__in void* address, const PageSegmentBase<T>& segment)
{
    bool inSegment = this->IsAddressInSegment(address, static_cast<const SegmentBase<T>&>(segment));

    if (inSegment)
    {
        return !segment.IsFreeOrDecommitted(address);
    }

    return inSegment;
}

template<typename T>
bool
PageAllocatorBase<T>::IsAddressInSegment(__in void* address, const SegmentBase<T>& segment)
{
    return segment.IsInSegment(address);
}

#if PDATA_ENABLED
#include "Memory\XDataAllocator.h"
template<typename T>
bool HeapPageAllocator<T>::CreateSecondaryAllocator(SegmentBase<T>* segment, SecondaryAllocator** allocator)
{
    Assert(segment->GetAllocator() == this);

    // If we are not allocating xdata there is nothing to do

    // ARM might allocate XDATA but not have a reserved region for it (no secondary alloc reserved space)
    if(!allocXdata)
    {
        Assert(segment->GetSecondaryAllocSize() == 0);
        *allocator = nullptr;
        return true;
    }

    XDataAllocator* secondaryAllocator = HeapNewNoThrow(XDataAllocator, (BYTE*)segment->GetSecondaryAllocStartAddress(), segment->GetSecondaryAllocSize());
    bool success = false;
    if(secondaryAllocator)
    {
        if(secondaryAllocator->Initialize((BYTE*)segment->GetAddress(), (BYTE*)segment->GetEndAddress()))
        {
            success = true;
        }
        else
        {
            HeapDelete(secondaryAllocator);
            secondaryAllocator = nullptr;
        }
    }
    *allocator = secondaryAllocator;
    return success;
}
#endif

template<typename T>
uint PageSegmentBase<T>::GetCountOfFreePages() const
{
    return this->freePages.Count();
}

template<typename T>
uint PageSegmentBase<T>::GetNextBitInFreePagesBitVector(uint index) const
{
    return this->freePages.GetNextBit(index);
}

template<typename T>
BOOLEAN PageSegmentBase<T>::TestRangeInFreePagesBitVector(uint index, uint pageCount) const
{
    return this->freePages.TestRange(index, pageCount);
}

template<typename T>
BOOLEAN PageSegmentBase<T>::TestInFreePagesBitVector(uint index) const
{
    return this->freePages.Test(index);
}

template<typename T>
void PageSegmentBase<T>::ClearAllInFreePagesBitVector()
{
    return this->freePages.ClearAll();
}

template<typename T>
void PageSegmentBase<T>::ClearRangeInFreePagesBitVector(uint index, uint pageCount)
{
    return this->freePages.ClearRange(index, pageCount);
}

template<typename T>
void PageSegmentBase<T>::SetRangeInFreePagesBitVector(uint index, uint pageCount)
{
    return this->freePages.SetRange(index, pageCount);
}

template<typename T>
void PageSegmentBase<T>::ClearBitInFreePagesBitVector(uint index)
{
    return this->freePages.Clear(index);
}

template<typename T>
BOOLEAN PageSegmentBase<T>::TestInDecommitPagesBitVector(uint index) const
{
    return this->decommitPages.Test(index);
}

template<typename T>
BOOLEAN PageSegmentBase<T>::TestRangeInDecommitPagesBitVector(uint index, uint pageCount) const
{
    return this->decommitPages.TestRange(index, pageCount);
}

template<typename T>
void PageSegmentBase<T>::SetRangeInDecommitPagesBitVector(uint index, uint pageCount)
{
    return this->decommitPages.SetRange(index, pageCount);
}

template<typename T>
void PageSegmentBase<T>::ClearRangeInDecommitPagesBitVector(uint index, uint pageCount)
{
    return this->decommitPages.ClearRange(index, pageCount);
}

template<typename T>
uint PageSegmentBase<T>::GetCountOfDecommitPages() const
{
    return this->decommitPages.Count();
}

template<typename T>
void PageSegmentBase<T>::SetBitInDecommitPagesBitVector(uint index)
{
    this->decommitPages.Set(index);
}

template<typename T>
template <bool noPageAligned>
char * PageSegmentBase<T>::DoAllocDecommitPages(uint pageCount, PageHeapMode pageHeapFlags)
{
    return this->AllocDecommitPages<PageSegmentBase<T>::PageBitVector, noPageAligned>(pageCount, this->freePages, this->decommitPages, pageHeapFlags);
}

template<typename T>
uint PageSegmentBase<T>::GetMaxPageCount()
{
    return MaxPageCount;
}

//Instantiate all the Templates in this class below.
template class PageAllocatorBase < PreReservedVirtualAllocWrapper >;
template class PageAllocatorBase < VirtualAllocWrapper >;
template class HeapPageAllocator < PreReservedVirtualAllocWrapper >;
template class HeapPageAllocator < VirtualAllocWrapper >;

template class SegmentBase       < VirtualAllocWrapper > ;
template class SegmentBase       < PreReservedVirtualAllocWrapper >;
template class PageSegmentBase   < VirtualAllocWrapper >;
template class PageSegmentBase   < PreReservedVirtualAllocWrapper >;

