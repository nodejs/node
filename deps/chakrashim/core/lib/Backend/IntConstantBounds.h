//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class IntConstantBounds
{
private:
    int32 lowerBound;
    int32 upperBound;

public:
    IntConstantBounds() : lowerBound(0), upperBound(0)
    {
    }

    IntConstantBounds(const int32 lowerBound, const int32 upperBound)
        : lowerBound(lowerBound), upperBound(upperBound)
    {
        Assert(lowerBound <= upperBound);
    }

public:
    int32 LowerBound() const
    {
        return lowerBound;
    }

    int32 UpperBound() const
    {
        return upperBound;
    }

    bool IsConstant() const
    {
        return lowerBound == upperBound;
    }

    bool IsTaggable() const
    {
#if INT32VAR
        // All 32-bit ints are taggable on 64-bit architectures
        return true;
#else
        return lowerBound >= Js::Constants::Int31MinValue && upperBound <= Js::Constants::Int31MaxValue;
#endif
    }

    bool IsLikelyTaggable() const
    {
#if INT32VAR
        // All 32-bit ints are taggable on 64-bit architectures
        return true;
#else
        return lowerBound <= Js::Constants::Int31MaxValue && upperBound >= Js::Constants::Int31MinValue;
#endif
    }

    ValueType GetValueType() const
    {
        return ValueType::GetInt(IsLikelyTaggable());
    }

    IntConstantBounds And_0x1f() const
    {
        const int32 mask = 0x1f;
        if(static_cast<UIntConstType>(upperBound - lowerBound) >= static_cast<UIntConstType>(mask) ||
            (lowerBound & mask) > (upperBound & mask))
        {
            // The range contains all items in the set {0-mask}, or the range crosses a boundary of {0-mask}. Since we cannot
            // represent ranges like {0-5,8-mask}, just use {0-mask}.
            return IntConstantBounds(0, mask);
        }
        return IntConstantBounds(lowerBound & mask, upperBound & mask);
    }

    bool Contains(const int32 value) const
    {
        return lowerBound <= value && value <= upperBound;
    }

    bool operator ==(const IntConstantBounds &other) const
    {
        return lowerBound == other.lowerBound && upperBound == other.upperBound;
    }
};
