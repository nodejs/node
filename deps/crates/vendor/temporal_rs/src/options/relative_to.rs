//! RelativeTo rounding option

use crate::builtins::core::zoned_date_time::interpret_isodatetime_offset;
use crate::builtins::core::{calendar::Calendar, time_zone::TimeZone, PlainDate, ZonedDateTime};
use crate::iso::{IsoDate, IsoTime};
use crate::options::{Disambiguation, OffsetDisambiguation, Overflow};
use crate::parsers::{parse_date_time, parse_zoned_date_time};
use crate::provider::TimeZoneProvider;
use crate::{TemporalResult, TemporalUnwrap};

use ixdtf::records::UtcOffsetRecordOrZ;

// ==== RelativeTo Object ====

#[derive(Debug, Clone)]
pub enum RelativeTo {
    PlainDate(PlainDate),
    ZonedDateTime(ZonedDateTime),
}

impl From<PlainDate> for RelativeTo {
    fn from(value: PlainDate) -> Self {
        Self::PlainDate(value)
    }
}

impl From<ZonedDateTime> for RelativeTo {
    fn from(value: ZonedDateTime) -> Self {
        Self::ZonedDateTime(value)
    }
}

impl RelativeTo {
    /// Attempts to parse a `ZonedDateTime` string falling back to a `PlainDate`
    /// if possible.
    ///
    /// If the fallback fails or either the `ZonedDateTime` or `PlainDate`
    /// is invalid, then an error is returned.
    pub fn try_from_str_with_provider(
        source: &str,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        // b. Let result be ? ParseISODateTime(value, « TemporalDateTimeString[+Zoned], TemporalDateTimeString[~Zoned] »).
        let bytes = source.as_bytes();
        let result = parse_date_time(bytes).or_else(|_| parse_zoned_date_time(bytes))?;

        let Some(annotation) = result.tz else {
            let date_record = result.date.temporal_unwrap()?;

            let calendar = result
                .calendar
                .map(Calendar::try_from_utf8)
                .transpose()?
                .unwrap_or_default();

            return Ok(PlainDate::try_new(
                date_record.year,
                date_record.month,
                date_record.day,
                calendar,
            )?
            .into());
        };

        // iv. Set matchBehaviour to match-minutes.
        let mut match_minutes = true;

        let time_zone = TimeZone::from_time_zone_record(annotation.tz, provider)?;

        let (offset_nanos, is_exact) = result
            .offset
            .map(|record| {
                let UtcOffsetRecordOrZ::Offset(offset) = record else {
                    return (None, true);
                };
                let hours_in_ns = i64::from(offset.hour()) * 3_600_000_000_000_i64;
                let minutes_in_ns = i64::from(offset.minute()) * 60_000_000_000_i64;
                let seconds_in_ns = i64::from(offset.second().unwrap_or(0)) * 1_000_000_000_i64;
                // 3. If offsetParseResult contains more than one MinuteSecond Parse Node, set matchBehaviour to match-exactly.
                if offset.second().is_some() {
                    match_minutes = false;
                }

                let ns = offset
                    .fraction()
                    .and_then(|x| x.to_nanoseconds())
                    .unwrap_or(0);
                (
                    Some(
                        (hours_in_ns + minutes_in_ns + seconds_in_ns + i64::from(ns))
                            * i64::from(offset.sign() as i8),
                    ),
                    false,
                )
            })
            .unwrap_or((None, false));

        let calendar = result
            .calendar
            .map(Calendar::try_from_utf8)
            .transpose()?
            .unwrap_or_default();

        let time = result.time.map(IsoTime::from_time_record).transpose()?;

        let date = result.date.temporal_unwrap()?;
        let iso = IsoDate::new_with_overflow(date.year, date.month, date.day, Overflow::Constrain)?;

        let epoch_ns = interpret_isodatetime_offset(
            iso,
            time,
            is_exact,
            offset_nanos,
            &time_zone,
            Disambiguation::Compatible,
            OffsetDisambiguation::Reject,
            match_minutes,
            provider,
        )?;

        Ok(ZonedDateTime::try_new_with_cached_offset(
            epoch_ns.ns.0,
            time_zone,
            calendar,
            epoch_ns.offset,
        )?
        .into())
    }
}

#[cfg(test)]
mod tests {
    #[cfg(feature = "compiled_data")]
    use super::*;
    #[cfg(feature = "compiled_data")]
    #[test]
    fn relativeto_offset_parse() {
        // Cases taken from intl402/Temporal/Duration/prototype/total/relativeto-sub-minute-offset

        let provider = &*crate::builtins::TZ_PROVIDER;
        let _ =
            RelativeTo::try_from_str_with_provider("1970-01-01T00:00-00:45:00[-00:45]", provider)
                .unwrap();
        // Rounded mm accepted
        let _ = RelativeTo::try_from_str_with_provider(
            "1970-01-01T00:00:00-00:45[Africa/Monrovia]",
            provider,
        )
        .unwrap();
        // unrounded mm::ss accepted
        let _ = RelativeTo::try_from_str_with_provider(
            "1970-01-01T00:00:00-00:44:30[Africa/Monrovia]",
            provider,
        )
        .unwrap();
        assert!(
            RelativeTo::try_from_str_with_provider(
                "1970-01-01T00:00:00-00:44:40[Africa/Monrovia]",
                provider
            )
            .is_err(),
            "Incorrect unrounded mm::ss rejected"
        );
        assert!(
            RelativeTo::try_from_str_with_provider(
                "1970-01-01T00:00:00-00:45:00[Africa/Monrovia]",
                provider
            )
            .is_err(),
            "Rounded mm::ss rejected"
        );
        assert!(
            RelativeTo::try_from_str_with_provider(
                "1970-01-01T00:00+00:44:30.123456789[+00:45]",
                provider
            )
            .is_err(),
            "Rounding not accepted between ISO offset and timezone"
        );
    }
}
