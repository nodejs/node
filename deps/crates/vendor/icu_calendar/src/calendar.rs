// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use calendrical_calculations::rata_die::RataDie;

use crate::cal::iso::IsoDateInner;
use crate::error::DateError;
use crate::{types, DateDuration, DateDurationUnit};
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
    type DateInner: Eq + Copy + fmt::Debug;
    /// The type of YearInfo returned by the date
    type Year: fmt::Debug + Into<types::YearInfo>;

    /// Construct a date from era/month codes and fields
    ///
    /// The year is extended_year if no era is provided
    #[allow(clippy::wrong_self_convention)]
    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError>;

    /// Construct the date from an ISO date
    #[allow(clippy::wrong_self_convention)]
    fn from_iso(&self, iso: IsoDateInner) -> Self::DateInner;
    /// Obtain an ISO date from this date
    fn to_iso(&self, date: &Self::DateInner) -> IsoDateInner;

    /// Construct the date from a [`RataDie`]
    #[allow(clippy::wrong_self_convention)]
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
    /// The extended year value
    fn extended_year(&self, date: &Self::DateInner) -> i32;
    /// The calendar-specific month represented by `date`
    fn month(&self, date: &Self::DateInner) -> types::MonthInfo;
    /// The calendar-specific day-of-month represented by `date`
    fn day_of_month(&self, date: &Self::DateInner) -> types::DayOfMonth;
    /// Information of the day of the year
    fn day_of_year(&self, date: &Self::DateInner) -> types::DayOfYear;

    #[doc(hidden)] // unstable
    /// Add `offset` to `date`
    fn offset_date(&self, date: &mut Self::DateInner, offset: DateDuration<Self>);
    #[doc(hidden)] // unstable
    /// Calculate `date2 - date` as a duration
    ///
    /// `calendar2` is the calendar object associated with `date2`. In case the specific calendar objects
    /// differ on data, the data for the first calendar is used, and `date2` may be converted if necessary.
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        calendar2: &Self,
        largest_unit: DateDurationUnit,
        smallest_unit: DateDurationUnit,
    ) -> DateDuration<Self>;

    /// Returns the [`CalendarAlgorithm`](crate::preferences::CalendarAlgorithm) that is required to match
    /// when parsing into this calendar.
    ///
    /// If left empty, any algorithm will parse successfully.
    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm>;

    /// Obtain a name for the calendar for debug printing
    fn debug_name(&self) -> &'static str;
}
