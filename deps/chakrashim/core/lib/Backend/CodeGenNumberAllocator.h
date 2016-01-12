//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

/****************************************************************************
 * CodeGenNumberThreadAllocator
 *
 *   Recycler can't be used by multiple threads. This allocator is used by
 *   the JIT to allocate numbers in the background thread. It allocates
 *   pages directly from the OS and then allocates numbers from these pages,
 *   along with a linked list of chunks that hold the numbers so the native
 *   entry points can keep these numbers alive. Then this memory is
 *   integrated back into the main thread's recycler at the beginning of
 *   GC.
 *
 *   This class requires intimate knowledge of how recycler allocates memory
 *   so the memory can be integrated back. So changes in the recycler
 *   will affect this allocator.
 *
 *   An alternative solution is to record the number we need to create,
 *   and only create them on the main thread when the entry point is used.
 *   However, since we don't have the address of the number at JIT time,
 *   we will have to incur two indirections (loading the array of pointers to
 *   numbers and loading the numbers) in the JIT code which is not desirable.
 *
 * Implementation details:
 *
 *   The segments can be integrated back to the recycler's page allocator
 *   immediately. They are all marked as used initially.
 *
 *   Numbers and the linked list chunks are allocated in separate pages
 *   as number will be a leaf page when integrated back to the recycler
 *   and the linked list chunks will be normal pages.
 *
 *   Generally, when a page is full, it is (almost) ready to be integrated
 *   back to the recycler. However the number pages need to wait until
 *   the referencing linked list chunk is integrated before it can be
 *   integrated (otherwise, the recycler will not see the reference that
 *   keeps the numbers alive). So when a number page is full, it will
 *   go to pendingReferenceNumberBlock list. Then, when a linked list
 *   chunk is full, all the pages in pendingReferenceNumberBlock can go to
 *   pendingFlushNumberBlock list and the linked list chunk page will
 *   go to pendingFlushChunkBlock.
 *
 *   Once we finish jitting a function and the number link list is set on the
 *   entry point, these pages are ready to be integrated back to recycler
 *   and be moved to pendingIntegration*Pages lists. Access to the
 *   pendingIntegration*Pages are synchronized, therefore the main thread
 *   can do the integration before GC happens.
 *
 ****************************************************************************/
struct CodeGenNumberChunk
{
    static int const MaxNumberCount = 3;
    Js::JavascriptNumber * numbers[MaxNumberCount];
    CodeGenNumberChunk * next;
};
CompileAssert(
    sizeof(CodeGenNumberChunk) == HeapConstants::ObjectGranularity ||
    sizeof(CodeGenNumberChunk) == HeapConstants::ObjectGranularity * 2);

class CodeGenNumberThreadAllocator
{
public:
    CodeGenNumberThreadAllocator(Recycler * recycler);
    ~CodeGenNumberThreadAllocator();

    // All the public API's need to be guarded by critical sections.
    // Multiple jit threads access this.
    Js::JavascriptNumber * AllocNumber();
    CodeGenNumberChunk * AllocChunk();
    void Integrate();
    void FlushAllocations();

private:
    // All allocations are small allocations
    const size_t BlockSize = SmallAllocationBlockAttributes::PageCount * AutoSystemInfo::PageSize;

    void AllocNewNumberBlock();
    void AllocNewChunkBlock();
    size_t GetNumberAllocSize();
    size_t GetChunkAllocSize();

    CriticalSection cs;

    Recycler * recycler;
    PageSegment * currentNumberSegment;
    PageSegment * currentChunkSegment;
    char * numberSegmentEnd;
    char * currentNumberBlockEnd;
    char * nextNumber;
    char * chunkSegmentEnd;
    char * currentChunkBlockEnd;
    char * nextChunk;
    bool hasNewNumberBlock;
    bool hasNewChunkBlock;
    struct BlockRecord
    {
        BlockRecord(__in_ecount_pagesize char * blockAddress, PageSegment * segment) : blockAddress(blockAddress), segment(segment) {};
        char * blockAddress;
        PageSegment * segment;
    };
    // Keep track of segments and pages that needs to be integrated to the recycler.
    uint pendingIntegrationNumberSegmentCount;
    uint pendingIntegrationChunkSegmentCount;
    size_t pendingIntegrationNumberSegmentPageCount;
    size_t pendingIntegrationChunkSegmentPageCount;
    DListBase<PageSegment> pendingIntegrationNumberSegment;
    DListBase<PageSegment> pendingIntegrationChunkSegment;
    SListBase<BlockRecord> pendingIntegrationNumberBlock;
    SListBase<BlockRecord> pendingIntegrationChunkBlock;

    // These are finished pages during the code gen of the current function
    // We can't integrate them until the code gen is done for the function,
    // because the references for the number is not set on the entry point yet.
    SListBase<BlockRecord> pendingFlushNumberBlock;
    SListBase<BlockRecord> pendingFlushChunkBlock;

    // Numbers are reference by the chunks, so we need to wait until that is ready
    // to be flushed before the number page can be flushed. Otherwise, we might have number
    // integrated back to the GC, but the chunk hasn't yet, thus GC won't see the reference.
    SListBase<BlockRecord> pendingReferenceNumberBlock;
};

class CodeGenNumberAllocator
{
public:
    CodeGenNumberAllocator(CodeGenNumberThreadAllocator * threadAlloc, Recycler * recycler);
// We should never call this function if we are using tagged float
#if !FLOATVAR
    Js::JavascriptNumber * Alloc();
#endif
    CodeGenNumberChunk * Finalize();
private:

    Recycler * recycler;
    CodeGenNumberThreadAllocator * threadAlloc;
    CodeGenNumberChunk * chunk;
    CodeGenNumberChunk * chunkTail;
    uint currentChunkNumberCount;
#if DBG
    bool finalized;
#endif
};
