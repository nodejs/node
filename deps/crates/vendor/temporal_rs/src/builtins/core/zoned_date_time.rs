//! This module contains the core implementation of the `ZonedDateTime`
//! builtin type.

use crate::provider::EpochNanosecondsAndOffset;
use alloc::string::String;
use core::{cmp::Ordering, num::NonZeroU128};
use tinystr::TinyAsciiStr;

use crate::{
    builtins::{
        calendar::CalendarFields,
        core::{
            calendar::Calendar,
            duration::normalized::{InternalDurationRecord, TimeDuration},
            time_zone::{TimeZone, UtcOffset},
            Duration, Instant, PlainDate, PlainDateTime, PlainTime,
        },
    },
    error::ErrorMessage,
    iso::{IsoDate, IsoDateTime, IsoTime},
    options::{
        DifferenceOperation, DifferenceSettings, Disambiguation, DisplayCalendar, DisplayOffset,
        DisplayTimeZone, OffsetDisambiguation, Overflow, ResolvedRoundingOptions,
        RoundingIncrement, RoundingMode, RoundingOptions, ToStringRoundingOptions, Unit, UnitGroup,
    },
    parsed_intermediates::ParsedZonedDateTime,
    parsers::{FormattableOffset, FormattableTime, IxdtfStringBuilder, Precision},
    partial::PartialTime,
    primitive::FiniteF64,
    provider::{TimeZoneProvider, TransitionDirection, UtcOffsetSeconds},
    rounding::IncrementRounder,
    temporal_assert,
    unix_time::EpochNanoseconds,
    MonthCode, Sign, TemporalError, TemporalResult, TemporalUnwrap,
};

#[cfg(test)]
mod tests;

/// A struct representing a partial `ZonedDateTime`.
#[derive(Debug, Default, Clone, PartialEq)]
pub struct PartialZonedDateTime {
    /// The `ZonedDateTimeFields` portion of a `PartialZonedDateTime`
    pub fields: ZonedDateTimeFields,
    /// The time zone value of a partial time zone.
    pub timezone: Option<TimeZone>,
    /// The calendar for the `PartialZonedDateTime`.
    pub calendar: Calendar,
}

impl PartialZonedDateTime {
    pub fn is_empty(&self) -> bool {
        self.fields.is_empty() && self.timezone.is_none()
    }

    pub const fn new() -> Self {
        Self {
            fields: ZonedDateTimeFields::new(),
            timezone: None,
            calendar: Calendar::ISO,
        }
    }

    pub const fn with_calendar_fields(mut self, fields: CalendarFields) -> Self {
        self.fields.calendar_fields = fields;
        self
    }

    pub const fn with_time(mut self, time: PartialTime) -> Self {
        self.fields.time = time;
        self
    }

    pub const fn with_offset(mut self, offset: UtcOffset) -> Self {
        self.fields.offset = Some(offset);
        self
    }

    pub fn with_timezone(mut self, timezone: Option<TimeZone>) -> Self {
        self.timezone = timezone;
        self
    }
}

#[derive(Debug, Default, Clone, PartialEq)]
pub struct ZonedDateTimeFields {
    pub calendar_fields: CalendarFields,

    pub time: PartialTime,

    /// An optional offset string
    pub offset: Option<UtcOffset>,
}

impl ZonedDateTimeFields {
    pub const fn new() -> Self {
        Self {
            calendar_fields: CalendarFields::new(),
            time: PartialTime::new(),
            offset: None,
        }
    }

    pub fn is_empty(&self) -> bool {
        self.calendar_fields.is_empty() && self.time.is_empty() && self.offset.is_none()
    }
}

/// The native Rust implementation of a Temporal `ZonedDateTime`.
///
/// A `ZonedDateTime` represents a date and time in a specific time zone and calendar.
/// Unlike `PlainDateTime`, it represents an exact moment in time by combining a
/// `PlainDateTime` with time zone information. It is internally represented as
/// an instant (epoch nanoseconds) along with calendar and time zone data.
///
/// Since `ZonedDateTime` includes timezone information, it can handle daylight saving time
/// transitions and timezone offset changes automatically. The type requires a timezone
/// data provider (implementing `TimeZoneProvider`) for most operations, which supplies
/// the necessary timezone rules and historical data.
///
/// Unlike `PlainDateTime` which can be ambiguous during DST transitions, `ZonedDateTime`
/// always represents an unambiguous moment in time.
///
/// ## Time zone provider API
///
/// The core implementation of `ZonedDateTime` uses time zone provider APIs denoted by
/// a `*_with_provider` suffix. This means a provider that implements the `TimeZoneProvider`
/// trait must be provided for timezone-aware operations.
///
/// Time zone providers available:
/// - **File system provider**: `FsTzdbProvider` (enabled with `tzdb` feature)
/// - **Compiled data provider**: Default implementation (enabled with `compiled_data` feature)
///
/// ## Examples
///
/// ### Creating a ZonedDateTime
///
/// ```rust
/// # #[cfg(feature = "compiled_data")] {
/// use temporal_rs::{Calendar, Instant, TimeZone, ZonedDateTime};
///
/// // Create from epoch nanoseconds
/// let zdt = ZonedDateTime::try_new(
///     0,                    // epoch nanoseconds (Unix epoch)
///     TimeZone::utc(),  // UTC timezone
///     Calendar::default(),  // ISO 8601 calendar
/// ).unwrap();
///
/// assert_eq!(zdt.epoch_milliseconds(), 0);
/// assert_eq!(zdt.epoch_nanoseconds().as_i128(), 0);
/// assert_eq!(zdt.time_zone().identifier().unwrap(), "UTC");
/// assert_eq!(zdt.calendar().identifier(), "iso8601");
/// # }
/// ```
///
/// ### Working with timezones (requires provider, or compiled data)
///
/// ```rust
/// # #[cfg(feature = "compiled_data")] {
/// use temporal_rs::{ZonedDateTime, TimeZone, Calendar};
///
/// let time_zone = TimeZone::try_from_str("America/New_York").unwrap();
/// let zoned_date_time = ZonedDateTime::try_new(
///     1609459200000000000, // 2021-01-01T00:00:00Z
///     time_zone,
///     Calendar::default(),
/// ).unwrap();
///
/// // Note: This would be December 31, 2020 19:00 in New York (EST)
/// assert_eq!(zoned_date_time.year(), 2020);
/// assert_eq!(zoned_date_time.month(), 12);
/// assert_eq!(zoned_date_time.day(), 31);
/// assert_eq!(zoned_date_time.hour(), 19);
/// # }
/// ```
///
/// ### ZonedDateTime arithmetic (requires provider)
///
/// ```rust
/// # #[cfg(feature = "compiled_data")] {
/// use temporal_rs::{ZonedDateTime, Duration, TimeZone, Calendar};
/// use std::str::FromStr;
///
/// let time_zone = TimeZone::try_from_str("Europe/London").unwrap();
/// let zdt = ZonedDateTime::try_new(
///     1609459200000000000, // 2021-01-01T00:00:00Z
///     time_zone,
///     Calendar::default(),
/// ).unwrap();
///
/// // Add 6 months
/// let later = zdt.add(
///     &Duration::from_str("P6M").unwrap(),
///     None,
/// ).unwrap();
///
/// assert_eq!(later.month(), 7); // July
/// # }
/// ```
///
/// ### Converting from PlainDateTime
///
/// ```rust
/// # #[cfg(feature = "compiled_data")] {
/// use temporal_rs::{PlainDateTime, ZonedDateTime, TimeZone, options::Disambiguation};
/// use std::str::FromStr;
///
/// let plain_date_time = PlainDateTime::from_str("2024-03-15T14:30:00").unwrap();
/// let time_zone = TimeZone::try_from_str("America/Los_Angeles").unwrap();
///
/// let zdt = plain_date_time.to_zoned_date_time(
///     time_zone,
///     Disambiguation::Compatible,
/// ).unwrap();
///
/// // Now we have an exact moment in time in the LA timezone
/// assert_eq!(zdt.time_zone().identifier().unwrap(), "America/Los_Angeles");
/// # }
/// ```
///
/// ### String formatting (requires provider)
///
/// ```rust
/// # #[cfg(feature = "compiled_data")] {
/// use temporal_rs::{ZonedDateTime, Calendar, TimeZone};
/// use temporal_rs::options::{DisplayOffset, DisplayTimeZone, DisplayCalendar, ToStringRoundingOptions};
///
/// let zdt = ZonedDateTime::try_new(
///     1609459200000000000,
///     TimeZone::try_from_str("Asia/Tokyo").unwrap(),
///     Calendar::default(),
/// ).unwrap();
///
/// let iso_string = zdt.to_ixdtf_string(
///     DisplayOffset::default(),
///     DisplayTimeZone::default(),
///     DisplayCalendar::default(),
///     ToStringRoundingOptions::default()
/// ).unwrap();
///
/// // Results in something like "2021-01-01T09:00:00+09:00[Asia/Tokyo]"
/// assert!(iso_string.contains("2021-01-01"));
/// assert!(iso_string.contains("+09:00"));
/// assert!(iso_string.contains("[Asia/Tokyo]"));
/// # }
/// ```
///
/// ## Reference
///
/// For more information, see the [MDN documentation][mdn-zoneddatetime].
///
/// [mdn-zoneddatetime]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Temporal/ZonedDateTime
#[non_exhaustive]
#[derive(Debug, Clone)]
pub struct ZonedDateTime {
    instant: Instant,
    calendar: Calendar,
    time_zone: TimeZone,
    cached_offset: UtcOffset,
}

// ==== Private API ====

impl ZonedDateTime {
    /// Creates a `ZonedDateTime` without validating the input.
    #[inline]
    #[must_use]
    pub(crate) fn new_unchecked(
        instant: Instant,
        time_zone: TimeZone,
        calendar: Calendar,
        cached_offset: UtcOffsetSeconds,
    ) -> Self {
        Self {
            instant,
            calendar,
            time_zone,
            cached_offset: cached_offset.into(),
        }
    }

    pub(crate) fn new_unchecked_with_provider(
        instant: Instant,
        time_zone: TimeZone,
        calendar: Calendar,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let offset = time_zone
            .get_utc_offset_for(instant.epoch_nanoseconds().0, provider)
            .temporal_unwrap()?;
        Ok(Self {
            instant,
            calendar,
            time_zone,
            cached_offset: offset,
        })
    }

    /// Equivalent to self.tz.get_iso_datetime_for(&self.instant, provider)
    ///
    /// (which is GetISODateTimeFor(self.[[TimeZone]], self.[[EpochNanoseconds]]).)
    pub(crate) fn get_iso_datetime(&self) -> IsoDateTime {
        self.cached_offset.get_iso_datetime_for(&self.instant)
    }

    pub(crate) fn add_zoned_date_time(
        &self,
        duration: InternalDurationRecord,
        overflow: Overflow,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Instant> {
        // 1. If DateDurationSign(duration.[[Date]]) = 0, then
        if duration.date().sign() == Sign::Zero {
            // a. Return ? AddInstant(epochNanoseconds, duration.[[Time]]).
            return self
                .instant
                .add_to_instant(&duration.normalized_time_duration());
        }
        // 2. Let isoDateTime be GetISODateTimeFor(timeZone, epochNanoseconds).
        let iso_datetime = self.get_iso_datetime();
        // 3. Let addedDate be ? CalendarDateAdd(calendar, isoDateTime.[[ISODate]], duration.[[Date]], overflow).
        let added_date =
            self.calendar()
                .date_add(&iso_datetime.date, &duration.date(), overflow)?;
        // 4. Let intermediateDateTime be CombineISODateAndTimeRecord(addedDate, isoDateTime.[[Time]]).
        let intermediate = IsoDateTime::new_unchecked(added_date.iso, iso_datetime.time);
        // 5. If ISODateTimeWithinLimits(intermediateDateTime) is false, throw a RangeError exception.
        if !intermediate.is_within_limits() {
            return Err(
                TemporalError::range().with_enum(ErrorMessage::IntermediateDateTimeOutOfRange)
            );
        }
        // 6. Let intermediateNs be ! GetEpochNanosecondsFor(timeZone, intermediateDateTime, compatible).
        let intermediate_ns = self.time_zone().get_epoch_nanoseconds_for(
            intermediate,
            Disambiguation::Compatible,
            provider,
        )?;

        // 7. Return ? AddInstant(intermediateNs, duration.[[Time]]).
        Instant::from(intermediate_ns.ns).add_to_instant(&duration.normalized_time_duration())
    }

    /// Adds a duration to the current `ZonedDateTime`, returning the resulting `ZonedDateTime`.
    ///
    /// Aligns with Abstract Operation 6.5.10
    #[inline]
    pub(crate) fn add_internal(
        &self,
        duration: &Duration,
        overflow: Overflow,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        // 1. Let duration be ? ToTemporalDuration(temporalDurationLike).
        // 2. If operation is subtract, set duration to CreateNegatedTemporalDuration(duration).
        // 3. Let resolvedOptions be ? GetOptionsObject(options).
        // 4. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
        // 5. Let calendar be zonedDateTime.[[Calendar]].
        // 6. Let timeZone be zonedDateTime.[[TimeZone]].
        // 7. Let internalDuration be ToInternalDurationRecord(duration).
        let internal_duration = duration.to_internal_duration_record();
        // 8. Let epochNanoseconds be ? AddZonedDateTime(zonedDateTime.[[EpochNanoseconds]], timeZone, calendar, internalDuration, overflow).
        let epoch_ns = self.add_zoned_date_time(internal_duration, overflow, provider)?;
        // 9. Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
        Self::new_unchecked_with_provider(
            epoch_ns,
            *self.time_zone(),
            self.calendar().clone(),
            provider,
        )
    }

    /// Internal representation of Abstract Op 6.5.7
    pub(crate) fn diff_with_rounding(
        &self,
        other: &Instant,
        resolved_options: ResolvedRoundingOptions,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<InternalDurationRecord> {
        // 1. If UnitCategory(largestUnit) is time, then
        if resolved_options.largest_unit.is_time_unit() {
            // a. Return DifferenceInstant(ns1, ns2, roundingIncrement, smallestUnit, roundingMode).
            return self.instant.diff_instant_internal(other, resolved_options);
        }
        // 2. let difference be ? differencezoneddatetime(ns1, ns2, timezone, calendar, largestunit).
        let diff = self.diff_zoned_datetime(other, resolved_options.largest_unit, provider)?;
        // 3. if smallestunit is nanosecond and roundingincrement = 1, return difference.
        if resolved_options.smallest_unit == Unit::Nanosecond
            && resolved_options.increment == RoundingIncrement::ONE
        {
            return Ok(diff);
        }
        // 4. let datetime be getisodatetimefor(timezone, ns1).
        let iso = self.get_iso_datetime();
        // 5. Return ? RoundRelativeDuration(difference, ns2, dateTime, timeZone, calendar, largestUnit, roundingIncrement, smallestUnit, roundingMode).
        diff.round_relative_duration(
            other.epoch_nanoseconds().as_i128(),
            &PlainDateTime::new_unchecked(iso, self.calendar().clone()),
            Some((self.time_zone(), provider)),
            resolved_options,
        )
    }

    /// Internal representation of Abstract Op 6.5.8
    pub(crate) fn diff_with_total(
        &self,
        other: &Instant,
        unit: Unit,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<FiniteF64> {
        // 1. If UnitCategory(unit) is time, then
        if unit.is_time_unit() {
            // a. Let difference be TimeDurationFromEpochNanosecondsDifference(ns2, ns1).
            let diff = TimeDuration::from_nanosecond_difference(
                other.epoch_nanoseconds().as_i128(),
                self.epoch_nanoseconds().as_i128(),
            )?;
            // b. Return TotalTimeDuration(difference, unit).
            return diff.total(unit);
        }

        // 2. Let difference be ? DifferenceZonedDateTime(ns1, ns2, timeZone, calendar, unit).
        let diff = self.diff_zoned_datetime(other, unit, provider)?;
        // 3. Let dateTime be GetISODateTimeFor(timeZone, ns1).
        let iso = self.get_iso_datetime();
        // 4. Return ? TotalRelativeDuration(difference, ns2, dateTime, timeZone, calendar, unit).
        diff.total_relative_duration(
            other.epoch_nanoseconds().as_i128(),
            &PlainDateTime::new_unchecked(iso, self.calendar().clone()),
            Some((self.time_zone(), provider)),
            unit,
        )
    }

    pub(crate) fn diff_zoned_datetime(
        &self,
        other: &Instant,
        largest_unit: Unit,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<InternalDurationRecord> {
        // 1. If ns1 = ns2, return CombineDateAndTimeDuration(ZeroDateDuration(), 0).
        if self.epoch_nanoseconds() == other.epoch_nanoseconds() {
            return Ok(InternalDurationRecord::default());
        }
        // 2. Let startDateTime be GetISODateTimeFor(timeZone, ns1).
        let start = self.get_iso_datetime();
        // 3. Let endDateTime be GetISODateTimeFor(timeZone, ns2).
        let end = self.time_zone.get_iso_datetime_for(other, provider)?;
        // 4. If ns2 - ns1 < 0, let sign be -1; else let sign be 1.
        let sign = if other.epoch_nanoseconds().as_i128() - self.epoch_nanoseconds().as_i128() < 0 {
            Sign::Negative
        } else {
            Sign::Positive
        };
        // 5. If sign = 1, let maxDayCorrection be 2; else let maxDayCorrection be 1.
        let max_correction = if sign == Sign::Positive { 2 } else { 1 };
        // 6. Let dayCorrection be 0.
        // 7. Let timeDuration be DifferenceTime(startDateTime.[[Time]], endDateTime.[[Time]]).
        let time = start.time.diff(&end.time);
        // 8. If TimeDurationSign(timeDuration) = -sign, set dayCorrection to dayCorrection + 1.
        let mut day_correction = if time.sign() as i8 == -(sign as i8) {
            1
        } else {
            0
        };

        // 9. Let success be false.
        let mut intermediate_dt = IsoDateTime::default();
        let mut time_duration = TimeDuration::default();
        let mut is_success = false;
        // 10. Repeat, while dayCorrection ≤ maxDayCorrection and success is false,
        while day_correction <= max_correction && !is_success {
            // a. Let intermediateDate be BalanceISODate(endDateTime.[[ISODate]].[[Year]],
            // endDateTime.[[ISODate]].[[Month]], endDateTime.[[ISODate]].[[Day]] - dayCorrection × sign).
            let intermediate = IsoDate::balance(
                end.date.year,
                end.date.month.into(),
                i32::from(end.date.day) - i32::from(day_correction * sign as i8),
            );
            // b. Let intermediateDateTime be CombineISODateAndTimeRecord(intermediateDate, startDateTime.[[Time]]).
            intermediate_dt = IsoDateTime::new_unchecked(intermediate, start.time);
            // c. Let intermediateNs be ? GetEpochNanosecondsFor(timeZone, intermediateDateTime, compatible).
            let intermediate_ns = self.time_zone.get_epoch_nanoseconds_for(
                intermediate_dt,
                Disambiguation::Compatible,
                provider,
            )?;
            // d. Set timeDuration to TimeDurationFromEpochNanosecondsDifference(ns2, intermediateNs).
            time_duration = TimeDuration::from_nanosecond_difference(
                other.epoch_nanoseconds().as_i128(),
                intermediate_ns.ns.0,
            )?;
            // e. Let timeSign be TimeDurationSign(timeDuration).
            let time_sign = time_duration.sign() as i8;
            // f. If sign ≠ -timeSign, then
            if sign as i8 != -time_sign {
                // i. Set success to true.
                is_success = true;
            }
            // g. Set dayCorrection to dayCorrection + 1.
            day_correction += 1;
        }
        // 11. Assert: success is true.
        // 12. Let dateLargestUnit be LargerOfTwoUnits(largestUnit, day).
        let date_largest = largest_unit.max(Unit::Day);
        // 13. Let dateDifference be CalendarDateUntil(calendar, startDateTime.[[ISODate]], intermediateDateTime.[[ISODate]], dateLargestUnit).
        // 14. Return CombineDateAndTimeDuration(dateDifference, timeDuration).
        let date_diff =
            self.calendar()
                .date_until(&start.date, &intermediate_dt.date, date_largest)?;
        InternalDurationRecord::new(date_diff.date(), time_duration)
    }

    /// `temporal_rs` equivalent to `DifferenceTemporalZonedDateTime`.
    pub(crate) fn diff_internal_with_provider(
        &self,
        op: DifferenceOperation,
        other: &Self,
        options: DifferenceSettings,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Duration> {
        // NOTE: for order of operations, this should be asserted prior to this point
        // by any engine implementors, but asserting out of caution.
        if self.calendar != other.calendar {
            return Err(TemporalError::range().with_enum(ErrorMessage::CalendarMismatch));
        }

        // 4. Set settings be ? GetDifferenceSettings(operation, resolvedOptions, datetime, « », nanosecond, hour).
        let resolved_options = ResolvedRoundingOptions::from_diff_settings(
            options,
            op,
            UnitGroup::DateTime,
            Unit::Hour,
            Unit::Nanosecond,
        )?;

        // 5. If UnitCategory(settings.[[LargestUnit]]) is time, then
        if resolved_options.largest_unit.is_time_unit() {
            // a. Let internalDuration be DifferenceInstant(zonedDateTime.[[EpochNanoseconds]], other.[[EpochNanoseconds]], settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
            let internal = self
                .instant
                .diff_instant_internal(&other.instant, resolved_options)?;
            // b. Let result be ! TemporalDurationFromInternal(internalDuration, settings.[[LargestUnit]]).
            let result = Duration::from_internal(internal, resolved_options.largest_unit)?;
            // c. If operation is since, set result to CreateNegatedTemporalDuration(result).
            // d. Return result.
            match op {
                DifferenceOperation::Since => return Ok(result.negated()),
                DifferenceOperation::Until => return Ok(result),
            }
        }

        // 6. NOTE: To calculate differences in two different time zones,
        // settings.[[LargestUnit]] must be a time unit, because day lengths
        // can vary between time zones due to DST and other UTC offset shifts.
        // 7. If TimeZoneEquals(zonedDateTime.[[TimeZone]], other.[[TimeZone]]) is false, then
        if !self
            .time_zone
            .time_zone_equals_with_provider(&other.time_zone, provider)?
        {
            // a. Throw a RangeError exception.
            return Err(TemporalError::range().with_enum(ErrorMessage::TzMismatch));
        }

        // 8. If zonedDateTime.[[EpochNanoseconds]] = other.[[EpochNanoseconds]], then
        if self.instant == other.instant {
            // a. Return ! CreateTemporalDuration(0, 0, 0, 0, 0, 0, 0, 0, 0, 0).
            return Ok(Duration::default());
        }

        // 9. Let internalDuration be ? DifferenceZonedDateTimeWithRounding(zonedDateTime.[[EpochNanoseconds]], other.[[EpochNanoseconds]], zonedDateTime.[[TimeZone]], zonedDateTime.[[Calendar]], settings.[[LargestUnit]], settings.[[RoundingIncrement]], settings.[[SmallestUnit]], settings.[[RoundingMode]]).
        let internal = self.diff_with_rounding(&other.instant, resolved_options, provider)?;
        // 10. Let result be ! TemporalDurationFromInternal(internalDuration, hour).
        let result = Duration::from_internal(internal, Unit::Hour)?;
        // 11. If operation is since, set result to CreateNegatedTemporalDuration(result).
        // 12. Return result.
        match op {
            DifferenceOperation::Since => Ok(result.negated()),
            DifferenceOperation::Until => Ok(result),
        }
    }
}

// ==== Public API ====

impl ZonedDateTime {
    /// Creates a new valid `ZonedDateTime`.
    #[inline]
    pub fn try_new_with_provider(
        nanos: i128,
        time_zone: TimeZone,
        calendar: Calendar,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let instant = Instant::try_new(nanos)?;
        Self::new_unchecked_with_provider(instant, time_zone, calendar, provider)
    }

    /// Creates a new valid `ZonedDateTime`.
    #[inline]
    pub(crate) fn try_new_with_cached_offset(
        nanos: i128,
        time_zone: TimeZone,
        calendar: Calendar,
        cached_offset: UtcOffsetSeconds,
    ) -> TemporalResult<Self> {
        let instant = Instant::try_new(nanos)?;
        Ok(Self::new_unchecked(
            instant,
            time_zone,
            calendar,
            cached_offset,
        ))
    }

    /// Creates a new valid `ZonedDateTime` with an ISO 8601 calendar.
    #[inline]
    pub fn try_new_iso_with_provider(
        nanos: i128,
        time_zone: TimeZone,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let instant = Instant::try_new(nanos)?;
        Self::new_unchecked_with_provider(instant, time_zone, Calendar::ISO, provider)
    }

    /// Creates a new valid `ZonedDateTime` from an [`Instant`].
    #[inline]
    pub fn try_new_from_instant_with_provider(
        instant: Instant,
        time_zone: TimeZone,
        calendar: Calendar,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        Self::new_unchecked_with_provider(instant, time_zone, calendar, provider)
    }

    /// Creates a new valid `ZonedDateTime` from an [`Instant`] with an ISO 8601 calendar.
    #[inline]
    pub fn try_new_iso_from_instant_with_provider(
        instant: Instant,
        time_zone: TimeZone,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        Self::new_unchecked_with_provider(instant, time_zone, Calendar::ISO, provider)
    }

    /// Returns `ZonedDateTime`'s Calendar.
    #[inline]
    #[must_use]
    pub fn calendar(&self) -> &Calendar {
        &self.calendar
    }

    /// Returns `ZonedDateTime`'s `TimeZone` slot.
    #[inline]
    #[must_use]
    pub fn time_zone(&self) -> &TimeZone {
        &self.time_zone
    }

    /// Create a `ZonedDateTime` from a `PartialZonedDateTime`.
    #[inline]
    pub fn from_partial_with_provider(
        partial: PartialZonedDateTime,
        overflow: Option<Overflow>,
        disambiguation: Option<Disambiguation>,
        offset_option: Option<OffsetDisambiguation>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let overflow = overflow.unwrap_or(Overflow::Constrain);
        let disambiguation = disambiguation.unwrap_or(Disambiguation::Compatible);
        let offset_option = offset_option.unwrap_or(OffsetDisambiguation::Reject);

        let date = partial
            .calendar
            .date_from_fields(partial.fields.calendar_fields, overflow)?
            .iso;

        let time = Some(IsoTime::default().with(partial.fields.time, overflow)?);

        // Handle time zones
        let offset_nanos = partial.fields.offset.map(|offset| offset.nanoseconds());

        let timezone = partial
            .timezone
            .unwrap_or_else(|| TimeZone::utc_with_provider(provider));
        let epoch_nanos = interpret_isodatetime_offset(
            date,
            time,
            false,
            offset_nanos,
            &timezone,
            disambiguation,
            offset_option,
            false,
            provider,
        )?;

        Ok(Self::new_unchecked(
            Instant::from(epoch_nanos.ns),
            timezone,
            partial.calendar,
            epoch_nanos.offset,
        ))
    }

    /// Returns the `epochMilliseconds` value of this `ZonedDateTime`.
    #[must_use]
    pub fn epoch_milliseconds(&self) -> i64 {
        self.instant.epoch_milliseconds()
    }

    /// Returns the `epochNanoseconds` value of this `ZonedDateTime`.
    #[must_use]
    pub fn epoch_nanoseconds(&self) -> &EpochNanoseconds {
        self.instant.epoch_nanoseconds()
    }

    /// Returns the current `ZonedDateTime` as an [`Instant`].
    #[must_use]
    pub fn to_instant(&self) -> Instant {
        self.instant
    }

    pub fn with_with_provider(
        &self,
        fields: ZonedDateTimeFields,
        disambiguation: Option<Disambiguation>,
        offset_option: Option<OffsetDisambiguation>,
        overflow: Option<Overflow>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let overflow = overflow.unwrap_or_default();
        let disambiguation = disambiguation.unwrap_or_default();
        let offset_option = offset_option.unwrap_or(OffsetDisambiguation::Reject);
        // 8. Let isoDateTime be GetISODateTimeFor(timeZone, epochNs).
        let iso_date_time = self.get_iso_datetime();
        let plain_date_time = PlainDateTime::new_unchecked(iso_date_time, self.calendar.clone());

        // 23. Let dateTimeResult be ? InterpretTemporalDateTimeFields(calendar, fields, overflow).
        let result_date = self.calendar.date_from_fields(
            fields.calendar_fields.with_fallback_datetime(
                &plain_date_time,
                self.calendar.kind(),
                overflow,
            )?,
            overflow,
        )?;

        let time = iso_date_time.time.with(fields.time, overflow)?;

        // 7. Let offsetNanoseconds be GetOffsetNanosecondsFor(timeZone, epochNs).
        let original_offset = self.offset_nanoseconds();
        // 24. Let newOffsetNanoseconds be ! ParseDateTimeUTCOffset(fields.[[OffsetString]]).
        let new_offset_nanos = fields
            .offset
            .map(|offset| offset.nanoseconds())
            .or(Some(original_offset));

        // 25. Let epochNanoseconds be ? InterpretISODateTimeOffset(dateTimeResult.[[ISODate]], dateTimeResult.[[Time]], option, newOffsetNanoseconds, timeZone, disambiguation, offset, match-exactly).
        let epoch_nanos = interpret_isodatetime_offset(
            result_date.iso,
            Some(time),
            // Set to Option ... we don't use an enum here, so any value will do.
            true,
            new_offset_nanos,
            &self.time_zone,
            disambiguation,
            offset_option,
            // match-exactly
            false,
            provider,
        )?;

        // 26. Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
        Ok(Self::new_unchecked(
            Instant::from(epoch_nanos.ns),
            self.time_zone,
            self.calendar.clone(),
            epoch_nanos.offset,
        ))
    }

    /// Creates a new `ZonedDateTime` from the current `ZonedDateTime`
    /// combined with the provided `TimeZone`.
    pub fn with_time_zone_with_provider(
        &self,
        time_zone: TimeZone,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        Self::try_new_with_provider(
            self.epoch_nanoseconds().as_i128(),
            time_zone,
            self.calendar.clone(),
            provider,
        )
    }

    /// Creates a new `ZonedDateTime` from the current `ZonedDateTime`
    /// combined with the provided `Calendar`.
    #[inline]
    #[must_use]
    pub fn with_calendar(&self, calendar: Calendar) -> Self {
        Self::new_unchecked(
            self.instant,
            self.time_zone,
            calendar,
            self.cached_offset.into(),
        )
    }

    /// Compares one `ZonedDateTime` to another `ZonedDateTime` using their
    /// `Instant` representation.
    ///
    /// # Note on Ordering.
    ///
    /// `temporal_rs` does not implement `PartialOrd`/`Ord` as `ZonedDateTime` does
    /// not fulfill all the conditions required to implement the traits. However,
    /// it is possible to compare `PlainDate`'s as their `IsoDate` representation.
    #[inline]
    #[must_use]
    pub fn compare_instant(&self, other: &Self) -> Ordering {
        self.instant.cmp(&other.instant)
    }
}

// ==== HoursInDay accessor method implementation ====

impl ZonedDateTime {
    /// Get the previous or next time zone transition for the current `ZonedDateTime`.
    pub fn get_time_zone_transition_with_provider(
        &self,
        direction: TransitionDirection,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Option<Self>> {
        // 8. If IsOffsetTimeZoneIdentifier(timeZone) is true, return null.
        let TimeZone::IanaIdentifier(identifier) = self.time_zone else {
            return Ok(None);
        };
        // 9. If direction is next, then
        // a. Let transition be GetNamedTimeZoneNextTransition(timeZone, zonedDateTime.[[EpochNanoseconds]]).
        // 10. Else,
        // a. Assert: direction is previous.
        // b. Let transition be GetNamedTimeZonePreviousTransition(timeZone, zonedDateTime.[[EpochNanoseconds]]).
        let transition = provider.get_time_zone_transition(
            identifier,
            self.epoch_nanoseconds().as_i128(),
            direction,
        )?;

        // 11. If transition is null, return null.
        let Some(transition) = transition else {
            return Ok(None);
        };

        if transition.check_validity().is_err() {
            // GetNamedTimeZoneNextTransition, GetNamedTimeZonePreviousTransition include a check for out-of-bounds
            // instants. Instead of requiring providers handle that, we handle it here.
            return Ok(None);
        }
        // 12. Return ! CreateTemporalZonedDateTime(transition, timeZone, zonedDateTime.[[Calendar]]).
        Ok(Some(
            ZonedDateTime::try_new_with_provider(
                transition.0,
                self.time_zone,
                self.calendar().clone(),
                provider,
            )
            .ok()
            .temporal_unwrap()?,
        ))
    }

    pub fn hours_in_day_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<f64> {
        // 1-3. Is engine specific steps
        // 4. Let isoDateTime be GetISODateTimeFor(timeZone, zonedDateTime.[[EpochNanoseconds]]).
        let iso = self.get_iso_datetime();
        // 5. Let today be isoDateTime.[[ISODate]].
        let today = iso.date;
        // 6. Let tomorrow be BalanceISODate(today.[[Year]], today.[[Month]], today.[[Day]] + 1).
        let tomorrow = IsoDate::balance(today.year, today.month.into(), i32::from(today.day + 1));
        // 7. Let todayNs be ? GetStartOfDay(timeZone, today).
        let today = self.time_zone.get_start_of_day(&today, provider)?;
        // 8. Let tomorrowNs be ? GetStartOfDay(timeZone, tomorrow).
        let tomorrow = self.time_zone.get_start_of_day(&tomorrow, provider)?;
        // 9. Let diff be TimeDurationFromEpochNanosecondsDifference(tomorrowNs, todayNs).
        let diff = TimeDuration::from_nanosecond_difference(tomorrow.ns.0, today.ns.0)?;
        // NOTE: The below should be safe as today_ns and tomorrow_ns should be at most 25 hours.
        // TODO: Tests for the below cast.
        // 10. Return 𝔽(TotalTimeDuration(diff, hour)).
        Ok(diff.divide(3_600_000_000_000.))
    }
}

// ==== Core accessor methods ====

impl ZonedDateTime {
    /// Returns the `year` value for this `ZonedDateTime`.
    #[inline]
    pub fn year(&self) -> i32 {
        let iso = self.get_iso_datetime();
        let dt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.year(&dt.iso.date)
    }

    /// Returns the `month` value for this `ZonedDateTime`.
    pub fn month(&self) -> u8 {
        let iso = self.get_iso_datetime();
        let dt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.month(&dt.iso.date)
    }

    /// Returns the `monthCode` value for this `ZonedDateTime`.
    pub fn month_code(&self) -> MonthCode {
        let iso = self.get_iso_datetime();
        let dt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.month_code(&dt.iso.date)
    }

    /// Returns the `day` value for this `ZonedDateTime`.
    pub fn day(&self) -> u8 {
        let iso = self.get_iso_datetime();
        let dt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.day(&dt.iso.date)
    }

    /// Returns the `hour` value for this `ZonedDateTime`.
    pub fn hour(&self) -> u8 {
        let iso = self.get_iso_datetime();
        iso.time.hour
    }

    /// Returns the `minute` value for this `ZonedDateTime`.
    pub fn minute(&self) -> u8 {
        let iso = self.get_iso_datetime();
        iso.time.minute
    }

    /// Returns the `second` value for this `ZonedDateTime`.
    pub fn second(&self) -> u8 {
        let iso = self.get_iso_datetime();
        iso.time.second
    }

    /// Returns the `millisecond` value for this `ZonedDateTime`.
    pub fn millisecond(&self) -> u16 {
        let iso = self.get_iso_datetime();
        iso.time.millisecond
    }

    /// Returns the `microsecond` value for this `ZonedDateTime`.
    pub fn microsecond(&self) -> u16 {
        let iso = self.get_iso_datetime();
        iso.time.microsecond
    }

    /// Returns the `nanosecond` value for this `ZonedDateTime`.
    pub fn nanosecond(&self) -> u16 {
        let iso = self.get_iso_datetime();
        iso.time.nanosecond
    }

    // TODO: Expose a format / writeable API
    /// Returns an offset string for the current `ZonedDateTime`.
    pub fn offset(&self) -> String {
        let offset = self.cached_offset.nanoseconds();
        nanoseconds_to_formattable_offset(offset as i128).to_string()
    }

    /// Returns the offset nanoseconds for the current `ZonedDateTime`.
    pub fn offset_nanoseconds(&self) -> i64 {
        self.cached_offset.nanoseconds()
    }
}

pub(crate) fn nanoseconds_to_formattable_offset(nanoseconds: i128) -> FormattableOffset {
    let sign = if nanoseconds >= 0 {
        Sign::Positive
    } else {
        Sign::Negative
    };
    let nanos = nanoseconds.unsigned_abs();
    let hour = (nanos / 3_600_000_000_000) as u8;
    let minute = ((nanos / 60_000_000_000) % 60) as u8;
    let second = ((nanos / 1_000_000_000) % 60) as u8;
    let nanosecond = (nanos % 1_000_000_000) as u32;

    let precision = if second == 0 && nanosecond == 0 {
        Precision::Minute
    } else {
        Precision::Auto
    };

    FormattableOffset {
        sign,
        time: FormattableTime {
            hour,
            minute,
            second,
            nanosecond,
            precision,
            include_sep: true,
        },
    }
}

// ==== Core calendar method implementations ====

impl ZonedDateTime {
    /// Returns the era for the current `ZonedDateTime`.
    pub fn era(&self) -> Option<TinyAsciiStr<16>> {
        let iso = self.get_iso_datetime();
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.era(&pdt.iso.date)
    }

    /// Returns the era-specific year for the current `ZonedDateTime`.
    pub fn era_year(&self) -> Option<i32> {
        let iso = self.get_iso_datetime();
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.era_year(&pdt.iso.date)
    }

    /// Returns the calendar day of week value.
    pub fn day_of_week(&self) -> u16 {
        let iso = self.get_iso_datetime();
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.day_of_week(&pdt.iso.date)
    }

    /// Returns the calendar day of year value.
    pub fn day_of_year(&self) -> u16 {
        let iso = self.get_iso_datetime();
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.day_of_year(&pdt.iso.date)
    }

    /// Returns the calendar week of year value.
    pub fn week_of_year(&self) -> Option<u8> {
        let iso = self.get_iso_datetime();
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.week_of_year(&pdt.iso.date)
    }

    /// Returns the calendar year of week value.
    pub fn year_of_week(&self) -> Option<i32> {
        let iso = self.get_iso_datetime();
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.year_of_week(&pdt.iso.date)
    }

    /// Returns the calendar days in week value.
    pub fn days_in_week(&self) -> u16 {
        let iso = self.get_iso_datetime();
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.days_in_week(&pdt.iso.date)
    }

    /// Returns the calendar days in month value.
    pub fn days_in_month(&self) -> u16 {
        let iso = self.get_iso_datetime();
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.days_in_month(&pdt.iso.date)
    }

    /// Returns the calendar days in year value.
    pub fn days_in_year(&self) -> u16 {
        let iso = self.get_iso_datetime();
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.days_in_year(&pdt.iso.date)
    }

    /// Returns the calendar months in year value.
    pub fn months_in_year(&self) -> u16 {
        let iso = self.get_iso_datetime();
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.months_in_year(&pdt.iso.date)
    }

    /// Returns returns whether the date in a leap year for the given calendar.
    pub fn in_leap_year(&self) -> bool {
        let iso = self.get_iso_datetime();
        let pdt = PlainDateTime::new_unchecked(iso, self.calendar.clone());
        self.calendar.in_leap_year(&pdt.iso.date)
    }
}

// ==== Core method implementations ====

impl ZonedDateTime {
    /// Creates a new `ZonedDateTime` from the current `ZonedDateTime`
    /// combined with the provided `TimeZone`.
    pub fn with_plain_time_and_provider(
        &self,
        time: Option<PlainTime>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let iso = self.get_iso_datetime();
        let epoch_ns = if let Some(time) = time {
            let result_iso = IsoDateTime::new_unchecked(iso.date, time.iso);
            self.time_zone.get_epoch_nanoseconds_for(
                result_iso,
                Disambiguation::Compatible,
                provider,
            )?
        } else {
            self.time_zone.get_start_of_day(&iso.date, provider)?
        };
        Self::try_new_with_cached_offset(
            epoch_ns.ns.0,
            self.time_zone,
            self.calendar.clone(),
            epoch_ns.offset,
        )
    }

    /// Add a duration to the current `ZonedDateTime`
    pub fn add_with_provider(
        &self,
        duration: &Duration,
        overflow: Option<Overflow>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        self.add_internal(duration, overflow.unwrap_or(Overflow::Constrain), provider)
    }

    /// Subtract a duration to the current `ZonedDateTime`
    pub fn subtract_with_provider(
        &self,
        duration: &Duration,
        overflow: Option<Overflow>,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        self.add_internal(
            &duration.negated(),
            overflow.unwrap_or(Overflow::Constrain),
            provider,
        )
    }

    /// Checks if the current `ZonedDateTime` is equal to another `ZonedDateTime`.
    pub fn equals_with_provider(
        &self,
        other: &Self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<bool> {
        // 4. If zonedDateTime.[[EpochNanoseconds]] ≠ other.[[EpochNanoseconds]], return false.
        if self.instant != other.instant {
            return Ok(false);
        }

        // 5. If TimeZoneEquals(zonedDateTime.[[TimeZone]], other.[[TimeZone]]) is false, return false.
        if !self
            .time_zone
            .time_zone_equals_with_provider(&other.time_zone, provider)?
        {
            return Ok(false);
        }
        // 6. Return CalendarEquals(zonedDateTime.[[Calendar]], other.[[Calendar]]).
        Ok(self.calendar == other.calendar)
    }

    /// Returns a [`Duration`] representing the period of time from this `ZonedDateTime` since the other `ZonedDateTime`.
    pub fn since_with_provider(
        &self,
        other: &Self,
        options: DifferenceSettings,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Duration> {
        self.diff_internal_with_provider(DifferenceOperation::Since, other, options, provider)
    }

    /// Returns a [`Duration`] representing the period of time from this `ZonedDateTime` since the other `ZonedDateTime`.
    pub fn until_with_provider(
        &self,
        other: &Self,
        options: DifferenceSettings,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Duration> {
        self.diff_internal_with_provider(DifferenceOperation::Until, other, options, provider)
    }

    /// Return a `ZonedDateTime` representing the start of the day
    /// for the current `ZonedDateTime`.
    pub fn start_of_day_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let iso = self.get_iso_datetime();
        let epoch_nanos = self.time_zone.get_start_of_day(&iso.date, provider)?;
        Self::try_new_with_cached_offset(
            epoch_nanos.ns.0,
            self.time_zone,
            self.calendar.clone(),
            epoch_nanos.offset,
        )
    }

    /// Convert the current `ZonedDateTime` to a [`PlainDate`]
    pub fn to_plain_date(&self) -> PlainDate {
        let iso = self.get_iso_datetime();
        PlainDate::new_unchecked(iso.date, self.calendar.clone())
    }

    /// Convert the current `ZonedDateTime` to a [`PlainTime`]
    pub fn to_plain_time(&self) -> PlainTime {
        let iso = self.get_iso_datetime();
        PlainTime::new_unchecked(iso.time)
    }

    /// Convert the current `ZonedDateTime` to a [`PlainDateTime`]
    pub fn to_plain_date_time(&self) -> PlainDateTime {
        let iso = self.get_iso_datetime();
        PlainDateTime::new_unchecked(iso, self.calendar.clone())
    }

    /// Creates a default formatted IXDTF (RFC 9557) date/time string for the provided `ZonedDateTime`.
    pub fn to_string_with_provider(
        &self,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<String> {
        self.to_ixdtf_string_with_provider(
            DisplayOffset::Auto,
            DisplayTimeZone::Auto,
            DisplayCalendar::Auto,
            ToStringRoundingOptions::default(),
            provider,
        )
    }

    /// 6.3.39 Temporal.ZonedDateTime.prototype.round
    pub fn round_with_provider(
        &self,
        options: RoundingOptions,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        // 1. Let zonedDateTime be the this value.
        // 2. Perform ? RequireInternalSlot(zonedDateTime, [[InitializedTemporalZonedDateTime]]).
        // 3. If roundTo is undefined, then
        // a. Throw a TypeError exception.
        // 4. If roundTo is a String, then
        // a. Let paramString be roundTo.
        // b. Set roundTo to OrdinaryObjectCreate(null).
        // c. Perform ! CreateDataPropertyOrThrow(roundTo, "smallestUnit", paramString).
        // 5. Else,
        // a. Set roundTo to ? GetOptionsObject(roundTo).
        // 6. NOTE: The following steps read options and perform independent validation in alphabetical order (GetRoundingIncrementOption reads "roundingIncrement" and GetRoundingModeOption reads "roundingMode").
        // 7. Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).
        // 8. Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
        // 9. Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo, "smallestUnit", time, required, « day »).
        // 10. If smallestUnit is day, then
        // a. Let maximum be 1.
        // b. Let inclusive be true.
        // 11. Else,
        // a. Let maximum be MaximumTemporalDurationRoundingIncrement(smallestUnit).
        // b. Assert: maximum is not unset.
        // c. Let inclusive be false.
        let resolved = ResolvedRoundingOptions::from_datetime_options(options)?;
        // 12. Perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, inclusive).
        // 13. If maximum is not unset, perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, false).
        // 13. If smallestUnit is nanosecond and roundingIncrement = 1, then
        if resolved.smallest_unit == Unit::Nanosecond
            && resolved.increment == RoundingIncrement::ONE
        {
            // a. Return ! CreateTemporalZonedDateTime(zonedDateTime.[[EpochNanoseconds]], zonedDateTime.[[TimeZone]], zonedDateTime.[[Calendar]]).
            return Ok(self.clone());
        }
        // 14. Let thisNs be zonedDateTime.[[EpochNanoseconds]].
        let this_ns = self.epoch_nanoseconds();
        // 15. Let timeZone be zonedDateTime.[[TimeZone]].
        // 16. Let calendar be zonedDateTime.[[Calendar]].
        // 17. Let isoDateTime be GetISODateTimeFor(timeZone, thisNs).
        // 18. If smallestUnit is day, then
        if resolved.smallest_unit == Unit::Day {
            // a. Let dateStart be isoDateTime.[[ISODate]].
            let iso_start = self.get_iso_datetime();
            // b. Let dateEnd be BalanceISODate(dateStart.[[Year]], dateStart.[[Month]], dateStart.[[Day]] + 1).
            let iso_end = IsoDate::balance(
                iso_start.date.year,
                iso_start.date.month.into(),
                i32::from(iso_start.date.day + 1),
            );
            // c. Let startNs be ? GetStartOfDay(timeZone, dateStart).
            // d. Assert: thisNs ≥ startNs.
            // e. Let endNs be ? GetStartOfDay(timeZone, dateEnd).
            // f. Assert: thisNs < endNs.
            let start = self.time_zone.get_start_of_day(&iso_start.date, provider)?;
            let end = self.time_zone.get_start_of_day(&iso_end, provider)?;
            if !(this_ns.0 >= start.ns.0 && this_ns.0 < end.ns.0) {
                return Err(TemporalError::range().with_enum(ErrorMessage::ZDTOutOfDayBounds));
            }
            // g. Let dayLengthNs be ℝ(endNs - startNs).
            // h. Let dayProgressNs be TimeDurationFromEpochNanosecondsDifference(thisNs, startNs).
            let day_len_ns = TimeDuration::from_nanosecond_difference(end.ns.0, start.ns.0)?;
            let day_progress_ns = TimeDuration::from_nanosecond_difference(this_ns.0, start.ns.0)?;
            // i. Let roundedDayNs be ! RoundTimeDurationToIncrement(dayProgressNs, dayLengthNs, roundingMode).
            let rounded = if let Some(increment) = NonZeroU128::new(day_len_ns.0.unsigned_abs()) {
                IncrementRounder::<i128>::from_signed_num(day_progress_ns.0, increment)?
                    .round(resolved.rounding_mode)
            } else {
                0 // Zero-length day: round to start of day
            };

            // The cached offset will be based on which way we round
            let offset = if rounded == 0 {
                start.offset
            } else {
                end.offset
            };

            // j. Let epochNanoseconds be AddTimeDurationToEpochNanoseconds(roundedDayNs, startNs).
            let candidate = start.ns.0 + rounded;
            Instant::try_new(candidate)?;
            // 20. Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
            ZonedDateTime::try_new_with_cached_offset(
                candidate,
                self.time_zone,
                self.calendar.clone(),
                offset,
            )
        } else {
            // 19. Else,
            // a. Let roundResult be RoundISODateTime(isoDateTime, roundingIncrement, smallestUnit, roundingMode).
            // b. Let offsetNanoseconds be GetOffsetNanosecondsFor(timeZone, thisNs).
            // c. Let epochNanoseconds be ? InterpretISODateTimeOffset(roundResult.[[ISODate]], roundResult.[[Time]], option, offsetNanoseconds, timeZone, compatible, prefer, match-exactly).
            // 20. Return ! CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).
            let iso_dt = self.get_iso_datetime();
            let rounded_dt = iso_dt.round(resolved)?;
            let offset_ns = self
                .time_zone
                .get_offset_nanos_for(this_ns.as_i128(), provider)?;

            let epoch_ns = interpret_isodatetime_offset(
                rounded_dt.date,
                Some(rounded_dt.time),
                false,
                Some(offset_ns as i64),
                &self.time_zone,
                Disambiguation::Compatible,
                OffsetDisambiguation::Prefer,
                // match-exactly
                false,
                provider,
            )?;

            ZonedDateTime::try_new_with_cached_offset(
                epoch_ns.ns.0,
                self.time_zone,
                self.calendar.clone(),
                epoch_ns.offset,
            )
        }
    }

    /// Creates an IXDTF (RFC 9557) date/time string for the provided `ZonedDateTime` according
    /// to the provided display options.
    pub fn to_ixdtf_string_with_provider(
        &self,
        display_offset: DisplayOffset,
        display_timezone: DisplayTimeZone,
        display_calendar: DisplayCalendar,
        options: ToStringRoundingOptions,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<String> {
        let resolved_options = options.resolve()?;
        let result =
            self.instant
                .round_instant(ResolvedRoundingOptions::from_to_string_options(
                    &resolved_options,
                ))?;
        let rounded_instant = Instant::try_new(result)?;

        let offset = self.time_zone.get_offset_nanos_for(result, provider)?;
        let datetime = self
            .time_zone
            .get_iso_datetime_for(&rounded_instant, provider)?;
        let (sign, hour, minute) = nanoseconds_to_formattable_offset_minutes(offset)?;
        let timezone_id = self.time_zone().identifier_with_provider(provider)?;

        let ixdtf_string = IxdtfStringBuilder::default()
            .with_date(datetime.date)
            .with_time(datetime.time, resolved_options.precision)
            .with_minute_offset(sign, hour, minute, display_offset)
            .with_timezone(&timezone_id, display_timezone)
            .with_calendar(self.calendar.identifier(), display_calendar)
            .build();

        Ok(ixdtf_string)
    }

    // TODO: Should IANA Identifier be prechecked or allow potentially invalid IANA Identifer values here?
    pub fn from_utf8_with_provider(
        source: &[u8],
        disambiguation: Disambiguation,
        offset_option: OffsetDisambiguation,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let parsed = ParsedZonedDateTime::from_utf8_with_provider(source, provider)?;

        Self::from_parsed_with_provider(parsed, disambiguation, offset_option, provider)
    }

    pub fn from_parsed_with_provider(
        parsed: ParsedZonedDateTime,
        disambiguation: Disambiguation,
        offset_option: OffsetDisambiguation,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<Self> {
        let date = IsoDate::new_with_overflow(
            parsed.date.record.year,
            parsed.date.record.month,
            parsed.date.record.day,
            Overflow::Reject,
        )?;

        let epoch_nanos = interpret_isodatetime_offset(
            date,
            parsed.time,
            parsed.has_utc_designator,
            parsed.offset.map(|o| o.nanoseconds()),
            &parsed.timezone,
            disambiguation,
            offset_option,
            parsed.match_minutes,
            provider,
        )?;
        Ok(Self::new_unchecked(
            Instant::from(epoch_nanos.ns),
            parsed.timezone,
            Calendar::new(parsed.date.calendar),
            epoch_nanos.offset,
        ))
    }
}

/// InterpretISODateTimeOffset
///
/// offsetBehavior is:
/// - OPTION if offset_nanos is Some
/// - WALL if offset_nanos is None and !is_exact
/// - EXACT if offset_nanos is None and is_exact
///
/// When offset_nanos is None, offsetNanoseconds is 0
#[allow(clippy::too_many_arguments)]
pub(crate) fn interpret_isodatetime_offset(
    date: IsoDate,
    time: Option<IsoTime>,
    is_exact: bool,
    offset_nanos: Option<i64>,
    timezone: &TimeZone,
    disambiguation: Disambiguation,
    offset_option: OffsetDisambiguation,
    match_minutes: bool,
    provider: &impl TimeZoneProvider,
) -> TemporalResult<EpochNanosecondsAndOffset> {
    // 1.  If time is start-of-day, then
    let Some(time) = time else {
        // a. Assert: offsetBehaviour is wall.
        // b. Assert: offsetNanoseconds is 0.
        temporal_assert!(offset_nanos.is_none());
        // c. Return ? GetStartOfDay(timeZone, isoDate).
        return timezone.get_start_of_day(&date, provider);
    };

    // 2. Let isoDateTime be CombineISODateAndTimeRecord(isoDate, time).
    let iso_datetime = IsoDateTime::new_unchecked(date, time);
    match (is_exact, offset_nanos, offset_option) {
        // 3. If offsetBehaviour is wall, or offsetBehaviour is option and offsetOption is ignore, then
        (false, None, _) | (_, Some(_), OffsetDisambiguation::Ignore) => {
            // a. Return ? GetEpochNanosecondsFor(timeZone, isoDateTime, disambiguation).
            timezone.get_epoch_nanoseconds_for(iso_datetime, disambiguation, provider)
        }
        // 4. If offsetBehaviour is exact, or offsetBehaviour is option and offsetOption is use, then
        (true, None, _) | (_, Some(_), OffsetDisambiguation::Use) => {
            let offset = offset_nanos.unwrap_or(0);
            // a. Let balanced be BalanceISODateTime(isoDate.[[Year]], isoDate.[[Month]],
            // isoDate.[[Day]], time.[[Hour]], time.[[Minute]], time.[[Second]], time.[[Millisecond]],
            // time.[[Microsecond]], time.[[Nanosecond]] - offsetNanoseconds).
            let iso = IsoDateTime::balance(
                date.year,
                date.month.into(),
                date.day.into(),
                time.hour.into(),
                time.minute.into(),
                time.second.into(),
                time.millisecond.into(),
                time.microsecond.into(),
                (i64::from(time.nanosecond) - offset).into(),
            );

            // b. Perform ? CheckISODaysRange(balanced.[[ISODate]]).
            iso.date.is_valid_day_range()?;

            // c. Let epochNanoseconds be GetUTCEpochNanoseconds(balanced).
            let ns = iso.as_nanoseconds();
            // d. If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a RangeError exception.
            ns.check_validity()?;

            // e. Return epochNanoseconds.
            Ok(EpochNanosecondsAndOffset {
                ns,
                offset: timezone.get_utc_offset_for(ns.0, provider)?.into(),
            })
        }
        // 5. Assert: offsetBehaviour is option.
        // 6. Assert: offsetOption is prefer or reject.
        (_, Some(offset), OffsetDisambiguation::Prefer | OffsetDisambiguation::Reject) => {
            // 7. Perform ? CheckISODaysRange(isoDate).
            date.is_valid_day_range()?;
            let iso = IsoDateTime::new_unchecked(date, time);
            // 8. Let utcEpochNanoseconds be GetUTCEpochNanoseconds(isoDateTime).
            let utc_epochs = iso.as_nanoseconds();
            // 9. Let possibleEpochNs be ? GetPossibleEpochNanoseconds(timeZone, isoDateTime).
            let possible_nanos = timezone.get_possible_epoch_ns_for(iso, provider)?;

            // 10. For each element candidate of possibleEpochNs, do
            for candidate in possible_nanos.as_slice() {
                // a. Let candidateOffset be utcEpochNanoseconds - candidate.
                let candidate_offset = utc_epochs.0 - candidate.ns.0;
                // b. If candidateOffset = offsetNanoseconds, then
                if candidate_offset == offset.into() {
                    // i. Return candidate.
                    return Ok(*candidate);
                }
                // c. If matchBehaviour is match-minutes, then
                if match_minutes {
                    // i. Let roundedCandidateNanoseconds be RoundNumberToIncrement(candidateOffset, 60 × 10**9, half-expand).
                    let rounded_candidate =
                        IncrementRounder::from_signed_num(candidate_offset, NS_PER_MINUTE_NONZERO)?
                            .round(RoundingMode::HalfExpand);
                    // ii. If roundedCandidateNanoseconds = offsetNanoseconds, then
                    if rounded_candidate == offset.into() {
                        // 1. Return candidate.
                        return Ok(*candidate);
                    }
                }
            }

            // 11. If offsetOption is reject, throw a RangeError exception.
            if offset_option == OffsetDisambiguation::Reject {
                return Err(
                    TemporalError::range().with_enum(ErrorMessage::OffsetNeedsDisambiguation)
                );
            }
            // 12. Return ? DisambiguatePossibleEpochNanoseconds(possibleEpochNs, timeZone, isoDateTime, disambiguation).
            timezone.disambiguate_possible_epoch_nanos(
                possible_nanos,
                iso,
                disambiguation,
                provider,
            )
        }
    }
}

// Formatting utils
const NS_PER_MINUTE: i128 = 60_000_000_000;
// Once MSRV is 1.83 we can update this to just calling .unwrap()
const NS_PER_MINUTE_NONZERO: NonZeroU128 = if let Some(nz) = NonZeroU128::new(NS_PER_MINUTE as u128)
{
    nz
} else {
    NonZeroU128::MIN
};

pub(crate) fn nanoseconds_to_formattable_offset_minutes(
    nanoseconds: i128,
) -> TemporalResult<(Sign, u8, u8)> {
    // Per 11.1.7 this should be rounding
    let nanoseconds = IncrementRounder::from_signed_num(nanoseconds, NS_PER_MINUTE_NONZERO)?
        .round(RoundingMode::HalfExpand);
    let offset_minutes = (nanoseconds / NS_PER_MINUTE) as i32;
    let sign = if offset_minutes < 0 {
        Sign::Negative
    } else {
        Sign::Positive
    };
    let hour = offset_minutes.abs() / 60;
    let minute = offset_minutes.abs() % 60;
    Ok((sign, hour as u8, minute as u8))
}
