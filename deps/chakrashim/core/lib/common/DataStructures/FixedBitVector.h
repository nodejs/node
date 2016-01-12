//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#define FOREACH_BITSET_IN_FIXEDBV(index, bv) \
{ \
    BVIndex index; \
    for(JsUtil::FBVEnumerator _bvenum = bv->BeginSetBits(); \
        !_bvenum.End(); \
        _bvenum++) \
    { \
        index = _bvenum.GetCurrent(); \

#define NEXT_BITSET_IN_FIXEDBV              }}


class BVFixed
{
// Data
protected:
    BVIndex       len;
    BVUnit      data[];

private:
    BVFixed(BVFixed * initBv);
    BVFixed(BVIndex length, bool initialSet = false);
    void ClearEnd();

// Creation Factory
public:

    template <typename TAllocator>
    static  BVFixed *       New(TAllocator* alloc, BVFixed * initBv);

    template <typename TAllocator>
    static  BVFixed *       New(BVIndex length, TAllocator* alloc, bool initialSet = false);

    template <typename TAllocator>
    static  BVFixed *       NewNoThrow(BVIndex length, TAllocator* alloc, bool initialSet = false);

    template <typename TAllocator>
    void                    Delete(TAllocator * alloc);

    // For preallocated memory
    static size_t           GetAllocSize(BVIndex length);
    void Init(BVIndex length);

// Implementation
protected:
            void            AssertRange(BVIndex i) const;
            void            AssertBV(const BVFixed * bv) const;

    static  BVIndex         WordCount(BVIndex length);

    const   BVUnit *        BitsFromIndex(BVIndex i) const;
            BVUnit *        BitsFromIndex(BVIndex i);
    const   BVUnit *        BeginUnit() const;
            BVUnit *        BeginUnit();
    const   BVUnit *        EndUnit() const;
            BVUnit *        EndUnit();


    template<class Fn>
    __inline void for_each(const BVFixed *bv2, const Fn callback)
    {
        AssertMsg(this->len == bv2->len, "Fatal: The 2 bitvectors should have had the same length.");

        BVUnit *        i;
        const BVUnit *  j;

        for(i  =  this->BeginUnit(), j = bv2->BeginUnit();
            i !=  this->EndUnit() ;
            i++, j++)
        {
            (i->*callback)(*j);
        }
    }

// Methods
public:

    void Set(BVIndex i)
    {
        AssertRange(i);
        this->BitsFromIndex(i)->Set(BVUnit::Offset(i));
    }

    void Clear(BVIndex i)
    {
        AssertRange(i);
        this->BitsFromIndex(i)->Clear(BVUnit::Offset(i));
    }

    void Compliment(BVIndex i)
    {
        AssertRange(i);
        this->BitsFromIndex(i)->Complement(BVUnit::Offset(i));
    }

    BOOLEAN Test(BVIndex i) const
    {
        AssertRange(i);
        return this->BitsFromIndex(i)->Test(BVUnit::Offset(i));
    }

    BOOLEAN         operator[](BVIndex i) const;

    BVIndex         GetNextBit(BVIndex i) const;

    BOOLEAN         TestAndSet(BVIndex i);
    BOOLEAN         TestAndClear(BVIndex i);

    void            OrComplimented(const BVFixed * bv);
    void            Or(const BVFixed *bv);
    uint            DiffCount(const BVFixed* bv) const;
    void            And(const BVFixed *bv);
    void            Minus(const BVFixed *bv);
    void            Copy(const BVFixed *bv);
    void            CopyBits(const BVFixed * bv, BVIndex i);
    void            ComplimentAll();
    void            SetAll();
    void            ClearAll();

    BVIndex         Count() const;
    BVIndex         Length() const;
    JsUtil::FBVEnumerator BeginSetBits();

    BVIndex         WordCount() const;
    bool            IsAllClear() const;
    template<typename Container>
    Container GetRange(BVIndex start, BVIndex len) const;
    template<typename Container>
    void SetRange(Container* value, BVIndex start, BVIndex len);

    BVUnit* GetData() const
    {
        return (BVUnit*)data;
    }
#if DBG_DUMP
    void            Dump() const;
#endif
};

template <typename TAllocator>
BVFixed * BVFixed::New(TAllocator * alloc, BVFixed * initBv)
{
    BVIndex length = initBv->Length();
    BVFixed *result = AllocatorNewPlus(TAllocator, alloc, sizeof(BVUnit) * BVFixed::WordCount(length), BVFixed, initBv);
    return result;
}

template <typename TAllocator>
BVFixed * BVFixed::New(BVIndex length, TAllocator * alloc, bool initialSet)
{
    BVFixed *result = AllocatorNewPlus(TAllocator, alloc, sizeof(BVUnit) * BVFixed::WordCount(length), BVFixed, length, initialSet);
    return result;
}

template <typename TAllocator>
BVFixed * BVFixed::NewNoThrow(BVIndex length, TAllocator * alloc, bool initialSet)
{
    BVFixed *result = AllocatorNewNoThrowPlus(TAllocator, alloc, sizeof(BVUnit) * BVFixed::WordCount(length), BVFixed, length, initialSet);
    return result;
}

template <typename TAllocator>
void BVFixed::Delete(TAllocator * alloc)
{
    AllocatorDeletePlus(TAllocator, alloc, sizeof(BVUnit) * this->WordCount(), this);
}

template<typename Container>
Container BVFixed::GetRange(BVIndex start, BVIndex len) const
{
    AssertRange(start);
    if (len == 0)
    {
        return Container(0);
    }
    Assert(len <= sizeof(Container) * MachBits);
    AssertMsg(len <= 64, "Currently doesn't support range bigger than 64 bits");
    BVIndex end = start + len - 1;
    AssertRange(end);
    BVIndex iStart = BVUnit::Position(start);
    BVIndex iEnd = BVUnit::Position(end);
    BVIndex oStart = BVUnit::Offset(start);
    BVIndex oEnd = BVUnit::Offset(end);
    // Simply using uint64 because it is much easier than to juggle with BVUnit::BVUnitTContainer's size
    // Special case, if oEnd == 63, 1 << 64 == 1. Therefore the result is incorrect
    uint64 mask = oEnd < 63 ? (((uint64)1 << (oEnd + 1)) - 1) : 0xFFFFFFFFFFFFFFFF;
    uint64 range;
    // Trivial case
    if (iStart == iEnd)
    {
        // remove the bits after oEnd with mask, then remove the bits before start with shift
        range = (mask & this->data[iStart].GetWord()) >> oStart;
    }
    // Still simple enough
    else if (iStart + 1 == iEnd)
    {
        auto startWord = this->data[iStart].GetWord();
        auto endWord = this->data[iEnd].GetWord();
        // remove the bits before start with shift
        range = startWord >> oStart;
        // remove the bits after oEnd with mask then position it after start bits
        range |= (mask & endWord) << (BVUnit::BitsPerWord - oStart);
    }
    // Spans over multiple value, need to loop
    else
    {
        // Get the first bits and move them to the beginning
        range = this->data[iStart].GetWord() >> oStart;
        // track how many bits have been read so far
        int nBitsUsed = BVUnit::BitsPerWord - oStart;
        for (uint i = iStart + 1; i < iEnd; ++i)
        {
            // put all bits from the data in the mid-range. Use the tracked read bits to position them
            range |= ((uint64)(this->data[i].GetWord())) << nBitsUsed;
            nBitsUsed += BVUnit::BitsPerWord;
        }
        // Read the last bits and remove those after oEnd with mask
        range |= (mask & this->data[iEnd].GetWord()) << nBitsUsed;
    }
    return Container(range);
}

template<typename Container>
void BVFixed::SetRange(Container* value, BVIndex start, BVIndex len)
{
    AssertRange(start);
    if (len == 0)
    {
        return;
    }
    Assert(len <= sizeof(Container) * MachBits);
    BVIndex end = start + len - 1;
    AssertRange(end);
    BVIndex iStart = BVUnit::Position(start);
    BVIndex iEnd = BVUnit::Position(end);
    BVIndex oStart = BVUnit::Offset(start);
    BVIndex oEnd = BVUnit::Offset(end);
    BVUnit::BVUnitTContainer* bits = (BVUnit::BVUnitTContainer*)value;
    const int oStartComplement = BVUnit::BitsPerWord - oStart;
    static_assert((BVUnit::BVUnitTContainer)BVUnit::AllOnesMask > 0, "Container type of BVFixed must be unsigned");
    //When making the mask, check the special case when we need all bits
#define MAKE_MASK(start, end) ( ((end) == BVUnit::BitsPerWord ? BVUnit::AllOnesMask : (((BVUnit::BVUnitTContainer)1 << ((end) - (start))) - 1))   << (start))
    // Or the value to set the bits to 1. And the value to set the bits to 0
    // The mask is used to make sure we don't modify the bits outside the range
#define SET_RANGE(i, value, mask) \
    this->data[i].Or((value) & mask);\
    this->data[i].And((value) | ~mask);

    BVUnit::BVUnitTContainer bitsToSet;
    // Fast Path
    if (iEnd == iStart)
    {
        const BVUnit::BVUnitTContainer mask = MAKE_MASK(oStart, oEnd + 1);
        // Shift to position the bits
        bitsToSet = (*bits << oStart);
        SET_RANGE(iStart, bitsToSet, mask);
    }
    // TODO: case iEnd == iStart + 1 to avoid a loop
    else if (oStart == 0)
    {
        // Simpler case where we don't have to shift the bits around
        for (uint i = iStart; i < iEnd; ++i)
        {
            SET_RANGE(i, *bits, BVUnit::AllOnesMask);
            ++bits;
        }
        // We still need to use a mask to remove the unused bits
        const BVUnit::BVUnitTContainer mask = MAKE_MASK(0, oEnd + 1);
        SET_RANGE(iEnd, *bits, mask);
    }
    else
    {
        // Default case. We need to process everything 1 at a time
        {
            // First set the first bits
            const BVUnit::BVUnitTContainer mask = MAKE_MASK(oStart, BVUnit::BitsPerWord);
            SET_RANGE(iStart, *bits << oStart, mask);
        }
        // Set the bits in the middle
        for (uint i = iStart + 1; i < iEnd; ++i)
        {
            bitsToSet = *bits >> oStartComplement;
            ++bits;
            bitsToSet |= *bits << oStart;
            SET_RANGE(i, bitsToSet, BVUnit::AllOnesMask);
        }
        // Set the last bits
        bitsToSet = *bits >> oStartComplement;
        ++bits;
        bitsToSet |= *bits << oStart;
        {
            const BVUnit::BVUnitTContainer mask = MAKE_MASK(0, oEnd + 1);
            SET_RANGE(iEnd, bitsToSet, mask);
        }
    }
#undef MAKE_MASK
#undef SET_RANGE
}

template <size_t bitCount>
class BVStatic
{
public:
    // Made public to allow for compile-time use
    static const size_t wordCount = ((bitCount - 1) >> BVUnit::ShiftValue) + 1;

// Data
private:
    BVUnit data[wordCount];

public:
    // Break on member changes. We rely on the layout of this class being static so we can
    // use initializer lists to generate collections of BVStatic.
    BVStatic()
    {
        Assert(sizeof(BVStatic<bitCount>) == sizeof(data));
        Assert((void*)this == (void*)&this->data);
    }

// Implementation
private:
    void AssertRange(BVIndex i) const { Assert(i < bitCount); }

    const BVUnit * BitsFromIndex(BVIndex i) const { AssertRange(i); return &this->data[BVUnit::Position(i)]; }
    BVUnit * BitsFromIndex(BVIndex i) { AssertRange(i); return &this->data[BVUnit::Position(i)]; }

    const BVUnit * BeginUnit() const { return &this->data[0]; }
    BVUnit * BeginUnit() { return &this->data[0]; }

    const BVUnit * EndUnit() const { return &this->data[wordCount]; }
    BVUnit * EndUnit() { return &this->data[wordCount]; }

    template<class Fn>
    __inline void for_each(const BVStatic *bv2, const Fn callback)
    {
        BVUnit *        i;
        const BVUnit *  j;

        for(i  =  this->BeginUnit(), j = bv2->BeginUnit();
            i !=  this->EndUnit() ;
            i++, j++)
        {
            (i->*callback)(*j);
        }
    }

    template<class Fn>
    static bool MapUntil(const BVStatic *bv1, const BVStatic *bv2, const Fn callback)
    {
        const BVUnit *  i;
        const BVUnit *  j;

        for(i  =  bv1->BeginUnit(), j = bv2->BeginUnit();
            i !=  bv1->EndUnit() ;
            i++, j++)
        {
            if (!callback(*i, *j))
            {
                return false;
            }
        }
        return true;
    }

    void ClearEnd()
    {
        uint offset = BVUnit::Offset(bitCount);
        if (offset != 0)
        {
            this->data[wordCount - 1].And((1 << offset) - 1);
        }
    }

// Methods
public:
    void Set(BVIndex i)
    {
        AssertRange(i);
        this->BitsFromIndex(i)->Set(BVUnit::Offset(i));
    }

    void Clear(BVIndex i)
    {
        AssertRange(i);
        this->BitsFromIndex(i)->Clear(BVUnit::Offset(i));
    }

    void Compliment(BVIndex i)
    {
        AssertRange(i);
        this->BitsFromIndex(i)->Complement(BVUnit::Offset(i));
    }

    BOOLEAN Equal(BVStatic<bitCount> const * o)
    {
        return MapUntil(this, o, [](BVUnit const& i, BVUnit const &j) { return i.Equal(j); });
    }

    BOOLEAN Test(BVIndex i) const
    {
        AssertRange(i);
        return this->BitsFromIndex(i)->Test(BVUnit::Offset(i));
    }

    BOOLEAN TestAndSet(BVIndex i)
    {
        AssertRange(i);
        return _bittestandset((long *)this->data, (long) i);
    }

    BOOLEAN TestIntrinsic(BVIndex i) const
    {
        AssertRange(i);
        return _bittest((long *)this->data, (long) i);
    }

    BOOLEAN TestAndSetInterlocked(BVIndex i)
    {
        AssertRange(i);
        return _interlockedbittestandset((long *)this->data, (long) i);
    }

    BOOLEAN TestAndClear(BVIndex i)
    {
        AssertRange(i);
        BVUnit * bvUnit = this->BitsFromIndex(i);
        BVIndex offset = BVUnit::Offset(i);
        BOOLEAN bit = bvUnit->Test(offset);
        bvUnit->Clear(offset);
        return bit;
    }

    void OrComplimented(const BVStatic * bv) { this->for_each(bv, &BVUnit::OrComplimented); ClearEnd(); }
    void Or(const BVStatic *bv) { this->for_each(bv, &BVUnit::Or); }
    void And(const BVStatic *bv) { this->for_each(bv, &BVUnit::And); }
    void Minus(const BVStatic *bv) { this->for_each(bv, &BVUnit::Minus); }

    void Copy(const BVStatic *bv) { js_memcpy_s(&this->data[0], wordCount * sizeof(BVUnit), &bv->data[0], wordCount * sizeof(BVUnit)); }

    void SetAll() { memset(&this->data[0], -1, wordCount * sizeof(BVUnit)); ClearEnd(); }
    void ClearAll() { memset(&this->data[0], 0, wordCount * sizeof(BVUnit)); }

    void ComplimentAll()
    {
        for (BVIndex i = 0; i < wordCount; i++)
        {
            this->data[i].ComplimentAll();
        }

        ClearEnd();
    }

    BVIndex Count() const
    {
        BVIndex sum = 0;
        for (BVIndex i = 0; i < wordCount; i++)
        {
            sum += this->data[i].Count();
        }

        Assert(sum <= bitCount);
        return sum;
    }

    BVIndex Length() const
    {
        return bitCount;
    }

    JsUtil::FBVEnumerator   BeginSetBits() { return JsUtil::FBVEnumerator(this->BeginUnit(), this->EndUnit()); }

    BVIndex GetNextBit(BVIndex i) const
    {
        AssertRange(i);

        const BVUnit * chunk = BitsFromIndex(i);
        BVIndex base = BVUnit::Floor(i);

        BVIndex offset = chunk->GetNextBit(BVUnit::Offset(i));
        if (-1 != offset)
        {
            return base + offset;
        }

        while (++chunk != this->EndUnit())
        {
            base += BVUnit::BitsPerWord;
            offset = chunk->GetNextBit();
            if (-1 != offset)
            {
                return base + offset;
            }
        }

       return BVInvalidIndex;
    }

    const BVUnit * GetRawData() const { return data; }

    template <size_t rangeSize>
    BVStatic<rangeSize> * GetRange(BVIndex startOffset)
    {
        AssertRange(startOffset);
        AssertRange(startOffset + rangeSize - 1);

        // Start offset and size must be word-aligned
        Assert(BVUnit::Offset(startOffset) == 0);
        Assert(BVUnit::Offset(rangeSize) == 0);

        return (BVStatic<rangeSize> *)BitsFromIndex(startOffset);
    }

    BOOLEAN TestRange(const BVIndex index, uint length) const
    {
        AssertRange(index);
        AssertRange(index + length - 1);

        const BVUnit * bvUnit = BitsFromIndex(index);
        uint offset = BVUnit::Offset(index);

        if (offset + length <= BVUnit::BitsPerWord)
        {
            // Bit range is in a single word
            return bvUnit->TestRange(offset, length);
        }

        // Bit range spans words.
        // Test the first word, from start offset to end of word
        if (!bvUnit->TestRange(offset, (BVUnit::BitsPerWord - offset)))
        {
            return FALSE;
        }

        bvUnit++;
        length -= (BVUnit::BitsPerWord - offset);

        // Test entire words until we are at the last word
        while (length >= BVUnit::BitsPerWord)
        {
            if (!bvUnit->IsFull())
            {
                return FALSE;
            }

            bvUnit++;
            length -= BVUnit::BitsPerWord;
        }

        // Test last word (unless we already ended on a word boundary)
        if (length > 0)
        {
            if (!bvUnit->TestRange(0, length))
            {
                return FALSE;
            }
        }

        return TRUE;
    }

    void SetRange(const BVIndex index, uint length)
    {
        AssertRange(index);
        AssertRange(index + length - 1);

        BVUnit * bvUnit = BitsFromIndex(index);
        uint offset = BVUnit::Offset(index);

        if (offset + length <= BVUnit::BitsPerWord)
        {
            // Bit range is in a single word
            return bvUnit->SetRange(offset, length);
        }

        // Bit range spans words.
        // Set the first word, from start offset to end of word
        bvUnit->SetRange(offset, (BVUnit::BitsPerWord - offset));

        bvUnit++;
        length -= (BVUnit::BitsPerWord - offset);

        // Set entire words until we are at the last word
        while (length >= BVUnit::BitsPerWord)
        {
            bvUnit->SetAll();

            bvUnit++;
            length -= BVUnit::BitsPerWord;
        }

        // Set last word (unless we already ended on a word boundary)
        if (length > 0)
        {
            bvUnit->SetRange(0, length);
        }
    }

    void ClearRange(const BVIndex index, uint length)
    {
        AssertRange(index);
        AssertRange(index + length - 1);

        BVUnit * bvUnit = BitsFromIndex(index);
        uint offset = BVUnit::Offset(index);

        if (offset + length <= BVUnit::BitsPerWord)
        {
            // Bit range is in a single word
            return bvUnit->ClearRange(offset, length);
        }

        // Bit range spans words.
        // Clear the first word, from start offset to end of word
        bvUnit->ClearRange(offset, (BVUnit::BitsPerWord - offset));

        bvUnit++;
        length -= (BVUnit::BitsPerWord - offset);

        // Set entire words until we are at the last word
        while (length >= BVUnit::BitsPerWord)
        {
            bvUnit->ClearAll();

            bvUnit++;
            length -= BVUnit::BitsPerWord;
        }

        // Set last word (unless we already ended on a word boundary)
        if (length > 0)
        {
            bvUnit->ClearRange(0, length);
        }
    }

    bool IsAllClear()
    {
        for (BVIndex i = 0; i < wordCount; i++)
        {
            if (!this->data[i].IsEmpty())
            {
                return false;
            }
        }

        return true;
    }

#if DBG_DUMP
    void Dump() const
    {
        bool hasBits = false;
        Output::Print(L"[  ");
        for (BVIndex i = 0; i < wordCount; i++)
        {
            hasBits = this->data[i].Dump(i * BVUnit::BitsPerWord, hasBits);
        }
        Output::Print(L"]\n");
    }
#endif
};




