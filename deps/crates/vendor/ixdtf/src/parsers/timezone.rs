// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Parsing for Time Zone and Offset data.

use super::{
    grammar::{
        is_a_key_leading_char, is_annotation_close, is_annotation_key_value_separator,
        is_annotation_open, is_ascii_sign, is_critical_flag, is_time_separator, is_tz_char,
        is_tz_leading_char, is_tz_name_separator, is_utc_designator,
    },
    time::{parse_fraction, parse_hour, parse_minute_second},
    Cursor,
};
use crate::{
    assert_syntax,
    core::EncodingType,
    records::{
        FullPrecisionOffset, MinutePrecisionOffset, Sign, TimeZoneAnnotation, TimeZoneRecord,
        UtcOffsetRecord, UtcOffsetRecordOrZ,
    },
    ParseError, ParserResult,
};

// NOTE: critical field on time zones is captured but not handled.

// ==== Time Zone Annotation Parsing ====

/// We support two kinds of annotations here: annotations (e.g. `[u-ca=foo]`)
/// and "time zone annotations" (`[UTC]` or `[+05:30]`)
///
/// When parsing bracketed contents, we need to figure out which one we're dealing with.
///
/// This function returns a time zone annotation if we are dealing with a time zone,
/// otherwise it returns None (and the caller must handle non-tz annotations).
pub(crate) fn parse_ambiguous_tz_annotation<'a, T: EncodingType>(
    cursor: &mut Cursor<'a, T>,
) -> ParserResult<Option<TimeZoneAnnotation<'a, T>>> {
    // Peek position + 1 to check for critical flag.
    let mut current_peek = 1;
    let critical = cursor
        .peek_n(current_peek)?
        .map(is_critical_flag)
        .ok_or(ParseError::abrupt_end("AmbiguousAnnotation"))?;

    // Advance cursor if critical flag present.
    if critical {
        current_peek += 1;
    }

    let leading_char = cursor
        .peek_n(current_peek)?
        .ok_or(ParseError::abrupt_end("AmbiguousAnnotation"))?;

    // Ambigious start values when lowercase alpha that is shared between `TzLeadingChar` and `KeyLeadingChar`.
    if is_a_key_leading_char(leading_char) {
        let mut peek_pos = current_peek + 1;
        // Go through looking for `=`
        while let Some(ch) = cursor.peek_n(peek_pos)? {
            if is_annotation_key_value_separator(ch) {
                // We have an `=` sign, this is a non-tz annotation
                return Ok(None);
            } else if is_annotation_close(ch) {
                // We found a `]` without an `=`, this is a time zone
                let tz = parse_tz_annotation(cursor)?;
                return Ok(Some(tz));
            }

            peek_pos += 1;
        }
        Err(ParseError::abrupt_end("AmbiguousAnnotation"))
    } else {
        // Unambiguously not a non-tz annotation, try parsing a tz annotation
        let tz = parse_tz_annotation(cursor)?;
        Ok(Some(tz))
    }
}

fn parse_tz_annotation<'a, T: EncodingType>(
    cursor: &mut Cursor<'a, T>,
) -> ParserResult<TimeZoneAnnotation<'a, T>> {
    assert_syntax!(
        is_annotation_open(cursor.next_or(ParseError::AnnotationOpen)?),
        AnnotationOpen
    );

    let critical = cursor.check_or(false, is_critical_flag)?;
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
pub(crate) fn parse_time_zone<'a, T: EncodingType>(
    cursor: &mut Cursor<'a, T>,
) -> ParserResult<TimeZoneRecord<'a, T>> {
    let is_iana = cursor
        .check(is_tz_leading_char)?
        .ok_or(ParseError::abrupt_end("TimeZoneAnnotation"))?;
    let is_offset = cursor.check_or(false, is_ascii_sign)?;

    if is_iana {
        return Ok(TimeZoneRecord::Name(parse_tz_iana_name(cursor)?));
    } else if is_offset {
        let offset = parse_utc_offset_minute_precision_strict(cursor)?;
        return Ok(TimeZoneRecord::Offset(offset));
    }

    Err(ParseError::TzLeadingChar)
}

/// Parse a `TimeZoneIANAName` Parse Node
pub(crate) fn parse_tz_iana_name<'a, T: EncodingType>(
    cursor: &mut Cursor<'a, T>,
) -> ParserResult<&'a [T::CodeUnit]> {
    assert_syntax!(cursor.check_or(false, is_tz_leading_char)?, TzLeadingChar);
    let tz_name_start = cursor.pos();
    while let Some(potential_value_char) = cursor.next()? {
        if cursor.check_or(true, is_annotation_close)? {
            // Return the valid TimeZoneIANAName
            break;
        }

        if is_tz_name_separator(potential_value_char) {
            assert_syntax!(cursor.check_or(false, is_tz_char)?, IanaCharPostSeparator,);
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
pub(crate) fn parse_date_time_utc_offset<T: EncodingType>(
    cursor: &mut Cursor<T>,
) -> ParserResult<UtcOffsetRecordOrZ> {
    if cursor.check_or(false, is_utc_designator)? {
        cursor.advance();
        return Ok(UtcOffsetRecordOrZ::Z);
    }

    let utc_offset = parse_utc_offset(cursor)?;
    Ok(UtcOffsetRecordOrZ::Offset(utc_offset))
}

/// Parse a potentially full precision `UtcOffset`
pub(crate) fn parse_utc_offset<T: EncodingType>(
    cursor: &mut Cursor<T>,
) -> ParserResult<UtcOffsetRecord> {
    let (minute_precision_offset, separated) = parse_utc_offset_minute_precision(cursor)?;

    // If `UtcOffsetWithSubMinuteComponents`, continue parsing.
    if !cursor.check_or(false, |ch| ch.is_ascii_digit() || is_time_separator(ch))? {
        return Ok(UtcOffsetRecord::MinutePrecision(minute_precision_offset));
    }

    if Some(separated) != cursor.check(is_time_separator)? {
        return Err(ParseError::UtcTimeSeparator);
    }
    cursor.advance_if(cursor.check_or(false, is_time_separator)?);

    let second = parse_minute_second(cursor, false)?;

    let fraction = parse_fraction(cursor)?;

    Ok(UtcOffsetRecord::FullPrecisionOffset(FullPrecisionOffset {
        minute_precision_offset,
        second,
        fraction,
    }))
}

pub(crate) fn parse_utc_offset_minute_precision_strict<T: EncodingType>(
    cursor: &mut Cursor<T>,
) -> ParserResult<MinutePrecisionOffset> {
    let (offset, _) = parse_utc_offset_minute_precision(cursor)?;
    if cursor.check_or(false, |ch| is_time_separator(ch) || ch.is_ascii_digit())? {
        return Err(ParseError::InvalidMinutePrecisionOffset);
    }
    Ok(offset)
}

/// Parse an `UtcOffsetMinutePrecision` node
///
/// Returns the offset and whether the utc parsing includes a minute separator.
pub(crate) fn parse_utc_offset_minute_precision<T: EncodingType>(
    cursor: &mut Cursor<T>,
) -> ParserResult<(MinutePrecisionOffset, bool)> {
    // https://www.rfc-editor.org/rfc/rfc3339#section-5.6
    let sign = cursor.next_or(ParseError::abrupt_end("time-numoffset"))?;
    if !is_ascii_sign(sign) {
        return Err(ParseError::OffsetNeedsSign);
    }
    let sign = Sign::from(sign == b'+');

    let hour = parse_hour(cursor)?;

    // If at the end of the utc, then return.
    if !cursor.check_or(false, |ch| ch.is_ascii_digit() || is_time_separator(ch))? {
        let offset = MinutePrecisionOffset {
            sign,
            hour,
            minute: 0,
        };
        return Ok((offset, false));
    }
    // Advance cursor beyond any TimeSeparator
    let separated = cursor.check_or(false, is_time_separator)?;
    cursor.advance_if(separated);

    let minute = parse_minute_second(cursor, false)?;

    Ok((MinutePrecisionOffset { sign, hour, minute }, separated))
}
