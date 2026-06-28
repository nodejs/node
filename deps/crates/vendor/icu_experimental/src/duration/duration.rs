// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Duration formatting

/// Represents a duration of time (intuitively, how long something took / will take).
/// Can be constructed ergonomically using the [`Default`] trait like so:
///
/// ```rust
/// # use icu_experimental::duration::Duration;
/// let d = Duration {
///     years: 1,
///     months: 2,
///     weeks: 3,
///     ..Default::default()
/// };
/// ```
#[allow(clippy::exhaustive_structs)] // this type should be stable (and is intended to be constructed manually)
#[derive(Debug, Default, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct Duration {
    /// Whether the duration is positive.
    pub sign: DurationSign,
    /// The number of years in the duration.
    pub years: u64,
    /// The number of months in the duration.
    pub months: u64,
    /// The number of weeks in the duration.
    pub weeks: u64,
    /// The number of days in the duration.
    pub days: u64,
    /// The number of hours in the duration.
    pub hours: u64,
    /// The number of minutes in the duration.
    pub minutes: u64,
    /// The number of seconds in the duration.
    pub seconds: u64,
    /// The number of milliseconds in the duration.
    pub milliseconds: u64,
    /// The number of microseconds in the duration.
    pub microseconds: u64,
    /// The number of nanoseconds in the duration.
    pub nanoseconds: u64,
}

impl Duration {
    /// Iterate over the units of the duration in descending order.
    pub(crate) fn iter_units(&self) -> [u64; 10] {
        [
            self.years,
            self.months,
            self.weeks,
            self.days,
            self.hours,
            self.minutes,
            self.seconds,
            self.milliseconds,
            self.microseconds,
            self.nanoseconds,
        ]
    }

    // Section 1.1.4 DurationSign
    pub(crate) fn get_sign(&self) -> fixed_decimal::Sign {
        for &unit in self.iter_units().iter() {
            if unit != 0 {
                return match self.sign {
                    DurationSign::Positive => fixed_decimal::Sign::None,
                    DurationSign::Negative => fixed_decimal::Sign::Negative,
                };
            }
        }
        fixed_decimal::Sign::None
    }

    // TODO: Currently, we do not validate durations.
    // // Section 1.1.5
    // pub(crate) fn is_valid_duration(&self) -> bool {
    //     todo!();
    // }
}

/// Describes whether a [`Duration`] is positive or negative.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
#[allow(clippy::exhaustive_enums)] // bool newtype
pub enum DurationSign {
    #[default]
    /// A positive duration.
    Positive,

    /// A negative duration.
    Negative,
}

impl Duration {
    /// Create a new positive [`Duration`] with all fields set to 0.
    pub fn new() -> Self {
        Self::default()
    }
}
