// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use calendrical_calculations::rata_die::RataDie;

use crate::cal::iso::IsoDateInner;
use crate::error::{DateAddError, DateError, DateFromFieldsError, DateNewError};
use crate::options::DateFromFieldsOptions;
use crate::options::{DateAddOptions, DateDifferenceOptions};
use crate::types::{self, Month, YearInput};
use crate::Iso;
use core::fmt;

#[cfg(doc)]
use crate::Date;

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
    /// Using the [`Eq`] or [`PartialOrd`] implementations requires
    /// the associated calendars to have passed [`Self::check_date_compatibility`].
    type DateInner: Eq + Copy + PartialOrd + fmt::Debug;
    /// The type of year info returned by the date
    type Year: fmt::Debug + Into<types::YearInfo>;
    /// The error that is returned by [`Self::check_date_compatibility`].
    ///
    /// Set this to [`core::convert::Infallible`] if the type is a singleton or
    /// the parameterization does not affect date semantics.
    type DateCompatibilityError: fmt::Debug;

    /// Construct a date from era/month codes and fields
    ///
    /// The year is the [extended year](crate::Date::extended_year) if no era is provided
    ///
    /// This is used by the deprecated [`Date::try_new_from_codes()`]. Implementors
    /// should rely on the default impl which adapts this to using [`Self::new_date()`].
    #[expect(clippy::wrong_self_convention)]
    #[deprecated(since = "2.2.0", note = "use `new_date`")]
    fn from_codes(
        &self,
        era: Option<&str>,
        year: i32,
        month_code: types::MonthCode,
        day: u8,
    ) -> Result<Self::DateInner, DateError> {
        let input_year = match era {
            Some(e) => YearInput::EraYear(e, year),
            None => year.into(),
        };
        let month = match Month::try_from_utf8(month_code.0.as_bytes()) {
            Ok(m) => m,
            Err(_) => return Err(DateError::UnknownMonthCode(month_code)),
        };
        let result = self.new_date(input_year, month, day);

        match result {
            Ok(date) => Ok(date),
            Err(codes_error) => Err(match codes_error {
                DateNewError::InvalidDay { max } => DateError::Range {
                    field: "day",
                    value: day as i32,
                    min: 1,
                    max: max as i32,
                },
                DateNewError::MonthNotInCalendar | DateNewError::MonthNotInYear => {
                    DateError::UnknownMonthCode(month_code)
                }
                DateNewError::InvalidEra => DateError::UnknownEra,
                DateNewError::InvalidYear => DateError::Range {
                    field: "year",
                    value: year,
                    min: -9999,
                    max: 9999,
                },
            }),
        }
    }

    /// Construct a date from a [`YearInput`], [`Month`], and a day.
    ///
    /// This is used by [`Date::try_new()`].
    fn new_date(
        &self,
        year: YearInput,
        month: Month,
        day: u8,
    ) -> Result<Self::DateInner, DateNewError>;

    /// Construct a date from a bag of date fields.
    #[expect(clippy::wrong_self_convention)]
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
        // `from_rata_die` precondition is satified by `to_rata_die`
        self.from_rata_die(Iso.to_rata_die(&iso))
    }
    /// Obtain an ISO date from this date.
    ///
    /// Only called if `HAS_CHEAP_ISO_CONVERSION` is set.
    fn to_iso(&self, date: &Self::DateInner) -> IsoDateInner {
        // `from_rata_die` precondition is satified by `to_rata_die`
        Iso.from_rata_die(self.to_rata_die(date))
    }

    /// Construct the date from a [`RataDie`]
    ///
    /// Precondition: `rd` needs to be in the `VALID_RD_RANGE`
    #[expect(clippy::wrong_self_convention)]
    fn from_rata_die(&self, rd: RataDie) -> Self::DateInner;
    /// Obtain a [`RataDie`] from this date
    ///
    /// The result is guaranteed to be in `VALID_RD_RANGE`
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
    fn add(
        &self,
        date: &Self::DateInner,
        duration: types::DateDuration,
        options: DateAddOptions,
    ) -> Result<Self::DateInner, DateAddError>;

    /// Calculate `date2 - date` as a duration.
    ///
    /// This requires the associated calendars to have passed [`Self::check_date_compatibility`].
    fn until(
        &self,
        date1: &Self::DateInner,
        date2: &Self::DateInner,
        options: DateDifferenceOptions,
    ) -> types::DateDuration;

    /// Returns whether [`Self::DateInner`] represents the same date in both calendars.
    ///
    /// This is checked by [`Date::try_until_with_options`](crate::Date::try_until_with_options),
    /// `impl PartialEq for Date<C>`, `impl PartialOrd for Date<C>`, and `impl Ord for Date<C>`.
    fn check_date_compatibility(&self, other: &Self) -> Result<(), Self::DateCompatibilityError>;

    /// Returns the [`CalendarAlgorithm`](crate::preferences::CalendarAlgorithm) that is required to match
    /// when parsing into this calendar.
    ///
    /// If left empty, any algorithm will parse successfully.
    fn calendar_algorithm(&self) -> Option<crate::preferences::CalendarAlgorithm>;

    /// Obtain a name for the calendar for debug printing
    fn debug_name(&self) -> &'static str;
}
