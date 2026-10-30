//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains common constants and tables for mathvec functions.
///
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

// Lookup table for log(1 / R[i]), where R is the reciprocal from the table
// found in libc/src/__support/math/common_constants.h, specifically optimised
// for mathvec RN. There are two main differences:
//  1. The final entry has LOG_TABLE[127] = log(2), rather than 0 in the scalar
//  table.
//  2. Some values in the table are adjusted to remove hard-to-round cases,
//  making the main computation path branchless.
//  The following adjustments are made, in FP64 ULP:
//     LOG_TABLE[23] -15
//     LOG_TABLE[49] +47
//     LOG_TABLE[65]  +2
//     LOG_TABLE[88] +51
//     LOG_TABLE[93] +21
//     LOG_TABLE[96]  +3
//     LOG_TABLE[101] -2
LIBC_INLINE_VAR constexpr double LOG_TABLE[128] = {
    0x0.0000000000000p+0, 0x1.010157588de71p-7, 0x1.0205658935847p-6,
    0x1.8492528c8cabfp-6, 0x1.0415d89e74444p-5, 0x1.466aed42de3eap-5,
    0x1.894aa149fb343p-5, 0x1.ccb73cdddb2ccp-5, 0x1.08598b59e3a07p-4,
    0x1.1973bd1465567p-4, 0x1.3bdf5a7d1ee64p-4, 0x1.5e95a4d9791cbp-4,
    0x1.700d30aeac0e1p-4, 0x1.9335e5d594989p-4, 0x1.b6ac88dad5b1cp-4,
    0x1.c885801bc4b23p-4, 0x1.ec739830a1120p-4, 0x1.fe89139dbd566p-4,
    0x1.1178e8227e47cp-3, 0x1.1aa2b7e23f72ap-3, 0x1.2d1610c86813ap-3,
    0x1.365fcb0159016p-3, 0x1.4913d8333b561p-3, 0x1.527e5e4a1b57ep-3,
    0x1.6574ebe8c133ap-3, 0x1.6f0128b756abcp-3, 0x1.823c16551a3c2p-3,
    0x1.8beafeb38fe8cp-3, 0x1.95a5adcf7017fp-3, 0x1.a93ed3c8ad9e3p-3,
    0x1.b31d8575bce3dp-3, 0x1.bd087383bd8adp-3, 0x1.d1037f2655e7bp-3,
    0x1.db13db0d48940p-3, 0x1.e530effe71012p-3, 0x1.ef5ade4dcffe6p-3,
    0x1.f991c6cb3b379p-3, 0x1.07138604d5862p-2, 0x1.0c42d676162e3p-2,
    0x1.1178e8227e47cp-2, 0x1.16b5ccbacfb73p-2, 0x1.1bf99635a6b95p-2,
    0x1.269621134db92p-2, 0x1.2bef07cdc9354p-2, 0x1.314f1e1d35ce4p-2,
    0x1.36b6776be1117p-2, 0x1.3c25277333184p-2, 0x1.419b423d5e8c7p-2,
    0x1.4718dc271c41bp-2, 0x1.4c9e09e172c6bp-2, 0x1.522ae0738a3d8p-2,
    0x1.57bf753c8d1fbp-2, 0x1.5d5bddf595f30p-2, 0x1.630030b3aac49p-2,
    0x1.68ac83e9c6a14p-2, 0x1.6e60ee6af1972p-2, 0x1.741d876c67bb1p-2,
    0x1.79e26687cfb3ep-2, 0x1.7fafa3bd8151cp-2, 0x1.85855776dcbfbp-2,
    0x1.8b639a88b2df5p-2, 0x1.914a8635bf68ap-2, 0x1.973a3431356aep-2,
    0x1.9d32bea15ed3bp-2, 0x1.a33440224fa79p-2, 0x1.a33440224fa7bp-2,
    0x1.a93ed3c8ad9e3p-2, 0x1.af5295248cdd0p-2, 0x1.b56fa04462909p-2,
    0x1.bb9611b80e2fbp-2, 0x1.c1c60693fa39ep-2, 0x1.c1c60693fa39ep-2,
    0x1.c7ff9c74554c9p-2, 0x1.ce42f18064743p-2, 0x1.d490246defa6bp-2,
    0x1.dae75484c9616p-2, 0x1.e148a1a2726cep-2, 0x1.e148a1a2726cep-2,
    0x1.e7b42c3ddad73p-2, 0x1.ee2a156b413e5p-2, 0x1.f4aa7ee03192dp-2,
    0x1.f4aa7ee03192dp-2, 0x1.fb358af7a4884p-2, 0x1.00e5ae5b207abp-1,
    0x1.04360be7603adp-1, 0x1.04360be7603adp-1, 0x1.078bf0533c568p-1,
    0x1.0ae76e2d054fap-1, 0x1.0ae76e2d0552dp-1, 0x1.0e4898611cce1p-1,
    0x1.11af823c75aa8p-1, 0x1.11af823c75aa8p-1, 0x1.151c3f6f29612p-1,
    0x1.188ee40f23cbbp-1, 0x1.188ee40f23ca6p-1, 0x1.1c07849ae6007p-1,
    0x1.1f8635fc6165cp-1, 0x1.1f8635fc61659p-1, 0x1.230b0d8bebc98p-1,
    0x1.269621134db92p-1, 0x1.269621134db92p-1, 0x1.2a2786d0ec105p-1,
    0x1.2dbf557b0df43p-1, 0x1.2dbf557b0df43p-1, 0x1.315da4434068bp-1,
    0x1.315da4434068bp-1, 0x1.35028ad9d8c86p-1, 0x1.38ae2171976e7p-1,
    0x1.38ae2171976e7p-1, 0x1.3c6080c36bfb5p-1, 0x1.3c6080c36bfb5p-1,
    0x1.4019c2125ca93p-1, 0x1.43d9ff2f923c5p-1, 0x1.43d9ff2f923c5p-1,
    0x1.47a1527e8a2d3p-1, 0x1.47a1527e8a2d3p-1, 0x1.4b6fd6f970c1fp-1,
    0x1.4b6fd6f970c1fp-1, 0x1.4f45a835a4e19p-1, 0x1.4f45a835a4e19p-1,
    0x1.5322e26867857p-1, 0x1.5322e26867857p-1, 0x1.5707a26bb8c66p-1,
    0x1.5707a26bb8c66p-1, 0x1.5af405c3649e0p-1, 0x1.5af405c3649e0p-1,
    0x1.5ee82aa241920p-1, 0x1.62e42fefa39eep-1};

} // namespace mathvec

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATHVEC_COMMON_CONSTANTS_H
