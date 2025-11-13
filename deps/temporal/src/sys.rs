use crate::builtins::Now;
use crate::host::HostClock;
use crate::host::HostHooks;
use crate::host::HostTimeZone;
use crate::TemporalResult;

use crate::unix_time::EpochNanoseconds;
use crate::TemporalError;
use crate::TimeZone;
#[cfg(feature = "sys")]
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
#[cfg(feature = "sys")]
pub struct Temporal;

#[cfg(feature = "sys")]
impl Temporal {
    /// Get a `Now` object for the default host system.
    pub fn now() -> Now<DefaultHostSystem> {
        Now::new(DefaultHostSystem)
    }
}

/// A default host system implementation
///
/// This implementation is backed by [`SystemTime`] and [`iana_time_zone`]
#[cfg(feature = "sys")]
pub struct DefaultHostSystem;

#[cfg(feature = "sys")]
impl HostHooks for DefaultHostSystem {}

#[cfg(feature = "sys")]
impl HostClock for DefaultHostSystem {
    fn get_host_epoch_nanoseconds(&self) -> TemporalResult<EpochNanoseconds> {
        get_system_nanoseconds()
    }
}

#[cfg(feature = "sys")]
impl HostTimeZone for DefaultHostSystem {
    fn get_host_time_zone(
        &self,
        provider: &impl timezone_provider::provider::TimeZoneProvider,
    ) -> TemporalResult<TimeZone> {
        get_system_timezone(provider)
    }
}

#[cfg(feature = "sys")]
#[inline]
pub(crate) fn get_system_timezone(provider: &impl TimeZoneProvider) -> TemporalResult<TimeZone> {
    iana_time_zone::get_timezone()
        .map(|s| TimeZone::try_from_identifier_str_with_provider(&s, provider))
        .map_err(|_| TemporalError::general("Error fetching system time"))?
}

/// Returns the system time in nanoseconds.
#[cfg(feature = "sys")]
pub(crate) fn get_system_nanoseconds() -> TemporalResult<EpochNanoseconds> {
    use crate::unix_time::EpochNanoseconds;

    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|_| TemporalError::general("Error fetching system time"))
        .map(|d| EpochNanoseconds::from(d.as_nanos() as i128))
}
