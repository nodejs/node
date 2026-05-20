//! Parsed intermediate types
//!
//! These are types which have been *parsed* from an IXDTF string, extracting
//! calendar/time zone/etc information, but have not yet been validated.
//!
//! They are primarily useful for clients implementing the Temporal specification
//! since the specification performs observable operations
//! between the parse and validate steps.

use crate::error::ErrorMessage;
use crate::error::TemporalError;
use crate::iso::IsoTime;
use crate::parsers;
use crate::provider::TimeZoneProvider;
use crate::Calendar;
use crate::TemporalResult;
use crate::TemporalUnwrap;
use crate::TimeZone;
use crate::UtcOffset;
use icu_calendar::AnyCalendarKind;
use ixdtf::records::DateRecord;
use ixdtf::records::UtcOffsetRecordOrZ;

fn extract_kind(calendar: Option<&[u8]>) -> TemporalResult<AnyCalendarKind> {
    Ok(calendar
        .map(Calendar::try_kind_from_utf8)
        .transpose()?
        .unwrap_or(AnyCalendarKind::Iso))
}

/// A parsed-but-not-validated date
#[derive(Copy, Clone, Debug)]
pub struct ParsedDate {
    pub(crate) record: DateRecord,
    pub(crate) calendar: AnyCalendarKind,
}

impl ParsedDate {
    /// Converts a UTF-8 encoded string into a `ParsedDate`.
    pub fn from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let parse_record = parsers::parse_date_time(s)?;

        let calendar = extract_kind(parse_record.calendar)?;

        // Assertion: PlainDate must exist on a DateTime parse.
        let record = parse_record.date.temporal_unwrap()?;

        Ok(Self { record, calendar })
    }
    /// Converts a UTF-8 encoded YearMonth string into a `ParsedDate`.
    pub fn year_month_from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let parse_record = parsers::parse_year_month(s)?;

        let calendar = extract_kind(parse_record.calendar)?;

        // Assertion: PlainDate must exist on a DateTime parse.
        let record = parse_record.date.temporal_unwrap()?;

        Ok(Self { record, calendar })
    }
    /// Converts a UTF-8 encoded MonthDay string into a `ParsedDate`.
    pub fn month_day_from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let parse_record = parsers::parse_month_day(s)?;

        let calendar = extract_kind(parse_record.calendar)?;

        // Assertion: PlainDate must exist on a DateTime parse.
        let record = parse_record.date.temporal_unwrap()?;

        Ok(Self { record, calendar })
    }
}

/// A parsed-but-not-validated datetime
#[derive(Copy, Clone, Debug)]
pub struct ParsedDateTime {
    pub(crate) date: ParsedDate,
    pub(crate) time: IsoTime,
}

impl ParsedDateTime {
    /// Converts a UTF-8 encoded string into a `ParsedDateTime`.
    pub fn from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let parse_record = parsers::parse_date_time(s)?;

        let calendar = extract_kind(parse_record.calendar)?;

        let time = parse_record
            .time
            .map(IsoTime::from_time_record)
            .transpose()?
            .unwrap_or_default();

        let record = parse_record.date.temporal_unwrap()?;
        Ok(Self {
            date: ParsedDate { record, calendar },
            time,
        })
    }
}

/// A parsed-but-not-validated zoned datetime
#[derive(Clone, Debug)]
pub struct ParsedZonedDateTime {
    pub(crate) date: ParsedDate,
    /// None time is START-OF-DAY
    pub(crate) time: Option<IsoTime>,
    /// Whether or not the string has a UTC designator (`Z`)
    ///
    /// Incompatible with having an offset (you can still have a offset-format timezone)
    pub(crate) has_utc_designator: bool,
    /// Whether or not to allow offsets rounded to the minute
    ///
    /// (Typically only needs to be set when parsing, can be false otherwise)
    pub(crate) match_minutes: bool,
    /// An optional offset string
    pub(crate) offset: Option<UtcOffset>,
    /// The time zone
    pub(crate) timezone: TimeZone,
}

impl ParsedZonedDateTime {
    /// Converts a UTF-8 encoded string into a `ParsedZonedDateTime`, using compiled data
    #[cfg(feature = "compiled_data")]
    pub fn from_utf8(source: &[u8]) -> TemporalResult<Self> {
        Self::from_utf8_with_provider(source, &*crate::builtins::TZ_PROVIDER)
    }

    /// Converts a UTF-8 encoded string into a `ParsedZonedDateTime`.
    pub fn from_utf8_with_provider(
        source: &[u8],
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        // Steps from the parse bits of of ToZonedDateTime

        // 3. Let matchBehaviour be match-minutes.
        let mut match_minutes = true;

        // b. Let result be ? ParseISODateTime(item, « TemporalDateTimeString[+Zoned] »).
        let parse_result = parsers::parse_zoned_date_time(source)?;

        // c. Let annotation be result.[[TimeZone]].[[TimeZoneAnnotation]].
        // d. Assert: annotation is not empty.
        // NOTE (nekevss): `parse_zoned_date_time` guarantees that this value exists.
        let annotation = parse_result.tz.temporal_unwrap()?;

        // e. Let timeZone be ? ToTemporalTimeZoneIdentifier(annotation).
        let time_zone = TimeZone::from_time_zone_record(annotation.tz, provider)?;

        // f. Let offsetString be result.[[TimeZone]].[[OffsetString]].
        let (offset, has_utc_designator) = match parse_result.offset {
            // g. If result.[[TimeZone]].[[Z]] is true, then
            // i. Set hasUTCDesignator to true.
            Some(UtcOffsetRecordOrZ::Z) => (None, true),
            Some(UtcOffsetRecordOrZ::Offset(offset)) => {
                if offset.second().is_some() {
                    // iii. If offsetParseResult contains more than one MinuteSecond Parse Node, set matchBehaviour to match-exactly.
                    match_minutes = false;
                }
                (Some(UtcOffset::from_ixdtf_record(offset)?), false)
            }
            None => (None, false),
        };

        // h. Let calendar be result.[[Calendar]].
        // i. If calendar is empty, set calendar to "iso8601".
        // j. Set calendar to ? CanonicalizeCalendar(calendar).
        let calendar = extract_kind(parse_result.calendar)?;

        let Some(date) = parse_result.date else {
            return Err(TemporalError::range().with_enum(ErrorMessage::ParserNeedsDate));
        };

        let time = parse_result
            .time
            .map(IsoTime::from_time_record)
            .transpose()?;

        Ok(Self {
            date: ParsedDate {
                record: date,
                calendar,
            },
            time,
            has_utc_designator,
            match_minutes,
            offset,
            timezone: time_zone,
        })
    }
}

#[test]
fn test_parsed_intermediate_invalid_dates() {
    // https://tc39.es/proposal-temporal/#sec-temporal-iso8601grammar-static-semantics-early-errors
    //
    // We need to error on invalid date/times *before* producing a ParsedFoo, since this is observable
    assert!(ParsedDate::from_utf8(b"2025-02-29").is_err());
    assert!(ParsedDate::from_utf8(b"2025-02-28").is_ok());
    assert!(ParsedDate::year_month_from_utf8(b"202513").is_err());
    assert!(ParsedDate::year_month_from_utf8(b"202512").is_ok());
    assert!(ParsedDate::month_day_from_utf8(b"1000").is_err());
    assert!(ParsedDate::month_day_from_utf8(b"1001").is_ok());
    assert!(ParsedDateTime::from_utf8(b"2025-02-28T25:69:69").is_err());
    assert!(ParsedDateTime::from_utf8(b"2025-02-28T23:59:59").is_ok());
    #[cfg(feature = "compiled_data")]
    assert!(ParsedZonedDateTime::from_utf8(b"2025-02-29T00:00:00Z[utc]").is_err());
    #[cfg(feature = "compiled_data")]
    assert!(ParsedZonedDateTime::from_utf8(b"2025-02-28T00:00:00Z[utc]").is_ok());
}
