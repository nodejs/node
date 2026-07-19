#[diplomat::bridge]
#[diplomat::abi_rename = "temporal_rs_{0}"]
pub mod ffi {
    use crate::calendar::ffi::{AnyCalendarKind, Calendar};
    use crate::error::ffi::TemporalError;
    use alloc::boxed::Box;

    use crate::options::ffi::{ArithmeticOverflow, DisplayCalendar};
    use crate::plain_date::ffi::{PartialDate, PlainDate};
    use crate::time_zone::ffi::TimeZone;

    use crate::provider::ffi::Provider;
    use alloc::string::String;
    use core::fmt::Write;
    use core::str::FromStr;
    use diplomat_runtime::DiplomatWrite;
    use diplomat_runtime::{DiplomatStr, DiplomatStr16};
    use writeable::Writeable;

    #[diplomat::opaque]
    pub struct PlainMonthDay(pub(crate) temporal_rs::PlainMonthDay);

    impl PlainMonthDay {
        pub fn try_new_with_overflow(
            month: u8,
            day: u8,
            calendar: AnyCalendarKind,
            overflow: ArithmeticOverflow,
            ref_year: Option<i32>,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainMonthDay::new_with_overflow(
                month,
                day,
                temporal_rs::Calendar::new(calendar.into()),
                overflow.into(),
                ref_year,
            )
            .map(|x| Box::new(PlainMonthDay(x)))
            .map_err(Into::into)
        }

        pub fn from_partial(
            partial: PartialDate,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<PlainMonthDay>, TemporalError> {
            temporal_rs::PlainMonthDay::from_partial(partial.try_into()?, overflow.map(Into::into))
                .map(|x| Box::new(PlainMonthDay(x)))
                .map_err(Into::into)
        }
        pub fn from_parsed(
            parsed: &crate::plain_date::ffi::ParsedDate,
        ) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainMonthDay::from_parsed(parsed.0)
                .map(|x| Box::new(PlainMonthDay(x)))
                .map_err(Into::into)
        }

        pub fn with(
            &self,
            partial: PartialDate,
            overflow: Option<ArithmeticOverflow>,
        ) -> Result<Box<PlainMonthDay>, TemporalError> {
            self.0
                .with(partial.try_into()?, overflow.map(Into::into))
                .map(|x| Box::new(PlainMonthDay(x)))
                .map_err(Into::into)
        }

        pub fn equals(&self, other: &Self) -> bool {
            self.0 == other.0
        }

        pub fn from_utf8(s: &DiplomatStr) -> Result<Box<Self>, TemporalError> {
            temporal_rs::PlainMonthDay::from_utf8(s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn from_utf16(s: &DiplomatStr16) -> Result<Box<Self>, TemporalError> {
            // TODO(#275) This should not need to convert
            let s = String::from_utf16(s).map_err(|_| temporal_rs::TemporalError::range())?;
            temporal_rs::PlainMonthDay::from_str(&s)
                .map(|c| Box::new(Self(c)))
                .map_err(Into::into)
        }

        pub fn day(&self) -> u8 {
            self.0.day()
        }
        pub fn calendar<'a>(&'a self) -> &'a Calendar {
            Calendar::transparent_convert(self.0.calendar())
        }

        pub fn month_code(&self, write: &mut DiplomatWrite) {
            let code = self.0.month_code();
            // throw away the error, this should always succeed
            let _ = write.write_str(code.as_str());
        }

        pub fn to_plain_date(
            &self,
            year: Option<PartialDate>,
        ) -> Result<Box<PlainDate>, TemporalError> {
            self.0
                .to_plain_date(year.map(|y| y.try_into()).transpose()?)
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
                .epoch_ns_for_with_provider(time_zone.into(), p)
                .map_err(TemporalError::from))?;

            let ns_i128 = ns.as_i128();
            let ms = ns_i128 / 1_000_000;
            if let Ok(ms) = i64::try_from(ms) {
                Ok(ms)
            } else {
                Err(TemporalError::assert("Found an out-of-range MonthDay"))
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
