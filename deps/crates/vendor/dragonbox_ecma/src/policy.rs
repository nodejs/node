// Translated from C++ to Rust. The original C++ code can be found at
// https://github.com/jk-jeon/dragonbox and carries the following license:
//
// Copyright 2020-2025 Junekey Jeon
//
// The contents of this file may be used under the terms of
// the Apache License v2.0 with LLVM Exceptions.
//
//    (See accompanying file LICENSE-Apache or copy at
//     https://llvm.org/foundation/relicensing/LICENSE.txt)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

use crate::CarrierUint;

pub(crate) fn prefer_round_down(significand: CarrierUint) -> bool {
    significand % 2 != 0
}

// Remove trailing zeros from decimal significand.
// Adapted from upstream's remove_trailing_zeros_traits for binary64.
// See: https://github.com/jk-jeon/rtz_benchmark
// The idea of branchless search is by reddit users r/pigeon768 and r/TheoreticalDumbass.
pub(crate) fn remove_trailing_zeros(significand: &mut CarrierUint, exponent: &mut i32) {
    let mut r = rotr64(significand.wrapping_mul(28999941890838049), 8);
    let mut b = r < 184467440738;
    let mut s = i32::from(b);
    *significand = if b { r } else { *significand };

    r = rotr64(significand.wrapping_mul(182622766329724561), 4);
    b = r < 1844674407370956;
    s = s * 2 + i32::from(b);
    *significand = if b { r } else { *significand };

    r = rotr64(significand.wrapping_mul(10330176681277348905), 2);
    b = r < 184467440737095517;
    s = s * 2 + i32::from(b);
    *significand = if b { r } else { *significand };

    r = rotr64(significand.wrapping_mul(14757395258967641293), 1);
    b = r < 1844674407370955162;
    s = s * 2 + i32::from(b);
    *significand = if b { r } else { *significand };

    *exponent += s;
}

const fn rotr64(n: CarrierUint, r: u32) -> CarrierUint {
    let r = r & 63;
    (n >> r) | (n << ((64 - r) & 63))
}
