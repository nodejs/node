// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use calendrical_calculations::rata_die::RataDie;

use crate::cal::iso::IsoDateInner;
use crate::error::{DateError, DateFromFieldsError};
use crate::options::DateFromFieldsOptions;
use crate::options::{DateAddOptions, DateDifferenceOptions};
use crate::{types, Iso};
use core::fmt;

/// A calendar implementation
///
/// Only implementors of [`Calendar`] should care about these methods, in general users of
/// these calendars should use the methods on [`Date`](crate::Date) instead.
///
/// Individual [`Calendar`] implementations may have inherent utility methods
/// allowing for direct construction, etc.
///
/// <div class="stab unstable">
/// 🚫 This trait is sealed; it should not be implemented by user code. If an API requests an item that implements this
/// trait, please consider using a type from the implementors listed below.
///
/// It is still possible to implement this trait in userland (since `UnstableSealed` is public),
/// do not do so unless you are prepared for things to occasionally break.
/// </div>
pub trait Calendar: crate::cal::scaffold::UnstableSealed {
    /// The internal type used to represent dates
    ///
    /// Equality and ordering should observe normal calendar semantics.
    type DateInner: Eq + Copy + PartialOrd + fmt::Debug;
    /// The type of YearInfo returned by the date
    type Year: fmt::Debug + Into<types::YearInfo>;
    /// The type of error returned by `until`
    type DifferenceError;

    /// Construct a date from era/month codes and fields
    ///
    /// The year is the [extended year](crate::Date::extended_year) if no era is provided
    #[expect(clippy::wrong_self_convention)]
    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError>;

    /// Construct a date from a bag of date fields.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
    ///
    /// Graduation tracking issue: [issue #7161](https://github.com/unicode-org/icu4x/issues/7161).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    #[expect(clippy::wrong_self_convention)]
    #[cfg(feature = "unstable")]
    fn from_fields(
        &self,
        fields: types::DateFields,
        options: DateFromFieldsOptions,
    ) -> Result<Self::DateInner, DateFromFieldsError>;

    /// Whether `from_iso`/`to_iso` is more efficient
    /// than `from_rata_die`/`to_rata_die`.
    fn has_cheap_iso_conversion(&self) -> bool;

    /// Construct the date from an ISO date.
    ///
    /// Only called if `HAS_CHEAP_ISO_CONVERSION` is set.
    #[expect(clippy::wrong_self_convention)]
    fn from_iso(&self, iso: IsoDateInner) -> Self::DateInner {
        self.from_rata_die(Iso.to_rata_die(&iso))
    }
    /// Obtain an ISO date from this date.
    ///
    /// Only called if `HAS_CHEAP_ISO_CONVERSION` is set.
    fn to_iso(&self, date: &Self::DateInner) -> IsoDateInner {
        Iso.from_rata_die(self.to_rata_die(date))
    }

    /// Construct the date from a [`RataDie`]
    #[expect(clippy::wrong_self_convention)]
    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner;
    /// Obtain a [`RataDie`] from this date
    fn to_rata_die(&self, date: &Self::DateInner) -> RataDie;

    /// Count the number of months in a given year, specified by providing a date
    /// from that year
    fn months_in_year(&self, date: &Self::DateInner) -> u8;
    /// Count the number of days in a given year, specified by providing a date
    /// from that year
    fn days_in_year(&self, date: &Self::DateInner) -> u16;
    /// Count the number of days in a given month, specified by providing a date
    /// from that year/month
    fn days_in_month(&self, date: &Self::DateInner) -> u8;
    /// Calculate if a date is in a leap year
    fn is_in_leap_year(&self, date: &Self::DateInner) -> bool;

    /// Information about the year
    fn year_info(&self, date: &Self::DateInner) -> Self::Year;

    /// The [extended year](crate::Date::extended_year).
    fn extended_year(&self, date: &Self::DateInner) -> i32 {
        self.year_info(date).into().extended_year()
    }
    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo;
    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth;
    /// Information of the day of the year
    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear;

    /// Add `duration` to `date`
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
    ///
    /// Graduation tracking issue: [issue #3964](https://github.com/unicode-org/icu4x/issues/3964).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    #[cfg(feature = "unstable")]
    fn add(
        &self,
        date: &Self::DateInner,
        duration: types::DateDuration,
        options: DateAddOptions,
    ) -> Result<Self::DateInner, DateError>;

    /// Calculate `date2 - date` as a duration
    ///
    /// `calendar2` is the calendar object associated with `date2`. In case the specific calendar objects
    /// differ on data, the data for the first calendar is used, and `date2` may be converted if necessary.
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
    ///
    /// Graduation tracking issue: [issue #3964](https://github.com/unicode-org/icu4x/issues/3964).
    /// </div>
    ///
    /// ✨ *Enabled with the `unstable` Cargo feature.*
    #[cfg(feature = "unstable")]
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        options: DateDifferenceOptions,
    ) -> Result<types::DateDuration, Self::DifferenceError>;

    /// Returns the [`CalendarAlgorithm`](crate::preferences::CalendarAlgorithm) that is required to match
    /// when parsing into this calendar.
    ///
    /// If left empty, any algorithm will parse successfully.
    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm>;

    /// Obtain a name for the calendar for debug printing
    fn debug_name(&self) -> &'static str;
}
