//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "CommonDefines.h"

namespace Memory
{
class HeapBlockMap32
{
public:
    // Segment mapping

    static const uint L1Count = 4096;
    static const uint L2Count = 256;
    static const uint PageSize = AutoSystemInfo::PageSize; // 4096

    // Mark bit definitions

    static const uint PageMarkBitCount = PageSize / HeapConstants::ObjectGranularity;
    static const uint L2ChunkMarkBitCount = L2Count * PageMarkBitCount;

#if defined(_M_X64_OR_ARM64) && !defined(JD_PRIVATE)
    static const size_t TotalSize = 0x100000000;        // 4GB
#endif

    typedef BVStatic<PageMarkBitCount> PageMarkBitVector;
    typedef BVStatic<L2ChunkMarkBitCount> L2ChunkMarkBitVector;

    // Max size of GetWriteWatch batching.
    // This must be >= PageSegment page count for small heap block segments,
    // so set it to the MaxPageCount for PageSegments.
    static const uint MaxGetWriteWatchPages = PageSegment::MaxPageCount;

#if defined(_M_X64_OR_ARM64)
    HeapBlockMap32(__in char * startAddress);
#else
    HeapBlockMap32();
#endif
    ~HeapBlockMap32();

    bool EnsureHeapBlock(void * address, uint pageCount);
    void SetHeapBlockNoCheck(void * address, uint pageCount, HeapBlock * heapBlock, HeapBlock::HeapBlockType blockType, byte bucketIndex);
    bool SetHeapBlock(void * address, uint pageCount, HeapBlock * heapBlock, HeapBlock::HeapBlockType blockType, byte bucketIndex);
    void ClearHeapBlock(void * address, uint pageCount);

    HeapBlock * GetHeapBlock(void * address);
    bool Empty() const { return count == 0; }

    // Heap block map marking
    PageMarkBitVector * GetPageMarkBitVector(void * address);
    template <size_t BitCount>
    BVStatic<BitCount>* GetMarkBitVectorForPages(void * address);

    uint GetMarkCount(void* address, uint pageCount);
    template <bool interlocked>
    void Mark(void * candidate, MarkContext * markContext);
    template <bool interlocked>
    void MarkInterior(void * candidate, MarkContext * markContext);

    bool IsMarked(void * address) const;
    void SetMark(void * address);
    bool TestAndSetMark(void * address);

    void ResetMarks();

#ifdef CONCURRENT_GC_ENABLED
    void ResetWriteWatch(Recycler * recycler);
    uint Rescan(Recycler * recycler, bool resetWriteWatch);
    void MakeAllPagesReadOnly(Recycler* recycler);
    void MakeAllPagesReadWrite(Recycler* recycler);
#endif

    void Cleanup(bool concurrentFindImplicitRoot);

    bool OOMRescan(Recycler * recycler);

#ifdef RECYCLER_STRESS
    void InduceFalsePositives(Recycler * recycler);
#endif

#ifdef RECYCLER_VERIFY_MARK
    bool IsAddressInNewChunk(void * address);
#endif

private:
    friend class PageSegmentBase<VirtualAllocWrapper>;

#ifdef JD_PRIVATE
    friend class EXT_CLASS;
    friend class HeapBlockHelper;
#endif

    template <class Fn>
    void ForEachSegment(Recycler * recycler, Fn func);

    void ChangeProtectionLevel(Recycler* recycler, DWORD protectFlags, DWORD expectedOldFlags);

    static uint GetLevel1Id(void * address)
    {
        return ::Math::PointerCastToIntegralTruncate<uint>(address) / L2Count / PageSize;
    }

public:
    static uint GetLevel2Id(void * address)
    {
        return ::Math::PointerCastToIntegralTruncate<uint>(address) % (L2Count * PageSize) / PageSize;
    }

private:
    static UINT GetWriteWatchHelper(Recycler * recycler, DWORD writeWatchFlags, void* baseAddress, size_t regionSize,
        void** addresses, ULONG_PTR* count, LPDWORD granularity);
    static UINT GetWriteWatchHelperOnOOM(DWORD writeWatchFlags, _In_ void* baseAddress, size_t regionSize,
        _Out_writes_(*count) void** addresses, _Inout_ ULONG_PTR* count, LPDWORD granularity);

    static void * GetAddressFromIds(uint id1, uint id2)
    {
        Assert(id1 < L1Count);
        Assert(id2 < L2Count);

        return (void *)(((id1 * L2Count) + id2) * PageSize);
    }

    struct HeapBlockInfo
    {
        HeapBlock::HeapBlockType blockType;
        byte bucketIndex;
    };

    // We want HeapBlockInfo to be as small as possible to get the best cache locality for this info.
    CompileAssert(sizeof(HeapBlockInfo) == sizeof(ushort));

    class L2MapChunk
    {
    public:
        L2MapChunk();
        ~L2MapChunk();
        HeapBlock * Get(void * address);
        void Set(uint id2, uint pageCount, HeapBlock * heapBlock, HeapBlock::HeapBlockType blockType, byte bucketIndex);
        void Clear(uint id2, uint pageCount);
        bool IsEmpty() const;
        PageMarkBitVector * GetPageMarkBitVector(void * address);
        PageMarkBitVector * GetPageMarkBitVector(uint pageIndex);
        template <size_t BitCount> BVStatic<BitCount>* GetMarkBitVectorForPages(void * address);
        template <size_t BitCount> BVStatic<BitCount>* GetMarkBitVectorForPages(uint pageIndex);

        bool IsMarked(void * address) const;
        void SetMark(void * address);

        static uint GetMarkBitIndex(void * address)
        {
            uint bitIndex = (::Math::PointerCastToIntegralTruncate<uint>(address) % (L2Count * PageSize)) / HeapConstants::ObjectGranularity;
            Assert(bitIndex < L2ChunkMarkBitCount);
            return bitIndex;
        }


        // Mark bits for objects that live in this particular chunk
        // Each L2 chunk has 1 bit for each object that lives in this chunk
        // The L2 chunk represents 256 * 4096 bytes = 1 MB of address space
        // This means, that on 32 bit systems, where each object is at least 16 bytes, we can have 64K objects
        // On 64 bit systems, where each object is at least 32 bytes, we can have 32K objects
        // Therefore, that on 32 bit systems, the mark bits take up 8KB, and on 64 bit systems, they take up 4KB
        // This is more general purpose than if the mark bits are on the heap block, and is more runtime efficient
        // However, it's less memory efficient (e.g. a large object which is 1 MB + 1 byte, rounded up to 1028 KB,
        // would before take up 1 byte for it's mark bits- now it'll have a cost of 16KB, one for each of the L2 segments it spans)
        L2ChunkMarkBitVector markBits;

        // HeapBlockInfo for each page in our range
        HeapBlockInfo blockInfo[L2Count];

        // HeapBlock * for each page in our range (or nullptr, if no block)
        HeapBlock* map[L2Count];

#ifdef RECYCLER_VERIFY_MARK
        bool isNewChunk;
#endif

#if DBG
        ushort pageMarkCount[L2Count];
#endif
    };

    template <bool interlocked>
    bool MarkInternal(L2MapChunk * chunk, void * candidate);

    template <bool interlocked, bool updateChunk>
    bool MarkInteriorInternal(MarkContext * markContext, L2MapChunk *& chunk, void * originalCandidate, void * realCandidate);

    template <typename TBlockType>
    TBlockType* GetHeapBlockForRescan(L2MapChunk* chunk, uint id2) const;

    template <class TBlockType>
    bool RescanHeapBlock(void * dirtyPage, HeapBlock::HeapBlockType blockType, L2MapChunk* chunk, uint id2, bool* anyObjectsMarkedOnPage, Recycler * recycler);
    template <class TBlockType>
    bool RescanHeapBlockOnOOM(TBlockType* heapBlock, char* pageAddress, HeapBlock::HeapBlockType blockType, uint bucketIndex, L2MapChunk * chunk, Recycler * recycler);
    bool RescanPage(void * dirtyPage, bool* anyObjectsMarkedOnPage, Recycler * recycler);

    uint count;
    L2MapChunk * map[L1Count];
    bool anyHeapBlockRescannedDuringOOM;

#if defined(_M_X64_OR_ARM64)
    // On 64 bit, this structure only maps one particular 32 bit space.
    // Store the startAddress of that 32 bit space so we know which it is.
    // This value should always be 4GB aligned.
    char * startAddress;
#endif

public:
#if DBG
    ushort GetPageMarkCount(void * address) const;
    void SetPageMarkCount(void * address, ushort markCount);

    template <uint BitVectorCount>
    void VerifyMarkCountForPages(void * address, uint pageCount);
#endif

private:
    template <class Fn>
    void ForEachChunkInAddressRange(void * address, size_t pageCount, Fn fn);
};


#if defined(_M_X64_OR_ARM64)

class HeapBlockMap64
{
public:
    HeapBlockMap64();
    ~HeapBlockMap64();

    bool EnsureHeapBlock(void * address, size_t pageCount);
    void SetHeapBlockNoCheck(void * address, size_t pageCount, HeapBlock * heapBlock, HeapBlock::HeapBlockType blockType, byte bucketIndex);
    bool SetHeapBlock(void * address, size_t pageCount, HeapBlock * heapBlock, HeapBlock::HeapBlockType blockType, byte bucketIndex);
    void ClearHeapBlock(void * address, size_t pageCount);
    HeapBlock * GetHeapBlock(void * address);

    HeapBlockMap32::PageMarkBitVector * GetPageMarkBitVector(void * address);

    template <size_t BitCount>
    BVStatic<BitCount>* GetMarkBitVectorForPages(void * address);

    uint GetMarkCount(void* address, uint pageCount);
    template <bool interlocked>
    void Mark(void * candidate, MarkContext * markContext);
    template <bool interlocked>
    void MarkInterior(void * candidate, MarkContext * markContext);

    bool IsMarked(void * address) const;
    void SetMark(void * address);
    bool TestAndSetMark(void * address);

    void ResetMarks();

#ifdef CONCURRENT_GC_ENABLED
    void ResetWriteWatch(Recycler * recycler);
    uint Rescan(Recycler * recycler, bool resetWriteWatch);
    void MakeAllPagesReadOnly(Recycler* recycler);
    void MakeAllPagesReadWrite(Recycler* recycler);
#endif

    void Cleanup(bool concurrentFindImplicitRoot);

    bool OOMRescan(Recycler * recycler);

#ifdef RECYCLER_STRESS
    void InduceFalsePositives(Recycler * recycler);
#endif

#ifdef RECYCLER_VERIFY_MARK
    bool IsAddressInNewChunk(void * address);
#endif

private:
    friend class HeapBlockMap32;
#ifdef JD_PRIVATE
    friend class HeapBlockHelper;
#endif

    struct Node
    {
        Node(__in char * startAddress) : map(startAddress) { }

        uint nodeIndex;
        Node * next;
        HeapBlockMap32 map;
    };

    static const uint PagesPer4GB = 1 << 20; // = 1M,  assume page size = 4K

    static uint GetNodeIndex(void * address)
    {
        return GetNodeIndex((ULONG64)address);
    }

    static uint GetNodeIndex(ULONG64 address)
    {
        return (uint)((ULONG64)address >> 32);
    }

    Node * FindOrInsertNode(void * address);
    Node * FindNode(void * address) const;

    template <class Fn>
    void ForEachNodeInAddressRange(void * address, size_t pageCount, Fn fn);

    Node * list;

public:
#if DBG
    ushort GetPageMarkCount(void * address) const;
    void SetPageMarkCount(void * address, ushort markCount);

    template <uint BitVectorCount>
    void VerifyMarkCountForPages(void * address, uint pageCount);
#endif

#if !defined(JD_PRIVATE)
    static char * GetNodeStartAddress(void * address)
    {
        return (char *)(((size_t)address) & ~(HeapBlockMap32::TotalSize - 1));
    }
#endif
};

typedef HeapBlockMap64 HeapBlockMap;

#else
typedef HeapBlockMap32 HeapBlockMap;
#endif
}
