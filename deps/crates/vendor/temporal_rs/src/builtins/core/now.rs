//! The Temporal Now component

use crate::iso::IsoDateTime;
use crate::TemporalResult;
use crate::{host::HostHooks, provider::TimeZoneProvider};

use super::{
    calendar::Calendar, time_zone::TimeZone, Instant, PlainDate, PlainDateTime, PlainTime,
    ZonedDateTime,
};

pub struct Now<H: HostHooks> {
    host_hooks: H,
}

impl<H: HostHooks> Now<H> {
    /// Create a new `Now`
    pub const fn new(host_hooks: H) -> Self {
        Self { host_hooks }
    }
}

impl<H: HostHooks> Now<H> {
    pub(crate) fn system_datetime_with_provider(
        self,
        time_zone: Option<TimeZone>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<IsoDateTime> {
        let system_nanoseconds = self.host_hooks.get_system_epoch_nanoseconds()?;
        let time_zone = time_zone.unwrap_or(self.host_hooks.get_system_time_zone(provider)?);
        time_zone.get_iso_datetime_for(&Instant::from(system_nanoseconds), provider)
    }

    /// Converts the current [`Now`] into a [`TimeZone`].
    pub fn time_zone_with_provider(
        self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<TimeZone> {
        self.host_hooks.get_system_time_zone(provider)
    }

    /// Converts the current [`Now`] into an [`Instant`].
    pub fn instant(self) -> TemporalResult<Instant> {
        Ok(Instant::from(
            self.host_hooks.get_system_epoch_nanoseconds()?,
        ))
    }

    /// Converts the current [`Now`] into an [`ZonedDateTime`] with an ISO8601 calendar.
    pub fn zoned_date_time_iso_with_provider(
        self,
        time_zone: Option<TimeZone>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<ZonedDateTime> {
        let system_nanoseconds = self.host_hooks.get_system_epoch_nanoseconds()?;
        let time_zone = time_zone.unwrap_or(self.host_hooks.get_system_time_zone(provider)?);
        let instant = Instant::from(system_nanoseconds);
        ZonedDateTime::new_unchecked_with_provider(instant, time_zone, Calendar::ISO, provider)
    }
}

impl<H: HostHooks> Now<H> {
    /// Converts `Now` into the current system [`PlainDateTime`] with an ISO8601 calendar.
    ///
    /// When `TimeZone` is `None`, the value will default to the
    /// system time zone or UTC if the system zone is unavailable.
    pub fn plain_date_time_iso_with_provider(
        self,
        time_zone: Option<TimeZone>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainDateTime> {
        let iso = self.system_datetime_with_provider(time_zone, provider)?;
        Ok(PlainDateTime::new_unchecked(iso, Calendar::ISO))
    }

    /// Converts `Now` into the current system [`PlainDate`] with an ISO8601 calendar.
    ///
    /// When `TimeZone` is `None`, the value will default to the
    /// system time zone or UTC if the system zone is unavailable.
    pub fn plain_date_iso_with_provider(
        self,
        time_zone: Option<TimeZone>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainDate> {
        let iso = self.system_datetime_with_provider(time_zone, provider)?;
        Ok(PlainDate::new_unchecked(iso.date, Calendar::ISO))
    }

    /// Converts `Now` into the current system [`PlainTime`].
    ///
    /// When `TimeZone` is `None`, the value will default to the
    /// system time zone or UTC if the system zone is unavailable.
    pub fn plain_time_with_provider(
        self,
        time_zone: Option<TimeZone>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<PlainTime> {
        let iso = self.system_datetime_with_provider(time_zone, provider)?;
        Ok(PlainTime::new_unchecked(iso.time))
    }
}

#[cfg(test)]
mod tests {

    #[cfg(feature = "tzdb")]
    use crate::options::DifferenceSettings;
    #[cfg(feature = "tzdb")]
    use crate::unix_time::EpochNanoseconds;

    #[cfg(feature = "tzdb")]
    #[test]
    fn mocked_datetime() {
        use timezone_provider::provider::TimeZoneProvider;

        use crate::{
            host::{HostClock, HostHooks, HostTimeZone},
            now::Now,
            tzdb::FsTzdbProvider,
            TemporalResult, TimeZone,
        };
        let provider = FsTzdbProvider::default();

        // Define mock test hooks
        struct TestHooks {
            seconds_to_add: i128,
            time_zone: TimeZone,
        }

        impl TestHooks {
            fn new(seconds_to_add: i128, time_zone: TimeZone) -> Self {
                Self {
                    seconds_to_add,
                    time_zone,
                }
            }
        }

        impl HostHooks for TestHooks {}

        impl HostClock for TestHooks {
            fn get_host_epoch_nanoseconds(&self) -> TemporalResult<EpochNanoseconds> {
                // 2025-03-11T10:47-06:00
                const TIME_BASE: i128 = 1_741_751_188_077_363_694;
                let epoch_nanoseconds = TIME_BASE + (self.seconds_to_add * 1_000_000_000);
                Ok(EpochNanoseconds::from(epoch_nanoseconds))
            }
        }

        impl HostTimeZone for TestHooks {
            fn get_host_time_zone(&self, _: &impl TimeZoneProvider) -> TemporalResult<TimeZone> {
                Ok(self.time_zone)
            }
        }

        // Define a UtcOffset zone
        let cdt = TimeZone::try_from_identifier_str_with_provider("-05:00", &provider).unwrap();

        // Define an IANA id zone
        let uschi =
            TimeZone::try_from_identifier_str_with_provider("America/Chicago", &provider).unwrap();

        let now = Now::new(TestHooks::new(0, cdt));
        let cdt_datetime = now
            .plain_date_time_iso_with_provider(None, &provider)
            .unwrap();
        assert_eq!(cdt_datetime.year(), 2025);
        assert_eq!(cdt_datetime.month(), 3);
        assert_eq!(cdt_datetime.month_code().as_str(), "M03");
        assert_eq!(cdt_datetime.day(), 11);
        assert_eq!(cdt_datetime.hour(), 22);
        assert_eq!(cdt_datetime.minute(), 46);
        assert_eq!(cdt_datetime.second(), 28);
        assert_eq!(cdt_datetime.millisecond(), 77);
        assert_eq!(cdt_datetime.microsecond(), 363);
        assert_eq!(cdt_datetime.nanosecond(), 694);

        let now_cdt = Now::new(TestHooks::new(0, cdt));
        let uschi_datetime = now_cdt
            .plain_date_time_iso_with_provider(Some(uschi), &provider)
            .unwrap();
        assert_eq!(cdt_datetime, uschi_datetime);

        let plus_5_now = Now::new(TestHooks::new(5, cdt));
        let plus_5_pdt = plus_5_now
            .plain_date_time_iso_with_provider(None, &provider)
            .unwrap();
        assert_eq!(plus_5_pdt.second(), 33);

        let duration = cdt_datetime
            .until(&plus_5_pdt, DifferenceSettings::default())
            .unwrap();
        assert_eq!(duration.hours(), 0);
        assert_eq!(duration.minutes(), 0);
        assert_eq!(duration.seconds(), 5);
        assert_eq!(duration.milliseconds(), 0);
    }

    #[cfg(all(feature = "tzdb", feature = "sys", feature = "compiled_data"))]
    #[test]
    fn now_datetime_test() {
        use crate::Temporal;
        use std::thread;
        use std::time::Duration as StdDuration;

        let sleep = 2;

        let before = Temporal::now().plain_date_time_iso(None).unwrap();
        thread::sleep(StdDuration::from_secs(sleep));
        let after = Temporal::now().plain_date_time_iso(None).unwrap();

        let diff = after.since(&before, DifferenceSettings::default()).unwrap();

        let sleep_base = sleep as i64;
        let tolerable_range = sleep_base..=sleep_base + 5;

        // We assert a tolerable range of sleep + 5 because std::thread::sleep
        // is only guaranteed to be >= the value to sleep. So to prevent sporadic
        // errors, we only assert a range.
        assert!(tolerable_range.contains(&diff.seconds()));
        assert_eq!(diff.hours(), 0);
        assert_eq!(diff.minutes(), 0);
    }
}
