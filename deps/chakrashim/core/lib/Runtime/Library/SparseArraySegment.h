//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class SparseArraySegmentBase
    {
    public:
        static const uint32 MaxLength;

        uint32 left;
        uint32 length; //we use length instead of right so that we can denote a segment is empty
        uint32 size;
        SparseArraySegmentBase* next;

        static const uint32 CHUNK_SIZE = 16;
        static const uint32 HEAD_CHUNK_SIZE = 16;
        static const uint32 INLINE_CHUNK_SIZE = 64; // Max number of elements in a segment that is initialized inline within the array.
        static const uint32 SMALL_CHUNK_SIZE = 4;
        static const uint32 BigLeft = 1 << 20;

        SparseArraySegmentBase(uint32 left, uint32 length, uint32 size);

        bool    HasIndex(uint32 index) { return (left <= index) && index < (left + length); };

        uint32  RemoveUndefined(ScriptContext* scriptContext); //returns count of undefined removed
        void    EnsureSizeInBound();

        static uint32 GetOffsetOfLeft() { return offsetof(SparseArraySegmentBase, left); }
        static uint32 GetOffsetOfLength() { return offsetof(SparseArraySegmentBase, length); }
        static uint32 GetOffsetOfSize() { return offsetof(SparseArraySegmentBase, size); }
        static uint32 GetOffsetOfNext() { return offsetof(SparseArraySegmentBase, next); }

        static bool DoNativeArrayLeafSegment() { return !PHASE_OFF1(Js::NativeArrayLeafSegmentPhase); }
        static bool IsLeafSegment(SparseArraySegmentBase *seg, Recycler *recycler);

    protected:
        static void EnsureSizeInBound(uint32 left, uint32 length, uint32& size, SparseArraySegmentBase* next);
    };

    template<typename T>
    class SparseArraySegment : public SparseArraySegmentBase
    {
    public:
        SparseArraySegment(uint32 left, uint32 length, uint32 size) :
            SparseArraySegmentBase(left, length, size) {}

        T elements[]; // actual elements will follow this determined by size

        inline void FillSegmentBuffer(uint start, uint size);
        inline T GetElement(uint32 index);
        inline void SetElement(Recycler *recycler, uint32 index, T value); // sets elements within the segment
        inline void RemoveElement(Recycler *recycler, uint32 index); // NOTE: RemoveElement calls memmove, for perf reasons use SetElement(index, null)

        inline SparseArraySegment<T> *GrowBy(Recycler *recycler, uint32 n);

        inline SparseArraySegment<T>* GrowByMin(Recycler *recycler, uint32 minValue);
        inline SparseArraySegment<T>* GrowByMinMax(Recycler *recycler, uint32 minValue, uint32 maxValue);
        inline SparseArraySegment<T>* GrowFrontByMax(Recycler *recycler, uint32 n);

        inline void ReverseSegment(Recycler *recycler);
        void    Truncate(uint32 index);

        //following will change the current segment allocation
        inline SparseArraySegment<T> *SetElementGrow(Recycler *recycler, SparseArraySegmentBase* prev, uint32 index, T value);

        static SparseArraySegment<T> *AllocateLiteralHeadSegment(Recycler *const recycler, const uint32 length);
        static inline SparseArraySegment<T> * AllocateSegment(Recycler* recycler, uint32 left, uint32 length, SparseArraySegmentBase *nextSeg);
        static inline SparseArraySegment<T> * AllocateSegment(Recycler* recycler, uint32 left, uint32 length, uint32 size, SparseArraySegmentBase *nextSeg);
        static inline SparseArraySegment<T> * AllocateSegment(Recycler* recycler, SparseArraySegmentBase* prev, uint32 index);
        template<bool isLeaf>
        static inline SparseArraySegment<T> * AllocateSegmentImpl(Recycler* recycler, uint32 left, uint32 length, SparseArraySegmentBase *nextSeg);

        template<bool isLeaf>
        static inline SparseArraySegment<T> * AllocateSegmentImpl(Recycler* recycler, uint32 left, uint32 length, uint32 size, SparseArraySegmentBase *nextSeg);

        template<bool isLeaf>
        static inline SparseArraySegment<T> * AllocateSegmentImpl(Recycler* recycler, SparseArraySegmentBase* prev, uint32 index);

        template<bool isLeaf>
        static SparseArraySegment<T> *AllocateLiteralHeadSegmentImpl(Recycler *const recycler, const uint32 length);

        static void ClearElements(__out_ecount(len) T* elements, uint32 len);
        static inline SparseArraySegment<T>* CopySegment(Recycler *recycler, SparseArraySegment<T>* dst, uint32 dstIndex, SparseArraySegment<T>* src, uint32 srcIndex, uint32 inputLen);

        static inline T GetMissingItem();
        static inline bool IsMissingItem(const T* value);

        static inline uint32 GetAlignedSize(uint32 size);

    private:
        template<bool isLeaf>
        static inline SparseArraySegment<T>* Allocate(Recycler* recycler, uint32 left, uint32 length, uint32 size, uint32 fillStart = 0);

        template<bool isLeaf>
        inline SparseArraySegment<T> *GrowByImpl(Recycler *recycler, uint32 n);

        template<bool isLeaf>
        inline SparseArraySegment<T>* GrowFrontByMaxImpl(Recycler *recycler, uint32 n);

        inline uint32 GetGrowByFactor();
    };

    template<typename T>
    T SparseArraySegment<T>::GetMissingItem()
    {
        return (T)JavascriptArray::MissingItem;
    }

    template<>
    int32 SparseArraySegment<int32>::GetMissingItem()
    {
        return 0x80000002;
    }

    template<>
    double SparseArraySegment<double>::GetMissingItem()
    {
        uint64 u = 0x8000000280000002;
        return *(double*)&u;
    }

    template<>
    bool SparseArraySegment<double>::IsMissingItem(const double* value)
    {
        return *(uint64*)value == 0x8000000280000002ull;
    }

    template<typename T>
    bool SparseArraySegment<T>::IsMissingItem(const T* value)
    {
        return *value == SparseArraySegment<T>::GetMissingItem();
    }

} // namespace Js
