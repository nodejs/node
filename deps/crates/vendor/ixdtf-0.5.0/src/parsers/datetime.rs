// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Parsing for `Date`, `DateTime`, and `MonthDay`.

use crate::{
    assert_syntax,
    parsers::{
        annotations,
        grammar::{is_annotation_open, is_date_time_separator, is_hyphen, is_utc_designator},
        records::{DateRecord, TimeRecord},
        time::parse_time_record,
        timezone, Cursor, IxdtfParseRecord,
    },
    ParseError, ParserResult,
};

use super::{
    grammar::is_ascii_sign,
    records::{Annotation, UtcOffsetRecordOrZ},
};

#[derive(Debug, Default, Clone)]
/// A `DateTime` Parse Node that contains the date, time, and offset info.
pub(crate) struct DateTimeRecord {
    /// Date
    pub(crate) date: Option<DateRecord>,
    /// Time
    pub(crate) time: Option<TimeRecord>,
    /// Tz Offset
    pub(crate) time_zone: Option<UtcOffsetRecordOrZ>,
}

/// This function handles parsing for [`AnnotatedDateTime`][datetime],
/// [`AnnotatedDateTimeTimeRequred`][time], and
/// [`TemporalInstantString.`][instant] according to the requirements
/// provided via Spec.
///
/// [datetime]: https://tc39.es/proposal-temporal/#prod-AnnotatedDateTime
/// [time]: https://tc39.es/proposal-temporal/#prod-AnnotatedDateTimeTimeRequired
/// [instant]: https://tc39.es/proposal-temporal/#prod-TemporalInstantString
pub(crate) fn parse_annotated_date_time<'a>(
    cursor: &mut Cursor<'a>,
    handler: impl FnMut(Annotation<'a>) -> Option<Annotation<'a>>,
) -> ParserResult<IxdtfParseRecord<'a>> {
    let date_time = parse_date_time(cursor)?;

    // Peek Annotation presence
    // Throw error if annotation does not exist and zoned is true, else return.
    if !cursor.check_or(false, is_annotation_open) {
        cursor.close()?;

        return Ok(IxdtfParseRecord {
            date: date_time.date,
            time: date_time.time,
            offset: date_time.time_zone,
            tz: None,
            calendar: None,
        });
    }

    let annotation_set = annotations::parse_annotation_set(cursor, handler)?;

    cursor.close()?;

    Ok(IxdtfParseRecord {
        date: date_time.date,
        time: date_time.time,
        offset: date_time.time_zone,
        tz: annotation_set.tz,
        calendar: annotation_set.calendar,
    })
}

/// Parses an AnnotatedMonthDay.
pub(crate) fn parse_annotated_month_day<'a>(
    cursor: &mut Cursor<'a>,
    handler: impl FnMut(Annotation<'a>) -> Option<Annotation<'a>>,
) -> ParserResult<IxdtfParseRecord<'a>> {
    let date = parse_month_day(cursor)?;

    if !cursor.check_or(false, is_annotation_open) {
        cursor.close()?;

        return Ok(IxdtfParseRecord {
            date: Some(date),
            time: None,
            offset: None,
            tz: None,
            calendar: None,
        });
    }

    let annotation_set = annotations::parse_annotation_set(cursor, handler)?;

    Ok(IxdtfParseRecord {
        date: Some(date),
        time: None,
        offset: None,
        tz: annotation_set.tz,
        calendar: annotation_set.calendar,
    })
}

/// Parse an annotated YearMonth
pub(crate) fn parse_annotated_year_month<'a>(
    cursor: &mut Cursor<'a>,
    handler: impl FnMut(Annotation<'a>) -> Option<Annotation<'a>>,
) -> ParserResult<IxdtfParseRecord<'a>> {
    let year = parse_date_year(cursor)?;
    cursor.advance_if(cursor.check_or(false, is_hyphen));
    let month = parse_date_month(cursor)?;

    let date = DateRecord {
        year,
        month,
        day: 1,
    };

    if !cursor.check_or(false, is_annotation_open) {
        cursor.close()?;

        return Ok(IxdtfParseRecord {
            date: Some(date),
            time: None,
            offset: None,
            tz: None,
            calendar: None,
        });
    }

    let annotation_set = annotations::parse_annotation_set(cursor, handler)?;

    Ok(IxdtfParseRecord {
        date: Some(date),
        time: None,
        offset: None,
        tz: annotation_set.tz,
        calendar: annotation_set.calendar,
    })
}

/// Parses a `DateTime` record.
fn parse_date_time(cursor: &mut Cursor) -> ParserResult<DateTimeRecord> {
    let date = parse_date(cursor)?;

    // If there is no `DateTimeSeparator`, return date early.
    if !cursor.check_or(false, is_date_time_separator) {
        return Ok(DateTimeRecord {
            date: Some(date),
            time: None,
            time_zone: None,
        });
    }

    cursor.advance();

    let time = parse_time_record(cursor)?;

    let time_zone = if cursor.check_or(false, |ch| is_ascii_sign(ch) || is_utc_designator(ch)) {
        Some(timezone::parse_date_time_utc_offset(cursor)?)
    } else {
        None
    };

    Ok(DateTimeRecord {
        date: Some(date),
        time: Some(time),
        time_zone,
    })
}

/// Parses `Date` record.
fn parse_date(cursor: &mut Cursor) -> ParserResult<DateRecord> {
    let year = parse_date_year(cursor)?;
    let hyphenated = cursor
        .check(is_hyphen)
        .ok_or(ParseError::abrupt_end("Date"))?;

    cursor.advance_if(hyphenated);

    let month = parse_date_month(cursor)?;

    let second_hyphen = cursor.check_or(false, is_hyphen);
    assert_syntax!(hyphenated == second_hyphen, DateSeparator);
    cursor.advance_if(second_hyphen);

    let day = parse_date_day(cursor)?;

    check_date_validity(year, month, day)?;

    Ok(DateRecord { year, month, day })
}

// ==== `MonthDay` parsing functions ====

/// Parses a `DateSpecMonthDay`
pub(crate) fn parse_month_day(cursor: &mut Cursor) -> ParserResult<DateRecord> {
    let hyphenated = cursor
        .check(is_hyphen)
        .ok_or(ParseError::abrupt_end("MonthDay"))?;
    cursor.advance_if(hyphenated);
    let balanced_hyphens = hyphenated
        && cursor
            .check(is_hyphen)
            .ok_or(ParseError::abrupt_end("MonthDay"))?;
    cursor.advance_if(balanced_hyphens);

    if hyphenated && !balanced_hyphens {
        return Err(ParseError::MonthDayHyphen);
    }

    let month = parse_date_month(cursor)?;

    cursor.advance_if(cursor.check_or(false, is_hyphen));

    let day = parse_date_day(cursor)?;

    assert_syntax!(cursor.check_or(true, is_annotation_open), InvalidEnd);

    Ok(DateRecord {
        year: 0,
        month,
        day,
    })
}

// ==== Unit Parsers ====

#[inline]
fn parse_date_year(cursor: &mut Cursor) -> ParserResult<i32> {
    if cursor.check_or(false, is_ascii_sign) {
        let sign = if cursor.next_or(ParseError::ImplAssert)? == b'+' {
            1
        } else {
            -1
        };

        let first = cursor.next_digit()?.ok_or(ParseError::DateExtendedYear)? as i32 * 100_000;
        let second = cursor.next_digit()?.ok_or(ParseError::DateExtendedYear)? as i32 * 10_000;
        let third = cursor.next_digit()?.ok_or(ParseError::DateExtendedYear)? as i32 * 1000;
        let fourth = cursor.next_digit()?.ok_or(ParseError::DateExtendedYear)? as i32 * 100;
        let fifth = cursor.next_digit()?.ok_or(ParseError::DateExtendedYear)? as i32 * 10;

        let year_value = first
            + second
            + third
            + fourth
            + fifth
            + cursor.next_digit()?.ok_or(ParseError::DateExtendedYear)? as i32;

        // 13.30.1 Static Semantics: Early Errors
        //
        // It is a Syntax Error if DateYear is "-000000" or "−000000" (U+2212 MINUS SIGN followed by 000000).
        if sign == -1 && year_value == 0 {
            return Err(ParseError::DateExtendedYear);
        }

        let year = sign * year_value;

        return Ok(year);
    }

    let first = cursor.next_digit()?.ok_or(ParseError::DateYear)? as i32 * 1000;
    let second = cursor.next_digit()?.ok_or(ParseError::DateYear)? as i32 * 100;
    let third = cursor.next_digit()?.ok_or(ParseError::DateYear)? as i32 * 10;
    let year_value =
        first + second + third + cursor.next_digit()?.ok_or(ParseError::DateYear)? as i32;

    Ok(year_value)
}

#[inline]
fn parse_date_month(cursor: &mut Cursor) -> ParserResult<u8> {
    let first = cursor.next_digit()?.ok_or(ParseError::DateMonth)?;
    let month_value = first * 10 + cursor.next_digit()?.ok_or(ParseError::DateMonth)?;
    if !(1..=12).contains(&month_value) {
        return Err(ParseError::InvalidMonthRange);
    }
    Ok(month_value)
}

#[inline]
fn parse_date_day(cursor: &mut Cursor) -> ParserResult<u8> {
    let first = cursor.next_digit()?.ok_or(ParseError::DateDay)?;
    let day_value = first * 10 + cursor.next_digit()?.ok_or(ParseError::DateDay)?;
    Ok(day_value)
}

#[inline]
fn check_date_validity(year: i32, month: u8, day: u8) -> ParserResult<()> {
    let Some(days_in_month) = days_in_month(year, month) else {
        // NOTE: This should never through due to check in `parse_date_month`
        return Err(ParseError::InvalidMonthRange);
    };
    if !(1..=days_in_month).contains(&day) {
        return Err(ParseError::InvalidDayRange);
    }
    Ok(())
}

/// Utilty to return the days in month, returns None if month is invalid
#[inline]
fn days_in_month(year: i32, month: u8) -> Option<u8> {
    match month {
        1 | 3 | 5 | 7 | 8 | 10 | 12 => Some(31),
        4 | 6 | 9 | 11 => Some(30),
        2 => Some(28 + u8::from(in_leap_year(year))),
        _ => None,
    }
}

/// Utility that returns whether a year is a leap year.
#[inline]
fn in_leap_year(year: i32) -> bool {
    if year % 4 != 0 {
        false
    } else if year % 4 == 0 && year % 100 != 0 {
        true
    } else if year % 100 == 0 && year % 400 != 0 {
        false
    } else {
        assert_eq!(year % 400, 0);
        true
    }
}
