//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCommonPch.h"
#include "math.h"

bool
Math::FitsInDWord(size_t value)
{
    return ((size_t)(signed int)(value & 0xFFFFFFFF) == value);
}

uint32
Math::NextPowerOf2(uint32 n)
{
    n = n - 1;
    n = n | (n >> 1);
    n = n | (n >> 2);
    n = n | (n >> 4);
    n = n | (n >> 8);
    n = n | (n >> 16);
    n++;
    return n;
}

UINT_PTR
Math::Rand()
{
    unsigned int rand;
    rand_s(&rand);
    UINT_PTR newRand = static_cast<UINT_PTR>(rand);

#if TARGET_64
    rand_s(&rand);
    newRand |= static_cast<UINT_PTR>(rand) << 32;
#endif

    return newRand;
}

__declspec(noreturn) void Math::DefaultOverflowPolicy()
{
    Js::Throw::OutOfMemory();
}
