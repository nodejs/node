//===-- Implementation header for powf --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_POWF_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_POWF_H

#include "src/__support/macros/optimization.h"

#if defined(LIBC_MATH_HAS_SKIP_ACCURATE_PASS) &&                               \
    defined(LIBC_MATH_HAS_SMALL_TABLES)

#include "src/__support/math/powf_small_tables.h"

#else

#include "common_constants.h" // Lookup tables EXP_M1 and EXP_M2.
#include "exp10f.h"           // Speedup for powf(10, y) = exp10f(y)
#include "exp2f.h"            // Speedup for powf(2, y) = exp2f(y)
#include "exp_constants.h"

#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS && LIBC_MATH_HAS_SMALL_TABLES

#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/nearest_integer.h"
#include "src/__support/FPUtil/sqrt.h" // Speedup for powf(x, 1/2) = sqrtf(x)
#include "src/__support/FPUtil/triple_double.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

namespace powf_internal {

using fputil::DoubleDouble;
using fputil::TripleDouble;

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
alignas(16) LIBC_INLINE_VAR constexpr DoubleDouble LOG2_R_DD[128] = {
    {0.0, 0.0},
    {-0x1.177c23362928cp-25, 0x1.72c8p-7},
    {-0x1.179e0caa9c9abp-22, 0x1.744p-6},
    {-0x1.c6cea541f5b7p-23, 0x1.184cp-5},
    {-0x1.66c4d4e554434p-22, 0x1.773ap-5},
    {-0x1.70700a00fdd55p-24, 0x1.d6ecp-5},
    {0x1.53002a4e86631p-23, 0x1.1bb3p-4},
    {0x1.fcd15f101c142p-25, 0x1.4c56p-4},
    {0x1.25b3eed319cedp-22, 0x1.7d6p-4},
    {-0x1.4195120d8486fp-22, 0x1.960dp-4},
    {0x1.45b878e27d0d9p-23, 0x1.c7b5p-4},
    {0x1.770744593a4cbp-22, 0x1.f9c9p-4},
    {0x1.c673032495d24p-22, 0x1.097ep-3},
    {-0x1.1eaa65b49696ep-22, 0x1.22dbp-3},
    {0x1.b2866f2850b22p-22, 0x1.3c6f8p-3},
    {0x1.8ee37cd2ea9d3p-25, 0x1.494f8p-3},
    {0x1.7e86f9c2154fbp-24, 0x1.633a8p-3},
    {0x1.8e3cfc25f0ce6p-26, 0x1.7046p-3},
    {0x1.57f7a64ccd537p-28, 0x1.8a898p-3},
    {-0x1.a761c09fbd2aep-22, 0x1.97c2p-3},
    {0x1.24bea9a2c66f3p-22, 0x1.b26p-3},
    {-0x1.60002ccfe43f5p-25, 0x1.bfc68p-3},
    {0x1.69f220e97f22cp-22, 0x1.dac2p-3},
    {-0x1.6164f64c210ep-22, 0x1.e858p-3},
    {-0x1.0c1678ae89767p-24, 0x1.01d9cp-2},
    {-0x1.f26a05c813d57p-22, 0x1.08bdp-2},
    {0x1.4d8fc561c8d44p-24, 0x1.169cp-2},
    {-0x1.362ad8f7ca2dp-22, 0x1.1d984p-2},
    {0x1.2b13cd6c4d042p-22, 0x1.249ccp-2},
    {-0x1.1c8f11979a5dbp-22, 0x1.32cp-2},
    {0x1.c2ab3edefe569p-23, 0x1.39de8p-2},
    {0x1.7c3eca28e69cap-26, 0x1.4106p-2},
    {-0x1.34c4e99e1c6c6p-24, 0x1.4f6fcp-2},
    {-0x1.194a871b63619p-22, 0x1.56b24p-2},
    {0x1.e3dd5c1c885aep-23, 0x1.5dfdcp-2},
    {-0x1.6ccf3b1129b7cp-23, 0x1.6552cp-2},
    {-0x1.2f346e2bf924bp-23, 0x1.6cb1p-2},
    {-0x1.fa61aaa59c1d8p-23, 0x1.7b8ap-2},
    {0x1.90c11fd32a3abp-22, 0x1.8304cp-2},
    {0x1.57f7a64ccd537p-27, 0x1.8a898p-2},
    {0x1.249ba76fee235p-27, 0x1.9218p-2},
    {-0x1.aad2729b21ae5p-23, 0x1.99b08p-2},
    {0x1.71810a5e1818p-22, 0x1.a8ff8p-2},
    {-0x1.6172fe015e13cp-27, 0x1.b0b68p-2},
    {0x1.5ec6c1bfbf89ap-24, 0x1.b877cp-2},
    {0x1.678bf6cdedf51p-24, 0x1.c0438p-2},
    {0x1.c2d45fe43895ep-22, 0x1.c819cp-2},
    {-0x1.9ee52ed49d71dp-22, 0x1.cffbp-2},
    {0x1.5786af187a96bp-27, 0x1.d7e6cp-2},
    {0x1.3ab0dc56138c9p-23, 0x1.dfdd8p-2},
    {0x1.fe538ab34efb5p-22, 0x1.e7df4p-2},
    {-0x1.e4fee07aa4b68p-22, 0x1.efec8p-2},
    {-0x1.172f32fe67287p-22, 0x1.f804cp-2},
    {-0x1.9a83ff9ab9cc8p-22, 0x1.00144p-1},
    {-0x1.68cb06cece193p-22, 0x1.042bep-1},
    {0x1.8cd71ddf82e2p-22, 0x1.08494p-1},
    {0x1.5e18ab2df3ae6p-22, 0x1.0c6cap-1},
    {0x1.5dee4d9d8a273p-25, 0x1.1096p-1},
    {0x1.fcd15f101c142p-26, 0x1.14c56p-1},
    {-0x1.2474b0f992ba1p-23, 0x1.18faep-1},
    {0x1.4b5a92a606047p-24, 0x1.1d368p-1},
    {0x1.16186fcf54bbdp-22, 0x1.21786p-1},
    {0x1.18efabeb7d722p-27, 0x1.25c0ap-1},
    {-0x1.e5fc7d238691dp-24, 0x1.2a0f4p-1},
    {0x1.f5809faf6283cp-22, 0x1.2e644p-1},
    {0x1.f5809faf6283cp-22, 0x1.2e644p-1},
    {0x1.c6e1dcd0cb449p-22, 0x1.32bfep-1},
    {0x1.76e0e8f74b4d5p-22, 0x1.37222p-1},
    {-0x1.cb82c89692d99p-24, 0x1.3b8b2p-1},
    {-0x1.63161c5432aebp-22, 0x1.3ffaep-1},
    {0x1.458104c41b901p-22, 0x1.44716p-1},
    {0x1.458104c41b901p-22, 0x1.44716p-1},
    {-0x1.cd9d0cde578d5p-22, 0x1.48efp-1},
    {0x1.b9884591add87p-26, 0x1.4d738p-1},
    {0x1.c6042978605ffp-22, 0x1.51ff2p-1},
    {-0x1.fc4c96b37dcf6p-22, 0x1.56922p-1},
    {-0x1.2f346e2bf924bp-24, 0x1.5b2c4p-1},
    {-0x1.2f346e2bf924bp-24, 0x1.5b2c4p-1},
    {0x1.c4e4fbb68a4d1p-22, 0x1.5fcdcp-1},
    {-0x1.9d499bd9b3226p-23, 0x1.6476ep-1},
    {-0x1.f89b355ede26fp-23, 0x1.69278p-1},
    {-0x1.f89b355ede26fp-23, 0x1.69278p-1},
    {0x1.53c7e319f6e92p-24, 0x1.6ddfcp-1},
    {-0x1.b291f070528c7p-22, 0x1.729fep-1},
    {0x1.2967a451a7b48p-25, 0x1.7767cp-1},
    {0x1.2967a451a7b48p-25, 0x1.7767cp-1},
    {0x1.244fcff690fcep-22, 0x1.7c37ap-1},
    {0x1.46fd97f5dc572p-23, 0x1.810fap-1},
    {0x1.46fd97f5dc572p-23, 0x1.810fap-1},
    {-0x1.f3a7352663e5p-22, 0x1.85efep-1},
    {0x1.b3cda690370b5p-23, 0x1.8ad84p-1},
    {0x1.b3cda690370b5p-23, 0x1.8ad84p-1},
    {0x1.3226b211bf1d9p-23, 0x1.8fc92p-1},
    {0x1.d24b136c101eep-23, 0x1.94c28p-1},
    {0x1.d24b136c101eep-23, 0x1.94c28p-1},
    {0x1.7c40c7907e82ap-22, 0x1.99c48p-1},
    {-0x1.e81781d97ee91p-22, 0x1.9ecf6p-1},
    {-0x1.e81781d97ee91p-22, 0x1.9ecf6p-1},
    {-0x1.6a77813f94e01p-22, 0x1.a3e3p-1},
    {-0x1.1cfdeb43cfdp-22, 0x1.a8ffap-1},
    {-0x1.1cfdeb43cfdp-22, 0x1.a8ffap-1},
    {-0x1.f983f74d3138fp-23, 0x1.ae256p-1},
    {-0x1.e278ae1a1f51fp-23, 0x1.b3546p-1},
    {-0x1.e278ae1a1f51fp-23, 0x1.b3546p-1},
    {-0x1.97552b7b5ea45p-23, 0x1.b88ccp-1},
    {-0x1.97552b7b5ea45p-23, 0x1.b88ccp-1},
    {-0x1.19b4f3c72c4f8p-24, 0x1.bdceap-1},
    {0x1.f7402d26f1a12p-23, 0x1.c31a2p-1},
    {0x1.f7402d26f1a12p-23, 0x1.c31a2p-1},
    {-0x1.2056d5dd31d96p-23, 0x1.c86f8p-1},
    {-0x1.2056d5dd31d96p-23, 0x1.c86f8p-1},
    {-0x1.6e46335aae723p-24, 0x1.cdcecp-1},
    {-0x1.beb244c59f331p-22, 0x1.d3382p-1},
    {-0x1.beb244c59f331p-22, 0x1.d3382p-1},
    {0x1.16c071e93fd97p-27, 0x1.d8abap-1},
    {0x1.16c071e93fd97p-27, 0x1.d8abap-1},
    {0x1.d8175819530c2p-22, 0x1.de298p-1},
    {0x1.d8175819530c2p-22, 0x1.de298p-1},
    {0x1.51bd552842c1cp-23, 0x1.e3b2p-1},
    {0x1.51bd552842c1cp-23, 0x1.e3b2p-1},
    {0x1.914e204f19d94p-22, 0x1.e9452p-1},
    {0x1.914e204f19d94p-22, 0x1.e9452p-1},
    {0x1.c55d997da24fdp-22, 0x1.eee32p-1},
    {0x1.c55d997da24fdp-22, 0x1.eee32p-1},
    {-0x1.685c2d2298a6ep-22, 0x1.f48c4p-1},
    {-0x1.685c2d2298a6ep-22, 0x1.f48c4p-1},
    {0x1.7a4887bd74039p-22, 0x1.fa406p-1},
    {0.0, 1.0},
};

#else

#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
LIBC_INLINE_VAR constexpr uint64_t ERR = 64;
#else
LIBC_INLINE_VAR constexpr uint64_t ERR = 128;
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

// We choose the precision of the high part to be 53 - 24 - 8, so that when
//   y * (e_x + LOG2_R_DD[i].hi) is exact.
// Generated by Sollya with:
// > for i from 0 to 127 do {
//     r = 2^-8 * ceil(2^8 * (1 - 2^-8) / (1 + i * 2^-7) );
//     a = -log2(r);
//     b = round(1 + a, 53 - 24 - 8, RN) - 1;
//     c = round(a - b, D, RN);
//     d = round(a - b - c, D, RN);
//     print("{", d, ",", c, ", ", b, "},");
//    };
LIBC_INLINE_VAR constexpr TripleDouble LOG2_R_TD[128] = {
    {0.0, 0.0, 0.0},
    {0x1.84a2c615b70adp-79, -0x1.177c23362928cp-25, 0x1.72c8p-7},
    {-0x1.f27b820fd03eap-76, -0x1.179e0caa9c9abp-22, 0x1.744p-6},
    {-0x1.f27ef487c8f34p-77, -0x1.c6cea541f5b7p-23, 0x1.184cp-5},
    {-0x1.e3f80fbc71454p-76, -0x1.66c4d4e554434p-22, 0x1.773ap-5},
    {-0x1.9f8ef14d5f6eep-79, -0x1.70700a00fdd55p-24, 0x1.d6ecp-5},
    {0x1.452bbce7398c1p-77, 0x1.53002a4e86631p-23, 0x1.1bb3p-4},
    {-0x1.990555535afdp-81, 0x1.fcd15f101c142p-25, 0x1.4c56p-4},
    {0x1.447e30ad393eep-78, 0x1.25b3eed319cedp-22, 0x1.7d6p-4},
    {0x1.b7759da88a2dap-76, -0x1.4195120d8486fp-22, 0x1.960dp-4},
    {0x1.cee7766ece702p-78, 0x1.45b878e27d0d9p-23, 0x1.c7b5p-4},
    {-0x1.a55c745ecdc2fp-77, 0x1.770744593a4cbp-22, 0x1.f9c9p-4},
    {0x1.f7ec992caa67fp-77, 0x1.c673032495d24p-22, 0x1.097ep-3},
    {-0x1.433638c6ece3ep-77, -0x1.1eaa65b49696ep-22, 0x1.22dbp-3},
    {0x1.58f27b6518824p-76, 0x1.b2866f2850b22p-22, 0x1.3c6f8p-3},
    {-0x1.86bdcfdfd4a4cp-79, 0x1.8ee37cd2ea9d3p-25, 0x1.494f8p-3},
    {-0x1.ff7044a68a7fap-80, 0x1.7e86f9c2154fbp-24, 0x1.633a8p-3},
    {-0x1.aa21694561327p-81, 0x1.8e3cfc25f0ce6p-26, 0x1.7046p-3},
    {-0x1.d209f2d4239c6p-87, 0x1.57f7a64ccd537p-28, 0x1.8a898p-3},
    {-0x1.a55e97e60e632p-76, -0x1.a761c09fbd2aep-22, 0x1.97c2p-3},
    {0x1.261179225541ep-76, 0x1.24bea9a2c66f3p-22, 0x1.b26p-3},
    {-0x1.08fa30510fca9p-82, -0x1.60002ccfe43f5p-25, 0x1.bfc68p-3},
    {-0x1.63ec8d56242f9p-76, 0x1.69f220e97f22cp-22, 0x1.dac2p-3},
    {0x1.8bcdaf0534365p-76, -0x1.6164f64c210ep-22, 0x1.e858p-3},
    {0x1.1003282896056p-78, -0x1.0c1678ae89767p-24, 0x1.01d9cp-2},
    {0x1.01bcc7025fa92p-78, -0x1.f26a05c813d57p-22, 0x1.08bdp-2},
    {-0x1.fe8a8648e9ebcp-80, 0x1.4d8fc561c8d44p-24, 0x1.169cp-2},
    {0x1.08dfb23650c75p-79, -0x1.362ad8f7ca2dp-22, 0x1.1d984p-2},
    {-0x1.f8d5a89861a5ep-79, 0x1.2b13cd6c4d042p-22, 0x1.249ccp-2},
    {-0x1.a1c872983511ep-76, -0x1.1c8f11979a5dbp-22, 0x1.32cp-2},
    {0x1.e8e21bff3336bp-77, 0x1.c2ab3edefe569p-23, 0x1.39de8p-2},
    {0x1.fd1994fb2c4a1p-80, 0x1.7c3eca28e69cap-26, 0x1.4106p-2},
    {0x1.6b94b51cf76b1p-80, -0x1.34c4e99e1c6c6p-24, 0x1.4f6fcp-2},
    {-0x1.31d55da1d0f66p-76, -0x1.194a871b63619p-22, 0x1.56b24p-2},
    {-0x1.378b22691e28bp-77, 0x1.e3dd5c1c885aep-23, 0x1.5dfdcp-2},
    {0x1.99e302970e411p-83, -0x1.6ccf3b1129b7cp-23, 0x1.6552cp-2},
    {0x1.20164a049664dp-82, -0x1.2f346e2bf924bp-23, 0x1.6cb1p-2},
    {-0x1.d14aac4d864c3p-77, -0x1.fa61aaa59c1d8p-23, 0x1.7b8ap-2},
    {0x1.496ab4e4b293fp-79, 0x1.90c11fd32a3abp-22, 0x1.8304cp-2},
    {-0x1.d209f2d4239c6p-86, 0x1.57f7a64ccd537p-27, 0x1.8a898p-2},
    {0x1.eae3326327babp-81, 0x1.249ba76fee235p-27, 0x1.9218p-2},
    {0x1.fa05bddfded8cp-77, -0x1.aad2729b21ae5p-23, 0x1.99b08p-2},
    {-0x1.624140d175ba2p-77, 0x1.71810a5e1818p-22, 0x1.a8ff8p-2},
    {0x1.f1c5160c515c1p-81, -0x1.6172fe015e13cp-27, 0x1.b0b68p-2},
    {-0x1.86a6204eec8cp-79, 0x1.5ec6c1bfbf89ap-24, 0x1.b877cp-2},
    {0x1.718f761dd3915p-78, 0x1.678bf6cdedf51p-24, 0x1.c0438p-2},
    {-0x1.d4ee66c3700e4p-76, 0x1.c2d45fe43895ep-22, 0x1.c819cp-2},
    {-0x1.7d14533586306p-77, -0x1.9ee52ed49d71dp-22, 0x1.cffbp-2},
    {0x1.5ce9fb5a7bb5bp-81, 0x1.5786af187a96bp-27, 0x1.d7e6cp-2},
    {-0x1.ae6face57ad3bp-77, 0x1.3ab0dc56138c9p-23, 0x1.dfdd8p-2},
    {0x1.5ac93b443d55fp-78, 0x1.fe538ab34efb5p-22, 0x1.e7df4p-2},
    {0x1.f1753e0ae1e8fp-76, -0x1.e4fee07aa4b68p-22, 0x1.efec8p-2},
    {0x1.cdfd4c297069bp-76, -0x1.172f32fe67287p-22, 0x1.f804cp-2},
    {0x1.97a0e8f3ba742p-79, -0x1.9a83ff9ab9cc8p-22, 0x1.00144p-1},
    {-0x1.800450f5b2357p-78, -0x1.68cb06cece193p-22, 0x1.042bep-1},
    {-0x1.a839041241fe7p-78, 0x1.8cd71ddf82e2p-22, 0x1.08494p-1},
    {0x1.ed0b8eeccca86p-78, 0x1.5e18ab2df3ae6p-22, 0x1.0c6cap-1},
    {0x1.3dd41df9689b3p-79, 0x1.5dee4d9d8a273p-25, 0x1.1096p-1},
    {-0x1.990555535afdp-82, 0x1.fcd15f101c142p-26, 0x1.14c56p-1},
    {-0x1.1773d02c9055cp-77, -0x1.2474b0f992ba1p-23, 0x1.18faep-1},
    {-0x1.4aeef330c53c1p-78, 0x1.4b5a92a606047p-24, 0x1.1d368p-1},
    {0x1.8e6ff749ebacbp-77, 0x1.16186fcf54bbdp-22, 0x1.21786p-1},
    {0x1.c09d761c548ebp-84, 0x1.18efabeb7d722p-27, 0x1.25c0ap-1},
    {0x1.aaa73a428e1e4p-78, -0x1.e5fc7d238691dp-24, 0x1.2a0f4p-1},
    {-0x1.af2f3d8b63fbap-79, 0x1.f5809faf6283cp-22, 0x1.2e644p-1},
    {-0x1.af2f3d8b63fbap-79, 0x1.f5809faf6283cp-22, 0x1.2e644p-1},
    {0x1.78de359f2bb88p-77, 0x1.c6e1dcd0cb449p-22, 0x1.32bfep-1},
    {-0x1.415ae1a715618p-76, 0x1.76e0e8f74b4d5p-22, 0x1.37222p-1},
    {-0x1.4991b5375621fp-79, -0x1.cb82c89692d99p-24, 0x1.3b8b2p-1},
    {-0x1.827d37deb2236p-76, -0x1.63161c5432aebp-22, 0x1.3ffaep-1},
    {0x1.9576edac01c78p-77, 0x1.458104c41b901p-22, 0x1.44716p-1},
    {0x1.9576edac01c78p-77, 0x1.458104c41b901p-22, 0x1.44716p-1},
    {-0x1.05a27b81e2219p-77, -0x1.cd9d0cde578d5p-22, 0x1.48efp-1},
    {0x1.237616778b4bap-82, 0x1.b9884591add87p-26, 0x1.4d738p-1},
    {0x1.3b7d7e5d148bbp-76, 0x1.c6042978605ffp-22, 0x1.51ff2p-1},
    {-0x1.cc3f936a5977cp-79, -0x1.fc4c96b37dcf6p-22, 0x1.56922p-1},
    {0x1.20164a049664dp-83, -0x1.2f346e2bf924bp-24, 0x1.5b2c4p-1},
    {0x1.20164a049664dp-83, -0x1.2f346e2bf924bp-24, 0x1.5b2c4p-1},
    {-0x1.a212919a92f7ap-77, 0x1.c4e4fbb68a4d1p-22, 0x1.5fcdcp-1},
    {-0x1.b64b03f7230ddp-77, -0x1.9d499bd9b3226p-23, 0x1.6476ep-1},
    {-0x1.1ec6379e6e3b9p-77, -0x1.f89b355ede26fp-23, 0x1.69278p-1},
    {-0x1.1ec6379e6e3b9p-77, -0x1.f89b355ede26fp-23, 0x1.69278p-1},
    {-0x1.4ba44c03bfbbdp-78, 0x1.53c7e319f6e92p-24, 0x1.6ddfcp-1},
    {-0x1.c36fc650d030fp-77, -0x1.b291f070528c7p-22, 0x1.729fep-1},
    {-0x1.69e5693a7f067p-80, 0x1.2967a451a7b48p-25, 0x1.7767cp-1},
    {-0x1.69e5693a7f067p-80, 0x1.2967a451a7b48p-25, 0x1.7767cp-1},
    {0x1.6598aae91499ap-76, 0x1.244fcff690fcep-22, 0x1.7c37ap-1},
    {0x1.99d61ec432837p-77, 0x1.46fd97f5dc572p-23, 0x1.810fap-1},
    {0x1.99d61ec432837p-77, 0x1.46fd97f5dc572p-23, 0x1.810fap-1},
    {0x1.855c42078f81bp-76, -0x1.f3a7352663e5p-22, 0x1.85efep-1},
    {-0x1.59408e815107p-77, 0x1.b3cda690370b5p-23, 0x1.8ad84p-1},
    {-0x1.59408e815107p-77, 0x1.b3cda690370b5p-23, 0x1.8ad84p-1},
    {0x1.33b318085e50ap-78, 0x1.3226b211bf1d9p-23, 0x1.8fc92p-1},
    {0x1.343fe7c9cb4aep-79, 0x1.d24b136c101eep-23, 0x1.94c28p-1},
    {0x1.343fe7c9cb4aep-79, 0x1.d24b136c101eep-23, 0x1.94c28p-1},
    {-0x1.d19522e56fe6p-76, 0x1.7c40c7907e82ap-22, 0x1.99c48p-1},
    {-0x1.23b9d8ea55c3ep-77, -0x1.e81781d97ee91p-22, 0x1.9ecf6p-1},
    {-0x1.23b9d8ea55c3ep-77, -0x1.e81781d97ee91p-22, 0x1.9ecf6p-1},
    {0x1.829440c24aeb6p-78, -0x1.6a77813f94e01p-22, 0x1.a3e3p-1},
    {-0x1.624140d175ba2p-76, -0x1.1cfdeb43cfdp-22, 0x1.a8ffap-1},
    {-0x1.624140d175ba2p-76, -0x1.1cfdeb43cfdp-22, 0x1.a8ffap-1},
    {0x1.afa6f024fb045p-77, -0x1.f983f74d3138fp-23, 0x1.ae256p-1},
    {-0x1.603ad3a5d326dp-78, -0x1.e278ae1a1f51fp-23, 0x1.b3546p-1},
    {-0x1.603ad3a5d326dp-78, -0x1.e278ae1a1f51fp-23, 0x1.b3546p-1},
    {-0x1.0c1e0e5855d6ap-77, -0x1.97552b7b5ea45p-23, 0x1.b88ccp-1},
    {-0x1.0c1e0e5855d6ap-77, -0x1.97552b7b5ea45p-23, 0x1.b88ccp-1},
    {0x1.c817ad56baa16p-78, -0x1.19b4f3c72c4f8p-24, 0x1.bdceap-1},
    {0x1.44c47ac1bf62bp-77, 0x1.f7402d26f1a12p-23, 0x1.c31a2p-1},
    {0x1.44c47ac1bf62bp-77, 0x1.f7402d26f1a12p-23, 0x1.c31a2p-1},
    {-0x1.69b9465eae1e6p-78, -0x1.2056d5dd31d96p-23, 0x1.c86f8p-1},
    {-0x1.69b9465eae1e6p-78, -0x1.2056d5dd31d96p-23, 0x1.c86f8p-1},
    {-0x1.24a6d9d1d1904p-79, -0x1.6e46335aae723p-24, 0x1.cdcecp-1},
    {-0x1.3826144575ac4p-76, -0x1.beb244c59f331p-22, 0x1.d3382p-1},
    {-0x1.3826144575ac4p-76, -0x1.beb244c59f331p-22, 0x1.d3382p-1},
    {0x1.dbc96b3b12b25p-81, 0x1.16c071e93fd97p-27, 0x1.d8abap-1},
    {0x1.dbc96b3b12b25p-81, 0x1.16c071e93fd97p-27, 0x1.d8abap-1},
    {0x1.68a8ccdbd1f33p-77, 0x1.d8175819530c2p-22, 0x1.de298p-1},
    {0x1.68a8ccdbd1f33p-77, 0x1.d8175819530c2p-22, 0x1.de298p-1},
    {0x1.e586711df5ea1p-79, 0x1.51bd552842c1cp-23, 0x1.e3b2p-1},
    {0x1.e586711df5ea1p-79, 0x1.51bd552842c1cp-23, 0x1.e3b2p-1},
    {-0x1.bc25adf042483p-79, 0x1.914e204f19d94p-22, 0x1.e9452p-1},
    {-0x1.bc25adf042483p-79, 0x1.914e204f19d94p-22, 0x1.e9452p-1},
    {0x1.d7d82b65c5686p-76, 0x1.c55d997da24fdp-22, 0x1.eee32p-1},
    {0x1.d7d82b65c5686p-76, 0x1.c55d997da24fdp-22, 0x1.eee32p-1},
    {-0x1.3f108c0857ca3p-77, -0x1.685c2d2298a6ep-22, 0x1.f48c4p-1},
    {-0x1.3f108c0857ca3p-77, -0x1.685c2d2298a6ep-22, 0x1.f48c4p-1},
    {-0x1.bd800bca7a221p-78, 0x1.7a4887bd74039p-22, 0x1.fa406p-1},
    {0.0, 0.0, 1.0},
};

// Look up table for the second range reduction step:
// Generated by Sollya with:
// > for i from -64 to 128 do {
//     r = 2^-16 * nearestint(2^16 / (1 + i * 2^-14) );
//     a = -log2(r);
//     b = round(a, D, RN);
//     c = round(a - b, D, RN);
//     print("{", c, ", ", b, "},");
//    };
LIBC_INLINE_VAR constexpr DoubleDouble LOG2_R2_DD[] = {
    {0x1.ff25180953e64p-62, -0x1.720c2ab2312a9p-8},
    {-0x1.15ffd79560d8fp-62, -0x1.6c4c92b1478ffp-8},
    {0x1.b8d6d6f2e3579p-62, -0x1.668ce3c873549p-8},
    {-0x1.5bfc3f0d5ef71p-62, -0x1.60cd1df6fde91p-8},
    {-0x1.d1f7a8777984ap-64, -0x1.5b0d413c30b5ep-8},
    {0x1.8e858515b8343p-66, -0x1.554d4d97551abp-8},
    {0x1.e165c4014c1f2p-62, -0x1.4f8d4307b46ecp-8},
    {0x1.0f84b2cc14c7ep-63, -0x1.49cd218c9800bp-8},
    {0x1.de618ed0db9a6p-62, -0x1.440ce9254916cp-8},
    {-0x1.f6b8587e64f22p-62, -0x1.3e4c99d110ee7p-8},
    {-0x1.7f793c84cfa63p-64, -0x1.388c338f38bdp-8},
    {-0x1.7d7ecf6258c9ap-65, -0x1.32cbb65f09aeep-8},
    {-0x1.810bc5ac188f5p-62, -0x1.2d0b223fcce81p-8},
    {-0x1.950035fc5b67cp-62, -0x1.274a7730cb841p-8},
    {0x1.4f47f3048cdadp-62, -0x1.2189b5314e95dp-8},
    {0x1.269519861e298p-68, -0x1.1bc8dc409f279p-8},
    {-0x1.5c2b0a46a7e2fp-62, -0x1.1607ec5e063b3p-8},
    {0x1.5001ac8f0bda8p-63, -0x1.1046e588cccap-8},
    {0x1.106f246af5d41p-62, -0x1.0a85c7c03bc4ap-8},
    {0x1.82a00583b34bap-66, -0x1.0354423e3c666p-8},
    {0x1.b6f37deb3137p-65, -0x1.fb25e19f11aecp-9},
    {-0x1.44a2140444811p-63, -0x1.efa310d6550ecp-9},
    {0x1.f5e68a763133fp-63, -0x1.e4201220d4858p-9},
    {0x1.692083115f0b9p-63, -0x1.d89ce57d219a6p-9},
    {0x1.144bb17b9ac9cp-63, -0x1.cd198ae9cdc3dp-9},
    {0x1.ee7f086d32c05p-63, -0x1.c19602656a671p-9},
    {-0x1.d4f1167538dbep-63, -0x1.b6124bee88d82p-9},
    {0x1.7df8d226c67ep-63, -0x1.aa8e6783ba5a2p-9},
    {0x1.60545d61b9512p-63, -0x1.9f0a5523901ebp-9},
    {0x1.54c99c291702p-63, -0x1.938614cc9b468p-9},
    {-0x1.a7e678d7280dep-64, -0x1.8801a67d6ce1p-9},
    {-0x1.6d419bbeb223ap-64, -0x1.7c7d0a3495ec9p-9},
    {0x1.ce2b9892e27e9p-64, -0x1.70f83ff0a7565p-9},
    {-0x1.a4db4eff7bd61p-63, -0x1.657347b031fa2p-9},
    {0x1.5bb04682fab82p-63, -0x1.59ee2171c6a2fp-9},
    {-0x1.78b8bfe6a3adep-64, -0x1.4e68cd33f60a3p-9},
    {0x1.574c3ce9b89b1p-63, -0x1.42e34af550d87p-9},
    {0x1.08fb216647b7bp-63, -0x1.375d9ab467a4dp-9},
    {0x1.ed5a50e7b919cp-66, -0x1.2bd7bc6fcaf56p-9},
    {0x1.91ad7a23f86fep-63, -0x1.2051b0260b3fp-9},
    {0x1.3ab2c932b8b0ap-64, -0x1.14cb75d5b8e54p-9},
    {-0x1.c63bcdf120f7ap-63, -0x1.09450d7d643a9p-9},
    {0x1.8af8c4ab4e82dp-64, -0x1.fb7cee373b008p-10},
    {0x1.a52c2ca9d8b9bp-65, -0x1.e46f655de9cc6p-10},
    {-0x1.460b177a58742p-64, -0x1.cd61806bf5166p-10},
    {0x1.611089de8d12ap-66, -0x1.b6533f5e7cf9bp-10},
    {-0x1.4209853cee70cp-69, -0x1.9f44a232a16eep-10},
    {0x1.964e032541a28p-64, -0x1.8835a8e5824c3p-10},
    {-0x1.fa9f94392637bp-66, -0x1.712653743f454p-10},
    {-0x1.3293693721a53p-64, -0x1.5a16a1dbf7eb6p-10},
    {-0x1.6e2af03c83c6ep-68, -0x1.43069419cbad5p-10},
    {-0x1.b5f05b9d5bd29p-65, -0x1.2bf62a2ad9d74p-10},
    {0x1.3db883c072f72p-64, -0x1.14e5640c4193p-10},
    {-0x1.a675a1c045304p-68, -0x1.fba8837643cf6p-11},
    {0x1.3b9c2aeb00068p-66, -0x1.cd85866933743p-11},
    {-0x1.2911a381901ebp-66, -0x1.9f61d0eb8f98bp-11},
    {-0x1.5ea75a74def03p-68, -0x1.713d62f7957c3p-11},
    {-0x1.305b92f93ffep-67, -0x1.43183c878218dp-11},
    {0x1.b7c8c8dd40d35p-68, -0x1.14f25d959223ap-11},
    {0x1.dc915d58a62f6p-66, -0x1.cd978c3804191p-12},
    {0x1.c7bc3fe53cd94p-66, -0x1.7148ec2a1bfc9p-12},
    {-0x1.427ce595cc53cp-67, -0x1.14f8daf5e3bcfp-12},
    {-0x1.d523885ac824cp-67, -0x1.714eb11fa5363p-13},
    {-0x1.945957f63330ap-69, -0x1.715193b17d35dp-14},
    {0, 0},
    {-0x1.88fb2ea8bf9eap-70, 0x1.7157590356aeep-14},
    {-0x1.5aeaee345d04ep-68, 0x1.715a3bc3593d5p-13},
    {-0x1.7fce430230132p-66, 0x1.1505d6ee104c5p-12},
    {-0x1.9a480f204ff09p-70, 0x1.716001718cb2bp-12},
    {-0x1.00e7233f2d8bdp-68, 0x1.cdbb9d77ae5a8p-12},
    {0x1.09d379fa18c5dp-67, 0x1.150c5586012b8p-11},
    {0x1.b6b9d90a104d3p-65, 0x1.433b951d0b231p-11},
    {0x1.4d9a3ea651885p-65, 0x1.716b8d86bc285p-11},
    {-0x1.7590b3a76f0f9p-67, 0x1.9f9c3ec8db94fp-11},
    {0x1.f183ca5b21bfep-65, 0x1.cdcda8e93107fp-11},
    {-0x1.a7e3465ba127p-66, 0x1.fbffcbed8465fp-11},
    {-0x1.7821f738d1221p-64, 0x1.151953edceec6p-10},
    {0x1.3bb4c0fb95359p-65, 0x1.2c331e5ca2e7dp-10},
    {0x1.236028e962f8p-64, 0x1.434d4546227fcp-10},
    {0x1.aaaa64d30f184p-66, 0x1.5a67c8ad32315p-10},
    {-0x1.a821b7cc57a7ap-64, 0x1.7182a894b69c6p-10},
    {-0x1.13d9d78aace21p-64, 0x1.889de4ff94838p-10},
    {-0x1.2f249a6b923ap-64, 0x1.9fb97df0b0cc2p-10},
    {-0x1.d47dc3664be7ap-68, 0x1.b6d5736af07e6p-10},
    {0x1.bd1522c6418fbp-64, 0x1.cdf1c57138c53p-10},
    {-0x1.bacdbb22d2163p-64, 0x1.e50e74066eee6p-10},
    {-0x1.ca7604812d77bp-64, 0x1.fc2b7f2d786a5p-10},
    {-0x1.2b6832f8830bfp-63, 0x1.09a473749d663p-9},
    {0x1.4e712033d0457p-65, 0x1.1533559e4de55p-9},
    {-0x1.473dd044017b5p-66, 0x1.20c26615409f1p-9},
    {-0x1.e033bcac726d3p-63, 0x1.2c51a4dae8915p-9},
    {-0x1.4a47a2b18a0fap-63, 0x1.37e111f0b8cb5p-9},
    {0x1.6f3615771c17bp-66, 0x1.4370ad58246ddp-9},
    {0x1.c0ee6c32d6236p-65, 0x1.4f0077129eabp-9},
    {0x1.fa94c99761b8fp-64, 0x1.5a906f219ac67p-9},
    {-0x1.979e6b473fbf8p-64, 0x1.662095868c153p-9},
    {0x1.30edde8d24c7bp-64, 0x1.71b0ea42e5fdap-9},
    {-0x1.d01594fe1421cp-64, 0x1.7d416d581bf7cp-9},
    {0x1.50bf7b995b49ap-63, 0x1.88d21ec7a18cdp-9},
    {-0x1.28ea2bcec5018p-63, 0x1.9462fe92ea57cp-9},
    {0x1.ed6add489c30bp-65, 0x1.9ff40cbb6a04bp-9},
    {0x1.201d5c3bbeb69p-64, 0x1.ab85494294517p-9},
    {-0x1.a05d0d4461ea9p-64, 0x1.b716b429dd0d3p-9},
    {-0x1.7c974c8a392fdp-63, 0x1.c2a84d72b8189p-9},
    {-0x1.f068238451bdep-64, 0x1.ce3a151e9965bp-9},
    {-0x1.5e4d95c6259c3p-66, 0x1.d9cc0b2ef4f83p-9},
    {-0x1.1fc262efaad6cp-63, 0x1.e55e2fa53ee53p-9},
    {0x1.49eee7abc7716p-63, 0x1.f0f08282eb533p-9},
    {-0x1.903de284d2782p-65, 0x1.fc8303c96e7a6p-9},
    {-0x1.ec564845134cbp-63, 0x1.040ad9bd1e522p-8},
    {-0x1.7692b7791cf1fp-66, 0x1.0861eadabc3dcp-8},
    {-0x1.37829afb11c1p-62, 0x1.0e2b6b51e4f7ep-8},
    {0x1.6706b91c3b0bap-62, 0x1.13f5030033459p-8},
    {-0x1.7558ccd710756p-62, 0x1.19beb1e6616c9p-8},
    {0x1.79f72a5bbe9dep-62, 0x1.1f88780529bb1p-8},
    {-0x1.e1297c110b25p-62, 0x1.2552555d46886p-8},
    {0x1.29930d567ca26p-62, 0x1.2b1c49ef72343p-8},
    {0x1.a08cbd7592a17p-65, 0x1.30e655bc67275p-8},
    {0x1.e4f9d4ac5db83p-62, 0x1.36b078c4dfd31p-8},
    {-0x1.ed1b0aafd30c2p-62, 0x1.3c7ab30996b1cp-8},
    {0x1.e78f0aa014b32p-62, 0x1.4245048b46462p-8},
    {0x1.8594548038a0fp-69, 0x1.480f6d4aa91c2p-8},
    {0x1.3df498168a333p-63, 0x1.4dd9ed4879c82p-8},
    {0x1.b1c502544f82ap-62, 0x1.53a4848572e77p-8},
    {-0x1.dc50552fe0da9p-63, 0x1.596f33024f203p-8},
    {-0x1.671d85c357d5ep-62, 0x1.5f39f8bfc9212p-8},
    {0x1.1c670cabccefap-64, 0x1.6504d5be9ba1ep-8},
    {-0x1.9983a9e98f318p-62, 0x1.6acfc9ff8162fp-8},
    {0x1.ae1a26af3eebep-62, 0x1.709ad583352d6p-8},
    {0x1.655eb510bfda3p-62, 0x1.7665f84a71d35p-8},
    {-0x1.e287bc0192e15p-64, 0x1.7c313255f22f8p-8},
    {0x1.cc4944139ccbfp-63, 0x1.81fc83a671257p-8},
    {0x1.4e09b4cb8645bp-62, 0x1.87c7ec3ca9a19p-8},
    {-0x1.5becc991e3a5fp-64, 0x1.8d936c1956991p-8},
    {-0x1.ddfa3f1e15ba8p-62, 0x1.935f033d3309ep-8},
    {-0x1.b7b06ea3fb362p-62, 0x1.992ab1a8f9facp-8},
    {0x1.32d614904e46cp-62, 0x1.9ef6775d667b4p-8},
    {-0x1.7186892b5bfaep-64, 0x1.a4c2545b33a3ep-8},
    {-0x1.d4de10b28dfd8p-62, 0x1.aa8e48a31c95cp-8},
    {0x1.4bb4b3bdc8175p-62, 0x1.b05a5435dc7adp-8},
    {0x1.9cedbd1d7fba5p-62, 0x1.b62677142e86p-8},
    {-0x1.0ed3379beaffdp-66, 0x1.bbf2b13ecdf2fp-8},
    {0x1.6e86a125567a6p-62, 0x1.c1bf02b67606p-8},
    {-0x1.35038e0c0a52cp-62, 0x1.c6184f1b326d9p-8},
    {0x1.05ef8bf5adf5ep-67, 0x1.cbe4c95b6c5abp-8},
    {-0x1.b7338b99a6b26p-65, 0x1.d1b15aeab217cp-8},
    {0x1.9e901c30c427ep-63, 0x1.d77e03c9bf0a4p-8},
    {-0x1.1f28a9c0b3d47p-62, 0x1.dd4ac3f94ea0ap-8},
    {-0x1.140ef760d3b63p-62, 0x1.e3179b7a1c52p-8},
    {-0x1.ab65b1037f517p-63, 0x1.e8e48a4ce39e7p-8},
    {-0x1.76940c457ce6dp-63, 0x1.eeb19072600edp-8},
    {0x1.da3ae65a605cfp-64, 0x1.f47eadeb4d34dp-8},
    {0x1.b15d0bce2ede6p-62, 0x1.fa4be2b866abp-8},
    {0x1.e02aa1fa9dc57p-61, 0x1.000c976d340a6p-7},
    {0x1.6be971a5565b9p-62, 0x1.02f34929068f3p-7},
    {-0x1.8a9319a6ed164p-64, 0x1.05da069008be7p-7},
    {0x1.825079f1e0ec5p-62, 0x1.08c0cfa298771p-7},
    {0x1.60d5749321466p-63, 0x1.0ba7a461139c8p-7},
    {-0x1.5b8f4c479e2ep-61, 0x1.0e8e84cbd8169p-7},
    {-0x1.e3e1248004e29p-62, 0x1.117570e343d17p-7},
    {0x1.9ac06487c375p-63, 0x1.145c68a7b4bddp-7},
    {0x1.f657ea5c03ea4p-62, 0x1.17436c1988d0dp-7},
    {-0x1.5a965659a05e2p-61, 0x1.1a2a7b391e04p-7},
    {-0x1.21ce9b9bfc512p-61, 0x1.1d119606d2554p-7},
    {-0x1.30fda247ad0e1p-61, 0x1.1ff8bc8303c7p-7},
    {-0x1.382c78a45cdeap-62, 0x1.22dfeeae10601p-7},
    {0x1.46ae4a64073d4p-61, 0x1.250d5bf952374p-7},
    {-0x1.dcad2cec3b84bp-62, 0x1.27f4a29740a2fp-7},
    {-0x1.413fbeb0b0635p-61, 0x1.2adbf4e50cdf9p-7},
    {0x1.f28e6a48bcb9p-61, 0x1.2dc352e315049p-7},
    {-0x1.96f286e1eb086p-61, 0x1.30aabc91b72ep-7},
    {-0x1.f88c04206dfa1p-61, 0x1.339231f1517c1p-7},
    {-0x1.11ea20e195841p-61, 0x1.3679b30242139p-7},
    {-0x1.d6e71452b674ap-63, 0x1.39613fc4e71dcp-7},
    {-0x1.57c578233b1b3p-61, 0x1.3c48d8399ec85p-7},
    {-0x1.ec430f03b76ep-63, 0x1.3f307c60c7455p-7},
    {0x1.e00dd1902ffb9p-61, 0x1.42182c3abecb5p-7},
    {-0x1.f22bcd96afe38p-61, 0x1.44ffe7c7e3957p-7},
    {0x1.08fd90f841d3p-61, 0x1.47e7af0893e2fp-7},
    {0x1.09594c5552bccp-62, 0x1.4acf81fd2df7ep-7},
    {-0x1.01a8a652e5602p-61, 0x1.4db760a6101c9p-7},
    {-0x1.826168febb3dp-64, 0x1.509f4b03989dcp-7},
    {-0x1.7eb21a35021e3p-62, 0x1.5387411625cccp-7},
    {-0x1.66cbc818e175p-61, 0x1.566f42de15ff4p-7},
    {0x1.9b784dd6cebdap-64, 0x1.5957505bc78f6p-7},
    {0x1.2b121ab482456p-61, 0x1.5b8562298c65bp-7},
    {-0x1.5d29869dd8233p-62, 0x1.5e6d842633702p-7},
    {-0x1.572a1b6cd63cfp-61, 0x1.6155b1d99f672p-7},
    {-0x1.a1f355360e877p-62, 0x1.643deb442eb59p-7},
    {-0x1.b6f1cd2e1c03fp-61, 0x1.672630663fcadp-7},
    {-0x1.2aaa11ccddcaep-61, 0x1.6a0e8140311aap-7},
    {0x1.3d979ddf4746cp-61, 0x1.6cf6ddd2611d4p-7},
    {-0x1.dc930484501f8p-63, 0x1.6fdf461d2e4f8p-7},
};
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

LIBC_INLINE constexpr bool is_odd_integer(float x) {
  using FPBits = typename fputil::FPBits<float>;
  uint32_t x_u = cpp::bit_cast<uint32_t>(x);
  int32_t x_e =
      static_cast<int32_t>((x_u & FPBits::EXP_MASK) >> FPBits::FRACTION_LEN);
  int32_t lsb = cpp::countr_zero(x_u | FPBits::EXP_MASK);
  constexpr int32_t UNIT_EXPONENT =
      FPBits::EXP_BIAS + static_cast<int32_t>(FPBits::FRACTION_LEN);
  return (x_e + lsb == UNIT_EXPONENT);
}

LIBC_INLINE constexpr bool is_integer(float x) {
  using FPBits = typename fputil::FPBits<float>;
  uint32_t x_u = cpp::bit_cast<uint32_t>(x);
  int32_t x_e =
      static_cast<int32_t>((x_u & FPBits::EXP_MASK) >> FPBits::FRACTION_LEN);
  int32_t lsb = cpp::countr_zero(x_u | FPBits::EXP_MASK);
  constexpr int32_t UNIT_EXPONENT =
      FPBits::EXP_BIAS + static_cast<int32_t>(FPBits::FRACTION_LEN);
  return (x_e + lsb >= UNIT_EXPONENT);
}

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
LIBC_INLINE constexpr bool larger_exponent(double a, double b) {
  using DoubleBits = typename fputil::FPBits<double>;
  return DoubleBits(a).get_biased_exponent() >=
         DoubleBits(b).get_biased_exponent();
}

// Calculate 2^(y * log2(x)) in double-double precision.
// At this point we can reuse the following values:
//   idx_x: index for extra precision of log2 for the middle part of log2(x).
//   dx: the reduced argument for log2(x)
//   y6: 2^6 * y.
//   lo6_hi: the high part of 2^6 * (y - (hi + mid))
//   exp2_hi_mid: high part of 2^(hi + mid)
LIBC_INLINE double powf_double_double(int idx_x, double dx, double y6,
                                      double lo6_hi,
                                      const DoubleDouble &exp2_hi_mid) {
  using DoubleBits = typename fputil::FPBits<double>;

  // Perform a second range reduction step:
  //   idx2 = round(2^14 * (dx  + 2^-8)) = round ( dx * 2^14 + 2^6)
  //   dx2 = (1 + dx) * r2 - 1
  // Output range:
  //   -0x1.3ffcp-15 <= dx2 <= 0x1.3e3dp-15
  int idx2 = static_cast<int>(
      fputil::nearest_integer(fputil::multiply_add(dx, 0x1.0p14, 0x1.0p6)));
  double dx2 = fputil::multiply_add(
      1.0 + dx, common_constants_internal::R2[idx2], -1.0); // Exact

  // Degree-5 polynomial approximation of log2(1 + x)/x in double-double
  // precision.  Generate by Solya with:
  // > P = fpminimax(log2(1 + x)/x, 5, [|DD...|],
  //                 [-0x1.3ffcp-15, 0x1.3e3dp-15]);
  // > dirtyinfnorm(log2(1 + x)/x - P, [-0x1.3ffcp-15, 0x1.3e3dp-15]);
  // 0x1.8be5...p-96.
  constexpr DoubleDouble COEFFS[] = {
      {0x1.777d0ffda25ep-56, 0x1.71547652b82fep0},
      {-0x1.777d101cf0a84p-57, -0x1.71547652b82fep-1},
      {0x1.ce04b5140d867p-56, 0x1.ec709dc3a03fdp-2},
      {0x1.137b47e635be5p-56, -0x1.71547652b82fbp-2},
      {-0x1.b5a30b3bdb318p-58, 0x1.2776c516a92a2p-2},
      {0x1.2d2fbd081e657p-57, -0x1.ec70af1929ca6p-3},
  };

  DoubleDouble dx_dd({0.0, dx2});
  DoubleDouble p = fputil::polyeval(dx_dd, COEFFS[0], COEFFS[1], COEFFS[2],
                                    COEFFS[3], COEFFS[4], COEFFS[5]);
  // log2(1 + dx2) ~ dx2 * P(dx2)
  DoubleDouble log2_x_lo = fputil::quick_mult(dx2, p);
  // Lower parts of (e_x - log2(r1)) of the first range reduction constant
  DoubleDouble log2_x_mid({LOG2_R_TD[idx_x].lo, LOG2_R_TD[idx_x].mid});
  // -log2(r2) + lower part of (e_x - log2(r1))
  DoubleDouble log2_x_m = fputil::add(LOG2_R2_DD[idx2], log2_x_mid);
  // log2(1 + dx2) - log2(r2) + lower part of (e_x - log2(r1))
  // Since we don't know which one has larger exponent to apply Fast2Sum
  // algorithm, we need to check them before calling double-double addition.
  DoubleDouble log2_x = larger_exponent(log2_x_m.hi, log2_x_lo.hi)
                            ? fputil::add(log2_x_m, log2_x_lo)
                            : fputil::add(log2_x_lo, log2_x_m);
  DoubleDouble lo6_hi_dd({0.0, lo6_hi});
  // 2^6 * y * (log2(1 + dx2) - log2(r2) + lower part of (e_x - log2(r1)))
  DoubleDouble prod = fputil::quick_mult(y6, log2_x);
  // 2^6 * (y * log2(x) - (hi + mid)) = 2^6 * lo
  DoubleDouble lo6 = larger_exponent(prod.hi, lo6_hi)
                         ? fputil::add(prod, lo6_hi_dd)
                         : fputil::add(lo6_hi_dd, prod);

  constexpr DoubleDouble EXP2_COEFFS[] = {
      {0, 0x1p0},
      {0x1.abc9e3b398024p-62, 0x1.62e42fefa39efp-7},
      {-0x1.5e43a5429bddbp-69, 0x1.ebfbdff82c58fp-15},
      {-0x1.d33162491268fp-77, 0x1.c6b08d704a0cp-23},
      {0x1.4fb32d240a14ep-86, 0x1.3b2ab6fba4e77p-31},
      {0x1.e84e916be83ep-97, 0x1.5d87fe78a6731p-40},
      {-0x1.9a447bfddc5e6p-103, 0x1.430912f86bfb8p-49},
      {-0x1.31a55719de47fp-113, 0x1.ffcbfc588ded9p-59},
      {-0x1.0ba57164eb36bp-122, 0x1.62c034beb8339p-68},
      {-0x1.8483eabd9642dp-132, 0x1.b5251ff97bee1p-78},
  };

  DoubleDouble pp = fputil::polyeval(
      lo6, EXP2_COEFFS[0], EXP2_COEFFS[1], EXP2_COEFFS[2], EXP2_COEFFS[3],
      EXP2_COEFFS[4], EXP2_COEFFS[5], EXP2_COEFFS[6], EXP2_COEFFS[7],
      EXP2_COEFFS[8], EXP2_COEFFS[9]);
  DoubleDouble rr = fputil::quick_mult(exp2_hi_mid, pp);

  // Make sure the sum is normalized:
  DoubleDouble r = fputil::exact_add(rr.hi, rr.lo);
  // Round to odd.
  uint64_t r_bits = cpp::bit_cast<uint64_t>(r.hi);
  if (LIBC_UNLIKELY(((r_bits & 0xfff'ffff) == 0) && (r.lo != 0.0))) {
    Sign hi_sign = DoubleBits(r.hi).sign();
    Sign lo_sign = DoubleBits(r.lo).sign();
    if (hi_sign == lo_sign) {
      ++r_bits;
    } else if ((r_bits & DoubleBits::FRACTION_MASK) > 0) {
      --r_bits;
    }
  }

  return cpp::bit_cast<double>(r_bits);
}
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

} // namespace powf_internal

LIBC_INLINE float powf(float x, float y) {
  using namespace powf_internal;
  using FloatBits = typename fputil::FPBits<float>;
  using DoubleBits [[maybe_unused]] = typename fputil::FPBits<double>;

  FloatBits xbits(x), ybits(y);

  uint32_t x_u = xbits.uintval();
  uint32_t x_abs = xbits.abs().uintval();
  uint32_t y_u = ybits.uintval();
  uint32_t y_abs = ybits.abs().uintval();

  ///////// BEGIN - Check exceptional cases ////////////////////////////////////

  // The single precision number that is closest to 1 is (1 - 2^-24), which has
  //   log2(1 - 2^-24) ~ -1.715...p-24.
  // So if |y| > 151 * 2^24, and x is finite:
  //   |y * log2(x)| = 0 or > 151.
  // Hence x^y will either overflow or underflow if x is not zero.
  if (LIBC_UNLIKELY((y_abs & 0x0007'ffff) == 0) || (y_abs > 0x4f170000)) {
    // y is signaling NaN
    if (xbits.is_signaling_nan() || ybits.is_signaling_nan()) {
      fputil::raise_except_if_required(FE_INVALID);
      return FloatBits::quiet_nan().get_val();
    }

    // Exceptional exponents.
    if (y == 0.0f)
      return 1.0f;

    switch (y_abs) {
    case 0x7f80'0000: { // y = +-Inf
      if (x_abs > 0x7f80'0000) {
        // pow(NaN, +-Inf) = NaN
        return x;
      }
      if (x_abs == 0x3f80'0000) {
        // pow(+-1, +-Inf) = 1.0f
        return 1.0f;
      }
      if (x == 0.0f && y_u == 0xff80'0000) {
        // pow(+-0, -Inf) = +inf and raise FE_DIVBYZERO
        fputil::set_errno_if_required(EDOM);
        fputil::raise_except_if_required(FE_DIVBYZERO);
        return FloatBits::inf().get_val();
      }
      // pow (|x| < 1, -inf) = +inf
      // pow (|x| < 1, +inf) = 0.0f
      // pow (|x| > 1, -inf) = 0.0f
      // pow (|x| > 1, +inf) = +inf
      return ((x_abs < 0x3f80'0000) == (y_u == 0xff80'0000))
                 ? FloatBits::inf().get_val()
                 : 0.0f;
    }
    default:
      // Speed up for common exponents
      float r = fputil::sqrt<float>(x);
      switch (y_u) {
      case 0x3f00'0000: // y = 0.5f
        // pow(x, 1/2) = sqrt(x)
        if (LIBC_UNLIKELY(x == 0.0f || x_u == 0xff80'0000)) {
          // pow(-0, 1/2) = +0
          // pow(-inf, 1/2) = +inf
          // Make sure it is correct for FTZ/DAZ.
          return x * x;
        }
        return (FloatBits(r).uintval() != 0x8000'0000) ? r : 0.0f;
      case 0x3f80'0000: // y = 1.0f
        return x;
      case 0x4000'0000: // y = 2.0f
        // pow(x, 2) = x^2
        return x * x;
        // TODO: Enable special case speed-up for x^(-1/2) when rsqrt is ready.
        // case 0xbf00'0000:  // pow(x, -1/2) = rsqrt(x)
        //   return rsqrt(x);
      }
      if (is_integer(y) && (y_u > 0x4000'0000) && (y_u <= 0x41c0'0000)) {
        // Check for exact cases when 2 < y < 25 and y is an integer.
        int msb =
            (x_abs == 0) ? (FloatBits::TOTAL_LEN - 2) : cpp::countl_zero(x_abs);
        msb = (msb > FloatBits::EXP_LEN) ? msb : FloatBits::EXP_LEN;
        int lsb = (x_abs == 0) ? 0 : cpp::countr_zero(x_abs);
        lsb = (lsb > FloatBits::FRACTION_LEN) ? FloatBits::FRACTION_LEN : lsb;
        int extra_bits = FloatBits::TOTAL_LEN - 2 - lsb - msb;
        int iter = static_cast<int>(y);

        if (extra_bits * iter <= FloatBits::FRACTION_LEN + 2) {
          // The result is either exact or exactly half-way.
          // But it is exactly representable in double precision.
          double x_d = static_cast<double>(x);
          double result = x_d;
          for (int i = 1; i < iter; ++i)
            result *= x_d;
          return static_cast<float>(result);
        }
      }
      if (y_abs > 0x4f17'0000) {
        // if y is NaN
        if (y_abs > 0x7f80'0000) {
          if (x_u == 0x3f80'0000) { // x = 1.0f
            // pow(1, NaN) = 1
            return 1.0f;
          }
          // pow(x, NaN) = NaN
          return y;
        }
        // x^y will be overflow / underflow in single precision.  Set y to a
        // large enough exponent but not too large, so that the computations
        // won't be overflow in double precision.
        y = cpp::bit_cast<float>((y_u & FloatBits::SIGN_MASK) + 0x4f800000U);
      }
    }
  }

  int ex = -FloatBits::EXP_BIAS;
  uint64_t sign = 0;

  // y is finite and non-zero.
  if (LIBC_UNLIKELY(((x_u & 0x801f'ffffU) == 0) || x_u >= 0x7f80'0000U ||
                    x_u < 0x0080'0000U)) {
    // if x is signaling NaN
    if (xbits.is_signaling_nan()) {
      fputil::raise_except_if_required(FE_INVALID);
      return FloatBits::quiet_nan().get_val();
    }

    switch (x_u) {
    case 0x3f80'0000: // x = 1.0f
      return 1.0f;
      // TODO: Put these 2 entrypoint dependency under control flag.
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    case 0x4000'0000: // x = 2.0f
      // pow(2, y) = exp2(y)
      return math::exp2f(y);
    case 0x4120'0000: // x = 10.0f
      // pow(10, y) = exp10(y)
      return math::exp10f(y);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    }

    const bool x_is_neg = x_u >= FloatBits::SIGN_MASK;

    if (x == 0.0f) {
      const bool out_is_neg =
          x_is_neg && is_odd_integer(FloatBits(y_u).get_val());
      if (y_u > 0x8000'0000U) {
        // pow(0, negative number) = inf
        fputil::set_errno_if_required(EDOM);
        fputil::raise_except_if_required(FE_DIVBYZERO);
        return FloatBits::inf(out_is_neg ? Sign::NEG : Sign::POS).get_val();
      }
      // pow(0, positive number) = 0
      return out_is_neg ? -0.0f : 0.0f;
    }

    if (x_abs == 0x7f80'0000) {
      // x = +-Inf
      const bool out_is_neg =
          x_is_neg && is_odd_integer(FloatBits(y_u).get_val());
      if (y_u >= FloatBits::SIGN_MASK) {
        return out_is_neg ? -0.0f : 0.0f;
      }
      return FloatBits::inf(out_is_neg ? Sign::NEG : Sign::POS).get_val();
    }

    if (x_abs > 0x7f80'0000) {
      // x is NaN.
      // pow (aNaN, 0) is already taken care above.
      return x;
    }

    // Normalize denormal inputs.
    if (x_abs < 0x0080'0000U) {
      ex -= 64;
      x *= 0x1.0p64f;
    }

    // x is finite and negative, and y is a finite integer.
    if (x_is_neg) {
      if (is_integer(y)) {
        x = -x;
        if (is_odd_integer(y)) {
          // sign = -1.0;
          sign = 0x8000'0000'0000'0000ULL;
        }
      } else {
        // pow( negative, non-integer ) = NaN
        fputil::set_errno_if_required(EDOM);
        fputil::raise_except_if_required(FE_INVALID);
        return FloatBits::quiet_nan().get_val();
      }
    }
  }

  ///////// END - Check exceptional cases //////////////////////////////////////

#if defined(LIBC_MATH_HAS_SKIP_ACCURATE_PASS) &&                               \
    defined(LIBC_MATH_HAS_SMALL_TABLES)
  return powf_small_tables(x, ex, sign, y);
#else
  using namespace common_constants_internal;

  // x^y = 2^( y * log2(x) )
  //     = 2^( y * ( e_x + log2(m_x) ) )
  // First we compute log2(x) = e_x + log2(m_x)
  x_u = FloatBits(x).uintval();

  // Extract exponent field of x.
  ex += (x_u >> FloatBits::FRACTION_LEN);
  double e_x = static_cast<double>(ex);
  // Use the highest 7 fractional bits of m_x as the index for look up tables.
  uint32_t x_mant = x_u & FloatBits::FRACTION_MASK;
  int idx_x = static_cast<int>(x_mant >> (FloatBits::FRACTION_LEN - 7));
  // Add the hidden bit to the mantissa.
  // 1 <= m_x < 2
  float m_x = cpp::bit_cast<float>(x_mant | 0x3f800000);

  // Reduced argument for log2(m_x):
  //   dx = r * m_x - 1.
  // The computation is exact, and -2^-8 <= dx < 2^-7.
  // Then m_x = (1 + dx) / r, and
  //   log2(m_x) = log2( (1 + dx) / r )
  //             = log2(1 + dx) - log2(r).
#ifdef LIBC_TARGET_CPU_HAS_FMA_FLOAT
  double dx =
      static_cast<double>(fputil::multiply_add(m_x, R[idx_x], -1.0f)); // Exact
#else
  double dx =
      fputil::multiply_add(static_cast<double>(m_x), RD[idx_x], -1.0); // Exact
#endif // LIBC_TARGET_CPU_HAS_FMA_FLOAT

  // Degree-5 polynomial approximation:
  //   dx * P(dx) ~ log2(1 + dx)
  // Generated by Sollya with:
  // > P = fpminimax(log2(1 + x)/x, 5, [|D...|], [-2^-8, 2^-7]);
  // > dirtyinfnorm(log2(1 + x)/x - P, [-2^-8, 2^-7]);
  //   0x1.653...p-52
  constexpr double COEFFS[] = {0x1.71547652b82fep0,  -0x1.71547652b7a07p-1,
                               0x1.ec709dc458db1p-2, -0x1.715479c2266c9p-2,
                               0x1.2776ae1ddf8fp-2,  -0x1.e7b2178870157p-3};

  double dx2 = dx * dx; // Exact
  double c0 = fputil::multiply_add(dx, COEFFS[1], COEFFS[0]);
  double c1 = fputil::multiply_add(dx, COEFFS[3], COEFFS[2]);
  double c2 = fputil::multiply_add(dx, COEFFS[5], COEFFS[4]);

  double p = fputil::polyeval(dx2, c0, c1, c2);

  //////////////////////////////////////////////////////////////////////////////
  // NOTE: For some reason, this is significantly less efficient than above!
  //
  // > P = fpminimax(log2(1 + x)/x, 4, [|D...|], [-2^-8, 2^-7]);
  // > dirtyinfnorm(log2(1 + x)/x - P, [-2^-8, 2^-7]);
  //   0x1.d04...p-44
  // constexpr double COEFFS[] = {0x1.71547652b8133p0, -0x1.71547652d1e33p-1,
  //                              0x1.ec70a098473dep-2, -0x1.7154c5ccdf121p-2,
  //                              0x1.2514fd90a130ap-2};
  //
  // double dx2 = dx * dx;
  // double c0 = fputil::multiply_add(dx, COEFFS[1], COEFFS[0]);
  // double c1 = fputil::multiply_add(dx, COEFFS[3], COEFFS[2]);
  // double p = fputil::polyeval(dx2, c0, c1, COEFFS[4]);
  //////////////////////////////////////////////////////////////////////////////

  // s = e_x - log2(r) + dx * P(dx)
  // Approximation errors:
  //   |log2(x) - s| < ulp(e_x) + (bounds on dx) * (error bounds of P(dx))
  //                 = ulp(e_x) + 2^-7 * 2^-51
  //                 < 2^8 * 2^-52 + 2^-7 * 2^-43
  //                 ~ 2^-44 + 2^-50
  double s = fputil::multiply_add(dx, p, LOG2_R[idx_x] + e_x);

  // To compute 2^(y * log2(x)), we break the exponent into 3 parts:
  //   y * log(2) = hi + mid + lo, where
  //   hi is an integer
  //   mid * 2^6 is an integer
  //   |lo| <= 2^-7
  // Then:
  //   x^y = 2^(y * log2(x)) = 2^hi * 2^mid * 2^lo,
  // In which 2^mid is obtained from a look-up table of size 2^6 = 64 elements,
  // and 2^lo ~ 1 + lo * P(lo).
  // Thus, we have:
  //   hi + mid = 2^-6 * round( 2^6 * y * log2(x) )
  // If we restrict the output such that |hi| < 150, (hi + mid) uses (8 + 6)
  // bits, hence, if we use double precision to perform
  //   round( 2^6 * y * log2(x))
  // the lo part is bounded by 2^-7 + 2^(-(52 - 14)) = 2^-7 + 2^-38

  // In the following computations:
  //   y6  = 2^6 * y
  //   hm  = 2^6 * (hi + mid) = round(2^6 * y * log2(x)) ~ round(y6 * s)
  //   lo6 = 2^6 * lo = 2^6 * (y - (hi + mid)) = y6 * log2(x) - hm.
  double y6 = static_cast<double>(y * 0x1.0p6f); // Exact.
  double hm = fputil::nearest_integer(s * y6);
#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  // lo6 = 2^6 * lo.
  double lo6_hi =
      fputil::multiply_add(y6, e_x + LOG2_R_DD[idx_x].hi, -hm); // Exact
  // Error bounds:
  //   | (y*log2(x) - hm * 2^-6 - lo) / y| < err(dx * p) + err(LOG2_R_DD.lo)
  //                                       < 2^-51 + 2^-75
  double lo6 = fputil::multiply_add(
      y6, fputil::multiply_add(dx, p, LOG2_R_DD[idx_x].lo), lo6_hi);
#else
  // lo6 = 2^6 * lo.
  double lo6_hi =
      fputil::multiply_add(y6, e_x + LOG2_R_TD[idx_x].hi, -hm); // Exact
  // Error bounds:
  //   | (y*log2(x) - hm * 2^-6 - lo) / y| < err(dx * p) + err(LOG2_R_DD.lo)
  //                                       < 2^-51 + 2^-75
  double lo6 = fputil::multiply_add(
      y6, fputil::multiply_add(dx, p, LOG2_R_TD[idx_x].mid), lo6_hi);
#endif

  // |2^(hi + mid) - exp2_hi_mid| <= ulp(exp2_hi_mid) / 2
  // Clamp the exponent part into smaller range that fits double precision.
  // For those exponents that are out of range, the final conversion will round
  // them correctly to inf/max float or 0/min float accordingly.
  int64_t hm_i = static_cast<int64_t>(hm);
  hm_i = (hm_i > (1 << 15)) ? (1 << 15)
                            : (hm_i < (-(1 << 15)) ? -(1 << 15) : hm_i);

  int idx_y = hm_i & 0x3f;

  // 2^hi
  int64_t exp_hi_i = (hm_i >> 6) << DoubleBits::FRACTION_LEN;
  // 2^mid
  int64_t exp_mid_i = cpp::bit_cast<uint64_t>(EXP2_MID1[idx_y].hi);
  // (-1)^sign * 2^hi * 2^mid
  // Error <= 2^hi * 2^-53
  uint64_t exp2_hi_mid_i = static_cast<uint64_t>(exp_hi_i + exp_mid_i) + sign;
  double exp2_hi_mid = cpp::bit_cast<double>(exp2_hi_mid_i);

  // Degree-5 polynomial approximation P(lo6) ~ 2^(lo6 / 2^6) = 2^(lo).
  // Generated by Sollya with:
  // > P = fpminimax(2^(x/64), 5, [|1, D...|], [-2^-1, 2^-1]);
  // > dirtyinfnorm(2^(x/64) - P, [-0.5, 0.5]);
  // 0x1.a2b77e618f5c4c176fd11b7659016cde5de83cb72p-60
  constexpr double EXP2_COEFFS[] = {0x1p0,
                                    0x1.62e42fefa39efp-7,
                                    0x1.ebfbdff82a23ap-15,
                                    0x1.c6b08d7076268p-23,
                                    0x1.3b2ad33f8b48bp-31,
                                    0x1.5d870c4d84445p-40};

  double lo6_sqr = lo6 * lo6;
  double d0 = fputil::multiply_add(lo6, EXP2_COEFFS[1], EXP2_COEFFS[0]);
  double d1 = fputil::multiply_add(lo6, EXP2_COEFFS[3], EXP2_COEFFS[2]);
  double d2 = fputil::multiply_add(lo6, EXP2_COEFFS[5], EXP2_COEFFS[4]);
  double pp = fputil::polyeval(lo6_sqr, d0, d1, d2);

  double r = pp * exp2_hi_mid;

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  return static_cast<float>(r);
#else
  // Ziv accuracy test.
  uint64_t r_u = cpp::bit_cast<uint64_t>(r);
  float r_upper = static_cast<float>(cpp::bit_cast<double>(r_u + ERR));
  float r_lower = static_cast<float>(cpp::bit_cast<double>(r_u - ERR));

  if (LIBC_LIKELY(r_upper == r_lower)) {
    // Check for overflow or underflow.
    if (LIBC_UNLIKELY(FloatBits(r_upper).get_mantissa() == 0)) {
      if (FloatBits(r_upper).is_inf()) {
        fputil::set_errno_if_required(ERANGE);
        fputil::raise_except_if_required(FE_OVERFLOW);
      } else if (r_upper == 0.0f) {
        fputil::set_errno_if_required(ERANGE);
        fputil::raise_except_if_required(FE_UNDERFLOW);
      }
    }
    return r_upper;
  }

  // Scale lower part of 2^(hi + mid)
  DoubleDouble exp2_hi_mid_dd;
  exp2_hi_mid_dd.lo =
      (idx_y != 0)
          ? cpp::bit_cast<double>(exp_hi_i +
                                  cpp::bit_cast<int64_t>(EXP2_MID1[idx_y].mid))
          : 0.0;
  exp2_hi_mid_dd.hi = exp2_hi_mid;

  double r_dd = powf_double_double(idx_x, dx, y6, lo6_hi, exp2_hi_mid_dd);

  return static_cast<float>(r_dd);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS && LIBC_MATH_HAS_SMALL_TABLES
}

} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_POWF_H
