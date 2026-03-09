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

//! [![github]](https://github.com/magic-akari/dragonbox)&ensp;[![crates-io]](https://crates.io/crates/dragonbox_ecma)&ensp;[![docs-rs]](https://docs.rs/dragonbox_ecma)
//!
//! [github]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-io]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-rs]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logo=docs.rs
//!
//! <br>
//!
//! This crate contains a basic port of <https://github.com/jk-jeon/dragonbox>
//! to Rust for benchmarking purposes.
//!
//! Please see the upstream repo for an explanation of the approach and
//! comparison to the Ryū algorithm.
//!
//! # Example
//!
//! ```
//! fn main() {
//!     let mut buffer = dragonbox_ecma::Buffer::new();
//!     let printed = buffer.format(1.234);
//!     assert_eq!(printed, "1.234");
//! }
//! ```
//!
//! ## Performance (lower is better)
//!
//! ![performance](https://raw.githubusercontent.com/dtolnay/dragonbox/master/performance.png)

#![no_std]
#![doc(html_root_url = "https://docs.rs/dragonbox/0.1.12")]
#![allow(unsafe_op_in_unsafe_fn, unused_parens)]
#![cfg_attr(dragonbox_dead_code_workaround, allow(dead_code))]
#![allow(
    clippy::assertions_on_constants,
    clippy::bool_to_int_with_if,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::comparison_chain,
    clippy::doc_markdown,
    clippy::eq_op,
    clippy::expl_impl_clone_on_copy,
    clippy::if_not_else,
    clippy::items_after_statements,
    clippy::must_use_candidate,
    clippy::needless_bitwise_bool,
    clippy::needless_doctest_main,
    clippy::never_loop,
    clippy::similar_names,
    clippy::too_many_lines,
    clippy::unreadable_literal,
    clippy::verbose_bit_mask
)]

mod buffer;
mod cache;
mod div;
mod log;
mod policy;
#[cfg_attr(feature = "ecma", path = "to_chars_ecma.rs")]
mod to_chars;
mod wuint;

use crate::buffer::Sealed;
use crate::cache::EntryTypeExt as _;
use core::mem::MaybeUninit;

/// Buffer correctly sized to hold the text representation of any floating point
/// value.
///
/// ## Example
///
/// ```
/// let mut buffer = dragonbox_ecma::Buffer::new();
/// let printed = buffer.format_finite(1.234);
/// assert_eq!(printed, "1.234");
/// ```
pub struct Buffer {
    bytes: [MaybeUninit<u8>; to_chars::MAX_OUTPUT_STRING_LENGTH],
}

/// A floating point number that can be written into a
/// [`dragonbox_ecma::Buffer`][Buffer].
///
/// This trait is sealed and cannot be implemented for types outside of the
/// `dragonbox` crate.
pub trait Float: Sealed {}

// IEEE754-binary64
const TOTAL_BITS: usize = 64;
const SIGNIFICAND_BITS: usize = 52;
const EXPONENT_BITS: usize = 11;
const MIN_EXPONENT: i32 = -1022;
const MAX_EXPONENT: i32 = 1023;
const EXPONENT_BIAS: i32 = -1023;
#[allow(dead_code)]
const DECIMAL_SIGNIFICAND_DIGITS: usize = 17;
#[allow(dead_code)]
const DECIMAL_EXPONENT_DIGITS: usize = 3;

// Defines an unsigned integer type that is large enough
// to carry a variable of type f64.
// Most of the operations will be done on this integer type.
type CarrierUint = u64;

// Number of bits in the above unsigned integer type.
const CARRIER_BITS: usize = 64;

// Specifies the signed integer type to hold the exponent of Float.
type ExponentInt = i32;

// Extract exponent bits from a bit pattern. The result must be aligned to the
// LSB so that there is no additional zero paddings on the right. This function
// does not do bias adjustment.
const fn extract_exponent_bits(u: CarrierUint) -> ExponentInt {
    (u >> SIGNIFICAND_BITS) as ExponentInt & (((1 as ExponentInt) << EXPONENT_BITS) - 1)
}

// Remove the exponent bits and extract significand bits together with the sign
// bit.
const fn remove_exponent_bits(u: CarrierUint) -> CarrierUint {
    u & !(((1 << EXPONENT_BITS) - 1) << SIGNIFICAND_BITS)
}

// Shift the obtained signed significand bits to the left by 1 to remove the
// sign bit.
const fn remove_sign_bit_and_shift(u: CarrierUint) -> CarrierUint {
    (u << 1) & ((((1 << (TOTAL_BITS - 1)) - 1) << 1) | 1)
}

const fn is_nonzero(u: CarrierUint) -> bool {
    (u & ((1 << (SIGNIFICAND_BITS + EXPONENT_BITS)) - 1)) != 0
}

const fn is_positive(u: CarrierUint) -> bool {
    u < (1 << (SIGNIFICAND_BITS + EXPONENT_BITS))
}

const fn is_negative(u: CarrierUint) -> bool {
    !is_positive(u)
}

const fn has_even_significand_bits(s: CarrierUint) -> bool {
    s % 2 == 0
}

const fn compute_power32<const K: u32>(a: u32) -> u32 {
    let mut p = 1;
    let mut i = 0;
    while i < K {
        p *= a;
        i += 1;
    }
    p
}

const fn compute_power64<const K: u32>(a: u64) -> u64 {
    let mut p = 1;
    let mut i = 0;
    while i < K {
        p *= a;
        i += 1;
    }
    p
}

const fn count_factors<const A: usize>(mut n: usize) -> u32 {
    const {
        assert!(A > 1);
    }
    let mut c = 0;
    while n % A == 0 {
        n /= A;
        c += 1;
    }
    c
}

struct Decimal {
    significand: u64,
    exponent: i32,
}

const KAPPA: u32 = (log::floor_log10_pow2::<
    { log::FLOOR_LOG10_POW2_MIN_EXPONENT },
    { log::FLOOR_LOG10_POW2_MAX_EXPONENT },
>(CARRIER_BITS as i32 - SIGNIFICAND_BITS as i32 - 2)
    - 1) as u32;
const _: () = assert!(
    CARRIER_BITS as i32
        >= SIGNIFICAND_BITS as i32
            + 2
            + log::floor_log2_pow10::<
                { log::FLOOR_LOG2_POW10_MIN_EXPONENT },
                { log::FLOOR_LOG2_POW10_MAX_EXPONENT },
            >(KAPPA as i32 + 1),
);

const fn min(x: i32, y: i32) -> i32 {
    if x < y {
        x
    } else {
        y
    }
}

const fn max(x: i32, y: i32) -> i32 {
    if x > y {
        x
    } else {
        y
    }
}

const MIN_K: i32 = min(
    -log::floor_log10_pow2_minus_log10_4_over_3::<
        { log::FLOOR_LOG10_POW2_MINUS_LOG10_4_OVER_3_MIN_EXPONENT },
        { log::FLOOR_LOG10_POW2_MINUS_LOG10_4_OVER_3_MAX_EXPONENT },
    >(MAX_EXPONENT - SIGNIFICAND_BITS as i32),
    -log::floor_log10_pow2::<
        { log::FLOOR_LOG10_POW2_MIN_EXPONENT },
        { log::FLOOR_LOG10_POW2_MAX_EXPONENT },
    >(MAX_EXPONENT - SIGNIFICAND_BITS as i32)
        + KAPPA as i32,
);
const _: () = assert!(MIN_K >= cache::MIN_K);

// We do invoke shorter_interval_case for exponent == min_exponent case, so we
// should not add 1 here.
const MAX_K: i32 = max(
    -log::floor_log10_pow2_minus_log10_4_over_3::<
        { log::FLOOR_LOG10_POW2_MINUS_LOG10_4_OVER_3_MIN_EXPONENT },
        { log::FLOOR_LOG10_POW2_MINUS_LOG10_4_OVER_3_MAX_EXPONENT },
    >(MIN_EXPONENT - SIGNIFICAND_BITS as i32 /* + 1*/),
    -log::floor_log10_pow2::<
        { log::FLOOR_LOG10_POW2_MIN_EXPONENT },
        { log::FLOOR_LOG10_POW2_MAX_EXPONENT },
    >(MIN_EXPONENT - SIGNIFICAND_BITS as i32)
        + KAPPA as i32,
);
const _: () = assert!(MAX_K <= cache::MAX_K);

struct ComputeMulResult {
    integer_part: CarrierUint,
    is_integer: bool,
}

fn compute_mul(u: CarrierUint, cache: &cache::EntryType) -> ComputeMulResult {
    let r = wuint::umul192_upper128(u, *cache);
    ComputeMulResult {
        integer_part: r.high(),
        is_integer: r.low() == 0,
    }
}

fn compute_delta(cache: &cache::EntryType, beta: i32) -> u32 {
    (cache.high() >> ((CARRIER_BITS - 1) as i32 - beta)) as u32
}

struct ComputeMulParityResult {
    parity: bool,
    is_integer: bool,
}

fn compute_mul_parity(
    two_f: CarrierUint,
    cache: &cache::EntryType,
    beta: i32,
) -> ComputeMulParityResult {
    debug_assert!(beta >= 1);
    debug_assert!(beta < 64);

    let r = wuint::umul192_lower128(two_f, *cache);
    ComputeMulParityResult {
        parity: ((r.high() >> (64 - beta)) & 1) != 0,
        is_integer: ((r.high() << beta) | (r.low() >> (64 - beta))) == 0,
    }
}

fn compute_left_endpoint_for_shorter_interval_case(
    cache: &cache::EntryType,
    beta: i32,
) -> CarrierUint {
    (cache.high() - (cache.high() >> (SIGNIFICAND_BITS + 2)))
        >> ((CARRIER_BITS - SIGNIFICAND_BITS - 1) as i32 - beta)
}

fn compute_right_endpoint_for_shorter_interval_case(
    cache: &cache::EntryType,
    beta: i32,
) -> CarrierUint {
    (cache.high() + (cache.high() >> (SIGNIFICAND_BITS + 1)))
        >> ((CARRIER_BITS - SIGNIFICAND_BITS - 1) as i32 - beta)
}

fn compute_round_up_for_shorter_interval_case(cache: &cache::EntryType, beta: i32) -> CarrierUint {
    (cache.high() >> ((CARRIER_BITS - SIGNIFICAND_BITS - 2) as i32 - beta)).div_ceil(2)
}

const fn floor_log2(mut n: u64) -> i32 {
    let mut count = -1;
    while n != 0 {
        count += 1;
        n >>= 1;
    }
    count
}

fn is_left_endpoint_integer_shorter_interval(exponent: i32) -> bool {
    const CASE_SHORTER_INTERVAL_LEFT_ENDPOINT_LOWER_THRESHOLD: i32 = 2;
    const CASE_SHORTER_INTERVAL_LEFT_ENDPOINT_UPPER_THRESHOLD: i32 = 2 + floor_log2(
        compute_power64::<
            {
                count_factors::<5>((((1 as CarrierUint) << (SIGNIFICAND_BITS + 2)) - 1) as usize)
                    + 1
            },
        >(10)
            / 3,
    );
    (CASE_SHORTER_INTERVAL_LEFT_ENDPOINT_LOWER_THRESHOLD
        ..=CASE_SHORTER_INTERVAL_LEFT_ENDPOINT_UPPER_THRESHOLD)
        .contains(&exponent)
}

// The main algorithm assumes the input is a normal/subnormal finite number.
fn compute_nearest(signed_significand_bits: CarrierUint, exponent_bits: ExponentInt) -> Decimal {
    let mut two_fc = remove_sign_bit_and_shift(signed_significand_bits);
    let mut binary_exponent = exponent_bits;

    // Is the input a normal number?
    if binary_exponent != 0 {
        binary_exponent += EXPONENT_BIAS - SIGNIFICAND_BITS as i32;

        // Shorter interval case; proceed like Schubfach. One might think this
        // condition is wrong, since when exponent_bits == 1 and two_fc == 0,
        // the interval is actually regular. However, it turns out that this
        // seemingly wrong condition is actually fine, because the end result is
        // anyway the same.
        //
        // [binary32]
        // (fc-1/2) * 2^e = 1.175'494'28... * 10^-38
        // (fc-1/4) * 2^e = 1.175'494'31... * 10^-38
        //    fc    * 2^e = 1.175'494'35... * 10^-38
        // (fc+1/2) * 2^e = 1.175'494'42... * 10^-38
        //
        // Hence, shorter_interval_case will return 1.175'494'4 * 10^-38.
        // 1.175'494'3 * 10^-38 is also a correct shortest representation that
        // will be rejected if we assume shorter interval, but 1.175'494'4 *
        // 10^-38 is closer to the true value so it doesn't matter.
        //
        // [binary64]
        // (fc-1/2) * 2^e = 2.225'073'858'507'201'13... * 10^-308
        // (fc-1/4) * 2^e = 2.225'073'858'507'201'25... * 10^-308
        //    fc    * 2^e = 2.225'073'858'507'201'38... * 10^-308
        // (fc+1/2) * 2^e = 2.225'073'858'507'201'63... * 10^-308
        //
        // Hence, shorter_interval_case will return 2.225'073'858'507'201'4 * 10^-308.
        // This is indeed of the shortest length, and it is the unique one
        // closest to the true value among valid representations of the same
        // length.

        // Shorter interval case.
        if two_fc == 0 {
            // Compute k and beta.
            let minus_k = log::floor_log10_pow2_minus_log10_4_over_3::<
                { MIN_EXPONENT - SIGNIFICAND_BITS as i32 },
                { MAX_EXPONENT - SIGNIFICAND_BITS as i32 },
            >(binary_exponent);
            let beta = binary_exponent + log::floor_log2_pow10::<MIN_K, MAX_K>(-minus_k);

            // Compute xi and zi.
            let cache = unsafe { cache::get(-minus_k) };

            let mut xi = compute_left_endpoint_for_shorter_interval_case(&cache, beta);
            let zi = compute_right_endpoint_for_shorter_interval_case(&cache, beta);

            // If the left endpoint is not an integer, increase it.
            if !is_left_endpoint_integer_shorter_interval(binary_exponent) {
                xi += 1;
            }

            // Try bigger divisor.
            // zi is at most floor((f_c + 1/2) * 2^e * 10^k0).
            // Substituting f_c = 2^p and k0 = -floor(log10(3 * 2^(e-2))), we get
            // zi <= floor((2^(p+1) + 1) * 20/3) <= ceil((2^(p+1) + 1)/3) * 20.
            // This computation does not overflow for any of the formats I care about.
            let mut decimal_significand = div::divide_by_pow10::<
                1,
                { ((((1 << (SIGNIFICAND_BITS + 1)) + 1) / 3) + 1) * 20 },
            >(zi);

            // If succeed, remove trailing zeros if necessary and return.
            if decimal_significand * 10 >= xi {
                return Decimal {
                    significand: decimal_significand,
                    exponent: minus_k + 1,
                };
            }

            // Otherwise, compute the round-up of y.
            decimal_significand = compute_round_up_for_shorter_interval_case(&cache, beta);

            // When tie occurs, choose one of them according to the rule.
            const SHORTER_INTERVAL_TIE_LOWER_THRESHOLD: i32 =
                -log::floor_log5_pow2_minus_log5_3(SIGNIFICAND_BITS as i32 + 4)
                    - 2
                    - SIGNIFICAND_BITS as i32;
            const SHORTER_INTERVAL_TIE_UPPER_THRESHOLD: i32 =
                -log::floor_log5_pow2(SIGNIFICAND_BITS as i32 + 2) - 2 - SIGNIFICAND_BITS as i32;
            if policy::prefer_round_down(decimal_significand)
                && binary_exponent >= SHORTER_INTERVAL_TIE_LOWER_THRESHOLD
                && binary_exponent <= SHORTER_INTERVAL_TIE_UPPER_THRESHOLD
            {
                decimal_significand -= 1;
            } else if decimal_significand < xi {
                decimal_significand += 1;
            }
            return Decimal {
                significand: decimal_significand,
                exponent: minus_k,
            };
        }

        two_fc |= 1 << (SIGNIFICAND_BITS + 1);
    }
    // Is the input a subnormal number?
    else {
        binary_exponent = MIN_EXPONENT - SIGNIFICAND_BITS as i32;
    }

    //////////////////////////////////////////////////////////////////////
    // Step 1: Schubfach multiplier calculation.
    //////////////////////////////////////////////////////////////////////

    let has_even_significand_bits = has_even_significand_bits(signed_significand_bits);

    // Compute k and beta.
    let minus_k = log::floor_log10_pow2::<
        { MIN_EXPONENT - SIGNIFICAND_BITS as i32 },
        { MAX_EXPONENT - SIGNIFICAND_BITS as i32 },
    >(binary_exponent)
        - KAPPA as i32;
    let cache = unsafe { cache::get(-minus_k) };
    let beta = binary_exponent + log::floor_log2_pow10::<MIN_K, MAX_K>(-minus_k);

    // Compute zi and deltai.
    // 10^kappa <= deltai < 10^(kappa + 1)
    let deltai = compute_delta(&cache, beta);
    // For the case of binary32, the result of integer check is not correct for
    // 29711844 * 2^-82
    // = 6.1442653300000000008655037797566933477355632930994033813476... * 10^-18
    // and 29711844 * 2^-81
    // = 1.2288530660000000001731007559513386695471126586198806762695... * 10^-17,
    // and they are the unique counterexamples. However, since 29711844 is even,
    // this does not cause any problem for the endpoints calculations; it can
    // only cause a problem when we need to perform integer check for the
    // center. Fortunately, with these inputs, that branch is never executed, so
    // we are fine.
    let z_result = compute_mul((two_fc | 1) << beta, &cache);

    //////////////////////////////////////////////////////////////////////
    // Step 2: Try larger divisor; remove trailing zeros if necessary.
    //////////////////////////////////////////////////////////////////////

    const BIG_DIVISOR: u32 = compute_power32::<{ KAPPA + 1 }>(10);
    const SMALL_DIVISOR: u32 = compute_power32::<KAPPA>(10);

    // Using an upper bound on zi, we might be able to optimize the division
    // better than the compiler; we are computing zi / big_divisor here.
    let mut decimal_significand = div::divide_by_pow10::<
        { KAPPA + 1 },
        { (1 << (SIGNIFICAND_BITS + 1)) * BIG_DIVISOR as u64 - 1 },
    >(z_result.integer_part);
    let mut r = (z_result.integer_part - u64::from(BIG_DIVISOR) * decimal_significand) as u32;

    loop {
        if r < deltai {
            // Exclude the right endpoint if necessary.
            if (r | u32::from(!z_result.is_integer) | u32::from(has_even_significand_bits)) == 0 {
                decimal_significand -= 1;
                r = BIG_DIVISOR;
                break;
            }
        } else if r > deltai {
            break;
        } else {
            // r == deltai; compare fractional parts.
            let x_result = compute_mul_parity(two_fc - 1, &cache, beta);

            if !(x_result.parity | (x_result.is_integer & has_even_significand_bits)) {
                break;
            }
        }

        return Decimal {
            significand: decimal_significand,
            exponent: minus_k + KAPPA as i32 + 1,
        };
    }

    //////////////////////////////////////////////////////////////////////
    // Step 3: Find the significand with the smaller divisor.
    //////////////////////////////////////////////////////////////////////

    decimal_significand *= 10;

    // delta is equal to 10^(kappa + elog10(2) - floor(elog10(2))), so dist
    // cannot be larger than r.
    let mut dist = r - (deltai / 2) + (SMALL_DIVISOR / 2);
    let approx_y_parity = ((dist ^ (SMALL_DIVISOR / 2)) & 1) != 0;

    // Is dist divisible by 10^kappa?
    let divisible_by_small_divisor = div::check_divisibility_and_divide_by_pow10(&mut dist);

    // Add dist / 10^kappa to the significand.
    decimal_significand += CarrierUint::from(dist);

    if divisible_by_small_divisor {
        // Check z^(f) >= epsilon^(f).
        // We have either yi == zi - epsiloni or yi == (zi - epsiloni) - 1,
        // where yi == zi - epsiloni if and only if z^(f) >= epsilon^(f).
        // Since there are only 2 possibilities, we only need to care about the parity.
        // Also, zi and r should have the same parity since the divisor
        // is an even number.
        let y_result = compute_mul_parity(two_fc, &cache, beta);
        if y_result.parity != approx_y_parity {
            decimal_significand -= 1;
        } else {
            // If z^(f) >= epsilon^(f), we might have a tie
            // when z^(f) == epsilon^(f), or equivalently, when y is an integer.
            // For tie-to-up case, we can just choose the upper one.
            if policy::prefer_round_down(decimal_significand) & y_result.is_integer {
                decimal_significand -= 1;
            }
        }
    }

    Decimal {
        significand: decimal_significand,
        exponent: minus_k + KAPPA as i32,
    }
}

#[cfg(not(feature = "ecma"))]
fn to_decimal(x: f64) -> Decimal {
    let br = x.to_bits();
    let exponent_bits = extract_exponent_bits(br);
    let s = remove_exponent_bits(br);

    compute_nearest(s, exponent_bits)
}

#[cfg(feature = "ecma")]
fn to_decimal(x: f64) -> Decimal {
    let br = x.to_bits();
    let exponent_bits = extract_exponent_bits(br);
    let s = remove_exponent_bits(br);

    let mut decimal = compute_nearest(s, exponent_bits);
    policy::remove_trailing_zeros(&mut decimal.significand, &mut decimal.exponent);
    decimal
}
