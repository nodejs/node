use ixdtf::{
    encoding::Utf8,
    parsers::{IxdtfParser, TimeZoneParser},
    records::{TimeZoneRecord, UtcOffsetRecord, UtcOffsetRecordOrZ},
};

use crate::provider::TimeZoneProvider;
use crate::{builtins::time_zone::UtcOffset, TemporalError, TemporalResult, TimeZone};

use super::{parse_ixdtf, ParseVariant};

#[inline]
pub(crate) fn parse_allowed_timezone_formats(
    s: &str,
    provider: &impl TimeZoneProvider,
) -> Option<TimeZone> {
    let (offset, annotation) = if let Ok((offset, annotation)) =
        parse_ixdtf(s.as_bytes(), ParseVariant::DateTime).map(|r| (r.offset, r.tz))
    {
        (offset, annotation)
    } else if let Ok((offset, annotation)) = IxdtfParser::from_str(s)
        .parse_time()
        .map(|r| (r.offset, r.tz))
    {
        (offset, annotation)
    } else if let Ok((offset, annotation)) =
        parse_ixdtf(s.as_bytes(), ParseVariant::YearMonth).map(|r| (r.offset, r.tz))
    {
        (offset, annotation)
    } else if let Ok((offset, annotation)) =
        parse_ixdtf(s.as_bytes(), ParseVariant::MonthDay).map(|r| (r.offset, r.tz))
    {
        (offset, annotation)
    } else {
        return None;
    };

    if let Some(annotation) = annotation {
        return TimeZone::from_time_zone_record(annotation.tz, provider).ok();
    };

    if let Some(offset) = offset {
        match offset {
            UtcOffsetRecordOrZ::Z => return Some(TimeZone::utc_with_provider(provider)),
            UtcOffsetRecordOrZ::Offset(offset) => {
                let offset = match offset {
                    UtcOffsetRecord::MinutePrecision(offset) => offset,
                    _ => return None,
                };
                return Some(TimeZone::UtcOffset(UtcOffset::from_ixdtf_minute_record(
                    offset,
                )));
            }
        }
    }

    None
}

#[inline]
pub(crate) fn parse_identifier(source: &str) -> TemporalResult<TimeZoneRecord<'_, Utf8>> {
    let mut parser = TimeZoneParser::from_str(source);
    parser.parse_identifier().or(Err(
        TemporalError::range().with_message("Invalid TimeZone Identifier")
    ))
}
