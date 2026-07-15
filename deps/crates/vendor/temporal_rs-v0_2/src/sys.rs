use crate::builtins::Now;
use crate::host::HostClock;
use crate::host::HostHooks;
use crate::host::HostTimeZone;
use crate::TemporalResult;

use crate::unix_time::EpochNanoseconds;
use crate::TemporalError;
use crate::TimeZone;
use timezone_provider::provider::TimeZoneProvider;
use web_time::{SystemTime, UNIX_EPOCH};

// TODO: Look into and potentially implement a `SystemTime` struct allows
// providing closures or trait implementations that can then
// be used to construct [`Now`]. Basically `Temporal` but with
// traits or closures.
//
// Temporal could then be something like:
//
// pub struct Temporal(SystemTime<DefaultSystemClock, DefaultSystemTimeZone>)
//

/// The Temporal object for accessing current system time
pub struct Temporal;

impl Temporal {
    /// Get a `Now` object for the default host system.
    #[cfg(feature = "sys-local")]
    #[deprecated(
        since = "0.1.0",
        note = "`now` deprecated was not clear about the host system implementation, please use `local_now`"
    )]
    pub fn now() -> Now<LocalHostSystem> {
        Now::new(LocalHostSystem)
    }

    /// Get a `Now` object with a [`LocalHostSystem`], which
    /// will use the host system's time zone as a fallback.
    #[cfg(feature = "sys-local")]
    pub fn local_now() -> Now<LocalHostSystem> {
        Now::new(LocalHostSystem)
    }

    /// Get a `Now` object with a [`UtcHostSystem`], which
    /// will use a UTC time zone as a fallback.
    #[cfg(feature = "sys")]
    pub fn utc_now() -> Now<UtcHostSystem> {
        Now::new(UtcHostSystem)
    }
}

/// A UTC host system implementation that will return the current time
/// with the a UTC time zone as fallback.
///
/// This implementation is backed by [`std::time::SystemTime`].
#[cfg(feature = "sys")]
pub struct UtcHostSystem;

#[cfg(feature = "sys")]
impl HostHooks for UtcHostSystem {}

#[cfg(feature = "sys")]
impl HostClock for UtcHostSystem {
    fn get_host_epoch_nanoseconds(&self) -> TemporalResult<EpochNanoseconds> {
        get_system_nanoseconds()
    }
}

#[cfg(feature = "sys")]
impl HostTimeZone for UtcHostSystem {
    fn get_host_time_zone(
        &self,
        provider: &(impl TimeZoneProvider + ?Sized),
    ) -> TemporalResult<TimeZone> {
        Ok(TimeZone::utc_with_provider(provider))
    }
}

/// A local host system implementation that will return the current time
/// with the system time zone as a fallback.
///
/// This implementation is backed by [`std::time::SystemTime`] and [`iana_time_zone`]
#[cfg(feature = "sys-local")]
pub struct LocalHostSystem;

#[cfg(feature = "sys-local")]
impl HostHooks for LocalHostSystem {}

#[cfg(feature = "sys-local")]
impl HostClock for LocalHostSystem {
    fn get_host_epoch_nanoseconds(&self) -> TemporalResult<EpochNanoseconds> {
        get_system_nanoseconds()
    }
}

#[cfg(feature = "sys-local")]
impl HostTimeZone for LocalHostSystem {
    fn get_host_time_zone(
        &self,
        provider: &(impl TimeZoneProvider + ?Sized),
    ) -> TemporalResult<TimeZone> {
        get_system_timezone(provider)
    }
}

#[cfg(feature = "sys-local")]
#[inline]
pub(crate) fn get_system_timezone(
    provider: &(impl TimeZoneProvider + ?Sized),
) -> TemporalResult<TimeZone> {
    iana_time_zone::get_timezone()
        .map(|s| TimeZone::try_from_identifier_str_with_provider(&s, provider))
        .map_err(|_| TemporalError::general("Error fetching system time"))?
}

/// Returns the system time in nanoseconds.
pub(crate) fn get_system_nanoseconds() -> TemporalResult<EpochNanoseconds> {
    use crate::unix_time::EpochNanoseconds;

    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|_| TemporalError::general("Error fetching system time"))
        .map(|d| EpochNanoseconds::from(d.as_nanos() as i128))
}
