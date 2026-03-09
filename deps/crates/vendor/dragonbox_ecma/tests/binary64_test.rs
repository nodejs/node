// Translated from C to Rust. The original C code can be found at
// https://github.com/ulfjack/ryu and carries the following license:
//
// Copyright 2018 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.

#![allow(
    clippy::approx_constant,
    clippy::cast_lossless,
    clippy::excessive_precision,
    clippy::float_cmp,
    clippy::int_plus_one,
    clippy::non_ascii_literal,
    clippy::unreadable_literal,
    clippy::unseparated_literal_suffix
)]

#[macro_use]
mod macros;

use dragonbox_ecma as dragonbox;
use std::f64;

fn to_chars(f: f64) -> String {
    dragonbox::Buffer::new().format(f).to_owned()
}

fn ieee_parts_to_double(sign: bool, ieee_exponent: u32, ieee_mantissa: u64) -> f64 {
    assert!(ieee_exponent <= 2047);
    assert!(ieee_mantissa <= (1u64 << 53) - 1);
    f64::from_bits(((sign as u64) << 63) | ((ieee_exponent as u64) << 52) | ieee_mantissa)
}

#[test]
fn test_dragonbox() {
    check!(3E-1);
    check!(1.234E15);
    check!(2.71828E0);
    check!(1.1E128);
    check!(1.1E-64);
    check!(2.718281828459045E0);
    check!(5E-324);
    check!(1.7976931348623157E308);
}

#[test]
fn test_random() {
    let n = if cfg!(miri) { 100 } else { 1000000 };
    let mut buffer = dragonbox::Buffer::new();
    for _ in 0..n {
        let f: f64 = rand::random();
        assert_eq!(f, buffer.format_finite(f).parse().unwrap());
    }
}

#[test]
fn test_basic() {
    check!(0E0);
    check!(-0E0);
    check!(1E0);
    check!(-1E0);
    assert_eq!(to_chars(f64::NAN.copysign(1.0)), "NaN");
    assert_eq!(to_chars(f64::NAN.copysign(-1.0)), "NaN");
    assert_eq!(to_chars(f64::INFINITY), "inf");
    assert_eq!(to_chars(f64::NEG_INFINITY), "-inf");
}

#[test]
fn test_switch_to_subnormal() {
    check!(2.2250738585072014E-308);
}

#[test]
fn test_min_and_max() {
    assert_eq!(f64::from_bits(0x7fefffffffffffff), 1.7976931348623157E308);
    check!(1.7976931348623157E308);
    assert_eq!(f64::from_bits(1), 5E-324);
    check!(5E-324);
}

#[test]
fn test_lots_of_trailing_zeros() {
    check!(2.9802322387695312E-8);
}

#[test]
fn test_regression() {
    check!(-2.109808898695963E16);
    check!(4.940656E-318);
    check!(1.18575755E-316);
    check!(2.989102097996E-312);
    check!(9.0608011534336E15);
    check!(4.708356024711512E18);
    check!(9.409340012568248E18);
    check!(1.2345678E0);
}

#[test]
fn test_looks_like_pow5() {
    // These numbers have a mantissa that is a multiple of the largest power of
    // 5 that fits, and an exponent that causes the computation for q to result
    // in 22, which is a corner case for Ryū.
    assert_eq!(f64::from_bits(0x4830F0CF064DD592), 5.764607523034235E39);
    check!(5.764607523034235E39);
    assert_eq!(f64::from_bits(0x4840F0CF064DD592), 1.152921504606847E40);
    check!(1.152921504606847E40);
    assert_eq!(f64::from_bits(0x4850F0CF064DD592), 2.305843009213694E40);
    check!(2.305843009213694E40);
}

#[test]
fn test_output_length() {
    check!(1E0); // already tested in Basic
    check!(1.2E0);
    check!(1.23E0);
    check!(1.234E0);
    check!(1.2345E0);
    check!(1.23456E0);
    check!(1.234567E0);
    check!(1.2345678E0); // already tested in Regression
    check!(1.23456789E0);
    check!(1.234567895E0); // 1.234567890 would be trimmed
    check!(1.2345678901E0);
    check!(1.23456789012E0);
    check!(1.234567890123E0);
    check!(1.2345678901234E0);
    check!(1.23456789012345E0);
    check!(1.234567890123456E0);
    check!(1.2345678901234567E0);

    // Test 32-bit chunking
    check!(4.294967294E0); // 2^32 - 2
    check!(4.294967295E0); // 2^32 - 1
    check!(4.294967296E0); // 2^32
    check!(4.294967297E0); // 2^32 + 1
    check!(4.294967298E0); // 2^32 + 2
}

// Test min, max shift values in shiftright128
#[test]
fn test_min_max_shift() {
    let max_mantissa = (1u64 << 53) - 1;

    // 32-bit opt-size=0:  49 <= dist <= 50
    // 32-bit opt-size=1:  30 <= dist <= 50
    // 64-bit opt-size=0:  50 <= dist <= 50
    // 64-bit opt-size=1:  30 <= dist <= 50
    assert_eq!(1.7800590868057611E-307, ieee_parts_to_double(false, 4, 0));
    check!(1.7800590868057611E-307);
    // 32-bit opt-size=0:  49 <= dist <= 49
    // 32-bit opt-size=1:  28 <= dist <= 49
    // 64-bit opt-size=0:  50 <= dist <= 50
    // 64-bit opt-size=1:  28 <= dist <= 50
    assert_eq!(
        2.8480945388892175E-306,
        ieee_parts_to_double(false, 6, max_mantissa)
    );
    check!(2.8480945388892175E-306);
    // 32-bit opt-size=0:  52 <= dist <= 53
    // 32-bit opt-size=1:   2 <= dist <= 53
    // 64-bit opt-size=0:  53 <= dist <= 53
    // 64-bit opt-size=1:   2 <= dist <= 53
    assert_eq!(2.446494580089078E-296, ieee_parts_to_double(false, 41, 0));
    check!(2.446494580089078E-296);
    // 32-bit opt-size=0:  52 <= dist <= 52
    // 32-bit opt-size=1:   2 <= dist <= 52
    // 64-bit opt-size=0:  53 <= dist <= 53
    // 64-bit opt-size=1:   2 <= dist <= 53
    assert_eq!(
        4.8929891601781557E-296,
        ieee_parts_to_double(false, 40, max_mantissa)
    );
    check!(4.8929891601781557E-296);

    // 32-bit opt-size=0:  57 <= dist <= 58
    // 32-bit opt-size=1:  57 <= dist <= 58
    // 64-bit opt-size=0:  58 <= dist <= 58
    // 64-bit opt-size=1:  58 <= dist <= 58
    assert_eq!(1.8014398509481984E16, ieee_parts_to_double(false, 1077, 0));
    check!(1.8014398509481984E16);
    // 32-bit opt-size=0:  57 <= dist <= 57
    // 32-bit opt-size=1:  57 <= dist <= 57
    // 64-bit opt-size=0:  58 <= dist <= 58
    // 64-bit opt-size=1:  58 <= dist <= 58
    assert_eq!(
        3.6028797018963964E16,
        ieee_parts_to_double(false, 1076, max_mantissa)
    );
    check!(3.6028797018963964E16);
    // 32-bit opt-size=0:  51 <= dist <= 52
    // 32-bit opt-size=1:  51 <= dist <= 59
    // 64-bit opt-size=0:  52 <= dist <= 52
    // 64-bit opt-size=1:  52 <= dist <= 59
    assert_eq!(2.900835519859558E-216, ieee_parts_to_double(false, 307, 0));
    check!(2.900835519859558E-216);
    // 32-bit opt-size=0:  51 <= dist <= 51
    // 32-bit opt-size=1:  51 <= dist <= 59
    // 64-bit opt-size=0:  52 <= dist <= 52
    // 64-bit opt-size=1:  52 <= dist <= 59
    assert_eq!(
        5.801671039719115E-216,
        ieee_parts_to_double(false, 306, max_mantissa)
    );
    check!(5.801671039719115E-216);

    // https://github.com/ulfjack/ryu/commit/19e44d16d80236f5de25800f56d82606d1be00b9#commitcomment-30146483
    // 32-bit opt-size=0:  49 <= dist <= 49
    // 32-bit opt-size=1:  44 <= dist <= 49
    // 64-bit opt-size=0:  50 <= dist <= 50
    // 64-bit opt-size=1:  44 <= dist <= 50
    assert_eq!(
        3.196104012172126E-27,
        ieee_parts_to_double(false, 934, 0x000FA7161A4D6E0C)
    );
    check!(3.196104012172126E-27);
}

#[test]
fn test_small_integers() {
    check!(9.007199254740991E15); // 2^53-1
    check!(9.007199254740992E15); // 2^53

    check!(1E0);
    check!(1.2E1);
    check!(1.23E2);
    check!(1.234E3);
    check!(1.2345E4);
    check!(1.23456E5);
    check!(1.234567E6);
    check!(1.2345678E7);
    check!(1.23456789E8);
    check!(1.23456789E9);
    check!(1.234567895E10);
    check!(1.2345678901E11);
    check!(1.23456789012E12);
    check!(1.234567890123E13);
    check!(1.2345678901234E14);
    check!(1.23456789012345E15);
    check!(1.234567890123456E16);

    // 10^i
    check!(1E0);
    check!(1E1);
    check!(1E2);
    check!(1E3);
    check!(1E4);
    check!(1E5);
    check!(1E6);
    check!(1E7);
    check!(1E8);
    check!(1E9);
    check!(1E10);
    check!(1E11);
    check!(1E12);
    check!(1E13);
    check!(1E14);
    check!(1E15);

    // 10^15 + 10^i
    check!(1.000000000000001E15);
    check!(1.00000000000001E15);
    check!(1.0000000000001E15);
    check!(1.000000000001E15);
    check!(1.00000000001E15);
    check!(1.0000000001E15);
    check!(1.000000001E15);
    check!(1.00000001E15);
    check!(1.0000001E15);
    check!(1.000001E15);
    check!(1.00001E15);
    check!(1.0001E15);
    check!(1.001E15);
    check!(1.01E15);
    check!(1.1E15);

    // Largest power of 2 <= 10^(i+1)
    check!(8E0);
    check!(6.4E1);
    check!(5.12E2);
    check!(8.192E3);
    check!(6.5536E4);
    check!(5.24288E5);
    check!(8.388608E6);
    check!(6.7108864E7);
    check!(5.36870912E8);
    check!(8.589934592E9);
    check!(6.8719476736E10);
    check!(5.49755813888E11);
    check!(8.796093022208E12);
    check!(7.0368744177664E13);
    check!(5.62949953421312E14);
    check!(9.007199254740992E15);

    // 1000 * (Largest power of 2 <= 10^(i+1))
    check!(8E3);
    check!(6.4E4);
    check!(5.12E5);
    check!(8.192E6);
    check!(6.5536E7);
    check!(5.24288E8);
    check!(8.388608E9);
    check!(6.7108864E10);
    check!(5.36870912E11);
    check!(8.589934592E12);
    check!(6.8719476736E13);
    check!(5.49755813888E14);
    check!(8.796093022208E15);
}

#[test]
fn test_issue_3() {
    assert_eq!(to_chars(1.0902420340782359E+57), "1.0902420340782359E57");
    assert_eq!(to_chars(5.3461812015486243E+26), "5.346181201548624E26");
    assert_eq!(to_chars(2.8674588045599296E+66), "2.8674588045599296E66");
    assert_eq!(to_chars(1.2182856909681335E+77), "1.2182856909681335E77");
    assert_eq!(to_chars(2.0596292913323055E+69), "2.0596292913323055E69");
    assert_eq!(to_chars(1.3331169462028886E+60), "1.3331169462028886E60");
}

#[test]
fn test_critical_boundary_case() {
    // See https://github.com/dtolnay/dragonbox/pull/7#discussion_r2169239437
    let test_value = f64::from_bits(0x3FF0000000000001); // 1 + 2^-52
    let result = to_chars(test_value);
    let expected = "1.0000000000000002E0";
    assert_eq!(
        result, expected,
        "Critical boundary case failed. Input: {test_value}, Expected: {expected}, Got: {result}",
    );

    // Also verify round-trip conversion
    assert_eq!(
        test_value,
        result.parse().unwrap(),
        "Round-trip conversion failed for critical boundary case",
    );

    // Additional test case: A power of 2 boundary that might trigger the condition
    // This is 2^53 + 1, which should trigger our edge case handling
    let test_value = f64::from_bits(0x4350000000000001); // 2^53 + 1
    let result = to_chars(test_value);

    // With the correct logic, this should properly round-trip
    assert_eq!(
        test_value,
        result.parse().unwrap(),
        "Failed for power of 2 boundary case: {test_value}, result: {result}",
    );
}
