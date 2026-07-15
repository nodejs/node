// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Error types for functions in the calendar crate

use crate::types::MonthCode;
use displaydoc::Display;

/// Error type for date creation via [`Date::try_new`].
///
/// [`Date::try_new`]: crate::Date::try_new
#[derive(Debug, Copy, Clone, PartialEq, Display)]
#[non_exhaustive]
pub enum DateError {
    /// A field is out of range for its domain.
    #[displaydoc("The {field} = {value} argument is out of range {min}..={max}")]
    Range {
        /// The field that is out of range, such as "year"
        field: &'static str,
        /// The actual value
        value: i32,
        /// The minimum value (inclusive). This might not be tight.
        min: i32,
        /// The maximum value (inclusive). This might not be tight.
        max: i32,
    },
    /// The era code is invalid for the calendar.
    #[displaydoc("Unknown era")]
    UnknownEra,
    /// The month code is invalid for the calendar or year.
    ///
    /// # Examples
    ///
    /// ```
    /// # #![allow(deprecated)]
    /// use icu::calendar::cal::Hebrew;
    /// use icu::calendar::types::Month;
    /// use icu::calendar::Date;
    /// use icu::calendar::DateError;
    ///
    /// Date::try_new_from_codes(None, 5784, Month::leap(5).code(), 1, Hebrew)
    ///     .expect("5784 is a leap year");
    ///
    /// let err =
    ///     Date::try_new_from_codes(None, 5785, Month::leap(5).code(), 1, Hebrew)
    ///         .expect_err("5785 is a common year");
    ///
    /// assert!(matches!(err, DateError::UnknownMonthCode(_)));
    /// ```
    #[displaydoc("Unknown month code {0:?}")]
    UnknownMonthCode(MonthCode),
}

impl core::error::Error for DateError {}

/// Error type for date creation for lunisolar calendars.
#[derive(Debug, Copy, Clone, PartialEq, Display)]
#[non_exhaustive]
pub enum LunisolarDateError {
    /// The day is invalid for the given month.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::error::LunisolarDateError;
    /// use icu::calendar::types::Month;
    /// use icu::calendar::Date;
    ///
    /// let err = Date::try_new_hebrew_v2(5785, Month::new(5), 50)
    ///     .expect_err("no month has 50 days");
    ///
    /// assert!(matches!(err, LunisolarDateError::InvalidDay { max: 30 }));
    /// ```
    #[displaydoc("Invalid day for month, max is {max}")]
    InvalidDay {
        /// The maximum allowed value (the minimum is 1).
        max: u8,
    },
    /// The month is invalid for the given year.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::error::LunisolarDateError;
    /// use icu::calendar::types::Month;
    /// use icu::calendar::Date;
    ///
    /// Date::try_new_hebrew_v2(5784, Month::leap(5), 1)
    ///     .expect("5784 is a leap year");
    ///
    /// let err = Date::try_new_hebrew_v2(5785, Month::leap(5), 1)
    ///     .expect_err("5785 is a common year");
    ///
    /// assert!(matches!(err, LunisolarDateError::MonthNotInYear));
    /// ```
    #[displaydoc("The specified month exists in this calendar, but not for this year")]
    MonthNotInYear,
    /// The month is invalid for the calendar.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::error::LunisolarDateError;
    /// use icu::calendar::types::Month;
    /// use icu::calendar::Date;
    ///
    /// let err = Date::try_new_hebrew_v2(5785, Month::leap(1), 1)
    ///     .expect_err("Tishrei does not have a leap month");
    ///
    /// assert!(matches!(err, LunisolarDateError::MonthNotInCalendar));
    /// ```
    #[displaydoc("The specified month does not exist in this calendar")]
    MonthNotInCalendar,
    /// The year exceeds the allowed range.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::error::LunisolarDateError;
    /// use icu::calendar::types::Month;
    /// use icu::calendar::Date;
    ///
    /// let err = Date::try_new_hebrew_v2(56812, Month::leap(5), 1)
    ///     .expect_err("56812 is too big");
    ///
    /// assert!(matches!(err, LunisolarDateError::InvalidYear));
    /// ```
    #[displaydoc("Invalid year")]
    InvalidYear,
}

impl core::error::Error for LunisolarDateError {}

/// Error type for date creation via [`Date::try_from_fields`].
///
/// [`Date::try_from_fields`]: crate::Date::try_from_fields
#[derive(Debug, Copy, Clone, PartialEq, Display)]
#[non_exhaustive]
pub enum DateFromFieldsError {
    /// The day is invalid for the given month.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::error::DateFromFieldsError;
    /// use icu::calendar::error::RangeError;
    /// use icu::calendar::types::DateFields;
    /// use icu::calendar::Date;
    /// use icu::calendar::Iso;
    ///
    /// let mut fields = DateFields::default();
    /// fields.extended_year = Some(2000);
    /// fields.ordinal_month = Some(11);
    /// fields.day = Some(31);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Iso)
    ///     .expect_err("no day 31 in November");
    ///
    /// assert!(matches!(err, DateFromFieldsError::InvalidDay { max: 30 }));
    /// ```
    #[displaydoc("Invalid day for month, max is {max}")]
    InvalidDay {
        /// The maximum allowed value (the minimum is 1).
        max: u8,
    },
    /// The ordinal month is is invalid for the given year.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::error::DateFromFieldsError;
    /// use icu::calendar::error::RangeError;
    /// use icu::calendar::types::DateFields;
    /// use icu::calendar::Date;
    /// use icu::calendar::Iso;
    ///
    /// let mut fields = DateFields::default();
    /// fields.extended_year = Some(2000);
    /// fields.ordinal_month = Some(13);
    /// fields.day = Some(1);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Iso)
    ///     .expect_err("no month 13 in the ISO calendar");
    ///
    /// assert!(matches!(
    ///     err,
    ///     DateFromFieldsError::InvalidOrdinalMonth { max: 12 }
    /// ));
    /// ```
    #[displaydoc("Invalid ordinal month for year, max is {max}")]
    InvalidOrdinalMonth {
        /// The maximum allowed value (the minimum is 1).
        max: u8,
    },
    /// The month code syntax is invalid.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::error::DateFromFieldsError;
    /// use icu::calendar::types::DateFields;
    /// use icu::calendar::Date;
    /// use icu::calendar::Iso;
    ///
    /// let mut fields = DateFields::default();
    /// fields.extended_year = Some(2000);
    /// fields.month_code = Some(b"sep");
    /// fields.day = Some(1);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Iso)
    ///     .expect_err("month code is invalid");
    ///
    /// assert_eq!(err, DateFromFieldsError::MonthCodeInvalidSyntax);
    /// ```
    #[displaydoc("Invalid month code syntax")]
    MonthCodeInvalidSyntax,
    /// The specified month does not exist in this calendar.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::cal::Hebrew;
    /// use icu::calendar::error::DateFromFieldsError;
    /// use icu::calendar::types::{DateFields, Month};
    /// use icu::calendar::Date;
    ///
    /// let mut fields = DateFields::default();
    /// fields.extended_year = Some(5783);
    /// fields.month = Some(Month::new(13));
    /// fields.day = Some(1);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Hebrew)
    ///     .expect_err("no month M13 in Hebrew");
    /// assert_eq!(err, DateFromFieldsError::MonthNotInCalendar);
    /// ```
    #[displaydoc("The specified month does not exist in this calendar")]
    MonthNotInCalendar,
    /// The specified month exists in this calendar, but not in the specified year.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::cal::Hebrew;
    /// use icu::calendar::error::DateFromFieldsError;
    /// use icu::calendar::types::{DateFields, Month};
    /// use icu::calendar::Date;
    ///
    /// let mut fields = DateFields::default();
    /// fields.extended_year = Some(5783);
    /// fields.month = Some(Month::leap(5));
    /// fields.day = Some(1);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Hebrew)
    ///     .expect_err("no month M05L in Hebrew year 5783");
    /// assert_eq!(err, DateFromFieldsError::MonthNotInYear);
    /// ```
    #[displaydoc("The specified month exists in this calendar, but not for this year")]
    MonthNotInYear,
    /// The era code is invalid for the calendar.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::cal::Hebrew;
    /// use icu::calendar::error::DateFromFieldsError;
    /// use icu::calendar::types::DateFields;
    /// use icu::calendar::Date;
    ///
    /// let mut fields = DateFields::default();
    /// fields.era = Some(b"ce"); // valid in Gregorian, but not Hebrew
    /// fields.era_year = Some(1);
    /// fields.ordinal_month = Some(1);
    /// fields.day = Some(1);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Hebrew::new())
    ///     .expect_err("era is unknown for Hebrew");
    ///
    /// assert_eq!(err, DateFromFieldsError::InvalidEra);
    /// ```
    #[displaydoc("Unknown era or invalid syntax")]
    InvalidEra,
    /// The year was specified in multiple inconsistent ways.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::cal::Japanese;
    /// use icu::calendar::error::DateFromFieldsError;
    /// use icu::calendar::types::DateFields;
    /// use icu::calendar::Date;
    ///
    /// let mut fields = DateFields::default();
    /// fields.era = Some(b"reiwa");
    /// fields.era_year = Some(6);
    /// fields.ordinal_month = Some(1);
    /// fields.day = Some(1);
    ///
    /// Date::try_from_fields(fields, Default::default(), Japanese::new())
    ///     .expect("a well-defined Japanese date");
    ///
    /// fields.extended_year = Some(1900);
    ///
    /// let err =
    ///     Date::try_from_fields(fields, Default::default(), Japanese::new())
    ///         .expect_err("year 1900 is not the same as 6 Reiwa");
    ///
    /// assert_eq!(err, DateFromFieldsError::InconsistentYear);
    /// ```
    #[displaydoc("Inconsistent year")]
    InconsistentYear,
    /// The month was specified in multiple inconsistent ways.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::cal::Hebrew;
    /// use icu::calendar::error::DateFromFieldsError;
    /// use icu::calendar::types::{DateFields, Month};
    /// use icu::calendar::Date;
    /// use tinystr::tinystr;
    ///
    /// let mut fields = DateFields::default();
    /// fields.extended_year = Some(5783);
    /// fields.month = Some(Month::new(6));
    /// fields.ordinal_month = Some(6);
    /// fields.day = Some(1);
    ///
    /// Date::try_from_fields(fields, Default::default(), Hebrew)
    ///     .expect("a well-defined Hebrew date in a common year");
    ///
    /// fields.extended_year = Some(5784);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Hebrew)
    ///     .expect_err("month M06 is not the 6th month in leap year 5784");
    ///
    /// assert_eq!(err, DateFromFieldsError::InconsistentMonth);
    /// ```
    #[displaydoc("Inconsistent month")]
    InconsistentMonth,
    /// Too many fields were specified to form a well-defined date.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::error::DateFromFieldsError;
    /// use icu::calendar::types::{DateFields, Month};
    /// use icu::calendar::Date;
    /// use icu::calendar::Iso;
    ///
    /// let mut fields = DateFields::default();
    /// fields.extended_year = Some(2000);
    /// fields.month = Some(Month::new(1));
    /// fields.month_code = Some(b"M01");
    /// fields.day = Some(1);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Iso)
    ///     .expect_err("cannot specify both month and month_code");
    ///
    /// assert_eq!(err, DateFromFieldsError::TooManyFields);
    /// ```
    #[displaydoc("Too many fields")]
    TooManyFields,
    /// Not enough fields were specified to form a well-defined date.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::cal::Hebrew;
    /// use icu::calendar::error::DateFromFieldsError;
    /// use icu::calendar::types::DateFields;
    /// use icu::calendar::Date;
    /// use tinystr::tinystr;
    ///
    /// let mut fields = DateFields::default();
    ///
    /// fields.ordinal_month = Some(3);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Hebrew)
    ///     .expect_err("need more than just an ordinal month");
    /// assert_eq!(err, DateFromFieldsError::NotEnoughFields);
    ///
    /// fields.era_year = Some(5783);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Hebrew)
    ///     .expect_err("need more than an ordinal month and an era year");
    /// assert_eq!(err, DateFromFieldsError::NotEnoughFields);
    ///
    /// fields.extended_year = Some(5783);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Hebrew)
    ///     .expect_err("era year still needs an era");
    /// assert_eq!(err, DateFromFieldsError::NotEnoughFields);
    ///
    /// fields.era = Some(b"am");
    ///
    /// let date = Date::try_from_fields(fields, Default::default(), Hebrew)
    ///     .expect_err("still missing the day");
    /// assert_eq!(err, DateFromFieldsError::NotEnoughFields);
    ///
    /// fields.day = Some(1);
    /// let date = Date::try_from_fields(fields, Default::default(), Hebrew)
    ///     .expect("we have enough fields!");
    /// ```
    #[displaydoc("Not enough fields")]
    NotEnoughFields,
    /// The date is out of range (see docs on [`Date`](crate::Date)
    /// for more information about `Date`'s fundamental range invariant).
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::error::DateFromFieldsError;
    /// use icu::calendar::error::RangeError;
    /// use icu::calendar::types::DateFields;
    /// use icu::calendar::Date;
    /// use icu::calendar::Iso;
    ///
    /// let mut fields = DateFields::default();
    /// fields.extended_year = Some(12345678);
    /// fields.ordinal_month = Some(12);
    /// fields.day = Some(31);
    ///
    /// let err = Date::try_from_fields(fields, Default::default(), Iso)
    ///     .expect_err("date out of range");
    ///
    /// assert!(matches!(
    ///     err,
    ///     DateFromFieldsError::Overflow
    /// ));
    #[displaydoc("Result out of range")]
    Overflow,
}

impl core::error::Error for DateFromFieldsError {}

/// Error type for date addition via [`Date::try_add_with_options`].
///
/// [`Date::try_add_with_options`]: crate::Date::try_add_with_options
#[derive(Debug, Copy, Clone, PartialEq, Display)]
#[non_exhaustive]
pub enum DateAddError {
    /// The day is invalid for the given month.
    ///
    /// This is only possible with [`Overflow::Reject`](crate::options::Overflow::Reject).
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::error::DateAddError;
    /// use icu::calendar::options::{DateAddOptions, Overflow};
    /// use icu::calendar::types::DateDuration;
    /// use icu::calendar::Date;
    ///
    /// // There is a day 31 in October but not in November.
    /// let d = Date::try_new_iso(2025, 10, 31).unwrap();
    /// let duration = DateDuration::for_months(1);
    ///
    /// let mut options = DateAddOptions::default();
    /// options.overflow = Some(Overflow::Reject);
    ///
    /// let err = d
    ///     .try_added_with_options(duration, options)
    ///     .expect_err("no day 31 in November");
    ///
    /// assert!(matches!(err, DateAddError::InvalidDay { max: 30 }));
    /// ```
    #[displaydoc("Invalid day for month, max is {max}")]
    InvalidDay {
        /// The maximum allowed value (the minimum is 1).
        max: u8,
    },
    /// The specified month exists in this calendar, but not in the specified year.
    ///
    /// This is only possible with [`Overflow::Reject`](crate::options::Overflow::Reject).
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::cal::Hebrew;
    /// use icu::calendar::error::DateAddError;
    /// use icu::calendar::options::{DateAddOptions, Overflow};
    /// use icu::calendar::types::{DateDuration, Month};
    /// use icu::calendar::Date;
    ///
    /// // Hebrew year 5784 is a leap year, 5785 is not.
    /// // Adar I (the leap month) is month 5 in a leap year.
    /// let d = Date::try_new_hebrew_v2(5784, Month::leap(5), 1).unwrap();
    /// let duration = DateDuration::for_years(1);
    ///
    /// let mut options = DateAddOptions::default();
    /// options.overflow = Some(Overflow::Reject);
    ///
    /// let err = d
    ///     .try_added_with_options(duration, options)
    ///     .expect_err("5785 is not a leap year");
    ///
    /// assert_eq!(err, DateAddError::MonthNotInYear);
    /// ```
    #[displaydoc("The specified month exists in this calendar, but not for this year")]
    MonthNotInYear,
    /// The date is out of range (see docs on [`Date`](crate::Date)
    /// for more information about `Date`'s fundamental range invariant).
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::error::DateAddError;
    /// use icu::calendar::types::DateDuration;
    /// use icu::calendar::Date;
    ///
    /// let d = Date::try_new_iso(2025, 1, 1).unwrap();
    /// let duration = DateDuration::for_years(1_000_000);
    ///
    /// let err = d
    ///     .try_added_with_options(duration, Default::default())
    ///     .expect_err("date overflow");
    ///
    /// assert_eq!(err, DateAddError::Overflow);
    /// ```
    #[displaydoc("Result out of range")]
    Overflow,
}

impl core::error::Error for DateAddError {}

/// Error returned when interacting two [`Date`](crate::Date)s with non-singleton calendars.
///
/// # Examples
///
/// ```
/// use icu::calendar::error::MismatchedCalendarError;
/// use icu::calendar::options::DateDifferenceOptions;
/// use icu::calendar::options::DateDurationUnit;
/// use icu::calendar::Date;
///
/// let d1 = Date::try_new_gregorian(2000, 1, 1).unwrap().to_any();
/// let d2 = Date::try_new_persian(1562, 1, 1).unwrap().to_any();
///
/// assert_eq!(
///     d1.try_until_with_options(&d2, Default::default())
///         .unwrap_err(),
///     MismatchedCalendarError,
/// );
///
/// // To compare the dates, convert them to the same calendar.
/// // Note that the result may differ based on the calendar used,
/// // e.g if comparing in months and days.
///
/// let mut options = DateDifferenceOptions::default();
/// options.largest_unit = Some(DateDurationUnit::Months);
///
/// let diff1 = d1
///     .to_calendar(d2.calendar().clone())
///     .try_until_with_options(&d2, options)
///     .unwrap();
/// let diff2 = d1
///     .try_until_with_options(&d2.to_calendar(d1.calendar().clone()), options)
///     .unwrap();
///
/// assert_ne!(diff1, diff2);
/// ```
#[derive(Clone, Copy, PartialEq, Debug, Display)]
#[displaydoc("Attempted to interact two `Date`s with different calendars")]
#[allow(
    clippy::exhaustive_structs,
    reason = "This is the only possible error with multi-calendar operations"
)]
pub struct MismatchedCalendarError;

impl core::error::Error for MismatchedCalendarError {}

/// Error type for date creation via [`Date::try_new`].
///
/// [`Date::try_new`]: crate::Date::try_new
#[derive(Debug, Copy, Clone, PartialEq, Display)]
#[non_exhaustive]
pub enum DateNewError {
    /// The day is invalid for the given month.
    #[displaydoc("Invalid day for month, max is {max}")]
    InvalidDay {
        /// The maximum allowed value (the minimum is 1).
        max: u8,
    },
    /// The specified month does not exist in this calendar.
    #[displaydoc("The specified month does not exist in this calendar")]
    MonthNotInCalendar,
    /// The specified month exists in this calendar, but not in the specified year.
    #[displaydoc("The specified month exists in this calendar, but not for this year")]
    MonthNotInYear,
    /// The era code is invalid for the calendar.
    #[displaydoc("Unknown era or invalid syntax")]
    InvalidEra,

    /// The year was out of range
    #[displaydoc("Invalid year, must be within -9999..=9999")]
    InvalidYear,
}

impl core::error::Error for DateNewError {}

impl From<UnknownEraError> for DateNewError {
    fn from(_: UnknownEraError) -> Self {
        Self::InvalidEra
    }
}

/// Internal narrow error type for functions that only fail on unknown eras
pub(crate) struct UnknownEraError;

impl From<UnknownEraError> for DateError {
    #[inline]
    fn from(_value: UnknownEraError) -> Self {
        DateError::UnknownEra
    }
}

impl From<UnknownEraError> for DateFromFieldsError {
    #[inline]
    fn from(_value: UnknownEraError) -> Self {
        DateFromFieldsError::InvalidEra
    }
}

/// Error for [`Month`](crate::types::Month) parsing
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum MonthCodeParseError {
    /// Invalid syntax
    InvalidSyntax,
}

impl From<MonthCodeParseError> for DateFromFieldsError {
    #[inline]
    fn from(value: MonthCodeParseError) -> Self {
        match value {
            MonthCodeParseError::InvalidSyntax => DateFromFieldsError::MonthCodeInvalidSyntax,
        }
    }
}

/// Internal narrow error type for functions that only fail on month operations
#[derive(Debug, PartialEq)]
pub(crate) enum MonthError {
    NotInCalendar,
    NotInYear,
}

impl From<MonthError> for DateFromFieldsError {
    #[inline]
    fn from(value: MonthError) -> Self {
        match value {
            MonthError::NotInCalendar => DateFromFieldsError::MonthNotInCalendar,
            MonthError::NotInYear => DateFromFieldsError::MonthNotInYear,
        }
    }
}

/// Errors that can occur when parsing an ISO 8601 date-only duration string.
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
#[non_exhaustive]
pub enum DateDurationParseError {
    /// The input does not follow the expected ISO 8601 date only duration structure.
    ///
    /// This error occurs when the duration string is incomplete,
    /// or contains unexpected characters.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::calendar::error::DateDurationParseError;
    /// use icu::calendar::types::DateDuration;
    ///
    /// assert_eq!(
    ///     DateDuration::try_from_str("P"),
    ///     Err(DateDurationParseError::InvalidStructure)
    /// );
    /// assert_eq!(
    ///     DateDuration::try_from_str("P1"),
    ///     Err(DateDurationParseError::InvalidStructure)
    /// );
    /// ```
    InvalidStructure,

    /// The duration contains a time component, which is not supported.
    ///
    /// Only date based units (`Y`, `M`, `W`, `D`) are supported. Any duration
    /// containing a `T` time separator is rejected.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::calendar::error::DateDurationParseError;
    /// use icu::calendar::types::DateDuration;
    ///
    /// assert_eq!(
    ///     DateDuration::try_from_str("PT5M"),
    ///     Err(DateDurationParseError::TimeNotSupported)
    /// );
    /// assert_eq!(
    ///     DateDuration::try_from_str("P1DT"),
    ///     Err(DateDurationParseError::TimeNotSupported)
    /// );
    /// ```
    TimeNotSupported,

    /// A duration unit appeared without a number before it.
    ///
    /// For example, the string contains `Y`, `M`, `W`, or `D` without a
    /// numeric value directly in front of it.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::calendar::error::DateDurationParseError;
    /// use icu::calendar::types::DateDuration;
    ///
    /// assert_eq!(
    ///     DateDuration::try_from_str("PY"),
    ///     Err(DateDurationParseError::MissingValue)
    /// );
    /// assert_eq!(
    ///     DateDuration::try_from_str("PX1D"),
    ///     Err(DateDurationParseError::MissingValue)
    /// );
    /// ```
    MissingValue,

    /// A duration unit was specified more than once.
    ///
    /// Each unit (`Y`, `M`, `W`, `D`) may appear at most once only.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::calendar::error::DateDurationParseError;
    /// use icu::calendar::types::DateDuration;
    ///
    /// assert_eq!(
    ///     DateDuration::try_from_str("P1Y2Y"),
    ///     Err(DateDurationParseError::DuplicateUnit)
    /// );
    /// assert_eq!(
    ///     DateDuration::try_from_str("P1D1D"),
    ///     Err(DateDurationParseError::DuplicateUnit)
    /// );
    /// ```
    DuplicateUnit,

    /// A numeric value exceeded or was more than the supported range.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::calendar::error::DateDurationParseError;
    /// use icu::calendar::types::DateDuration;
    ///
    /// assert_eq!(
    ///     DateDuration::try_from_str("P4294967296Y"),
    ///     Err(DateDurationParseError::NumberOverflow)
    /// );
    /// ```
    NumberOverflow,

    /// A duration starts with a `+` sign, which is not allowed.
    ///
    /// Only negative durations using a leading `-` are supported.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use icu::calendar::error::DateDurationParseError;
    /// use icu::calendar::types::DateDuration;
    ///
    /// assert_eq!(
    ///     DateDuration::try_from_str("+P1D"),
    ///     Err(DateDurationParseError::PlusNotAllowed)
    /// );
    /// ```
    PlusNotAllowed,
}

mod inner {
    /// Internal narrow error type for calculating the ECMA reference year
    ///
    /// Public but unstable because it is used on [`hijri::Rules`](crate::cal::hijri::Rules)
    ///
    /// <div class="stab unstable">
    /// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
    /// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
    ///
    /// Graduation tracking issue: [issue #6962](https://github.com/unicode-org/icu4x/issues/6962).
    /// </div>
    #[derive(Debug, Copy, Clone, PartialEq, Eq)]
    #[allow(missing_docs)] // TODO: fix when graduating
    #[non_exhaustive]
    pub enum EcmaReferenceYearError {
        Unimplemented,
        MonthNotInCalendar,
        /// This leap month could not be found within recent times.
        /// Constrain to the regular month if allowed, otherwise reject.
        ///
        /// Note: This currently encodes Chinese/Dangi-specific specced
        /// semantics. Before graduation, we should consider having this be
        /// something like UseIfConstrain(y, m, d).
        UseRegularIfConstrain,
    }
}

#[cfg(feature = "unstable")]
pub use inner::EcmaReferenceYearError;
#[cfg(not(feature = "unstable"))]
pub(crate) use inner::EcmaReferenceYearError;

impl From<EcmaReferenceYearError> for DateFromFieldsError {
    #[inline]
    fn from(value: EcmaReferenceYearError) -> Self {
        match value {
            EcmaReferenceYearError::Unimplemented => DateFromFieldsError::NotEnoughFields,
            EcmaReferenceYearError::MonthNotInCalendar => DateFromFieldsError::MonthNotInCalendar,
            // This only happens when the month-day combo isn't in the year, and if not rejecting
            // we constrain to the non-leap month
            //
            // We can potentially consider turning this into a RangeError, then UseRegularIfConstrain
            // would need to carry a range error inside it.
            EcmaReferenceYearError::UseRegularIfConstrain => DateFromFieldsError::MonthNotInYear,
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Display)]
/// An argument is out of range for its domain.
#[displaydoc("The {field} = {value} argument is out of range {min}..={max}")]
#[allow(clippy::exhaustive_structs)]
pub struct RangeError {
    /// The argument that is out of range, such as "year"
    pub field: &'static str,
    /// The actual value
    pub value: i32,
    /// The minimum value (inclusive). This might not be tight.
    pub min: i32,
    /// The maximum value (inclusive). This might not be tight.
    pub max: i32,
}

impl core::error::Error for RangeError {}

impl From<RangeError> for DateError {
    #[inline]
    fn from(value: RangeError) -> Self {
        let RangeError {
            field,
            value,
            min,
            max,
        } = value;
        DateError::Range {
            field,
            value,
            min,
            max,
        }
    }
}

pub(crate) fn range_check<T: Ord + Into<i32> + Copy>(
    value: T,
    field: &'static str,
    bounds: core::ops::RangeInclusive<T>,
) -> Result<T, RangeError> {
    if !bounds.contains(&value) {
        return Err(RangeError {
            field,
            value: value.into(),
            min: (*bounds.start()).into(),
            max: (*bounds.end()).into(),
        });
    }
    Ok(value)
}
