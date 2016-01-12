//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

///---------------------------------------------------------------------------
///
/// class Math
///
///---------------------------------------------------------------------------

class Math
{
public:

    // Explicit cast to integral (may truncate).  Avoids warning C4302 'type cast': truncation
    template <typename T>
    static T PointerCastToIntegralTruncate(void * pointer)
    {
        return (T)(uintptr)pointer;
    }

    // Explicit cast to integral. Assert that it doesn't truncate.  Avoids warning C4302 'type cast': truncation
    template <typename T>
    static T PointerCastToIntegral(void * pointer)
    {
        T value = PointerCastToIntegralTruncate<T>(pointer);
        Assert((uintptr)value == (uintptr)pointer);
        return value;
    }

    static bool     FitsInDWord(size_t value);
    static UINT_PTR Rand();
    static bool     IsPow2(int32 val) { return (val > 0 && ((val-1) & val) == 0); }
    static uint32   NextPowerOf2(uint32 n);

    // Use for compile-time evaluation of powers of 2
    template<uint32 val> struct Is
    {
        static const bool Pow2 = ((val-1) & val) == 0;
    };

    // Defined in the header so that the Recycler static lib doesn't
    // need to pull in jscript.common.common.lib
    static uint32 Log2(uint32 value)
    {
        int i;

        for (i = 0; value >>= 1; i++);
        return i;
    }

    // Define a couple of overflow policies for the UInt32Math routines.

    // The default policy for overflow is to throw an OutOfMemory exception
    __declspec(noreturn) static void DefaultOverflowPolicy();

    // A functor (class with operator()) which records whether a the calculation
    // encountered an overflow condition.
    class RecordOverflowPolicy
    {
        bool fOverflow;
    public:
        RecordOverflowPolicy() : fOverflow(false)
        {
        }

        // Called when an overflow is detected
        void operator()()
        {
            fOverflow = true;
        }

        bool HasOverflowed() const
        {
            return fOverflow;
        }
    };

    template <typename T>
    static T Align(T size, T alignment)
    {
        return ((size + (alignment-1)) & ~(alignment-1));
    }

    template <typename T, class Func>
    static T AlignOverflowCheck(T size, T alignment, __inout Func& overflowFn)
    {
        Assert(size >= 0);
        T alignSize = Align(size, alignment);
        if (alignSize < size)
        {
            overflowFn();
        }
        return alignSize;
    }

    template <typename T>
    static T AlignOverflowCheck(T size, T alignment)
    {
        return AlignOverflowCheck(size, alignment, DefaultOverflowPolicy);
    }

};
