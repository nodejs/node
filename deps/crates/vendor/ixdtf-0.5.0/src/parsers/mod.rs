// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! The parser module contains the implementation details for `IxdtfParser` and `IsoDurationParser`

use crate::{ParseError, ParserResult};

#[cfg(feature = "duration")]
use records::DurationParseRecord;
use records::{IxdtfParseRecord, UtcOffsetRecord};

use self::records::Annotation;

pub mod records;

mod annotations;
pub(crate) mod datetime;
#[cfg(feature = "duration")]
pub(crate) mod duration;
mod grammar;
mod time;
pub(crate) mod timezone;

#[cfg(test)]
mod tests;

/// `assert_syntax!` is a parser specific utility macro for asserting a syntax test, and returning the
/// the provided provided error if the assertion fails.
#[macro_export]
macro_rules! assert_syntax {
    ($cond:expr, $err:ident $(,)?) => {
        if !$cond {
            return Err(ParseError::$err);
        }
    };
}

/// `IxdtfParser` is the primary parser implementation of `ixdtf`.
///
/// This parser provides various options for parsing date/time strings with the extended notation
/// laid out in [RFC9557][rfc9557] along with other variations laid out in the [`Temporal`][temporal-proposal].
///
/// ```rust
/// use ixdtf::parsers::{
///     records::{Sign, TimeZoneRecord, UtcOffsetRecord},
///     IxdtfParser,
/// };
///
/// let ixdtf_str = "2024-03-02T08:48:00-05:00[America/New_York]";
///
/// let result = IxdtfParser::from_str(ixdtf_str).parse().unwrap();
///
/// let date = result.date.unwrap();
/// let time = result.time.unwrap();
/// let offset = result.offset.unwrap().resolve_rfc_9557();
/// let tz_annotation = result.tz.unwrap();
///
/// assert_eq!(date.year, 2024);
/// assert_eq!(date.month, 3);
/// assert_eq!(date.day, 2);
/// assert_eq!(time.hour, 8);
/// assert_eq!(time.minute, 48);
/// assert_eq!(offset.sign(), Sign::Negative);
/// assert_eq!(offset.hour(), 5);
/// assert_eq!(offset.minute(), 0);
/// assert_eq!(offset.second(), None);
/// assert_eq!(offset.fraction(), None);
/// assert!(!tz_annotation.critical);
/// assert_eq!(
///     tz_annotation.tz,
///     TimeZoneRecord::Name("America/New_York".as_bytes())
/// );
/// ```
///
/// [rfc9557]: https://datatracker.ietf.org/doc/rfc9557/
/// [temporal-proposal]: https://tc39.es/proposal-temporal/
#[derive(Debug)]
pub struct IxdtfParser<'a> {
    cursor: Cursor<'a>,
}

impl<'a> IxdtfParser<'a> {
    /// Creates a new `IxdtfParser` from a slice of utf-8 bytes.
    #[inline]
    #[must_use]
    pub fn from_utf8(source: &'a [u8]) -> Self {
        Self {
            cursor: Cursor::new(source),
        }
    }

    /// Creates a new `IxdtfParser` from a source `&str`.
    #[inline]
    #[must_use]
    #[allow(clippy::should_implement_trait)]
    pub fn from_str(source: &'a str) -> Self {
        Self::from_utf8(source.as_bytes())
    }

    /// Parses the source as an [extended Date/Time string][rfc9557].
    ///
    /// This is the baseline parse method for `ixdtf`. For this method, the
    /// TimeRecord, UTCOffsetRecord, and all annotations are optional.
    ///
    /// # Example
    ///
    /// [rfc9557]: https://datatracker.ietf.org/doc/rfc9557/
    pub fn parse(&mut self) -> ParserResult<IxdtfParseRecord<'a>> {
        self.parse_with_annotation_handler(Some)
    }

    /// Parses the source as an extended Date/Time string with an Annotation handler.
    ///
    /// For more, see [Implementing Annotation Handlers](crate#implementing-annotation-handlers)
    pub fn parse_with_annotation_handler(
        &mut self,
        handler: impl FnMut(Annotation<'a>) -> Option<Annotation<'a>>,
    ) -> ParserResult<IxdtfParseRecord<'a>> {
        datetime::parse_annotated_date_time(&mut self.cursor, handler)
    }

    /// Parses the source as an extended [YearMonth string][temporal-ym].
    ///
    /// # Example
    ///
    /// ```rust
    /// # use ixdtf::parsers::IxdtfParser;
    ///
    /// let extended_year_month = "2020-11[u-ca=iso8601]";
    ///
    /// let result = IxdtfParser::from_str(extended_year_month)
    ///     .parse_year_month()
    ///     .unwrap();
    ///
    /// let date = result.date.unwrap();
    ///
    /// assert_eq!(date.year, 2020);
    /// assert_eq!(date.month, 11);
    /// ```
    ///
    /// [temporal-ym]: https://tc39.es/proposal-temporal/#prod-TemporalYearMonthString
    pub fn parse_year_month(&mut self) -> ParserResult<IxdtfParseRecord<'a>> {
        self.parse_year_month_with_annotation_handler(Some)
    }

    /// Parses the source as an extended YearMonth string with an Annotation handler.
    ///
    /// For more, see [Implementing Annotation Handlers](crate#implementing-annotation-handlers)
    pub fn parse_year_month_with_annotation_handler(
        &mut self,
        handler: impl FnMut(Annotation<'a>) -> Option<Annotation<'a>>,
    ) -> ParserResult<IxdtfParseRecord<'a>> {
        datetime::parse_annotated_year_month(&mut self.cursor, handler)
    }

    /// Parses the source as an extended [MonthDay string][temporal-md].
    ///
    /// # Example
    ///
    /// ```rust
    /// # use ixdtf::parsers::IxdtfParser;
    /// let extended_month_day = "1107[+04:00]";
    ///
    /// let result = IxdtfParser::from_str(extended_month_day)
    ///     .parse_month_day()
    ///     .unwrap();
    ///
    /// let date = result.date.unwrap();
    ///
    /// assert_eq!(date.month, 11);
    /// assert_eq!(date.day, 7);
    /// ```
    ///
    /// [temporal-md]: https://tc39.es/proposal-temporal/#prod-TemporalMonthDayString
    pub fn parse_month_day(&mut self) -> ParserResult<IxdtfParseRecord<'a>> {
        self.parse_month_day_with_annotation_handler(Some)
    }

    /// Parses the source as an extended MonthDay string with an Annotation handler.
    ///
    /// For more, see [Implementing Annotation Handlers](crate#implementing-annotation-handlers)
    pub fn parse_month_day_with_annotation_handler(
        &mut self,
        handler: impl FnMut(Annotation<'a>) -> Option<Annotation<'a>>,
    ) -> ParserResult<IxdtfParseRecord<'a>> {
        datetime::parse_annotated_month_day(&mut self.cursor, handler)
    }

    /// Parses the source as an extended [Time string][temporal-time].
    ///
    /// # Example
    ///
    /// ```rust
    /// # use ixdtf::parsers::{IxdtfParser, records::{Sign, TimeZoneRecord}};
    /// let extended_time = "12:01:04-05:00[America/New_York][u-ca=iso8601]";
    ///
    /// let result = IxdtfParser::from_str(extended_time).parse_time().unwrap();
    ///
    /// let time = result.time.unwrap();
    /// let offset = result.offset.unwrap().resolve_rfc_9557();
    /// let tz_annotation = result.tz.unwrap();
    ///
    /// assert_eq!(time.hour, 12);
    /// assert_eq!(time.minute, 1);
    /// assert_eq!(time.second, 4);
    /// assert_eq!(offset.sign(), Sign::Negative);
    /// assert_eq!(offset.hour(), 5);
    /// assert_eq!(offset.minute(), 0);
    /// assert!(!tz_annotation.critical);
    /// assert_eq!(
    ///     tz_annotation.tz,
    ///     TimeZoneRecord::Name("America/New_York".as_bytes())
    /// );
    /// ```
    ///
    /// [temporal-time]: https://tc39.es/proposal-temporal/#prod-TemporalTimeString
    pub fn parse_time(&mut self) -> ParserResult<IxdtfParseRecord<'a>> {
        self.parse_time_with_annotation_handler(Some)
    }

    /// Parses the source as an extended Time string with an Annotation handler.
    ///
    /// For more, see [Implementing Annotation Handlers](crate#implementing-annotation-handlers)
    pub fn parse_time_with_annotation_handler(
        &mut self,
        handler: impl FnMut(Annotation<'a>) -> Option<Annotation<'a>>,
    ) -> ParserResult<IxdtfParseRecord<'a>> {
        time::parse_annotated_time_record(&mut self.cursor, handler)
    }
}

/// A parser for time zone offset and IANA identifier strings.
///
/// ✨ *Enabled with the `timezone` Cargo feature.*
#[derive(Debug)]
pub struct TimeZoneParser<'a> {
    cursor: Cursor<'a>,
}

impl<'a> TimeZoneParser<'a> {
    /// Creates a new `TimeZoneParser` from a slice of utf-8 bytes.
    #[inline]
    #[must_use]
    pub fn from_utf8(source: &'a [u8]) -> Self {
        Self {
            cursor: Cursor::new(source),
        }
    }

    /// Creates a new `TimeZoneParser` from a source `&str`.
    #[inline]
    #[must_use]
    #[allow(clippy::should_implement_trait)]
    pub fn from_str(source: &'a str) -> Self {
        Self::from_utf8(source.as_bytes())
    }

    /// Parse a UTC offset from the provided source.
    ///
    /// This method can parse both a minute precision and full
    /// precision offset.
    ///
    /// ## Minute precision offset example
    ///
    /// ```rust
    /// use ixdtf::parsers::{records::Sign, TimeZoneParser};
    ///
    /// let offset_src = "-05:00";
    /// let parse_result =
    ///     TimeZoneParser::from_str(offset_src).parse_offset().unwrap();
    /// assert_eq!(parse_result.sign(), Sign::Negative);
    /// assert_eq!(parse_result.hour(), 5);
    /// assert_eq!(parse_result.minute(), 0);
    /// assert_eq!(parse_result.second(), None);
    /// assert_eq!(parse_result.fraction(), None);
    /// ```
    ///
    /// ## Full precision offset example
    ///
    /// ```rust
    /// use ixdtf::parsers::{records::Sign, TimeZoneParser};
    ///
    /// let offset_src = "-05:00:30.123456789";
    /// let parse_result =
    ///     TimeZoneParser::from_str(offset_src).parse_offset().unwrap();
    /// assert_eq!(parse_result.sign(), Sign::Negative);
    /// assert_eq!(parse_result.hour(), 5);
    /// assert_eq!(parse_result.minute(), 0);
    /// assert_eq!(parse_result.second(), Some(30));
    /// let fraction = parse_result.fraction().unwrap();
    /// assert_eq!(fraction.to_nanoseconds(), Some(123456789));
    /// ```
    #[inline]
    pub fn parse_offset(&mut self) -> ParserResult<UtcOffsetRecord> {
        let result = timezone::parse_utc_offset(&mut self.cursor)?;
        self.cursor.close()?;
        Ok(result)
    }

    /// Parse an IANA identifier name.
    ///
    ///
    /// ```rust
    /// use ixdtf::parsers::{records::Sign, TimeZoneParser};
    ///
    /// let iana_identifier = "America/Chicago";
    /// let parse_result = TimeZoneParser::from_str(iana_identifier)
    ///     .parse_iana_identifier()
    ///     .unwrap();
    /// assert_eq!(parse_result, iana_identifier.as_bytes());
    ///
    /// let iana_identifier = "Europe/Berlin";
    /// let parse_result = TimeZoneParser::from_str(iana_identifier)
    ///     .parse_iana_identifier()
    ///     .unwrap();
    /// assert_eq!(parse_result, iana_identifier.as_bytes());
    /// ```
    #[inline]
    pub fn parse_iana_identifier(&mut self) -> ParserResult<&'a [u8]> {
        let result = timezone::parse_tz_iana_name(&mut self.cursor)?;
        self.cursor.close()?;
        Ok(result)
    }
}

/// A parser for ISO8601 Duration strings.
///
/// ✨ *Enabled with the `duration` Cargo feature.*
///
/// # Example
///
/// ```rust
/// use ixdtf::parsers::{IsoDurationParser, records::{Sign, DurationParseRecord, TimeDurationRecord}};
///
/// let duration_str = "P1Y2M1DT2H10M30S";
///
/// let result = IsoDurationParser::from_str(duration_str).parse().unwrap();
///
/// let date_duration = result.date.unwrap();
///
/// let (hours, minutes, seconds, fraction) = match result.time {
///     // Hours variant is defined as { hours: u32, fraction: Option<Fraction> }
///     Some(TimeDurationRecord::Hours{ hours, fraction }) => (hours, 0, 0, fraction),
///     // Minutes variant is defined as { hours: u32, minutes: u32, fraction: Option<Fraction> }
///     Some(TimeDurationRecord::Minutes{ hours, minutes, fraction }) => (hours, minutes, 0, fraction),
///     // Seconds variant is defined as { hours: u32, minutes: u32, seconds: u32, fraction: Option<Fraction> }
///     Some(TimeDurationRecord::Seconds{ hours, minutes, seconds, fraction }) => (hours, minutes, seconds, fraction),
///     None => (0,0,0, None),
/// };
///
/// assert_eq!(result.sign, Sign::Positive);
/// assert_eq!(date_duration.years, 1);
/// assert_eq!(date_duration.months, 2);
/// assert_eq!(date_duration.weeks, 0);
/// assert_eq!(date_duration.days, 1);//
/// assert_eq!(hours, 2);
/// assert_eq!(minutes, 10);
/// assert_eq!(seconds, 30);
/// assert_eq!(fraction, None);
/// ```
#[cfg(feature = "duration")]
#[derive(Debug)]
pub struct IsoDurationParser<'a> {
    cursor: Cursor<'a>,
}

#[cfg(feature = "duration")]
impl<'a> IsoDurationParser<'a> {
    /// Creates a new `IsoDurationParser` from a slice of utf-8 bytes.
    #[inline]
    #[must_use]
    pub fn from_utf8(source: &'a [u8]) -> Self {
        Self {
            cursor: Cursor::new(source),
        }
    }

    /// Creates a new `IsoDurationParser` from a source `&str`.
    #[inline]
    #[must_use]
    #[allow(clippy::should_implement_trait)]
    pub fn from_str(source: &'a str) -> Self {
        Self::from_utf8(source.as_bytes())
    }

    /// Parse the contents of this `IsoDurationParser` into a `DurationParseRecord`.
    ///
    /// # Examples
    ///
    /// ## Parsing a date duration
    ///
    /// ```
    /// # use ixdtf::parsers::{IsoDurationParser, records::DurationParseRecord };
    /// let date_duration = "P1Y2M3W1D";
    ///
    /// let result = IsoDurationParser::from_str(date_duration).parse().unwrap();
    ///
    /// let date_duration = result.date.unwrap();
    ///
    /// assert!(result.time.is_none());
    /// assert_eq!(date_duration.years, 1);
    /// assert_eq!(date_duration.months, 2);
    /// assert_eq!(date_duration.weeks, 3);
    /// assert_eq!(date_duration.days, 1);
    /// ```
    ///
    /// ## Parsing a time duration
    ///
    /// ```rust
    /// # use ixdtf::parsers::{IsoDurationParser, records::{DurationParseRecord, TimeDurationRecord }};
    /// let time_duration = "PT2H10M30S";
    ///
    /// let result = IsoDurationParser::from_str(time_duration).parse().unwrap();
    ///
    /// let (hours, minutes, seconds, fraction) = match result.time {
    ///     // Hours variant is defined as { hours: u32, fraction: Option<Fraction> }
    ///     Some(TimeDurationRecord::Hours{ hours, fraction }) => (hours, 0, 0, fraction),
    ///     // Minutes variant is defined as { hours: u32, minutes: u32, fraction: Option<Fraction> }
    ///     Some(TimeDurationRecord::Minutes{ hours, minutes, fraction }) => (hours, minutes, 0, fraction),
    ///     // Seconds variant is defined as { hours: u32, minutes: u32, seconds: u32, fraction: Option<Fraction> }
    ///     Some(TimeDurationRecord::Seconds{ hours, minutes, seconds, fraction }) => (hours, minutes, seconds, fraction),
    ///     None => (0,0,0, None),
    /// };
    /// assert!(result.date.is_none());
    /// assert_eq!(hours, 2);
    /// assert_eq!(minutes, 10);
    /// assert_eq!(seconds, 30);
    /// assert_eq!(fraction, None);
    /// ```
    pub fn parse(&mut self) -> ParserResult<DurationParseRecord> {
        duration::parse_duration(&mut self.cursor)
    }
}

// ==== Mini cursor implementation for Iso8601 targets ====

/// `Cursor` is a small cursor implementation for parsing Iso8601 grammar.
#[derive(Debug)]
pub(crate) struct Cursor<'a> {
    pos: usize,
    source: &'a [u8],
}

impl<'a> Cursor<'a> {
    /// Create a new cursor from a source `String` value.
    #[must_use]
    pub fn new(source: &'a [u8]) -> Self {
        Self { pos: 0, source }
    }

    /// Returns a string value from a slice of the cursor.
    fn slice(&self, start: usize, end: usize) -> Option<&'a [u8]> {
        self.source.get(start..end)
    }

    /// Get current position
    const fn pos(&self) -> usize {
        self.pos
    }

    /// Peek the value at next position (current + 1).
    fn peek(&self) -> Option<u8> {
        self.peek_n(1)
    }

    /// Returns current position in source as `char`.
    fn current(&self) -> Option<u8> {
        self.peek_n(0)
    }

    /// Peeks the value at `n` as a `char`.
    fn peek_n(&self, n: usize) -> Option<u8> {
        self.source.get(self.pos + n).copied()
    }

    /// Runs the provided check on the current position.
    fn check<F>(&self, f: F) -> Option<bool>
    where
        F: FnOnce(u8) -> bool,
    {
        self.current().map(f)
    }

    /// Runs the provided check on current position returns the default value if None.
    fn check_or<F>(&self, default: bool, f: F) -> bool
    where
        F: FnOnce(u8) -> bool,
    {
        self.current().map_or(default, f)
    }

    /// Returns `Cursor`'s current char and advances to the next position.
    fn next(&mut self) -> Option<u8> {
        let result = self.current();
        self.advance_n(1);
        result
    }

    /// Returns the next value as a digit
    ///
    /// # Errors
    ///   - Returns an AbruptEnd error if cursor ends.
    fn next_digit(&mut self) -> ParserResult<Option<u8>> {
        let ascii_char = self.next_or(ParseError::AbruptEnd { location: "digit" })?;
        if ascii_char.is_ascii_digit() {
            Ok(Some(ascii_char - 48))
        } else {
            Ok(None)
        }
    }

    /// A utility next method that returns an `AbruptEnd` error if invalid.
    fn next_or(&mut self, err: ParseError) -> ParserResult<u8> {
        self.next().ok_or(err)
    }

    /// Advances the cursor's position by n bytes.
    fn advance_n(&mut self, n: usize) {
        self.pos += n;
    }

    // Advances the cursor by 1 byte.
    fn advance(&mut self) {
        self.advance_n(1)
    }

    /// Utility function to advance when a condition is true
    fn advance_if(&mut self, condition: bool) {
        if condition {
            self.advance();
        }
    }

    /// Closes the current cursor by checking if all contents have been consumed. If not, returns an error for invalid syntax.
    fn close(&mut self) -> ParserResult<()> {
        if self.pos < self.source.len() {
            return Err(ParseError::InvalidEnd);
        }
        Ok(())
    }
}
