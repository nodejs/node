//===-- Double-precision log2(x) function ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_LOG2_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_LOG2_H

#include "common_constants.h"
#include "log_range_reduction.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/double_double.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/common.h"
#include "src/__support/integer_literals.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {

namespace math {
namespace log2_internal {
// 128-bit precision dyadic floating point numbers.
using Float128 = typename fputil::DyadicFloat<128>;

using LIBC_NAMESPACE::operator""_u128;

using namespace common_constants_internal;
using namespace math::log_range_reduction_internal;

LIBC_INLINE_VAR constexpr fputil::DoubleDouble LOG2_E = {0x1.777d0ffda0d24p-56,
                                                         0x1.71547652b82fep0};

alignas(16) LIBC_INLINE_VAR const fputil::DoubleDouble LOG_R1[128] = {
    {0.0, 0.0},
    {0x1.46662d417cedp-62, 0x1.010157588de71p-7},
    {0x1.27c8e8416e71fp-60, 0x1.0205658935847p-6},
    {-0x1.d192d0619fa67p-60, 0x1.8492528c8cabfp-6},
    {0x1.c05cf1d753622p-59, 0x1.0415d89e74444p-5},
    {-0x1.cdd6f7f4a137ep-59, 0x1.466aed42de3eap-5},
    {0x1.a8be97660a23dp-60, 0x1.894aa149fb343p-5},
    {-0x1.e48fb0500efd4p-59, 0x1.ccb73cdddb2ccp-5},
    {-0x1.dd7009902bf32p-58, 0x1.08598b59e3a07p-4},
    {-0x1.7558367a6acf6p-59, 0x1.1973bd1465567p-4},
    {0x1.7a976d3b5b45fp-59, 0x1.3bdf5a7d1ee64p-4},
    {0x1.f38745c5c450ap-58, 0x1.5e95a4d9791cbp-4},
    {-0x1.72566212cdd05p-61, 0x1.700d30aeac0e1p-4},
    {-0x1.478a85704ccb7p-58, 0x1.9335e5d594989p-4},
    {-0x1.0057eed1ca59fp-59, 0x1.b6ac88dad5b1cp-4},
    {0x1.a38cb559a6706p-58, 0x1.c885801bc4b23p-4},
    {-0x1.a2bf991780d3fp-59, 0x1.ec739830a112p-4},
    {-0x1.ac9f4215f9393p-58, 0x1.fe89139dbd566p-4},
    {-0x1.0e63a5f01c691p-58, 0x1.1178e8227e47cp-3},
    {-0x1.c6ef1d9b2ef7ep-59, 0x1.1aa2b7e23f72ap-3},
    {-0x1.499a3f25af95fp-58, 0x1.2d1610c86813ap-3},
    {0x1.7d411a5b944adp-58, 0x1.365fcb0159016p-3},
    {-0x1.0d5604930f135p-58, 0x1.4913d8333b561p-3},
    {-0x1.71a9682395bfdp-61, 0x1.527e5e4a1b58dp-3},
    {-0x1.d34f0f4621bedp-60, 0x1.6574ebe8c133ap-3},
    {-0x1.8de59c21e166cp-57, 0x1.6f0128b756abcp-3},
    {-0x1.1232ce70be781p-57, 0x1.823c16551a3c2p-3},
    {0x1.55aa8b6997a4p-58, 0x1.8beafeb38fe8cp-3},
    {0x1.142c507fb7a3dp-58, 0x1.95a5adcf7017fp-3},
    {0x1.bcafa9de97203p-57, 0x1.a93ed3c8ad9e3p-3},
    {-0x1.6353ab386a94dp-57, 0x1.b31d8575bce3dp-3},
    {0x1.dd355f6a516d7p-60, 0x1.bd087383bd8adp-3},
    {0x1.60629242471a2p-57, 0x1.d1037f2655e7bp-3},
    {0x1.aa11d49f96cb9p-58, 0x1.db13db0d4894p-3},
    {0x1.2276041f43042p-59, 0x1.e530effe71012p-3},
    {-0x1.08ab2ddc708ap-58, 0x1.ef5ade4dcffe6p-3},
    {0x1.f665066f980a2p-57, 0x1.f991c6cb3b379p-3},
    {0x1.cdb16ed4e9138p-56, 0x1.07138604d5862p-2},
    {0x1.162c79d5d11eep-58, 0x1.0c42d676162e3p-2},
    {-0x1.0e63a5f01c691p-57, 0x1.1178e8227e47cp-2},
    {0x1.66fbd28b40935p-56, 0x1.16b5ccbacfb73p-2},
    {-0x1.12aeb84249223p-57, 0x1.1bf99635a6b95p-2},
    {0x1.e0efadd9db02bp-56, 0x1.269621134db92p-2},
    {-0x1.82dad7fd86088p-56, 0x1.2bef07cdc9354p-2},
    {-0x1.3d69909e5c3dcp-56, 0x1.314f1e1d35ce4p-2},
    {-0x1.324f0e883858ep-58, 0x1.36b6776be1117p-2},
    {-0x1.2ad27e50a8ec6p-56, 0x1.3c25277333184p-2},
    {0x1.0dbb243827392p-57, 0x1.419b423d5e8c7p-2},
    {0x1.8fb4c14c56eefp-60, 0x1.4718dc271c41bp-2},
    {-0x1.123615b147a5dp-58, 0x1.4c9e09e172c3cp-2},
    {-0x1.8f7e9b38a6979p-57, 0x1.522ae0738a3d8p-2},
    {-0x1.0908d15f88b63p-57, 0x1.57bf753c8d1fbp-2},
    {-0x1.6541148cbb8a2p-56, 0x1.5d5bddf595f3p-2},
    {0x1.dc18ce51fff99p-57, 0x1.630030b3aac49p-2},
    {0x1.a64eadd740178p-58, 0x1.68ac83e9c6a14p-2},
    {0x1.657c222d868cdp-58, 0x1.6e60ee6af1972p-2},
    {0x1.84a4ee3059583p-56, 0x1.741d876c67bb1p-2},
    {-0x1.c168817443f22p-56, 0x1.79e26687cfb3ep-2},
    {-0x1.219024acd3b77p-58, 0x1.7fafa3bd8151cp-2},
    {-0x1.486666443b153p-56, 0x1.85855776dcbfbp-2},
    {-0x1.70f2f38238303p-56, 0x1.8b639a88b2df5p-2},
    {-0x1.ad4bb98c1f2c5p-56, 0x1.914a8635bf68ap-2},
    {-0x1.89d2816cf838fp-57, 0x1.973a3431356aep-2},
    {0x1.87bcbcfd3e187p-59, 0x1.9d32bea15ed3bp-2},
    {-0x1.ba8062860ae23p-57, 0x1.a33440224fa79p-2},
    {-0x1.ba8062860ae23p-57, 0x1.a33440224fa79p-2},
    {0x1.bcafa9de97203p-56, 0x1.a93ed3c8ad9e3p-2},
    {0x1.9d56c45dd3e86p-56, 0x1.af5295248cddp-2},
    {0x1.494b610665378p-56, 0x1.b56fa04462909p-2},
    {0x1.6fd02999b21e1p-59, 0x1.bb9611b80e2fbp-2},
    {-0x1.bfc00b8f3feaap-56, 0x1.c1c60693fa39ep-2},
    {-0x1.bfc00b8f3feaap-56, 0x1.c1c60693fa39ep-2},
    {0x1.223eadb651b4ap-57, 0x1.c7ff9c74554c9p-2},
    {0x1.0798270b29f39p-56, 0x1.ce42f18064743p-2},
    {0x1.d7f4d3b3d406bp-56, 0x1.d490246defa6bp-2},
    {-0x1.0b5837185a661p-56, 0x1.dae75484c9616p-2},
    {-0x1.ac81cc8a4dfb8p-56, 0x1.e148a1a2726cep-2},
    {-0x1.ac81cc8a4dfb8p-56, 0x1.e148a1a2726cep-2},
    {0x1.57d646a17bc6ap-56, 0x1.e7b42c3ddad73p-2},
    {-0x1.74b71fb5e57e3p-62, 0x1.ee2a156b413e5p-2},
    {-0x1.0d487f5aba5e5p-57, 0x1.f4aa7ee03192dp-2},
    {-0x1.0d487f5aba5e5p-57, 0x1.f4aa7ee03192dp-2},
    {0x1.7e8f05924d259p-57, 0x1.fb358af7a4884p-2},
    {0x1.1713a36138e19p-57, 0x1.00e5ae5b207abp-1},
    {-0x1.17f9e54e78104p-57, 0x1.04360be7603adp-1},
    {-0x1.17f9e54e78104p-57, 0x1.04360be7603adp-1},
    {0x1.2241edf5fd1f7p-57, 0x1.078bf0533c568p-1},
    {0x1.0d710fcfc4e0dp-55, 0x1.0ae76e2d054fap-1},
    {0x1.0d710fcfc4e0dp-55, 0x1.0ae76e2d054fap-1},
    {0x1.3300f002e836ep-55, 0x1.0e4898611cce1p-1},
    {-0x1.91eee7772c7c2p-55, 0x1.11af823c75aa8p-1},
    {-0x1.91eee7772c7c2p-55, 0x1.11af823c75aa8p-1},
    {0x1.342eb628dba17p-56, 0x1.151c3f6f29612p-1},
    {0x1.89df1568ca0bp-55, 0x1.188ee40f23ca6p-1},
    {0x1.89df1568ca0bp-55, 0x1.188ee40f23ca6p-1},
    {0x1.59bddae1ccce2p-56, 0x1.1c07849ae6007p-1},
    {-0x1.2164ff40e9817p-56, 0x1.1f8635fc61659p-1},
    {-0x1.2164ff40e9817p-56, 0x1.1f8635fc61659p-1},
    {-0x1.fcc8dbccc25cbp-57, 0x1.230b0d8bebc98p-1},
    {0x1.e0efadd9db02bp-55, 0x1.269621134db92p-1},
    {0x1.e0efadd9db02bp-55, 0x1.269621134db92p-1},
    {-0x1.6a0c343be95dcp-56, 0x1.2a2786d0ec107p-1},
    {-0x1.b941ee770436bp-56, 0x1.2dbf557b0df43p-1},
    {-0x1.b941ee770436bp-56, 0x1.2dbf557b0df43p-1},
    {0x1.6c3a5f12642c9p-57, 0x1.315da4434068bp-1},
    {0x1.6c3a5f12642c9p-57, 0x1.315da4434068bp-1},
    {-0x1.f01ab6065515cp-56, 0x1.35028ad9d8c86p-1},
    {0x1.21512aa596ea3p-55, 0x1.38ae2171976e7p-1},
    {0x1.21512aa596ea3p-55, 0x1.38ae2171976e7p-1},
    {0x1.1930603d87b6ep-56, 0x1.3c6080c36bfb5p-1},
    {0x1.1930603d87b6ep-56, 0x1.3c6080c36bfb5p-1},
    {0x1.86cf0f38b461ap-57, 0x1.4019c2125ca93p-1},
    {-0x1.84f481051f71ap-56, 0x1.43d9ff2f923c5p-1},
    {-0x1.84f481051f71ap-56, 0x1.43d9ff2f923c5p-1},
    {0x1.2541aca7d5844p-55, 0x1.47a1527e8a2d3p-1},
    {0x1.2541aca7d5844p-55, 0x1.47a1527e8a2d3p-1},
    {0x1.c457b531506f6p-55, 0x1.4b6fd6f970c1fp-1},
    {0x1.c457b531506f6p-55, 0x1.4b6fd6f970c1fp-1},
    {0x1.d749362382a77p-56, 0x1.4f45a835a4e19p-1},
    {0x1.d749362382a77p-56, 0x1.4f45a835a4e19p-1},
    {0x1.988ba4aea614dp-56, 0x1.5322e26867857p-1},
    {0x1.988ba4aea614dp-56, 0x1.5322e26867857p-1},
    {0x1.80bff3303dd48p-55, 0x1.5707a26bb8c66p-1},
    {0x1.80bff3303dd48p-55, 0x1.5707a26bb8c66p-1},
    {-0x1.6714fbcd8135bp-55, 0x1.5af405c3649ep-1},
    {-0x1.6714fbcd8135bp-55, 0x1.5af405c3649ep-1},
    {0x1.1c066d235ee63p-56, 0x1.5ee82aa24192p-1},
    {0.0, 0.0},
};

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
// Extra errors from P is from using x^2 to reduce evaluation latency.
LIBC_INLINE_VAR constexpr double P_ERR = 0x1.0p-49;

alignas(16) LIBC_INLINE_VAR constexpr LogRR LOG2_TABLE = {
    // -log2(r) with 128-bit precision generated by SageMath with:
    // def format_hex(value):
    //     l = hex(value)[2:]
    //     n = 8
    //     x = [l[i:i + n] for i in range(0, len(l), n)]
    //     return "0x" + "'".join(x) + "_u128"
    // for i in range(1, 127):
    //   r = 2^-8 * ceil( 2^8 * (1 - 2^(-8)) / (1 + i*2^(-7)) );
    //   s, m, e = RealField(128)(r).log2().sign_mantissa_exponent();
    //   print("{Sign::POS,", e, ", format_hex(m), "},");
    /* .step_1 = */ {
        {Sign::POS, 0, 0_u128},
        {Sign::POS, -134, 0xb963dd10'7b993ada'e8c25163'0adb856a_u128},
        {Sign::POS, -133, 0xba1f7430'f9aab1b2'a41b08fb'e05f82d0_u128},
        {Sign::POS, -132, 0x8c25c726'2b57c149'1f06c085'bc1b865d_u128},
        {Sign::POS, -132, 0xbb9ca64e'cac6aaef'2e1c07f0'438ebac0_u128},
        {Sign::POS, -132, 0xeb75e8f8'ff5ff022'aacc0e21'd6541224_u128},
        {Sign::POS, -131, 0x8dd99530'02a4e866'31514aef'39ce6303_u128},
        {Sign::POS, -131, 0xa62b07f3'457c4070'50799bea'aab2940c_u128},
        {Sign::POS, -131, 0xbeb024b6'7dda6339'da288fc6'15a727dc_u128},
        {Sign::POS, -131, 0xcb0657cd'5dbe4f6f'22dbbace'd44516ce_u128},
        {Sign::POS, -131, 0xe3da945b'878e27d0'd939dcee'cdd9ce05_u128},
        {Sign::POS, -131, 0xfce4aee0'e88b2749'9596a8e2'e84c8f45_u128},
        {Sign::POS, -130, 0x84bf1c67'3032495d'243efd93'25954cfe_u128},
        {Sign::POS, -130, 0x916d6e15'59a4b696'91d79938'e7226384_u128},
        {Sign::POS, -130, 0x9e37db28'66f2850b'22563c9e'd9462091_u128},
        {Sign::POS, -130, 0xa4a7c31d'c6f9a5d5'3a53ca11'81015ada_u128},
        {Sign::POS, -130, 0xb19d45fa'1be70855'3eb8023e'ed65d601_u128},
        {Sign::POS, -130, 0xb823018e'3cfc25f0'ce5cabbd'2d753d9b_u128},
        {Sign::POS, -130, 0xc544c055'fde99333'54dbf16f'b0695ee3_u128},
        {Sign::POS, -130, 0xcbe0e589'e3f6042d'5196a85a'067c6739_u128},
        {Sign::POS, -130, 0xd930124b'ea9a2c66'f349845e'48955078_u128},
        {Sign::POS, -130, 0xdfe33d3f'ffa66037'815ef705'cfaef035_u128},
        {Sign::POS, -130, 0xed61169f'220e97f2'2ba704dc'aa76f41d_u128},
        {Sign::POS, -130, 0xf42be9e9'b09b3def'2062f36b'c14d0d93_u128},
        {Sign::POS, -129, 0x80ecdde7'd30ea2ed'13288019'4144b02b_u128},
        {Sign::POS, -129, 0x845e706c'afd1bf61'54880de6'3812fd49_u128},
        {Sign::POS, -129, 0x8b4e029b'1f8ac391'a87c02ea'f36e2c29_u128},
        {Sign::POS, -129, 0x8ecc164e'a93841ae'9804237e'c8d9431d_u128},
        {Sign::POS, -129, 0x924e6958'9e6b6268'20f81ca9'5d9e7968_u128},
        {Sign::POS, -129, 0x995ff71b'8773432d'124bc6f1'acf95dc4_u128},
        {Sign::POS, -129, 0x9cef470a'acfb7bf9'5a5e8e21'bff3336b_u128},
        {Sign::POS, -129, 0xa08300be'1f651473'4e53fa33'29f65894_u128},
        {Sign::POS, -129, 0xa7b7dd96'762cc3c7'2742d729'6a39eed6_u128},
        {Sign::POS, -129, 0xab591735'abc724e4'f359c554'4bc5e134_u128},
        {Sign::POS, -129, 0xaefee78f'75707221'6b6c874d'd96e1d75_u128},
        {Sign::POS, -129, 0xb2a95a4c'c313bb59'21006678'c0a5c390_u128},
        {Sign::POS, -129, 0xb6587b43'2e47501b'6d40900b'25024b32_u128},
        {Sign::POS, -129, 0xbdc4f816'7955698f'89e2eb55'3b279b3d_u128},
        {Sign::POS, -129, 0xc1826c86'08fe9951'd58525aa'd392ca50_u128},
        {Sign::POS, -129, 0xc544c055'fde99333'54dbf16f'b0695ee3_u128},
        {Sign::POS, -129, 0xc90c0049'26e9dbfb'88d5eae3'326327bb_u128},
        {Sign::POS, -129, 0xccd83954'b6359379'46dfa05b'ddfded8c_u128},
        {Sign::POS, -129, 0xd47fcb8c'0852f0c0'bfe9dbeb'f2e8a45e_u128},
        {Sign::POS, -129, 0xd85b3fa7'a3407fa8'7b11f1c5'160c515c_u128},
        {Sign::POS, -129, 0xdc3be2bd'8d837f7f'1339e567'7ec44dd0_u128},
        {Sign::POS, -129, 0xe021c2cf'17ed9bdb'ea2b8c7b'b0ee9c8b_u128},
        {Sign::POS, -129, 0xe40cee16'a2ff21c4'aec56233'2791fe38_u128},
        {Sign::POS, -129, 0xe7fd7308'd6895b14'71682eba'cca79cfa_u128},
        {Sign::POS, -129, 0xebf36055'e1abc61e'a5ad5ce9'fb5a7bb6_u128},
        {Sign::POS, -129, 0xefeec4ea'c371584e'32251905'31a852c5_u128},
        {Sign::POS, -129, 0xf3efaff2'9c559a77'da8ad649'da21eab0_u128},
        {Sign::POS, -129, 0xf7f630d8'08fc2ada'4c3e2ea7'c15c3d1e_u128},
        {Sign::POS, -129, 0xfc025746'86680cc6'bcb9bfa9'852e0d35_u128},
        {Sign::POS, -128, 0x800a1995'f0019518'ce032f41'd1e774e8_u128},
        {Sign::POS, -128, 0x8215ea5c'd3e4c4c7'9b39ffee'bc29372a_u128},
        {Sign::POS, -128, 0x8424a633'5c777e0b'87f95f1b'efb6f806_u128},
        {Sign::POS, -128, 0x86365578'62acb7ce'b987b42e'3bb332a1_u128},
        {Sign::POS, -128, 0x884b00ae'f726cec5'139a7ba8'3bf2d136_u128},
        {Sign::POS, -128, 0x8a62b07f'3457c407'050799be'aaab2941_u128},
        {Sign::POS, -128, 0x8c7d6db7'169e0cda'8bd74461'7e9b7d52_u128},
        {Sign::POS, -128, 0x8e9b414b'5a92a606'046ad444'333ceb10_u128},
        {Sign::POS, -128, 0x90bc3458'61bf3d52'ef4c737f'ba4f5d66_u128},
        {Sign::POS, -128, 0x92e05023'1df57d6f'ae441c09'd761c549_u128},
        {Sign::POS, -128, 0x95079e1a'0382dc79'6e36aa9c'e90a3879_u128},
        {Sign::POS, -128, 0x973227d6'027ebd8a'0efca1a1'84e93809_u128},
        {Sign::POS, -128, 0x973227d6'027ebd8a'0efca1a1'84e93809_u128},
        {Sign::POS, -128, 0x995ff71b'8773432d'124bc6f1'acf95dc4_u128},
        {Sign::POS, -128, 0x9b9115db'83a3dd2d'352bea51'e58ea9e8_u128},
        {Sign::POS, -128, 0x9dc58e34'7d37696d'266d6cdc'959153bc_u128},
        {Sign::POS, -128, 0x9ffd6a73'a78eaf35'4527d82c'8214ddca_u128},
        {Sign::POS, -128, 0xa238b516'0413106e'404cabb7'6d600e3c_u128},
        {Sign::POS, -128, 0xa238b516'0413106e'404cabb7'6d600e3c_u128},
        {Sign::POS, -128, 0xa47778c9'8bcc86a1'cab7d2ec'23f0eef3_u128},
        {Sign::POS, -128, 0xa6b9c06e'6211646b'761c48dd'859de2d3_u128},
        {Sign::POS, -128, 0xa8ff9718'10a5e181'7fd3b7d7'e5d148bb_u128},
        {Sign::POS, -128, 0xab49080e'cda53208'c27c6780'd92b4d11_u128},
        {Sign::POS, -128, 0xad961ed0'cb91d406'db502402'c94092cd_u128},
        {Sign::POS, -128, 0xad961ed0'cb91d406'db502402'c94092cd_u128},
        {Sign::POS, -128, 0xafe6e713'93eeda29'3432ef6b'732b6843_u128},
        {Sign::POS, -128, 0xb23b6cc5'6cc84c99'bb324da7'e046e792_u128},
        {Sign::POS, -128, 0xb493bc0e'c9954243'b21709ce'430c8e24_u128},
        {Sign::POS, -128, 0xb493bc0e'c9954243'b21709ce'430c8e24_u128},
        {Sign::POS, -128, 0xb6efe153'c7e319f6'e91ad16e'cff10111_u128},
        {Sign::POS, -128, 0xb94fe935'b83e3eb5'ce31e481'cd797e79_u128},
        {Sign::POS, -128, 0xbbb3e094'b3d228d3'da3e961a'96c580fa_u128},
        {Sign::POS, -128, 0xbbb3e094'b3d228d3'da3e961a'96c580fa_u128},
        {Sign::POS, -128, 0xbe1bd491'3f3fda43'f396598a'ae91499a_u128},
        {Sign::POS, -128, 0xc087d28d'fb2febb8'ae4cceb0'f621941b_u128},
        {Sign::POS, -128, 0xc087d28d'fb2febb8'ae4cceb0'f621941b_u128},
        {Sign::POS, -128, 0xc2f7e831'632b6670'6c1855c4'2078f81b_u128},
        {Sign::POS, -128, 0xc56c2367'9b4d206e'169535fb'8bf577c8_u128},
        {Sign::POS, -128, 0xc56c2367'9b4d206e'169535fb'8bf577c8_u128},
        {Sign::POS, -128, 0xc7e49264'4d64237e'3b24cecc'60217942_u128},
        {Sign::POS, -128, 0xca6143a4'9626d820'3dc2687f'cf939696_u128},
        {Sign::POS, -128, 0xca6143a4'9626d820'3dc2687f'cf939696_u128},
        {Sign::POS, -128, 0xcce245f1'031e41fa'0a62e6ad'd1a901a0_u128},
        {Sign::POS, -128, 0xcf67a85f'a1f89a04'5bb6e231'38ad51e1_u128},
        {Sign::POS, -128, 0xcf67a85f'a1f89a04'5bb6e231'38ad51e1_u128},
        {Sign::POS, -128, 0xd1f17a56'21fb01ac'7fc60a51'03092bae_u128},
        {Sign::POS, -128, 0xd47fcb8c'0852f0c0'bfe9dbeb'f2e8a45e_u128},
        {Sign::POS, -128, 0xd47fcb8c'0852f0c0'bfe9dbeb'f2e8a45e_u128},
        {Sign::POS, -128, 0xd712ac0c'f811659d'8e2d7d37'8127d823_u128},
        {Sign::POS, -128, 0xd9aa2c3b'0ea3cbc1'5c1a7f14'b168b365_u128},
        {Sign::POS, -128, 0xd9aa2c3b'0ea3cbc1'5c1a7f14'b168b365_u128},
        {Sign::POS, -128, 0xdc465cd1'55a90942'b7579f0f'8d3d514b_u128},
        {Sign::POS, -128, 0xdc465cd1'55a90942'b7579f0f'8d3d514b_u128},
        {Sign::POS, -128, 0xdee74ee6'4b0c38d3'b087205e'b55aea85_u128},
        {Sign::POS, -128, 0xe18d13ee'805a4de3'424a2623'd60dfb16_u128},
        {Sign::POS, -128, 0xe18d13ee'805a4de3'424a2623'd60dfb16_u128},
        {Sign::POS, -128, 0xe437bdbf'5254459c'4d3a591a'e6854787_u128},
        {Sign::POS, -128, 0xe437bdbf'5254459c'4d3a591a'e6854787_u128},
        {Sign::POS, -128, 0xe6e75e91'b9cca551'8dcdb6b2'4c5c5cdf_u128},
        {Sign::POS, -128, 0xe99c0905'36ece983'33ac7d9e'bba8a53c_u128},
        {Sign::POS, -128, 0xe99c0905'36ece983'33ac7d9e'bba8a53c_u128},
        {Sign::POS, -128, 0xec55d022'd80e3d27'fb2eede4'b59d8959_u128},
        {Sign::POS, -128, 0xec55d022'd80e3d27'fb2eede4'b59d8959_u128},
        {Sign::POS, -128, 0xef14c760'5d60654c'308b4546'66de8f99_u128},
        {Sign::POS, -128, 0xef14c760'5d60654c'308b4546'66de8f99_u128},
        {Sign::POS, -128, 0xf1d902a3'7aaa5085'8383cb0c'e23bebd4_u128},
        {Sign::POS, -128, 0xf1d902a3'7aaa5085'8383cb0c'e23bebd4_u128},
        {Sign::POS, -128, 0xf4a29645'38813c67'64fc87b4'a41f7b70_u128},
        {Sign::POS, -128, 0xf4a29645'38813c67'64fc87b4'a41f7b70_u128},
        {Sign::POS, -128, 0xf7719715'7665f689'3f5d7d82'b65c5686_u128},
        {Sign::POS, -128, 0xf7719715'7665f689'3f5d7d82'b65c5686_u128},
        {Sign::POS, -128, 0xfa461a5e'8f4b759d'6476077b'9fbd41ae_u128},
        {Sign::POS, -128, 0xfa461a5e'8f4b759d'6476077b'9fbd41ae_u128},
        {Sign::POS, -128, 0xfd2035e9'221ef5d0'0e3909ff'd0d61778_u128},
        {Sign::POS, 0, 0_u128},
    },
    // -log2(r) for the second step, generated by SageMath with:
    //
    // for i in range(-2^6, 2^7 + 1):
    //   r = 2^-16 * round( 2^16 / (1 + i*2^(-14)) );
    //   s, m, e = RealField(128)(r).log2().sign_mantissa_exponent();
    //   print("{Sign::NEG," if s == 1 else "{Sign::POS,", e, ",
    //         format_hex(m), "},");
    /* .step_2 = */
    {
        {Sign::NEG, -135, 0xb9061559'18954401'b5cfed58'337e848a_u128},
        {Sign::NEG, -135, 0xb6264958'a3c7fa2b'ffaf2ac1'b1d20910_u128},
        {Sign::NEG, -135, 0xb34671e4'39aa448e'52521a39'50ea2ed8_u128},
        {Sign::NEG, -135, 0xb0668efb'7ef48ab7'f87e1abd'ee10fd95_u128},
        {Sign::NEG, -135, 0xad86a09e'185af0e8'fbd43bbc'c24c5e43_u128},
        {Sign::NEG, -135, 0xaaa6a6cb'aa8d57ce'2f4f5d48'f9796742_u128},
        {Sign::NEG, -135, 0xa7c6a183'da375c3d'3477fd67'c1cab6b3_u128},
        {Sign::NEG, -135, 0xa4e690c6'4c0056f0'7b4d33eb'381fe558_u128},
        {Sign::NEG, -135, 0xa2067492'a48b5c43'3ce25e48'cb498dea_u128},
        {Sign::NEG, -135, 0x9f264ce8'88773bed'70b0fcc9'e4330983_u128},
        {Sign::NEG, -135, 0x9c4619c7'9c5e80bf'bc9e4267'd3189b22_u128},
        {Sign::NEG, -135, 0x9965db2f'84d7705f'5fb3d896'326615c4_u128},
        {Sign::NEG, -135, 0x9685911f'e6740b02'178b5831'1e96d323_u128},
        {Sign::NEG, -135, 0x93a53b98'65c20b2a'006bf8b6'cf73d847_u128},
        {Sign::NEG, -135, 0x90c4da98'a74ae561'7019f6e6'4a580a02_u128},
        {Sign::NEG, -135, 0x8de46e20'4f93c7f6'cb5733cf'0eb4191d_u128},
        {Sign::NEG, -135, 0x8b03f62f'031d9ab8'56148d4f'c5e415b6_u128},
        {Sign::NEG, -135, 0x882372c4'6664feaf'fe5370f4'25872623_u128},
        {Sign::NEG, -135, 0x8542e3e0'1de24ddf'21b72a14'57ee70d6_u128},
        {Sign::NEG, -135, 0x81aa211f'1e332fcf'abff4f89'968bed0b_u128},
        {Sign::NEG, -136, 0xfd92f0cf'88d75f24'86410a67'6480a5a7_u128},
        {Sign::NEG, -136, 0xf7d1886b'2a876289'44280889'021970e4_u128},
        {Sign::NEG, -136, 0xf2100910'6a42bc14'32eb139d'9812090d_u128},
        {Sign::NEG, -136, 0xec4e72be'90cd2d2d'bef9dd41'e8e42810_u128},
        {Sign::NEG, -136, 0xe68cc574'e6e1e5d7'689d08ca'6c7c3eb1_u128},
        {Sign::NEG, -136, 0xe0cb0132'b5338423'01ef259a'7f69821d_u128},
        {Sign::NEG, -136, 0xdb0925f7'446c13a9'e22cea71'b7bb8467_u128},
        {Sign::NEG, -136, 0xd54733c1'dd2d0d04'0e5bb273'03f542fe_u128},
        {Sign::NEG, -136, 0xcf852a91'c80f553f'57453c8d'5dc64ce1_u128},
        {Sign::NEG, -136, 0xc9c30a66'4da33d56'6cc7add1'fc09ef92_u128},
        {Sign::NEG, -136, 0xc400d33e'b67081a7'e678d728'0de1c07f_u128},
        {Sign::NEG, -136, 0xbe3e851a'4af6496d'419bbeb2'239bdc39_u128},
        {Sign::NEG, -136, 0xb87c1ff8'53ab2631'd4676d1d'81755809_u128},
        {Sign::NEG, -136, 0xb2b9a3d8'18fd1349'b69dfef7'ac2e2890_u128},
        {Sign::NEG, -136, 0xacf710b8'e3517548'9f72fa0a'8fccabc0_u128},
        {Sign::NEG, -136, 0xa7346699'fb051978'b8bfe6a3'addb988e_u128},
        {Sign::NEG, -136, 0xa171a57a'a86c3551'67862c8e'c9dcd60d_u128},
        {Sign::NEG, -136, 0x9baecd5a'33d265ee'09bd3370'909e28a6_u128},
        {Sign::NEG, -136, 0x95ebde37'e57aaf84'a96bc611'b991419b_u128},
        {Sign::NEG, -136, 0x9028d813'059f7cdc'a50bb80f'203f0d62_u128},
        {Sign::NEG, -136, 0x8a65baea'dc729ec5'4d36cd47'4f65a317_u128},
        {Sign::NEG, -136, 0x84a286be'b21d4b8c'779be241'ef4874a3_u128},
        {Sign::NEG, -137, 0xfdbe771b'9d803cea'0e76a962'fa65ace3_u128},
        {Sign::NEG, -137, 0xf237b2ae'f4e62e5a'd3d35627'464a5267_u128},
        {Sign::NEG, -137, 0xe6b0c035'fa8b328c'162ef4b0'e838c363_u128},
        {Sign::NEG, -137, 0xdb299faf'3e7cd74f'77bb10b9'76b3b9ca_u128},
        {Sign::NEG, -137, 0xcfa25119'50b77014'209853ce'e70bc58b_u128},
        {Sign::NEG, -137, 0xc41ad472'c12614d3'63f9b57c'baf2e58d_u128},
        {Sign::NEG, -137, 0xb89329ba'1fa2a0fd'4fca1c93'1bd6e6d6_u128},
        {Sign::NEG, -137, 0xad0b50ed'fbf5b265'26d26e43'4a53490a_u128},
        {Sign::NEG, -137, 0xa1834a0c'e5d6a82d'c55e0790'78dc86a0_u128},
        {Sign::NEG, -137, 0x95fb1515'6ceba1b5'f05b9d5b'd28f540b_u128},
        {Sign::NEG, -137, 0x8a72b206'20c97d84'8ef87f1a'11cdb727_u128},
        {Sign::NEG, -138, 0xfdd441bb'21e7b069'9d687011'4c1183cf_u128},
        {Sign::NEG, -138, 0xe6c2c334'99ba16c4'63d514ff'f97e86f3_u128},
        {Sign::NEG, -138, 0xcfb0e875'c7cc5929'11a38190'1eadd883_u128},
        {Sign::NEG, -138, 0xb89eb17b'cabe1857'a9d69d37'bc0a5bac_u128},
        {Sign::NEG, -138, 0xa18c1e43'c10c6898'2dc97c9f'fefd2497_u128},
        {Sign::NEG, -138, 0x8a792eca'c911cf92'0dcdc8af'cb2ac09a_u128},
        {Sign::NEG, -139, 0xe6cbc61c'020c8446'dd454eb3'a1489470_u128},
        {Sign::NEG, -139, 0xb8a47615'0dfe4470'87803586'4d84b319_u128},
        {Sign::NEG, -139, 0x8a7c6d7a'f1de7942'7ce595cc'53b8342c_u128},
        {Sign::NEG, -140, 0xb8a7588f'd29b1baa'4710b590'49899141_u128},
        {Sign::NEG, -141, 0xb8a8c9d8'be9ae994'5957f633'309d74e3_u128},
        {Sign::POS, 0, 0_u128},
        {Sign::POS, -141, 0xb8abac81'ab576f3b'8268aba0'30b1adf6_u128},
        {Sign::POS, -140, 0xb8ad1de1'ac9ea6a5'1511cba2'fb213a10_u128},
        {Sign::POS, -139, 0x8a82eb77'08262500'6379fb9f'd9bc6235_u128},
        {Sign::POS, -139, 0xb8b000b8'c65957cc'b6fe1bf6'01ee27d5_u128},
        {Sign::POS, -139, 0xe6ddcebb'd72d3f7f'8c6e6069'3a14e6d0_u128},
        {Sign::POS, -138, 0x8a862ac3'0095c084'e9bcfd0c'62eaa2ca_u128},
        {Sign::POS, -138, 0xa19dca8e'85918b6d'73b21420'9a5234a7_u128},
        {Sign::POS, -138, 0xb8b5c6c3'5e142a9b'347d4ca3'109fe4db_u128},
        {Sign::POS, -138, 0xcfce1f64'6dca7745'37a62c48'783bb066_u128},
        {Sign::POS, -138, 0xe6e6d474'9883fbe3'0794b643'7fb56344_u128},
        {Sign::POS, -138, 0xfdffe5f6'c232f658'1cb9a45e'd90318e6_u128},
        {Sign::POS, -137, 0x8a8ca9f6'e7762d0f'bc118e5d'bbef7dbc_u128},
        {Sign::POS, -137, 0x96198f2e'5173e93b'b4c0fb95'35907cf8_u128},
        {Sign::POS, -137, 0xa1a6a2a3'113fe246'c051d2c5'f00a9bb9_u128},
        {Sign::POS, -137, 0xad33e456'9918a8d5'55326987'8c1e5110_u128},
        {Sign::POS, -137, 0xb8c1544a'5b4e2caf'bc906750'b0ce372c_u128},
        {Sign::POS, -137, 0xc44ef27f'ca41bdd8'4c50eaa6'3be294b6_u128},
        {Sign::POS, -137, 0xcfdcbef8'58660da1'b6cb28db'8c065b44_u128},
        {Sign::POS, -137, 0xdb6ab9b5'783f2fc5'70479336'830ceb05_u128},
        {Sign::POS, -137, 0xe6f8e2b8'9c629b7a'2a458c83'1f6aeb49_u128},
        {Sign::POS, -137, 0xf2873a03'37772c8a'6489ba5b'd391e206_u128},
        {Sign::POS, -137, 0xfe15bf96'bc35246b'13f6fda5'10aeec3b_u128},
        {Sign::POS, -136, 0x84d239ba'4eb315a9'2f9a0ef9'e8250836_u128},
        {Sign::POS, -136, 0x8a99aacf'26f2a8a7'389019e8'22b70f1e_u128},
        {Sign::POS, -136, 0x9061330a'a04f87ae'308beeff'a12cf669_u128},
        {Sign::POS, -136, 0x9628d26d'7448a43f'9886a71b'25a2085d_u128},
        {Sign::POS, -136, 0x9bf088f8'5c65a56b'70ba9ceb'e0b969c3_u128},
        {Sign::POS, -136, 0xa1b856ac'1236e85b'cd855dc7'05ea2bea_u128},
        {Sign::POS, -136, 0xa7803b89'4f5580e0'7736196b'11afb331_u128},
        {Sign::POS, -136, 0xad483790'cd6339fa'94c99761'b8eab3d8_u128},
        {Sign::POS, -136, 0xb3104ac3'460a9668'6194b8c0'40814736_u128},
        {Sign::POS, -136, 0xb8d87521'72fed130'edde8d24'c7a999cc_u128},
        {Sign::POS, -136, 0xbea0b6ac'0dfbde2f'ea6b01eb'de42f1d0_u128},
        {Sign::POS, -136, 0xc4690f63'd0c66aa1'7ef732b6'9334cf50_u128},
        {Sign::POS, -136, 0xca317f49'752bddae'2ba86275'fcfc2d72_u128},
        {Sign::POS, -136, 0xcffa065d'b50258f6'b56ea44e'185bf99f_u128},
        {Sign::POS, -136, 0xd5c2a4a1'4a28b920'1d5c3bbe'b6902bfe_u128},
        {Sign::POS, -136, 0xdb8b5a14'ee86965f'a2f2bb9e'156b0f37_u128},
        {Sign::POS, -136, 0xe15426b9'5c0c4506'd166eb8d'a06ab5ef_u128},
        {Sign::POS, -136, 0xe71d0a8f'4cb2d60f'97dc7bae'4219de0f_u128},
        {Sign::POS, -136, 0xece60597'7a7c17a8'6c9a8e76'98f416c4_u128},
        {Sign::POS, -136, 0xf2af17d2'9f7295c0'7b3a20aa'5289695e_u128},
        {Sign::POS, -136, 0xf8784141'75a99a93'ddcf578e'e2c2897b_u128},
        {Sign::POS, -136, 0xfe4181e4'b73d2f37'e10ebd96'c3ec30ec_u128},
        {Sign::POS, -135, 0x82056cde'8f290e13'a9b7baec'b34ba577_u128},
        {Sign::POS, -135, 0x8430f56d'5e1edfd1'2da910dc'61c182da_u128},
        {Sign::POS, -135, 0x8715b5a8'f27bed90'faca09dc'7e0ba8b5_u128},
        {Sign::POS, -135, 0x89fa8180'19a2cace'0d723876'173c0947_u128},
        {Sign::POS, -135, 0x8cdf58f3'30b64515'4e6651df'154e8f8c_u128},
        {Sign::POS, -135, 0x8fc43c02'94dd8af3'ee54b77d'3bc34b6d_u128},
        {Sign::POS, -135, 0x92a92aae'a3442c3d'ad07dde9'b5f92cce_u128},
        {Sign::POS, -135, 0x958e24f7'b91a1a53'261aacf9'44b638f0_u128},
        {Sign::POS, -135, 0x98732ade'3393a868'232f5d64'a85b219d_u128},
        {Sign::POS, -135, 0x9b583c62'6fe98bc9'f3a958bb'706093fc_u128},
        {Sign::POS, -135, 0x9e3d5984'cb58dc25'c9eaa059'e7b0333a_u128},
        {Sign::POS, -135, 0xa1228245'a32313cf'1e154029'663243c0_u128},
        {Sign::POS, -135, 0xa407b6a5'548e1006'16515200'e283d006_u128},
        {Sign::POS, -135, 0xa6ecf6a4'3ce4113d'f498168a'3337ca4f_u128},
        {Sign::POS, -135, 0xa9d24242'b973bb63'8a04a89f'0548a10f_u128},
        {Sign::POS, -135, 0xacb79981'27901623'afaad01f'25772805_u128},
        {Sign::POS, -135, 0xaf9cfc5f'e4908d31'c4f47950'543fe0b8_u128},
        {Sign::POS, -135, 0xb2826adf'4dd0f08e'338655e6'77d0d3ec_u128},
        {Sign::POS, -135, 0xb567e4ff'c0b174cc'f8ac2ce1'9d009541_u128},
        {Sign::POS, -135, 0xb84d6ac1'9a96b35c'344d5e7d'd7b2f465_u128},
        {Sign::POS, -135, 0xbb32fc25'38e9aaca'bd6a217f'b4598ec7_u128},
        {Sign::POS, -135, 0xbe18992a'f917bf0e'bc21ff36'8f562b75_u128},
        {Sign::POS, -135, 0xc0fe41d3'3892b9cc'4944139c'cbf2cb9a_u128},
        {Sign::POS, -135, 0xc3e3f61e'54d0ca9c'1369970c'8b67e6b5_u128},
        {Sign::POS, -135, 0xc6c9b60c'ab4c8752'099b370e'2d04a530_u128},
        {Sign::POS, -135, 0xc9af819e'9984ec44'0b81c3d4'8aff589f_u128},
        {Sign::POS, -135, 0xcc9558d4'7cfd5c90'9f22b809'93be311b_u128},
        {Sign::POS, -135, 0xcf7b3bae'b33da265'ac29209c'8d8985ae_u128},
        {Sign::POS, -135, 0xd2612a2d'99d1ef47'3cbb6a52'0292351d_u128},
        {Sign::POS, -135, 0xd5472451'8e4adc56'43de9ae4'0507ef24_u128},
        {Sign::POS, -135, 0xd82d2a1a'ee3d6a97'69677b90'2ea4df3a_u128},
        {Sign::POS, -135, 0xdb133b8a'17430339'db7a3aff'74967bd5_u128},
        {Sign::POS, -135, 0xddf9589f'66f977de'25990c82'a0066ac6_u128},
        {Sign::POS, -135, 0xe0df815b'3b0302dd'0d424aac'f4babf55_u128},
        {Sign::POS, -135, 0xe30c278d'9936c595'f8e3e7eb'5a7bdebb_u128},
        {Sign::POS, -135, 0xe5f264ad'b62d5810'5ef8bf5a'df5deebe_u128},
        {Sign::POS, -135, 0xe8d8ad75'590bdf92'331d1996'5368fc82_u128},
        {Sign::POS, -135, 0xebbf01e4'df85219e'901c30c4'27e358b8_u128},
        {Sign::POS, -135, 0xeea561fc'a7504dc1'aeac7e98'57253b06_u128},
        {Sign::POS, -135, 0xf18bcdbd'0e28fdd7'e2113e58'93ab5b40_u128},
        {Sign::POS, -135, 0xf4724526'71cf3654'9a4efc80'ae977826_u128},
        {Sign::POS, -135, 0xf758c839'30076689'6bf3ba83'19332c9f_u128},
        {Sign::POS, -135, 0xfa3f56f5'a69a68ed'1d732d30'2e75018b_u128},
        {Sign::POS, -135, 0xfd25f15c'33558362'ba179c5d'bcceec01_u128},
        {Sign::POS, -134, 0x80064bb6'9a0533c0'5543f53b'8ad85039_u128},
        {Sign::POS, -134, 0x8179a494'8347996b'e971a556'5b93cb67_u128},
        {Sign::POS, -134, 0x82ed0348'045f379d'5b399644'ba714691_u128},
        {Sign::POS, -134, 0x846067d1'4c3b8982'5079f1e0'ec4b8496_u128},
        {Sign::POS, -134, 0x85d3d230'89ce40b0'6aba4990'a32e8873_u128},
        {Sign::POS, -134, 0x87474265'ec0b4548'e16770c3'a404291c_u128},
        {Sign::POS, -134, 0x88bab871'a1e8b61c'1edb7ffb'1d6b3eab_u128},
        {Sign::POS, -134, 0x8a2e3453'da5ee8cd'603243e1'ba7c7865_u128},
        {Sign::POS, -134, 0x8ba1b60c'c46869f6'57ea5c03'ea4621dd_u128},
        {Sign::POS, -134, 0x8d153d9c'8f01fd4a'd3534cbf'43bd7fd8_u128},
        {Sign::POS, -134, 0x8e88cb03'692a9dbc'62c8c807'5dc91cd5_u128},
        {Sign::POS, -134, 0x8ffc5e41'81e37d9e'04bb70a5'e3db7b85_u128},
        {Sign::POS, -134, 0x916ff757'083006c7'd3875ba3'2159547a_u128},
        {Sign::POS, -134, 0x9286adfc'a91ba28d'5c94c80e'7a8f66b1_u128},
        {Sign::POS, -134, 0x93fa514b'a0517623'52d313c4'7b4f91db_u128},
        {Sign::POS, -134, 0x956dfa72'866fc57d'80829e9f'3957a4c3_u128},
        {Sign::POS, -134, 0x96e1a971'8a824be5'1cd49179'72015ae7_u128},
        {Sign::POS, -134, 0x98555e48'db96fcd2'1af23c29'ef3032da_u128},
        {Sign::POS, -134, 0x99c918f8'a8be040e'e7f7bf24'0be67b80_u128},
        {Sign::POS, -134, 0x9b3cd981'2109c5dc'2bbe3cd4'f7d868fa_u128},
        {Sign::POS, -134, 0x9cb09fe2'738edf14'8c75d6a4'c5ae460d_u128},
        {Sign::POS, -134, 0x9e246c1c'cf642550'750fb989'c9a06186_u128},
        {Sign::POS, -134, 0x9f983e30'63a2a709'de787e24'4901bdf9_u128},
        {Sign::POS, -134, 0xa10c161d'5f65abc0'1ba3205f'f729efa4_u128},
        {Sign::POS, -134, 0xa27ff3e3'f1cab41b'a864d2a0'38fb19cd_u128},
        {Sign::POS, -134, 0xa3f3d784'49f17a11'fb21f083'a5fec56d_u128},
        {Sign::POS, -134, 0xa567c0fe'96fbf109'594c5552'bcc377f5_u128},
        {Sign::POS, -134, 0xa6dbb053'080e45fc'aeb35a35'3fc5a503_u128},
        {Sign::POS, -134, 0xa84fa581'cc4edf9f'67a5c051'30c0f330_u128},
        {Sign::POS, -134, 0xa9c3a08b'12e65e81'4de5cafd'e1caf46f_u128},
        {Sign::POS, -134, 0xab37a16f'0aff9d32'686fce3d'160e88fd_u128},
        {Sign::POS, -134, 0xacaba82d'e3c7b066'de1375b3'af6749a6_u128},
        {Sign::POS, -134, 0xadc2b114'c632da56'24356904'8ac4affe_u128},
        {Sign::POS, -134, 0xaf36c213'19b80ea2'd6796227'dcd39551_u128},
        {Sign::POS, -134, 0xb0aad8ec'cfb38d51'abc92653'86172074_u128},
        {Sign::POS, -134, 0xb21ef5a2'175ac65e'0caac9f1'7896f2ce_u128},
        {Sign::POS, -134, 0xb3931833'1fe56492'1c65a3c7'f828972b_u128},
        {Sign::POS, -134, 0xb50740a0'188d4daa'abdc6644'6a4286d9_u128},
        {Sign::POS, -134, 0xb67b6ee9'308ea27b'2f3bbe8e'8d72abec_u128},
        {Sign::POS, -134, 0xb7efa30e'9727bf11'b67dbdd7'f03d168c_u128},
    },
    // -log2(r) for the third step, generated by SageMath with:
    //
    // for i in range(-80, 81):
    //   r = 2^-21 * round( 2^21 / (1 + i*2^(-21)) );
    //   s, m, e = RealField(128)(r).log2().sign_mantissa_exponent();
    //   print("{Sign::NEG," if (s == 1) else "{Sign::POS,", e, ",
    //         format_hex(m), "},");
    /* .step_3 = */
    {
        {Sign::NEG, -142, 0xe6d3a96b'978fc16e'26f2c63c'0827ccbb_u128},
        {Sign::NEG, -142, 0xe3f107a9'fbfc50ca'4b56fe66'7c8ec091_u128},
        {Sign::NEG, -142, 0xe10e65d1'4b937265'647d7618'1aec10fc_u128},
        {Sign::NEG, -142, 0xde2bc3e1'8653b4f5'99e8f4d5'379eca79_u128},
        {Sign::NEG, -142, 0xdb4921da'ac3ba730'f07da899'90c20623_u128},
        {Sign::NEG, -142, 0xd8667fbc'bd49d7cd'4a812184'8531851a_u128},
        {Sign::NEG, -142, 0xd583dd87'b97cd580'679a4d85'4ae13619_u128},
        {Sign::NEG, -142, 0xd2a13b3b'a0d32eff'e4d17407'2487a514_u128},
        {Sign::NEG, -142, 0xcfbe98d8'734b7301'3c90319d'969b54be_u128},
        {Sign::NEG, -142, 0xccdbf65e'30e43039'c6a173b0'9ba301e6_u128},
        {Sign::NEG, -142, 0xc9f953cc'd99bf55e'b8317428'd7d8d06b_u128},
        {Sign::NEG, -142, 0xc716b124'6d715125'23cdb51b'cc2061cd_u128},
        {Sign::NEG, -142, 0xc4340e64'ec62d241'f964fc78'084fd515_u128},
        {Sign::NEG, -142, 0xc1516b8e'566f076a'06474fb1'5ccbb015_u128},
        {Sign::NEG, -142, 0xbe6ec8a0'ab947f51'f525ef6d'0b75b1c3_u128},
        {Sign::NEG, -142, 0xbb8c259b'ebd1c8ae'4e13532d'f7ee8da7_u128},
        {Sign::NEG, -142, 0xb8a98280'17257233'76832500'd72a9027_u128},
        {Sign::NEG, -142, 0xb5c6df4d'2d8e0a95'b14a3d28'5e592ba0_u128},
        {Sign::NEG, -142, 0xb2e43c03'2f0a2089'1e9e9dc9'711f6e20_u128},
        {Sign::NEG, -142, 0xb00198a2'1b9842c1'bc176e97'4f255fac_u128},
        {Sign::NEG, -142, 0xad1ef529'f336fff3'64acf87f'c0f648e6_u128},
        {Sign::NEG, -142, 0xaa3c519a'b5e4e6d1'd0b8a157'4433e1f8_u128},
        {Sign::NEG, -142, 0xa759adf4'63a08610'95f4e785'371c69a9_u128},
        {Sign::NEG, -142, 0xa4770a36'fc686c63'277d5db0'0363a46f_u128},
        {Sign::NEG, -142, 0xa1946662'803b287c'd5cea669'485ec36c_u128},
        {Sign::NEG, -142, 0x9eb1c276'ef174910'cec66fda'04833322_u128},
        {Sign::NEG, -142, 0x9bcf1e74'48fb5cd2'1da36f6e'be3851db_u128},
        {Sign::NEG, -142, 0x98ec7a5a'8de5f273'ab055d83'abfc0d82_u128},
        {Sign::NEG, -142, 0x9609d629'bdd598a8'3cecf110'dbda68e9_u128},
        {Sign::NEG, -142, 0x932731e1'd8c8de22'76bbdb56'5a37e84b_u128},
        {Sign::NEG, -142, 0x90448d82'debe5194'd934c388'57eee4f3_u128},
        {Sign::NEG, -142, 0x8d61e90c'cfb481b1'c27b427b'4fbfc7db_u128},
        {Sign::NEG, -142, 0x8a7f447f'aba9fd2b'6e13de50'2b142b39_u128},
        {Sign::NEG, -142, 0x879c9fdb'729d52b3'f4e40620'6614e2ba_u128},
        {Sign::NEG, -142, 0x84b9fb20'248d10fd'4d320daa'3312ea6c_u128},
        {Sign::NEG, -142, 0x81d7564d'c177c6b9'4aa528fc'9d433c1a_u128},
        {Sign::NEG, -143, 0xfde962c8'92b80533'3c8ad047'559b1622_u128},
        {Sign::NEG, -143, 0xf82418c7'7870a69f'acf765a8'fc5bcc31_u128},
        {Sign::NEG, -143, 0xf25ece98'34168f1a'be238832'edd27f20_u128},
        {Sign::NEG, -143, 0xec99843a'c5a6dc07'02644bfc'a329b708_u128},
        {Sign::NEG, -143, 0xe6d439af'2d1eaac6'c6d05a78'8e614744_u128},
        {Sign::NEG, -143, 0xe10eeef5'6a7b18bc'133fe9cc'57a8c1d0_u128},
        {Sign::NEG, -143, 0xdb49a40d'7db94348'aa4cb429'195fb5dd_u128},
        {Sign::NEG, -143, 0xd58458f7'66d647ce'0951ef23'9abbb959_u128},
        {Sign::NEG, -143, 0xcfbf0db3'25cf43ad'686c430c'89143d35_u128},
        {Sign::NEG, -143, 0xc9f9c240'baa15447'ba79c248'afd42c12_u128},
        {Sign::NEG, -143, 0xc43476a0'254996fd'ad19e0a9'2f115327_u128},
        {Sign::NEG, -143, 0xbe6f2ad1'65c5292f'a8ad6ac3'b0c99520_u128},
        {Sign::NEG, -143, 0xb8a9ded4'7c11283d'd0567d4a'9cc5e6a1_u128},
        {Sign::NEG, -143, 0xb2e492a9'682ab188'01f87c65'4b231443_u128},
        {Sign::NEG, -143, 0xad1f4650'2a0ee26d'd6380b08'358051bc_u128},
        {Sign::NEG, -143, 0xa759f9c8'c1bad84e'a07b024d'26d391f6_u128},
        {Sign::NEG, -143, 0xa194ad13'2f2bb089'6ee868cb'69e3a7d8_u128},
        {Sign::NEG, -143, 0x9bcf602f'725e887d'0a6869ef'f6682f73_u128},
        {Sign::NEG, -143, 0x960a131d'8b507d87'f6a44d55'9ccf3f61_u128},
        {Sign::NEG, -143, 0x9044c5dd'79fead08'72066e1d'30a8e210_u128},
        {Sign::NEG, -143, 0x8a7f786f'3e66345c'75ba3245'b1b856af_u128},
        {Sign::NEG, -143, 0x84ba2ad2'd88430e1'b5ac0204'73ab198f_u128},
        {Sign::NEG, -144, 0xfde9ba10'90ab7feb'41127e3a'88eb6741_u128},
        {Sign::NEG, -144, 0xf25f1e1f'1baffdea'bf807875'22aca1c4_u128},
        {Sign::NEG, -144, 0xe6d481d1'5210167b'af00688b'14fa3adc_u128},
        {Sign::NEG, -144, 0xdb49e527'33c60457'4d72837c'8ab4d1e5_u128},
        {Sign::NEG, -144, 0xcfbf4820'c0cc0236'4e38ac27'bb252090_u128},
        {Sign::NEG, -144, 0xc434aabd'f91c4ad0'da3661f9'292f59e8_u128},
        {Sign::NEG, -144, 0xb8aa0cfe'dcb118de'8fd0af9b'dfd21488_u128},
        {Sign::NEG, -144, 0xad1f6ee3'6b84a716'82ee19a9'abf0bfa5_u128},
        {Sign::NEG, -144, 0xa194d06b'a591302f'3cf68d5b'5369a251_u128},
        {Sign::NEG, -144, 0x960a3197'8ad0eede'bcd34f38'c977647e_u128},
        {Sign::NEG, -144, 0x8a7f9267'1b3e1dda'76eee9c9'605e2143_u128},
        {Sign::NEG, -145, 0xfde9e5b4'ada5efae'aa6a3887'f0c803ab_u128},
        {Sign::NEG, -145, 0xe6d4a5e2'7b136f13'6e25927e'582ac191_u128},
        {Sign::NEG, -145, 0xcfbf6557'9eb92f4a'e2ebcac2'f3a8e9eb_u128},
        {Sign::NEG, -145, 0xb8aa2414'188ba5bb'9d9acc22'd5690751_u128},
        {Sign::NEG, -145, 0xa194e217'e87f47cb'1e12604b'6d4132ef_u128},
        {Sign::NEG, -145, 0x8a7f9f63'0e888add'cf340d2a'cb9b92a9_u128},
        {Sign::NEG, -146, 0xe6d4b7eb'1537c8ae'0dc5e49f'bde3c520_u128},
        {Sign::NEG, -146, 0xb8aa2f9e'b95b9332'0c074c95'57c01188_u128},
        {Sign::NEG, -146, 0x8a7fa5e1'09656009'f0f82818'ff9b654f_u128},
        {Sign::NEG, -147, 0xb8aa3564'0a7c33eb'd4cd6120'78bbe9b0_u128},
        {Sign::NEG, -148, 0xb8aa3846'b33aaecf'f08cf68f'42e09fa0_u128},
        {Sign::POS, 0, 0_u128},
        {Sign::POS, -148, 0xb8aa3e0c'0513f9b1'68bd0fac'df0ddaaf_u128},
        {Sign::POS, -147, 0xb8aa40ee'ae2ec9b3'192af653'dd41575b_u128},
        {Sign::POS, -146, 0x8a7fb2dd'018e4892'3b5c8984'2e540a51_u128},
        {Sign::POS, -146, 0xb8aa46b4'00c0bee3'34ad8ebd'd8b2750c_u128},
        {Sign::POS, -146, 0xe6d4dbfc'54c5dd1b'70b12bd6'98e5be74_u128},
        {Sign::POS, -145, 0x8a7fb95a'feda5c46'08c7e424'efbd90e1_u128},
        {Sign::POS, -145, 0xa1950570'7dd23344'31b8eba7'74a1de77_u128},
        {Sign::POS, -145, 0xb8aa523e'a755fe32'ee400e8c'68838733_u128},
        {Sign::POS, -145, 0xcfbf9fc5'7b7147be'0e71fa0b'5603bc2f_u128},
        {Sign::POS, -145, 0xe6d4ee04'fa2f9a92'7763c919'd8ac65f1_u128},
        {Sign::POS, -145, 0xfdea3cfd'239c815e'232b270b'b6046ec1_u128},
        {Sign::POS, -144, 0x8a7fc656'fbe1c368'106f3919'7e068972_u128},
        {Sign::POS, -144, 0x960a6e8b'bb581acc'4a4a6f40'12941bd9_u128},
        {Sign::POS, -144, 0xa195171c'd0370c34'5bb34c11'20b3e54b_u128},
        {Sign::POS, -144, 0xad1fc00a'3a845cf9'6bb67313'92a3147a_u128},
        {Sign::POS, -144, 0xb8aa6953'fa45d275'2be1268d'cee3c8fc_u128},
        {Sign::POS, -144, 0xc43512fa'0f813201'd84158d5'd50251a9_u128},
        {Sign::POS, -144, 0xcfbfbcfc'7a3c40fa'3765bda1'5d0ef0fa_u128},
        {Sign::POS, -144, 0xdb4a675b'3a7cc4b9'9a5ddb55'f9cc27d9_u128},
        {Sign::POS, -144, 0xe6d51216'5048829b'dcba1c59'3d918775_u128},
        {Sign::POS, -144, 0xf25fbd2d'bba53ffd'648be060'e1e30a95_u128},
        {Sign::POS, -144, 0xfdea68a1'7c98c23b'22658dc2'f1bcf6e8_u128},
        {Sign::POS, -143, 0x84ba8a38'c9946759'48ad5162'fb4a236e_u128},
        {Sign::POS, -143, 0x8a7fe04e'ffad9560'db7fe378'9405ce3a_u128},
        {Sign::POS, -143, 0x90453693'609acde3'91b56e2e'4f2e5ed8_u128},
        {Sign::POS, -143, 0x960a8d05'ec5ef390'f8998880'c3bb4d76_u128},
        {Sign::POS, -143, 0x9bcfe3a6'a2fce918'e2b87805'2f67efee_u128},
        {Sign::POS, -143, 0xa1953a75'8477912b'67df3991'93f707c0_u128},
        {Sign::POS, -143, 0xa75a9172'90d1ce78'e51b89e4'd5d095e1_u128},
        {Sign::POS, -143, 0xad1fe89d'c80e83b1'fcbbee4e'dbf9f47d_u128},
        {Sign::POS, -143, 0xb2e53ff7'2a309387'964fbd58'b168371b_u128},
        {Sign::POS, -143, 0xb8aa977e'b73ae0aa'dea7276c'a7acd135_u128},
        {Sign::POS, -143, 0xbe6fef34'6f304dcd'47d33f7e'7afc83a6_u128},
        {Sign::POS, -143, 0xc4354718'5213bda0'892603b3'77909123_u128},
        {Sign::POS, -143, 0xc9fa9f2a'5fe812d6'9f32660a'a06239fb_u128},
        {Sign::POS, -143, 0xcfbff76a'98b03021'cbcc5504'd7407f6c_u128},
        {Sign::POS, -143, 0xd5854fd8'fc6ef834'9608c44d'06402ebe_u128},
        {Sign::POS, -143, 0xdb4aa875'8b274dc1'ca3db560'4a863477_u128},
        {Sign::POS, -143, 0xe1100140'44dc137c'7a024036'206c37d6_u128},
        {Sign::POS, -143, 0xe6d55a39'29902c17'fc2e9be8'90ff7ee3_u128},
        {Sign::POS, -143, 0xec9ab360'39467a47'ecdc275c'60da1b53_u128},
        {Sign::POS, -143, 0xf2600cb5'7401e0c0'2d6571e9'4056607f_u128},
        {Sign::POS, -143, 0xf8256638'd9c54234'e4664401'fd1ca2a7_u128},
        {Sign::POS, -143, 0xfdeabfea'6a93815a'7dbba7dc'b50b3fd7_u128},
        {Sign::POS, -142, 0x81d80ce5'1337c072'd541f90d'853c794b_u128},
        {Sign::POS, -142, 0x84bab9ec'06ae11c5'b08f6539'2ce8b75b_u128},
        {Sign::POS, -142, 0x879d670a'0fae2600'6e969a29'f8462436_u128},
        {Sign::POS, -142, 0x8a80143f'2e396e7d'cfc8cbca'a2bf130c_u128},
        {Sign::POS, -142, 0x8d62c18b'62515c98'b737e48c'19421e68_u128},
        {Sign::POS, -142, 0x90456eee'abf761ac'2a9689b9'97c50c0b_u128},
        {Sign::POS, -142, 0x93281c69'0b2cef13'52381fcc'c774d66b_u128},
        {Sign::POS, -142, 0x960ac9fa'7ff37629'7910cec1'dd92dc10_u128},
        {Sign::POS, -142, 0x98ed77a3'0a4c684a'0cb5866b'baff34cb_u128},
        {Sign::POS, -142, 0x9bd02562'aa3936d0'9d5c02c8'0c702d11_u128},
        {Sign::POS, -142, 0x9eb2d339'5fbb5318'dddad053'6b56e775_u128},
        {Sign::POS, -142, 0xa1958127'2ad42e7e'a3a9505d'7f71247a_u128},
        {Sign::POS, -142, 0xa4782f2c'0b853a5d'e6dfbd5d'210830d7_u128},
        {Sign::POS, -142, 0xa75add48'01cfe812'c2372f44'7bdcfa45_u128},
        {Sign::POS, -142, 0xaa3d8b7b'0db5a8f9'73099fd5'32c14b05_u128},
        {Sign::POS, -142, 0xad2039c5'2f37ee6e'5951eef4'83de2c37_u128},
        {Sign::POS, -142, 0xb002e826'665829cd'f7abe6ff'6da76f1e_u128},
        {Sign::POS, -142, 0xb2e5969e'b317cc74'f354411e'd47c5d7b_u128},
        {Sign::POS, -142, 0xb5c8452e'157847c0'1428a99b'a8f5911f_u128},
        {Sign::POS, -142, 0xb8aaf3d4'8d7b0d0c'44a7c433'0edff2c8_u128},
        {Sign::POS, -142, 0xbb8da292'1b218db6'91f1306a'84e4e07b_u128},
        {Sign::POS, -142, 0xbe705166'be6d3b1c'2bc58de4'0cdf7b6a_u128},
        {Sign::POS, -142, 0xc1530052'775f869a'648680b2'54df1d99_u128},
        {Sign::POS, -142, 0xc435af55'45f9e18e'b136b5ac'e0d6f74d_u128},
        {Sign::POS, -142, 0xc7185e6f'2a3dbd56'a979e6c4'34fad480_u128},
        {Sign::POS, -142, 0xc9fb0da0'242c8b50'0794df56'00c90a5a_u128},
        {Sign::POS, -142, 0xccddbce8'33c7bcd8'a86d8081'4ac18cf1_u128},
        {Sign::POS, -142, 0xcfc06c47'5910c34e'8b8ac57a'9cca2d56_u128},
        {Sign::POS, -142, 0xd2a31bbd'9409100f'd314c7e0'3140001f_u128},
        {Sign::POS, -142, 0xd585cb4a'e4b2147a'c3d4c40e'20b5ec89_u128},
        {Sign::POS, -142, 0xd8687aef'4b0d41ed'c5351d72'9060644e_u128},
        {Sign::POS, -142, 0xdb4b2aaa'c71c09c7'614162e1'e12e445d_u128},
        {Sign::POS, -142, 0xde2dda7d'58dfdd66'44a652ea'df8ede85_u128},
        {Sign::POS, -142, 0xe1108a67'005a2e29'3eb1e02a'f3e52c3c_u128},
        {Sign::POS, -142, 0xe3f33a67'bd8c6d6f'415335a2'53a82aa2_u128},
        {Sign::POS, -142, 0xe6d5ea7f'90780c97'611abb08'33305fe1_u128},
    },
    // -log2(r) for the fourth step, generated by SageMath with:
    //
    // for i in range(-65, 65):
    //   r = 2^-28 * round( 2^28 / (1 + i*2^(-28)) );
    //   s, m, e = RealField(128)(r).log2().sign_mantissa_exponent();
    //   print("{Sign::NEG," if (s == 1) else "{Sign::POS,", e, ",
    //         format_hex(m), "},");
    /* .step_4 = */
    {
        {Sign::NEG, -149, 0xbb8ce299'0b5d0b90'ef1bffe5'65ce0a46_u128},
        {Sign::NEG, -149, 0xb8aa39b8'07a576e4'bea32445'60ca3d99_u128},
        {Sign::NEG, -149, 0xb5c790d6'd5c354df'8b91f71c'eefa31a2_u128},
        {Sign::NEG, -149, 0xb2e4e7f5'75b6a57b'9096e3d6'84001c0e_u128},
        {Sign::NEG, -149, 0xb0023f13'e77f68b3'086054c7'94367f36_u128},
        {Sign::NEG, -149, 0xad1f9632'2b1d9e80'2d9cb330'94afe4de_u128},
        {Sign::NEG, -149, 0xaa3ced50'409146dd'3afa673c'fb3698f3_u128},
        {Sign::NEG, -149, 0xa75a446e'27da61c4'6b27d803'3e4c6450_u128},
        {Sign::NEG, -149, 0xa4779b8b'e0f8ef2f'f8d36b84'd52a477b_u128},
        {Sign::NEG, -149, 0xa194f2a9'6becef1a'1eab86ae'37c03565_u128},
        {Sign::NEG, -149, 0x9eb249c6'c8b6617d'175e8d56'deb4ce2c_u128},
        {Sign::NEG, -149, 0x9bcfa0e3'f7554653'1d9ae241'436519da_u128},
        {Sign::NEG, -149, 0x98ecf800'f7c99d96'6c0ee71a'dfe44325_u128},
        {Sign::NEG, -149, 0x960a4f1d'ca136741'3d68fc7c'2efb522f_u128},
        {Sign::NEG, -149, 0x9327a63a'6e32a34d'cc5781e8'ac28e749_u128},
        {Sign::NEG, -149, 0x9044fd56'e42751b6'5388d5ce'd3a0f5af_u128},
        {Sign::NEG, -149, 0x8d625473'2bf17275'0dab5588'224c7e4a_u128},
        {Sign::NEG, -149, 0x8a7fab8f'45910584'356d5d59'15c94a70_u128},
        {Sign::NEG, -149, 0x879d02ab'31060ade'057d4871'2c69a6a7_u128},
        {Sign::NEG, -149, 0x84ba59c6'ee50827c'b88970ea'e5341d60_u128},
        {Sign::NEG, -149, 0x81d7b0e2'7d706c5a'89402fcb'bfe331bb_u128},
        {Sign::NEG, -150, 0xfdea0ffb'bccb90e3'649fba08'79ca348b_u128},
        {Sign::NEG, -150, 0xf824be32'22612d78'dccd9edf'bab6f777_u128},
        {Sign::NEG, -150, 0xf25f6c68'2ba1ae69'f066b9aa'4636478e_u128},
        {Sign::NEG, -150, 0xec9a1a9d'd88d13ab'14c7b3cb'21578781_u128},
        {Sign::NEG, -150, 0xe6d4c8d3'29235d30'bf4d347b'528f56e1_u128},
        {Sign::NEG, -150, 0xe10f7708'1d648aef'6553e0c9'e1b70799_u128},
        {Sign::NEG, -150, 0xdb4a253c'b5509cdb'7c385b9b'd80c1375_u128},
        {Sign::NEG, -150, 0xd584d370'f0e792e9'795745ac'402f919d_u128},
        {Sign::NEG, -150, 0xcfbf81a4'd0296d0d'd20d3d8c'2625ac1b_u128},
        {Sign::NEG, -150, 0xc9fa2fd8'53162b3c'fbb6dfa2'97551554_u128},
        {Sign::NEG, -150, 0xc434de0b'79adcd6b'6bb0c62c'a2867d91_u128},
        {Sign::NEG, -150, 0xbe6f8c3e'43f0538d'9757893d'57e40877_u128},
        {Sign::NEG, -150, 0xb8aa3a70'b1ddbd97'f407bebd'c8f8c28e_u128},
        {Sign::NEG, -150, 0xb2e4e8a2'c3760b7e'f71dfa6d'08b016be_u128},
        {Sign::NEG, -150, 0xad1f96d4'78b93d37'15f6cde0'2b5543ce_u128},
        {Sign::NEG, -150, 0xa75a4505'd1a752b4'c5eec882'4692d1e9_u128},
        {Sign::NEG, -150, 0xa194f336'ce404bec'7c627794'7172081a_u128},
        {Sign::NEG, -150, 0x9bcfa167'6e8428d2'aeae662d'c45a61ce_u128},
        {Sign::NEG, -150, 0x960a4f97'b272e95b'd22f1d3b'59110455_u128},
        {Sign::NEG, -150, 0x9044fdc7'9a0c8d7c'5c412380'4ab83462_u128},
        {Sign::NEG, -150, 0x8a7fabf7'25511528'c240fd95'b5cecb89_u128},
        {Sign::NEG, -150, 0x84ba5a26'54408055'798b2dea'b82fadc4_u128},
        {Sign::NEG, -151, 0xfdea10aa'4db59ded'eef86988'e2227ddb_u128},
        {Sign::NEG, -151, 0xf25f6d07'3a400203'62e1207c'0209b090_u128},
        {Sign::NEG, -151, 0xe6d4c963'6e202cd4'39897891'13ec7bee_u128},
        {Sign::NEG, -151, 0xdb4a25be'e9561e49'5daa6556'5e562909_u128},
        {Sign::NEG, -151, 0xcfbf8219'abe1d64b'b9fcd606'2a84acbd_u128},
        {Sign::NEG, -151, 0xc434de73'b5c354c4'3939b586'c46792b3_u128},
        {Sign::NEG, -151, 0xb8aa3acd'06fa999b'c619ea6a'7a9ee85e_u128},
        {Sign::NEG, -151, 0xad1f9725'9f87a4bb'4b5656ef'9e7a27fd_u128},
        {Sign::NEG, -151, 0xa194f37d'7f6a760b'b3a7d900'83f7239c_u128},
        {Sign::NEG, -151, 0x960a4fd4'a6a30d75'e9c74a33'81c0f016_u128},
        {Sign::NEG, -151, 0x8a7fac2b'15316ae2'd86d7fca'f12ed012_u128},
        {Sign::NEG, -152, 0xfdea1101'962b1c76'd4a6956a'5c863e0f_u128},
        {Sign::NEG, -152, 0xe6d4c9ab'909eeed1'1462ef19'2f547877_u128},
        {Sign::NEG, -152, 0xcfbf8254'19be4ca6'45819d2f'1d72eb8b_u128},
        {Sign::NEG, -152, 0xb8aa3afb'318935c8'3d742790'eedbe719_u128},
        {Sign::NEG, -152, 0xa194f3a0'd7ffaa08'd1ac0d7b'70d74492_u128},
        {Sign::NEG, -152, 0x8a7fac45'0d21a939'd79ac583'75f83d0c_u128},
        {Sign::NEG, -153, 0xe6d4c9cf'a1de665a'49637b2b'ac367e87_u128},
        {Sign::NEG, -153, 0xb8aa3b12'46d08f69'1cc4b5ee'dcc78b35_u128},
        {Sign::NEG, -153, 0x8a7fac52'0919cd43'd43bf48a'42745836_u128},
        {Sign::NEG, -154, 0xb8aa3b1d'd1743f1c'3557bdcf'592619eb_u128},
        {Sign::NEG, -155, 0xb8aa3b23'96c617ae'6bdc2e83'd3ebb0c4_u128},
        {Sign::POS, 0, 0_u128},
        {Sign::POS, -155, 0xb8aa3b2f'2169ca44'2d5b4005'0e44e8ab_u128},
        {Sign::POS, -154, 0xb8aa3b34'e6bba447'b8560371'b8f04afe_u128},
        {Sign::POS, -153, 0x8a7fac6c'010a1f14'c79a43cc'c70459cc_u128},
        {Sign::POS, -153, 0xb8aa3b40'715f59c0'22c25632'f519f77f_u128},
        {Sign::POS, -153, 0xe6d4ca17'c45d8282'42c10a31'4e35fb9e_u128},
        {Sign::POS, -152, 0x8a7fac78'fd024cdb'be5a212e'd7b949e4_u128},
        {Sign::POS, -152, 0xa194f3e7'892a4fde'12dcf94e'f5c5b918_u128},
        {Sign::POS, -152, 0xb8aa3b57'86a6ca76'49781013'e57110ce_u128},
        {Sign::POS, -152, 0xcfbf82c8'f577bcd2'8cba70c0'85c12cb3_u128},
        {Sign::POS, -152, 0xe6d4ca3b'd59d2721'07332f3f'b09328b8_u128},
        {Sign::POS, -152, 0xfdea11b0'2717098f'e3716824'3a9d8b14_u128},
        {Sign::POS, -151, 0x8a7fac92'f4f2b226'a6022054'79b93722_u128},
        {Sign::POS, -151, 0x960a504e'8f041bc3'b5bd7358'52c0d583_u128},
        {Sign::POS, -151, 0xa194f40a'e1bfc1b6'36324863'0b0d812d_u128},
        {Sign::POS, -151, 0xad1f97c7'ed25a415'3ca83f0e'02b823c0_u128},
        {Sign::POS, -151, 0xb8aa3b85'b135c2f7'de66fb46'974bc4fd_u128},
        {Sign::POS, -151, 0xc434df44'2df01e75'30b6254e'23c69fc2_u128},
        {Sign::POS, -151, 0xcfbf8303'6354b6a4'48dd69ba'009b370c_u128},
        {Sign::POS, -151, 0xdb4a26c3'51638b9c'3c247973'83b16af5_u128},
        {Sign::POS, -151, 0xe6d4ca83'f81c9d74'1fd309b8'00678db7_u128},
        {Sign::POS, -151, 0xf25f6e45'577fec43'0930d418'c79378a3_u128},
        {Sign::POS, -151, 0xfdea1207'6f8d7820'0d85967b'2783a12c_u128},
        {Sign::POS, -150, 0x84ba5ae5'2022a091'210c898c'360016ed_u128},
        {Sign::POS, -150, 0x8a7facc6'e4d3a3b0'5e19883e'ef2605ab_u128},
        {Sign::POS, -150, 0x9044fea9'05d9c579'488dacc6'629300ae_u128},
        {Sign::POS, -150, 0x960a508b'833505f7'6b0cdebd'3264e3e3_u128},
        {Sign::POS, -150, 0x9bcfa26e'5ce56536'503b07e7'ff788dc2_u128},
        {Sign::POS, -150, 0xa194f451'92eae341'82bc1435'696a69d1_u128},
        {Sign::POS, -150, 0xa75a4635'25458024'8d33f1be'0e96fb1f_u128},
        {Sign::POS, -150, 0xad1f9819'13f53bea'fa4690c4'8c1b66c9_u128},
        {Sign::POS, -150, 0xb2e4e9fd'5efa16a0'5497e3b5'7dd5fe75_u128},
        {Sign::POS, -150, 0xb8aa3be2'06541050'26cbdf27'7e66cad5_u128},
        {Sign::POS, -150, 0xbe6f8dc7'0a032905'fb8679db'27301625_u128},
        {Sign::POS, -150, 0xc434dfac'6a0760cd'5d6bacbb'1056f6aa_u128},
        {Sign::POS, -150, 0xc9fa3192'2660b7b1'd71f72db'd0c3d936_u128},
        {Sign::POS, -150, 0xcfbf8378'3f0f2dbe'f345c97b'fe230ba2_u128},
        {Sign::POS, -150, 0xd584d55e'b412c300'3c82b004'2ce54751_u128},
        {Sign::POS, -150, 0xdb4a2745'856b7781'3d7a2806'f0403bae_u128},
        {Sign::POS, -150, 0xe10f792c'b3194b4d'80d03540'da2f18ae_u128},
        {Sign::POS, -150, 0xe6d4cb14'3d1c3e70'9128dd98'7b73194f_u128},
        {Sign::POS, -150, 0xec9a1cfc'237450f5'f928291e'63940e14_u128},
        {Sign::POS, -150, 0xf25f6ee4'662182e9'4372220d'20e0e78a_u128},
        {Sign::POS, -150, 0xf824c0cd'0523d455'faaad4c9'407040c7_u128},
        {Sign::POS, -150, 0xfdea12b6'007b4547'a9764fe1'4e20e9e4_u128},
        {Sign::POS, -149, 0x81d7b24f'ac13eae4'ed3c5206'ea4d3942_u128},
        {Sign::POS, -149, 0x84ba5b44'8614c2f4'0c2af218'aea6da27_u128},
        {Sign::POS, -149, 0x879d0439'8e402ad6'f6d912ac'383aaeba_u128},
        {Sign::POS, -149, 0x8a7fad2e'c4962293'7298bf5c'ca8b3d95_u128},
        {Sign::POS, -149, 0x8d625624'2916aa2f'44bc04da'a8808214_u128},
        {Sign::POS, -149, 0x9044ff19'bbc1c1b0'3294f0eb'14683198_u128},
        {Sign::POS, -149, 0x9327a80f'7c97691c'01759268'4ff600c3_u128},
        {Sign::POS, -149, 0x960a5105'6b97a078'76aff941'9c43e8b9_u128},
        {Sign::POS, -149, 0x98ecf9fb'88c267cb'5796367b'39d26c63_u128},
        {Sign::POS, -149, 0x9bcfa2f1'd417bf1a'697a5c2e'6888ddaa_u128},
        {Sign::POS, -149, 0x9eb24be8'4d97a66b'71ae7d89'67b5a2b7_u128},
        {Sign::POS, -149, 0xa194f4de'f5421dc4'3584aecf'760e7b39_u128},
        {Sign::POS, -149, 0xa4779dd5'cb17252a'7a4f0558'd1b0c59e_u128},
        {Sign::POS, -149, 0xa75a46cc'cf16bca4'055f9792'b821c455_u128},
        {Sign::POS, -149, 0xaa3cefc4'0140e436'9c087cff'664ee311_u128},
        {Sign::POS, -149, 0xad1f98bb'61959be8'039bce36'188dfc04_u128},
        {Sign::POS, -149, 0xb00241b2'f014e3be'016ba4e3'0a9d9d21_u128},
        {Sign::POS, -149, 0xb2e4eaaa'acbebbbe'5aca1bc7'77a54d5e_u128},
        {Sign::POS, -149, 0xb5c793a2'979323ee'd5094eb9'9a35d1f0_u128},
        {Sign::POS, -149, 0xb8aa3c9a'b0921c55'357b5aa4'ac49738d_u128},
    }};

// > P = fpminimax(log2(1 + x)/x, 3, [|128...|], [-0x1.0002143p-29 , 0x1p-29]);
// > P;
// > dirtyinfnorm(log2(1 + x)/x - P, [-0x1.0002143p-29 , 0x1p-29]);
// 0x1.27ad5...p-121
LIBC_INLINE_VAR constexpr Float128 BIG_COEFFS[4]{
    {Sign::NEG, -129, 0xb8aa3b29'5c2b21e3'3eccf694'0d66bbcc_u128},
    {Sign::POS, -129, 0xf6384ee1'd01febc9'ee39a6d6'49394bb1_u128},
    {Sign::NEG, -128, 0xb8aa3b29'5c17f0bb'be87fed0'67ea2ad5_u128},
    {Sign::POS, -127, 0xb8aa3b29'5c17f0bb'be87fed0'691d3e3f_u128},
};

// Reuse the output of the fast pass range reduction.
// -2^-8 <= m_x < 2^-7
LIBC_INLINE double log2_accurate(int e_x, int index, double m_x) {

  Float128 sum(static_cast<float>(e_x));
  sum = fputil::quick_add(sum, LOG2_TABLE.step_1[index]);

  Float128 v_f128 = log_range_reduction(m_x, LOG2_TABLE, sum);

  // Polynomial approximation
  Float128 p = fputil::quick_mul(v_f128, BIG_COEFFS[0]);
  p = fputil::quick_mul(v_f128, fputil::quick_add(p, BIG_COEFFS[1]));
  p = fputil::quick_mul(v_f128, fputil::quick_add(p, BIG_COEFFS[2]));
  p = fputil::quick_mul(v_f128, fputil::quick_add(p, BIG_COEFFS[3]));

  Float128 r = fputil::quick_add(sum, p);

  return static_cast<double>(r);
}
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS

} // namespace log2_internal

LIBC_INLINE double log2(double x) {
  using namespace log2_internal;
  using namespace common_constants_internal;
  using FPBits_t = typename fputil::FPBits<double>;

  FPBits_t xbits(x);
  uint64_t x_u = xbits.uintval();

  int x_e = -FPBits_t::EXP_BIAS;

  if (LIBC_UNLIKELY(xbits == FPBits_t::one())) {
    // log2(1.0) = +0.0
    return 0.0;
  }

  if (LIBC_UNLIKELY(xbits.uintval() < FPBits_t::min_normal().uintval() ||
                    xbits.uintval() > FPBits_t::max_normal().uintval())) {
    if (x == 0.0) {
      // return -Inf and raise FE_DIVBYZERO.
      fputil::set_errno_if_required(ERANGE);
      fputil::raise_except_if_required(FE_DIVBYZERO);
      return FPBits_t::inf(Sign::NEG).get_val();
    }
    if (xbits.is_neg() && !xbits.is_nan()) {
      fputil::set_errno_if_required(EDOM);
      fputil::raise_except_if_required(FE_INVALID);
      return FPBits_t::quiet_nan().get_val();
    }
    if (xbits.is_inf_or_nan()) {
      return x;
    }
    // Normalize denormal inputs.
    xbits = FPBits_t(x * 0x1.0p52);
    x_e -= 52;
    x_u = xbits.uintval();
  }

  // log2(x) = log2(2^x_e * x_m)
  //         = x_e + log2(x_m)
  // Range reduction for log2(x_m):
  // For each x_m, we would like to find r such that:
  //   -2^-8 <= r * x_m - 1 < 2^-7
  int shifted = static_cast<int>(x_u >> 45);
  int index = shifted & 0x7F;
  double r = RD[index];

  // Add unbiased exponent. Add an extra 1 if the 8 leading fractional bits are
  // all 1's.
  x_e += static_cast<int>((x_u + (1ULL << 45)) >> 52);
  double e_x = static_cast<double>(x_e);

  // Set m = 1.mantissa.
  uint64_t x_m = (x_u & 0x000F'FFFF'FFFF'FFFFULL) | 0x3FF0'0000'0000'0000ULL;
  double m = FPBits_t(x_m).get_val();

  fputil::DoubleDouble r1;

  // Perform exact range reduction
#ifdef LIBC_TARGET_CPU_HAS_FMA_DOUBLE
  double u = fputil::multiply_add(r, m, -1.0); // exact
#else
  uint64_t c_m = x_m & 0x3FFF'E000'0000'0000ULL;
  double c = FPBits_t(c_m).get_val();
  double u = fputil::multiply_add(r, m - c, CD[index]); // exact
#endif // LIBC_TARGET_CPU_HAS_FMA_DOUBLE

  // Exact sum:
  //   r1.hi + r1.lo = e_x * log(2)_hi - log(r)_hi + u
  r1 = fputil::exact_add(LOG_R1[index].hi, u);

  // Error of u_sq = ulp(u^2);
  double u_sq = u * u;
  // Degree-7 minimax polynomial
  double p0 = fputil::multiply_add(u, LOG_COEFFS[1], LOG_COEFFS[0]);
  double p1 = fputil::multiply_add(u, LOG_COEFFS[3], LOG_COEFFS[2]);
  double p2 = fputil::multiply_add(u, LOG_COEFFS[5], LOG_COEFFS[4]);
  double p = fputil::polyeval(u_sq, LOG_R1[index].lo, p0, p1, p2);

  r1.lo += p;

  // Quick double-double multiplication:
  //   r2.hi + r2.lo ~ r1 * log2(e),
  // with error bounded by:
  //   4*ulp( ulp(r2.hi) )
  fputil::DoubleDouble r2 = fputil::quick_mult(r1, LOG2_E);
  fputil::DoubleDouble r3 = fputil::exact_add(e_x, r2.hi);
  r3.lo += r2.lo;

  // Overall, if we choose sufficiently large constant C, the total error is
  // bounded by (C * ulp(u^2)).

#ifdef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  return r3.hi + r3.lo;
#else
  // Total error is bounded by ~ C * ulp(u^2).
  double err = u_sq * P_ERR;
  // Lower bound from the result
  double left = r3.hi + (r3.lo - err);
  // Upper bound from the result
  double right = r3.hi + (r3.lo + err);

  // Ziv's test if fast pass is accurate enough.
  if (left == right)
    return left;

  return log2_accurate(x_e, index, u);
#endif // LIBC_MATH_HAS_SKIP_ACCURATE_PASS
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_LOG2_H
