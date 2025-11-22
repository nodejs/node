//! An implementation of the Temporal Instant.

use alloc::string::String;
use core::{num::NonZeroU128, str::FromStr};

use crate::{
    builtins::core::{zoned_date_time::nanoseconds_to_formattable_offset_minutes, Duration},
    error::ErrorMessage,
    iso::IsoDateTime,
    options::{
        DifferenceOperation, DifferenceSettings, DisplayOffset, ResolvedRoundingOptions,
        RoundingOptions, ToStringRoundingOptions, Unit, UnitGroup,
    },
    parsers::{parse_instant, IxdtfStringBuilder},
    provider::TimeZoneProvider,
    rounding::IncrementRounder,
    unix_time::EpochNanoseconds,
    Calendar, TemporalError, TemporalResult, TemporalUnwrap, TimeZone,
};

use ixdtf::records::UtcOffsetRecordOrZ;
use num_traits::Euclid;
use writeable::Writeable;

use super::{
    duration::normalized::{InternalDurationRecord, TimeDuration},
    DateDuration, ZonedDateTime,
};

pub(crate) const NANOSECONDS_PER_SECOND: i64 = 1_000_000_000;
const NANOSECONDS_PER_MINUTE: i64 = 60 * NANOSECONDS_PER_SECOND;
const NANOSECONDS_PER_HOUR: i64 = 60 * NANOSECONDS_PER_MINUTE;

/// The native Rust implementation of `Temporal.Instant`.
///
/// Represents a precise moment in time measured as nanoseconds since the Unix epoch
/// (1970-01-01T00:00:00\[UTC\]). An `Instant` provides a universal timestamp
/// that represents the same moment regardless of timezone or calendar system.
///
/// Use `Instant` when you need to record exact moments in time, measure elapsed time,
/// or work with high-precision timestamps. Unlike `PlainDateTime`, an `Instant`
/// represents an absolute point on the timeline.
///
/// ## Examples
///
/// ### Creating instants
///
/// ```rust
/// use temporal_rs::Instant;
///
/// // From epoch nanoseconds (high-precision timestamps)
/// let precise_moment = Instant::try_new(1609459200000000000).unwrap();
/// assert_eq!(precise_moment.epoch_milliseconds(), 1609459200000);
///
/// // From epoch milliseconds (common in web applications)
/// let web_timestamp = Instant::from_epoch_milliseconds(1609459200000).unwrap();
/// assert_eq!(web_timestamp.epoch_nanoseconds().as_i128(), 1609459200000000000);
/// ```
///
/// ### Parsing ISO 8601 instant strings
///
/// ```rust
/// use temporal_rs::Instant;
/// use core::str::FromStr;
///
/// // Parse ISO 8601 instant strings (must include timezone info)
/// let instant = Instant::from_str("2024-03-15T14:30:45.123Z").unwrap();
/// assert_eq!(instant.epoch_milliseconds(), 1710513045123);
///
/// // Parse instants with different timezone notations
/// let instant2 = Instant::from_str("2024-03-15T14:30:45.123+00:00").unwrap();
/// let instant3 = Instant::from_str("2024-03-15T14:30:45.123-00:00").unwrap();
/// assert_eq!(instant, instant2);
/// assert_eq!(instant2, instant3);
/// ```
///
/// ### Instant arithmetic
///
/// ```rust
/// use temporal_rs::{Instant, Duration};
/// use core::str::FromStr;
///
/// let instant = Instant::try_new(1609459200000000000).unwrap(); // 2021-01-01T00:00:00Z
///
/// // Add time duration (only time durations, not date durations)
/// let later = instant.add(&Duration::from_str("PT1H30M").unwrap()).unwrap();
/// let expected_ns = 1609459200000000000 + (1 * 3600 + 30 * 60) * 1_000_000_000;
/// assert_eq!(later.epoch_nanoseconds().as_i128(), expected_ns);
///
/// // Calculate difference between instants
/// let earlier = Instant::try_new(1609459200000000000 - 3600_000_000_000).unwrap();
/// let duration = earlier.until(&instant, Default::default()).unwrap();
/// assert_eq!(duration.seconds(), 3600);
/// ```
///
/// ### Instant precision and limits
///
/// ```rust
/// use temporal_rs::Instant;
///
/// // Instants have well-defined limits based on approximately 100 million days before/after Unix epoch
/// let max_ns = 8_640_000_000_000_000_000_000i128;  // ~100M days * 24 * 60 * 60 * 1e9
/// let min_ns = -max_ns;
///
/// let max_instant = Instant::try_new(max_ns).unwrap();
/// let min_instant = Instant::try_new(min_ns).unwrap();
///
/// // Values outside the range will fail
/// assert!(Instant::try_new(max_ns + 1).is_err());
/// assert!(Instant::try_new(min_ns - 1).is_err());
/// ```
///
/// ### Converting to ZonedDateTime (requires provider)
///
/// ```rust,ignore
/// use temporal_rs::{Instant, TimeZone, Calendar};
///
/// let instant = Instant::try_new(1609459200000000000).unwrap();
/// let timezone = TimeZone::try_from_str("America/New_York").unwrap();
///
/// // Convert to a zoned date-time for display in local time
/// let zdt = instant.to_zoned_date_time_iso(timezone);
/// assert_eq!(zdt.timezone().identifier(), "America/New_York");
/// assert_eq!(zdt.calendar().identifier(), "iso8601");
/// ```
///
/// ### Rounding instants
///
/// ```rust
/// use temporal_rs::{Instant, options::{RoundingOptions, Unit}};
///
/// let instant = Instant::try_new(1609459245123456789).unwrap(); // some precise moment
///
/// let mut opts = RoundingOptions::default();
/// opts.smallest_unit = Some(Unit::Second);
/// let rounded = instant.round(opts).unwrap();
///
/// // Rounded to the nearest second
/// assert_eq!(rounded.epoch_nanoseconds().as_i128() % 1_000_000_000, 0);
/// ```
///
/// ## Reference
///
/// For more information, see the [MDN documentation][mdn-instant].
///
/// [mdn-instant]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Temporal/Instant
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct Instant(EpochNanoseconds);

impl From<EpochNanoseconds> for Instant {
    fn from(value: EpochNanoseconds) -> Self {
        Self(value)
    }
}

// ==== Private API ====

impl Instant {
    /// Adds a `TimeDuration` to the current `Instant`.
    ///
    /// Temporal-Proposal equivalent: `AddInstant`.
    pub(crate) fn add_to_instant(&self, duration: &TimeDuration) -> TemporalResult<Self> {
        // 1. Let result be AddTimeDurationToEpochNanoseconds(timeDuration, epochNanoseconds).
        let result = self.epoch_nanoseconds().0 + duration.0;
        let ns = EpochNanoseconds::from(result);
        // 2. If IsValidEpochNanoseconds(result) is false, throw a RangeError exception.
        ns.check_validity()?;
        // 3. Return result.
        Ok(Self::from(ns))
    }

    /// 8.5.10 AddDurationToInstant ( operation, instant, temporalDurationLike )
    pub(crate) fn add_duration_to_instant(&self, duration: &Duration) -> TemporalResult<Self> {
        // 3. Let largestUnit be DefaultTemporalLargestUnit(duration).
        let largest_unit = duration.default_largest_unit();
        // 4. If TemporalUnitCategory(largestUnit) is date, throw a RangeError exception.
        if largest_unit.is_date_unit() {
            return Err(TemporalError::range().with_enum(ErrorMessage::LargestUnitCannotBeDateUnit));
        }
        // 5. Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
        let internal_duration = InternalDurationRecord::from_duration_with_24_hour_days(duration)?;
        // 6. Let ns be ? AddInstant(instant.[[EpochNanoseconds]], internalDuration.[[Time]]).
        // 7. Return ! CreateTemporalInstant(ns).
        self.add_to_instant(&internal_duration.normalized_time_duration())
    }

    /// `temporal_rs` equivalent of `DifferenceInstant`
    pub(crate) fn diff_instant_internal(
        &self,
        other: &Self,
        resolved_options: ResolvedRoundingOptions,
    ) -> TemporalResult<InternalDurationRecord> {
        let diff = TimeDuration::from_nanosecond_difference(other.as_i128(), self.as_i128())?;
        let normalized_time = diff.round(resolved_options)?;
        InternalDurationRecord::new(DateDuration::default(), normalized_time)
    }

    // TODO: Add test for `diff_instant`.
    // NOTE(nekevss): As the below is internal, op will be left as a boolean
    // with a `since` op being true and `until` being false.
    /// Internal operation to handle `since` and `until` difference ops.
    pub(crate) fn diff_instant(
        &self,
        op: DifferenceOperation,
        other: &Self,
        options: DifferenceSettings,
    ) -> TemporalResult<Duration> {
        // 1. If operation is since, let sign be -1. Otherwise, let sign be 1.
        // 2. Set other to ? ToTemporalInstant(other).
        // 3. Let resolvedOptions be ? SnapshotOwnProperties(? GetOptionsObject(options), null).
        // 4. Let settings be ? GetDifferenceSettings(operation, resolvedOptions, time, « », "nanosecond", "second").
        let resolved_options = ResolvedRoundingOptions::from_diff_settings(
            options,
            op,
            UnitGroup::Time,
            Unit::Second,
            Unit::Nanosecond,
        )?;

        // Below are the steps from Difference Instant.
        // 5. Let diffRecord be DifferenceInstant(instant.[[Nanoseconds]], other.[[Nanoseconds]],
        // settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
        let internal_record = self.diff_instant_internal(other, resolved_options)?;

        let result = Duration::from_internal(internal_record, resolved_options.largest_unit)?;

        // 6. Let norm be diffRecord.[[TimeDuration]].
        // 7. Let result be ! BalanceTimeDuration(norm, settings.[[LargestUnit]]).
        // 8. Return ! CreateTemporalDuration(0, 0, 0, 0, sign × result.[[Hours]], sign × result.[[Minutes]], sign × result.[[Seconds]], sign × result.[[Milliseconds]], sign × result.[[Microseconds]], sign × result.[[Nanoseconds]]).
        match op {
            DifferenceOperation::Until => Ok(result),
            DifferenceOperation::Since => Ok(result.negated()),
        }
    }

    /// Rounds a current `Instant` given the resolved options, returning a `BigInt` result.
    pub(crate) fn round_instant(
        &self,
        resolved_options: ResolvedRoundingOptions,
    ) -> TemporalResult<i128> {
        let increment = resolved_options.increment.as_extended_increment();
        let increment = match resolved_options.smallest_unit {
            Unit::Hour => increment
                .checked_mul(NonZeroU128::new(NANOSECONDS_PER_HOUR as u128).temporal_unwrap()?),
            Unit::Minute => increment
                .checked_mul(NonZeroU128::new(NANOSECONDS_PER_MINUTE as u128).temporal_unwrap()?),
            Unit::Second => increment
                .checked_mul(NonZeroU128::new(NANOSECONDS_PER_SECOND as u128).temporal_unwrap()?),
            Unit::Millisecond => {
                increment.checked_mul(NonZeroU128::new(1_000_000).temporal_unwrap()?)
            }
            Unit::Microsecond => increment.checked_mul(NonZeroU128::new(1_000).temporal_unwrap()?),
            Unit::Nanosecond => Some(increment),
            _ => {
                return Err(TemporalError::range()
                    .with_message("Invalid unit provided for Instant::round."))
            }
        };

        // NOTE: Potentially remove the below and just `temporal_unwrap`
        let Some(increment) = increment else {
            return Err(TemporalError::range().with_message("Increment exceeded a valid range."));
        };

        let rounded = IncrementRounder::<i128>::from_signed_num(self.as_i128(), increment)?
            .round_as_if_positive(resolved_options.rounding_mode);

        Ok(rounded)
    }

    // Utility for converting `Instant` to `i128`.
    pub fn as_i128(&self) -> i128 {
        self.epoch_nanoseconds().0
    }
}

// ==== Public API ====

impl Instant {
    /// Create a new validated `Instant`.
    #[inline]
    pub fn try_new(nanoseconds: i128) -> TemporalResult<Self> {
        let ns = EpochNanoseconds::from(nanoseconds);
        ns.check_validity()?;
        Ok(Self::from(ns))
    }

    /// Creates a new `Instant` from the provided Epoch millisecond value.
    pub fn from_epoch_milliseconds(epoch_milliseconds: i64) -> TemporalResult<Self> {
        // Input at most is `i64::MAX`. This means guarantees that the
        // transition into nanoseconds MUST be in range of `i128`
        let epoch_nanos = (epoch_milliseconds as i128) * 1_000_000;
        Self::try_new(epoch_nanos)
    }

    // Converts a UTF-8 encoded string into a `Instant`.
    pub fn from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let ixdtf_record = parse_instant(s)?;

        // Find the offset
        let ns_offset = match ixdtf_record.offset {
            UtcOffsetRecordOrZ::Offset(offset) => {
                let ns = offset
                    .fraction()
                    .and_then(|x| x.to_nanoseconds())
                    .unwrap_or(0);
                (offset.hour() as i64 * NANOSECONDS_PER_HOUR
                    + i64::from(offset.minute()) * NANOSECONDS_PER_MINUTE
                    + i64::from(offset.second().unwrap_or(0)) * NANOSECONDS_PER_SECOND
                    + i64::from(ns))
                    * offset.sign() as i64
            }
            UtcOffsetRecordOrZ::Z => 0,
        };

        let time_nanoseconds = ixdtf_record
            .time
            .fraction
            .and_then(|x| x.to_nanoseconds())
            .unwrap_or(0);
        let (millisecond, rem) = time_nanoseconds.div_rem_euclid(&1_000_000);
        let (microsecond, nanosecond) = rem.div_rem_euclid(&1_000);

        let balanced = IsoDateTime::balance(
            ixdtf_record.date.year,
            ixdtf_record.date.month.into(),
            ixdtf_record.date.day.into(),
            ixdtf_record.time.hour.into(),
            ixdtf_record.time.minute.into(),
            ixdtf_record.time.second.clamp(0, 59).into(),
            millisecond.into(),
            microsecond.into(),
            i128::from(nanosecond) - i128::from(ns_offset),
        );

        let nanoseconds = balanced.as_nanoseconds();

        nanoseconds.check_validity()?;

        Ok(Self(nanoseconds))
    }

    /// Adds a `Duration` to the current `Instant`, returning an error if the `Duration`
    /// contains a `DateDuration`.
    #[inline]
    pub fn add(&self, duration: &Duration) -> TemporalResult<Self> {
        self.add_duration_to_instant(duration)
    }

    /// Subtract a `Duration` to the current `Instant`, returning an error if the `Duration`
    /// contains a `DateDuration`.
    #[inline]
    pub fn subtract(&self, duration: &Duration) -> TemporalResult<Self> {
        self.add_duration_to_instant(&duration.negated())
    }

    /// Returns a `Duration` representing the duration since provided `Instant`
    #[inline]
    pub fn since(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff_instant(DifferenceOperation::Since, other, settings)
    }

    /// Returns a `Duration` representing the duration until provided `Instant`
    #[inline]
    pub fn until(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff_instant(DifferenceOperation::Until, other, settings)
    }

    /// Returns an `Instant` by rounding the current `Instant` according to the provided settings.
    #[inline]
    pub fn round(&self, options: RoundingOptions) -> TemporalResult<Self> {
        let resolved_options = ResolvedRoundingOptions::from_instant_options(options)?;

        let round_result = self.round_instant(resolved_options)?;
        Self::try_new(round_result)
    }

    /// Returns the `epochMilliseconds` value for this `Instant`.
    #[inline]
    #[must_use]
    pub fn epoch_milliseconds(&self) -> i64 {
        self.as_i128().div_euclid(1_000_000) as i64
    }

    /// Returns the [`EpochNanoseconds`] value for this `Instant`.
    #[inline]
    #[must_use]
    pub fn epoch_nanoseconds(&self) -> &EpochNanoseconds {
        &self.0
    }

    /// Returns a [`ZonedDateTime`] for the current `Instant`.
    #[inline]
    pub fn to_zoned_date_time_iso_with_provider(
        &self,
        time_zone: TimeZone,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<ZonedDateTime> {
        ZonedDateTime::new_unchecked_with_provider(*self, time_zone, Calendar::ISO, provider)
    }
}

// ==== Instant Provider API ====

impl Instant {
    /// Returns an RFC9557 IXDTF string representing the current `Instant`.
    pub fn to_ixdtf_string_with_provider(
        &self,
        timezone: Option<TimeZone>,
        options: ToStringRoundingOptions,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<String> {
        self.to_ixdtf_writeable_with_provider(timezone, options, provider)
            .map(|x| x.write_to_string().into())
    }

    /// Returns a [`Writeable`] for formatting the current `Instant` in RFC9557's IXDTF.
    pub fn to_ixdtf_writeable_with_provider(
        &self,
        timezone: Option<TimeZone>,
        options: ToStringRoundingOptions,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<impl Writeable + '_> {
        let resolved_options = options.resolve()?;
        let round = self.round_instant(ResolvedRoundingOptions::from_to_string_options(
            &resolved_options,
        ))?;
        let rounded_instant = Instant::try_new(round)?;

        let mut ixdtf = IxdtfStringBuilder::default();
        let datetime = if let Some(timezone) = timezone {
            let datetime = timezone.get_iso_datetime_for(&rounded_instant, provider)?;
            let nanoseconds = timezone.get_offset_nanos_for(rounded_instant.as_i128(), provider)?;
            let (sign, hour, minute) = nanoseconds_to_formattable_offset_minutes(nanoseconds)?;
            ixdtf = ixdtf.with_minute_offset(sign, hour, minute, DisplayOffset::Auto);
            datetime
        } else {
            ixdtf = ixdtf.with_z(DisplayOffset::Auto);
            TimeZone::utc_with_provider(provider)
                .get_iso_datetime_for(&rounded_instant, provider)?
        };
        let builder = ixdtf
            .with_date(datetime.date)
            .with_time(datetime.time, resolved_options.precision);

        Ok(builder)
    }
}

// ==== Utility Functions ====

impl FromStr for Instant {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::from_utf8(s.as_bytes())
    }
}

// ==== Instant Tests ====

#[cfg(test)]
mod tests {

    use core::str::FromStr;

    use crate::{
        builtins::{core::Instant, duration::duration_sign},
        options::{DifferenceSettings, RoundingMode, Unit},
        partial::PartialDuration,
        unix_time::EpochNanoseconds,
        Duration, NS_MAX_INSTANT, NS_MIN_INSTANT,
    };

    #[test]
    #[allow(clippy::float_cmp)]
    fn max_and_minimum_instant_bounds() {
        // This test is primarily to assert that the `expect` in the epoch methods is
        // valid, i.e., a valid instant is within the range of an f64.
        let max = NS_MAX_INSTANT;
        let min = NS_MIN_INSTANT;
        let max_instant = Instant::try_new(max).unwrap();
        let min_instant = Instant::try_new(min).unwrap();

        assert_eq!(max_instant.epoch_nanoseconds().0, max);
        assert_eq!(min_instant.epoch_nanoseconds().0, min);

        let max_plus_one = NS_MAX_INSTANT + 1;
        let min_minus_one = NS_MIN_INSTANT - 1;

        assert!(Instant::try_new(max_plus_one).is_err());
        assert!(Instant::try_new(min_minus_one).is_err());
    }

    #[test]
    fn max_min_epoch_millseconds() {
        // Assert the casting is valid.
        let max = NS_MAX_INSTANT;
        let min = NS_MIN_INSTANT;
        let max_instant = Instant::try_new(max).unwrap();
        let min_instant = Instant::try_new(min).unwrap();

        // Assert max and min are valid for casting.
        assert_eq!(
            max_instant.epoch_milliseconds(),
            max.div_euclid(1_000_000) as i64
        );
        assert_eq!(
            min_instant.epoch_milliseconds(),
            min.div_euclid(1_000_000) as i64
        );
        // Assert the max and min are not being truncated.
        assert_ne!(max_instant.epoch_milliseconds(), i64::MAX);
        assert_ne!(max_instant.epoch_milliseconds(), i64::MIN);
    }

    #[test]
    fn instant_parsing_limits() {
        // valid cases
        let valid_str = "-271821-04-20T00:00Z";
        let instant = Instant::from_str(valid_str).unwrap();
        assert_eq!(
            instant,
            Instant::from(EpochNanoseconds(-8640000000000000000000))
        );

        let valid_str = "-271821-04-19T23:00-01:00";
        let instant = Instant::from_str(valid_str).unwrap();
        assert_eq!(
            instant,
            Instant::from(EpochNanoseconds(-8640000000000000000000))
        );

        let valid_str = "-271821-04-19T00:00:00.000000001-23:59:59.999999999";
        let instant = Instant::from_str(valid_str).unwrap();
        assert_eq!(
            instant,
            Instant::from(EpochNanoseconds(-8640000000000000000000))
        );

        let valid_str = "+275760-09-13T00:00Z";
        let instant = Instant::from_str(valid_str).unwrap();
        assert_eq!(
            instant,
            Instant::from(EpochNanoseconds(8640000000000000000000))
        );

        // invalid cases
        let invalid_str = "-271821-04-19T00:00Z";
        let instant = Instant::from_str(invalid_str);
        assert!(instant.is_err());

        let invalid_str = "-271821-04-19T23:59:59.999999999Z";
        let instant = Instant::from_str(invalid_str);
        assert!(instant.is_err());

        let invalid_str = "-271821-04-19T23:00-00:59:59.999999999";
        let instant = Instant::from_str(invalid_str);
        assert!(instant.is_err());
    }

    #[test]
    fn basic_instant_until() {
        let init_diff_setting = |unit: Unit| -> DifferenceSettings {
            DifferenceSettings {
                largest_unit: Some(Unit::Hour),
                rounding_mode: Some(RoundingMode::Ceil),
                increment: None,
                smallest_unit: Some(unit),
            }
        };

        let assert_duration = |td: Duration, expected: (i64, i64, i64, i64, i128, i128)| {
            assert_eq!(
                td,
                Duration {
                    sign: duration_sign(&[
                        expected.0,
                        expected.1,
                        expected.2,
                        expected.3,
                        expected.4 as i64,
                        expected.5 as i64
                    ]),
                    hours: expected.0.unsigned_abs(),
                    minutes: expected.1.unsigned_abs(),
                    seconds: expected.2.unsigned_abs(),
                    milliseconds: expected.3.unsigned_abs(),
                    microseconds: expected.4.unsigned_abs(),
                    nanoseconds: expected.5.unsigned_abs(),
                    ..Default::default()
                }
            )
        };

        let earlier = Instant::try_new(
            217_178_610_123_456_789, /* 1976-11-18T15:23:30.123456789Z */
        )
        .unwrap();
        let later = Instant::try_new(
            1_572_345_998_271_986_289, /* 2019-10-29T10:46:38.271986289Z */
        )
        .unwrap();

        let positive_result = earlier
            .until(&later, init_diff_setting(Unit::Hour))
            .unwrap();
        assert_duration(positive_result, (376436, 0, 0, 0, 0, 0));
        let negative_result = later
            .until(&earlier, init_diff_setting(Unit::Hour))
            .unwrap();
        assert_duration(negative_result, (-376435, 0, 0, 0, 0, 0));

        let positive_result = earlier
            .until(&later, init_diff_setting(Unit::Minute))
            .unwrap();
        assert_duration(positive_result, (376435, 24, 0, 0, 0, 0));
        let negative_result = later
            .until(&earlier, init_diff_setting(Unit::Minute))
            .unwrap();
        assert_duration(negative_result, (-376435, -23, 0, 0, 0, 0));

        // ... Skip to lower units ...

        let positive_result = earlier
            .until(&later, init_diff_setting(Unit::Microsecond))
            .unwrap();
        assert_duration(positive_result, (376435, 23, 8, 148, 530, 0));
        let negative_result = later
            .until(&earlier, init_diff_setting(Unit::Microsecond))
            .unwrap();
        assert_duration(negative_result, (-376435, -23, -8, -148, -529, 0));

        let positive_result = earlier
            .until(&later, init_diff_setting(Unit::Nanosecond))
            .unwrap();
        assert_duration(positive_result, (376435, 23, 8, 148, 529, 500));
        let negative_result = later
            .until(&earlier, init_diff_setting(Unit::Nanosecond))
            .unwrap();
        assert_duration(negative_result, (-376435, -23, -8, -148, -529, -500));
    }

    #[test]
    fn basic_instant_since() {
        let init_diff_setting = |unit: Unit| -> DifferenceSettings {
            DifferenceSettings {
                largest_unit: Some(Unit::Hour),
                rounding_mode: Some(RoundingMode::Ceil),
                increment: None,
                smallest_unit: Some(unit),
            }
        };

        let assert_duration = |td: Duration, expected: (i64, i64, i64, i64, i128, i128)| {
            assert_eq!(
                td,
                Duration {
                    sign: duration_sign(&[
                        expected.0,
                        expected.1,
                        expected.2,
                        expected.3,
                        expected.4 as i64,
                        expected.5 as i64
                    ]),
                    hours: expected.0.unsigned_abs(),
                    minutes: expected.1.unsigned_abs(),
                    seconds: expected.2.unsigned_abs(),
                    milliseconds: expected.3.unsigned_abs(),
                    microseconds: expected.4.unsigned_abs(),
                    nanoseconds: expected.5.unsigned_abs(),
                    ..Default::default()
                }
            )
        };

        let earlier = Instant::try_new(
            217_178_610_123_456_789, /* 1976-11-18T15:23:30.123456789Z */
        )
        .unwrap();
        let later = Instant::try_new(
            1_572_345_998_271_986_289, /* 2019-10-29T10:46:38.271986289Z */
        )
        .unwrap();

        let positive_result = later
            .since(&earlier, init_diff_setting(Unit::Hour))
            .unwrap();
        assert_duration(positive_result, (376436, 0, 0, 0, 0, 0));
        let negative_result = earlier
            .since(&later, init_diff_setting(Unit::Hour))
            .unwrap();
        assert_duration(negative_result, (-376435, 0, 0, 0, 0, 0));

        let positive_result = later
            .since(&earlier, init_diff_setting(Unit::Minute))
            .unwrap();
        assert_duration(positive_result, (376435, 24, 0, 0, 0, 0));
        let negative_result = earlier
            .since(&later, init_diff_setting(Unit::Minute))
            .unwrap();
        assert_duration(negative_result, (-376435, -23, 0, 0, 0, 0));

        // ... Skip to lower units ...

        let positive_result = later
            .since(&earlier, init_diff_setting(Unit::Microsecond))
            .unwrap();
        assert_duration(positive_result, (376435, 23, 8, 148, 530, 0));
        let negative_result = earlier
            .since(&later, init_diff_setting(Unit::Microsecond))
            .unwrap();
        assert_duration(negative_result, (-376435, -23, -8, -148, -529, 0));

        let positive_result = later
            .since(&earlier, init_diff_setting(Unit::Nanosecond))
            .unwrap();
        assert_duration(positive_result, (376435, 23, 8, 148, 529, 500));
        let negative_result = earlier
            .since(&later, init_diff_setting(Unit::Nanosecond))
            .unwrap();
        assert_duration(negative_result, (-376435, -23, -8, -148, -529, -500));
    }

    // /test/built-ins/Temporal/Instant/prototype/add/cross-epoch.js
    #[cfg(feature = "tzdb")]
    #[test]
    fn instant_add_across_epoch() {
        use crate::builtins::core::Duration;
        use crate::{
            options::ToStringRoundingOptions, partial::PartialDuration, tzdb::FsTzdbProvider,
        };
        use core::str::FromStr;

        let instant = Instant::from_str("1969-12-25T12:23:45.678901234Z").unwrap();
        let one = instant
            .subtract(
                &Duration::from_partial_duration(PartialDuration {
                    hours: Some(240.into()),
                    nanoseconds: Some(800.into()),
                    ..Default::default()
                })
                .unwrap(),
            )
            .unwrap();
        let two = instant
            .add(
                &Duration::from_partial_duration(PartialDuration {
                    hours: Some(240.into()),
                    nanoseconds: Some(800.into()),
                    ..Default::default()
                })
                .unwrap(),
            )
            .unwrap();
        let three = two
            .subtract(
                &Duration::from_partial_duration(PartialDuration {
                    hours: Some(480.into()),
                    nanoseconds: Some(1600.into()),
                    ..Default::default()
                })
                .unwrap(),
            )
            .unwrap();
        let four = one
            .add(
                &Duration::from_partial_duration(PartialDuration {
                    hours: Some(480.into()),
                    nanoseconds: Some(1600.into()),
                    ..Default::default()
                })
                .unwrap(),
            )
            .unwrap();

        let one_comp = Instant::from_str("1969-12-15T12:23:45.678900434Z").unwrap();
        let two_comp = Instant::from_str("1970-01-04T12:23:45.678902034Z").unwrap();

        // Assert the comparisons all hold.
        assert_eq!(one, one_comp);
        assert_eq!(two, two_comp);
        assert_eq!(three, one);
        assert_eq!(four, two);

        // Assert the to_string is valid.
        let provider = &FsTzdbProvider::default();
        let inst_string = instant
            .to_ixdtf_string_with_provider(None, ToStringRoundingOptions::default(), provider)
            .unwrap();
        let one_string = one
            .to_ixdtf_string_with_provider(None, ToStringRoundingOptions::default(), provider)
            .unwrap();
        let two_string = two
            .to_ixdtf_string_with_provider(None, ToStringRoundingOptions::default(), provider)
            .unwrap();

        assert_eq!(&inst_string, "1969-12-25T12:23:45.678901234Z");
        assert_eq!(&one_string, "1969-12-15T12:23:45.678900434Z");
        assert_eq!(&two_string, "1970-01-04T12:23:45.678902034Z");
    }

    // Adapted from add[subtract]/minimum-maximum-instant.js
    #[test]
    fn instant_add_subtract_max_min() {
        let min = Instant::try_new(-86_40000_00000_00000_00000).unwrap();
        let max = Instant::try_new(86_40000_00000_00000_00000).unwrap();

        let zero = Duration::from_partial_duration(PartialDuration::default().with_nanoseconds(0))
            .unwrap();
        let one = Duration::from_partial_duration(PartialDuration::default().with_nanoseconds(1))
            .unwrap();
        let neg_one =
            Duration::from_partial_duration(PartialDuration::default().with_nanoseconds(-1))
                .unwrap();
        // Adding/subtracting zero does not cross max/min boundary.
        assert_eq!(min.add(&zero).unwrap(), min);
        assert_eq!(min.subtract(&zero).unwrap(), min);

        // Addition or subtraction crosses boundary.
        assert!(max.add(&one).is_err());
        assert!(max.subtract(&neg_one).is_err());
        assert!(min.add(&neg_one).is_err());
        assert!(min.subtract(&one).is_err());

        let insanely_large_partial =
            PartialDuration::default().with_nanoseconds(-86_40000_00000_00000_00000 * 2);
        let large_duration = Duration::from_partial_duration(insanely_large_partial).unwrap();
        assert_eq!(min.subtract(&large_duration).unwrap(), max);
        assert_eq!(max.add(&large_duration).unwrap(), min);
    }
}
