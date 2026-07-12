// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Parsing of Time Values

use core::num::NonZeroU8;

use crate::{
    assert_syntax,
    core::EncodingType,
    parsers::{
        datetime::{parse_month_day, parse_year_month},
        grammar::{
            is_annotation_open, is_decimal_separator, is_time_designator, is_time_separator,
            is_utc_designator,
        },
        Cursor,
    },
    records::{Annotation, Fraction, IxdtfParseRecord, TimeRecord},
    ParseError, ParserResult,
};

use super::{annotations, grammar::is_ascii_sign, timezone};

/// Parse annotated time record is silently fallible returning None in the case that the
/// value does not align
pub(crate) fn parse_annotated_time_record<'a, T: EncodingType>(
    cursor: &mut Cursor<'a, T>,
    handler: impl FnMut(Annotation<'a, T>) -> Option<Annotation<'a, T>>,
) -> ParserResult<IxdtfParseRecord<'a, T>> {
    let start = cursor.pos();
    let designator = cursor.check_or(false, is_time_designator)?;
    cursor.advance_if(designator);

    let time = parse_time_record(cursor)?;

    // If Time was successfully parsed, assume from this point that this IS a
    // valid AnnotatedTimeRecord.

    let offset = if cursor.check_or(false, |ch| is_ascii_sign(ch) || is_utc_designator(ch))? {
        Some(timezone::parse_date_time_utc_offset(cursor)?)
    } else {
        None
    };

    // Check if annotations exist.
    if !cursor.check_or(false, is_annotation_open)? {
        cursor.close()?;
        check_time_ambiguity(cursor, start)?;

        return Ok(IxdtfParseRecord {
            date: None,
            time: Some(time),
            offset,
            tz: None,
            calendar: None,
        });
    }

    check_time_ambiguity(cursor, start)?;
    let annotations = annotations::parse_annotation_set(cursor, handler)?;

    cursor.close()?;

    Ok(IxdtfParseRecord {
        date: None,
        time: Some(time),
        offset,
        tz: annotations.tz,
        calendar: annotations.calendar,
    })
}

#[inline]
fn check_time_ambiguity<T: EncodingType>(cursor: &mut Cursor<T>, start: usize) -> ParserResult<()> {
    let current_loc = cursor.pos();
    // It is a Syntax Error if ParseText(Time DateTimeUTCOffset[~Z], DateSpecMonthDay) is a Parse Node.
    cursor.set_position(start);
    if parse_month_day(cursor).is_ok() {
        return Err(ParseError::AmbiguousTimeMonthDay);
    }
    // It is a Syntax Error if ParseText(Time DateTimeUTCOffset[~Z], DateSpecYearMonth) is a Parse Node.
    cursor.set_position(start);
    if parse_year_month(cursor).is_ok() {
        return Err(ParseError::AmbiguousTimeYearMonth);
    }
    cursor.set_position(current_loc);
    Ok(())
}

/// Parse `TimeRecord`
pub(crate) fn parse_time_record<T: EncodingType>(
    cursor: &mut Cursor<T>,
) -> ParserResult<TimeRecord> {
    let hour = parse_hour(cursor)?;

    if !cursor.check_or(false, |ch| is_time_separator(ch) || ch.is_ascii_digit())? {
        return Ok(TimeRecord {
            hour,
            minute: 0,
            second: 0,
            fraction: None,
        });
    }

    let separator_present = cursor.check_or(false, is_time_separator)?;
    cursor.advance_if(separator_present);

    let minute = parse_minute_second(cursor, false)?;

    if !cursor.check_or(false, |ch| is_time_separator(ch) || ch.is_ascii_digit())? {
        return Ok(TimeRecord {
            hour,
            minute,
            second: 0,
            fraction: None,
        });
    }

    let second_separator = cursor.check_or(false, is_time_separator)?;
    assert_syntax!(separator_present == second_separator, TimeSeparator);
    cursor.advance_if(second_separator);

    let second = parse_minute_second(cursor, true)?;

    let fraction = parse_fraction(cursor)?;

    Ok(TimeRecord {
        hour,
        minute,
        second,
        fraction,
    })
}

/// Parse an hour value.
#[inline]
pub(crate) fn parse_hour<T: EncodingType>(cursor: &mut Cursor<T>) -> ParserResult<u8> {
    let first = cursor.next_digit()?.ok_or(ParseError::TimeHour)?;
    let hour_value = first * 10 + cursor.next_digit()?.ok_or(ParseError::TimeHour)?;
    if !(0..=23).contains(&hour_value) {
        return Err(ParseError::TimeHour);
    }
    Ok(hour_value)
}

/// Parses `MinuteSecond` value.
#[inline]
pub(crate) fn parse_minute_second<T: EncodingType>(
    cursor: &mut Cursor<T>,
    is_leap_second_valid: bool,
) -> ParserResult<u8> {
    let (valid_range, err) = if is_leap_second_valid {
        (0..=60, ParseError::TimeSecond)
    } else {
        (0..=59, ParseError::TimeMinuteSecond)
    };
    let first = cursor.next_digit()?.ok_or(err)?;
    let min_sec_value = first * 10 + cursor.next_digit()?.ok_or(err)?;
    if !valid_range.contains(&min_sec_value) {
        return Err(err);
    }
    Ok(min_sec_value)
}

/// Parse a `Fraction` value
///
/// This is primarily used in ISO8601 to add percision past
/// a second.
#[inline]
pub(crate) fn parse_fraction<T: EncodingType>(
    cursor: &mut Cursor<T>,
) -> ParserResult<Option<Fraction>> {
    // Assert that the first char provided is a decimal separator.
    if !cursor.check_or(false, is_decimal_separator)? {
        return Ok(None);
    }
    cursor.next_or(ParseError::FractionPart)?;

    let mut value = 0;
    let mut digits: u8 = 0;
    while cursor.check_or(false, |ch| ch.is_ascii_digit())? {
        let next_value = u64::from(cursor.next_digit()?.ok_or(ParseError::ImplAssert)?);
        if digits < 18 {
            value = value * 10 + next_value;
        }
        digits = digits.saturating_add(1);
    }

    let digits = NonZeroU8::new(digits).ok_or(ParseError::FractionPart)?;

    Ok(Some(Fraction { digits, value }))
}
