//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template<typename T>
    uint32 SparseArraySegment<T>::GetAlignedSize(uint32 size)
    {
        return (uint32)(HeapInfo::GetAlignedSize(UInt32Math::MulAdd<sizeof(T), sizeof(SparseArraySegmentBase)>(size)) - sizeof(SparseArraySegmentBase)) / sizeof(T);
    }

    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T> * SparseArraySegment<T>::Allocate(Recycler* recycler, uint32 left, uint32 length, uint32 size, uint32 fillStart /*= 0*/)
    {
        Assert(length <= size);
        Assert(size <= JavascriptArray::MaxArrayLength - left);

        uint32 bufferSize = UInt32Math::Mul<sizeof(T)>(size);
        SparseArraySegment<T> *seg =
            isLeaf ?
            RecyclerNewPlusLeafZ(recycler, bufferSize, SparseArraySegment<T>, left, length, size) :
            RecyclerNewPlusZ(recycler, bufferSize, SparseArraySegment<T>, left, length, size);
        seg->FillSegmentBuffer(fillStart, size);

        return seg;
    }

    template<>
    __inline SparseArraySegment<int> *SparseArraySegment<int>::AllocateLiteralHeadSegment(
        Recycler *const recycler,
        const uint32 length)
    {
        if (DoNativeArrayLeafSegment())
        {
            return SparseArraySegment<int>::AllocateLiteralHeadSegmentImpl<true>(recycler, length);
        }
        return SparseArraySegment<int>::AllocateLiteralHeadSegmentImpl<false>(recycler, length);
    }

    template<>
    __inline SparseArraySegment<double> *SparseArraySegment<double>::AllocateLiteralHeadSegment(
        Recycler *const recycler,
        const uint32 length)
    {
        if (DoNativeArrayLeafSegment())
        {
            return SparseArraySegment<double>::AllocateLiteralHeadSegmentImpl<true>(recycler, length);
        }
        return SparseArraySegment<double>::AllocateLiteralHeadSegmentImpl<false>(recycler, length);
    }

    template<typename T>
    __inline SparseArraySegment<T> *SparseArraySegment<T>::AllocateLiteralHeadSegment(
        Recycler *const recycler,
        const uint32 length)
    {
        return SparseArraySegment<T>::AllocateLiteralHeadSegmentImpl<false>(recycler, length);
    }

    template<typename T>
    template<bool isLeaf>
    __inline SparseArraySegment<T> *SparseArraySegment<T>::AllocateLiteralHeadSegmentImpl(
        Recycler *const recycler,
        const uint32 length)
    {
        Assert(length != 0);
        const uint32 size = GetAlignedSize(length);
        return SparseArraySegment<T>::Allocate<isLeaf>(recycler, 0, length, size, length);
    }

    template<>
    SparseArraySegment<int> * SparseArraySegment<int>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, SparseArraySegmentBase *nextSeg)
    {
        if (DoNativeArrayLeafSegment() && nextSeg == nullptr)
        {
            return AllocateSegmentImpl<true>(recycler, left, length, nextSeg);
        }
        return AllocateSegmentImpl<false>(recycler, left, length, nextSeg);
    }

    template<>
    SparseArraySegment<double> * SparseArraySegment<double>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, SparseArraySegmentBase *nextSeg)
    {
        if (DoNativeArrayLeafSegment() && nextSeg == nullptr)
        {
            return AllocateSegmentImpl<true>(recycler, left, length, nextSeg);
        }
        return AllocateSegmentImpl<false>(recycler, left, length, nextSeg);
    }

    template<typename T>
    SparseArraySegment<T> * SparseArraySegment<T>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, SparseArraySegmentBase *nextSeg)
    {
        return AllocateSegmentImpl<false>(recycler, left, length, nextSeg);
    }

    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T> * SparseArraySegment<T>::AllocateSegmentImpl(Recycler* recycler, uint32 left, uint32 length, SparseArraySegmentBase *nextSeg)
    {
        Assert(!isLeaf || nextSeg == nullptr);

        uint32 size;
        if ((length <= CHUNK_SIZE) && (left < BigLeft))
        {
            size = GetAlignedSize(CHUNK_SIZE);
        }
        else
        {
            size = GetAlignedSize(length);
        }

        //But don't overshoot next segment
        EnsureSizeInBound(left, length, size, nextSeg);

        return SparseArraySegment<T>::Allocate<isLeaf>(recycler, left, length, size);
    }

    template<>
    SparseArraySegment<int> * SparseArraySegment<int>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, uint32 size, SparseArraySegmentBase *nextSeg)
    {
        if (DoNativeArrayLeafSegment() && nextSeg == nullptr)
        {
            return AllocateSegmentImpl<true>(recycler, left, length, size, nextSeg);
        }
        return AllocateSegmentImpl<false>(recycler, left, length, size, nextSeg);
    }

    template<>
    SparseArraySegment<double> * SparseArraySegment<double>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, uint32 size, SparseArraySegmentBase *nextSeg)
    {
        if (DoNativeArrayLeafSegment() && nextSeg == nullptr)
        {
            return AllocateSegmentImpl<true>(recycler, left, length, size, nextSeg);
        }
        return AllocateSegmentImpl<false>(recycler, left, length, size, nextSeg);
    }

    template<typename T>
    SparseArraySegment<T> * SparseArraySegment<T>::AllocateSegment(Recycler* recycler, uint32 left, uint32 length, uint32 size, SparseArraySegmentBase *nextSeg)
    {
        return AllocateSegmentImpl<false>(recycler, left, length, size, nextSeg);
    }

    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T> * SparseArraySegment<T>::AllocateSegmentImpl(Recycler* recycler, uint32 left, uint32 length, uint32 size, SparseArraySegmentBase *nextSeg)
    {
        Assert(!isLeaf || nextSeg == nullptr);

        AssertMsg(size > 0, "size too small");
        if ((size <= CHUNK_SIZE) && (size < BigLeft))
        {
            size = GetAlignedSize(CHUNK_SIZE);
        }
        else
        {
            size = GetAlignedSize(size);
        }

        //But don't overshoot next segment
        EnsureSizeInBound(left, length, size, nextSeg);

        return SparseArraySegment<T>::Allocate<isLeaf>(recycler, left, length, size);
    }

    template<>
    SparseArraySegment<int>* SparseArraySegment<int>::AllocateSegment(Recycler* recycler, SparseArraySegmentBase* prev, uint32 index)
    {
        if (DoNativeArrayLeafSegment() && prev->next == nullptr)
        {
            return AllocateSegmentImpl<true>(recycler, prev, index);
        }
        return AllocateSegmentImpl<false>(recycler, prev, index);
    }

    template<>
    SparseArraySegment<double>* SparseArraySegment<double>::AllocateSegment(Recycler* recycler, SparseArraySegmentBase* prev, uint32 index)
    {
        if (DoNativeArrayLeafSegment() && prev->next == nullptr)
        {
            return AllocateSegmentImpl<true>(recycler, prev, index);
        }
        return AllocateSegmentImpl<false>(recycler, prev, index);
    }

    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::AllocateSegment(Recycler* recycler, SparseArraySegmentBase* prev, uint32 index)
    {
        return AllocateSegmentImpl<false>(recycler, prev, index);
    }

    // Allocate a segment in between (prev, next) to contain an index
    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T>* SparseArraySegment<T>::AllocateSegmentImpl(Recycler* recycler, SparseArraySegmentBase* prev, uint32 index)
    {
        Assert(prev);
        Assert(index > prev->left && index - prev->left >= prev->size);

        SparseArraySegmentBase* next = prev->next;
        Assert(!next || index < next->left);
        Assert(!next || !isLeaf);

        uint32 left = index;
        uint32 size = (left < BigLeft ? GetAlignedSize(CHUNK_SIZE) : GetAlignedSize(1));

        // Try to move the segment leftwards if it overshoots next segment
        if (next && size > next->left - left)
        {
            size = min(size, next->left - (prev->left + prev->size));
            left = next->left - size;
        }

        Assert(index >= left && index - left < size);
        uint32 length = index - left + 1;

        EnsureSizeInBound(left, length, size, next);
        return SparseArraySegment<T>::Allocate<isLeaf>(recycler, left, length, size);
    }

    template<typename T>
    void SparseArraySegment<T>::FillSegmentBuffer(uint32 start, uint32 size)
    {
        // Fill the segment buffer using gp-register-sized stores. Avoid using the FPU for the sake
        // of perf (especially x86).
        Var fill = JavascriptArray::MissingItem;
        if (sizeof(Var) > sizeof(T))
        {
            // Pointer size is greater than the element (int32 buffer on x64).
            // Fill as much as we can and do one int32-sized store at the end if necessary.
            uint32 i, step = sizeof(Var) / sizeof(T);
            if (start & 1)
            {
                Assert(sizeof(T) == sizeof(int32));
                ((int32*)(this->elements))[start] = JavascriptNativeIntArray::MissingItem;
            }
            for (i = (start + step-1)/step; i < (size/step); i++)
            {
                ((Var*)(this->elements))[i] = fill;
            }
            if ((i *= step) < size)
            {
                Assert(sizeof(T) == sizeof(int32));
                ((int32*)(this->elements))[i] = JavascriptNativeIntArray::MissingItem;
            }
        }
        else
        {
            // Pointer size <= element size. Fill with pointer-sized stores.
            Assert(sizeof(T) % sizeof(Var) == 0);
            uint step = sizeof(T) / sizeof(Var);

            for (uint i = start; i < size * step; i++)
            {
                ((Var*)(this->elements))[i] = fill;
            }
        }
    }

    template<typename T>
    void SparseArraySegment<T>::SetElement(Recycler *recycler, uint32 index, T value)
    {
        AssertMsg(index >= left && index - left < size, "Index out of range");
        uint32 offset = index - left;

        elements[offset] = value;

        if ((offset + 1) > length)
        {
            length =  offset + 1;
        }
        Assert(length <= size);
        Assert(left + length > left);
    }

    template<typename T>
    SparseArraySegment<T> *SparseArraySegment<T>::SetElementGrow(Recycler *recycler, SparseArraySegmentBase* prev, uint32 index, T value)
    {
        AssertMsg((index + 1) == left || index == (left + size), "Index out of range");

        uint32 offset = index - left;
        SparseArraySegment<T> *current = this;

        if (index + 1 == left)
        {
            Assert(prev && prev->next == current);
            Assert(left > prev->left && left - prev->left > prev->size);
            Assert(left - prev->left - prev->size > 1); // Otherwise we would be growing/merging prev

            // Grow front up to (prev->left + prev->size + 1), so that setting (prev->left + prev->size)
            // later would trigger segment merging.
            current = GrowFrontByMax(recycler, left - prev->left - prev->size - 1);
            current->SetElement(recycler, index, value);
        }
        else if (offset == size)
        {
            if (next == nullptr)
            {
                current = GrowByMin(recycler, offset + 1 - size);
            }
            else
            {
                current = GrowByMinMax(recycler, offset + 1 - size, next->left - left - size);
            }
            current->elements[offset] = value;
            current->length =  offset + 1;
        }
        else
        {
            AssertMsg(false, "Invalid call to SetElementGrow");
        }

        return current;
    }

    template<typename T>
    T SparseArraySegment<T>::GetElement(uint32 index)
    {
        AssertMsg(index >= left && index <= left + length - 1, "Index is out of the segment range");
        return elements[index - left];
    }

    // This is a very inefficient function, we have to move element
    template<typename T>
    void SparseArraySegment<T>::RemoveElement(Recycler *recycler, uint32 index)
    {
        AssertMsg(index >= left && index < left + length, "Index is out of the segment range");
        if (index + 1 < left + length)
        {
            memmove(elements + index - left, elements + index + 1 - left, sizeof(T) * (length - (index - left) - 1));
        }
        Assert(length);
        length--;
        elements[length] = SparseArraySegment<T>::GetMissingItem();
    }


    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::CopySegment(Recycler *recycler, SparseArraySegment<T>* dst, uint32 dstIndex, SparseArraySegment<T>* src, uint32 srcIndex, uint32 inputLen)
    {
        AssertMsg(src != nullptr && dst != nullptr, "Null input!");

        uint32 newLen = dstIndex - dst->left + inputLen;
        if (newLen > dst->size)
        {
            dst = dst->GrowBy(recycler, newLen - dst->size);
        }
        dst->length = newLen;
        Assert(dst->length <= dst->size);
        AssertMsg(srcIndex - src->left >= 0,"src->left > srcIndex resulting in negative indexing of src->elements");
        js_memcpy_s(dst->elements + dstIndex - dst->left, sizeof(T) * inputLen, src->elements + srcIndex - src->left, sizeof(T) * inputLen);
        return dst;
    }

    template<typename T>
    uint32 SparseArraySegment<T>::GetGrowByFactor()
    {
        if (size < CHUNK_SIZE/2)
        {
            return (GetAlignedSize(size * 4) - size);
        }
        else if (size < 1024)
        {
            return (GetAlignedSize(size * 2) - size);
        }
        return (GetAlignedSize(UInt32Math::Mul(size, 5) / 3) - size);
    }

    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowByMin(Recycler *recycler, uint32 minValue)
    {
        Assert(size <= JavascriptArray::MaxArrayLength - left);

        uint32 maxGrow = JavascriptArray::MaxArrayLength - (left + size);
        return GrowByMinMax(recycler, minValue, maxGrow);
    }

    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowByMinMax(Recycler *recycler, uint32 minValue, uint32 maxValue)
    {
        Assert(size <= JavascriptArray::MaxArrayLength - left);
        Assert(maxValue <= JavascriptArray::MaxArrayLength - (left + size));
        AssertMsg(minValue <= maxValue, "Invalid values to GrowByMinMax");

        return GrowBy(recycler, max(minValue,min(maxValue, GetGrowByFactor())));
    }

    template<>
    SparseArraySegment<int>* SparseArraySegment<int>::GrowBy(Recycler *recycler, uint32 n)
    {
        if (!DoNativeArrayLeafSegment() || this->next != nullptr)
        {
            return GrowByImpl<false>(recycler, n);
        }
        return GrowByImpl<true>(recycler, n);
    }

    template<>
    SparseArraySegment<double>* SparseArraySegment<double>::GrowBy(Recycler *recycler, uint32 n)
    {
        if (!DoNativeArrayLeafSegment() || this->next != nullptr)
        {
            return GrowByImpl<false>(recycler, n);
        }
        return GrowByImpl<true>(recycler, n);
    }

    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowBy(Recycler *recycler, uint32 n)
    {
        return GrowByImpl<false>(recycler, n);
    }

    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowByImpl(Recycler *recycler, uint32 n)
    {
        Assert(length <= size);
        Assert(n != 0);

        uint32 newSize = size + n;
        if (newSize <= size)
        {
            Throw::OutOfMemory();
        }

        SparseArraySegment<T> *newSeg = Allocate<isLeaf>(recycler, left, length, newSize);
        newSeg->next = this->next;
        // (sizeof(T) * newSize) will throw OOM in Allocate if it overflows.
        js_memcpy_s(newSeg->elements, sizeof(T) * newSize, this->elements, sizeof(T) * length);

        return newSeg;
    }

    //
    // Grows segment in the front. Note that the result new segment's left is changed.
    //
    template<>
    SparseArraySegment<int>* SparseArraySegment<int>::GrowFrontByMax(Recycler *recycler, uint32 n)
    {
        if (DoNativeArrayLeafSegment() && this->next == nullptr)
        {
            return GrowFrontByMaxImpl<true>(recycler, n);
        }
        return GrowFrontByMaxImpl<false>(recycler, n);
    }

    template<>
    SparseArraySegment<double>* SparseArraySegment<double>::GrowFrontByMax(Recycler *recycler, uint32 n)
    {
        if (DoNativeArrayLeafSegment() && this->next == nullptr)
        {
            return GrowFrontByMaxImpl<true>(recycler, n);
        }
        return GrowFrontByMaxImpl<false>(recycler, n);
    }

    template<typename T>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowFrontByMax(Recycler *recycler, uint32 n)
    {
        return GrowFrontByMaxImpl<false>(recycler, n);
    }

    template<typename T>
    template<bool isLeaf>
    SparseArraySegment<T>* SparseArraySegment<T>::GrowFrontByMaxImpl(Recycler *recycler, uint32 n)
    {
        Assert(length <= size);
        Assert(n > 0);
        Assert(n <= left);
        Assert(size + n > size);

        n = min(n, GetGrowByFactor());

        if (size + n <= size)
        {
            Throw::OutOfMemory();
        }

        SparseArraySegment<T> *newSeg = Allocate<isLeaf>(recycler, left - n, length + n, size + n);
        newSeg->next = this->next;
        js_memcpy_s(&newSeg->elements[n], sizeof(T) * length, this->elements, sizeof(T) * length);

        return newSeg;
    }

    template<typename T>
    void SparseArraySegment<T>::ClearElements(__out_ecount(len) T* elements, uint32 len)
    {
        T fill = SparseArraySegment<T>::GetMissingItem();
        for (uint i = 0; i < len; i++)
        {
            elements[i] = fill;
        }
    }

    template<typename T>
    void SparseArraySegment<T>::Truncate(uint32 index)
    {
        AssertMsg(index >= left && (index - left) < size, "Index out of range");

        ClearElements(elements + (index - left), size - (index - left));
        if (index - left < length)
        {
            length = index - left;
        }
        Assert(length <= size);
    }


    template<typename T>
    void SparseArraySegment<T>::ReverseSegment(Recycler *recycler)
    {
        if (length <= 1)
        {
            return;
        }

        T temp;
        uint32 lower = 0;
        uint32 upper = length - 1;
        while (lower < upper)
        {
            temp = elements[lower];
            elements[lower] = elements[upper];
            elements[upper] = temp;
            ++lower;
            --upper;
        }
    }

}
