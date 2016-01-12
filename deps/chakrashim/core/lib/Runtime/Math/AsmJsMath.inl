//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Js
{
    template<typename T>
    __inline T AsmJsMath::Min(T aLeft, T aRight)
    {
        return aLeft < aRight ? aLeft : aRight;
    }

    template<>
    __inline double AsmJsMath::Min<double>(double aLeft, double aRight)
    {
        if (NumberUtilities::IsNan(aLeft) || NumberUtilities::IsNan(aRight))
        {
            return NumberConstants::NaN;
        }
        return aLeft < aRight ? aLeft : aRight;
    }

    template<typename T>
    __inline T AsmJsMath::Max(T aLeft, T aRight)
    {
        return aLeft > aRight ? aLeft : aRight;
    }

    template<>
    __inline double AsmJsMath::Max<double>(double aLeft, double aRight)
    {
        if (NumberUtilities::IsNan(aLeft) || NumberUtilities::IsNan(aRight))
        {
            return NumberConstants::NaN;
        }
        return aLeft > aRight ? aLeft : aRight;
    }

    template<typename T>
    __inline T AsmJsMath::Add( T aLeft, T aRight )
    {
        return aLeft + aRight;
    }

    template<typename T>
    __inline T AsmJsMath::Div( T aLeft, T aRight )
    {
        return aRight == 0 ? 0 : ( aLeft == (1<<31) && aRight == -1) ? aLeft : aLeft / aRight;
    }

    template<>
    __inline double AsmJsMath::Div<double>( double aLeft, double aRight )
    {
        return aLeft / aRight;
    }

    template<typename T>
    __inline T AsmJsMath::Mul( T aLeft, T aRight )
    {
        return aLeft * aRight;
    }

    template<>
    __inline int AsmJsMath::Mul( int aLeft, int aRight )
    {
        return (int)((int64)aLeft * (int64)aRight);
    }

    template<typename T>
    __inline T AsmJsMath::Sub( T aLeft, T aRight )
    {
        return aLeft - aRight;
    }

    template<typename T> __inline int AsmJsMath::CmpLt( T aLeft, T aRight ){return (int)(aLeft <  aRight);}
    template<typename T> __inline int AsmJsMath::CmpLe( T aLeft, T aRight ){return (int)(aLeft <= aRight);}
    template<typename T> __inline int AsmJsMath::CmpGt( T aLeft, T aRight ){return (int)(aLeft >  aRight);}
    template<typename T> __inline int AsmJsMath::CmpGe( T aLeft, T aRight ){return (int)(aLeft >= aRight);}
    template<typename T> __inline int AsmJsMath::CmpEq( T aLeft, T aRight ){return (int)(aLeft == aRight);}
    template<typename T> __inline int AsmJsMath::CmpNe( T aLeft, T aRight ){return (int)(aLeft != aRight);}

    template<typename T>
    __inline T AsmJsMath::Rem( T aLeft, T aRight )
    {
        return (aRight == 0) ? 0 : aLeft % aRight;
    }

    template<>
    __inline int AsmJsMath::Rem<int>( int aLeft, int aRight )
    {
        return ((aRight == 0) || (aLeft == (1<<31) && aRight == -1)) ? 0 : aLeft % aRight;
    }

    template<>
    __inline double AsmJsMath::Rem<double>( double aLeft, double aRight )
    {
        return NumberUtilities::Modulus( aLeft, aRight );
    }

    __inline int AsmJsMath::And( int aLeft, int aRight )
    {
        return aLeft & aRight;
    }

    __inline int AsmJsMath::Or( int aLeft, int aRight )
    {
        return aLeft | aRight;
    }

    __inline int AsmJsMath::Xor( int aLeft, int aRight )
    {
        return aLeft ^ aRight;
    }

    __inline int AsmJsMath::Shl( int aLeft, int aRight )
    {
        return aLeft << aRight;
    }

    __inline int AsmJsMath::Shr( int aLeft, int aRight )
    {
        return aLeft >> aRight;
    }

    __inline int AsmJsMath::ShrU( int aLeft, int aRight )
    {
        return (unsigned int)aLeft >> (unsigned int)aRight;
    }

    template<typename T>
    __inline T AsmJsMath::Neg( T aLeft )
    {
        return -aLeft;
    }

    __inline int AsmJsMath::Not( int aLeft )
    {
        return ~aLeft;
    }

    __inline int AsmJsMath::LogNot( int aLeft )
    {
        return !aLeft;
    }

    __inline int AsmJsMath::ToBool( int aLeft )
    {
        return !!aLeft;
    }

    inline int AsmJsMath::Clz32( int value)
    {
        DWORD index;
        if (_BitScanReverse(&index, value))
        {
            return 31 - index;
        }
        return 32;
    }
}
