//===-- Look-up tables for exp*f functions ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP_FLOAT_CONSTANTS_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP_FLOAT_CONSTANTS_H

#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {
// Lookup table for exp(m) with m = -104, ..., 89.
//   -104 = floor(log(single precision's min denormal))
//     89 = ceil(log(single precision's max normal))
// Table is generated with Sollya as follow:
// > display = hexadecimal;
// > for i from -104 to 89 do { D(exp(i)); };
LIBC_INLINE_VAR constexpr double EXP_M1[195] = {
    0x1.f1e6b68529e33p-151, 0x1.525be4e4e601dp-149, 0x1.cbe0a45f75eb1p-148,
    0x1.3884e838aea68p-146, 0x1.a8c1f14e2af5dp-145, 0x1.20a717e64a9bdp-143,
    0x1.8851d84118908p-142, 0x1.0a9bdfb02d240p-140, 0x1.6a5bea046b42ep-139,
    0x1.ec7f3b269efa8p-138, 0x1.4eafb87eab0f2p-136, 0x1.c6e2d05bbc000p-135,
    0x1.35208867c2683p-133, 0x1.a425b317eeacdp-132, 0x1.1d8508fa8246ap-130,
    0x1.840fbc08fdc8ap-129, 0x1.07b7112bc1ffep-127, 0x1.666d0dad2961dp-126,
    0x1.e726c3f64d0fep-125, 0x1.4b0dc07cabf98p-123, 0x1.c1f2daf3b6a46p-122,
    0x1.31c5957a47de2p-120, 0x1.9f96445648b9fp-119, 0x1.1a6baeadb4fd1p-117,
    0x1.7fd974d372e45p-116, 0x1.04da4d1452919p-114, 0x1.62891f06b3450p-113,
    0x1.e1dd273aa8a4ap-112, 0x1.4775e0840bfddp-110, 0x1.bd109d9d94bdap-109,
    0x1.2e73f53fba844p-107, 0x1.9b138170d6bfep-106, 0x1.175af0cf60ec5p-104,
    0x1.7baee1bffa80bp-103, 0x1.02057d1245cebp-101, 0x1.5eafffb34ba31p-100,
    0x1.dca23bae16424p-99,  0x1.43e7fc88b8056p-97,  0x1.b83bf23a9a9ebp-96,
    0x1.2b2b8dd05b318p-94,  0x1.969d47321e4ccp-93,  0x1.1452b7723aed2p-91,
    0x1.778fe2497184cp-90,  0x1.fe7116182e9ccp-89,  0x1.5ae191a99585ap-87,
    0x1.d775d87da854dp-86,  0x1.4063f8cc8bb98p-84,  0x1.b374b315f87c1p-83,
    0x1.27ec458c65e3cp-81,  0x1.923372c67a074p-80,  0x1.1152eaeb73c08p-78,
    0x1.737c5645114b5p-77,  0x1.f8e6c24b5592ep-76,  0x1.571db733a9d61p-74,
    0x1.d257d547e083fp-73,  0x1.3ce9b9de78f85p-71,  0x1.aebabae3a41b5p-70,
    0x1.24b6031b49bdap-68,  0x1.8dd5e1bb09d7ep-67,  0x1.0e5b73d1ff53dp-65,
    0x1.6f741de1748ecp-64,  0x1.f36bd37f42f3ep-63,  0x1.536452ee2f75cp-61,
    0x1.cd480a1b74820p-60,  0x1.39792499b1a24p-58,  0x1.aa0de4bf35b38p-57,
    0x1.2188ad6ae3303p-55,  0x1.898471fca6055p-54,  0x1.0b6c3afdde064p-52,
    0x1.6b7719a59f0e0p-51,  0x1.ee001eed62aa0p-50,  0x1.4fb547c775da8p-48,
    0x1.c8464f7616468p-47,  0x1.36121e24d3bbap-45,  0x1.a56e0c2ac7f75p-44,
    0x1.1e642baeb84a0p-42,  0x1.853f01d6d53bap-41,  0x1.0885298767e9ap-39,
    0x1.67852a7007e42p-38,  0x1.e8a37a45fc32ep-37,  0x1.4c1078fe9228ap-35,
    0x1.c3527e433fab1p-34,  0x1.32b48bf117da2p-32,  0x1.a0db0d0ddb3ecp-31,
    0x1.1b48655f37267p-29,  0x1.81056ff2c5772p-28,  0x1.05a628c699fa1p-26,
    0x1.639e3175a689dp-25,  0x1.e355bbaee85cbp-24,  0x1.4875ca227ec38p-22,
    0x1.be6c6fdb01612p-21,  0x1.2f6053b981d98p-19,  0x1.9c54c3b43bc8bp-18,
    0x1.18354238f6764p-16,  0x1.7cd79b5647c9bp-15,  0x1.02cf22526545ap-13,
    0x1.5fc21041027adp-12,  0x1.de16b9c24a98fp-11,  0x1.44e51f113d4d6p-9,
    0x1.b993fe00d5376p-8,   0x1.2c155b8213cf4p-6,   0x1.97db0ccceb0afp-5,
    0x1.152aaa3bf81ccp-3,   0x1.78b56362cef38p-2,   0x1.0000000000000p+0,
    0x1.5bf0a8b145769p+1,   0x1.d8e64b8d4ddaep+2,   0x1.415e5bf6fb106p+4,
    0x1.b4c902e273a58p+5,   0x1.28d389970338fp+7,   0x1.936dc5690c08fp+8,
    0x1.122885aaeddaap+10,  0x1.749ea7d470c6ep+11,  0x1.fa7157c470f82p+12,
    0x1.5829dcf950560p+14,  0x1.d3c4488ee4f7fp+15,  0x1.3de1654d37c9ap+17,
    0x1.b00b5916ac955p+18,  0x1.259ac48bf05d7p+20,  0x1.8f0ccafad2a87p+21,
    0x1.0f2ebd0a80020p+23,  0x1.709348c0ea4f9p+24,  0x1.f4f22091940bdp+25,
    0x1.546d8f9ed26e1p+27,  0x1.ceb088b68e804p+28,  0x1.3a6e1fd9eecfdp+30,
    0x1.ab5adb9c43600p+31,  0x1.226af33b1fdc1p+33,  0x1.8ab7fb5475fb7p+34,
    0x1.0c3d3920962c9p+36,  0x1.6c932696a6b5dp+37,  0x1.ef822f7f6731dp+38,
    0x1.50bba3796379ap+40,  0x1.c9aae4631c056p+41,  0x1.370470aec28edp+43,
    0x1.a6b765d8cdf6dp+44,  0x1.1f43fcc4b662cp+46,  0x1.866f34a725782p+47,
    0x1.0953e2f3a1ef7p+49,  0x1.689e221bc8d5bp+50,  0x1.ea215a1d20d76p+51,
    0x1.4d13fbb1a001ap+53,  0x1.c4b334617cc67p+54,  0x1.33a43d282a519p+56,
    0x1.a220d397972ebp+57,  0x1.1c25c88df6862p+59,  0x1.8232558201159p+60,
    0x1.0672a3c9eb871p+62,  0x1.64b41c6d37832p+63,  0x1.e4cf766fe49bep+64,
    0x1.49767bc0483e3p+66,  0x1.bfc951eb8bb76p+67,  0x1.304d6aeca254bp+69,
    0x1.9d97010884251p+70,  0x1.19103e4080b45p+72,  0x1.7e013cd114461p+73,
    0x1.03996528e074cp+75,  0x1.60d4f6fdac731p+76,  0x1.df8c5af17ba3bp+77,
    0x1.45e3076d61699p+79,  0x1.baed16a6e0da7p+80,  0x1.2cffdfebde1a1p+82,
    0x1.9919cabefcb69p+83,  0x1.160345c9953e3p+85,  0x1.79dbc9dc53c66p+86,
    0x1.00c810d464097p+88,  0x1.5d009394c5c27p+89,  0x1.da57de8f107a8p+90,
    0x1.425982cf597cdp+92,  0x1.b61e5ca3a5e31p+93,  0x1.29bb825dfcf87p+95,
    0x1.94a90db0d6fe2p+96,  0x1.12fec759586fdp+98,  0x1.75c1dc469e3afp+99,
    0x1.fbfd219c43b04p+100, 0x1.5936d44e1a146p+102, 0x1.d531d8a7ee79cp+103,
    0x1.3ed9d24a2d51bp+105, 0x1.b15cfe5b6e17bp+106, 0x1.268038c2c0e00p+108,
    0x1.9044a73545d48p+109, 0x1.1002ab6218b38p+111, 0x1.71b3540cbf921p+112,
    0x1.f6799ea9c414ap+113, 0x1.55779b984f3ebp+115, 0x1.d01a210c44aa4p+116,
    0x1.3b63da8e91210p+118, 0x1.aca8d6b0116b8p+119, 0x1.234de9e0c74e9p+121,
    0x1.8bec7503ca477p+122, 0x1.0d0eda9796b90p+124, 0x1.6db0118477245p+125,
    0x1.f1056dc7bf22dp+126, 0x1.51c2cc3433801p+128, 0x1.cb108ffbec164p+129,
};

// Lookup table for exp(m * 2^(-7)) with m = 0, ..., 127.
// Table is generated with Sollya as follow:
// > display = hexadecimal;
// > for i from 0 to 127 do { D(exp(i / 128)); };
LIBC_INLINE_VAR constexpr double EXP_M2[128] = {
    0x1.0000000000000p0, 0x1.0202015600446p0, 0x1.04080ab55de39p0,
    0x1.06122436410ddp0, 0x1.08205601127edp0, 0x1.0a32a84e9c1f6p0,
    0x1.0c49236829e8cp0, 0x1.0e63cfa7ab09dp0, 0x1.1082b577d34edp0,
    0x1.12a5dd543ccc5p0, 0x1.14cd4fc989cd6p0, 0x1.16f9157587069p0,
    0x1.192937074e0cdp0, 0x1.1b5dbd3f68122p0, 0x1.1d96b0eff0e79p0,
    0x1.1fd41afcba45ep0, 0x1.2216045b6f5cdp0, 0x1.245c7613b8a9bp0,
    0x1.26a7793f60164p0, 0x1.28f7170a755fdp0, 0x1.2b4b58b372c79p0,
    0x1.2da4478b620c7p0, 0x1.3001ecf601af7p0, 0x1.32645269ea829p0,
    0x1.34cb8170b5835p0, 0x1.373783a722012p0, 0x1.39a862bd3c106p0,
    0x1.3c1e2876834aap0, 0x1.3e98deaa11dccp0, 0x1.41188f42c3e32p0,
    0x1.439d443f5f159p0, 0x1.462707b2bac21p0, 0x1.48b5e3c3e8186p0,
    0x1.4b49e2ae5ac67p0, 0x1.4de30ec211e60p0, 0x1.50817263c13cdp0,
    0x1.5325180cfacf7p0, 0x1.55ce0a4c58c7cp0, 0x1.587c53c5a7af0p0,
    0x1.5b2fff3210fd9p0, 0x1.5de9176045ff5p0, 0x1.60a7a734ab0e8p0,
    0x1.636bb9a983258p0, 0x1.663559cf1bc7cp0, 0x1.690492cbf9433p0,
    0x1.6bd96fdd034a2p0, 0x1.6eb3fc55b1e76p0, 0x1.719443a03acb9p0,
    0x1.747a513dbef6ap0, 0x1.776630c678bc1p0, 0x1.7a57ede9ea23ep0,
    0x1.7d4f946f0ba8dp0, 0x1.804d30347b546p0, 0x1.8350cd30ac390p0,
    0x1.865a7772164c5p0, 0x1.896a3b1f66a0ep0, 0x1.8c802477b0010p0,
    0x1.8f9c3fd29beafp0, 0x1.92be99a09bf00p0, 0x1.95e73e6b1b75ep0,
    0x1.99163ad4b1dccp0, 0x1.9c4b9b995509bp0, 0x1.9f876d8e8c566p0,
    0x1.a2c9bda3a3e78p0, 0x1.a61298e1e069cp0, 0x1.a9620c6cb3374p0,
    0x1.acb82581eee54p0, 0x1.b014f179fc3b8p0, 0x1.b3787dc80f95fp0,
    0x1.b6e2d7fa5eb18p0, 0x1.ba540dba56e56p0, 0x1.bdcc2cccd3c85p0,
    0x1.c14b431256446p0, 0x1.c4d15e873c193p0, 0x1.c85e8d43f7cd0p0,
    0x1.cbf2dd7d490f2p0, 0x1.cf8e5d84758a9p0, 0x1.d3311bc7822b4p0,
    0x1.d6db26d16cd67p0, 0x1.da8c8d4a66969p0, 0x1.de455df80e3c0p0,
    0x1.e205a7bdab73ep0, 0x1.e5cd799c6a54ep0, 0x1.e99ce2b397649p0,
    0x1.ed73f240dc142p0, 0x1.f152b7a07bb76p0, 0x1.f539424d90f5ep0,
    0x1.f927a1e24bb76p0, 0x1.fd1de6182f8c9p0, 0x1.008e0f64294abp1,
    0x1.02912df5ce72ap1, 0x1.049856cd84339p1, 0x1.06a39207f0a09p1,
    0x1.08b2e7d2035cfp1, 0x1.0ac6606916501p1, 0x1.0cde041b0e9aep1,
    0x1.0ef9db467dcf8p1, 0x1.1119ee5ac36b6p1, 0x1.133e45d82e952p1,
    0x1.1566ea50201d7p1, 0x1.1793e4652cc50p1, 0x1.19c53ccb3fc6bp1,
    0x1.1bfafc47bda73p1, 0x1.1e352bb1a74adp1, 0x1.2073d3f1bd518p1,
    0x1.22b6fe02a3b9cp1, 0x1.24feb2f105cb8p1, 0x1.274afbdbba4a6p1,
    0x1.299be1f3e7f1cp1, 0x1.2bf16e7d2a38cp1, 0x1.2e4baacdb6614p1,
    0x1.30aaa04e80d05p1, 0x1.330e587b62b28p1, 0x1.3576dce33feadp1,
    0x1.37e437282d4eep1, 0x1.3a5670ff972edp1, 0x1.3ccd9432682b4p1,
    0x1.3f49aa9d30590p1, 0x1.41cabe304cb34p1, 0x1.4450d8f00edd4p1,
    0x1.46dc04f4e5338p1, 0x1.496c4c6b832dap1, 0x1.4c01b9950a111p1,
    0x1.4e9c56c731f5dp1, 0x1.513c2e6c731d7p1, 0x1.53e14b042f9cap1,
    0x1.568bb722dd593p1, 0x1.593b7d72305bbp1,
};

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP_FLOAT_CONSTANTS_H
