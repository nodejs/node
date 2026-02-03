//! This module implements `Time` and any directly related algorithms.

use crate::{
    builtins::{
        core::{DateDuration, Duration},
        duration::normalized::InternalDurationRecord,
    },
    iso::IsoTime,
    options::{
        DifferenceOperation, DifferenceSettings, Overflow, ResolvedRoundingOptions,
        RoundingOptions, ToStringRoundingOptions, Unit, UnitGroup,
    },
    parsers::{parse_time, IxdtfStringBuilder},
    TemporalError, TemporalResult,
};
use alloc::string::String;
use core::str::FromStr;
use writeable::Writeable;

use super::{duration::normalized::TimeDuration, PlainDateTime};

// TODO: add a PartialSignedTime

/// A `PartialTime` represents partially filled `Time` fields.
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct PartialTime {
    // A potentially set `hour` field.
    pub hour: Option<u8>,
    // A potentially set `minute` field.
    pub minute: Option<u8>,
    // A potentially set `second` field.
    pub second: Option<u8>,
    // A potentially set `millisecond` field.
    pub millisecond: Option<u16>,
    // A potentially set `microsecond` field.
    pub microsecond: Option<u16>,
    // A potentially set `nanosecond` field.
    pub nanosecond: Option<u16>,
}

impl PartialTime {
    pub fn is_empty(&self) -> bool {
        *self == Self::default()
    }
}

/// Convenience methods for building a `PartialTime`
impl PartialTime {
    pub const fn new() -> Self {
        Self {
            hour: None,
            minute: None,
            second: None,
            millisecond: None,
            microsecond: None,
            nanosecond: None,
        }
    }

    pub const fn with_hour(mut self, hour: Option<u8>) -> Self {
        self.hour = hour;
        self
    }

    pub const fn with_minute(mut self, minute: Option<u8>) -> Self {
        self.minute = minute;
        self
    }

    pub const fn with_second(mut self, second: Option<u8>) -> Self {
        self.second = second;
        self
    }

    pub const fn with_millisecond(mut self, millisecond: Option<u16>) -> Self {
        self.millisecond = millisecond;
        self
    }

    pub const fn with_microsecond(mut self, microsecond: Option<u16>) -> Self {
        self.microsecond = microsecond;
        self
    }

    pub const fn with_nanosecond(mut self, nanosecond: Option<u16>) -> Self {
        self.nanosecond = nanosecond;
        self
    }
}

/// The native Rust implementation of `Temporal.PlainTime`.
///
/// Represents a time of day such as "14:30:45.123" without any date or timezone
/// information. Contains hour, minute, second, and subsecond components with
/// nanosecond precision.
///
/// Ideal for representing recurring daily events, schedules, or any time value
/// where the specific date is not relevant.
///
/// ## Examples
///
/// ### Creating time values
///
/// ```rust
/// use temporal_rs::PlainTime;
///
/// // High-precision time
/// let precise_time = PlainTime::try_new(
///     14, 30, 45,      // 2:30:45 PM
///     123, 456, 789    // subsecond components
/// ).unwrap();
/// assert_eq!(precise_time.hour(), 14);
/// assert_eq!(precise_time.millisecond(), 123);
///
/// // Simple time without subseconds
/// let simple = PlainTime::try_new(9, 0, 0, 0, 0, 0).unwrap();
/// assert_eq!(simple.hour(), 9);
/// ```
///
/// ### Parsing ISO 8601 time strings
///
/// ```rust
/// use temporal_rs::PlainTime;
/// use core::str::FromStr;
///
/// let time = PlainTime::from_str("14:30:45.123456789").unwrap();
/// assert_eq!(time.hour(), 14);
/// assert_eq!(time.minute(), 30);
/// assert_eq!(time.second(), 45);
/// assert_eq!(time.millisecond(), 123);
/// assert_eq!(time.microsecond(), 456);
/// assert_eq!(time.nanosecond(), 789);
///
/// // Also supports time-only portions of ISO 8601 strings
/// let time = PlainTime::from_str("T09:15:30").unwrap();
/// assert_eq!(time.hour(), 9);
/// assert_eq!(time.minute(), 15);
/// assert_eq!(time.second(), 30);
/// ```
///
/// ### Time arithmetic
///
/// ```rust
/// use temporal_rs::{PlainTime, Duration};
/// use core::str::FromStr;
///
/// let time = PlainTime::from_str("12:30:00").unwrap();
///
/// // Add 2 hours and 30 minutes
/// let later = time.add(&Duration::from_str("PT2H30M").unwrap()).unwrap();
/// assert_eq!(later.hour(), 15);
/// assert_eq!(later.minute(), 0);
/// assert_eq!(later.second(), 0);
///
/// // Calculate difference between times
/// let earlier = PlainTime::from_str("10:00:00").unwrap();
/// let duration = earlier.until(&time, Default::default()).unwrap();
/// assert_eq!(duration.hours(), 2);
/// assert_eq!(duration.minutes(), 30);
/// ```
///
/// ### Working with partial time fields
///
/// ```rust
/// use temporal_rs::{PlainTime, partial::PartialTime};
/// use core::str::FromStr;
///
/// let time = PlainTime::from_str("12:30:45").unwrap();
///
/// // Change only the minute
/// let partial = PartialTime::new().with_minute(Some(15));
/// let modified = time.with(partial, None).unwrap();
/// assert_eq!(modified.hour(), 12); // unchanged
/// assert_eq!(modified.minute(), 15); // changed
/// assert_eq!(modified.second(), 45); // unchanged
/// ```
///
/// ### Rounding times
///
/// ```rust
/// use temporal_rs::{PlainTime, options::{Unit, RoundingOptions}};
/// use core::str::FromStr;
///
/// let time = PlainTime::from_str("14:23:47.789").unwrap();
///
/// let mut options = RoundingOptions::default();
/// options.smallest_unit = Some(Unit::Minute);
/// // Round to nearest minute
/// let rounded = time.round(options).unwrap();
/// assert_eq!(rounded.hour(), 14);
/// assert_eq!(rounded.minute(), 24);
/// assert_eq!(rounded.second(), 0);
/// assert_eq!(rounded.millisecond(), 0);
/// ```
///
/// ## Reference
///
/// For more information, see the [MDN documentation][mdn-plaintime].
///
/// [mdn-plaintime]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Temporal/PlainTime
#[non_exhaustive]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct PlainTime {
    pub(crate) iso: IsoTime,
}

// ==== Private API ====

impl PlainTime {
    #[inline]
    #[must_use]
    /// Creates a new unvalidated `Time`.
    pub(crate) fn new_unchecked(iso: IsoTime) -> Self {
        Self { iso }
    }

    /// Specification equivalent to `4.5.15 AddTime ( time, timeDuration )`
    ///
    /// Spec: <https://tc39.es/proposal-temporal/#sec-temporal-addtime>
    pub(crate) fn add_normalized_time_duration(&self, norm: TimeDuration) -> (i64, Self) {
        // 1. Set second to second + TimeDurationSeconds(norm).
        let second = i64::from(self.second()) + norm.seconds();
        // 2. Set nanosecond to nanosecond + TimeDurationSubseconds(norm).
        let nanosecond = i32::from(self.nanosecond()) + norm.subseconds();
        // 3. Return BalanceTime(hour, minute, second, millisecond, microsecond, nanosecond).
        let (day, balance_result) = IsoTime::balance(
            self.hour().into(),
            self.minute().into(),
            second,
            self.millisecond().into(),
            self.microsecond().into(),
            nanosecond.into(),
        );

        (day, Self::new_unchecked(balance_result))
    }

    /// Adds a `Duration` to the current `Time`.
    ///
    /// Spec Equivalent: `AddDurationToOrSubtractDurationFromPlainTime`.
    pub(crate) fn add_to_time(&self, duration: &Duration) -> TemporalResult<Self> {
        let (_, result) = IsoTime::balance(
            i64::from(self.hour()).saturating_add(duration.hours()),
            i64::from(self.minute()).saturating_add(duration.minutes()),
            i64::from(self.second()).saturating_add(duration.seconds()),
            i64::from(self.millisecond()).saturating_add(duration.milliseconds()),
            i128::from(self.microsecond()).saturating_add(duration.microseconds()),
            i128::from(self.nanosecond()).saturating_add(duration.nanoseconds()),
        );

        // NOTE (nekevss): IsoTime::balance should never return an invalid `IsoTime`

        Ok(Self::new_unchecked(result))
    }

    // TODO: Migrate to
    /// Performs a desired difference op between two `Time`'s, returning the resulting `Duration`.
    pub(crate) fn diff_time(
        &self,
        op: DifferenceOperation,
        other: &PlainTime,
        settings: DifferenceSettings,
    ) -> TemporalResult<Duration> {
        // 1. If operation is SINCE, let sign be -1. Otherwise, let sign be 1.
        // 2. Set other to ? ToTemporalTime(other).
        // 3. Let resolvedOptions be ? SnapshotOwnProperties(? GetOptionsObject(options), null).
        // 4. Let settings be ? GetDifferenceSettings(operation, resolvedOptions, TIME, « », "nanosecond", "hour").
        let resolved = ResolvedRoundingOptions::from_diff_settings(
            settings,
            op,
            UnitGroup::Time,
            Unit::Hour,
            Unit::Nanosecond,
        )?;
        // 4. Let timeDuration be DifferenceTime(temporalTime.[[Time]], other.[[Time]]).
        let mut normalized_time = self.iso.diff(&other.iso);
        // 5. Set timeDuration to ! RoundTimeDuration(timeDuration, settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
        normalized_time = normalized_time.round(resolved)?;
        // 6. Let duration be CombineDateAndTimeDuration(ZeroDateDuration(), timeDuration).
        let duration = InternalDurationRecord::combine(DateDuration::default(), normalized_time);
        // 7. Let result be ! TemporalDurationFromInternal(duration, settings.[[LargestUnit]]).
        let result = Duration::from_internal(duration, resolved.largest_unit)?;
        // 8. If operation is since, set result to CreateNegatedTemporalDuration(result).
        // 9. Return result.
        // 8. Return ! CreateTemporalDuration(0, 0, 0, 0, sign × result.[[Hours]], sign × result.[[Minutes]], sign × result.[[Seconds]], sign × result.[[Milliseconds]], sign × result.[[Microseconds]], sign × result.[[Nanoseconds]]).
        match op {
            DifferenceOperation::Until => Ok(result),
            DifferenceOperation::Since => Ok(result.negated()),
        }
    }
}

// ==== Public API ====

impl PlainTime {
    /// Creates a new `PlainTime`, constraining any field into a valid range.
    ///
    /// ```rust
    /// use temporal_rs::PlainTime;
    ///
    /// let time = PlainTime::new(23, 59, 59, 999, 999, 999).unwrap();
    ///
    /// let constrained_time = PlainTime::new(24, 59, 59, 999, 999, 999).unwrap();
    /// assert_eq!(time, constrained_time);
    /// ```
    pub fn new(
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
    ) -> TemporalResult<Self> {
        Self::new_with_overflow(
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            Overflow::Constrain,
        )
    }

    /// Creates a new `PlainTime`, rejecting any field that is not in a valid range.
    ///
    /// ```rust
    /// use temporal_rs::PlainTime;
    ///
    /// let time = PlainTime::try_new(23, 59, 59, 999, 999, 999).unwrap();
    ///
    /// let invalid_time = PlainTime::try_new(24, 59, 59, 999, 999, 999);
    /// assert!(invalid_time.is_err());
    /// ```
    pub fn try_new(
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
    ) -> TemporalResult<Self> {
        Self::new_with_overflow(
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            Overflow::Reject,
        )
    }

    /// Creates a new `PlainTime` with the provided [`Overflow`] option.
    #[inline]
    pub fn new_with_overflow(
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
        overflow: Overflow,
    ) -> TemporalResult<Self> {
        let time = IsoTime::new(
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            overflow,
        )?;
        Ok(Self::new_unchecked(time))
    }

    /// Creates a new `PlainTime` from a `PartialTime`.
    ///
    /// ```rust
    /// use temporal_rs::{partial::PartialTime, PlainTime};
    ///
    /// let partial_time = PartialTime {
    ///     hour: Some(22),
    ///     ..Default::default()
    /// };
    ///
    /// let time = PlainTime::from_partial(partial_time, None).unwrap();
    ///
    /// assert_eq!(time.hour(), 22);
    /// assert_eq!(time.minute(), 0);
    /// assert_eq!(time.second(), 0);
    /// assert_eq!(time.millisecond(), 0);
    /// assert_eq!(time.microsecond(), 0);
    /// assert_eq!(time.nanosecond(), 0);
    ///
    /// ```
    pub fn from_partial(partial: PartialTime, overflow: Option<Overflow>) -> TemporalResult<Self> {
        // NOTE: 4.5.12 ToTemporalTimeRecord requires one field to be set.
        if partial.is_empty() {
            return Err(TemporalError::r#type().with_message("PartialTime cannot be empty."));
        }

        let overflow = overflow.unwrap_or_default();
        let iso = IsoTime::default().with(partial, overflow)?;
        Ok(Self::new_unchecked(iso))
    }

    // Converts a UTF-8 encoded string into a `PlainTime`.
    pub fn from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let result = parse_time(s)?;
        let iso = IsoTime::from_time_record(result)?;
        Ok(Self::new_unchecked(iso))
    }

    /// Creates a new `PlainTime` using the current `PlainTime` fields as a fallback.
    ///
    /// ```rust
    /// use temporal_rs::{partial::PartialTime, PlainTime};
    ///
    /// let partial_time = PartialTime {
    ///     hour: Some(22),
    ///     ..Default::default()
    /// };
    ///
    /// let initial = PlainTime::try_new(15, 30, 12, 123, 456, 789).unwrap();
    ///
    /// let time = initial.with(partial_time, None).unwrap();
    ///
    /// assert_eq!(time.hour(), 22);
    /// assert_eq!(time.minute(), 30);
    /// assert_eq!(time.second(), 12);
    /// assert_eq!(time.millisecond(), 123);
    /// assert_eq!(time.microsecond(), 456);
    /// assert_eq!(time.nanosecond(), 789);
    ///
    /// ```
    pub fn with(&self, partial: PartialTime, overflow: Option<Overflow>) -> TemporalResult<Self> {
        // NOTE: 4.5.12 ToTemporalTimeRecord requires one field to be set.
        if partial.is_empty() {
            return Err(TemporalError::r#type().with_message("PartialTime cannot be empty."));
        }

        let iso = self
            .iso
            .with(partial, overflow.unwrap_or(Overflow::Constrain))?;
        Ok(Self::new_unchecked(iso))
    }

    /// Returns the internal `hour` field.
    #[inline]
    #[must_use]
    pub const fn hour(&self) -> u8 {
        self.iso.hour
    }

    /// Returns the internal `minute` field.
    #[inline]
    #[must_use]
    pub const fn minute(&self) -> u8 {
        self.iso.minute
    }

    /// Returns the internal `second` field.
    #[inline]
    #[must_use]
    pub const fn second(&self) -> u8 {
        self.iso.second
    }

    /// Returns the internal `millisecond` field.
    #[inline]
    #[must_use]
    pub const fn millisecond(&self) -> u16 {
        self.iso.millisecond
    }

    /// Returns the internal `microsecond` field.
    #[inline]
    #[must_use]
    pub const fn microsecond(&self) -> u16 {
        self.iso.microsecond
    }

    /// Returns the internal `nanosecond` field.
    #[inline]
    #[must_use]
    pub const fn nanosecond(&self) -> u16 {
        self.iso.nanosecond
    }

    /// Add a `Duration` to the current `Time`.
    pub fn add(&self, duration: &Duration) -> TemporalResult<Self> {
        self.add_to_time(duration)
    }

    /// Subtract a `Duration` to the current `Time`.
    pub fn subtract(&self, duration: &Duration) -> TemporalResult<Self> {
        self.add_to_time(&duration.negated())
    }

    /// Returns the `Duration` until the provided `Time` from the current `Time`.
    ///
    /// NOTE: `until` assumes the provided other time will occur in the future relative to the current.
    #[inline]
    pub fn until(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff_time(DifferenceOperation::Until, other, settings)
    }

    /// Returns the `Duration` since the provided `Time` from the current `Time`.
    ///
    /// NOTE: `since` assumes the provided other time is in the past relative to the current.
    #[inline]
    pub fn since(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff_time(DifferenceOperation::Since, other, settings)
    }

    // TODO (nekevss): optimize and test rounding_increment type (f64 vs. u64).
    /// Rounds the current `Time` according to provided options.
    pub fn round(&self, options: RoundingOptions) -> TemporalResult<Self> {
        let resolved = ResolvedRoundingOptions::from_time_options(options)?;
        let (_, result) = self.iso.round(resolved)?;

        Ok(Self::new_unchecked(result))
    }

    pub fn to_ixdtf_string(&self, options: ToStringRoundingOptions) -> TemporalResult<String> {
        self.to_ixdtf_writeable(options)
            .map(|x| x.write_to_string().into())
    }

    #[inline]
    pub fn to_ixdtf_writeable(
        &self,
        options: ToStringRoundingOptions,
    ) -> TemporalResult<impl Writeable + '_> {
        let resolved = options.resolve()?;
        let (_, result) = self
            .iso
            .round(ResolvedRoundingOptions::from_to_string_options(&resolved))?;
        let builder = IxdtfStringBuilder::default().with_time(result, resolved.precision);
        Ok(builder)
    }
}

impl From<PlainDateTime> for PlainTime {
    fn from(pdt: PlainDateTime) -> Self {
        pdt.to_plain_time()
    }
}

impl FromStr for PlainTime {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::from_utf8(s.as_bytes())
    }
}

// ==== Test land ====

#[cfg(test)]
mod tests {
    use core::str::FromStr;

    use crate::{
        builtins::core::Duration,
        iso::IsoTime,
        options::{DifferenceSettings, Overflow, RoundingIncrement, RoundingOptions, Unit},
    };
    use num_traits::FromPrimitive;

    use super::PlainTime;

    fn assert_time(result: PlainTime, values: (u8, u8, u8, u16, u16, u16)) {
        assert_eq!(
            result,
            PlainTime {
                iso: IsoTime {
                    hour: values.0,
                    minute: values.1,
                    second: values.2,
                    millisecond: values.3,
                    microsecond: values.4,
                    nanosecond: values.5,
                }
            }
        );
    }

    fn assert_duration(
        result: Duration,
        values: (i64, i64, i64, i64, i64, i64, i64, i64, i128, i128),
    ) {
        assert_eq!(
            (
                result.years(),
                result.months(),
                result.weeks(),
                result.days(),
                result.hours(),
                result.minutes(),
                result.seconds(),
                result.milliseconds(),
                result.microseconds(),
                result.nanoseconds()
            ),
            values
        )
    }

    fn options(unit: Unit, increment: f64) -> RoundingOptions {
        RoundingOptions {
            smallest_unit: Some(unit),
            increment: RoundingIncrement::try_from(increment).ok(),
            ..Default::default()
        }
    }

    #[test]
    fn from_str_cast_sanity_test() {
        let max = u32::MAX;
        let (millisecond, rem) = (max / 1_000_000, max % 1_000_000);
        let (microsecond, nanosecond) = (rem / 1_000, rem % 1_000);

        assert!(i32::from_u32(millisecond).is_some());
        assert!(i32::from_u32(microsecond).is_some());
        assert!(i32::from_u32(nanosecond).is_some());
    }

    #[test]
    fn basic_parse_time() {
        let result = "T12:05:24-05:00[u-ca=iso8601]".parse::<PlainTime>();
        assert_time(result.unwrap(), (12, 5, 24, 0, 0, 0));

        let result = "T12:05:24.123456789-05:00[u-ca=iso8601]".parse::<PlainTime>();
        assert_time(result.unwrap(), (12, 5, 24, 123, 456, 789));

        let result = "2024-05-04 12:05:24.123456789-05:00[u-ca=iso8601]".parse::<PlainTime>();
        assert_time(result.unwrap(), (12, 5, 24, 123, 456, 789));

        let result = "2024-05-04 12:05:24.123456789-05:00[u-ca=iso8601]".parse::<PlainTime>();
        assert_time(result.unwrap(), (12, 5, 24, 123, 456, 789));
    }

    #[test]
    fn time_round_millisecond() {
        let base = PlainTime::new_unchecked(IsoTime::new_unchecked(3, 34, 56, 987, 654, 321));

        let result_1 = base.round(options(Unit::Millisecond, 1.0)).unwrap();
        assert_time(result_1, (3, 34, 56, 988, 0, 0));

        let result_2 = base.round(options(Unit::Millisecond, 2.0)).unwrap();
        assert_time(result_2, (3, 34, 56, 988, 0, 0));

        let result_3 = base.round(options(Unit::Millisecond, 4.0)).unwrap();
        assert_time(result_3, (3, 34, 56, 988, 0, 0));

        let result_4 = base.round(options(Unit::Millisecond, 5.0)).unwrap();
        assert_time(result_4, (3, 34, 56, 990, 0, 0));
    }

    #[test]
    fn time_round_microsecond() {
        let base = PlainTime::new_unchecked(IsoTime::new_unchecked(3, 34, 56, 987, 654, 321));

        let result_1 = base.round(options(Unit::Microsecond, 1.0)).unwrap();
        assert_time(result_1, (3, 34, 56, 987, 654, 0));

        let result_2 = base.round(options(Unit::Microsecond, 2.0)).unwrap();
        assert_time(result_2, (3, 34, 56, 987, 654, 0));

        let result_3 = base.round(options(Unit::Microsecond, 4.0)).unwrap();
        assert_time(result_3, (3, 34, 56, 987, 656, 0));

        let result_4 = base.round(options(Unit::Microsecond, 5.0)).unwrap();
        assert_time(result_4, (3, 34, 56, 987, 655, 0));
    }

    #[test]
    fn time_round_nanoseconds() {
        let base = PlainTime::new_unchecked(IsoTime::new_unchecked(3, 34, 56, 987, 654, 321));

        let result_1 = base.round(options(Unit::Nanosecond, 1.0)).unwrap();
        assert_time(result_1, (3, 34, 56, 987, 654, 321));

        let result_2 = base.round(options(Unit::Nanosecond, 2.0)).unwrap();
        assert_time(result_2, (3, 34, 56, 987, 654, 322));

        let result_3 = base.round(options(Unit::Nanosecond, 4.0)).unwrap();
        assert_time(result_3, (3, 34, 56, 987, 654, 320));

        let result_4 = base.round(options(Unit::Nanosecond, 5.0)).unwrap();
        assert_time(result_4, (3, 34, 56, 987, 654, 320));
    }

    #[test]
    fn add_duration_basic() {
        let base = PlainTime::new_unchecked(IsoTime::new_unchecked(15, 23, 30, 123, 456, 789));
        let result = base.add(&"PT16H".parse::<Duration>().unwrap()).unwrap();

        assert_time(result, (7, 23, 30, 123, 456, 789));
    }

    #[test]
    fn since_basic() {
        let one =
            PlainTime::new_with_overflow(15, 23, 30, 123, 456, 789, Overflow::Constrain).unwrap();
        let two =
            PlainTime::new_with_overflow(14, 23, 30, 123, 456, 789, Overflow::Constrain).unwrap();
        let three =
            PlainTime::new_with_overflow(13, 30, 30, 123, 456, 789, Overflow::Constrain).unwrap();

        let result = one.since(&two, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), 1);

        let result = two.since(&one, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), -1);

        let result = one.since(&three, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), 1);
        assert_eq!(result.minutes(), 53);

        let result = three.since(&one, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), -1);
        assert_eq!(result.minutes(), -53);
    }

    #[test]
    fn until_basic() {
        let one =
            PlainTime::new_with_overflow(15, 23, 30, 123, 456, 789, Overflow::Constrain).unwrap();
        let two =
            PlainTime::new_with_overflow(16, 23, 30, 123, 456, 789, Overflow::Constrain).unwrap();
        let three =
            PlainTime::new_with_overflow(17, 0, 30, 123, 456, 789, Overflow::Constrain).unwrap();

        let result = one.until(&two, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), 1);

        let result = two.until(&one, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), -1);

        let result = one.until(&three, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), 1);
        assert_eq!(result.minutes(), 37);

        let result = three.until(&one, DifferenceSettings::default()).unwrap();
        assert_eq!(result.hours(), -1);
        assert_eq!(result.minutes(), -37);
    }

    #[test]
    fn since_rounding() {
        let earlier = PlainTime::new(3, 12, 34, 123, 456, 789).unwrap();
        let later = PlainTime::new(13, 47, 57, 988, 655, 322).unwrap();

        let settings = DifferenceSettings {
            smallest_unit: Some(Unit::Second),
            increment: Some(RoundingIncrement::try_new(1).unwrap()),
            ..Default::default()
        };
        assert_duration(
            later.since(&earlier, settings).unwrap(),
            (0, 0, 0, 0, 10, 35, 23, 0, 0, 0),
        );

        let settings = DifferenceSettings {
            smallest_unit: Some(Unit::Second),
            increment: Some(RoundingIncrement::try_new(4).unwrap()),
            ..Default::default()
        };
        assert_duration(
            later.since(&earlier, settings).unwrap(),
            (0, 0, 0, 0, 10, 35, 20, 0, 0, 0),
        );
    }

    #[test]
    // test262/test/built-ins/Temporal/PlainTime/prototype/round/roundingincrement-nanoseconds.js
    fn rounding_increment_nanos() {
        let time =
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 321, Overflow::Constrain).unwrap();

        assert_eq!(
            time.round(options(Unit::Nanosecond, 1.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 321, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 2.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 322, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 4.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 5.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 8.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 10.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 20.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 25.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 325, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 40.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 320, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 50.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 300, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 100.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 300, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 125.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 375, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 200.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 400, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 250.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 250, Overflow::Constrain).unwrap()
        );
        assert_eq!(
            time.round(options(Unit::Nanosecond, 500.0)).unwrap(),
            PlainTime::new_with_overflow(3, 34, 56, 987, 654, 500, Overflow::Constrain).unwrap()
        );
    }

    #[test]
    fn invalid_time_from_strs() {
        // UTC designator case
        let invalid_cases = [
            "2019-10-01T09:00:00Z",
            "2019-10-01T09:00:00Z[UTC]",
            "09:00:00Z[UTC]",
            "09:00:00Z",
        ];
        for invalid_str in invalid_cases {
            let err = PlainTime::from_str(invalid_str);
            assert!(err.is_err());
        }
    }
}
