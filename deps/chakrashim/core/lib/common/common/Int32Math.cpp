//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCommonPch.h"
#include "Common\Int32Math.h"

bool
Int32Math::Add(int32 left, int32 right, int32 *pResult)
{
    if (sizeof(void *) == 4)
    {
        // Overflow occurs when the result has a different sign from both the left and right operands
        *pResult = left + right;
        return ((left ^ *pResult) & (right ^ *pResult)) < 0;
    }

    Assert(sizeof(void *) == 8);
    int64 result64 = (int64)left + (int64)right;
    *pResult = (int32)result64;
    return result64 != (int64)(*pResult);
}

bool
Int32Math::Mul(int32 left, int32 right, int32 *pResult)
{
    bool fOverflow;
#if _M_IX86
    __asm
    {
        mov eax, left
        imul right
        seto fOverflow
        mov ecx, pResult
        mov[ecx], eax
    }
#else
    int64 result64 = (int64)left * (int64)right;

    *pResult = (int32)result64;

    fOverflow = (result64 != (int64)(*pResult));
#endif

    return fOverflow;
}

bool
Int32Math::Mul(int32 left, int32 right, int32 *pResult, int32* pOverflowValue)
{
    bool fOverflow;
#if _M_IX86
    __asm
    {
        mov eax, left
        imul right
        seto fOverflow
        mov ecx, pResult
        mov[ecx], eax
        mov ecx, pOverflowValue
        mov[ecx], edx
    }
#else
    int64 result64 = (int64)left * (int64)right;

    *pResult = (int32)result64;
    *pOverflowValue = (int32)(result64 >> 32);

    fOverflow = (result64 != (int64)(*pResult));
#endif

    return fOverflow;
}

bool
Int32Math::Shl(int32 left, int32 right, int32 *pResult)
{
    *pResult = left << (right & 0x1F);
    return (left != (int32)((uint32)*pResult >> right));
}

bool
Int32Math::Sub(int32 left, int32 right, int32 *pResult)
{
    if(sizeof(void *) == 4)
    {
        // Overflow occurs when the result has a different sign from the left operand, and the result has the same sign as the
        // right operand
        *pResult = left - right;
        return ((left ^ *pResult) & ~(right ^ *pResult)) < 0;
    }

    Assert(sizeof(void *) == 8);
    int64 result64 = (int64)left - (int64)right;
    *pResult = (int32)result64;
    return result64 != (int64)(*pResult);
}

bool
Int32Math::Div(int32 left, int32 right, int32 *pResult)
{
    AssertMsg(right != 0, "Divide by zero...");

    if (right == -1 && left == INT_MIN)
    {
        //Special check for INT_MIN/-1
        return true;
    }
    *pResult = left / right;
    return false;
}

bool
Int32Math::Mod(int32 left, int32 right, int32 *pResult)
{
    AssertMsg(right != 0, "Mod by zero...");
    if (right == -1 && left == INT_MIN)
    {
        //Special check for INT_MIN/-1
        return true;
    }
    *pResult = left % right;
    return false;
}

bool
Int32Math::Shr(int32 left, int32 right, int32 *pResult)
{
    *pResult = left >> (right & 0x1F);
    return false;
}

bool
Int32Math::ShrU(int32 left, int32 right, int32 *pResult)
{
    uint32 uResult = ((uint32)left) >> (right & 0x1F);
    *pResult = uResult;
    return false;
}

bool
Int32Math::And(int32 left, int32 right, int32 *pResult)
{
    *pResult = left & right;
    return false;
}

bool
Int32Math::Or(int32 left, int32 right, int32 *pResult)
{
    *pResult = left | right;
    return false;
}

bool
Int32Math::Xor(int32 left, int32 right, int32 *pResult)
{
    *pResult = left ^ right;
    return false;
}

bool
Int32Math::Neg(int32 val, int32 *pResult)
{
    *pResult = -val;
    return *pResult == INT_MIN;
}

bool
Int32Math::Not(int32 val, int32 *pResult)
{
    *pResult = ~val;
    return false;
}

bool
Int32Math::Inc(int32 val, int32 *pResult)
{
    *pResult = val + 1;
    // Overflow if result ends up less than input
    return *pResult <= val;
}

bool
Int32Math::Dec(int32 val, int32 *pResult)
{
    *pResult = val - 1;
    // Overflow if result ends up greater than input
    return *pResult >= val;
}

int32
Int32Math::NearestInRangeTo(const int32 value, const int32 minimum, const int32 maximum) // inclusive
{
    Assert(minimum <= maximum);
    return minimum >= value ? minimum : maximum <= value ? maximum : value;
}
