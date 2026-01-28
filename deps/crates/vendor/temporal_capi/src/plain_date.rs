use temporal_rs::MonthCode;

use crate::error::ffi::TemporalError;

#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
pub mod ffi {
    use crate::calendar::ffi::{AnyCalendarKind, Calendar};
    use crate::duration::ffi::Duration;
    use crate::error::ffi::TemporalError;
    use crate::instant::ffi::I128Nanoseconds;
    use crate::options::ffi::{ArithmeticOverflow, DifferenceSettings, DisplayCalendar};
    use crate::plain_date_time::ffi::PlainDateTime;
    use crate::plain_month_day::ffi::PlainMonthDay;
    use crate::plain_time::ffi::PlainTime;
    use crate::plain_year_month::ffi::PlainYearMonth;
    use crate::provider::ffi::Provider;
    use crate::time_zone::ffi::TimeZone;
    use crate::zoned_date_time::ffi::ZonedDateTime;
    use alloc::boxed::Box;
    use alloc::string::String;
    use core::fmt::Write;
    use diplomat_runtime::{DiplomatOption, DiplomatStrSlice, DiplomatWrite};
    use diplomat_runtime::{DiplomatStr, DiplomatStr16};
    use writeable::Writeable;

    use core::str::FromStr;

    #[diplomat::opaque]
    pub struct PlainDate(pub(crate) temporal_rs::PlainDate);

    pub struct PartialDate<'a> {
        pub year: DiplomatOption<i32>,
        pub month: DiplomatOption<u8>,
        // None if empty
        pub month_code: DiplomatStrSlice<'a>,
        pub day: DiplomatOption<u8>,
        // None if empty
        pub era: DiplomatStrSlice<'a>,
        pub era_year: DiplomatOption<i32>,
        pub calendar: AnyCalendarKind,
    }

    #[diplomat::opaque]
    pub struct ParsedDate(pub(crate) temporal_rs::parsed_intermediates::ParsedDate);

    impl ParsedDate {
        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::parsed_intermediates::ParsedDate::from_utf8(s)
                .map(|x| Box::new(ParsedDate(x)))
                .map_err(Into::<TemporalError>::into)
        }
        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;

            temporal_rs::parsed_intermediates::ParsedDate::from_utf8(s.as_bytes())
                .map(|x| Box::new(ParsedDate(x)))
                .map_err(Into::<TemporalError>::into)
        }

        pub fn year_month_from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::parsed_intermediates::ParsedDate::year_month_from_utf8(s)
                .map(|x| Box::new(ParsedDate(x)))
                .map_err(Into::<TemporalError>::into)
        }
        pub fn year_month_from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;

            temporal_rs::parsed_intermediates::ParsedDate::year_month_from_utf8(s.as_bytes())
                .map(|x| Box::new(ParsedDate(x)))
                .map_err(Into::<TemporalError>::into)
        }

        pub fn month_day_from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::parsed_intermediates::ParsedDate::month_day_from_utf8(s)
                .map(|x| Box::new(ParsedDate(x)))
                .map_err(Into::<TemporalError>::into)
        }
        pub fn month_day_from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;

            temporal_rs::parsed_intermediates::ParsedDate::month_day_from_utf8(s.as_bytes())
                .map(|x| Box::new(ParsedDate(x)))
                .map_err(Into::<TemporalError>::into)
        }
    }

    impl PlainDate {
        pub fn try_new_constrain(
            year: i32,
            month: u8,
            day: u8,
            calendar: AnyCalendarKind,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDate::new(
                year,
                month,
                day,
                temporal_rs::Calendar::new(calendar.into()),
            )
            .map(|x| Box::new(PlainDate(x)))
            .map_err(Into::into)
        }
        pub fn try_new(
            year: i32,
            month: u8,
            day: u8,
            calendar: AnyCalendarKind,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDate::try_new(
                year,
                month,
                day,
                temporal_rs::Calendar::new(calendar.into()),
            )
            .map(|x| Box::new(PlainDate(x)))
            .map_err(Into::into)
        }
        pub fn try_new_with_overflow(
            year: i32,
            month: u8,
            day: u8,
            calendar: AnyCalendarKind,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDate::new_with_overflow(
                year,
                month,
                day,
                temporal_rs::Calendar::new(calendar.into()),
                overflow.into(),
            )
            .map(|x| Box::new(PlainDate(x)))
            .map_err(Into::into)
        }
        pub fn from_partial(
            partial: PartialDate,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDate::from_partial(partial.try_into()?, overflow.map(Into::into))
                .map(|x| Box::new(PlainDate(x)))
                .map_err(Into::into)
        }

        pub fn from_parsed(parsed: &ParsedDate) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDate::from_parsed(parsed.0)
                .map(|x| Box::new(PlainDate(x)))
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
            Ok(Box::new(Self(zdt.to_plain_date())))
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
            Ok(Box::new(Self(zdt.to_plain_date())))
        }

        pub fn with(
            &self,
            partial: PartialDate,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .with(partial.try_into()?, overflow.map(Into::into))
                .map(|x| Box::new(PlainDate(x)))
                .map_err(Into::into)
        }

        pub fn with_calendar(&self, calendar: AnyCalendarKind) -> Box<Self> {
            Box::new(PlainDate(
                self.0
                    .with_calendar(temporal_rs::Calendar::new(calendar.into())),
            ))
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainDate::from_utf8(s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            temporal_rs::PlainDate::from_str(&s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn calendar<'a>(&'a self) -> &'a Calendar {
            Calendar::transparent_convert(self.0.calendar())
        }

        pub fn is_valid(&self) -> bool {
            self.0.is_valid()
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

        pub fn to_plain_date_time(
            &self,
            time: Option<&PlainTime>,
        ) -> Result<Box<PlainDateTime>, TemporalError> {
            self.0
                .to_plain_date_time(time.map(|t| t.0))
                .map(|x| Box::new(PlainDateTime(x)))
                .map_err(Into::into)
        }

        pub fn to_plain_month_day(&self) -> Result<Box<PlainMonthDay>, TemporalError> {
            self.0
                .to_plain_month_day()
                .map(|x| Box::new(PlainMonthDay(x)))
                .map_err(Into::into)
        }

        pub fn to_plain_year_month(&self) -> Result<Box<PlainYearMonth>, TemporalError> {
            self.0
                .to_plain_year_month()
                .map(|x| Box::new(PlainYearMonth(x)))
                .map_err(Into::into)
        }
        #[cfg(feature = "compiled_data")]
        pub fn to_zoned_date_time(
            &self,
            time_zone: TimeZone,
            time: Option<&PlainTime>,
        ) -> Result<Box<ZonedDateTime>, TemporalError> {
            self.to_zoned_date_time_with_provider(time_zone, time, &Provider::compiled())
        }
        pub fn to_zoned_date_time_with_provider<'p>(
            &self,
            time_zone: TimeZone,
            time: Option<&PlainTime>,
            p: &Provider<'p>,
        ) -> Result<Box<ZonedDateTime>, TemporalError> {
            with_provider!(p, |p| self.0.to_zoned_date_time_with_provider(
                time_zone.into(),
                time.map(|x| x.0),
                p
            ))
            .map(|x| Box::new(ZonedDateTime(x)))
            .map_err(Into::into)
        }
        pub fn to_ixdtf_string(
            &self,
            display_calendar: DisplayCalendar,
            write: &mut DiplomatWrite,
        ) {
            let writeable = self.0.to_ixdtf_writeable(display_calendar.into());
            // This can only fail in cases where the DiplomatWriteable is capped, we
            // don't care about that.
            let _ = writeable.write_to(write);
        }

        #[allow(clippy::should_implement_trait)]
        pub fn clone(&self) -> Box<Self> {
            Box::new(Self(self.0.clone()))
        }
    }
}

impl TryFrom<ffi::PartialDate<'_>> for temporal_rs::partial::PartialDate {
    type Error = TemporalError;
    fn try_from(other: ffi::PartialDate<'_>) -> Result<Self, TemporalError> {
        let calendar = temporal_rs::Calendar::new(other.calendar.into());
        Ok(Self {
            calendar_fields: other.try_into()?,
            calendar,
        })
    }
}

impl TryFrom<ffi::PartialDate<'_>> for temporal_rs::partial::PartialYearMonth {
    type Error = TemporalError;
    fn try_from(other: ffi::PartialDate<'_>) -> Result<Self, TemporalError> {
        let calendar = temporal_rs::Calendar::new(other.calendar.into());
        Ok(Self {
            calendar_fields: other.try_into()?,
            calendar,
        })
    }
}

impl TryFrom<ffi::PartialDate<'_>> for temporal_rs::fields::CalendarFields {
    type Error = TemporalError;
    fn try_from(other: ffi::PartialDate<'_>) -> Result<Self, TemporalError> {
        use temporal_rs::TinyAsciiStr;

        let month_code = if other.month_code.is_empty() {
            None
        } else {
            Some(MonthCode::try_from_utf8(other.month_code.into()).map_err(TemporalError::from)?)
        };

        let era = if other.era.is_empty() {
            None
        } else {
            Some(TinyAsciiStr::try_from_utf8(other.era.into()).map_err(|_| {
                TemporalError::from(
                    temporal_rs::TemporalError::range().with_message("Invalid era code."),
                )
            })?)
        };
        Ok(Self {
            year: other.year.into(),
            month: other.month.into(),
            month_code,
            day: other.day.into(),
            era_year: other.era_year.into(),
            era,
        })
    }
}

impl TryFrom<ffi::PartialDate<'_>> for temporal_rs::fields::YearMonthCalendarFields {
    type Error = TemporalError;
    fn try_from(other: ffi::PartialDate<'_>) -> Result<Self, TemporalError> {
        use temporal_rs::TinyAsciiStr;

        let month_code = if other.month_code.is_empty() {
            None
        } else {
            Some(MonthCode::try_from_utf8(other.month_code.into()).map_err(TemporalError::from)?)
        };

        let era = if other.era.is_empty() {
            None
        } else {
            Some(TinyAsciiStr::try_from_utf8(other.era.into()).map_err(|_| {
                TemporalError::from(
                    temporal_rs::TemporalError::range().with_message("Invalid era code."),
                )
            })?)
        };
        Ok(Self {
            year: other.year.into(),
            month: other.month.into(),
            month_code,
            era_year: other.era_year.into(),
            era,
        })
    }
}
