// This file is part of ICU4X.
//
// The contents of this file implement algorithms from Calendrical Calculations
// by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018),
// which have been released as Lisp code at <https://github.com/EdReingold/calendar-code2/>
// under the Apache-2.0 license. Accordingly, this file is released under
// the Apache License, Version 2.0 which can be found at the calendrical_calculations
// package root or at http://www.apache.org/licenses/LICENSE-2.0.

use crate::astronomy::Location;
use crate::rata_die::{Moment, RataDie};
#[allow(unused_imports)]
use core_maths::*;

pub(crate) trait IntegerRoundings {
    fn div_ceil(self, rhs: Self) -> Self;
}

impl IntegerRoundings for i64 {
    // copied from std
    fn div_ceil(self, rhs: Self) -> Self {
        let d = self / rhs;
        let r = self % rhs;
        if (r > 0 && rhs > 0) || (r < 0 && rhs < 0) {
            d + 1
        } else {
            d
        }
    }
}

#[test]
fn test_div_ceil() {
    assert_eq!(IntegerRoundings::div_ceil(i64::MIN, 1), i64::MIN);
    assert_eq!(
        IntegerRoundings::div_ceil(i64::MIN, 2),
        -4611686018427387904
    );
    assert_eq!(
        IntegerRoundings::div_ceil(i64::MIN, 3),
        -3074457345618258602
    );

    assert_eq!(IntegerRoundings::div_ceil(-10, 1), -10);
    assert_eq!(IntegerRoundings::div_ceil(-10, 2), -5);
    assert_eq!(IntegerRoundings::div_ceil(-10, 3), -3);

    assert_eq!(IntegerRoundings::div_ceil(-9, 1), -9);
    assert_eq!(IntegerRoundings::div_ceil(-9, 2), -4);
    assert_eq!(IntegerRoundings::div_ceil(-9, 3), -3);

    assert_eq!(IntegerRoundings::div_ceil(-8, 1), -8);
    assert_eq!(IntegerRoundings::div_ceil(-8, 2), -4);
    assert_eq!(IntegerRoundings::div_ceil(-8, 3), -2);

    assert_eq!(IntegerRoundings::div_ceil(-2, 1), -2);
    assert_eq!(IntegerRoundings::div_ceil(-2, 2), -1);
    assert_eq!(IntegerRoundings::div_ceil(-2, 3), 0);

    assert_eq!(IntegerRoundings::div_ceil(-1, 1), -1);
    assert_eq!(IntegerRoundings::div_ceil(-1, 2), 0);
    assert_eq!(IntegerRoundings::div_ceil(-1, 3), 0);

    assert_eq!(IntegerRoundings::div_ceil(0, 1), 0);
    assert_eq!(IntegerRoundings::div_ceil(0, 2), 0);
    assert_eq!(IntegerRoundings::div_ceil(0, 3), 0);

    assert_eq!(IntegerRoundings::div_ceil(1, 1), 1);
    assert_eq!(IntegerRoundings::div_ceil(1, 2), 1);
    assert_eq!(IntegerRoundings::div_ceil(1, 3), 1);

    assert_eq!(IntegerRoundings::div_ceil(2, 1), 2);
    assert_eq!(IntegerRoundings::div_ceil(2, 2), 1);
    assert_eq!(IntegerRoundings::div_ceil(2, 3), 1);

    assert_eq!(IntegerRoundings::div_ceil(8, 1), 8);
    assert_eq!(IntegerRoundings::div_ceil(8, 2), 4);
    assert_eq!(IntegerRoundings::div_ceil(8, 3), 3);

    assert_eq!(IntegerRoundings::div_ceil(9, 1), 9);
    assert_eq!(IntegerRoundings::div_ceil(9, 2), 5);
    assert_eq!(IntegerRoundings::div_ceil(9, 3), 3);

    assert_eq!(IntegerRoundings::div_ceil(10, 1), 10);
    assert_eq!(IntegerRoundings::div_ceil(10, 2), 5);
    assert_eq!(IntegerRoundings::div_ceil(10, 3), 4);

    assert_eq!(IntegerRoundings::div_ceil(i64::MAX, 1), 9223372036854775807);
    assert_eq!(IntegerRoundings::div_ceil(i64::MAX, 2), 4611686018427387904);
    assert_eq!(IntegerRoundings::div_ceil(i64::MAX, 3), 3074457345618258603);

    for n in -100..100 {
        for d in 1..5 {
            let x1 = IntegerRoundings::div_ceil(n, d);
            let x2 = (n as f64 / d as f64).ceil();
            assert_eq!(x1, x2 as i64);
        }
    }
}

// Highest power is *last*
pub(crate) fn poly(x: f64, coeffs: &[f64]) -> f64 {
    coeffs.iter().rev().fold(0.0, |a, c| a * x + c)
}

// A generic function that finds a value within an interval
// where a certain condition is satisfied.
pub(crate) fn binary_search(
    mut l: f64,
    mut h: f64,
    test: impl Fn(f64) -> bool,
    epsilon: f64,
) -> f64 {
    debug_assert!(l < h);

    loop {
        let mid = l + (h - l) / 2.0;

        // Determine which direction to go. `test` returns true if we need to go left.
        (l, h) = if test(mid) { (l, mid) } else { (mid, h) };

        // When the interval width reaches `epsilon`, return the current midpoint.
        if (h - l) < epsilon {
            return mid;
        }
    }
}

pub(crate) fn next_moment<F>(mut index: Moment, location: Location, condition: F) -> RataDie
where
    F: Fn(Moment, Location) -> bool,
{
    loop {
        if condition(index, location) {
            return index.as_rata_die();
        }
        index += 1.0;
    }
}

pub(crate) fn next<F>(mut index: RataDie, condition: F) -> RataDie
where
    F: Fn(RataDie) -> bool,
{
    loop {
        if condition(index) {
            return index;
        }
        index += 1;
    }
}

pub(crate) fn next_u8<F>(mut index: u8, condition: F) -> u8
where
    F: Fn(u8) -> bool,
{
    loop {
        if condition(index) {
            return index;
        }
        index += 1;
    }
}

// "Final" is a reserved keyword in rust, which explains the naming convention here.
pub(crate) fn final_func<F>(mut index: i32, condition: F) -> i32
where
    F: Fn(i32) -> bool,
{
    while condition(index) {
        index += 1;
    }
    index - 1
}

#[test]
fn test_binary_search() {
    struct TestCase {
        test_fn: fn(f64) -> bool,
        range: (f64, f64),
        expected: f64,
    }

    let test_cases = [
        TestCase {
            test_fn: |x: f64| x >= 4.0,
            range: (0.0, 10.0),
            expected: 4.0,
        },
        TestCase {
            test_fn: |x: f64| x * x >= 2.0,
            range: (0.0, 2.0),
            expected: 2.0f64.sqrt(),
        },
        TestCase {
            test_fn: |x: f64| x >= -4.0,
            range: (-10.0, 0.0),
            expected: -4.0,
        },
        TestCase {
            test_fn: |x: f64| x >= 0.0,
            range: (0.0, 10.0),
            expected: 0.0,
        },
        TestCase {
            test_fn: |x: f64| x > 10.0,
            range: (0.0, 10.0),
            expected: 10.0,
        },
    ];

    for case in test_cases {
        let result = binary_search(case.range.0, case.range.1, case.test_fn, 1e-4);
        assert!((result - case.expected).abs() < 0.0001);
    }
}

pub(crate) fn invert_angular<F: Fn(f64) -> f64>(f: F, y: f64, r: (f64, f64)) -> f64 {
    binary_search(r.0, r.1, |x| (f(x) - y).rem_euclid(360.0) < 180.0, 1e-5)
}

#[test]
fn test_invert_angular() {
    struct TestCase {
        f: fn(f64) -> f64,
        y: f64,
        r: (f64, f64),
        expected: f64,
    }

    fn f1(x: f64) -> f64 {
        (2.0 * x).rem_euclid(360.0)
    }

    fn f2(x: f64) -> f64 {
        (3.0 * x).rem_euclid(360.0)
    }

    fn f3(x: f64) -> f64 {
        (x).rem_euclid(360.0)
    }
    // tolerance for comparing floating points.
    let tolerance = 1e-5;

    let test_cases = [
        TestCase {
            f: f1,
            y: 4.0,
            r: (0.0, 10.0),
            expected: 4.0,
        },
        TestCase {
            f: f2,
            y: 6.0,
            r: (0.0, 20.0),
            expected: 6.0,
        },
        TestCase {
            f: f3,
            y: 400.0,
            r: (0.0, 10.0),
            expected: 10.0,
        },
        TestCase {
            f: f3,
            y: 0.0,
            r: (0.0, 10.0),
            expected: 0.0,
        },
        TestCase {
            f: f3,
            y: 10.0,
            r: (0.0, 10.0),
            expected: 10.0,
        },
    ];

    for case in test_cases {
        let x = invert_angular(case.f, case.y, case.r);
        assert!((((case.f)(x)).rem_euclid(360.0) - case.expected).abs() < tolerance);
    }
}

/// Error returned when casting from an i32
#[derive(Copy, Clone, Debug, displaydoc::Display)]
#[allow(clippy::exhaustive_enums)] // enum is specific to function and has a closed set of possible values
pub enum I32CastError {
    /// Less than i32::MIN
    BelowMin,
    /// Greater than i32::MAX
    AboveMax,
}

impl core::error::Error for I32CastError {}

impl I32CastError {
    /// Recovers the value saturated to `i32:::MIN..=i32::MAX`.
    pub const fn saturate(self) -> i32 {
        match self {
            I32CastError::BelowMin => i32::MIN,
            I32CastError::AboveMax => i32::MAX,
        }
    }
}

/// Convert an i64 to i32 and with information on which way it was out of bounds if so
#[inline]
pub const fn i64_to_i32(input: i64) -> Result<i32, I32CastError> {
    if input < i32::MIN as i64 {
        Err(I32CastError::BelowMin)
    } else if input > i32::MAX as i64 {
        Err(I32CastError::AboveMax)
    } else {
        Ok(input as i32)
    }
}

/// Convert an i64 to i32 but saturate at th ebounds
#[inline]
pub(crate) fn i64_to_saturated_i32(input: i64) -> i32 {
    i64_to_i32(input).unwrap_or_else(|i| i.saturate())
}

#[test]
fn test_i64_to_saturated_i32() {
    assert_eq!(i64_to_saturated_i32(i64::MIN), i32::MIN);
    assert_eq!(i64_to_saturated_i32(-2147483649), -2147483648);
    assert_eq!(i64_to_saturated_i32(-2147483648), -2147483648);
    assert_eq!(i64_to_saturated_i32(-2147483647), -2147483647);
    assert_eq!(i64_to_saturated_i32(-2147483646), -2147483646);
    assert_eq!(i64_to_saturated_i32(-100), -100);
    assert_eq!(i64_to_saturated_i32(0), 0);
    assert_eq!(i64_to_saturated_i32(100), 100);
    assert_eq!(i64_to_saturated_i32(2147483646), 2147483646);
    assert_eq!(i64_to_saturated_i32(2147483647), 2147483647);
    assert_eq!(i64_to_saturated_i32(2147483648), 2147483647);
    assert_eq!(i64_to_saturated_i32(2147483649), 2147483647);
    assert_eq!(i64_to_saturated_i32(i64::MAX), i32::MAX);
}

/// returns the weekday (0-6) after (strictly) the fixed date
pub(crate) const fn k_day_after(weekday: i64, fixed: RataDie) -> RataDie {
    let day_of_week = fixed.to_i64_date().rem_euclid(7);
    let beginning_of_week = fixed.to_i64_date() - day_of_week;
    let day = beginning_of_week + weekday;
    RataDie::new(day + if weekday <= day_of_week { 7 } else { 0 })
}
