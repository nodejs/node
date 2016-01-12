//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

// Initialization order
//  AB AutoSystemInfo
//  AD PerfCounter
//  AE PerfCounterSet
//  AM Output/Configuration
//  AN MemProtectHeap
//  AP DbgHelpSymbolManager
//  AQ CFGLogger
//  AR LeakReport
//  AS JavascriptDispatch/RecyclerObjectDumper
//  AT HeapAllocator/RecyclerHeuristic
//  AU RecyclerWriteBarrierManager
#pragma warning(disable:4075)       // initializers put in unrecognized initialization area on purpose
#pragma init_seg(".CRT$XCAU")

#ifdef RECYCLER_WRITE_BARRIER

#ifdef RECYCLER_WRITE_BARRIER_BYTE
#ifdef _M_X64_OR_ARM64
X64WriteBarrierCardTableManager RecyclerWriteBarrierManager::x64CardTableManager;
X64WriteBarrierCardTableManager::CommitedSectionBitVector X64WriteBarrierCardTableManager::commitedSections(&HeapAllocator::Instance);

BYTE* RecyclerWriteBarrierManager::cardTable = RecyclerWriteBarrierManager::x64CardTableManager.Initialize();
#else
// Each byte in the card table covers 4096 bytes so the range covered by the table is 4GB
BYTE RecyclerWriteBarrierManager::cardTable[1 * 1024 * 1024];
#endif

#else
// Each *bit* in the card table covers 128 bytes. So each DWORD covers 4096 bytes and therefore the cardTable covers 4GB
DWORD RecyclerWriteBarrierManager::cardTable[1 * 1024 * 1024];
#endif

#ifdef RECYCLER_WRITE_BARRIER_BYTE
#ifdef _M_X64_OR_ARM64

bool
X64WriteBarrierCardTableManager::OnThreadInit()
{
    // We page in the card table sections for the current threads stack reservation
    // So any writes to stack allocated vars can also have the write barrier set

    NT_TIB* teb = (NT_TIB*) ::NtCurrentTeb();

    char* stackBase = (char*) teb->StackBase;
    char* stackEnd  = (char*) teb->StackLimit;

    size_t numPages = (stackBase - stackEnd) / AutoSystemInfo::PageSize;

    // stackEnd is the lower boundary
    return OnSegmentAlloc(stackEnd, numPages);
}

bool
X64WriteBarrierCardTableManager::OnSegmentAlloc(_In_ char* segmentAddress, size_t numPages)
{
    Assert(_cardTable);

    SetCommitState(OnSegmentAlloc);

    if (segmentAddress >= AutoSystemInfo::Data.lpMaximumApplicationAddress)
    {
        Assert(false); // How did this happen?
        SetCommitState(FailedMaxAddressExceeded);
        Js::Throw::FatalInternalError();
    }

    AutoCriticalSection critSec(&_cardTableInitCriticalSection);

    size_t  pageSize = AutoSystemInfo::PageSize;

    // First, check if the pages for this segment have already been committed
    // If they have, there is nothing for us to do here.
    void*   segmentEndAddress = segmentAddress + (numPages * pageSize);
    void*   segmentLastWritableAddress = (char*)segmentEndAddress - 1;
    BVIndex sectionStartIndex = GetSectionIndex(segmentAddress);
    BVIndex sectionLastIndex = GetSectionIndex(segmentLastWritableAddress);

#ifdef X64_WB_DIAG
    this->_lastSegmentAddress = segmentAddress;
    this->_lastSegmentNumPages = numPages;
    this->_lastSectionIndexStart = sectionStartIndex;
    this->_lastSectionIndexLast = sectionLastIndex;
#endif

    bool needCommit = false;
    for (BVIndex i = sectionStartIndex; i <= sectionLastIndex; i++)
    {
        if (!commitedSections.Test(i))
        {
            needCommit = true;
            break;
        }
    }

    if (!needCommit)
    {
        // The pages for this segment have already been committed.
        // We don't need to do anything more, since write barriers can
        // already be set for writes to this segment
        return true;
    }

    SetCommitState(OnNeedCommit);

    // There are uncommitted pages in this range. We'll commit the full range
    // We might commit some pages that are already committed but that's okay
    const uintptr_t startIndex = RecyclerWriteBarrierManager::GetCardTableIndex(segmentAddress);
    const uintptr_t endIndex   = RecyclerWriteBarrierManager::GetCardTableIndex(segmentEndAddress);

    Assert(startIndex <= endIndex);

    // Section Start is the card table's starting entry aligned *down* to the page boundary
    // Section End is the card table's ending entry aligned *up* to the page boundary
    BYTE* sectionStart = (BYTE*) (((uintptr_t) &_cardTable[startIndex]) & ~(pageSize - 1));
    BYTE* sectionEnd   = (BYTE*) Math::Align<uintptr_t>((uintptr_t)&_cardTable[endIndex], pageSize);
    size_t commitSize  = (sectionEnd - sectionStart);

#ifdef X64_WB_DIAG
    _lastSectionStart = sectionStart;
    _lastSectionEnd = sectionEnd;
#endif

    Assert(commitSize > 0);
    Assert(commitSize % pageSize == 0);
    Assert(commitSize / pageSize == sectionLastIndex - sectionStartIndex + 1);

    LPVOID ret = ::VirtualAlloc((LPVOID) sectionStart, commitSize, MEM_COMMIT, PAGE_READWRITE);
    if (!ret)
    {
        // If this is the error that occurred while trying to commit the page, this likely means
        // that the page we tried to commit is outside out reservation, which means that our reservation
        // was too small. This can happen if Windows increases the maximum process address space size
        // If this happens, X64WriteBarrierCardTableManager::Initialize will have to be updated
        Assert(::GetLastError() != ERROR_INVALID_ADDRESS);
        SetCommitState(FailedVirtualAlloc);
        return false;
    }

    SetCommitState(OnSectionCommitted);
    BVIndex sectionIndex = sectionStartIndex;

    try
    {
#ifdef EXCEPTION_CHECK
        AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_DisableCheck);
#endif

        for (; sectionIndex <= sectionLastIndex; sectionIndex++)
        {
            commitedSections.Set(sectionIndex);
        }

        SetCommitState(OnCommitBitSet);
    }
    catch (Js::OutOfMemoryException)
    {
        SetCommitState(FailedCommitBitSet);

        // We ran out of memory allocating a node for the sparse bit vector, so clean up
        // and return false
        // Since setting sectionIndex threw the exception, we don't clear it, we clear until the index before it
        for (BVIndex i = sectionStartIndex; i < sectionIndex; i++)
        {
            BOOLEAN wasSet = commitedSections.TestAndClear(i);
            Assert(wasSet == TRUE);
        }

#pragma prefast(suppress:6250, "This method decommits memory")
        BOOL result = ::VirtualFree((LPVOID)sectionStart, commitSize, MEM_DECOMMIT);
        Assert(result != 0);
        return false;
    }

    return true;
}

bool
X64WriteBarrierCardTableManager::OnSegmentFree(_In_ char* segmentAddress, size_t numPages)
{
    Assert(_cardTable);
    return true;
}

X64WriteBarrierCardTableManager::~X64WriteBarrierCardTableManager()
{
    if (_cardTable != nullptr)
    {
        BOOL fSuccess = ::VirtualFree(_cardTable, 0, MEM_RELEASE);
        Assert(fSuccess == TRUE);
    }
}

BVIndex
X64WriteBarrierCardTableManager::GetSectionIndex(void* address)
{
    size_t pageSize = AutoSystemInfo::PageSize;
    size_t sectionSize = (pageSize * pageSize);

    BVIndex sectionIndex = (BVIndex)(((uintptr_t)address) / sectionSize);
    return sectionIndex;
}

BYTE *
X64WriteBarrierCardTableManager::Initialize()
{
    AutoCriticalSection critSec(&_cardTableInitCriticalSection);

    if (_cardTable == nullptr)
    {
        // We have two sizes for the card table on 64 bit builds
        // On Win8.1 and later, the process address space size is 128 TB, so we reserve 32 GB for the card table
        // On Win7, the max address space size is 192 GB, so we reserve 48 MB for the card table.
        // On Win8, reserving 32 GB is fine since reservations don't incur a cost. On Win7, the cost
        // of a reservation can be approximated as 2KB per MB of reserved size. In our case, we take
        // an overhead of 96KB for our card table.
        const unsigned __int64 maxUmProcessAddressSpace = (__int64) AutoSystemInfo::Data.lpMaximumApplicationAddress;
        _cardTableNumEntries = Math::Align<size_t>(maxUmProcessAddressSpace / AutoSystemInfo::PageSize, AutoSystemInfo::PageSize) /* s_writeBarrierPageSize */;

        LPVOID cardTableSpace = ::VirtualAlloc(NULL, _cardTableNumEntries, MEM_RESERVE, PAGE_READWRITE);

        if (cardTableSpace == nullptr)
        {
            return false;
        }

        _cardTable = (BYTE*) cardTableSpace;
    }

    return _cardTable;
}

bool
RecyclerWriteBarrierManager::OnThreadInit()
{
    return x64CardTableManager.OnThreadInit();
}

bool
RecyclerWriteBarrierManager::OnSegmentAlloc(_In_ char* segmentAddress, size_t numPages)
{
    return x64CardTableManager.OnSegmentAlloc(segmentAddress, numPages);
}

bool
RecyclerWriteBarrierManager::OnSegmentFree(_In_ char* segmentAddress, size_t numPages)
{
    return x64CardTableManager.OnSegmentFree(segmentAddress, numPages);
}
#endif

#else
#error Not implemented for bit-array card table
#endif

void
RecyclerWriteBarrierManager::WriteBarrier(void * address)
{
#ifdef RECYCLER_WRITE_BARRIER_BYTE
    const uintptr_t index = GetCardTableIndex(address);
    cardTable[index] = 1;
#else
    uint bitShift = (((uint)address) >> s_BitArrayCardTableShift);
    uint bitMask = 1 << bitShift;
    const uint cardIndex = ((uint) address) / (s_BytesPerCard);
    cardTable[cardIndex] |= bitMask;
#endif
#if DBG_DUMP
    // Global to process, use global configuration here
    if (PHASE_VERBOSE_TRACE1(Js::SWBPhase))
    {
        Output::Print(L"Writing to 0x%p (CIndex: %u)\n", address, index);
    }
#endif
}


void
RecyclerWriteBarrierManager::WriteBarrier(void * address, size_t ptrCount)
{
#ifdef RECYCLER_WRITE_BARRIER_BYTE
    uintptr_t startIndex = GetCardTableIndex(address);
    char * endAddress = (char *)Math::Align<INT_PTR>((INT_PTR)((char *)address + sizeof(void *) * ptrCount), s_WriteBarrierPageSize);
    uintptr_t endIndex = GetCardTableIndex(endAddress);
    Assert(startIndex <= endIndex);
    memset(cardTable + startIndex, 1, endIndex - startIndex);
    GlobalSwbVerboseTrace(L"Writing to 0x%p (CIndex: %u-%u)\n", address, startIndex, endIndex);
#else
    uint bitShift = (((uint)address) >> s_BitArrayCardTableShift);
    uint bitMask = 0xFFFFFFFF << bitShift;
    uint cardIndex = ((uint)address) / s_BytesPerCard);

    char * endAddress = (char *)Math::Align((INT_PTR)((char *)address + sizeof(void *) * ptrCount), s_BytesPerCardBit);
    char * alignedAddress = (char *)Math::Align((INT_PTR)address, s_WriteBarrierPageSize);
    if (alignedAddress > endAddress)
    {
        uint endAddressShift = (((uint)endAddress) >> s_BitArrayCardTableShift);
        uint endAddressBitMask = 0xFFFFFFFF << endAddressShift;
        bitMask &= ~endAddressBitMask;
        cardTable[cardIndex] |= bitMask;
        return;
    }
    cardTable[cardIndex] |= bitMask;

    size_t remainingBytes = endAddress - alignedAddress;
    size_t fullMaskCount = remainingBytes  / g_WriteBarrierPageSize;
    memset(&cardTable[cardIndex + 1], 0xFFFFFFFF, fullMaskCount * sizeof(DWORD));

    uint endAddressShift = (((uint)endAddress) >> s_BitArrayCardTableShift);
    uint endAddressBitMask = 0xFFFFFFFF << endAddressShift;
    cardTable[cardIndex + 1 + fullMaskCount] |= ~endAddressBitMask;
#endif
}

uintptr_t
RecyclerWriteBarrierManager::GetCardTableIndex(void *address)
{
    return ((uintptr_t)address) / s_BytesPerCard;
}

void
RecyclerWriteBarrierManager::ResetWriteBarrier(void * address, size_t pageCount)
{
    uintptr_t cardIndex = GetCardTableIndex(address);
    if (pageCount == 1)
    {
        cardTable[cardIndex] = 0;
    }
    else
    {
#ifdef RECYCLER_WRITE_BARRIER_BYTE
        memset(&cardTable[cardIndex], 0, pageCount);
#else
        memset(&cardTable[cardIndex], 0, sizeof(DWORD) * pageCount);
#endif
    }
#if DBG_DUMP
    // Global to process, use global configuration here
    if (PHASE_VERBOSE_TRACE1(Js::SWBPhase))
    {
        Output::Print(L"Resetting %u pages at CIndex: %u\n", address, pageCount, cardIndex);
    }
#endif
}

#ifdef RECYCLER_WRITE_BARRIER_BYTE
BYTE
#else
DWORD
#endif
RecyclerWriteBarrierManager::GetWriteBarrier(void * address)
{
    return cardTable[GetCardTableIndex(address)];
}

#endif
