// This file is part of ICU4X.
//
// The contents of this file implement algorithms from Calendrical Calculations
// by Reingold & Dershowitz, Cambridge University Press, 4th edition (2018),
// which have been released as Lisp code at <https://github.com/EdReingold/calendar-code2/>
// under the Apache-2.0 license. Accordingly, this file is released under
// the Apache License, Version 2.0 which can be found at the calendrical_calculations
// package root or at http://www.apache.org/licenses/LICENSE-2.0.

use core::fmt;
use core::ops::{Add, AddAssign, Sub, SubAssign};
#[allow(unused_imports)]
use core_maths::*;

/// The *Rata Die*, or *R.D.*: number of days since January 1, 1 CE.
///
/// **The primary definition of this type is in the [`calendrical_calculations`](https://docs.rs/calendrical_calculations) crate.**
///
/// See: <https://en.wikipedia.org/wiki/Rata_Die>
///
/// Typically, one should obtain RataDies from other calendrical code, rather than constructing them from integers.
/// The valid range for direct construction is deliberately not documented as it may change.
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct RataDie(i64);

impl RataDie {
    /// Create a `RataDie`
    ///
    /// Typically, one should obtain `RataDie`s from other calendrical code, rather than
    /// constructing them from integers. The valid range for direct construction is
    /// deliberately not documented as it may change.
    pub const fn new(fixed_date: i64) -> Self {
        let result = Self(fixed_date);
        #[cfg(debug_assertions)]
        result.check();
        result
    }

    /// Check that it is in range
    #[cfg(debug_assertions)]
    const fn check(self) {
        if self.0 > i64::MAX / 256 {
            debug_assert!(
                false,
                "RataDie is not designed to store values near to the overflow boundary"
            );
        }
        if self.0 < i64::MIN / 256 {
            debug_assert!(
                false,
                "RataDie is not designed to store values near to the overflow boundary"
            );
        }
    }

    /// A valid `RataDie` that is intended to be below all dates representable in calendars
    #[doc(hidden)] // for testing only
    pub const fn big_negative() -> Self {
        Self::new(i64::MIN / 256 / 256)
    }

    /// Convert this to an `i64` value representing the `RataDie`
    pub const fn to_i64_date(self) -> i64 {
        self.0
    }

    /// Convert this to an `f64` value representing the `RataDie`
    pub(crate) const fn to_f64_date(self) -> f64 {
        self.0 as f64
    }

    /// Calculate the number of days between two `RataDie` in a const-friendly way
    ///
    /// ```
    /// use calendrical_calculations::julian::fixed_from_julian;
    ///
    /// assert_eq!(
    ///     fixed_from_julian(1930, 2, 2).since(fixed_from_julian(1930, 1, 1)),
    ///     32
    /// );
    /// ```
    pub const fn since(self, rhs: Self) -> i64 {
        self.0 - rhs.0
    }

    /// Calculate the number of days between two `RataDie` in a const-friendly way
    ///
    /// ```
    /// use calendrical_calculations::julian::fixed_from_julian;
    ///
    /// assert_eq!(
    ///     fixed_from_julian(1930, 1, 1).until(fixed_from_julian(1930, 2, 2)),
    ///     32
    /// );
    /// ```
    pub const fn until(self, rhs: Self) -> i64 {
        rhs.0 - self.0
    }

    /// Adds a number of days to this `RataDie` in a const-friendly way
    pub const fn add(self, rhs: i64) -> Self {
        let result = Self(self.0 + rhs);
        #[cfg(debug_assertions)]
        result.check();
        result
    }

    /// Convert this to a [`Moment`]
    pub(crate) const fn as_moment(self) -> Moment {
        Moment::new(self.0 as f64)
    }
}

impl fmt::Debug for RataDie {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let rd = self.0;
        if let Ok((y, m, d)) = crate::gregorian::gregorian_from_fixed(*self) {
            write!(f, "{rd} R.D. ({y}-{m:02}-{d:02})")
        } else {
            write!(f, "{rd} R.D. (out of bounds)")
        }
    }
}

/// Shift a RataDie N days into the future
impl Add<i64> for RataDie {
    type Output = Self;
    fn add(self, rhs: i64) -> Self::Output {
        self.add(rhs)
    }
}

impl AddAssign<i64> for RataDie {
    fn add_assign(&mut self, rhs: i64) {
        self.0 += rhs;
    }
}

/// Shift a RataDie N days into the past
impl Sub<i64> for RataDie {
    type Output = Self;
    fn sub(self, rhs: i64) -> Self::Output {
        let result = Self(self.0 - rhs);
        #[cfg(debug_assertions)]
        result.check();
        result
    }
}

impl SubAssign<i64> for RataDie {
    fn sub_assign(&mut self, rhs: i64) {
        self.0 -= rhs;
    }
}

/// Calculate the number of days between two RataDie
impl Sub for RataDie {
    type Output = i64;
    fn sub(self, rhs: Self) -> Self::Output {
        self.since(rhs)
    }
}

/// A moment is a RataDie with a fractional part giving the time of day.
///
/// NOTE: This should not cause overflow errors for most cases, but consider
/// alternative implementations if necessary.
#[derive(Debug, Copy, Clone, PartialEq, PartialOrd)]
pub(crate) struct Moment(f64);

/// Add a number of days to a Moment
impl Add<f64> for Moment {
    type Output = Self;
    fn add(self, rhs: f64) -> Self::Output {
        Self(self.0 + rhs)
    }
}

impl AddAssign<f64> for Moment {
    fn add_assign(&mut self, rhs: f64) {
        self.0 += rhs;
    }
}

/// Subtract a number of days from a Moment
impl Sub<f64> for Moment {
    type Output = Self;
    fn sub(self, rhs: f64) -> Self::Output {
        Self(self.0 - rhs)
    }
}

impl SubAssign<f64> for Moment {
    fn sub_assign(&mut self, rhs: f64) {
        self.0 -= rhs;
    }
}

/// Calculate the number of days between two moments
impl Sub for Moment {
    type Output = f64;
    fn sub(self, rhs: Self) -> Self::Output {
        self.0 - rhs.0
    }
}

impl Moment {
    /// Create a new moment
    pub const fn new(value: f64) -> Moment {
        Moment(value)
    }

    /// Get the inner field of a Moment
    pub const fn inner(self) -> f64 {
        self.0
    }

    /// Get the RataDie of a Moment
    pub fn as_rata_die(self) -> RataDie {
        RataDie::new(self.0.floor() as i64)
    }
}

#[test]
fn test_moment_to_rata_die_conversion() {
    for i in -1000..=1000 {
        let moment = Moment::new(i as f64);
        let rata_die = moment.as_rata_die();
        assert_eq!(rata_die.to_i64_date(), i);
    }
}
