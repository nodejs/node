//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
class Int64Math
{
public:
    static bool Add(int64 left, int64 right, int64 *pResult);
    static bool Sub(int64 left, int64 right, int64 *pResult);
    static bool Mul(int64 left, int64 right, int64 *pResult);
    static bool Div(int64 left, int64 right, int64 *pResult);
    static bool Mod(int64 left, int64 right, int64 *pResult);
    static bool Shl(int64 left, int64 right, int64 *pResult);
    static bool Shr(int64 left, int64 right, int64 *pResult);
    static bool ShrU(int64 left, int64 right, int64 *pResult);
    static bool And(int64 left, int64 right, int64 *pResult);
    static bool  Or(int64 left, int64 right, int64 *pResult);
    static bool Xor(int64 left, int64 right, int64 *pResult);

    static bool Neg(int64 val, int64 *pResult);
    static bool Not(int64 val, int64 *pResult);
    static bool Inc(int64 val, int64 *pResult);
    static bool Dec(int64 val, int64 *pResult);

    static uint64 Log2(int64 val);

    static int64 NearestInRangeTo(const int64 value, const int64 minimum, const int64 maximum); // inclusive
};
