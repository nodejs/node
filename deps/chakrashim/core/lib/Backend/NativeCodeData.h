//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#define NativeCodeDataNew(alloc, T, ...) AllocatorNew(NativeCodeData::Allocator, alloc, T, __VA_ARGS__)
#define NativeCodeDataNewZ(alloc, T, ...) AllocatorNewZ(NativeCodeData::Allocator, alloc, T, __VA_ARGS__)
#define NativeCodeDataNewArray(alloc, T, count) AllocatorNewArray(NativeCodeData::Allocator, alloc, T, count)
#define NativeCodeDataNewArrayZ(alloc, T, count) AllocatorNewArrayZ(NativeCodeData::Allocator, alloc, T, count)

struct CodeGenAllocators;

class NativeCodeData
{

private:
    struct DataChunk
    {
        DataChunk * next;
        char data[0];
    };
    NativeCodeData(DataChunk * chunkList);
    DataChunk * chunkList;

#ifdef PERF_COUNTERS
    size_t size;
#endif
    static void DeleteChunkList(DataChunk * chunkList);
public:
    class Allocator
    {
    public:
        static const bool FakeZeroLengthArray = false;

        Allocator();
        ~Allocator();

        char * Alloc(size_t requestedBytes);
        char * AllocZero(size_t requestedBytes);
        NativeCodeData * Finalize();
        void Free(void * buffer, size_t byteSize);

#ifdef TRACK_ALLOC
        // Doesn't support tracking information, dummy implementation
        Allocator * TrackAllocInfo(TrackAllocData const& data) { return this; }
        void ClearTrackAllocInfo(TrackAllocData* data = NULL) {}
#endif
    private:
        DataChunk * chunkList;
#if DBG
        bool finalized;
#endif
#ifdef PERF_COUNTERS
        size_t size;
#endif
    };

    ~NativeCodeData();

};


