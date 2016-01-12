//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
class Int16Math
{
public:
    template< class Func >
    static int16 Add(int16 lhs, int16 rhs, __inout Func& overflowFn)
    {
        int16 result = lhs + rhs;

        // If the result is smaller than the LHS, then we overflowed
        if( result < lhs )
        {
            overflowFn();
        }

        return result;
    }

    template< class Func >
    static void Inc(int16& lhs, __inout Func& overflowFn)
    {
        ++lhs;

        // If lhs becomes 0, then we overflowed
        if(!lhs)
        {
            overflowFn();
        }
    }

    // Convenience function which uses DefaultOverflowPolicy (throws OOM when overflow)
    static int16 Add(int16 lhs, uint16 rhs)
    {
        return Add(lhs, rhs, ::Math::DefaultOverflowPolicy);
    }

    // Convenience functions which return a bool indicating overflow
    static bool Add(int16 lhs, int16 rhs, __out int16* result)
    {
        ::Math::RecordOverflowPolicy overflowGuard;
        *result = Add(lhs, rhs, overflowGuard);
        return overflowGuard.HasOverflowed();
    }

    // Convenience function which uses DefaultOverflowPolicy (throws OOM when overflow)
    static void Inc(int16& lhs)
    {
        Inc(lhs, ::Math::DefaultOverflowPolicy);
    }
};
