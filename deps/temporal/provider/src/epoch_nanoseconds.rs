use crate::TimeZoneProviderError;

/// Number of nanoseconds in a day
#[doc(hidden)]
pub const NS_PER_DAY: u64 = MS_PER_DAY as u64 * 1_000_000;

pub(crate) const NS_IN_S: i128 = 1_000_000_000;

/// Milliseconds per day constant: 8.64e+7
pub(crate) const MS_PER_DAY: u32 = 24 * 60 * 60 * 1000;
/// Max Instant nanosecond constant
#[doc(hidden)]
pub(crate) const NS_MAX_INSTANT: i128 = NS_PER_DAY as i128 * 100_000_000i128;
/// Min Instant nanosecond constant
#[doc(hidden)]
pub(crate) const NS_MIN_INSTANT: i128 = -NS_MAX_INSTANT;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Default)]
pub struct EpochNanoseconds(pub i128);

impl From<i128> for EpochNanoseconds {
    fn from(value: i128) -> Self {
        Self(value)
    }
}

// Potential TODO: Build out primitive arthmetic methods if needed.
impl EpochNanoseconds {
    pub fn as_i128(&self) -> i128 {
        self.0
    }

    pub fn check_validity(&self) -> Result<(), TimeZoneProviderError> {
        if !is_valid_epoch_nanos(&self.0) {
            return Err(TimeZoneProviderError::InstantOutOfRange);
        }
        Ok(())
    }

    pub fn from_seconds(seconds: i64) -> Self {
        Self(seconds as i128 * NS_IN_S)
    }
}

/// Utility for determining if the nanos are within a valid range.
#[inline]
#[must_use]
pub fn is_valid_epoch_nanos(nanos: &i128) -> bool {
    (NS_MIN_INSTANT..=NS_MAX_INSTANT).contains(nanos)
}

#[cfg(feature = "tzif")]
impl From<tzif::data::time::Seconds> for EpochNanoseconds {
    fn from(value: tzif::data::time::Seconds) -> Self {
        EpochNanoseconds::from_seconds(value.0)
    }
}

#[inline]
#[cfg(any(feature = "tzif", feature = "zoneinfo64"))]
pub(crate) fn seconds_to_nanoseconds(seconds: i64) -> i128 {
    seconds as i128 * NS_IN_S
}
