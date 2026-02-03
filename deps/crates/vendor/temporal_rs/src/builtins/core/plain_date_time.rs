//! This module implements `PlainDateTime` any directly related algorithms.

use super::{
    duration::normalized::InternalDurationRecord, Duration, PartialTime, PlainDate, PlainTime,
    ZonedDateTime,
};
use crate::parsed_intermediates::ParsedDateTime;
use crate::{
    builtins::{
        calendar::CalendarFields,
        core::{calendar::Calendar, Instant},
    },
    iso::{IsoDate, IsoDateTime, IsoTime},
    options::{
        DifferenceOperation, DifferenceSettings, Disambiguation, DisplayCalendar, Overflow,
        ResolvedRoundingOptions, RoundingOptions, ToStringRoundingOptions, Unit, UnitGroup,
    },
    parsers::IxdtfStringBuilder,
    primitive::FiniteF64,
    provider::{NeverProvider, TimeZoneProvider},
    MonthCode, TemporalError, TemporalResult, TimeZone,
};
use alloc::string::String;
use core::{cmp::Ordering, str::FromStr};
use tinystr::TinyAsciiStr;
use writeable::Writeable;

/// A partial PlainDateTime record
#[derive(Debug, Default, Clone)]
pub struct PartialDateTime {
    /// The `PartialDate` portion of a `PartialDateTime`
    pub fields: DateTimeFields,
    /// The calendar of the `PartialDateTime`.
    pub calendar: Calendar,
}

#[derive(Debug, Default, Clone)]
pub struct DateTimeFields {
    pub calendar_fields: CalendarFields,
    pub time: PartialTime,
}

impl DateTimeFields {
    pub fn is_empty(&self) -> bool {
        self.calendar_fields.is_empty() && self.time.is_empty()
    }
}

impl DateTimeFields {
    pub const fn new() -> Self {
        Self {
            calendar_fields: CalendarFields::new(),
            time: PartialTime::new(),
        }
    }

    pub const fn with_partial_date(mut self, fields: CalendarFields) -> Self {
        self.calendar_fields = fields;
        self
    }

    pub const fn with_partial_time(mut self, partial_time: PartialTime) -> Self {
        self.time = partial_time;
        self
    }
}

/// The native Rust implementation of a Temporal `PlainDateTime`.
///
/// Combines a date and time into a single value representing a specific moment
/// in calendar time, such as "2024-03-15T14:30:45". Unlike `Instant`,
/// a `PlainDateTime` does not include timezone information.
///
/// Use `PlainDateTime` when you need to represent a specific date and time but
/// timezone handling is not required, or when working with local times that
/// don't need to be converted across timezones.
///
/// ## Examples
///
/// ### Creating date and time values
///
/// ```rust
/// use temporal_rs::PlainDateTime;
/// use core::str::FromStr;
///
/// // Create a specific date and time from IXDTF string
/// let meeting = PlainDateTime::from_str("2024-03-15T14:30:45.123456789").unwrap();
/// assert_eq!(meeting.year(), 2024);
/// assert_eq!(meeting.hour(), 14);
/// assert_eq!(meeting.minute(), 30);
/// ```
///
/// ### Parsing ISO 8601 datetime strings
///
/// ```rust
/// use temporal_rs::PlainDateTime;
/// use core::str::FromStr;
///
/// let dt = PlainDateTime::from_str("2024-03-15T14:30:45.123456789").unwrap();
/// assert_eq!(dt.year(), 2024);
/// assert_eq!(dt.month(), 3);
/// assert_eq!(dt.day(), 15);
/// assert_eq!(dt.hour(), 14);
/// assert_eq!(dt.minute(), 30);
/// assert_eq!(dt.second(), 45);
/// assert_eq!(dt.millisecond(), 123);
/// assert_eq!(dt.microsecond(), 456);
/// assert_eq!(dt.nanosecond(), 789);
/// ```
///
/// ### DateTime arithmetic
///
/// ```rust
/// use temporal_rs::{PlainDateTime, Duration};
/// use core::str::FromStr;
///
/// let dt = PlainDateTime::from_str("2024-01-15T12:00:00").unwrap();
///
/// // Add duration
/// let later = dt.add(&Duration::from_str("P1M2DT3H4M").unwrap(), None).unwrap();
/// assert_eq!(later.month(), 2);
/// assert_eq!(later.day(), 17);
/// assert_eq!(later.hour(), 15);
/// assert_eq!(later.minute(), 4);
///
/// // Calculate difference
/// let earlier = PlainDateTime::from_str("2024-01-10T10:30:00").unwrap();
/// let duration = earlier.until(&dt, Default::default()).unwrap();
/// assert_eq!(duration.days(), 5);
/// assert_eq!(duration.hours(), 1);
/// assert_eq!(duration.minutes(), 30);
/// ```
///
/// ### Working with partial fields
///
/// ```rust
/// use temporal_rs::{PlainDateTime, fields::DateTimeFields, partial::PartialTime};
/// use core::str::FromStr;
///
/// let dt = PlainDateTime::from_str("2024-01-15T12:30:45").unwrap();
///
/// // Change only the hour
/// let fields = DateTimeFields::new().with_partial_time(PartialTime::new().with_hour(Some(18)));
/// let modified = dt.with(fields, None).unwrap();
/// assert_eq!(modified.hour(), 18);
/// assert_eq!(modified.minute(), 30); // unchanged
/// assert_eq!(modified.second(), 45); // unchanged
/// ```
///
/// ### Converting to other types
///
/// ```rust
/// use temporal_rs::{PlainDateTime, PlainTime};
/// use core::str::FromStr;
///
/// let dt = PlainDateTime::from_str("2024-03-15T14:30:45").unwrap();
///
/// // Extract date component
/// let date = dt.to_plain_date();
/// assert_eq!(date.year(), 2024);
/// assert_eq!(date.month(), 3);
/// assert_eq!(date.day(), 15);
///
/// // Extract time component
/// let time = dt.to_plain_time();
/// assert_eq!(time.hour(), 14);
/// assert_eq!(time.minute(), 30);
/// assert_eq!(time.second(), 45);
/// ```
///
/// ## Reference
///
/// For more information, see the [MDN documentation][mdn-datetime].
///
/// [mdn-datetime]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Temporal/PlainDateTime
#[non_exhaustive]
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct PlainDateTime {
    pub(crate) iso: IsoDateTime,
    calendar: Calendar,
}

impl core::fmt::Display for PlainDateTime {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        let string =
            self.to_ixdtf_string(ToStringRoundingOptions::default(), DisplayCalendar::Auto);

        debug_assert!(
            string.is_ok(),
            "Duration must return a valid string with default options."
        );

        f.write_str(&string.map_err(|_| Default::default())?)
    }
}

// ==== Private PlainDateTime API ====

impl PlainDateTime {
    /// Creates a new unchecked `PlainDateTime`.
    #[inline]
    #[must_use]
    pub(crate) fn new_unchecked(iso: IsoDateTime, calendar: Calendar) -> Self {
        Self { iso, calendar }
    }

    // TODO: Potentially deprecate and remove.
    /// Create a new `PlainDateTime` from an `Instant`.
    #[allow(unused)]
    #[inline]
    pub(crate) fn from_instant(instant: &Instant, offset: i64, calendar: Calendar) -> Self {
        let iso = IsoDateTime::from_epoch_nanos(instant.epoch_nanoseconds(), offset);
        Self { iso, calendar }
    }

    // 5.5.14 AddDurationToOrSubtractDurationFromPlainDateTime ( operation, dateTime, temporalDurationLike, options )
    fn add_or_subtract_duration(
        &self,
        duration: &Duration,
        overflow: Option<Overflow>,
    ) -> TemporalResult<Self> {
        // SKIP: 1, 2, 3, 4
        // 5. Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
        let internal_duration = InternalDurationRecord::from_duration_with_24_hour_days(duration)?;
        // 6. Let timeResult be AddTime(dateTime.[[ISODateTime]].[[Time]], internalDuration.[[Time]]).
        let (days, time_result) = self
            .iso
            .time
            .add(internal_duration.normalized_time_duration());
        // 7. Let dateDuration be ? AdjustDateDurationRecord(internalDuration.[[Date]], timeResult.[[Days]]).
        let date_duration = internal_duration.date().adjust(days, None, None)?;
        // 8. Let addedDate be ? CalendarDateAdd(dateTime.[[Calendar]], dateTime.[[ISODateTime]].[[ISODate]], dateDuration, overflow).
        let added_date = self.calendar().date_add(
            &self.iso.date,
            &date_duration,
            overflow.unwrap_or(Overflow::Constrain),
        )?;
        // 9. Let result be CombineISODateAndTimeRecord(addedDate, timeResult).
        let result = IsoDateTime::new(added_date.iso, time_result)?;
        // 10. Return ? CreateTemporalDateTime(result, dateTime.[[Calendar]]).
        Ok(Self::new_unchecked(result, self.calendar().clone()))
    }

    /// Difference two `PlainDateTime`s together.
    pub(crate) fn diff(
        &self,
        op: DifferenceOperation,
        other: &Self,
        settings: DifferenceSettings,
    ) -> TemporalResult<Duration> {
        // 3. If ? CalendarEquals(dateTime.[[Calendar]], other.[[Calendar]]) is false, throw a RangeError exception.
        if self.calendar != other.calendar {
            return Err(TemporalError::range()
                .with_message("Calendar must be the same when diffing two PlainDateTimes"));
        }

        // 5. Let settings be ? GetDifferenceSettings(operation, resolvedOptions, datetime, « », "nanosecond", "day").
        let options = ResolvedRoundingOptions::from_diff_settings(
            settings,
            op,
            UnitGroup::DateTime,
            Unit::Day,
            Unit::Nanosecond,
        )?;

        // Step 7-8 combined.
        if self.iso == other.iso {
            return Ok(Duration::default());
        }

        // Step 10-11.
        let norm_record = self.diff_dt_with_rounding(other, options)?;

        let result = Duration::from_internal(norm_record, options.largest_unit)?;

        // Step 12
        match op {
            DifferenceOperation::Until => Ok(result),
            DifferenceOperation::Since => Ok(result.negated()),
        }
    }

    // TODO: Figure out whether to handle resolvedOptions
    // 5.5.12 DifferencePlainDateTimeWithRounding ( y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, d2, h2, min2, s2, ms2,
    // mus2, ns2, calendarRec, largestUnit, roundingIncrement, smallestUnit, roundingMode, resolvedOptions )
    pub(crate) fn diff_dt_with_rounding(
        &self,
        other: &Self,
        options: ResolvedRoundingOptions,
    ) -> TemporalResult<InternalDurationRecord> {
        // 1. If CompareISODateTime(y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, d2, h2, min2, s2, ms2, mus2, ns2) = 0, then
        if matches!(self.iso.cmp(&other.iso), Ordering::Equal) {
            // a. Let durationRecord be CreateDurationRecord(0, 0, 0, 0, 0, 0, 0, 0, 0, 0).
            // b. Return the Record { [[DurationRecord]]: durationRecord, [[Total]]: 0 }.
            return Ok(InternalDurationRecord::default());
        }
        // 2. If ISODateTimeWithinLimits(isoDateTime1) is false or ISODateTimeWithinLimits(isoDateTime2) is false, throw a RangeError exception.
        self.iso.check_within_limits()?;
        other.iso.check_within_limits()?;

        // 3. Let diff be DifferenceISODateTime(isoDateTime1, isoDateTime2, calendar, largestUnit).
        let diff = self
            .iso
            .diff(&other.iso, &self.calendar, options.largest_unit)?;
        // 4. If smallestUnit is nanosecond and roundingIncrement = 1, return diff.
        if options.smallest_unit == Unit::Nanosecond && options.increment.get() == 1 {
            return Ok(diff);
        }

        // 5. Let destEpochNs be GetUTCEpochNanoseconds(isoDateTime2).
        let dest_epoch_ns = other.iso.as_nanoseconds();
        // 6. Return ? RoundRelativeDuration(diff, destEpochNs, isoDateTime1, unset, calendar, largestUnit, roundingIncrement, smallestUnit, roundingMode).
        diff.round_relative_duration(
            dest_epoch_ns.0,
            self,
            Option::<(&TimeZone, &NeverProvider)>::None,
            options,
        )
    }

    // 5.5.14 DifferencePlainDateTimeWithTotal ( isoDateTime1, isoDateTime2, calendar, unit )
    pub(crate) fn diff_dt_with_total(&self, other: &Self, unit: Unit) -> TemporalResult<FiniteF64> {
        // 1. If CompareISODateTime(isoDateTime1, isoDateTime2) = 0, then
        //    a. Return 0.
        if matches!(self.iso.cmp(&other.iso), Ordering::Equal) {
            return FiniteF64::try_from(0.0);
        }
        // 2. If ISODateTimeWithinLimits(isoDateTime1) is false or ISODateTimeWithinLimits(isoDateTime2) is false, throw a RangeError exception.
        if !self.iso.is_within_limits() || !other.iso.is_within_limits() {
            return Err(TemporalError::range().with_message("DateTime is not within valid limits."));
        }
        // 3. Let diff be DifferenceISODateTime(isoDateTime1, isoDateTime2, calendar, unit).
        let diff = self.iso.diff(&other.iso, &self.calendar, unit)?;
        // 4. If unit is nanosecond, return diff.[[Time]].
        if unit == Unit::Nanosecond {
            return FiniteF64::try_from(diff.normalized_time_duration().0);
        }
        // 5. Let destEpochNs be GetUTCEpochNanoseconds(isoDateTime2).
        let dest_epoch_ns = other.iso.as_nanoseconds();
        // 6. Return ? TotalRelativeDuration(diff, destEpochNs, isoDateTime1, unset, calendar, unit).
        diff.total_relative_duration(
            dest_epoch_ns.0,
            self,
            Option::<(&TimeZone, &NeverProvider)>::None,
            unit,
        )
    }

    /// Returns this `PlainDateTime`'s ISO year value.
    #[inline]
    #[must_use]
    pub const fn iso_year(&self) -> i32 {
        self.iso.date.year
    }

    /// Returns this `PlainDateTime`'s ISO month value.
    #[inline]
    #[must_use]
    pub const fn iso_month(&self) -> u8 {
        self.iso.date.month
    }

    /// Returns this `PlainDateTime`'s ISO day value.
    #[inline]
    #[must_use]
    pub const fn iso_day(&self) -> u8 {
        self.iso.date.day
    }
}

// ==== Public PlainDateTime API ====

impl PlainDateTime {
    /// Creates a new `PlainDateTime`, constraining any arguments that are invalid
    /// into a valid range.
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
        calendar: Calendar,
    ) -> TemporalResult<Self> {
        Self::new_with_overflow(
            year,
            month,
            day,
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            calendar,
            Overflow::Constrain,
        )
    }

    /// Creates a new `PlainDateTime` with an ISO 8601 calendar, constraining any
    /// arguments that are invalid into a valid range.
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn new_iso(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
    ) -> TemporalResult<Self> {
        Self::new(
            year,
            month,
            day,
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            Calendar::default(),
        )
    }

    /// Creates a new `PlainDateTime`, rejecting any arguments that are not in a valid range.
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn try_new(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
        calendar: Calendar,
    ) -> TemporalResult<Self> {
        Self::new_with_overflow(
            year,
            month,
            day,
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            calendar,
            Overflow::Reject,
        )
    }

    /// Creates a new `PlainDateTime` with an ISO 8601 calendar, rejecting any arguments that are not in a valid range.
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn try_new_iso(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
    ) -> TemporalResult<Self> {
        Self::try_new(
            year,
            month,
            day,
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            Calendar::default(),
        )
    }

    /// Creates a new `PlainDateTime` with the provided [`Overflow`] option.
    #[inline]
    #[allow(clippy::too_many_arguments)]
    pub fn new_with_overflow(
        year: i32,
        month: u8,
        day: u8,
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
        calendar: Calendar,
        overflow: Overflow,
    ) -> TemporalResult<Self> {
        let iso_date = IsoDate::new_with_overflow(year, month, day, overflow)?;
        let iso_time = IsoTime::new(
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            overflow,
        )?;
        Ok(Self::new_unchecked(
            IsoDateTime::new(iso_date, iso_time)?,
            calendar,
        ))
    }

    /// Create a `PlainDateTime` from a `Date` and a `Time`.
    pub fn from_date_and_time(date: PlainDate, time: PlainTime) -> TemporalResult<Self> {
        Ok(Self::new_unchecked(
            IsoDateTime::new(date.iso, time.iso)?,
            date.calendar().clone(),
        ))
    }

    /// Creates a `PlainDateTime` from a `PartialDateTime`.
    ///
    /// ```rust
    /// use temporal_rs::{Calendar, PlainDateTime, fields::{CalendarFields, DateTimeFields}, partial::{PartialDateTime, PartialTime, PartialDate}};
    ///
    /// let calendar_fields = CalendarFields::new()
    ///     .with_year(2000)
    ///     .with_month(13)
    ///     .with_day(2);
    ///
    /// let time = PartialTime {
    ///     hour: Some(4),
    ///     minute: Some(25),
    ///     ..Default::default()
    /// };
    ///
    /// let fields = PartialDateTime {
    ///     fields: DateTimeFields {
    ///         calendar_fields,
    ///         time,
    ///     },
    ///     calendar: Calendar::ISO,
    /// };
    ///
    /// let date = PlainDateTime::from_partial(fields, None).unwrap();
    ///
    /// assert_eq!(date.year(), 2000);
    /// assert_eq!(date.month(), 12);
    /// assert_eq!(date.day(), 2);
    /// assert_eq!(date.calendar().identifier(), "iso8601");
    /// assert_eq!(date.hour(), 4);
    /// assert_eq!(date.minute(), 25);
    /// assert_eq!(date.second(), 0);
    /// assert_eq!(date.millisecond(), 0);
    ///
    /// ```
    pub fn from_partial(
        partial: PartialDateTime,
        overflow: Option<Overflow>,
    ) -> TemporalResult<Self> {
        if partial.fields.is_empty() {
            return Err(TemporalError::r#type().with_message("PartialDateTime cannot be empty."));
        }
        // The steps here largely follow `InterpretTemporalDateTimeFields`
        // 1. Let isoDate be ? CalendarDateFromFields(calendar, fields, overflow).
        let date = partial.calendar.date_from_fields(
            partial.fields.calendar_fields,
            overflow.unwrap_or(Overflow::Constrain),
        )?;
        // 2. Let time be ? RegulateTime(fields.[[Hour]], fields.[[Minute]], fields.[[Second]], fields.[[Millisecond]], fields.[[Microsecond]], fields.[[Nanosecond]], overflow).
        let iso_time =
            IsoTime::default().with(partial.fields.time, overflow.unwrap_or_default())?;
        // 3. Return CombineISODateAndTimeRecord(isoDate, time).
        let iso = IsoDateTime::new(date.iso, iso_time)?;
        Ok(Self::new_unchecked(iso, partial.calendar))
    }

    // Converts a UTF-8 encoded string into a `PlainDateTime`.
    pub fn from_utf8(s: &[u8]) -> TemporalResult<Self> {
        let parsed = ParsedDateTime::from_utf8(s)?;
        Self::from_parsed(parsed)
    }

    pub fn from_parsed(parsed: ParsedDateTime) -> TemporalResult<Self> {
        let date = IsoDate::new_with_overflow(
            parsed.date.record.year,
            parsed.date.record.month,
            parsed.date.record.day,
            Overflow::Reject,
        )?;
        let iso = IsoDateTime::new(date, parsed.time)?;

        Ok(Self::new_unchecked(
            iso,
            Calendar::new(parsed.date.calendar),
        ))
    }

    /// Creates a new `PlainDateTime` with the fields of a `PartialDateTime`.
    ///
    /// ```rust
    /// use temporal_rs::{Calendar, PlainDateTime, fields::{CalendarFields, DateTimeFields}, partial::{PartialDateTime, PartialTime, PartialDate}};
    ///
    /// let initial = PlainDateTime::try_new(2000, 12, 2, 0,0,0,0,0,0, Calendar::default()).unwrap();
    ///
    /// let calendar_fields = CalendarFields::new()
    ///     .with_month(5);
    ///
    /// let time = PartialTime {
    ///     hour: Some(4),
    ///     second: Some(30),
    ///     ..Default::default()
    /// };
    ///
    /// let fields = DateTimeFields { calendar_fields, time };
    ///
    /// let date = initial.with(fields, None).unwrap();
    ///
    /// assert_eq!(date.year(), 2000);
    /// assert_eq!(date.month(), 5);
    /// assert_eq!(date.day(), 2);
    /// assert_eq!(date.calendar().identifier(), "iso8601");
    /// assert_eq!(date.hour(), 4);
    /// assert_eq!(date.minute(), 0);
    /// assert_eq!(date.second(), 30);
    /// assert_eq!(date.millisecond(), 0);
    ///
    /// ```
    #[inline]
    pub fn with(&self, fields: DateTimeFields, overflow: Option<Overflow>) -> TemporalResult<Self> {
        if fields.is_empty() {
            return Err(
                TemporalError::r#type().with_message("A PartialDateTime must have a valid field.")
            );
        }
        let overflow = overflow.unwrap_or(Overflow::Constrain);

        let result_date = self.calendar.date_from_fields(
            fields
                .calendar_fields
                .with_fallback_datetime(self, self.calendar.kind(), overflow)?,
            overflow,
        )?;

        // Determine the `Time` based off the partial values.
        let time = self.iso.time.with(fields.time, overflow)?;

        let iso_datetime = IsoDateTime::new(result_date.iso, time)?;

        Ok(Self::new_unchecked(iso_datetime, self.calendar().clone()))
    }

    /// Creates a new `PlainDateTime` from the current `PlainDateTime` and the provided `Time`.
    #[inline]
    pub fn with_time(&self, time: Option<PlainTime>) -> TemporalResult<Self> {
        let time = time.unwrap_or_default();
        Self::try_new(
            self.iso_year(),
            self.iso_month(),
            self.iso_day(),
            time.hour(),
            time.minute(),
            time.second(),
            time.millisecond(),
            time.microsecond(),
            time.nanosecond(),
            self.calendar.clone(),
        )
    }

    /// Creates a new `PlainDateTime` from the current `PlainDateTime` and a provided [`Calendar`].
    #[inline]
    pub fn with_calendar(&self, calendar: Calendar) -> Self {
        Self::new_unchecked(self.iso, calendar)
    }

    /// Returns the hour value
    #[inline]
    #[must_use]
    pub fn hour(&self) -> u8 {
        self.iso.time.hour
    }

    /// Returns the minute value
    #[inline]
    #[must_use]
    pub fn minute(&self) -> u8 {
        self.iso.time.minute
    }

    /// Returns the second value
    #[inline]
    #[must_use]
    pub fn second(&self) -> u8 {
        self.iso.time.second
    }

    /// Returns the `millisecond` value
    #[inline]
    #[must_use]
    pub fn millisecond(&self) -> u16 {
        self.iso.time.millisecond
    }

    /// Returns the `microsecond` value
    #[inline]
    #[must_use]
    pub fn microsecond(&self) -> u16 {
        self.iso.time.microsecond
    }

    /// Returns the `nanosecond` value
    #[inline]
    #[must_use]
    pub fn nanosecond(&self) -> u16 {
        self.iso.time.nanosecond
    }

    /// Returns the Calendar value.
    #[inline]
    #[must_use]
    pub fn calendar(&self) -> &Calendar {
        &self.calendar
    }
}

// ==== Calendar-derived public API ====

impl PlainDateTime {
    /// Returns the calendar year value.
    #[inline]
    pub fn year(&self) -> i32 {
        self.calendar.year(&self.iso.date)
    }

    /// Returns the calendar month value.
    #[inline]
    pub fn month(&self) -> u8 {
        self.calendar.month(&self.iso.date)
    }

    /// Returns the calendar month code value.
    #[inline]
    pub fn month_code(&self) -> MonthCode {
        self.calendar.month_code(&self.iso.date)
    }

    /// Returns the calendar day value.
    #[inline]
    pub fn day(&self) -> u8 {
        self.calendar.day(&self.iso.date)
    }

    /// Returns the calendar day of week value.
    #[inline]
    pub fn day_of_week(&self) -> u16 {
        self.calendar.day_of_week(&self.iso.date)
    }

    /// Returns the calendar day of year value.
    #[inline]
    pub fn day_of_year(&self) -> u16 {
        self.calendar.day_of_year(&self.iso.date)
    }

    /// Returns the calendar week of year value.
    #[inline]
    pub fn week_of_year(&self) -> Option<u8> {
        self.calendar.week_of_year(&self.iso.date)
    }

    /// Returns the calendar year of week value.
    #[inline]
    pub fn year_of_week(&self) -> Option<i32> {
        self.calendar.year_of_week(&self.iso.date)
    }

    /// Returns the calendar days in week value.
    #[inline]
    pub fn days_in_week(&self) -> u16 {
        self.calendar.days_in_week(&self.iso.date)
    }

    /// Returns the calendar days in month value.
    #[inline]
    pub fn days_in_month(&self) -> u16 {
        self.calendar.days_in_month(&self.iso.date)
    }

    /// Returns the calendar days in year value.
    #[inline]
    pub fn days_in_year(&self) -> u16 {
        self.calendar.days_in_year(&self.iso.date)
    }

    /// Returns the calendar months in year value.
    #[inline]
    pub fn months_in_year(&self) -> u16 {
        self.calendar.months_in_year(&self.iso.date)
    }

    /// Returns whether the date in a leap year for the given calendar.
    #[inline]
    pub fn in_leap_year(&self) -> bool {
        self.calendar.in_leap_year(&self.iso.date)
    }

    /// Returns the era for the current `PlainDateTime`.
    #[inline]
    pub fn era(&self) -> Option<TinyAsciiStr<16>> {
        self.calendar.era(&self.iso.date)
    }

    /// Returns the era year for the current `PlainDateTime`.
    #[inline]
    pub fn era_year(&self) -> Option<i32> {
        self.calendar.era_year(&self.iso.date)
    }
}

impl PlainDateTime {
    /// Compares one `PlainDateTime` to another `PlainDateTime` using their
    /// `IsoDate` representation.
    ///
    /// # Note on Ordering
    ///
    /// `temporal_rs` does not implement `PartialOrd`/`Ord` as `PlainDateTime` does
    /// not fulfill all the conditions required to implement the traits. However,
    /// it is possible to compare `PlainDate`'s as their `IsoDate` representation.
    #[inline]
    #[must_use]
    pub fn compare_iso(&self, other: &Self) -> Ordering {
        self.iso.cmp(&other.iso)
    }

    /// Adds a [`Duration`] to the current `PlainDateTime`.
    #[inline]
    pub fn add(&self, duration: &Duration, overflow: Option<Overflow>) -> TemporalResult<Self> {
        self.add_or_subtract_duration(duration, overflow)
    }

    /// Subtracts a [`Duration`] to the current `PlainDateTime`.
    #[inline]
    pub fn subtract(
        &self,
        duration: &Duration,
        overflow: Option<Overflow>,
    ) -> TemporalResult<Self> {
        self.add_or_subtract_duration(&duration.negated(), overflow)
    }

    /// Returns a [`Duration`] representing the period of time from this `PlainDateTime` until the other `PlainDateTime`.
    #[inline]
    pub fn until(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff(DifferenceOperation::Until, other, settings)
    }

    /// Returns a [`Duration`] representing the period of time from this `PlainDateTime` since the other `PlainDateTime`.
    #[inline]
    pub fn since(&self, other: &Self, settings: DifferenceSettings) -> TemporalResult<Duration> {
        self.diff(DifferenceOperation::Since, other, settings)
    }

    /// Rounds the current `PlainDateTime` based on provided options.
    #[inline]
    pub fn round(&self, options: RoundingOptions) -> TemporalResult<Self> {
        let resolved = ResolvedRoundingOptions::from_datetime_options(options)?;

        if resolved.is_noop() {
            return Ok(self.clone());
        }

        let result = self.iso.round(resolved)?;

        Ok(Self::new_unchecked(result, self.calendar.clone()))
    }

    /// Create a [`ZonedDateTime`] from the current `PlainDateTime` with the provided options.
    #[inline]
    pub fn to_zoned_date_time_with_provider(
        &self,
        time_zone: TimeZone,
        disambiguation: Disambiguation,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<ZonedDateTime> {
        // 6. Let epochNs be ? GetEpochNanosecondsFor(timeZone, dateTime.[[ISODateTime]], disambiguation).
        let epoch_ns = time_zone.get_epoch_nanoseconds_for(self.iso, disambiguation, provider)?;
        // 7. Return ! CreateTemporalZonedDateTime(epochNs, timeZone, dateTime.[[Calendar]]).
        Ok(ZonedDateTime::new_unchecked(
            Instant::from(epoch_ns.ns),
            time_zone,
            self.calendar.clone(),
            epoch_ns.offset,
        ))
    }

    /// Create a [`PlainDate`] from the current `PlainDateTime`.
    #[inline]
    pub fn to_plain_date(&self) -> PlainDate {
        // 3. Return ! CreateTemporalDate(dateTime.[[ISODateTime]].[[ISODate]], dateTime.[[Calendar]]).
        PlainDate::new_unchecked(self.iso.date, self.calendar.clone())
    }

    /// Create a [`PlainTime`] from the current `PlainDateTime`.
    #[inline]
    pub fn to_plain_time(&self) -> PlainTime {
        // 3. Return ! CreateTemporalTime(dateTime.[[ISODateTime]].[[Time]]).
        PlainTime::new_unchecked(self.iso.time)
    }

    /// Creates an efficient [`Writeable`] implementation for the RFC 9557 IXDTF format
    /// using the current `PlainDateTime` and options.
    #[inline]
    pub fn to_ixdtf_writeable(
        &self,
        options: ToStringRoundingOptions,
        display_calendar: DisplayCalendar,
    ) -> TemporalResult<impl Writeable + '_> {
        let resolved_options = options.resolve()?;
        let result = self
            .iso
            .round(ResolvedRoundingOptions::from_to_string_options(
                &resolved_options,
            ))?;
        if !result.is_within_limits() {
            return Err(TemporalError::range().with_message("DateTime is not within valid limits."));
        }
        let builder = IxdtfStringBuilder::default()
            .with_date(result.date)
            .with_time(result.time, resolved_options.precision)
            .with_calendar(self.calendar.identifier(), display_calendar);
        Ok(builder)
    }

    /// Returns the RFC 9557 IXDTF string for the current `PlainDateTime` with the provided
    /// options.
    #[inline]
    pub fn to_ixdtf_string(
        &self,
        options: ToStringRoundingOptions,
        display_calendar: DisplayCalendar,
    ) -> TemporalResult<String> {
        self.to_ixdtf_writeable(options, display_calendar)
            .map(|x| x.write_to_string().into())
    }
}

// ==== Trait impls ====

impl FromStr for PlainDateTime {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::from_utf8(s.as_bytes())
    }
}

#[cfg(test)]
mod tests {
    use tinystr::{tinystr, TinyAsciiStr};

    use crate::{
        builtins::{
            calendar::CalendarFields,
            core::{
                calendar::Calendar, duration::DateDuration, plain_date_time::DateTimeFields,
                Duration, PartialTime, PlainDateTime,
            },
        },
        iso::{IsoDate, IsoDateTime, IsoTime},
        options::{
            DifferenceSettings, DisplayCalendar, RoundingIncrement, RoundingMode, RoundingOptions,
            ToStringRoundingOptions, Unit,
        },
        parsers::Precision,
        MonthCode, TemporalResult,
    };

    fn assert_datetime(
        dt: PlainDateTime,
        fields: (i32, u8, TinyAsciiStr<4>, u8, u8, u8, u8, u16, u16, u16),
    ) {
        assert_eq!(dt.year(), fields.0);
        assert_eq!(dt.month(), fields.1);
        assert_eq!(dt.month_code(), MonthCode(fields.2));
        assert_eq!(dt.day(), fields.3);
        assert_eq!(dt.hour(), fields.4);
        assert_eq!(dt.minute(), fields.5);
        assert_eq!(dt.second(), fields.6);
        assert_eq!(dt.millisecond(), fields.7);
        assert_eq!(dt.microsecond(), fields.8);
        assert_eq!(dt.nanosecond(), fields.9);
    }

    fn pdt_from_date(year: i32, month: u8, day: u8) -> TemporalResult<PlainDateTime> {
        PlainDateTime::try_new(year, month, day, 0, 0, 0, 0, 0, 0, Calendar::default())
    }

    #[test]
    #[allow(clippy::float_cmp)]
    fn plain_date_time_limits() {
        // This test is primarily to assert that the `expect` in the epoch methods is
        // valid, i.e., a valid instant is within the range of an f64.
        let negative_limit = pdt_from_date(-271_821, 4, 19);
        assert!(negative_limit.is_err());
        let positive_limit = pdt_from_date(275_760, 9, 14);
        assert!(positive_limit.is_err());
        let within_negative_limit = pdt_from_date(-271_821, 4, 20);
        assert_eq!(
            within_negative_limit,
            Ok(PlainDateTime {
                iso: IsoDateTime {
                    date: IsoDate {
                        year: -271_821,
                        month: 4,
                        day: 20,
                    },
                    time: IsoTime::default(),
                },
                calendar: Calendar::default(),
            })
        );

        let within_positive_limit = pdt_from_date(275_760, 9, 13);
        assert_eq!(
            within_positive_limit,
            Ok(PlainDateTime {
                iso: IsoDateTime {
                    date: IsoDate {
                        year: 275_760,
                        month: 9,
                        day: 13,
                    },
                    time: IsoTime::default(),
                },
                calendar: Calendar::default(),
            })
        );
    }

    #[test]
    fn basic_with_test() {
        let pdt =
            PlainDateTime::try_new(1976, 11, 18, 15, 23, 30, 123, 456, 789, Calendar::default())
                .unwrap();

        // Test year
        let fields = DateTimeFields {
            calendar_fields: CalendarFields::new().with_year(2019),
            time: PartialTime::default(),
        };
        let result = pdt.with(fields, None).unwrap();
        assert_datetime(
            result,
            (2019, 11, tinystr!(4, "M11"), 18, 15, 23, 30, 123, 456, 789),
        );

        // Test month
        let fields = DateTimeFields {
            calendar_fields: CalendarFields::new().with_month(5),
            time: PartialTime::default(),
        };
        let result = pdt.with(fields, None).unwrap();
        assert_datetime(
            result,
            (1976, 5, tinystr!(4, "M05"), 18, 15, 23, 30, 123, 456, 789),
        );

        // Test monthCode
        let fields = DateTimeFields {
            calendar_fields: CalendarFields::new().with_month_code(MonthCode(tinystr!(4, "M05"))),
            time: PartialTime::default(),
        };
        let result = pdt.with(fields, None).unwrap();
        assert_datetime(
            result,
            (1976, 5, tinystr!(4, "M05"), 18, 15, 23, 30, 123, 456, 789),
        );

        // Test day
        let fields = DateTimeFields {
            calendar_fields: CalendarFields::new().with_day(5),
            time: PartialTime::default(),
        };
        let result = pdt.with(fields, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 5, 15, 23, 30, 123, 456, 789),
        );

        // Test hour
        let fields = DateTimeFields {
            calendar_fields: CalendarFields::default(),
            time: PartialTime {
                hour: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(fields, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 5, 23, 30, 123, 456, 789),
        );

        // Test minute
        let fields = DateTimeFields {
            calendar_fields: CalendarFields::default(),
            time: PartialTime {
                minute: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(fields, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 15, 5, 30, 123, 456, 789),
        );

        // Test second
        let fields = DateTimeFields {
            calendar_fields: CalendarFields::default(),
            time: PartialTime {
                second: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(fields, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 15, 23, 5, 123, 456, 789),
        );

        // Test second
        let fields = DateTimeFields {
            calendar_fields: CalendarFields::default(),
            time: PartialTime {
                millisecond: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(fields, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 15, 23, 30, 5, 456, 789),
        );

        // Test second
        let fields = DateTimeFields {
            calendar_fields: CalendarFields::default(),
            time: PartialTime {
                microsecond: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(fields, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 15, 23, 30, 123, 5, 789),
        );

        // Test second
        let fields = DateTimeFields {
            calendar_fields: CalendarFields::default(),
            time: PartialTime {
                nanosecond: Some(5),
                ..Default::default()
            },
        };
        let result = pdt.with(fields, None).unwrap();
        assert_datetime(
            result,
            (1976, 11, tinystr!(4, "M11"), 18, 15, 23, 30, 123, 456, 5),
        );
    }

    #[test]
    fn datetime_with_empty_partial() {
        let pdt =
            PlainDateTime::try_new(2020, 1, 31, 12, 34, 56, 987, 654, 321, Calendar::default())
                .unwrap();

        let err = pdt.with(DateTimeFields::default(), None);
        assert!(err.is_err());
    }

    // options-undefined.js
    #[test]
    fn datetime_add_test() {
        let pdt =
            PlainDateTime::try_new(2020, 1, 31, 12, 34, 56, 987, 654, 321, Calendar::default())
                .unwrap();

        let result = pdt
            .add(
                &Duration::from(DateDuration::new(0, 1, 0, 0).unwrap()),
                None,
            )
            .unwrap();

        assert_eq!(result.month(), 2);
        assert_eq!(result.day(), 29);
    }

    // options-undefined.js
    #[test]
    fn datetime_subtract_test() {
        let pdt =
            PlainDateTime::try_new(2000, 3, 31, 12, 34, 56, 987, 654, 321, Calendar::default())
                .unwrap();

        let result = pdt
            .subtract(
                &Duration::from(DateDuration::new(0, 1, 0, 0).unwrap()),
                None,
            )
            .unwrap();

        assert_eq!(result.month(), 2);
        assert_eq!(result.day(), 29);
    }

    // subtract/hour-overflow.js
    #[test]
    fn datetime_subtract_hour_overflows() {
        let dt =
            PlainDateTime::try_new(2019, 10, 29, 10, 46, 38, 271, 986, 102, Calendar::default())
                .unwrap();

        let result = dt.subtract(&Duration::from_hours(12), None).unwrap();
        assert_datetime(
            result,
            (2019, 10, tinystr!(4, "M10"), 28, 22, 46, 38, 271, 986, 102),
        );

        let result = dt.add(&Duration::from_hours(-12), None).unwrap();
        assert_datetime(
            result,
            (2019, 10, tinystr!(4, "M10"), 28, 22, 46, 38, 271, 986, 102),
        );
    }

    fn create_diff_setting(
        smallest: Unit,
        increment: u32,
        rounding_mode: RoundingMode,
    ) -> DifferenceSettings {
        DifferenceSettings {
            largest_unit: None,
            smallest_unit: Some(smallest),
            increment: Some(RoundingIncrement::try_new(increment).unwrap()),
            rounding_mode: Some(rounding_mode),
        }
    }

    #[test]
    fn dt_until_basic() {
        let earlier =
            PlainDateTime::try_new(2019, 1, 8, 8, 22, 36, 123, 456, 789, Calendar::default())
                .unwrap();
        let later =
            PlainDateTime::try_new(2021, 9, 7, 12, 39, 40, 987, 654, 321, Calendar::default())
                .unwrap();

        let settings = create_diff_setting(Unit::Hour, 3, RoundingMode::HalfExpand);
        let result = earlier.until(&later, settings).unwrap();

        assert_eq!(result.days(), 973);
        assert_eq!(result.hours(), 3);

        let settings = create_diff_setting(Unit::Minute, 30, RoundingMode::HalfExpand);
        let result = earlier.until(&later, settings).unwrap();

        assert_eq!(result.days(), 973);
        assert_eq!(result.hours(), 4);
        assert_eq!(result.minutes(), 30);
    }

    #[test]
    fn dt_since_basic() {
        let earlier =
            PlainDateTime::try_new(2019, 1, 8, 8, 22, 36, 123, 456, 789, Calendar::default())
                .unwrap();
        let later =
            PlainDateTime::try_new(2021, 9, 7, 12, 39, 40, 987, 654, 321, Calendar::default())
                .unwrap();

        let settings = create_diff_setting(Unit::Hour, 3, RoundingMode::HalfExpand);
        let result = later.since(&earlier, settings).unwrap();

        assert_eq!(result.days(), 973);
        assert_eq!(result.hours(), 3);

        let settings = create_diff_setting(Unit::Minute, 30, RoundingMode::HalfExpand);
        let result = later.since(&earlier, settings).unwrap();

        assert_eq!(result.days(), 973);
        assert_eq!(result.hours(), 4);
        assert_eq!(result.minutes(), 30);
    }

    #[test]
    fn dt_round_basic() {
        let assert_datetime =
            |dt: PlainDateTime, expected: (i32, u8, u8, u8, u8, u8, u16, u16, u16)| {
                assert_eq!(dt.iso_year(), expected.0);
                assert_eq!(dt.iso_month(), expected.1);
                assert_eq!(dt.iso_day(), expected.2);
                assert_eq!(dt.hour(), expected.3);
                assert_eq!(dt.minute(), expected.4);
                assert_eq!(dt.second(), expected.5);
                assert_eq!(dt.millisecond(), expected.6);
                assert_eq!(dt.microsecond(), expected.7);
                assert_eq!(dt.nanosecond(), expected.8);
            };

        let gen_rounding_options = |smallest: Unit, increment: u32| -> RoundingOptions {
            RoundingOptions {
                largest_unit: None,
                smallest_unit: Some(smallest),
                increment: Some(RoundingIncrement::try_new(increment).unwrap()),
                rounding_mode: None,
            }
        };
        let dt =
            PlainDateTime::try_new(1976, 11, 18, 14, 23, 30, 123, 456, 789, Calendar::default())
                .unwrap();

        let result = dt.round(gen_rounding_options(Unit::Hour, 4)).unwrap();
        assert_datetime(result, (1976, 11, 18, 16, 0, 0, 0, 0, 0));

        let result = dt.round(gen_rounding_options(Unit::Minute, 15)).unwrap();
        assert_datetime(result, (1976, 11, 18, 14, 30, 0, 0, 0, 0));

        let result = dt.round(gen_rounding_options(Unit::Second, 30)).unwrap();
        assert_datetime(result, (1976, 11, 18, 14, 23, 30, 0, 0, 0));

        let result = dt
            .round(gen_rounding_options(Unit::Millisecond, 10))
            .unwrap();
        assert_datetime(result, (1976, 11, 18, 14, 23, 30, 120, 0, 0));

        let result = dt
            .round(gen_rounding_options(Unit::Microsecond, 10))
            .unwrap();
        assert_datetime(result, (1976, 11, 18, 14, 23, 30, 123, 460, 0));

        let result = dt
            .round(gen_rounding_options(Unit::Nanosecond, 10))
            .unwrap();
        assert_datetime(result, (1976, 11, 18, 14, 23, 30, 123, 456, 790));
    }

    #[test]
    fn datetime_round_options() {
        let dt =
            PlainDateTime::try_new(1976, 11, 18, 14, 23, 30, 123, 456, 789, Calendar::default())
                .unwrap();

        let bad_options = RoundingOptions {
            largest_unit: None,
            smallest_unit: None,
            rounding_mode: Some(RoundingMode::Ceil),
            increment: Some(RoundingIncrement::ONE),
        };

        let err = dt.round(bad_options);
        assert!(err.is_err());

        let err = dt.round(RoundingOptions::default());
        assert!(err.is_err());
    }

    // Mapped from fractionaldigits-number.js
    #[test]
    fn to_string_precision_digits() {
        let few_seconds =
            PlainDateTime::try_new(1976, 2, 4, 5, 3, 1, 0, 0, 0, Calendar::default()).unwrap();
        let zero_seconds =
            PlainDateTime::try_new(1976, 11, 18, 15, 23, 0, 0, 0, 0, Calendar::default()).unwrap();
        let whole_seconds =
            PlainDateTime::try_new(1976, 11, 18, 15, 23, 30, 0, 0, 0, Calendar::default()).unwrap();
        let subseconds =
            PlainDateTime::try_new(1976, 11, 18, 15, 23, 30, 123, 400, 0, Calendar::default())
                .unwrap();

        let options = ToStringRoundingOptions {
            precision: Precision::Digit(0),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &few_seconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-02-04T05:03:01",
            "pads parts with 0"
        );

        let options = ToStringRoundingOptions {
            precision: Precision::Digit(0),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30",
            "truncates 4 decimal places to 0"
        );

        let options = ToStringRoundingOptions {
            precision: Precision::Digit(2),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &zero_seconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:00.00",
            "pads zero seconds to 2 decimal places"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(2),
            smallest_unit: None,
            rounding_mode: None,
        };

        assert_eq!(
            &whole_seconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.00",
            "pads whole seconds to 2 decimal places"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(2),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.12",
            "truncates 4 decimal places to 2"
        );

        let options = ToStringRoundingOptions {
            precision: Precision::Digit(3),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.123",
            "truncates 4 decimal places to 3"
        );

        let options = ToStringRoundingOptions {
            precision: Precision::Digit(6),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.123400",
            "pads 4 decimal places to 6"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(7),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &zero_seconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:00.0000000",
            "pads zero seconds to 7 decimal places"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(7),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &whole_seconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.0000000",
            "pads whole seconds to 7 decimal places"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(7),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.1234000",
            "pads 4 decimal places to 7"
        );
        let options = ToStringRoundingOptions {
            precision: Precision::Digit(9),
            smallest_unit: None,
            rounding_mode: None,
        };
        assert_eq!(
            &subseconds
                .to_ixdtf_string(options, DisplayCalendar::Auto)
                .unwrap(),
            "1976-11-18T15:23:30.123400000",
            "pads 4 decimal places to 9"
        );
    }

    #[test]
    fn datetime_add() {
        use crate::{Duration, PlainDateTime};
        use core::str::FromStr;

        let dt = PlainDateTime::from_str("2024-01-15T12:00:00").unwrap();

        let duration = Duration::from_str("P1M2DT3H4M").unwrap();

        // Add duration
        let later = dt.add(&duration, None).unwrap();
        assert_eq!(later.month(), 2);
        assert_eq!(later.day(), 17);
        assert_eq!(later.hour(), 15);
        assert_eq!(later.minute(), 4);
    }
}
