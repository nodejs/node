// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Parsing for Time Zone and Offset data.

use super::{
    grammar::{
        is_a_key_char, is_a_key_leading_char, is_annotation_close,
        is_annotation_key_value_separator, is_annotation_open, is_ascii_sign, is_critical_flag,
        is_time_separator, is_tz_char, is_tz_leading_char, is_tz_name_separator, is_utc_designator,
    },
    records::{
        FullPrecisionOffset, MinutePrecisionOffset, Sign, TimeZoneAnnotation, TimeZoneRecord,
        UtcOffsetRecord, UtcOffsetRecordOrZ,
    },
    time::{parse_fraction, parse_hour, parse_minute_second},
    Cursor,
};
use crate::{assert_syntax, ParseError, ParserResult};

// NOTE: critical field on time zones is captured but not handled.

// ==== Time Zone Annotation Parsing ====

pub(crate) fn parse_ambiguous_tz_annotation<'a>(
    cursor: &mut Cursor<'a>,
) -> ParserResult<Option<TimeZoneAnnotation<'a>>> {
    // Peek position + 1 to check for critical flag.
    let mut current_peek = 1;
    let critical = cursor
        .peek_n(current_peek)
        .map(is_critical_flag)
        .ok_or(ParseError::abrupt_end("AmbiguousAnnotation"))?;

    // Advance cursor if critical flag present.
    if critical {
        current_peek += 1;
    }

    let leading_char = cursor
        .peek_n(current_peek)
        .ok_or(ParseError::abrupt_end("AmbiguousAnnotation"))?;

    if is_tz_leading_char(leading_char) || is_ascii_sign(leading_char) {
        // Ambigious start values when lowercase alpha that is shared between `TzLeadingChar` and `KeyLeadingChar`.
        if is_a_key_leading_char(leading_char) {
            let mut peek_pos = current_peek + 1;
            while let Some(ch) = cursor.peek_n(peek_pos) {
                if is_tz_name_separator(ch) || (is_tz_char(ch) && !is_a_key_char(ch)) {
                    let tz = parse_tz_annotation(cursor)?;
                    return Ok(Some(tz));
                } else if is_annotation_key_value_separator(ch)
                    || (is_a_key_char(ch) && !is_tz_char(ch))
                {
                    return Ok(None);
                } else if is_annotation_close(ch) {
                    return Err(ParseError::InvalidAnnotation);
                }

                peek_pos += 1;
            }
            return Err(ParseError::abrupt_end("AmbiguousAnnotation"));
        }
        let tz = parse_tz_annotation(cursor)?;
        return Ok(Some(tz));
    }

    if is_a_key_leading_char(leading_char) {
        return Ok(None);
    };

    Err(ParseError::AnnotationChar)
}

fn parse_tz_annotation<'a>(cursor: &mut Cursor<'a>) -> ParserResult<TimeZoneAnnotation<'a>> {
    assert_syntax!(
        is_annotation_open(cursor.next_or(ParseError::AnnotationOpen)?),
        AnnotationOpen
    );

    let critical = cursor.check_or(false, is_critical_flag);
    cursor.advance_if(critical);

    let tz = parse_time_zone(cursor)?;

    assert_syntax!(
        is_annotation_close(cursor.next_or(ParseError::AnnotationClose)?),
        AnnotationClose
    );

    Ok(TimeZoneAnnotation { critical, tz })
}

/// Parses the [`TimeZoneIdentifier`][tz] node.
///
/// [tz]: https://tc39.es/proposal-temporal/#prod-TimeZoneIdentifier
pub(crate) fn parse_time_zone<'a>(cursor: &mut Cursor<'a>) -> ParserResult<TimeZoneRecord<'a>> {
    let is_iana = cursor
        .check(is_tz_leading_char)
        .ok_or(ParseError::abrupt_end("TimeZoneAnnotation"))?;
    let is_offset = cursor.check_or(false, is_ascii_sign);

    if is_iana {
        return Ok(TimeZoneRecord::Name(parse_tz_iana_name(cursor)?));
    } else if is_offset {
        let (offset, _) = parse_utc_offset_minute_precision(cursor)?;
        return Ok(TimeZoneRecord::Offset(offset));
    }

    Err(ParseError::TzLeadingChar)
}

/// Parse a `TimeZoneIANAName` Parse Node
pub(crate) fn parse_tz_iana_name<'a>(cursor: &mut Cursor<'a>) -> ParserResult<&'a [u8]> {
    assert_syntax!(cursor.check_or(false, is_tz_leading_char), TzLeadingChar);
    let tz_name_start = cursor.pos();
    while let Some(potential_value_char) = cursor.next() {
        if cursor.check_or(true, is_annotation_close) {
            // Return the valid TimeZoneIANAName
            break;
        }

        if is_tz_name_separator(potential_value_char) {
            assert_syntax!(cursor.check_or(false, is_tz_char), IanaCharPostSeparator,);
            continue;
        }

        assert_syntax!(is_tz_char(potential_value_char), IanaChar,);
    }

    cursor
        .slice(tz_name_start, cursor.pos())
        .ok_or(ParseError::ImplAssert)
}

// ==== Utc Offset Parsing ====

/// Parses a potentially full precision UTC offset or Z
pub(crate) fn parse_date_time_utc_offset(cursor: &mut Cursor) -> ParserResult<UtcOffsetRecordOrZ> {
    if cursor.check_or(false, is_utc_designator) {
        cursor.advance();
        return Ok(UtcOffsetRecordOrZ::Z);
    }

    let utc_offset = parse_utc_offset(cursor)?;
    Ok(UtcOffsetRecordOrZ::Offset(utc_offset))
}

/// Parse a potentially full precision `UtcOffset`
pub(crate) fn parse_utc_offset(cursor: &mut Cursor) -> ParserResult<UtcOffsetRecord> {
    let (minute_precision_offset, separated) = parse_utc_offset_minute_precision(cursor)?;

    // If `UtcOffsetWithSubMinuteComponents`, continue parsing.
    if !cursor.check_or(false, |ch| ch.is_ascii_digit() || is_time_separator(ch)) {
        return Ok(UtcOffsetRecord::MinutePrecision(minute_precision_offset));
    }

    if Some(separated) != cursor.check(is_time_separator) {
        return Err(ParseError::UtcTimeSeparator);
    }
    cursor.advance_if(cursor.check_or(false, is_time_separator));

    let second = parse_minute_second(cursor, false)?;

    let fraction = parse_fraction(cursor)?;

    Ok(UtcOffsetRecord::FullPrecisionOffset(FullPrecisionOffset {
        minute_precision_offset,
        second,
        fraction,
    }))
}

/// Parse an `UtcOffsetMinutePrecision` node
///
/// Returns the offset and whether the utc parsing includes a minute.
pub(crate) fn parse_utc_offset_minute_precision(
    cursor: &mut Cursor,
) -> ParserResult<(MinutePrecisionOffset, bool)> {
    let sign = if cursor.check_or(false, is_ascii_sign) {
        let sign = cursor.next_or(ParseError::ImplAssert)?;
        Sign::from(sign == b'+')
    } else {
        Sign::Positive
    };
    let hour = parse_hour(cursor)?;

    // If at the end of the utc, then return.
    if !cursor.check_or(false, |ch| ch.is_ascii_digit() || is_time_separator(ch)) {
        let offset = MinutePrecisionOffset {
            sign,
            hour,
            minute: 0,
        };
        return Ok((offset, false));
    }
    // Advance cursor beyond any TimeSeparator
    let separated = cursor.check_or(false, is_time_separator);
    cursor.advance_if(separated);

    let minute = parse_minute_second(cursor, false)?;

    Ok((MinutePrecisionOffset { sign, hour, minute }, separated))
}
