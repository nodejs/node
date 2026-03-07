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

////////////////////////////////////////////////////////////////////////////////////////
// Utilities for fast/constexpr log computation.
////////////////////////////////////////////////////////////////////////////////////////

const _: () = assert!(
    (-1 >> 1) == -1,
    "right-shift for signed integers must be arithmetic",
);

// Implementations of various log computations using binary approximations. On
// less powerful platforms, using smaller integer types and magic numbers for
// these computations significantly improve codegen, so depending on the range
// of input we try to choose the smallest possible magic numbers that result in
// the best codegen. To generically achieve that goal, we list multiple sets of
// magic numbers with associated ranges of inputs, called "tiers", so that we
// choose the smallest tier whose associated range covers the requested range of
// input.

trait TierInfo {
    const TIER: usize;
    const MULTIPLY: u32;
    const SUBTRACT: u32;
    const SHIFT: u32;
    const MIN_EXPONENT: i32;
    const MAX_EXPONENT: i32;
    type Next: TierInfo;
}

// Check if the given tier covers the requested range. Info<current_tier> is
// supposed to hold the magic numbers needed for the computation and the
// supported range of input (i.e. the range of input where the computation with
// the given magic numbers must be valid).
const fn is_in_range<Info: TierInfo, const MIN_EXPONENT: i32, const MAX_EXPONENT: i32>() -> bool {
    MIN_EXPONENT >= Info::MIN_EXPONENT && MAX_EXPONENT <= Info::MAX_EXPONENT
}

// Generic implementation of log computations.
const fn compute<Info: TierInfo, const MIN_EXPONENT: i32, const MAX_EXPONENT: i32>(e: i32) -> i32 {
    if is_in_range::<Info, MIN_EXPONENT, MAX_EXPONENT>() {
        // Specialization for the case when the given tier covers the requested
        // range of input. In this case, do the computation with the given magic
        // numbers.
        debug_assert!(MIN_EXPONENT <= e && e <= MAX_EXPONENT);
        (e * Info::MULTIPLY as i32 - Info::SUBTRACT as i32) >> Info::SHIFT
    } else {
        // Specialization for the case when the given tier does not cover the
        // requested range of input. In this case, delegate the computation to
        // the next tier. If the next tier does not exist, then the compilation
        // must fail.
        debug_assert!(Info::TIER < Info::Next::TIER);
        compute::<Info::Next, MIN_EXPONENT, MAX_EXPONENT>(e)
    }
}

pub(crate) const FLOOR_LOG10_POW2_MIN_EXPONENT: i32 = -2620;
pub(crate) const FLOOR_LOG10_POW2_MAX_EXPONENT: i32 = 2620;
pub(crate) const fn floor_log10_pow2<const MIN_EXPONENT: i32, const MAX_EXPONENT: i32>(
    e: i32,
) -> i32 {
    enum Info0 {}
    impl TierInfo for Info0 {
        const TIER: usize = 0;
        const MULTIPLY: u32 = 77;
        const SUBTRACT: u32 = 0;
        const SHIFT: u32 = 8;
        const MIN_EXPONENT: i32 = -102;
        const MAX_EXPONENT: i32 = 102;
        type Next = Info1;
    }

    enum Info1 {}
    impl TierInfo for Info1 {
        const TIER: usize = 1;
        // 24-bits are enough in fact.
        const MULTIPLY: u32 = 1233;
        const SUBTRACT: u32 = 0;
        const SHIFT: u32 = 12;
        // Formula itself holds on [-680,680]; [-425,425] is to ensure that
        // the output is within [-127,127].
        const MIN_EXPONENT: i32 = -425;
        const MAX_EXPONENT: i32 = 425;
        type Next = Info2;
    }

    enum Info2 {}
    impl TierInfo for Info2 {
        const TIER: usize = 2;
        const MULTIPLY: u32 = 315653;
        const SUBTRACT: u32 = 0;
        const SHIFT: u32 = 20;
        const MIN_EXPONENT: i32 = -2620;
        const MAX_EXPONENT: i32 = 2620;
        type Next = Self;
    }

    compute::<Info0, MIN_EXPONENT, MAX_EXPONENT>(e)
}

pub(crate) const FLOOR_LOG2_POW10_MIN_EXPONENT: i32 = -1233;
pub(crate) const FLOOR_LOG2_POW10_MAX_EXPONENT: i32 = 1233;
pub(crate) const fn floor_log2_pow10<const MIN_EXPONENT: i32, const MAX_EXPONENT: i32>(
    e: i32,
) -> i32 {
    enum Info0 {}
    impl TierInfo for Info0 {
        const TIER: usize = 0;
        const MULTIPLY: u32 = 53;
        const SUBTRACT: u32 = 0;
        const SHIFT: u32 = 4;
        const MIN_EXPONENT: i32 = -15;
        const MAX_EXPONENT: i32 = 18;
        type Next = Info1;
    }

    enum Info1 {}
    impl TierInfo for Info1 {
        const TIER: usize = 1;
        // 24-bits are enough in fact.
        const MULTIPLY: u32 = 1701;
        const SUBTRACT: u32 = 0;
        const SHIFT: u32 = 9;
        const MIN_EXPONENT: i32 = -58;
        const MAX_EXPONENT: i32 = 58;
        type Next = Info2;
    }

    enum Info2 {}
    impl TierInfo for Info2 {
        const TIER: usize = 2;
        const MULTIPLY: u32 = 1741647;
        const SUBTRACT: u32 = 0;
        const SHIFT: u32 = 19;
        // Formula itself holds on [-4003,4003]; [-1233,1233] is to ensure
        // no overflow.
        const MIN_EXPONENT: i32 = -1233;
        const MAX_EXPONENT: i32 = 1233;
        type Next = Self;
    }

    compute::<Info0, MIN_EXPONENT, MAX_EXPONENT>(e)
}

pub(crate) const FLOOR_LOG10_POW2_MINUS_LOG10_4_OVER_3_MIN_EXPONENT: i32 = -2985;
pub(crate) const FLOOR_LOG10_POW2_MINUS_LOG10_4_OVER_3_MAX_EXPONENT: i32 = 2936;
pub(crate) const fn floor_log10_pow2_minus_log10_4_over_3<
    const MIN_EXPONENT: i32,
    const MAX_EXPONENT: i32,
>(
    e: i32,
) -> i32 {
    enum Info0 {}
    impl TierInfo for Info0 {
        const TIER: usize = 0;
        const MULTIPLY: u32 = 77;
        const SUBTRACT: u32 = 31;
        const SHIFT: u32 = 8;
        const MIN_EXPONENT: i32 = -75;
        const MAX_EXPONENT: i32 = 129;
        type Next = Info1;
    }

    enum Info1 {}
    impl TierInfo for Info1 {
        const TIER: usize = 1;
        // 24-bits are enough in fact.
        const MULTIPLY: u32 = 19728;
        const SUBTRACT: u32 = 8241;
        const SHIFT: u32 = 16;
        // Formula itself holds on [-849,315]; [-424,315] is to ensure that
        // the output is within [-127,127].
        const MIN_EXPONENT: i32 = -424;
        const MAX_EXPONENT: i32 = 315;
        type Next = Info2;
    }

    enum Info2 {}
    impl TierInfo for Info2 {
        const TIER: usize = 2;
        const MULTIPLY: u32 = 631305;
        const SUBTRACT: u32 = 261663;
        const SHIFT: u32 = 21;
        const MIN_EXPONENT: i32 = -2985;
        const MAX_EXPONENT: i32 = 2936;
        type Next = Self;
    }

    compute::<Info0, MIN_EXPONENT, MAX_EXPONENT>(e)
}

pub(crate) const fn floor_log5_pow2(e: i32) -> i32 {
    enum Info0 {}
    impl TierInfo for Info0 {
        const TIER: usize = 0;
        const MULTIPLY: u32 = 225799;
        const SUBTRACT: u32 = 0;
        const SHIFT: u32 = 19;
        const MIN_EXPONENT: i32 = -1831;
        const MAX_EXPONENT: i32 = 1831;
        type Next = Self;
    }

    compute::<Info0, -1831, 1831>(e)
}

pub(crate) const fn floor_log5_pow2_minus_log5_3(e: i32) -> i32 {
    enum Info0 {}
    impl TierInfo for Info0 {
        const TIER: usize = 0;
        const MULTIPLY: u32 = 451597;
        const SUBTRACT: u32 = 715764;
        const SHIFT: u32 = 20;
        const MIN_EXPONENT: i32 = -3543;
        const MAX_EXPONENT: i32 = 2427;
        type Next = Self;
    }

    compute::<Info0, -3543, 2427>(e)
}
