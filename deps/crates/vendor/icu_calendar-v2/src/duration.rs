// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::error::DateDurationParseError;

/// A signed length of time in terms of days, weeks, months, and years.
///
/// This type represents the abstract concept of a date duration. For example, a duration of
/// "1 month" is represented as "1 month" in the data model, without any context of how many
/// days the month might be.
///
/// [`DateDuration`] is the input and output type of date arithmetic operations in `icu_calendar`,
/// such as [`Date::try_add_with_options()`] and [`Date::try_until_with_options()`].
/// It is not designed to be used more generally as a duration, such as for parsing,
/// formatting, or storage.
///
/// [`Date::try_add_with_options()`]: crate::Date::try_add_with_options
/// [`Date::try_until_with_options()`]: crate::Date::try_until_with_options
/// [`Date`]: crate::Date
///
/// # Example
///
/// ```rust
/// use icu::calendar::options::DateDifferenceOptions;
/// use icu::calendar::options::DateDurationUnit;
/// use icu::calendar::types::DateDuration;
/// use icu::calendar::types::Weekday;
/// use icu::calendar::Date;
///
/// // Creating ISO date: 1992-09-02.
/// let mut date_iso = Date::try_new_iso(1992, 9, 2)
///     .expect("Failed to initialize ISO Date instance.");
///
/// assert_eq!(date_iso.weekday(), Weekday::Wednesday);
/// assert_eq!(date_iso.era_year().year, 1992);
/// assert_eq!(date_iso.month().ordinal, 9);
/// assert_eq!(date_iso.day_of_month().0, 2);
///
/// // Answering questions about days in month and year.
/// assert_eq!(date_iso.days_in_year(), 366);
/// assert_eq!(date_iso.days_in_month(), 30);
///
/// // Advancing date in-place by 1 year, 2 months, 3 weeks, 4 days.
/// date_iso
///     .try_add_with_options(
///         DateDuration {
///             is_negative: false,
///             years: 1,
///             months: 2,
///             weeks: 3,
///             days: 4,
///         },
///         Default::default(),
///     )
///     .unwrap();
/// assert_eq!(date_iso.era_year().year, 1993);
/// assert_eq!(date_iso.month().ordinal, 11);
/// assert_eq!(date_iso.day_of_month().0, 27);
///
/// // Reverse date advancement.
/// date_iso
///     .try_add_with_options(
///         DateDuration {
///             is_negative: true,
///             years: 1,
///             months: 2,
///             weeks: 3,
///             days: 4,
///         },
///         Default::default(),
///     )
///     .unwrap();
/// assert_eq!(date_iso.era_year().year, 1992);
/// assert_eq!(date_iso.month().ordinal, 9);
/// assert_eq!(date_iso.day_of_month().0, 2);
///
/// // Creating ISO date: 2022-01-30.
/// let newer_date_iso = Date::try_new_iso(2022, 10, 30)
///     .expect("Failed to initialize ISO Date instance.");
///
/// // Comparing dates: 2022-01-30 and 1992-09-02.
/// let mut options = DateDifferenceOptions::default();
/// options.largest_unit = Some(DateDurationUnit::Years);
/// let Ok(duration) =
///     newer_date_iso.try_until_with_options(&date_iso, options);
/// assert_eq!(duration.years, 30);
/// assert_eq!(duration.months, 1);
/// assert_eq!(duration.days, 28);
///
/// // Create new date with date advancement. Reassign to new variable.
/// let mutated_date_iso = date_iso
///     .try_added_with_options(
///         DateDuration {
///             is_negative: false,
///             years: 1,
///             months: 2,
///             weeks: 3,
///             days: 4,
///         },
///         Default::default(),
///     )
///     .unwrap();
/// assert_eq!(mutated_date_iso.era_year().year, 1993);
/// assert_eq!(mutated_date_iso.month().ordinal, 11);
/// assert_eq!(mutated_date_iso.day_of_month().0, 27);
/// ```
#[derive(Debug, Copy, Clone, Eq, PartialEq, Default)]
#[allow(clippy::exhaustive_structs)] // spec-defined in Temporal
pub struct DateDuration {
    /// Whether the duration is negative.
    ///
    /// A negative duration is an abstract concept that could result, for example, from
    /// taking the difference between two [`Date`](crate::Date)s in ascending order.
    ///
    /// The fields of the duration are either all positive or all negative. Mixed signs
    /// are not possible.
    ///
    /// By convention, this field should be `false` if the duration is zero.
    pub is_negative: bool,
    /// The number of years
    pub years: u32,
    /// The number of months
    pub months: u32,
    /// The number of weeks
    pub weeks: u32,
    /// The number of days
    pub days: u32,
}

impl DateDuration {
    /// Parses an ISO 8601 date-only duration string into a [`DateDuration`].
    ///
    /// This is a wrapper around [`Self::try_from_utf8`] for UTF-8
    /// string inputs.
    pub fn try_from_str(s: &str) -> Result<Self, DateDurationParseError> {
        Self::try_from_utf8(s.as_bytes())
    }

    /// See [`Self::try_from_str`].
    pub fn try_from_utf8(code_units: &[u8]) -> Result<Self, DateDurationParseError> {
        let mut s = code_units;

        let mut is_negative = false;
        match s {
            [b'-', rest @ ..] => {
                is_negative = true;
                s = rest;
            }
            [b'+', ..] => return Err(DateDurationParseError::PlusNotAllowed),
            _ => {}
        }

        match s {
            [b'P', rest @ ..] => s = rest,
            _ => return Err(DateDurationParseError::InvalidStructure),
        }

        if s.is_empty() {
            return Err(DateDurationParseError::InvalidStructure);
        }

        let mut years: u32 = 0;
        let mut months: u32 = 0;
        let mut weeks: u32 = 0;
        let mut days: u32 = 0;

        let mut seen_years = false;
        let mut seen_months = false;
        let mut seen_weeks = false;
        let mut seen_days = false;

        while !s.is_empty() {
            if matches!(s, [b'T', ..]) {
                return Err(DateDurationParseError::TimeNotSupported);
            }

            let mut value: u64 = 0;
            let mut has_digits = false;

            while let [b @ b'0'..=b'9', rest @ ..] = s {
                value = value
                    .checked_mul(10)
                    .and_then(|v| v.checked_add((b - b'0') as u64))
                    .ok_or(DateDurationParseError::NumberOverflow)?;
                s = rest;
                has_digits = true;
            }

            if !has_digits {
                return Err(DateDurationParseError::MissingValue);
            }

            match s {
                [b'Y', rest @ ..] => {
                    if seen_years {
                        return Err(DateDurationParseError::DuplicateUnit);
                    }
                    years =
                        u32::try_from(value).map_err(|_| DateDurationParseError::NumberOverflow)?;
                    seen_years = true;
                    s = rest;
                }
                [b'M', rest @ ..] => {
                    if seen_months {
                        return Err(DateDurationParseError::DuplicateUnit);
                    }
                    months =
                        u32::try_from(value).map_err(|_| DateDurationParseError::NumberOverflow)?;
                    seen_months = true;
                    s = rest;
                }
                [b'W', rest @ ..] => {
                    if seen_weeks {
                        return Err(DateDurationParseError::DuplicateUnit);
                    }
                    weeks =
                        u32::try_from(value).map_err(|_| DateDurationParseError::NumberOverflow)?;
                    seen_weeks = true;
                    s = rest;
                }
                [b'D', rest @ ..] => {
                    if seen_days {
                        return Err(DateDurationParseError::DuplicateUnit);
                    }
                    days =
                        u32::try_from(value).map_err(|_| DateDurationParseError::NumberOverflow)?;
                    seen_days = true;
                    s = rest;
                }
                _ => return Err(DateDurationParseError::InvalidStructure),
            }
        }

        Ok(Self {
            is_negative,
            years,
            months,
            weeks,
            days,
        })
    }

    /// Returns a new [`DateDuration`] representing a number of years.
    pub fn for_years(years: i32) -> Self {
        Self {
            is_negative: years.is_negative(),
            years: years.unsigned_abs(),
            ..Default::default()
        }
    }

    /// Returns a new [`DateDuration`] representing a number of months.
    pub fn for_months(months: i32) -> Self {
        Self {
            is_negative: months.is_negative(),
            months: months.unsigned_abs(),
            ..Default::default()
        }
    }

    /// Returns a new [`DateDuration`] representing a number of weeks.
    pub fn for_weeks(weeks: i32) -> Self {
        Self {
            is_negative: weeks.is_negative(),
            weeks: weeks.unsigned_abs(),
            ..Default::default()
        }
    }

    /// Returns a new [`DateDuration`] representing a number of days.
    pub fn for_days(days: i32) -> Self {
        Self {
            is_negative: days.is_negative(),
            days: days.unsigned_abs(),
            ..Default::default()
        }
    }

    /// Returns a new [`DateDuration`] representing a number of days
    /// represented as weeks and days
    pub(crate) fn for_weeks_and_days(days: i32) -> Self {
        let weeks = days / 7;
        let days = days % 7;
        Self::from_signed_ymwd(0, 0, weeks, days)
    }

    /// Do NOT pass this function values of mixed signs!
    pub(crate) fn from_signed_ymwd(years: i32, months: i32, weeks: i32, days: i32) -> Self {
        let is_negative = years.is_negative()
            || months.is_negative()
            || weeks.is_negative()
            || days.is_negative();
        if is_negative
            && (years.is_positive()
                || months.is_positive()
                || weeks.is_positive()
                || days.is_positive())
        {
            debug_assert!(false, "mixed signs in from_signed_ymd");
        }
        Self {
            is_negative,
            years: years.unsigned_abs(),
            months: months.unsigned_abs(),
            weeks: weeks.unsigned_abs(),
            days: days.unsigned_abs(),
        }
    }

    #[inline]
    pub(crate) fn add_years_to(&self, year: i32) -> i32 {
        if !self.is_negative {
            match year.checked_add_unsigned(self.years) {
                Some(x) => x,
                None => {
                    debug_assert!(false, "{year} + {self:?} out of year range");
                    i32::MAX
                }
            }
        } else {
            match year.checked_sub_unsigned(self.years) {
                Some(x) => x,
                None => {
                    debug_assert!(false, "{year} - {self:?} out of year range");
                    i32::MIN
                }
            }
        }
    }

    #[inline]
    pub(crate) fn add_months_to(&self, month: u8) -> i32 {
        debug_assert!(i32::try_from(self.months).is_ok());
        if !self.is_negative {
            i32::from(month) + (self.months as i32)
        } else {
            i32::from(month) - (self.months as i32)
        }
    }

    #[inline]
    pub(crate) fn add_weeks_and_days_to(&self, day: u8) -> i32 {
        debug_assert!(i32::try_from(self.weeks).is_ok());
        if !self.is_negative {
            let day = i32::from(day) + (self.weeks as i32) * 7;
            match day.checked_add_unsigned(self.days) {
                Some(x) => x,
                None => {
                    debug_assert!(false, "{day} + {self:?} out of day range");
                    i32::MAX
                }
            }
        } else {
            let day = i32::from(day) - (self.weeks as i32) * 7;
            match day.checked_sub_unsigned(self.days) {
                Some(x) => x,
                None => {
                    debug_assert!(false, "{day} - {self:?} out of day range");
                    i32::MIN
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_single_unit_durations() {
        let d = DateDuration::try_from_str("P1D").unwrap();
        assert_eq!(
            d,
            DateDuration {
                days: 1,
                ..Default::default()
            }
        );

        let d = DateDuration::try_from_str("P3W").unwrap();
        assert_eq!(
            d,
            DateDuration {
                weeks: 3,
                ..Default::default()
            }
        );

        let d = DateDuration::try_from_str("P5M").unwrap();
        assert_eq!(
            d,
            DateDuration {
                months: 5,
                ..Default::default()
            }
        );

        let d = DateDuration::try_from_str("P7Y").unwrap();
        assert_eq!(
            d,
            DateDuration {
                years: 7,
                ..Default::default()
            }
        );
    }

    #[test]
    fn parse_multi_unit_durations() {
        let d = DateDuration::try_from_str("P1Y3M5W7D").unwrap();
        assert_eq!(
            d,
            DateDuration {
                years: 1,
                months: 3,
                weeks: 5,
                days: 7,
                ..Default::default()
            }
        );
    }

    #[test]
    fn parse_negative_durations() {
        let d = DateDuration::try_from_str("-P9W").unwrap();
        assert_eq!(
            d,
            DateDuration {
                is_negative: true,
                weeks: 9,
                ..Default::default()
            }
        );
    }
}
