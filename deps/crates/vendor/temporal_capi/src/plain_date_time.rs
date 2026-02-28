use crate::error::ffi::TemporalError;

#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
pub mod ffi {
    use crate::calendar::ffi::{AnyCalendarKind, Calendar};
    use crate::duration::ffi::Duration;
    use crate::error::ffi::TemporalError;
    use crate::instant::ffi::I128Nanoseconds;
    use crate::options::ffi::{
        ArithmeticOverflow, DifferenceSettings, DisplayCalendar, RoundingOptions,
        ToStringRoundingOptions,
    };
    use crate::provider::ffi::Provider;
    use crate::time_zone::ffi::TimeZone;
    use crate::zoned_date_time::ffi::ZonedDateTime;
    use alloc::boxed::Box;

    use crate::options::ffi::Disambiguation;
    use crate::plain_date::ffi::{PartialDate, PlainDate};
    use crate::plain_time::ffi::{PartialTime, PlainTime};
    use alloc::string::String;
    use core::fmt::Write;
    use core::str::FromStr;
    use diplomat_runtime::DiplomatWrite;
    use diplomat_runtime::{DiplomatStr, DiplomatStr16};
    use writeable::Writeable;

    #[diplomat::opaque]
    pub struct PlainDateTime(pub(crate) temporal_rs::PlainDateTime);

    pub struct PartialDateTime<'a> {
        pub date: PartialDate<'a>,
        pub time: PartialTime,
    }

    #[diplomat::opaque]
    pub struct ParsedDateTime(temporal_rs::parsed_intermediates::ParsedDateTime);

    impl ParsedDateTime {
        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::parsed_intermediates::ParsedDateTime::from_utf8(s)
                .map(|x| Box::new(ParsedDateTime(x)))
                .map_err(Into::<TemporalError>::into)
        }
        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;

            temporal_rs::parsed_intermediates::ParsedDateTime::from_utf8(s.as_bytes())
                .map(|x| Box::new(ParsedDateTime(x)))
                .map_err(Into::<TemporalError>::into)
        }
    }

    impl PlainDateTime {
        pub fn try_new_constrain(
            year: i32,
            month: u8,
            day: u8,
            hour: u8,
            minute: u8,
            second: u8,
            millisecond: u16,
            microsecond: u16,
            nanosecond: u16,
            calendar: AnyCalendarKind,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDateTime::new(
                year,
                month,
                day,
                hour,
                minute,
                second,
                millisecond,
                microsecond,
                nanosecond,
                temporal_rs::Calendar::new(calendar.into()),
            )
            .map(|x| Box::new(PlainDateTime(x)))
            .map_err(Into::into)
        }
        pub fn try_new(
            year: i32,
            month: u8,
            day: u8,
            hour: u8,
            minute: u8,
            second: u8,
            millisecond: u16,
            microsecond: u16,
            nanosecond: u16,
            calendar: AnyCalendarKind,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDateTime::try_new(
                year,
                month,
                day,
                hour,
                minute,
                second,
                millisecond,
                microsecond,
                nanosecond,
                temporal_rs::Calendar::new(calendar.into()),
            )
            .map(|x| Box::new(PlainDateTime(x)))
            .map_err(Into::into)
        }

        pub fn from_partial(
            partial: PartialDateTime,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDateTime::from_partial(partial.try_into()?, overflow.map(Into::into))
                .map(|x| Box::new(PlainDateTime(x)))
                .map_err(Into::into)
        }

        pub fn from_parsed(parsed: &ParsedDateTime) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDateTime::from_parsed(parsed.0)
                .map(|x| Box::new(PlainDateTime(x)))
                .map_err(Into::into)
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
            let zdt = crate::zoned_date_time::zdt_from_epoch_ms_with_provider(ms, tz.into(), p)?;
            Ok(Box::new(Self(zdt.to_plain_date_time())))
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
            Ok(Box::new(Self(zdt.to_plain_date_time())))
        }
        pub fn with(
            &self,
            partial: PartialDateTime,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .with(partial.try_into()?, overflow.map(Into::into))
                .map(|x| Box::new(PlainDateTime(x)))
                .map_err(Into::into)
        }

        pub fn with_time(&self, time: Option<&PlainTime>) -> Result<Box<Self>, TemporalError> {
            self.0
                .with_time(time.map(|t| t.0))
                .map(|x| Box::new(PlainDateTime(x)))
                .map_err(Into::into)
        }

        pub fn with_calendar(&self, calendar: AnyCalendarKind) -> Box<Self> {
            Box::new(PlainDateTime(
                self.0
                    .with_calendar(temporal_rs::Calendar::new(calendar.into())),
            ))
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDateTime::from_utf8(s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            temporal_rs::PlainDateTime::from_str(&s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn hour(&self) -> u8 {
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

        pub fn add(
            &self,
            duration: &Duration,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .add(&duration.0, overflow.map(Into::into))
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn subtract(
            &self,
            duration: &Duration,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .subtract(&duration.0, overflow.map(Into::into))
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn until(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.0
                .until(&other.0, settings.try_into()?)
                .map(|x| Box::new(Duration(x)))
                .map_err(Into::into)
        }
        pub fn since(
            &self,
            other: &Self,
            settings: DifferenceSettings,
        ) -> Result<Box<Duration>, TemporalError> {
            self.0
                .since(&other.0, settings.try_into()?)
                .map(|x| Box::new(Duration(x)))
                .map_err(Into::into)
        }

        pub fn equals(&self, other: &Self) -> bool {
            self.0 == other.0
        }

        pub fn compare(one: &Self, two: &Self) -> core::cmp::Ordering {
            one.0.compare_iso(&two.0)
        }

        pub fn round(&self, options: RoundingOptions) -> Result<Box<Self>, TemporalError> {
            self.0
                .round(options.try_into()?)
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }

        pub fn to_plain_date(&self) -> Box<PlainDate> {
            Box::new(PlainDate(self.0.to_plain_date()))
        }

        pub fn to_plain_time(&self) -> Box<PlainTime> {
            Box::new(PlainTime(self.0.to_plain_time()))
        }

        #[cfg(feature = "compiled_data")]
        pub fn to_zoned_date_time(
            &self,
            time_zone: TimeZone,
            disambiguation: Disambiguation,
        ) -> Result<Box<ZonedDateTime>, TemporalError> {
            self.to_zoned_date_time_with_provider(time_zone, disambiguation, &Provider::compiled())
        }

        pub fn to_zoned_date_time_with_provider<'p>(
            &self,
            time_zone: TimeZone,
            disambiguation: Disambiguation,
            p: &Provider<'p>,
        ) -> Result<Box<ZonedDateTime>, TemporalError> {
            with_provider!(p, |p| self.0.to_zoned_date_time_with_provider(
                time_zone.into(),
                disambiguation.into(),
                p
            ))
            .map(|x| Box::new(ZonedDateTime(x)))
            .map_err(Into::into)
        }

        pub fn to_ixdtf_string(
            &self,
            options: ToStringRoundingOptions,

            display_calendar: DisplayCalendar,
            write: &mut DiplomatWrite,
        ) -> Result<(), TemporalError> {
            let writeable = self
                .0
                .to_ixdtf_writeable(options.into(), display_calendar.into())?;

            // This can only fail in cases where the DiplomatWriteable is capped, we
            // don't care about that.
            let _ = writeable.write_to(write);
            Ok(())
        }

        #[allow(clippy::should_implement_trait)]
        pub fn clone(&self) -> Box<Self> {
            Box::new(Self(self.0.clone()))
        }
    }
}

impl TryFrom<ffi::PartialDateTime<'_>> for temporal_rs::partial::PartialDateTime {
    type Error = TemporalError;
    fn try_from(other: ffi::PartialDateTime<'_>) -> Result<Self, TemporalError> {
        let calendar = temporal_rs::Calendar::new(other.date.calendar.into());
        Ok(Self {
            fields: other.try_into()?,
            calendar,
        })
    }
}

impl TryFrom<ffi::PartialDateTime<'_>> for temporal_rs::fields::DateTimeFields {
    type Error = TemporalError;
    fn try_from(other: ffi::PartialDateTime<'_>) -> Result<Self, TemporalError> {
        Ok(Self {
            calendar_fields: other.date.try_into()?,
            time: other.time.into(),
        })
    }
}
