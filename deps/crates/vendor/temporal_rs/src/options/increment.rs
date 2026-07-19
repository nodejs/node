use core::num::{NonZeroU128, NonZeroU32};

use crate::{TemporalError, TemporalResult};
use num_traits::float::FloatCore;

// ==== RoundingIncrement option ====

// Invariants:
// RoundingIncrement(n): 1 <= n < 10^9
// TODO: Add explanation on what exactly are rounding increments.

/// A numeric rounding increment.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct RoundingIncrement(pub(crate) NonZeroU32);

impl Default for RoundingIncrement {
    fn default() -> Self {
        Self::ONE
    }
}

impl TryFrom<f64> for RoundingIncrement {
    type Error = TemporalError;

    fn try_from(value: f64) -> Result<Self, Self::Error> {
        // GetRoundingIncrementOption ( options )
        // https://tc39.es/proposal-temporal/#sec-temporal-getroundingincrementoption

        // 4. If increment is not finite, throw a RangeError exception.
        if !value.is_finite() {
            return Err(TemporalError::range().with_message("roundingIncrement must be finite"));
        }

        // 5. Let integerIncrement be truncate(ℝ(increment)).
        let integer_increment = FloatCore::trunc(value);
        // 6. If integerIncrement < 1 or integerIncrement > 10**9, throw a RangeError exception.
        // 7. Return integerIncrement.
        if !(1.0..=1_000_000_000.0).contains(&integer_increment) {
            Err(TemporalError::range()
                .with_message("roundingIncrement cannot be less that 1 or bigger than 10**9"))
        } else {
            // We already range-checked, so we can just unwrap_or and expect it to never be reached
            Ok(Self(
                NonZeroU32::new(integer_increment as u32).unwrap_or(NonZeroU32::MIN),
            ))
        }
    }
}

impl RoundingIncrement {
    // Using `MIN` avoids either a panic or using NonZeroU32::new_unchecked
    /// A rounding increment of 1 (normal rounding).
    pub const ONE: Self = Self(NonZeroU32::MIN);

    /// Create a new `RoundingIncrement`.
    ///
    /// # Errors
    ///
    /// - If `increment` is less than 1 or bigger than 10**9.
    pub fn try_new(increment: u32) -> TemporalResult<Self> {
        if !(1..=1_000_000_000).contains(&increment) {
            Err(TemporalError::range()
                .with_message("roundingIncrement cannot be less that 1 or bigger than 10**9"))
        } else {
            // We already range-checked, so we can just unwrap_or and expect it to never be reached
            Ok(Self(NonZeroU32::new(increment).unwrap_or(NonZeroU32::MIN)))
        }
    }

    /// Create a new `RoundingIncrement` without checking the validity of the
    /// increment.
    ///
    /// The increment is expected to be in `1..=10⁹`
    ///
    /// This is safe to invoke (and will result in incorrect but not unsafe behavior if
    /// invoked with an out-of-range `increment`).
    pub const fn new_unchecked(increment: NonZeroU32) -> Self {
        Self(increment)
    }

    /// Gets the numeric value of this `RoundingIncrement`.
    pub const fn get(self) -> u32 {
        self.0.get()
    }

    // ValidateTemporalRoundingIncrement ( increment, dividend, inclusive )
    // https://tc39.es/proposal-temporal/#sec-validatetemporalroundingincrement
    pub(crate) fn validate(self, dividend: u64, inclusive: bool) -> TemporalResult<()> {
        // 1. If inclusive is true, then
        //     a. Let maximum be dividend.
        // 2. Else,
        //     a. Assert: dividend > 1.
        //     b. Let maximum be dividend - 1.
        let max = dividend - u64::from(!inclusive);

        let increment = u64::from(self.get());

        // 3. If increment > maximum, throw a RangeError exception.
        if increment > max {
            return Err(TemporalError::range().with_message("roundingIncrement exceeds maximum"));
        }

        // 4. If dividend modulo increment ≠ 0, then
        if dividend.rem_euclid(increment) != 0 {
            //     a. Throw a RangeError exception.
            return Err(TemporalError::range()
                .with_message("dividend is not divisible by roundingIncrement"));
        }

        // 5. Return unused.
        Ok(())
    }

    pub(crate) fn as_extended_increment(&self) -> NonZeroU128 {
        NonZeroU128::from(self.0)
    }
}
