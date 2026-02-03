use crate::error::ffi::TemporalError;
use crate::instant::ffi::I128Nanoseconds;
use crate::provider::ffi::Provider;
use temporal_rs::options::RelativeTo;

#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
pub mod ffi {
    use crate::calendar::ffi::AnyCalendarKind;
    use crate::calendar::ffi::Calendar;
    use crate::duration::ffi::Duration;
    use crate::error::ffi::TemporalError;
    use crate::plain_date::ffi::{PartialDate, PlainDate};
    use crate::plain_date_time::ffi::PlainDateTime;
    use crate::plain_time::ffi::{PartialTime, PlainTime};
    use alloc::boxed::Box;

    use crate::provider::ffi::Provider;

    use crate::instant::ffi::I128Nanoseconds;
    use crate::instant::ffi::Instant;
    use crate::options::ffi::{
        ArithmeticOverflow, DifferenceSettings, Disambiguation, DisplayCalendar, DisplayOffset,
        DisplayTimeZone, OffsetDisambiguation, RoundingOptions, ToStringRoundingOptions,
        TransitionDirection,
    };

    use crate::time_zone::ffi::TimeZone;

    use alloc::string::String;
    use core::fmt::Write;

    use diplomat_runtime::DiplomatOption;
    use diplomat_runtime::DiplomatStrSlice;
    use diplomat_runtime::DiplomatWrite;

    pub struct PartialZonedDateTime<'a> {
        pub date: PartialDate<'a>,
        pub time: PartialTime,
        pub offset: DiplomatOption<DiplomatStrSlice<'a>>,
        pub timezone: DiplomatOption<TimeZone>,
    }

    pub struct RelativeTo<'a> {
        pub date: Option<&'a PlainDate>,
        pub zoned: Option<&'a ZonedDateTime>,
    }

    /// GetTemporalRelativeToOption can create fresh PlainDate/ZonedDateTimes by parsing them,
    /// we need a way to produce that result.
    #[diplomat::out]
    pub struct OwnedRelativeTo {
        pub date: Option<Box<PlainDate>>,
        pub zoned: Option<Box<ZonedDateTime>>,
    }

    impl OwnedRelativeTo {
        #[cfg(feature = "compiled_data")]
        pub fn from_utf8(s: &DiplomatStr) -> Result<Self, TemporalError> {
            Self::from_utf8_with_provider(s, &Provider::compiled())
        }
        pub fn from_utf8_with_provider<'p>(
            s: &DiplomatStr,
            p: &Provider<'p>,
        ) -> Result<Self, TemporalError> {
            // TODO(#275) This should not need to check
            let s = core::str::from_utf8(s).map_err(|_| temporal_rs::TemporalError::range())?;

            with_provider!(p, |p| super::RelativeTo::try_from_str_with_provider(s, p))
                .map(Into::into)
                .map_err(Into::<TemporalError>::into)
        }

        #[cfg(feature = "compiled_data")]
        pub fn from_utf16(s: &DiplomatStr16) -> Result<Self, TemporalError> {
            Self::from_utf16_with_provider(s, &Provider::compiled())
        }
        pub fn from_utf16_with_provider<'p>(
            s: &DiplomatStr16,
            p: &Provider<'p>,
        ) -> Result<Self, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            with_provider!(p, |p| super::RelativeTo::try_from_str_with_provider(&s, p))
                .map(Into::into)
                .map_err(Into::<TemporalError>::into)
        }

        pub fn empty() -> Self {
            Self {
                date: None,
                zoned: None,
            }
        }
    }

    #[diplomat::opaque]
    pub struct ParsedZonedDateTime(temporal_rs::parsed_intermediates::ParsedZonedDateTime);

    impl ParsedZonedDateTime {
        #[cfg(feature = "compiled_data")]
        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            Self::from_utf8_with_provider(s, &Provider::compiled())
        }
        pub fn from_utf8_with_provider<'p>(
            s: &DiplomatStr,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            with_provider!(p, |p| {
                temporal_rs::parsed_intermediates::ParsedZonedDateTime::from_utf8_with_provider(
                    s, p,
                )
            })
            .map(|x| Box::new(ParsedZonedDateTime(x)))
            .map_err(Into::<TemporalError>::into)
        }

        #[cfg(feature = "compiled_data")]
        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            Self::from_utf16_with_provider(s, &Provider::compiled())
        }
        pub fn from_utf16_with_provider<'p>(
            s: &DiplomatStr16,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;

            with_provider!(p, |p| {
                temporal_rs::parsed_intermediates::ParsedZonedDateTime::from_utf8_with_provider(
                    s.as_bytes(),
                    p,
                )
            })
            .map(|x| Box::new(ParsedZonedDateTime(x)))
            .map_err(Into::<TemporalError>::into)
        }
    }

    #[diplomat::opaque]
    pub struct ZonedDateTime(pub(crate) temporal_rs::ZonedDateTime);

    impl ZonedDateTime {
        #[cfg(feature = "compiled_data")]
        pub fn try_new(
            nanosecond: I128Nanoseconds,
            calendar: AnyCalendarKind,
            time_zone: TimeZone,
        ) -> Result<Box<Self>, TemporalError> {
            Self::try_new_with_provider(nanosecond, calendar, time_zone, &Provider::compiled())
        }
        pub fn try_new_with_provider<'p>(
            nanosecond: I128Nanoseconds,
            calendar: AnyCalendarKind,
            time_zone: TimeZone,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            with_provider!(p, |p| temporal_rs::ZonedDateTime::try_new_with_provider(
                nanosecond.into(),
                time_zone.into(),
                temporal_rs::Calendar::new(calendar.into()),
                p
            ))
            .map(|x| Box::new(ZonedDateTime(x)))
            .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn from_partial(
            partial: PartialZonedDateTime,
            overflow: Option<ArithmeticOverflow>,
            disambiguation: Option<Disambiguation>,
            offset_option: Option<OffsetDisambiguation>,
        ) -> Result<Box<Self>, TemporalError> {
            Self::from_partial_with_provider(
                partial,
                overflow,
                disambiguation,
                offset_option,
                &Provider::compiled(),
            )
        }
        pub fn from_partial_with_provider<'p>(
            partial: PartialZonedDateTime,
            overflow: Option<ArithmeticOverflow>,
            disambiguation: Option<Disambiguation>,
            offset_option: Option<OffsetDisambiguation>,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            with_provider!(p, |p| {
                temporal_rs::ZonedDateTime::from_partial_with_provider(
                    partial.try_into()?,
                    overflow.map(Into::into),
                    disambiguation.map(Into::into),
                    offset_option.map(Into::into),
                    p,
                )
            })
            .map(|x| Box::new(ZonedDateTime(x)))
            .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn from_parsed(
            parsed: &ParsedZonedDateTime,
            disambiguation: Disambiguation,
            offset_option: OffsetDisambiguation,
        ) -> Result<Box<Self>, TemporalError> {
            Self::from_parsed_with_provider(
                parsed,
                disambiguation,
                offset_option,
                &Provider::compiled(),
            )
        }
        pub fn from_parsed_with_provider<'p>(
            parsed: &ParsedZonedDateTime,
            disambiguation: Disambiguation,
            offset_option: OffsetDisambiguation,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            with_provider!(
                p,
                |p| temporal_rs::ZonedDateTime::from_parsed_with_provider(
                    parsed.0.clone(),
                    disambiguation.into(),
                    offset_option.into(),
                    p
                )
            )
            .map(|x| Box::new(ZonedDateTime(x)))
            .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn from_utf8(
            s: &DiplomatStr,
            disambiguation: Disambiguation,
            offset_disambiguation: OffsetDisambiguation,
        ) -> Result<Box<Self>, TemporalError> {
            Self::from_utf8_with_provider(
                s,
                disambiguation,
                offset_disambiguation,
                &Provider::compiled(),
            )
        }
        pub fn from_utf8_with_provider<'p>(
            s: &DiplomatStr,
            disambiguation: Disambiguation,
            offset_disambiguation: OffsetDisambiguation,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to check
            with_provider!(p, |p| temporal_rs::ZonedDateTime::from_utf8_with_provider(
                s,
                disambiguation.into(),
                offset_disambiguation.into(),
                p
            ))
            .map(|c| Box::new(Self(c)))
            .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn from_utf16(
            s: &DiplomatStr16,
            disambiguation: Disambiguation,
            offset_disambiguation: OffsetDisambiguation,
        ) -> Result<Box<Self>, TemporalError> {
            Self::from_utf16_with_provider(
                s,
                disambiguation,
                offset_disambiguation,
                &Provider::compiled(),
            )
        }
        pub fn from_utf16_with_provider<'p>(
            s: &DiplomatStr16,
            disambiguation: Disambiguation,
            offset_disambiguation: OffsetDisambiguation,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            with_provider!(p, |p| temporal_rs::ZonedDateTime::from_utf8_with_provider(
                s.as_bytes(),
                disambiguation.into(),
                offset_disambiguation.into(),
                p
            ))
            .map(|c| Box::new(Self(c)))
            .map_err(Into::into)
        }

        pub fn epoch_milliseconds(&self) -> i64 {
            self.0.epoch_milliseconds()
        }

        #[cfg(feature = "compiled_data")]
        pub fn from_epoch_milliseconds(ms: i64, tz: TimeZone) -> Result<Box<Self>, TemporalError> {
            Self::from_epoch_milliseconds_with_provider(ms, tz, &Provider::compiled())
        }
        pub fn from_epoch_milliseconds_with_provider<'p>(
            ms: i64,
            tz: TimeZone,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            super::zdt_from_epoch_ms_with_provider(ms, tz.into(), p).map(|c| Box::new(Self(c)))
        }

        #[cfg(feature = "compiled_data")]
        pub fn from_epoch_nanoseconds(
            ns: I128Nanoseconds,
            tz: TimeZone,
        ) -> Result<Box<Self>, TemporalError> {
            Self::from_epoch_nanoseconds_with_provider(ns, tz, &Provider::compiled())
        }
        pub fn from_epoch_nanoseconds_with_provider<'p>(
            ns: I128Nanoseconds,
            tz: TimeZone,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            let zdt = crate::zoned_date_time::zdt_from_epoch_ns_with_provider(ns, tz.into(), p)?;
            Ok(Box::new(Self(zdt)))
        }

        pub fn epoch_nanoseconds(&self) -> I128Nanoseconds {
            self.0.epoch_nanoseconds().as_i128().into()
        }

        pub fn offset_nanoseconds(&self) -> i64 {
            self.0.offset_nanoseconds()
        }

        pub fn to_instant(&self) -> Box<Instant> {
            Box::new(Instant(self.0.to_instant()))
        }

        #[cfg(feature = "compiled_data")]
        pub fn with(
            &self,
            partial: PartialZonedDateTime,
            disambiguation: Option<Disambiguation>,
            offset_option: Option<OffsetDisambiguation>,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.with_with_provider(
                partial,
                disambiguation,
                offset_option,
                overflow,
                &Provider::compiled(),
            )
        }
        pub fn with_with_provider<'p>(
            &self,
            partial: PartialZonedDateTime,
            disambiguation: Option<Disambiguation>,
            offset_option: Option<OffsetDisambiguation>,
            overflow: Option<ArithmeticOverflow>,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            with_provider!(p, |p| self.0.with_with_provider(
                partial.try_into()?,
                disambiguation.map(Into::into),
                offset_option.map(Into::into),
                overflow.map(Into::into),
                p
            ))
            .map(|x| Box::new(ZonedDateTime(x)))
            .map_err(Into::into)
        }

        #[cfg(feature = "compiled_data")]
        pub fn with_timezone(&self, zone: TimeZone) -> Result<Box<Self>, TemporalError> {
            self.with_timezone_with_provider(zone, &Provider::compiled())
        }
        pub fn with_timezone_with_provider<'p>(
            &self,
            zone: TimeZone,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            with_provider!(p, |p| self.0.with_time_zone_with_provider(zone.into(), p))
                .map(|x| Box::new(ZonedDateTime(x)))
                .map_err(Into::into)
        }

        pub fn timezone(&self) -> TimeZone {
            TimeZone::from(*self.0.time_zone())
        }

        pub fn compare_instant(&self, other: &Self) -> core::cmp::Ordering {
            self.0.compare_instant(&other.0)
        }

        #[cfg(feature = "compiled_data")]
        pub fn equals(&self, other: &Self) -> bool {
            self.equals_with_provider(other, &Provider::compiled())
                .unwrap_or(false)
        }
        pub fn equals_with_provider<'p>(
            &self,
            other: &Self,
            p: &Provider<'p>,
        ) -> Result<bool, TemporalError> {
            with_provider!(p, |p| self.0.equals_with_provider(&other.0, p)).map_err(Into::into)
        }

        pub fn offset(&self, write: &mut DiplomatWrite) -> Result<(), TemporalError> {
            let string = self.0.offset();
            // throw away the error, this should always succeed
            let _ = write.write_str(&string);
            Ok(())
        }

        #[cfg(feature = "compiled_data")]
        pub fn start_of_day(&self) -> Result<Box<ZonedDateTime>, TemporalError> {
            self.start_of_day_with_provider(&Provider::compiled())
        }
        pub fn start_of_day_with_provider<'p>(
            &self,
            p: &Provider<'p>,
        ) -> Result<Box<ZonedDateTime>, TemporalError> {
            with_provider!(p, |p| self.0.start_of_day_with_provider(p))
                .map(|x| Box::new(ZonedDateTime(x)))
                .map_err(Into::into)
        }

        #[cfg(feature = "compiled_data")]
        pub fn get_time_zone_transition(
            &self,
            direction: TransitionDirection,
        ) -> Result<Option<Box<Self>>, TemporalError> {
            self.get_time_zone_transition_with_provider(direction, &Provider::compiled())
        }
        pub fn get_time_zone_transition_with_provider<'p>(
            &self,
            direction: TransitionDirection,
            p: &Provider<'p>,
        ) -> Result<Option<Box<Self>>, TemporalError> {
            with_provider!(p, |p| self
                .0
                .get_time_zone_transition_with_provider(direction.into(), p))
            .map(|x| x.map(|y| Box::new(ZonedDateTime(y))))
            .map_err(Into::into)
        }

        #[cfg(feature = "compiled_data")]
        pub fn hours_in_day(&self) -> Result<f64, TemporalError> {
            self.hours_in_day_with_provider(&Provider::compiled())
        }
        pub fn hours_in_day_with_provider<'p>(
            &self,
            p: &Provider<'p>,
        ) -> Result<f64, TemporalError> {
            with_provider!(p, |p| self.0.hours_in_day_with_provider(p)).map_err(Into::into)
        }

        pub fn to_plain_datetime(&self) -> Box<PlainDateTime> {
            Box::new(PlainDateTime(self.0.to_plain_date_time()))
        }

        pub fn to_plain_date(&self) -> Box<PlainDate> {
            Box::new(PlainDate(self.0.to_plain_date()))
        }

        pub fn to_plain_time(&self) -> Box<PlainTime> {
            Box::new(PlainTime(self.0.to_plain_time()))
        }

        #[cfg(feature = "compiled_data")]
        pub fn to_ixdtf_string(
            &self,
            display_offset: DisplayOffset,
            display_timezone: DisplayTimeZone,
            display_calendar: DisplayCalendar,
            options: ToStringRoundingOptions,
            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            self.to_ixdtf_string_with_provider(
                display_offset,
                display_timezone,
                display_calendar,
                options,
                &Provider::compiled(),
                write,
            )
        }
        pub fn to_ixdtf_string_with_provider<'p>(
            &self,
            display_offset: DisplayOffset,
            display_timezone: DisplayTimeZone,
            display_calendar: DisplayCalendar,
            options: ToStringRoundingOptions,
            p: &Provider<'p>,
            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            // TODO this double-allocates, an API returning a Writeable or impl Write would be better
            let string = with_provider!(p, |p| self.0.to_ixdtf_string_with_provider(
                display_offset.into(),
                display_timezone.into(),
                display_calendar.into(),
                options.into(),
                p
            )?);
            // throw away the error, this should always succeed
            let _ = write.write_str(&string);
            Ok(())
        }

        // Same as PlainDateTime (non-getters)
        pub fn with_calendar(&self, calendar: AnyCalendarKind) -> Box<Self> {
            Box::new(ZonedDateTime(
                self.0
                    .with_calendar(temporal_rs::Calendar::new(calendar.into())),
            ))
        }
        #[cfg(feature = "compiled_data")]
        pub fn with_plain_time(
            &self,
            time: Option<&PlainTime>,
        ) -> Result<Box<Self>, TemporalError> {
            self.with_plain_time_and_provider(time, &Provider::compiled())
        }
        pub fn with_plain_time_and_provider<'p>(
            &self,
            time: Option<&PlainTime>,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            with_provider!(p, |p| self
                .0
                .with_plain_time_and_provider(time.map(|t| t.0), p))
            .map(|x| Box::new(ZonedDateTime(x)))
            .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn add(
            &self,
            duration: &Duration,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.add_with_provider(duration, overflow, &Provider::compiled())
        }
        pub fn add_with_provider<'p>(
            &self,
            duration: &Duration,
            overflow: Option<ArithmeticOverflow>,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            with_provider!(p, |p| self.0.add_with_provider(
                &duration.0,
                overflow.map(Into::into),
                p
            ))
            .map(|x| Box::new(Self(x)))
            .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn subtract(
            &self,
            duration: &Duration,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.subtract_with_provider(duration, overflow, &Provider::compiled())
        }
        pub fn subtract_with_provider<'p>(
            &self,
            duration: &Duration,
            overflow: Option<ArithmeticOverflow>,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            with_provider!(p, |p| self.0.subtract_with_provider(
                &duration.0,
                overflow.map(Into::into),
                p
            ))
            .map(|x| Box::new(Self(x)))
            .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn until(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.until_with_provider(other, settings, &Provider::compiled())
        }
        pub fn until_with_provider<'p>(
            &self,
            other: &Self,
            settings: DifferenceSettings,
            p: &Provider<'p>,
        ) -> Result<Box<Duration>, TemporalError> {
            with_provider!(p, |p| self.0.until_with_provider(
                &other.0,
                settings.try_into()?,
                p
            ))
            .map(|x| Box::new(Duration(x)))
            .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn since(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.since_with_provider(other, settings, &Provider::compiled())
        }
        pub fn since_with_provider<'p>(
            &self,
            other: &Self,
            settings: DifferenceSettings,
            p: &Provider<'p>,
        ) -> Result<Box<Duration>, TemporalError> {
            with_provider!(p, |p| self.0.since_with_provider(
                &other.0,
                settings.try_into()?,
                p
            ))
            .map(|x| Box::new(Duration(x)))
            .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn round(&self, options: RoundingOptions) -> Result<Box<Self>, TemporalError> {
            self.round_with_provider(options, &Provider::compiled())
        }
        pub fn round_with_provider<'p>(
            &self,
            options: RoundingOptions,
            p: &Provider<'p>,
        ) -> Result<Box<Self>, TemporalError> {
            with_provider!(p, |p| self.0.round_with_provider(options.try_into()?, p))
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }

        // Same as PlainDateTime (getters)
        pub fn hour(&self) -> u8 {
            // unwrap_or_default because of
            // https://github.com/boa-dev/temporal/issues/548
            self.0.hour()
        }
        pub fn minute(&self) -> u8 {
            self.0.minute()
        }
        pub fn second(&self) -> u8 {
            self.0.second()
        }
        pub fn millisecond(&self) -> u16 {
            self.0.millisecond()
        }
        pub fn microsecond(&self) -> u16 {
            self.0.microsecond()
        }
        pub fn nanosecond(&self) -> u16 {
            self.0.nanosecond()
        }

        pub fn calendar<'a>(&'a self) -> &'a Calendar {
            Calendar::transparent_convert(self.0.calendar())
        }
        pub fn year(&self) -> i32 {
            self.0.year()
        }
        pub fn month(&self) -> u8 {
            self.0.month()
        }
        pub fn month_code(&self, write: &mut DiplomatWrite) {
            // https://github.com/boa-dev/temporal/issues/328 for the fallibility
            let code = self.0.month_code();
            // throw away the error, this should always succeed
            let _ = write.write_str(code.as_str());
        }
        pub fn day(&self) -> u8 {
            self.0.day()
        }

        pub fn day_of_week(&self) -> u16 {
            self.0.day_of_week()
        }

        pub fn day_of_year(&self) -> u16 {
            self.0.day_of_year()
        }

        pub fn week_of_year(&self) -> Option<u8> {
            self.0.week_of_year()
        }

        pub fn year_of_week(&self) -> Option<i32> {
            self.0.year_of_week()
        }

        pub fn days_in_week(&self) -> u16 {
            self.0.days_in_week()
        }
        pub fn days_in_month(&self) -> u16 {
            self.0.days_in_month()
        }

        pub fn days_in_year(&self) -> u16 {
            self.0.days_in_year()
        }

        pub fn months_in_year(&self) -> u16 {
            self.0.months_in_year()
        }

        pub fn in_leap_year(&self) -> bool {
            self.0.in_leap_year()
        }
        // Writes an empty string for no era
        pub fn era(&self, write: &mut DiplomatWrite) {
            let era = self.0.era();
            if let Some(era) = era {
                // throw away the error, this should always succeed
                let _ = write.write_str(&era);
            }
        }

        pub fn era_year(&self) -> Option<i32> {
            self.0.era_year()
        }

        #[allow(clippy::should_implement_trait)]
        pub fn clone(&self) -> Box<Self> {
            Box::new(Self(self.0.clone()))
        }
    }
}

pub(crate) fn zdt_from_epoch_ms_with_provider<'p>(
    ms: i64,
    time_zone: temporal_rs::TimeZone,
    p: &Provider<'p>,
) -> Result<temporal_rs::ZonedDateTime, TemporalError> {
    let instant = temporal_rs::Instant::from_epoch_milliseconds(ms)?;
    with_provider!(p, |p| instant
        .to_zoned_date_time_iso_with_provider(time_zone, p))
    .map_err(Into::into)
}

pub(crate) fn zdt_from_epoch_ns_with_provider<'p>(
    ns: I128Nanoseconds,
    time_zone: temporal_rs::TimeZone,
    p: &Provider<'p>,
) -> Result<temporal_rs::ZonedDateTime, TemporalError> {
    let instant = temporal_rs::Instant::try_new(ns.into())?;
    with_provider!(p, |p| instant
        .to_zoned_date_time_iso_with_provider(time_zone, p))
    .map_err(Into::into)
}

impl TryFrom<ffi::PartialZonedDateTime<'_>> for temporal_rs::partial::PartialZonedDateTime {
    type Error = TemporalError;
    fn try_from(other: ffi::PartialZonedDateTime<'_>) -> Result<Self, TemporalError> {
        let timezone = other.timezone.clone().into_option().map(|x| x.tz());
        let calendar = temporal_rs::Calendar::new(other.date.calendar.into());
        Ok(Self {
            fields: other.try_into()?,
            timezone,
            calendar,
        })
    }
}

impl TryFrom<ffi::PartialZonedDateTime<'_>> for temporal_rs::fields::ZonedDateTimeFields {
    type Error = TemporalError;
    fn try_from(other: ffi::PartialZonedDateTime<'_>) -> Result<Self, TemporalError> {
        let offset = match other.offset.into_option() {
            Some(o) => Some(temporal_rs::UtcOffset::from_utf8(o.into())?),
            None => None,
        };
        Ok(Self {
            calendar_fields: other.date.try_into()?,
            time: other.time.into(),
            offset,
        })
    }
}

impl From<ffi::RelativeTo<'_>> for Option<temporal_rs::options::RelativeTo> {
    fn from(other: ffi::RelativeTo) -> Self {
        if let Some(pd) = other.date {
            Some(temporal_rs::options::RelativeTo::PlainDate(pd.0.clone()))
        } else {
            other
                .zoned
                .map(|z| temporal_rs::options::RelativeTo::ZonedDateTime(z.0.clone()))
        }
    }
}

impl From<RelativeTo> for ffi::OwnedRelativeTo {
    fn from(other: RelativeTo) -> Self {
        use alloc::boxed::Box;
        match other {
            RelativeTo::PlainDate(d) => Self {
                date: Some(Box::new(crate::plain_date::ffi::PlainDate(d))),
                zoned: None,
            },
            RelativeTo::ZonedDateTime(d) => Self {
                zoned: Some(Box::new(ffi::ZonedDateTime(d))),
                date: None,
            },
        }
    }
}
