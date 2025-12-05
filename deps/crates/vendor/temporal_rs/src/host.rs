//! Trait definitions for accessing values from the host environment.
//!
//! NOTE: This is a power user API.

use timezone_provider::{epoch_nanoseconds::EpochNanoseconds, provider::TimeZoneProvider};

use crate::{TemporalResult, TimeZone, UtcOffset};

/// The `HostClock` trait defines an accessor to the host's clock.
pub trait HostClock {
    fn get_host_epoch_nanoseconds(&self) -> TemporalResult<EpochNanoseconds>;
}

/// The `HostTimeZone` trait defines the host's time zone.
pub trait HostTimeZone {
    fn get_host_time_zone(&self, provider: &impl TimeZoneProvider) -> TemporalResult<TimeZone>;
}

/// `HostHooks` marks whether a trait implements the required host hooks with some
/// system methods.
pub trait HostHooks: HostClock + HostTimeZone {
    fn get_system_epoch_nanoseconds(&self) -> TemporalResult<EpochNanoseconds> {
        self.get_host_epoch_nanoseconds()
    }

    fn get_system_time_zone(&self, provider: &impl TimeZoneProvider) -> TemporalResult<TimeZone> {
        self.get_host_time_zone(provider)
    }
}

/// The empty host is a default implementation of a system host.
///
/// This implementation will always return zero epoch nanoseconds and
/// a +00:00 time zone.
///
/// ```
/// # #[cfg(feature = "compiled_data")] {
/// use temporal_rs::host::EmptyHostSystem;
/// use temporal_rs::now::Now;
///
/// let now = Now::new(EmptyHostSystem);
/// let zoned_date_time = now.zoned_date_time_iso(None).unwrap();
///
/// assert_eq!(zoned_date_time.to_string(), "1970-01-01T00:00:00+00:00[+00:00]");
///
/// # }
/// ```
pub struct EmptyHostSystem;

impl HostClock for EmptyHostSystem {
    fn get_host_epoch_nanoseconds(&self) -> TemporalResult<EpochNanoseconds> {
        Ok(EpochNanoseconds::from_seconds(0))
    }
}

impl HostTimeZone for EmptyHostSystem {
    fn get_host_time_zone(&self, _: &impl TimeZoneProvider) -> TemporalResult<TimeZone> {
        Ok(TimeZone::from(UtcOffset::default()))
    }
}

impl HostHooks for EmptyHostSystem {}
