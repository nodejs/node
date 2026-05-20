#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
pub mod ffi {
    use crate::calendar::ffi::{AnyCalendarKind, Calendar};
    use crate::duration::ffi::Duration;
    use crate::error::ffi::TemporalError;
    use crate::provider::ffi::Provider;
    use crate::time_zone::ffi::TimeZone;
    use alloc::boxed::Box;

    use crate::options::ffi::{ArithmeticOverflow, DifferenceSettings, DisplayCalendar};
    use crate::plain_date::ffi::{PartialDate, PlainDate};
    use alloc::string::String;
    use core::fmt::Write;
    use diplomat_runtime::DiplomatWrite;
    use diplomat_runtime::{DiplomatStr, DiplomatStr16};
    use writeable::Writeable;

    use core::str::FromStr;

    #[diplomat::opaque]
    pub struct PlainYearMonth(pub(crate) temporal_rs::PlainYearMonth);

    impl PlainYearMonth {
        pub fn try_new_with_overflow(
            year: i32,
            month: u8,
            reference_day: Option<u8>,
            calendar: AnyCalendarKind,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainYearMonth::new_with_overflow(
                year,
                month,
                reference_day,
                temporal_rs::Calendar::new(calendar.into()),
                overflow.into(),
            )
            .map(|x| Box::new(PlainYearMonth(x)))
            .map_err(Into::into)
        }

        pub fn from_partial(
            partial: PartialDate,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainYearMonth::from_partial(partial.try_into()?, overflow.map(Into::into))
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }

        pub fn from_parsed(
            parsed: &crate::plain_date::ffi::ParsedDate,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainYearMonth::from_parsed(parsed.0)
                .map(|x| Box::new(PlainYearMonth(x)))
                .map_err(Into::into)
        }

        pub fn with(
            &self,
            partial: PartialDate,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<Self>, TemporalError> {
            let fields: temporal_rs::fields::YearMonthCalendarFields = partial.try_into()?;
            self.0
                .with(fields, overflow.map(Into::into))
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainYearMonth::from_utf8(s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            temporal_rs::PlainYearMonth::from_str(&s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
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

        pub fn in_leap_year(&self) -> bool {
            self.0.in_leap_year()
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

        pub fn calendar<'a>(&'a self) -> &'a Calendar {
            Calendar::transparent_convert(self.0.calendar())
        }
        pub fn add(
            &self,
            duration: &Duration,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .add(&duration.0, overflow.into())
                .map(|x| Box::new(Self(x)))
                .map_err(Into::into)
        }
        pub fn subtract(
            &self,
            duration: &Duration,
            overflow: ArithmeticOverflow,
        ) -> Result<Box<Self>, TemporalError> {
            self.0
                .subtract(&duration.0, overflow.into())
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
        pub fn to_plain_date(
            &self,
            day: Option<PartialDate>,
        ) -> Result<Box<PlainDate>, TemporalError> {
            self.0
                .to_plain_date(day.map(|d| d.try_into()).transpose()?)
                .map(|x| Box::new(PlainDate(x)))
                .map_err(Into::into)
        }

        #[cfg(feature = "compiled_data")]
        pub fn epoch_ms_for(&self, time_zone: TimeZone) -> Result<i64, TemporalError> {
            self.epoch_ms_for_with_provider(time_zone, &Provider::compiled())
        }
        pub fn epoch_ms_for_with_provider<'p>(
            &self,
            time_zone: TimeZone,
            p: &Provider<'p>,
        ) -> Result<i64, TemporalError> {
            let ns = with_provider!(p, |p| self
                .0
                .epoch_ns_for_with_provider(time_zone.into(), p))
            .map_err(TemporalError::from)?;

            let ns_i128 = ns.as_i128();
            let ms = ns_i128 / 1_000_000;
            if let Ok(ms) = i64::try_from(ms) {
                Ok(ms)
            } else {
                Err(TemporalError::assert("Found an out-of-range YearMonth"))
            }
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
