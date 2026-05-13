//===-- Common constants for mathvec functions ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATHVEC_COMMON_CONSTANTS_H
#define LLVM_LIBC_SRC___SUPPORT_MATHVEC_COMMON_CONSTANTS_H

namespace LIBC_NAMESPACE_DECL {

namespace mathvec {

// Lookup table for mantissas of 2^(i / 64) with i = 0, ..., 63.
LIBC_INLINE_VAR constexpr uint64_t EXP_MANTISSA[64] = {
    0x0000000000000, 0x02c9a3e778061, 0x059b0d3158574, 0x0874518759bc8,
    0x0b5586cf9890f, 0x0e3ec32d3d1a2, 0x11301d0125b51, 0x1429aaea92de0,
    0x172b83c7d517b, 0x1a35beb6fcb75, 0x1d4873168b9aa, 0x2063b88628cd6,
    0x2387a6e756238, 0x26b4565e27cdd, 0x29e9df51fdee1, 0x2d285a6e4030b,
    0x306fe0a31b715, 0x33c08b26416ff, 0x371a7373aa9cb, 0x3a7db34e59ff7,
    0x3dea64c123422, 0x4160a21f72e2a, 0x44e086061892d, 0x486a2b5c13cd0,
    0x4bfdad5362a27, 0x4f9b2769d2ca7, 0x5342b569d4f82, 0x56f4736b527da,
    0x5ab07dd485429, 0x5e76f15ad2148, 0x6247eb03a5585, 0x6623882552225,
    0x6a09e667f3bcd, 0x6dfb23c651a2f, 0x71f75e8ec5f74, 0x75feb564267c9,
    0x7a11473eb0187, 0x7e2f336cf4e62, 0x82589994cce13, 0x868d99b4492ed,
    0x8ace5422aa0db, 0x8f1ae99157736, 0x93737b0cdc5e5, 0x97d829fde4e50,
    0x9c49182a3f090, 0xa0c667b5de565, 0xa5503b23e255d, 0xa9e6b5579fdbf,
    0xae89f995ad3ad, 0xb33a2b84f15fb, 0xb7f76f2fb5e47, 0xbcc1e904bc1d2,
    0xc199bdd85529c, 0xc67f12e57d14b, 0xcb720dcef9069, 0xd072d4a07897c,
    0xd5818dcfba487, 0xda9e603db3285, 0xdfc97337b9b5f, 0xe502ee78b3ff6,
    0xea4afa2a490da, 0xefa1bee615a27, 0xf50765b6e4540, 0xfa7c1819e90d8,
};

} // namespace mathvec

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATHVEC_COMMON_CONSTANTS_H
