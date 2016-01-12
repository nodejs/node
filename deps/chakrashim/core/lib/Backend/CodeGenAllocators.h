//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if PDATA_ENABLED
#define ALLOC_XDATA (true)
#else
#define ALLOC_XDATA (false)
#endif

struct CodeGenAllocators
{
    // emitBufferManager depends on allocator which in turn depends on pageAllocator, make sure the sequence is right
    PageAllocator pageAllocator;
    NoRecoverMemoryArenaAllocator    allocator;
    EmitBufferManager<CriticalSection> emitBufferManager;
#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
    bool canCreatePreReservedSegment;
#endif

#ifdef PERF_COUNTERS
    size_t staticNativeCodeData;
#endif

    CodeGenAllocators(AllocationPolicyManager * policyManager, Js::ScriptContext * scriptContext);
    PageAllocator *GetPageAllocator() { return &pageAllocator; };
    ~CodeGenAllocators();
};
