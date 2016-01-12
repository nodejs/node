//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "PageAllocatorDefines.h"

#ifdef PROFILE_MEM
struct PageMemoryData;
#endif

class CodeGenNumberThreadAllocator;

namespace Memory
{
typedef void* FunctionTableHandle;

#if DBG_DUMP && !defined(JD_PRIVATE)

#define GUARD_PAGE_TRACE(...) \
    if (Js::Configuration::Global.flags.PrintGuardPageBounds) \
{ \
    Output::Print(__VA_ARGS__); \
}

#define PAGE_ALLOC_TRACE(format, ...) PAGE_ALLOC_TRACE_EX(false, false, format, __VA_ARGS__)
#define PAGE_ALLOC_VERBOSE_TRACE(format, ...) PAGE_ALLOC_TRACE_EX(true, false, format, __VA_ARGS__)

#define PAGE_ALLOC_TRACE_AND_STATS(format, ...) PAGE_ALLOC_TRACE_EX(false, true, format, __VA_ARGS__)
#define PAGE_ALLOC_VERBOSE_TRACE_AND_STATS(format, ...) PAGE_ALLOC_TRACE_EX(true, true, format, __VA_ARGS__)

#define PAGE_ALLOC_TRACE_EX(verbose, stats, format, ...) \
    if (this->pageAllocatorFlagTable.Trace.IsEnabled(Js::PageAllocatorPhase)) \
    { \
        if (!verbose || this->pageAllocatorFlagTable.Verbose) \
        {   \
            Output::Print(L"%p : %p> PageAllocator(%p): ", GetCurrentThreadContextId(), ::GetCurrentThreadId(), this); \
            if (debugName != nullptr) \
            { \
                Output::Print(L"[%s] ", this->debugName); \
            } \
            Output::Print(format, __VA_ARGS__);         \
            Output::Print(L"\n"); \
            if (stats && this->pageAllocatorFlagTable.Stats.IsEnabled(Js::PageAllocatorPhase)) \
            { \
                this->DumpStats();         \
            }  \
            Output::Flush(); \
        } \
    }
#else
#define PAGE_ALLOC_TRACE(format, ...)
#define PAGE_ALLOC_VERBOSE_TRACE(format, ...)

#define PAGE_ALLOC_TRACE_AND_STATS(format, ...)
#define PAGE_ALLOC_VERBOSE_TRACE_AND_STATS(format, ...)
#endif

#ifdef _M_X64
#define XDATA_RESERVE_PAGE_COUNT   (2)       // Number of pages per page segment (32 pages) reserved for xdata.
#else
#define XDATA_RESERVE_PAGE_COUNT   (0)       // ARM uses the heap, so it's not required.
#endif

//
// Allocation done by the secondary allocator
//
struct SecondaryAllocation
{
    BYTE* address;   // address of the allocation by the secondary allocator

    SecondaryAllocation() : address(nullptr)
    {
    }
};


//
// For every page segment a page allocator can create a secondary allocator which can have a specified
// number of pages reserved for secondary allocations. These pages are always reserved at the end of the
// segment. The PageAllocator itself cannot allocate from the region demarcated for the secondary allocator.
// Currently this is used for xdata allocations.
//
class SecondaryAllocator
{
public:
    virtual bool Alloc(ULONG_PTR functionStart, DWORD functionSize, ushort pdataCount, ushort xdataSize, SecondaryAllocation* xdata) = 0;
    virtual void Release(const SecondaryAllocation& allocation) = 0;
    virtual void Delete() = 0;
    virtual bool CanAllocate() = 0;
    virtual ~SecondaryAllocator() {};
};

/*
 * A segment is a collection of pages. A page corresponds to the concept of an
 * OS memory page. Segments allocate memory using the OS VirtualAlloc call.
 * It'll allocate the pageCount * page size number of bytes, the latter being
 * a system-wide constant.
 */
template<typename TVirtualAlloc>
class SegmentBase
{
public:
    SegmentBase(PageAllocatorBase<TVirtualAlloc> * allocator, size_t pageCount);
    virtual ~SegmentBase();

    size_t GetPageCount() const { return segmentPageCount; }

    // Some pages are reserved upfront for secondary allocations
    // which are done by a secondary allocator as opposed to the PageAllocator
    size_t GetAvailablePageCount() const { return segmentPageCount - secondaryAllocPageCount; }

    char* GetSecondaryAllocStartAddress() const { return (this->address + GetAvailablePageCount() * AutoSystemInfo::PageSize); }
    uint GetSecondaryAllocSize() const { return this->secondaryAllocPageCount * AutoSystemInfo::PageSize; }

    char* GetAddress() const { return address; }
    char* GetEndAddress() const { return GetSecondaryAllocStartAddress(); }

    bool CanAllocSecondary() { Assert(secondaryAllocator); return secondaryAllocator->CanAllocate(); }

    PageAllocatorBase<TVirtualAlloc>* GetAllocator() const { return allocator; }
    bool IsInPreReservedHeapPageAllocator() const;

    bool Initialize(DWORD allocFlags, bool excludeGuardPages);

#if DBG
    virtual bool IsPageSegment() const
    {
        return false;
    }
#endif

    bool IsInSegment(void* address) const
    {
        void* start = static_cast<void*>(GetAddress());
        void* end = static_cast<void*>(GetEndAddress());
        return (address >= start && address < end);
    }

    bool IsInCustomHeapAllocator() const
    {
        return this->allocator->type == PageAllocatorType::PageAllocatorType_CustomHeap;
    }

    SecondaryAllocator* GetSecondaryAllocator() { return secondaryAllocator; }

#if defined(_M_X64_OR_ARM64) && defined(RECYCLER_WRITE_BARRIER)
    bool IsWriteBarrierAllowed()
    {
        return isWriteBarrierAllowed;
    }

#endif

protected:
#if _M_IX86_OR_ARM32
    static const uint VirtualAllocThreshold =  524288; // 512kb As per spec
#else // _M_X64_OR_ARM64
    static const uint VirtualAllocThreshold = 1048576; // 1MB As per spec : when we cross this threshold of bytes, we should add guard pages
#endif
    static const uint maxGuardPages = 15;
    static const uint minGuardPages =  1;

    SecondaryAllocator* secondaryAllocator;
    char * address;
    PageAllocatorBase<TVirtualAlloc> * allocator;
    size_t segmentPageCount;
    uint trailingGuardPageCount;
    uint leadingGuardPageCount;
    uint   secondaryAllocPageCount;
#if defined(_M_X64_OR_ARM64) && defined(RECYCLER_WRITE_BARRIER)
    bool   isWriteBarrierAllowed;
#endif
};

/*
 * Page Segments allows a client to deal with virtual memory on a page level
 * unlike Segment, which gives you access on a segment basis. Pages managed
 * by the page segment are initially in a “free list”, and have the no access
 * bit set on them. When a client wants pages, we get them from the free list
 * and commit them into memory. When the client no longer needs those pages,
 * we simply decommit them- this means that the pages are still reserved for
 * the process but are not a part of its working set and has no physical
 * storage associated with it.
 */

template<typename TVirtualAlloc>
class PageSegmentBase : public SegmentBase<TVirtualAlloc>
{
public:
    PageSegmentBase(PageAllocatorBase<TVirtualAlloc> * allocator, bool external);
    // Maximum possible size of a PageSegment; may be smaller.
    static const uint MaxDataPageCount = 256;     // 1 MB
    static const uint MaxGuardPageCount = 16;
    static const uint MaxPageCount = MaxDataPageCount + MaxGuardPageCount;  // 272 Pages

    typedef BVStatic<MaxPageCount> PageBitVector;

    uint GetAvailablePageCount() const
    {
        size_t availablePageCount = SegmentBase::GetAvailablePageCount();
        Assert(availablePageCount < MAXUINT32);
        return static_cast<uint>(availablePageCount);
    }

    void Prime();
#ifdef PAGEALLOCATOR_PROTECT_FREEPAGE
    bool Initialize(DWORD allocFlags, bool excludeGuardPages);
#endif
    uint GetFreePageCount() const { return freePageCount; }
    uint GetDecommitPageCount() const { return decommitPageCount; }

    static bool IsAllocationPageAligned(__in char* address, size_t pageCount, PageHeapMode pageHeapFlags);

    template <typename T, bool notPageAligned>
    char * AllocDecommitPages(uint pageCount, T freePages, T decommitPages, PageHeapMode pageHeapFlags);

    template <bool notPageAligned>
    char * AllocPages(uint pageCount, PageHeapMode pageHeapFlags);

    void ReleasePages(__in void * address, uint pageCount);
    template <bool onlyUpdateState>
    void DecommitPages(__in void * address, uint pageCount);

    uint GetCountOfFreePages() const;
    uint GetNextBitInFreePagesBitVector(uint index) const;
    BOOLEAN TestRangeInFreePagesBitVector(uint index, uint pageCount) const;
    BOOLEAN TestInFreePagesBitVector(uint index) const;
    void ClearAllInFreePagesBitVector();
    void ClearRangeInFreePagesBitVector(uint index, uint pageCount);
    void SetRangeInFreePagesBitVector(uint index, uint pageCount);
    void ClearBitInFreePagesBitVector(uint index);

    uint GetCountOfDecommitPages() const;
    BOOLEAN TestInDecommitPagesBitVector(uint index) const;
    BOOLEAN TestRangeInDecommitPagesBitVector(uint index, uint pageCount) const;
    void SetRangeInDecommitPagesBitVector(uint index, uint pageCount);
    void SetBitInDecommitPagesBitVector(uint index);
    void ClearRangeInDecommitPagesBitVector(uint index, uint pageCount);

    template <bool notPageAligned>
    char * DoAllocDecommitPages(uint pageCount, PageHeapMode pageHeapFlags);
    uint GetMaxPageCount();

    size_t DecommitFreePages(size_t pageToDecommit);

    bool IsEmpty() const
    {
        return this->freePageCount == this->GetAvailablePageCount();
    }

    //
    // If a segment has decommitted pages - then it's not considered full as allocations can take place from it
    // However, if secondary allocations cannot be made from it - it's considered full nonetheless
    //
    bool IsFull() const
    {
        return (this->freePageCount == 0 && !ShouldBeInDecommittedList()) ||
            (this->secondaryAllocator != nullptr && !this->secondaryAllocator->CanAllocate());
    }

    bool IsAllDecommitted() const
    {
        return this->GetAvailablePageCount() == this->decommitPageCount;
    }

    bool ShouldBeInDecommittedList() const
    {
        return this->decommitPageCount != 0;
    }

    bool IsFreeOrDecommitted(void* address, uint pageCount) const
    {
        Assert(IsInSegment(address));

        uint base = GetBitRangeBase(address);
        return this->TestRangeInDecommitPagesBitVector(base, pageCount) || this->TestRangeInFreePagesBitVector(base, pageCount);
    }

    bool IsFreeOrDecommitted(void* address) const
    {
        Assert(IsInSegment(address));

        uint base = GetBitRangeBase(address);
        return this->TestInDecommitPagesBitVector(base) || this->TestInFreePagesBitVector(base);
    }

    PageBitVector GetUnAllocatedPages() const
    {
        PageBitVector unallocPages = freePages;
        unallocPages.Or(&decommitPages);
        return unallocPages;
    }

    void ChangeSegmentProtection(DWORD protectFlags, DWORD expectedOldProtectFlags);

#if DBG
    bool IsPageSegment() const override
    {
        return true;
    }
#endif

//---------- Private members ---------------/
private:
    uint GetBitRangeBase(void* address) const
    {
        uint base = ((uint)(((char *)address) - this->address)) / AutoSystemInfo::PageSize;
        return base;
    }

    PageBitVector freePages;
    PageBitVector decommitPages;

    uint     freePageCount;
    uint     decommitPageCount;
};

template<typename TVirtualAlloc = VirtualAllocWrapper>
class HeapPageAllocator;

/*
 * A Page Allocation is an allocation made by a page allocator
 * This has a base address, and tracks the number of pages that
 * were allocated from the segment
 */
class PageAllocation
{
public:
    char * GetAddress() const { return ((char *)this) + sizeof(PageAllocation); }
    size_t GetSize() const { return pageCount * AutoSystemInfo::PageSize - sizeof(PageAllocation); }
    size_t GetPageCount() const { return pageCount; }
    void* GetSegment() const { return segment; }

private:
    size_t pageCount;
    void * segment;

    friend class PageAllocatorBase<VirtualAllocWrapper>;
    friend class PageAllocatorBase<PreReservedVirtualAllocWrapper>;
    friend class HeapPageAllocator<>;
};

/*
 * This allocator is responsible for allocating and freeing pages. It does
 * so by virtue of allocating segments for groups of pages, and then handing
 * out memory in these segments. It's also responsible for free segments.
 * This class also controls the idle decommit thread, which decommits pages
 * when they're no longer needed
 */

template<typename TVirtualAlloc>
class PageAllocatorBase
{
    friend class CodeGenNumberThreadAllocator;
    // Allowing recycler to report external memory allocation.
    friend class Recycler;
public:
    static uint const DefaultMaxFreePageCount = 0x400;       // 4 MB
    static uint const DefaultLowMaxFreePageCount = 0x100;    // 1 MB for low-memory process

    static uint const MinPartialDecommitFreePageCount = 0x1000;  // 16 MB

    static uint const DefaultMaxAllocPageCount = 32;        // 128K
    static uint const DefaultSecondaryAllocPageCount = 0;

    static size_t GetProcessUsedBytes();

    static size_t GetAndResetMaxUsedBytes();

    virtual BOOL ProtectPages(__in char* address, size_t pageCount, __in void* segment, DWORD dwVirtualProtectFlags, DWORD* dwOldVirtualProtectFlags, DWORD desiredOldProtectFlag)
    {
        Assert(false);
        return false;
    }

    virtual bool AllocSecondary(void* segment, ULONG_PTR functionStart, DWORD functionSize, ushort pdataCount, ushort xdataSize, SecondaryAllocation* allocation)
    {
        Assert(false);
        return false;
    }

    virtual void ReleaseSecondary(const SecondaryAllocation& allocation, void* segment)
    {
        Assert(false);
    }

    virtual void DecommitPages(__in char* address, int pageCount)
    {
        Assert(false);
    }

    virtual void TrackDecommitedPages(void * address, uint pageCount, __in void* segment)
    {
        Assert(false);
    }

    struct BackgroundPageQueue
    {
        BackgroundPageQueue();

        SLIST_HEADER freePageList;

        CriticalSection backgroundPageQueueCriticalSection;
#if DBG
        bool isZeroPageQueue;
#endif
    };
    struct ZeroPageQueue : BackgroundPageQueue
    {
        ZeroPageQueue();

        SLIST_HEADER pendingZeroPageList;
    };

    PageAllocatorBase(AllocationPolicyManager * policyManager,
#ifndef JD_PRIVATE
        Js::ConfigFlagsTable& flags = Js::Configuration::Global.flags,
#endif
        PageAllocatorType type = PageAllocatorType_Max,
        uint maxFreePageCount = DefaultMaxFreePageCount,
        bool zeroPages = false,
        BackgroundPageQueue * backgroundPageQueue = nullptr,
        uint maxAllocPageCount = DefaultMaxAllocPageCount,
        uint secondaryAllocPageCount = DefaultSecondaryAllocPageCount,
        bool stopAllocationOnOutOfMemory = false,
        bool excludeGuardPages = false);

    virtual ~PageAllocatorBase();

    bool IsClosed() const { return isClosed; }
    void Close() { Assert(!isClosed); isClosed = true; }

    AllocationPolicyManager * GetAllocationPolicyManager() { return policyManager; }

    uint GetMaxAllocPageCount();

    //VirtualAllocator APIs
    TVirtualAlloc * GetVirtualAllocator() { return virtualAllocator; }
    void SetVirtualAllocator(TVirtualAlloc * virtualAllocator)
    {
        Assert(virtualAllocator != nullptr);
        PVOID oldVirtualAllocator = InterlockedCompareExchangePointer((PVOID*) &(this->virtualAllocator), virtualAllocator, NULL);
        AssertMsg(oldVirtualAllocator == nullptr || oldVirtualAllocator == (PVOID)virtualAllocator, "Trying to set a new value for VirtualAllocWrapper ? - INVALID");
    }
    bool IsPreReservedPageAllocator() { return virtualAllocator != nullptr; }


    PageAllocation * AllocPagesForBytes(size_t requestedBytes);
    PageAllocation * AllocAllocation(size_t pageCount);

    void ReleaseAllocation(PageAllocation * allocation);
    void ReleaseAllocationNoSuspend(PageAllocation * allocation);

    char * Alloc(size_t * pageCount, SegmentBase<TVirtualAlloc> ** segment);

    void Release(void * address, size_t pageCount, void * segment);

    char * AllocPages(uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment);
    char * AllocPagesPageAligned(uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment, PageHeapMode pageHeapFlags);

    void ReleasePages(__in void * address, uint pageCount, __in void * pageSegment);
    void BackgroundReleasePages(void * address, uint pageCount, PageSegmentBase<TVirtualAlloc> * pageSegment);

    // Decommit
    void DecommitNow(bool all = true);
    void SuspendIdleDecommit();
    void ResumeIdleDecommit();

    void StartQueueZeroPage();
    void StopQueueZeroPage();
    void ZeroQueuedPages();
    void BackgroundZeroQueuedPages();
    void FlushBackgroundPages();

    bool DisableAllocationOutOfMemory() const { return disableAllocationOutOfMemory; }
    void ResetDisableAllocationOutOfMemory() { disableAllocationOutOfMemory = false; }

#ifdef RECYCLER_MEMORY_VERIFY
    void EnableVerify() { verifyEnabled = true; }
#endif
#if defined(RECYCLER_NO_PAGE_REUSE) || defined(ARENA_MEMORY_VERIFY)
    void ReenablePageReuse() { Assert(disablePageReuse); disablePageReuse = false; }
    bool DisablePageReuse() { bool wasDisablePageReuse = disablePageReuse; disablePageReuse = true; return wasDisablePageReuse; }
#endif

#if DBG
    bool HasZeroQueuedPages() const;
    virtual void SetDisableThreadAccessCheck() { disableThreadAccessCheck = true;}
    virtual void SetEnableThreadAccessCheck() { disableThreadAccessCheck = false; }

    virtual bool IsIdleDecommitPageAllocator() const { return false; }
    virtual bool HasMultiThreadAccess() const { return false; }
    bool ValidThreadAccess()
    {
        DWORD currentThreadId = ::GetCurrentThreadId();
        return disableThreadAccessCheck ||
            (this->concurrentThreadId == -1 && this->threadContextHandle == NULL) ||  // JIT thread after close
            (this->concurrentThreadId != -1 && this->concurrentThreadId == currentThreadId) ||
            this->threadContextHandle == GetCurrentThreadContextId();
    }
    virtual void UpdateThreadContextHandle(ThreadContextId updatedThreadContextHandle) { threadContextHandle = updatedThreadContextHandle; }
    void SetConcurrentThreadId(DWORD threadId) { this->concurrentThreadId = threadId; }
    void ClearConcurrentThreadId() { this->concurrentThreadId = (DWORD)-1; }
    DWORD GetConcurrentThreadId() { return this->concurrentThreadId;  }
    DWORD HasConcurrentThreadId() { return this->concurrentThreadId != -1; }

#endif

#if DBG_DUMP
    wchar_t const * debugName;
#endif
protected:
    SegmentBase<TVirtualAlloc> * AllocSegment(size_t pageCount);
    void ReleaseSegment(SegmentBase<TVirtualAlloc> * segment);

    template <bool doPageAlign>
    char * AllocInternal(size_t * pageCount, SegmentBase<TVirtualAlloc> ** segment);

    template <bool notPageAligned>
    char * SnailAllocPages(uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment, PageHeapMode pageHeapFlags);
    void OnAllocFromNewSegment(uint pageCount, __in void* pages, SegmentBase<TVirtualAlloc>* segment);

    template <bool notPageAligned>
    char * TryAllocFreePages(uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment, PageHeapMode pageHeapFlags);

    template <bool notPageAligned>
    char * TryAllocDecommitedPages(uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment, PageHeapMode pageHeapFlags);

    DListBase<PageSegmentBase<TVirtualAlloc>> * GetSegmentList(PageSegmentBase<TVirtualAlloc> * segment);
    void TransferSegment(PageSegmentBase<TVirtualAlloc> * segment, DListBase<PageSegmentBase<TVirtualAlloc>> * fromSegmentList);

    void FillAllocPages(__in void * address, uint pageCount);
    void FillFreePages(__in void * address, uint pageCount);

    struct FreePageEntry : public SLIST_ENTRY
    {
        PageSegmentBase<TVirtualAlloc> * segment;
        uint pageCount;
    };

    bool IsPageSegment(SegmentBase<TVirtualAlloc>* segment)
    {
        return segment->GetAvailablePageCount() <= maxAllocPageCount;
    }

#if DBG_DUMP
    virtual void DumpStats() const;
#endif
    virtual PageSegmentBase<TVirtualAlloc> * AddPageSegment(DListBase<PageSegmentBase<TVirtualAlloc>>& segmentList);
    static PageSegmentBase<TVirtualAlloc> * AllocPageSegment(DListBase<PageSegmentBase<TVirtualAlloc>>& segmentList, PageAllocatorBase<TVirtualAlloc> * pageAllocator, bool external);

    // Zero Pages
    void AddPageToZeroQueue(__in void * address, uint pageCount, __in PageSegmentBase<TVirtualAlloc> * pageSegment);
    bool HasZeroPageQueue() const;
    bool ZeroPages() const { return zeroPages; }
    bool QueueZeroPages() const { return queueZeroPages; }

    FreePageEntry * PopPendingZeroPage();
#if DBG
    void Check();
    bool disableThreadAccessCheck;
#endif

protected:
    // Data
    DListBase<PageSegmentBase<TVirtualAlloc>> segments;
    DListBase<PageSegmentBase<TVirtualAlloc>> fullSegments;
    DListBase<PageSegmentBase<TVirtualAlloc>> emptySegments;
    DListBase<PageSegmentBase<TVirtualAlloc>> decommitSegments;

    DListBase<SegmentBase<TVirtualAlloc>> largeSegments;

    uint maxAllocPageCount;
    DWORD allocFlags;
    uint maxFreePageCount;
    size_t freePageCount;
    uint secondaryAllocPageCount;
    bool isClosed;
    bool stopAllocationOnOutOfMemory;
    bool disableAllocationOutOfMemory;
    bool excludeGuardPages;
    AllocationPolicyManager * policyManager;
    TVirtualAlloc * virtualAllocator;

#ifndef JD_PRIVATE
    Js::ConfigFlagsTable& pageAllocatorFlagTable;
#endif

    // zero pages
    BackgroundPageQueue * backgroundPageQueue;
    bool zeroPages;
    bool queueZeroPages;
    bool hasZeroQueuedPages;

    // Idle Decommit
    bool isUsed;
    size_t minFreePageCount;
    uint idleDecommitEnterCount;

    void UpdateMinFreePageCount();
    void ResetMinFreePageCount();
    void ClearMinFreePageCount();
    void AddFreePageCount(uint pageCount);

    static uint GetFreePageLimit() { return 0; }

#if DBG
    size_t debugMinFreePageCount;
    ThreadContextId threadContextHandle;
    DWORD concurrentThreadId;
#endif
#if DBG_DUMP
    size_t decommitPageCount;
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    bool verifyEnabled;
#endif
#if defined(RECYCLER_NO_PAGE_REUSE) || defined(ARENA_MEMORY_VERIFY)
    bool disablePageReuse;
#endif

    friend class SegmentBase<TVirtualAlloc>;
    friend class PageSegmentBase<TVirtualAlloc>;
    friend class IdleDecommit;

protected:
    virtual bool CreateSecondaryAllocator(SegmentBase<TVirtualAlloc>* segment, SecondaryAllocator** allocator)
    {
        *allocator = nullptr;
        return true;
    }

    bool IsAddressInSegment(__in void* address, const PageSegmentBase<TVirtualAlloc>& segment);
    bool IsAddressInSegment(__in void* address, const SegmentBase<TVirtualAlloc>& segment);

private:
    uint GetSecondaryAllocPageCount() const { return this->secondaryAllocPageCount; }
    void IntegrateSegments(DListBase<PageSegmentBase<TVirtualAlloc>>& segmentList, uint segmentCount, size_t pageCount);
    void QueuePages(void * address, uint pageCount, PageSegmentBase<TVirtualAlloc> * pageSegment);

    template <bool notPageAligned>
    char* AllocPagesInternal(uint pageCount, PageSegmentBase<TVirtualAlloc> ** pageSegment, PageHeapMode pageHeapModeFlags = PageHeapMode::PageHeapModeOff);

#ifdef PROFILE_MEM
    PageMemoryData * memoryData;
#endif

    size_t usedBytes;
    PageAllocatorType type;

    size_t reservedBytes;
    size_t committedBytes;
    size_t numberOfSegments;

#ifdef PERF_COUNTERS
    PerfCounter::Counter& GetReservedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetReservedSizeCounter(type);
    }
    PerfCounter::Counter& GetCommittedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetCommittedSizeCounter(type);
    }
    PerfCounter::Counter& GetUsedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetUsedSizeCounter(type);
    }
    PerfCounter::Counter& GetTotalReservedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetTotalReservedSizeCounter();
    }
    PerfCounter::Counter& GetTotalCommittedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetTotalCommittedSizeCounter();
    }
    PerfCounter::Counter& GetTotalUsedSizeCounter() const
    {
        return PerfCounter::PageAllocatorCounterSet::GetTotalUsedSizeCounter();
    }
#endif
    void AddReservedBytes(size_t bytes);
    void SubReservedBytes(size_t bytes);
    void AddCommittedBytes(size_t bytes);
    void SubCommittedBytes(size_t bytes);
    void AddUsedBytes(size_t bytes);
    void SubUsedBytes(size_t bytes);
    void AddNumberOfSegments(size_t segmentCount);
    void SubNumberOfSegments(size_t segmentCount);

    bool RequestAlloc(size_t byteCount)
    {
        if (disableAllocationOutOfMemory)
        {
            return false;
        }

        if (policyManager != nullptr)
        {
            return policyManager->RequestAlloc(byteCount);
        }

        return true;
    }

    void ReportFree(size_t byteCount)
    {
        if (policyManager != nullptr)
        {
            policyManager->ReportFree(byteCount);
        }
    }

    template <typename T>
    void ReleaseSegmentList(DListBase<T> * segmentList);

protected:
    // Instrumentation
    void LogAllocSegment(SegmentBase<TVirtualAlloc> * segment);
    void LogAllocSegment(uint segmentCount, size_t pageCount);
    void LogFreeSegment(SegmentBase<TVirtualAlloc> * segment);
    void LogFreeDecommittedSegment(SegmentBase<TVirtualAlloc> * segment);
    void LogFreePartiallyDecommitedPageSegment(PageSegmentBase<TVirtualAlloc> * pageSegment);

    void LogAllocPages(size_t pageCount);
    void LogFreePages(size_t pageCount);
    void LogCommitPages(size_t pageCount);
    void LogRecommitPages(size_t pageCount);
    void LogDecommitPages(size_t pageCount);

    void ReportFailure(size_t byteCount)
    {
        if (this->stopAllocationOnOutOfMemory)
        {
            this->disableAllocationOutOfMemory = true;
        }

        if (policyManager != nullptr)
        {
            policyManager->ReportFailure(byteCount);
        }
    }
};

template<typename TVirtualAlloc>
class HeapPageAllocator : public PageAllocatorBase<TVirtualAlloc>
{
public:
    HeapPageAllocator(AllocationPolicyManager * policyManager, bool allocXdata, bool excludeGuardPages);

    BOOL ProtectPages(__in char* address, size_t pageCount, __in void* segment, DWORD dwVirtualProtectFlags, DWORD* dwOldVirtualProtectFlags, DWORD desiredOldProtectFlag);
    bool AllocSecondary(void* segment, ULONG_PTR functionStart, DWORD functionSize, ushort pdataCount, ushort xdataSize, SecondaryAllocation* allocation);
    void ReleaseSecondary(const SecondaryAllocation& allocation, void* segment);
    void TrackDecommitedPages(void * address, uint pageCount, __in void* segment);
    void DecommitPages(__in char* address, size_t pageCount = 1);

    // Release pages that has already been decommited
    void    ReleaseDecommited(void * address, size_t pageCount, __in void * segment);
    bool    IsAddressFromAllocator(__in void* address);
    char *  InitPageSegment();

    PageSegmentBase<TVirtualAlloc> * AddPageSegment(DListBase<PageSegmentBase<TVirtualAlloc>>& segmentList);


private:
    bool         allocXdata;
    void         ReleaseDecommitedSegment(__in SegmentBase<TVirtualAlloc>* segment);
#if PDATA_ENABLED
    virtual bool CreateSecondaryAllocator(SegmentBase<TVirtualAlloc>* segment, SecondaryAllocator** allocator) override;
#endif

};

}
