//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
struct FreeObject
{
public:
    FreeObject * GetNext() const
    {
        AssertMsg((taggedNext & TaggedBit) == TaggedBit, "Free list corrupted");
        return (FreeObject *)(taggedNext & ~TaggedBit);
    }

    void SetNext(FreeObject * next)
    {
        Assert(((INT_PTR)next & TaggedBit) == 0);
        taggedNext = ((INT_PTR)next) | TaggedBit;
    }
    void ZeroNext() { taggedNext = 0; }
#ifdef RECYCLER_MEMORY_VERIFY
#pragma warning(suppress:4310)
    void DebugFillNext() { taggedNext = (INT_PTR)0xCACACACACACACACA; }
#endif
private:
    INT_PTR taggedNext;
    static INT_PTR const TaggedBit = 0x1;
};
}
