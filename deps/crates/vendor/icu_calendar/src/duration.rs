// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::Calendar;
use core::fmt;
use core::marker::PhantomData;

/// A duration between two dates
///
/// Can be used to perform date arithmetic
///
/// # Example
///
/// ```rust
/// use icu::calendar::{types::Weekday, Date, DateDuration, DateDurationUnit};
///
/// // Creating ISO date: 1992-09-02.
/// let mut date_iso = Date::try_new_iso(1992, 9, 2)
///     .expect("Failed to initialize ISO Date instance.");
///
/// assert_eq!(date_iso.day_of_week(), Weekday::Wednesday);
/// assert_eq!(date_iso.era_year().year, 1992);
/// assert_eq!(date_iso.month().ordinal, 9);
/// assert_eq!(date_iso.day_of_month().0, 2);
///
/// // Answering questions about days in month and year.
/// assert_eq!(date_iso.days_in_year(), 366);
/// assert_eq!(date_iso.days_in_month(), 30);
///
/// // Advancing date in-place by 1 year, 2 months, 3 weeks, 4 days.
/// date_iso.add(DateDuration::new(1, 2, 3, 4));
/// assert_eq!(date_iso.era_year().year, 1993);
/// assert_eq!(date_iso.month().ordinal, 11);
/// assert_eq!(date_iso.day_of_month().0, 27);
///
/// // Reverse date advancement.
/// date_iso.add(DateDuration::new(-1, -2, -3, -4));
/// assert_eq!(date_iso.era_year().year, 1992);
/// assert_eq!(date_iso.month().ordinal, 9);
/// assert_eq!(date_iso.day_of_month().0, 2);
///
/// // Creating ISO date: 2022-01-30.
/// let newer_date_iso = Date::try_new_iso(2022, 1, 30)
///     .expect("Failed to initialize ISO Date instance.");
///
/// // Comparing dates: 2022-01-30 and 1992-09-02.
/// let duration = newer_date_iso.until(
///     &date_iso,
///     DateDurationUnit::Years,
///     DateDurationUnit::Days,
/// );
/// assert_eq!(duration.years, 30);
/// assert_eq!(duration.months, -8);
/// assert_eq!(duration.days, 28);
///
/// // Create new date with date advancement. Reassign to new variable.
/// let mutated_date_iso = date_iso.added(DateDuration::new(1, 2, 3, 4));
/// assert_eq!(mutated_date_iso.era_year().year, 1993);
/// assert_eq!(mutated_date_iso.month().ordinal, 11);
/// assert_eq!(mutated_date_iso.day_of_month().0, 27);
/// ```
///
/// Currently unstable for ICU4X 1.0
#[derive(Eq, PartialEq)]
#[allow(clippy::exhaustive_structs)] // this type should be stable (and is intended to be constructed manually)
#[doc(hidden)] // unstable
pub struct DateDuration<C: Calendar + ?Sized> {
    /// The number of years
    pub years: i32,
    /// The number of months
    pub months: i32,
    /// The number of weeks
    pub weeks: i32,
    /// The number of days
    pub days: i32,
    /// A marker for the calendar
    pub marker: PhantomData<C>,
}

// Custom impl so that C need not be bound on Copy/Clone
impl<C: Calendar + ?Sized> Clone for DateDuration<C> {
    fn clone(&self) -> Self {
        *self
    }
}

// Custom impl so that C need not be bound on Copy/Clone
impl<C: Calendar + ?Sized> Copy for DateDuration<C> {}

/// A "duration unit" used to specify the minimum or maximum duration of time to
/// care about
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
#[allow(clippy::exhaustive_enums)] // this type should be stable
#[doc(hidden)] // unstable
pub enum DateDurationUnit {
    /// Duration in years
    Years,
    /// Duration in months
    Months,
    /// Duration in weeks
    Weeks,
    /// Duration in days
    Days,
}

impl<C: Calendar + ?Sized> Default for DateDuration<C> {
    fn default() -> Self {
        Self {
            years: 0,
            months: 0,
            weeks: 0,
            days: 0,
            marker: PhantomData,
        }
    }
}

impl<C: Calendar + ?Sized> DateDuration<C> {
    /// Construct a DateDuration
    ///
    /// ```rust
    /// # use icu::calendar::*;
    /// // two years, three months, and five days
    /// let duration: DateDuration<Iso> = DateDuration::new(2, 3, 0, 5);
    /// ```
    pub fn new(years: i32, months: i32, weeks: i32, days: i32) -> Self {
        DateDuration {
            years,
            months,
            weeks,
            days,
            marker: PhantomData,
        }
    }

    /// Explicitly cast duration to one for a different calendar
    pub fn cast_unit<C2: Calendar + ?Sized>(self) -> DateDuration<C2> {
        DateDuration {
            years: self.years,
            months: self.months,
            days: self.days,
            weeks: self.weeks,
            marker: PhantomData,
        }
    }
}

impl<C: Calendar> fmt::Debug for DateDuration<C> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> Result<(), fmt::Error> {
        f.debug_struct("DateDuration")
            .field("years", &self.years)
            .field("months", &self.months)
            .field("weeks", &self.weeks)
            .field("days", &self.days)
            .finish()
    }
}
