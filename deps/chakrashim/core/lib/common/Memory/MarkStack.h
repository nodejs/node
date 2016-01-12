//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifdef MARKSTACK_TEST
#define MARKSTACK_DIAG 1
#endif

#define DEFAULT_CHUNK_SIZE  2 MEGABYTES // Reserve in 8 MB chunks
#define DEFAULT_PAGE_SIZE   4 KILOBYTES
#define DEFAULT_COMMIT_SIZE ((256 KILOBYTES) - DEFAULT_PAGE_SIZE) // Commit in 1 MB slices adjusted for the guard page

namespace Helpers
{
    void* CommitMemory(void* address, DWORD size, DWORD flags) throw();
    BOOL DecommitMemory(void* address, DWORD size) throw();
    void* ReserveMemory(DWORD size, DWORD flags) throw();
    BOOL ReleaseMemory(void* address, DWORD size) throw();
};

//
// Definition of the MarkStack class
// This is a guard-page based stack class that's used for the purposes of marking objects in the Recycler
// By default, it reserves an 8 MB chunk of memory, and commits in 1 MB chunks.
// At the end of 1 MB of memory is a guard page. When we've added 1 MB's worth of entries into the mark stack,
// the stack is moved to the guard page. When we attempt to write to this entry, a trap is triggered.
// At this point, we grow the stack. We then resume execution. Note that an entry is written to the start
// of the guard page when the trap is initially triggered but the rest of the page is left blank.
// When we pop the stack, we *do* do a bounds check. If the stack is not at the start, we simply move the stack back
// and return the current value. If the stack is at the start, we then "pop" the chunk. When the chunk is popped,
// the stack points to the start of the guard page.
//
// The stack consists of "Chunks", which is regions of memory that are initially reserved
// When we need more of the chunk, we commit pages within a chunk. If there are no more pages to commit, we allocate a new chunk.
// Chunks are linked through a doubly linked list whose pointers live on the start of the chunk
//
// By default, Chunks are 8MB, and commits are in 1MB ranges.
// There is a preallocated chunk- this means that when the recycler is created, we reserve 8MB for the purpose of the mark stack
// We commit 1MB of this memory up front (so it becomes part of the fixed cost of a Recycler instance)
// The preallocated chunk memory remains reserved for the lifetime of the Recycler
namespace MarkStack
{
    struct Chunk
    {
        Chunk* next;
        Chunk* previous;
    };

    struct MarkCandidate
    {
        void ** obj;
        size_t byteCount;
    };

    template <uint ChunkSize, uint PageSize, uint CommitSize>
    class MarkStack
    {
    public:
        __forceinline MarkCandidate* Push(void** obj, size_t byteCount) throw()
        {
            // We first cache the current stack locally
            // We then increment the stack pointer- at this point it could be pointing to an address that is past the guard page
            // We then try to write the item to the cached location. If that's within the guard page, the official stack gets adjusted
            MarkCandidate* item = (MarkCandidate*)stack;
            stack = stack + sizeof(MarkCandidate);
            item->obj = obj;
            item->byteCount = byteCount;
#if MARKSTACK_DIAG
            count++;
#endif

#if DBG
            if (this->chunkCount > 0)
            {
                for (Chunk* chunk = this->preAllocatedChunk; chunk->next != null; chunk = chunk->next)
                {
                    if (chunk->next == null)
                    {
                        char* page = (((char*)chunk) + ChunkSize - PageSize);
                        // Don't write to the old page when a new chunk is pushed.
                        ::VirtualProtect((LPVOID)page, PageSize, PAGE_NOACCESS, NULL);
                    }
                }
            }
#endif

            return item;
        }

        MarkStack() throw();
        ~MarkStack() throw();

        void Clear() throw();

        bool Empty() throw()
        {
            return (stack == start && chunkTail == preAllocatedChunk);
        }

        __forceinline MarkCandidate* Pop() throw()
        {
            // TODO: Can we make this faster?
            // One option (at the expense of wasting another 4K/Chunk) is to use a guard page at the start of the chunk too
            // Then we can eliminate this bound-check
            // Currently, the fast case takes 5 instructions:
            // 2 to load stack and start
            // 1 to check if they're not equal
            // 1 to subtract the stack
            // 1 to reload it back into the register
            if (stack != start)
            {
                stack = stack - sizeof(MarkCandidate);
#if MARKSTACK_DIAG
                count--;
#endif
                return (MarkCandidate*)stack;
            }

            return SlowPop();
        }

#if MARKSTACK_DIAG
        size_t Count() { return count; }
#endif

        static int HandleException(LPEXCEPTION_POINTERS pEP, Recycler* recycler) throw();

#ifndef MARKSTACK_TEST
    private:
#endif

        static int HandleExceptionInternal(LPEXCEPTION_POINTERS pEP, MarkStack<ChunkSize, PageSize, CommitSize>* markStack) throw();
        char* start;
        char* stack;
        char* end;
        Chunk* preAllocatedChunk;
        Chunk* chunkTail;

        // We copy the stack contents here if we pop the chunk
        // and have to reset the guard page
        MarkCandidate cachedEnd;

#if MARKSTACK_DIAG || defined(F_JSETW)
        unsigned int chunkCount;
#endif

    private:
        bool GrowStack() throw();
        void FreeNonPreAllocatedChunks() throw();
        void CreatePreallocatedChunk() throw();
        void ResetPreAllocatedChunk() throw();
        __forceinline MarkCandidate* SlowPop() throw();

        // Chunk manipulation
        Chunk* ReserveMemoryForNewChunk() throw();
        void FreeChunk(Chunk* chunk) throw();
        char* CommitAndPushChunk(Chunk* chunk) throw();
        __forceinline Chunk* PopChunk() throw();
        char* CommitNextPage() throw();

        // Memory wrappers
        void* CommitMemory(void* address, DWORD size, DWORD flags) throw();
        BOOL DecommitMemory(void* address, DWORD size) throw();
        void* ReserveMemory(DWORD size, DWORD flags) throw();
        BOOL ReleaseMemory(void* address, DWORD size) throw();

#if MARKSTACK_DIAG
        //
        // Debug structure to keep track of every call to VirtualAlloc with MEM_RESERVE
        //
        struct VirtualAllocationEntry
        {
            void* address;
            DWORD size;
        };

        typedef SList<VirtualAllocationEntry, HeapAllocator> VAEList;

        //
        // Find an allocation entry corresponding to the given address
        // This is fairly naive- we walk through the list of all entries and find one within
        // whose range this address lives
        //
        VAEList _allocations;
        VirtualAllocationEntry* CheckAllocationEntryExists(void* address)
        {
            VAEList::Iterator iterator(&this->_allocations);

            while (iterator.Next())
            {
                VirtualAllocationEntry& entry = iterator.Data();

                // Make sure we don't already have the new address isn't in range of existing allocations
                if (entry.address <= address && address < ((char*)entry.address) + entry.size)
                {
                    return (&entry);
                }
            }

            return nullptr;
        }

        size_t count;
        size_t committedBytes;
        size_t reservedBytes;
#endif
        void*  lastGuardPage;
        Chunk  staticChunk;
    };
};

typedef MarkStack::MarkStack<DEFAULT_CHUNK_SIZE, DEFAULT_PAGE_SIZE, DEFAULT_COMMIT_SIZE> RecyclerMarkStack;


