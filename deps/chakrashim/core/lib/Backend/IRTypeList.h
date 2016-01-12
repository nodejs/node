//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#define b(x) (x * MachBits)

//      Uppercase  Base      Size      Size        EnReg OK?  Brief name
//        Name     Type      Bytes     Bits          Flag     for dumps
//      --------- ---------- --------   ----------  --------  ----------

IRTYPE(Illegal,   Illegal,   0,        b(0),        0,        ill)
IRTYPE(Int8,      Int,       1,        b(1),        1,        i8)
IRTYPE(Int16,     Int,       2,        b(2),        1,        i16)
IRTYPE(Int32,     Int,       4,        b(4),        1,        i32)
IRTYPE(Int64,     Int,       8,        b(8),        1,        i64)
IRTYPE(Uint8,     Uint,      1,        b(1),        1,        u8)
IRTYPE(Uint16,    Uint,      2,        b(2),        1,        u16)
IRTYPE(Uint32,    Uint,      4,        b(4),        1,        u32)
IRTYPE(Uint64,    Uint,      8,        b(8),        1,        u64)
IRTYPE(Float32,   Float,     4,        b(4),        1,        f32)
IRTYPE(Float64,   Float,     8,        b(8),        1,        f64)

// IRTYPE(Simd128, Simd, 16, b(16), 1, simd128)
IRTYPE(Simd128F4,   Simd,     16,      b(16),       1,        simd128)
IRTYPE(Simd128I4,   Simd,     16,      b(16),       1,        simd128)
IRTYPE(Simd128D2,   Simd,     16,      b(16),       1,        simd128)

//
// review: MachPtr->Align is incorrect on AMD64. We don't use this value today (6/29/09) so its fine
// for now. make sure it is either fixed to removed.
//

IRTYPE(Var,       Var,       MachPtr,  b(MachPtr),  1,        var)
IRTYPE(Condcode, Condcode,   0,        4,           1,        cc)
IRTYPE(Misc,     Misc,       0,        0,           0,        misc)
#undef b
