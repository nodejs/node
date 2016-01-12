//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"

BVFixed::BVFixed(BVFixed * initBv) :
   len(initBv->Length())
{
   this->Copy(initBv);
}

BVFixed::BVFixed(BVIndex length, bool initialSet) :
    len(length)
{
    Assert(length != 0);
    if (initialSet)
    {
        this->SetAll();
    }
    else
    {
        this->ClearAll();
    }
}

size_t
BVFixed::GetAllocSize(BVIndex length)
{
    Assert(length != 0);
    return sizeof(BVFixed) + sizeof(BVUnit) * BVFixed::WordCount(length);
}

void
BVFixed::Init(BVIndex length)
{
    Assert(length != 0);
    len = length;

}

BVIndex
BVFixed::WordCount(BVIndex length)
{
    Assert(length != 0);
    return ((length - 1) >> BVUnit::ShiftValue) + 1;
}

BVIndex
BVFixed::WordCount() const
{
    return WordCount(Length());
}

const BVUnit *
BVFixed::BitsFromIndex(BVIndex i) const
{
    AssertRange(i);
    return &this->data[BVUnit::Position(i)];
}

BVUnit *
BVFixed::BitsFromIndex(BVIndex i)
{
    AssertRange(i);
    return &this->data[BVUnit::Position(i)];
}

const BVUnit *
BVFixed::BeginUnit() const
{
    return &this->data[0];
}

const BVUnit *
BVFixed::EndUnit() const
{
    return &this->data[WordCount()];
}

BVUnit *
BVFixed::BeginUnit()
{
    return &this->data[0];
}

BVUnit *
BVFixed::EndUnit()
{
    return &this->data[WordCount()];
}

BVIndex
BVFixed::GetNextBit(BVIndex i) const
{
    AssertRange(i);

    const   BVUnit * chunk      = BitsFromIndex(i);
            BVIndex  base       = BVUnit::Floor(i);


    BVIndex offset = chunk->GetNextBit(BVUnit::Offset(i));
    if(-1 != offset)
    {
        return base + offset;
    }

    while(++chunk != this->EndUnit())
    {
        base  += BVUnit::BitsPerWord;
        offset = chunk->GetNextBit();
        if(-1 != offset)
        {
            return base + offset;
        }
    }

   return BVInvalidIndex;
}

void
BVFixed::AssertRange(BVIndex i) const
{
    AssertMsg(i < this->Length(), "index out of bound");
}

void
BVFixed::AssertBV(const BVFixed *bv) const
{
    AssertMsg(NULL != bv, "Cannot operate on NULL bitvector");
}

BVIndex
BVFixed::Length() const
{
    return this->len;
}

void
BVFixed::SetAll()
{
    memset(&this->data[0], -1, WordCount() * sizeof(BVUnit));
    ClearEnd();
}

void
BVFixed::ClearAll()
{
    ZeroMemory(&this->data[0], WordCount() * sizeof(BVUnit));
}

BOOLEAN
BVFixed::TestAndSet(BVIndex i)
{
    AssertRange(i);
    BVUnit * bvUnit = this->BitsFromIndex(i);
    BVIndex offset = BVUnit::Offset(i);
    BOOLEAN bit = bvUnit->Test(offset);
    bvUnit->Set(offset);
    return bit;
}

BOOLEAN
BVFixed::TestAndClear(BVIndex i)
{
    AssertRange(i);
    BVUnit * bvUnit = this->BitsFromIndex(i);
    BVIndex offset = BVUnit::Offset(i);
    BOOLEAN bit = bvUnit->Test(offset);
    bvUnit->Clear(offset);
    return bit;
}

BOOLEAN
BVFixed::operator[](BVIndex i) const
{
    AssertRange(i);
    return this->Test(i);
}

void
BVFixed::Or(const BVFixed*bv)
{
    AssertBV(bv);
    this->for_each(bv, &BVUnit::Or);
}

//
// Xors the two bit vectors and returns the count of bits which are different.
//
uint
BVFixed::DiffCount(const BVFixed*bv) const
{
    const BVUnit *i, *j;
    uint count = 0;
    for(i  =  this->BeginUnit(), j = bv->BeginUnit();
        i !=  this->EndUnit() && j != bv->EndUnit();
        i++, j++)
    {
        count += i->DiffCount(*j);
    }

    // Assumes that the default value of is 0
    while(i != this->EndUnit())
    {
        count += i->Count();
        i++;
    }
    while(j != bv->EndUnit())
    {
        count += j->Count();
        j++;
    }
    return count;
}

void
BVFixed::OrComplimented(const BVFixed*bv)
{
    AssertBV(bv);
    this->for_each(bv, &BVUnit::OrComplimented);
    ClearEnd();
}

void
BVFixed::And(const BVFixed*bv)
{
    AssertBV(bv);
    this->for_each(bv, &BVUnit::And);
}

void
BVFixed::Minus(const BVFixed*bv)
{
    AssertBV(bv);
    this->for_each(bv, &BVUnit::Minus);
}

void
BVFixed::Copy(const BVFixed*bv)
{
    AssertBV(bv);
    Assert(len >= bv->len);

#if 1
    js_memcpy_s(&this->data[0], WordCount() * sizeof(BVUnit), &bv->data[0], bv->WordCount() * sizeof(BVUnit));
#else
    this->for_each(bv, &BVUnit::Copy);
#endif
}

void
BVFixed::CopyBits(const BVFixed * bv, BVIndex i)
{
    AssertBV(bv);
    BVIndex offset = BVUnit::Offset(i);
    BVIndex position = BVUnit::Position(i);
    BVIndex len = bv->WordCount() - position;
    BVIndex copylen = min(WordCount(), len);
    if (offset == 0)
    {
        js_memcpy_s(&this->data[0], copylen * sizeof(BVUnit), &bv->data[BVUnit::Position(i)], copylen * sizeof(BVUnit));
    }
    else
    {
        BVIndex pos = position;
        for (BVIndex j = 0; j < copylen; j++)
        {
            Assert(pos < bv->WordCount());
            this->data[j] = bv->data[pos];
            this->data[j].ShiftRight(offset);

            pos++;
            if (pos >= bv->WordCount())
            {
                break;
            }
            BVUnit temp = bv->data[pos];
            temp.ShiftLeft(BVUnit::BitsPerWord - offset);
            this->data[j].Or(temp);
        }
    }
#if DBG
    for (BVIndex curr = i; curr < i + this->Length(); curr++)
    {
        Assert(this->Test(curr - i) == bv->Test(curr));
    }
#endif
}

void
BVFixed::ComplimentAll()
{
    for(BVIndex i=0; i < this->WordCount(); i++)
    {
        this->data[i].ComplimentAll();
    }

    ClearEnd();
}

void
BVFixed::ClearEnd()
{
    uint offset = BVUnit::Offset(this->Length());
    if (offset != 0)
    {
        Assert((((uint64)1 << BVUnit::Offset(this->Length())) - 1) == BVUnit::GetTopBitsClear(this->Length()));
        this->data[this->WordCount() - 1].And(BVUnit::GetTopBitsClear(this->Length()));
    }
}

BVIndex
BVFixed::Count() const
{
    BVIndex sum = 0;
    for (BVIndex i=0; i < this->WordCount(); i++)
    {
        sum += this->data[i].Count();
    }
    Assert(sum <= this->Length());
    return sum;
}

JsUtil::FBVEnumerator
BVFixed::BeginSetBits()
{
    return JsUtil::FBVEnumerator(this->BeginUnit(), this->EndUnit());
}

bool BVFixed::IsAllClear() const
{
    bool isClear = true;
    for (BVIndex i=0; i < this->WordCount(); i++)
    {
        isClear = this->data[i].IsEmpty() && isClear;
        if(!isClear)
        {
            break;
        }
    }
    return isClear;
}


#if DBG_DUMP

void
BVFixed::Dump() const
{
    bool hasBits = false;
    Output::Print(L"[  ");
    for(BVIndex i=0; i < this->WordCount(); i++)
    {
        hasBits = this->data[i].Dump(i * BVUnit::BitsPerWord, hasBits);
    }
    Output::Print(L"]\n");
}
#endif
