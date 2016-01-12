//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

NativeCodeData::NativeCodeData(DataChunk * chunkList) : chunkList(chunkList)
{
#ifdef PERF_COUNTERS
    this->size = 0;
#endif
}

NativeCodeData::~NativeCodeData()
{
    NativeCodeData::DeleteChunkList(this->chunkList);
    PERF_COUNTER_SUB(Code, DynamicNativeCodeDataSize, this->size);
    PERF_COUNTER_SUB(Code, TotalNativeCodeDataSize, this->size);
}

void
NativeCodeData::DeleteChunkList(DataChunk * chunkList)
{
    DataChunk * next = chunkList;
    while (next != nullptr)
    {
        DataChunk * current = next;
        next = next->next;
        delete current;
    }
}

NativeCodeData::Allocator::Allocator() : chunkList(nullptr)
{
#if DBG
    this->finalized = false;
#endif
#ifdef PERF_COUNTERS
    this->size = 0;
#endif
}

NativeCodeData::Allocator::~Allocator()
{
    Assert(!finalized || this->chunkList == nullptr);
    NativeCodeData::DeleteChunkList(this->chunkList);
    PERF_COUNTER_SUB(Code, DynamicNativeCodeDataSize, this->size);
    PERF_COUNTER_SUB(Code, TotalNativeCodeDataSize, this->size);
}

char *
NativeCodeData::Allocator::Alloc(size_t requestSize)
{
    char * data = nullptr;
    Assert(!finalized);
    DataChunk * newChunk = HeapNewStructPlus(requestSize, DataChunk);
    newChunk->next = this->chunkList;
    this->chunkList = newChunk;
    data = newChunk->data;

#ifdef PERF_COUNTERS
    this->size += requestSize;
    PERF_COUNTER_ADD(Code, DynamicNativeCodeDataSize, requestSize);
#endif

    PERF_COUNTER_ADD(Code, TotalNativeCodeDataSize, requestSize);
    return data;
}

char *
NativeCodeData::Allocator::AllocZero(size_t requestSize)
{
    char * data = Alloc(requestSize);
    memset(data, 0, requestSize);
    return data;
}

NativeCodeData *
NativeCodeData::Allocator::Finalize()
{
    NativeCodeData * data = nullptr;
    if (this->chunkList != nullptr)
    {
        data = HeapNew(NativeCodeData, this->chunkList);
        this->chunkList = nullptr;
#ifdef PERF_COUNTERS
        data->size = this->size;
        this->size = 0;
#endif
    }
#if DBG
    this->finalized = true;
#endif
    return data;
}

//////////////////////////////////////////////////////////////////////////
//NativeCodeData::Allocator::Free
//This function should not be called at all because the life time is active during the run time
//This function is added to enable Dictionary(has calls to Free() Method - which will never be called as it will be
//allocated as a NativeAllocator to be allocated with NativeAllocator)
//////////////////////////////////////////////////////////////////////////
void
NativeCodeData::Allocator::Free(void * buffer, size_t byteSize)
{
}
