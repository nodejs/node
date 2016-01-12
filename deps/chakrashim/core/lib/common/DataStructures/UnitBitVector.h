//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


#define FOREACH_BITSET_IN_UNITBV(index, bv, BVUnitT) \
{ \
    BVIndex index; \
    BVUnitT _unit = bv; \
    for(index = _unit.GetNextBit(); index != -1; index = _unit.GetNextBit()) \
    { \
        _unit.Clear(index); \
        \


#define NEXT_BITSET_IN_UNITBV           }}

// Typedef
typedef uint32   UnitWord32;
typedef uint64   UnitWord64;

inline BOOLEAN
GetFirstBitSet(DWORD *Index, UnitWord32 Mask)
{
    return _BitScanForward(Index, Mask);
}

inline BOOLEAN
GetFirstBitSet(DWORD *Index, UnitWord64 Mask)
{
#if defined(_M_X64_OR_ARM64)
    return _BitScanForward64(Index, Mask);
#else
    //_BitScanForward64 intrinsic is not available in x86 & ARM
    if (_BitScanForward(Index, (UnitWord32)Mask))
    {
        return true;
    }

    if(_BitScanForward(Index, (UnitWord32) (Mask >> 32)))
    {
        *Index = *Index + 32;
        return true;
    }
    return false;
#endif
}

inline BOOLEAN
GetLastBitSet(DWORD *Index, UnitWord32 Mask)
{
    return _BitScanReverse(Index, Mask);
}
inline BOOLEAN
GetLastBitSet(DWORD *Index, UnitWord64 Mask)
{
#if defined(_M_X64_OR_ARM64)
    return _BitScanReverse64(Index, Mask);
#else
    //_BitScanReverse64 intrinsic is not available in x86 & ARM
    if (_BitScanReverse(Index, (UnitWord32)(Mask >> 32)))
    {
        *Index = *Index + 32;
        return true;
    }
    return _BitScanReverse(Index, (UnitWord32)Mask);
#endif
}

template <typename T>
class BVUnitT
{
// Data
private:

    T word;

// Constructor
public:
    BVUnitT(T initial = 0)
    {
        word = initial;
    }
    typedef T BVUnitTContainer;
// Implementation
private:

    static void AssertRange(BVIndex index)
    {
        AssertMsg(index < BitsPerWord, "index out of bound");
    }

    static UnitWord32 Reverse(UnitWord32 bitsToReverse) {
       bitsToReverse = (bitsToReverse & 0x55555555) <<  1 | (bitsToReverse & 0xAAAAAAAA) >>  1;
       bitsToReverse = (bitsToReverse & 0x33333333) <<  2 | (bitsToReverse & 0xCCCCCCCC) >>  2;
       bitsToReverse = (bitsToReverse & 0x0F0F0F0F) <<  4 | (bitsToReverse & 0xF0F0F0F0) >>  4;
       bitsToReverse = (bitsToReverse & 0x00FF00FF) <<  8 | (bitsToReverse & 0xFF00FF00) >>  8;
       bitsToReverse = (bitsToReverse & 0x0000FFFF) << 16 | (bitsToReverse & 0xFFFF0000) >> 16;
       return bitsToReverse;
    }

    static UnitWord64 Reverse(UnitWord64 bits)
    {
       UnitWord32 lower = (UnitWord32) bits;
       UnitWord32 upper = (UnitWord32) (bits >> 32);

       UnitWord64 result = ((UnitWord64) Reverse(lower)) << 32;
       result |= Reverse(upper);

       return result;
    }

    static BVIndex CountBit(UnitWord32 bits)
    {
        const uint _5_32 =     0x55555555;
        const uint _3_32 =     0x33333333;
        const uint _F1_32 =    0x0f0f0f0f;
        // In-place adder tree: perform 16 1-bit adds, 8 2-bit adds, 4 4-bit adds,
        // 2 8=bit adds, and 1 16-bit add.
        // From Dr. Dobb's Nov. 2000 letters, from Phil Bagwell, on reducing
        // the cost by removing some of the masks that can be "forgotten" dbitse
        // to the max # of bits set (32) that will fit in a byte.
        //
        bits -= (bits >> 1) & _5_32;
        bits = ((bits >> 2) & _3_32)  + (bits & _3_32);
        bits = ((bits >> 4) & _F1_32) + (bits & _F1_32);
        bits += bits >> 8;
        bits += bits >> 16;
        return BVIndex(bits & 0xff);
    }

    static BVIndex CountBit(UnitWord64 bits)
    {
#if DBG
        unsigned countBits = CountBit((UnitWord32)bits) + CountBit((UnitWord32)(bits >> 32));
#endif

        const uint64 _5_64 =     0x5555555555555555ui64;
        const uint64 _3_64 =     0x3333333333333333ui64;
        const uint64 _F1_64 =    0x0f0f0f0f0f0f0f0fui64;

        // In-place adder tree: perform 32 1-bit adds, 16 2-bit adds, 8 4-bit adds,
        // 4 8-bit adds, 2 16-bit adds, and 1 32-bit add.
        // From Dr. Dobb's Nov. 2000 letters, from Phil Bagwell, on reducing
        // the cost by removing some of the masks that can be "forgotten" due
        // to the max # of bits set (64) that will fit in a byte.
        //

        bits -= (bits >> 1) & _5_64;
        bits = ((bits >> 2) & _3_64)  + (bits & _3_64);
        bits = ((bits >> 4) & _F1_64) + (bits & _F1_64);
        bits += bits >> 8;
        bits += bits >> 16;
        bits += bits >> 32;

        AssertMsg(countBits == (bits & 0xff), "Wrong count?");

        return (BVIndex)(bits & 0xff);
    }

    static unsigned int NumLeadingZeroes(UnitWord32 bits)
    {
       int n = 0;

       if (bits == 0) return 32;

       // Binary search to figure out the number of leading zeroes
       if (bits <= 0x0000FFFF)
       {
           // At least 16 leading zeroes- so remove them, and
           // let's figure out how many leading zeroes in the last 16 bits
           n = n + 16;
           bits = bits << 16;
       }

       if (bits <= 0x00FFFFFF)
       {
           // At least 8 more zeroes- remove them, and repeat process
           n = n + 8;
           bits = bits << 8;
       }
       if (bits <= 0x0FFFFFFF)
       {
           n = n + 4;
           bits = bits << 4;
       }
       if (bits <= 0x3FFFFFFF)
       {
           n = n + 2;
           bits = bits << 2;
       }
       if (bits <= 0x7FFFFFFF)
       {
           n = n + 1;
       }

       return n;
    }

    static unsigned int NumLeadingZeroes(UnitWord64 bits)
    {
       UnitWord32 lower = (UnitWord32) bits;
       UnitWord32 upper = (UnitWord32) (bits >> 32);

       if (upper == 0)
       {
           return 32 + NumLeadingZeroes(lower);
       }
       else
       {
           return NumLeadingZeroes(upper);
       }
    }

public:

    enum
    {
        BitsPerWord  = sizeof(T) * MachBits,
        BitMask      = BitsPerWord - 1,
        AllOnesMask  = -1
    };

    //ShiftValue is essentially log(sizeof(T))
    //Initialization is through template specialization
    static const LONG ShiftValue;

    static BVIndex Position(BVIndex index)
    {
        return index >> ShiftValue;
    }

    static BVIndex Offset(BVIndex index)
    {
        return index & BitMask;
    }

    static BVIndex Floor(BVIndex index)
    {
        return index & (~BitMask);
    }

    static T GetTopBitsClear(BVIndex len)
    {
        return ((T)1 << Offset(len)) - 1;
    }

    bool Equal(BVUnitT unit) const
    {
        return this->word == unit.word;
    }

    void Set(BVIndex index)
    {
        AssertRange(index);
        this->word |= (T)1 << index;
    }


    void Clear(BVIndex index)
    {
        AssertRange(index);
        this->word &= ~((T)1 << index);
    }



    void Complement(BVIndex index)
    {
        AssertRange(index);
        this->word ^= (T)1 << index;
    }

    BOOLEAN Test(const BVIndex index) const
    {
        AssertRange(index);
        return (this->word & ( (T)1 << index)) != 0;
    }
    BOOLEAN Test(BVUnitT const unit) const
    {
        return (this->word & unit.word) != 0;
    }
    BOOLEAN TestRange(const BVIndex index, uint length) const
    {
        T mask = ((T)AllOnesMask) >> (BitsPerWord - length) << index;
        return (this->word & mask) == mask;
    }
    void SetRange(const BVIndex index, uint length)
    {
        T mask = ((T)AllOnesMask) >> (BitsPerWord - length) << index;
        this->word |= mask;
    }
    void ClearRange(const BVIndex index, uint length)
    {
        T mask = ((T)AllOnesMask) >> (BitsPerWord - length) << index;
        this->word &= ~mask;
    }

    BVIndex GetNextBit() const
    {
        DWORD index;
        if(GetFirstBitSet(&index, this->word))
        {
            return index;
        }
        else
        {
            return BVInvalidIndex;
        }
    }
    BVIndex GetNextBit(BVIndex index) const
    {
        AssertRange(index);
        BVUnitT temp = *this;
        temp.ClearAllTill(index);
        return temp.GetNextBit();
    }

    BVIndex GetPrevBit() const
    {
        DWORD index;
        if(GetLastBitSet(&index, this->word))
        {
            return index;
        }
        else
        {
            return BVInvalidIndex;
        }
    }

    inline unsigned int GetNumberOfLeadingZeroes()
    {
        return NumLeadingZeroes(this->word);
    }

    T GetWord() const
    {
        return this->word;
    }

    inline BVIndex FirstStringOfOnes(unsigned int l)
    {
       unsigned int leadingZeroes;

       BVIndex i = 0;

       if (this->word == 0)
       {
           return BVInvalidIndex;
       }

       T bitVector = Reverse(this->word);

       while (bitVector != 0) {
           // Find the number of leading zeroes
           leadingZeroes = NumLeadingZeroes(bitVector);

           // Skip over leading zeroes
           bitVector = bitVector << leadingZeroes;
           i = i + leadingZeroes;

           // Invert the bit vector- leading number of leading zeroes = number of leading 1's in the original bit vector
           leadingZeroes = NumLeadingZeroes(~bitVector);      // Count first/next group of 1's.

           if (leadingZeroes >= l)
             return i;

           // If there aren't enough ones, we skip over them, iterate again
           bitVector = bitVector << leadingZeroes;
           i = i + leadingZeroes;
       }

       // No sub-sequence of 1's of length l found in this bit vector
       return BVInvalidIndex;
    }

    void ClearAllTill(BVIndex index)
    {
        AssertRange(index);
        this->word &= ((T)BVUnitT::AllOnesMask) <<(T)index;
    }

    BVIndex Count() const
    {
        return CountBit(this->word);
    }

    bool IsEmpty() const
    {
        return 0 == this->word;
    }

    bool IsFull() const
    {
        return this->word == AllOnesMask;
    }

    void SetAll()
    {
        this->word = (T)AllOnesMask;
    }

    void ClearAll()
    {
        this->word = 0;
    }

    void ComplimentAll()
    {
        this->word = ~this->word;
    }

    void Or(BVUnitT x)
    {
        this->word |= x.word;
    }

    void OrComplimented(BVUnitT x)
    {
        this->word |= (~x.word);
    }

    void And(BVUnitT x)
    {
        this->word &= x.word;
    }

    void Xor(BVUnitT x)
    {
        this->word ^= x.word;
    }

    uint DiffCount(BVUnitT x) const
    {
        return CountBit(x.word ^ this->word);
    }

    void Minus(BVUnitT x)
    {
        this->word &= (~x.word);
    }

    void Copy(BVUnitT x)
    {
        this->word = x.word;
    }

    void ShiftLeft(BVIndex count)
    {
        this->word <<= count;
    }

    void ShiftRight(BVIndex count)
    {
        this->word >>= count;
    }

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    void DumpWord()
    {
        Output::Print(L"%p", this->word);
    }

    bool Dump(BVIndex base = 0, bool hasBits = false) const
    {
        FOREACH_BITSET_IN_UNITBV(index, *this, BVUnitT)
        {
            if (hasBits)
            {
                Output::Print(L", ");
            }
            Output::Print(L"%u", index + base);
            hasBits = true;
        }
        NEXT_BITSET_IN_UNITBV;
        return hasBits;
    }
#endif
};

typedef BVUnitT<UnitWord32> BVUnit32;
typedef BVUnitT<UnitWord64> BVUnit64;

const LONG BVUnitT<UnitWord32>::ShiftValue = 5;
const LONG BVUnitT<UnitWord64>::ShiftValue = 6;

#if defined(_M_X64_OR_ARM64)
    typedef BVUnit64 BVUnit;
#else
    typedef BVUnit32 BVUnit;
#endif


