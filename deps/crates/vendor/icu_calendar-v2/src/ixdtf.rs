// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::str::FromStr;

use crate::cal::abstract_gregorian::AbstractGregorianYear;
use crate::cal::iso::IsoDateInner;
use crate::calendar_arithmetic::{ArithmeticDate, VALID_RD_RANGE};
use crate::types::RataDie;
use crate::{AsCalendar, Calendar, Date, Iso, RangeError};
use calendrical_calculations::gregorian::fixed_from_gregorian;
use icu_locale_core::preferences::extensions::unicode::keywords::CalendarAlgorithm;
use ixdtf::encoding::Utf8;
use ixdtf::parsers::IxdtfParser;
use ixdtf::records::IxdtfParseRecord;
use ixdtf::ParseError as Rfc9557Error;

/// An error returned from parsing an RFC 9557 string to an `icu::calendar` type.
#[derive(Debug, displaydoc::Display)]
#[non_exhaustive]
pub enum ParseError {
    /// Syntax error.
    #[displaydoc("Syntax error in the RFC 9557 string: {0}")]
    Syntax(Rfc9557Error),
    /// Value is out of range.
    #[displaydoc("Value out of range: {0}")]
    Range(RangeError),
    /// The RFC 9557 string is missing fields required for parsing into the chosen type.
    MissingFields,
    /// The RFC 9557 string specifies an unknown calendar.
    UnknownCalendar,
    /// Expected a different calendar.
    #[displaydoc("Expected calendar {0:?} but found calendar {1:?}")]
    MismatchedCalendar(CalendarAlgorithm, CalendarAlgorithm),
}

impl From<RangeError> for ParseError {
    fn from(value: RangeError) -> Self {
        Self::Range(value)
    }
}

impl From<Rfc9557Error> for ParseError {
    fn from(value: Rfc9557Error) -> Self {
        Self::Syntax(value)
    }
}

impl FromStr for Date<Iso> {
    type Err = ParseError;
    fn from_str(rfc_9557_str: &str) -> Result<Self, Self::Err> {
        Self::try_from_str(rfc_9557_str, Iso)
    }
}

impl<A: AsCalendar> Date<A> {
    /// Creates a [`Date`] in the given calendar from an RFC 9557 string.
    ///
    /// Returns an error if the string has a calendar annotation that does not
    /// match the calendar argument.
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    ///
    /// # Examples
    ///
    /// ```
    /// use icu::calendar::{Date, Gregorian};
    ///
    /// let date = Date::try_from_str("2024-07-17", Gregorian).unwrap();
    /// let date =
    ///     Date::try_from_str("2024-07-17[u-ca=gregory]", Gregorian).unwrap();
    /// let _ =
    ///     Date::try_from_str("2024-07-17[u-ca=hebrew]", Gregorian).unwrap_err();
    ///
    /// assert_eq!(date.era_year().year, 2024);
    /// assert_eq!(date.month().number(), 7);
    /// assert_eq!(date.day_of_month().0, 17);
    /// ```
    pub fn try_from_str(rfc_9557_str: &str, calendar: A) -> Result<Self, ParseError> {
        Self::try_from_utf8(rfc_9557_str.as_bytes(), calendar)
    }

    /// Creates a [`Date`] in the given calendar from an RFC 9557 string.
    ///
    /// Returns an error if the string has a calendar annotation that does not
    /// match the calendar argument.
    ///
    /// See [`Self::try_from_str()`].
    ///
    /// ✨ *Enabled with the `ixdtf` Cargo feature.*
    pub fn try_from_utf8(rfc_9557_str: &[u8], calendar: A) -> Result<Self, ParseError> {
        let ixdtf_record = IxdtfParser::from_utf8(rfc_9557_str).parse()?;
        Self::try_from_ixdtf_record(&ixdtf_record, calendar)
    }

    #[doc(hidden)]
    pub fn try_from_ixdtf_record(
        ixdtf_record: &IxdtfParseRecord<'_, Utf8>,
        calendar: A,
    ) -> Result<Self, ParseError> {
        let inner = Self::try_inner_from_ixdtf_record(
            ixdtf_record,
            calendar.as_calendar().calendar_algorithm(),
        )?;

        // this is free for AbstractGregorian calendars
        Ok(Date::from_raw(inner, Iso).to_calendar(calendar))
    }

    // like `try_from_ixdtf_record`, but also returns the RD, which is useful
    // for creating the `ZoneNameTimestamp`
    #[doc(hidden)]
    pub fn try_from_ixdtf_record_with_rd(
        ixdtf_record: &IxdtfParseRecord<'_, Utf8>,
        calendar: A,
    ) -> Result<(Self, RataDie), ParseError> {
        let inner = Self::try_inner_from_ixdtf_record(
            ixdtf_record,
            calendar.as_calendar().calendar_algorithm(),
        )?;

        let rd = Iso.to_rata_die(&inner);

        let c = calendar.as_calendar();
        let inner = if c.has_cheap_iso_conversion() {
            // no-op
            c.from_iso(inner)
        } else {
            // `from_rata_die` precondition is satified by `to_rata_die`
            c.from_rata_die(rd)
        };

        Ok((Date::from_raw(inner, calendar), rd))
    }

    // Parses into `IsoDateInner` but validates the calendar annotation.
    // Does not perform any arithmetic.
    fn try_inner_from_ixdtf_record(
        ixdtf_record: &IxdtfParseRecord<'_, Utf8>,
        calendar: Option<CalendarAlgorithm>,
    ) -> Result<IsoDateInner, ParseError> {
        let date_record = ixdtf_record.date.ok_or(ParseError::MissingFields)?;

        if let Some(ixdtf_calendar) = ixdtf_record.calendar {
            if let Some(expected_calendar) = calendar {
                if ixdtf_calendar != expected_calendar.as_str().as_bytes() {
                    return Err(ParseError::MismatchedCalendar(
                        expected_calendar,
                        icu_locale_core::extensions::unicode::Value::try_from_utf8(ixdtf_calendar)
                            .ok()
                            .and_then(|v| CalendarAlgorithm::try_from(&v).ok())
                            .ok_or(ParseError::UnknownCalendar)?,
                    ));
                }
            }
        }

        // `date_record` is in `VALID_RD_RANGE` by `ixdtf` invariants
        const _: () = assert!(
            VALID_RD_RANGE.start().to_i64_date()
                <= fixed_from_gregorian(-999_999, 1, 1).to_i64_date()
                && fixed_from_gregorian(999_999, 12, 31).to_i64_date()
                    <= VALID_RD_RANGE.end().to_i64_date()
        );

        Ok(IsoDateInner(ArithmeticDate::new_unchecked(
            AbstractGregorianYear::from_iso_year(date_record.year),
            date_record.month,
            date_record.day,
        )))
    }
}

#[test]
fn invalid_calendar() {
    Date::try_from_str("2025-01-01T00:00:00[u-ca=foo]", crate::Gregorian).unwrap_err();
}
