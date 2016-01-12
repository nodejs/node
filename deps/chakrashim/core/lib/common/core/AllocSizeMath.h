//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
class AllocSizeMath
{
    static const size_t MaxMemory = static_cast<size_t>(-1);

public:
    // Works for both 32bit and 64bit size_t arithmetic. It's also pretty
    // optimal in the cases where either left or right or both are small, compile-
    // time constants.
    static size_t Add(size_t left, size_t right)
    {
        size_t allocSize = left + right;
        if (allocSize < left)
        {
            // Integer overflow in computation, allocate max memory which will fail with out of memory
            return MaxMemory;
        }
        return allocSize;
    }

    template <typename T>
    static T Min(const T& a, const T& b)
    {
        return (a < b ? a : b);
    }

    // Optimized for right being a constant power of 2...
    static size_t Mul(size_t left, size_t right)
    {
        size_t allocSize = left * right;
        if (left != (allocSize / right))
        {
            // Integer overflow in computation, allocate max memory which will fail with out of memory
            return MaxMemory;
        }
        return allocSize;
    }

    static size_t Align(size_t size, size_t alignment)
    {
        if (size >= (MaxMemory & ~(alignment - 1)))
        {
            return MaxMemory & ~(alignment - 1);
        }
        return Math::Align(size, alignment);
    }
};
