//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// Allows an external memory tracking utility (such as MemSpect) to track recycler allocations

// Routines called to allow tracking memory. These routines are empty for RET
// builds but allow external tools (such as Memspect) to track arena allocations individually
// for CHK and TEST builds. Requires the appropriate configuration flag, e.g. -MemspectEnabled.

namespace Memory
{
class ArenaAllocator;

class ArenaMemoryTracking
{
public:
    static void Activate();
    static void ArenaCreated(Allocator *arena,  __in LPCWSTR name);
    static void ArenaDestroyed(Allocator *arena);
    static void ReportAllocation(Allocator *arena, void *address, size_t size);
    static void ReportReallocation(Allocator *arena, void *address, size_t existingSize, size_t newSize);
    static void ReportFree(Allocator *arena, void *address, size_t size);
    static void ReportFreeAll(Allocator *arena);
};

class Recycler;

class RecyclerMemoryTracking
{
public:
    static bool IsActive();
    static void Activate();
    static void ReportRecyclerCreate(Recycler * recycler);
    static void ReportRecyclerDestroy(Recycler * recycler);
    static void ReportAllocation(Recycler * recycler, __in void *address, size_t size);
    static void ReportFree(Recycler * recycler, __in void *address, size_t size);
    static void ReportUnallocated(Recycler * recycler, __in void* address, __in void *endAddress, size_t sizeCat);
};

class PageTracking
{
public:
    static void Activate();
    static void PageAllocatorCreated(PageAllocator *pageAllocator);
    static void PageAllocatorDestroyed(PageAllocator *pageAllocator);
    static void ReportAllocation(PageAllocator *pageAllocator, __in void *address, size_t size);
    static void ReportFree(PageAllocator *pageAllocator, __in void *address, size_t size);
};
}
