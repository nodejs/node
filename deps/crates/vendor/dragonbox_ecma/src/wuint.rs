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

use crate::cache::EntryTypeExt as _;

// Get high half of the 128-bit result of multiplication of two 64-bit unsigned
// integers.
pub(crate) fn umul128_upper64(x: u64, y: u64) -> u64 {
    let p = u128::from(x) * u128::from(y);
    (p >> 64) as u64
}

// Get upper 128-bits of multiplication of a 64-bit unsigned integer and a
// 128-bit unsigned integer.
pub(crate) fn umul192_upper128(x: u64, y: u128) -> u128 {
    let mut r = u128::from(x) * u128::from(y.high());
    r += u128::from(umul128_upper64(x, y.low()));
    r
}

// Get lower 128-bits of multiplication of a 64-bit unsigned integer and a
// 128-bit unsigned integer.
pub(crate) fn umul192_lower128(x: u64, y: u128) -> u128 {
    let high = x.wrapping_mul(y.high());
    let high_low = u128::from(x) * u128::from(y.low());
    (u128::from(high) << 64).wrapping_add(high_low)
}
