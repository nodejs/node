//! This module implements the normalized `Duration` records.

use core::{cmp, num::NonZeroU128, ops::Add};

use num_traits::AsPrimitive;

use crate::{
    builtins::core::{time_zone::TimeZone, PlainDate, PlainDateTime},
    iso::{IsoDate, IsoDateTime},
    options::{
        Disambiguation, Overflow, ResolvedRoundingOptions, RoundingIncrement, RoundingMode, Unit,
        UNIT_VALUE_TABLE,
    },
    primitive::FiniteF64,
    provider::TimeZoneProvider,
    rounding::IncrementRounder,
    Calendar, TemporalError, TemporalResult, TemporalUnwrap, NS_PER_DAY, NS_PER_DAY_NONZERO,
};

use super::{DateDuration, Duration, Sign};

const MAX_TIME_DURATION: i128 = 9_007_199_254_740_991_999_999_999;

// Nanoseconds constants

const NS_PER_DAY_128BIT: i128 = NS_PER_DAY as i128;
const NANOSECONDS_PER_MINUTE: i128 = 60 * 1_000_000_000;
const NANOSECONDS_PER_HOUR: i128 = 60 * NANOSECONDS_PER_MINUTE;

// ==== TimeDuration ====
//
// A time duration represented in pure nanoseconds.
//
// Invariants:
//
// nanoseconds.abs() <= MAX_TIME_DURATION

/// A Normalized `TimeDuration` that represents the current `TimeDuration` in nanoseconds.
#[derive(Debug, Clone, Copy, Default, PartialEq, PartialOrd, Eq, Ord)]
pub(crate) struct TimeDuration(pub(crate) i128);

impl TimeDuration {
    /// Creates a `TimeDuration` from signed integer components.
    /// This method preserves the sign of each component during the calculation.
    pub(crate) fn from_components(
        hours: i64,
        minutes: i64,
        seconds: i64,
        milliseconds: i64,
        microseconds: i128,
        nanoseconds: i128,
    ) -> Self {
        let mut total_nanoseconds: i128 = 0;

        total_nanoseconds += i128::from(hours) * NANOSECONDS_PER_HOUR;
        total_nanoseconds += i128::from(minutes) * NANOSECONDS_PER_MINUTE;
        total_nanoseconds += i128::from(seconds) * 1_000_000_000;
        total_nanoseconds += i128::from(milliseconds) * 1_000_000;
        total_nanoseconds += microseconds * 1_000;
        total_nanoseconds += nanoseconds;

        debug_assert!(total_nanoseconds.abs() <= MAX_TIME_DURATION);
        Self(total_nanoseconds)
    }

    /// Equivalent: 7.5.20 NormalizeTimeDuration ( hours, minutes, seconds, milliseconds, microseconds, nanoseconds )
    pub(crate) fn from_duration(duration: &Duration) -> Self {
        // Note: Calculations must be done after casting to `i128` in order to preserve precision
        let sign_multiplier = duration.sign().as_sign_multiplier() as i128;
        let mut nanoseconds: i128 =
            i128::from(duration.hours) * NANOSECONDS_PER_HOUR * sign_multiplier;
        nanoseconds += i128::from(duration.minutes) * NANOSECONDS_PER_MINUTE * sign_multiplier;
        nanoseconds += i128::from(duration.seconds) * 1_000_000_000 * sign_multiplier;
        nanoseconds += i128::from(duration.milliseconds) * 1_000_000 * sign_multiplier;
        nanoseconds += duration.microseconds as i128 * 1_000 * sign_multiplier;
        nanoseconds += duration.nanoseconds as i128 * sign_multiplier;
        // NOTE(nekevss): Is it worth returning a `RangeError` below.
        debug_assert!(nanoseconds.abs() <= MAX_TIME_DURATION);
        Self(nanoseconds)
    }

    /// Equivalent to 7.5.27 TimeDurationFromEpochNanosecondsDifference ( one, two )
    pub(crate) fn from_nanosecond_difference(one: i128, two: i128) -> TemporalResult<Self> {
        let result = one - two;
        if result.abs() > MAX_TIME_DURATION {
            return Err(
                TemporalError::range().with_message("TimeDuration exceeds maxTimeDuration.")
            );
        }
        Ok(Self(result))
    }

    /// Equivalent: 7.5.23 Add24HourDaysToTimeDuration ( d, days )
    /// Add24HourDaysToTimeDuration??
    pub(crate) fn add_days(&self, days: i64) -> TemporalResult<Self> {
        let result = self.0 + i128::from(days) * i128::from(NS_PER_DAY);
        if result.abs() > MAX_TIME_DURATION {
            return Err(TemporalError::range()
                .with_message("SubtractTimeDuration exceeded a valid Duration range."));
        }
        Ok(Self(result))
    }

    /// `Divide the NormalizedTimeDuraiton` by a divisor, truncating
    /// the result
    pub(crate) fn truncated_divide(&self, divisor: u64) -> i128 {
        // TODO: Validate.
        self.0 / i128::from(divisor)
    }

    pub(crate) fn divide(&self, divisor: f64) -> f64 {
        self.0 as f64 / divisor
    }

    /// Equivalent: 7.5.31 TimeDurationSign ( d )
    #[inline]
    #[must_use]
    pub(crate) fn sign(&self) -> Sign {
        Sign::from(self.0.cmp(&0) as i8)
    }

    // NOTE(nekevss): non-euclid is required here for negative rounding.
    /// Return the seconds value of the `TimeDuration`.
    pub(crate) fn seconds(&self) -> i64 {
        // SAFETY: See validate_second_cast test.
        (self.0 / 1_000_000_000) as i64
    }

    // NOTE(nekevss): non-euclid is required here for negative rounding.
    /// Returns the subsecond components of the `TimeDuration`.
    pub(crate) fn subseconds(&self) -> i32 {
        // SAFETY: Remainder is 10e9 which is in range of i32
        (self.0 % 1_000_000_000) as i32
    }

    fn negate(&self) -> Self {
        Self(-self.0)
    }

    pub(crate) fn checked_sub(&self, other: &Self) -> TemporalResult<Self> {
        let result = self.0 - other.0;
        if result.abs() > MAX_TIME_DURATION {
            return Err(TemporalError::range()
                .with_message("SubtractTimeDuration exceeded a valid TimeDuration range."));
        }
        Ok(Self(result))
    }

    /// The equivalent of `RoundTimeDuration` abstract operation.
    pub(crate) fn round(&self, options: ResolvedRoundingOptions) -> TemporalResult<Self> {
        // a. Assert: The value in the "Category" column of the row of Table 22 whose "Singular" column contains unit, is time.
        // b. Let divisor be the value in the "Length in Nanoseconds" column of the row of Table 22 whose "Singular" column contains unit.
        let divisor = options.smallest_unit.as_nanoseconds().temporal_unwrap()?;
        // c. Let total be DivideTimeDuration(norm, divisor).
        let increment = options
            .increment
            .as_extended_increment()
            .checked_mul(divisor)
            .temporal_unwrap()?;
        // d. Set norm to ? RoundTimeDurationToIncrement(norm, divisor × increment, roundingMode).
        self.round_inner(increment, options.rounding_mode)
    }

    /// Equivalent: 7.5.31 TotalTimeDuration ( timeDuration, unit )
    /// TODO Fix: Arithemtic on floating point numbers is not safe. According to NOTE 2 in the spec
    pub(crate) fn total(&self, unit: Unit) -> TemporalResult<FiniteF64> {
        let time_duration = self.0;
        // 1. Let divisor be the value in the "Length in Nanoseconds" column of the row of Table 21 whose "Value" column contains unit.
        let unit_nanoseconds = unit.as_nanoseconds().temporal_unwrap()?;
        // 2. NOTE: The following step cannot be implemented directly using floating-point arithmetic when 𝔽(timeDuration) is not a safe integer.
        // The division can be implemented in C++ with the __float128 type if the compiler supports it, or with software emulation such as in the SoftFP library.
        // 3. Return timeDuration / divisor.
        DurationTotal::new(time_duration, unit_nanoseconds.get() as u64).to_fractional_total()
    }

    pub(crate) fn round_to_fractional_days(
        &self,
        increment: RoundingIncrement,
        mode: RoundingMode,
    ) -> TemporalResult<i64> {
        let adjusted_increment = increment
            .as_extended_increment()
            .saturating_mul(NS_PER_DAY_NONZERO);
        let rounded =
            IncrementRounder::<i128>::from_signed_num(self.0, adjusted_increment)?.round(mode);
        Ok((rounded / NS_PER_DAY_128BIT) as i64)
    }

    /// Round the current `TimeDuration`.
    pub(super) fn round_inner(
        &self,
        increment: NonZeroU128,
        mode: RoundingMode,
    ) -> TemporalResult<Self> {
        let rounded = IncrementRounder::<i128>::from_signed_num(self.0, increment)?.round(mode);
        if rounded.abs() > MAX_TIME_DURATION {
            return Err(
                TemporalError::range().with_message("TimeDuration exceeds maxTimeDuration.")
            );
        }
        Ok(Self(rounded))
    }

    pub(super) fn checked_add(&self, other: i128) -> TemporalResult<Self> {
        let result = self.0 + other;
        if result.abs() > MAX_TIME_DURATION {
            return Err(
                TemporalError::range().with_message("TimeDuration exceeds maxTimeDuration.")
            );
        }
        Ok(Self(result))
    }
}

// NOTE(nekevss): As this `Add` impl is fallible. Maybe it would be best implemented as a method.
/// Equivalent: 7.5.22 AddTimeDuration ( one, two )
impl Add<Self> for TimeDuration {
    type Output = TemporalResult<Self>;

    fn add(self, rhs: Self) -> Self::Output {
        let result = self.0 + rhs.0;
        if result.abs() > MAX_TIME_DURATION {
            return Err(
                TemporalError::range().with_message("TimeDuration exceeds maxTimeDuration.")
            );
        }
        Ok(Self(result))
    }
}

// Struct to handle division steps in `TotalTimeDuration`
struct DurationTotal {
    quotient: i128,
    remainder: i128,
    unit_nanoseconds: u64,
}

impl DurationTotal {
    pub fn new(time_duration: i128, unit_nanoseconds: u64) -> Self {
        let quotient = time_duration.div_euclid(unit_nanoseconds as i128);
        let remainder = time_duration.rem_euclid(unit_nanoseconds as i128);

        Self {
            quotient,
            remainder,
            unit_nanoseconds,
        }
    }

    pub(crate) fn to_fractional_total(&self) -> TemporalResult<FiniteF64> {
        let fractional = FiniteF64::try_from(self.remainder)?
            .checked_div(&FiniteF64::try_from(self.unit_nanoseconds)?)?;
        FiniteF64::try_from(self.quotient)?.checked_add(&fractional)
    }
}

// ==== Internal Duration record ====
//
// A record consisting of a DateDuration and TimeDuration
//

/// An InternalDurationRecord is a duration record that contains
/// a `DateDuration` and `TimeDuration`.
#[derive(Debug, Default, Clone, Copy)]
pub struct InternalDurationRecord {
    date: DateDuration,
    norm: TimeDuration,
}

impl InternalDurationRecord {
    pub(crate) fn combine(date: DateDuration, norm: TimeDuration) -> Self {
        // 1. Let dateSign be DateDurationSign(dateDuration).
        // 2. Let timeSign be TimeDurationSign(timeDuration).
        // 3. Assert: If dateSign ≠ 0 and timeSign ≠ 0, dateSign = timeSign.
        // 4. Return Internal Duration Record { [[Date]]: dateDuration, [[Time]]: timeDuration  }.
        Self { date, norm }
    }

    /// Creates a new `NormalizedDurationRecord`.
    ///
    /// Equivalent: `CreateNormalizedDurationRecord` & `CombineDateAndTimeDuration`.
    pub(crate) fn new(date: DateDuration, norm: TimeDuration) -> TemporalResult<Self> {
        if date.sign() != Sign::Zero && norm.sign() != Sign::Zero && date.sign() != norm.sign() {
            return Err(TemporalError::range()
                .with_message("DateDuration and TimeDuration must agree if both are not zero."));
        }
        Ok(Self { date, norm })
    }

    /// Equivalent of `7.5.6 ToInternalDurationRecordWith24HourDays ( duration )`
    ///
    /// Spec: <https://tc39.es/proposal-temporal/#sec-temporal-tointernaldurationrecordwith24hourdays>
    pub(crate) fn from_duration_with_24_hour_days(duration: &Duration) -> TemporalResult<Self> {
        // 1. Let timeDuration be TimeDurationFromComponents(duration.[[Hours]], duration.[[Minutes]],
        // duration.[[Seconds]], duration.[[Milliseconds]], duration.[[Microseconds]], duration.[[Nanoseconds]]).
        let normalized_time = TimeDuration::from_duration(duration);
        // 2. Set timeDuration to ! Add24HourDaysToTimeDuration(timeDuration, duration.[[Days]]).
        let normalized_time = normalized_time.add_days(duration.days())?;
        // 3. Let dateDuration be ! CreateDateDurationRecord(duration.[[Years]], duration.[[Months]], duration.[[Weeks]], 0).
        let date =
            DateDuration::new_unchecked(duration.years(), duration.months(), duration.weeks(), 0);
        // 4. Return CombineDateAndTimeDuration(dateDuration, timeDuration).
        Self::new(date, normalized_time)
    }

    /// Equivalent of [`7.5.7 ToDateDurationRecordWithoutTime ( duration )`][spec]
    ///
    /// [spec]: <https://tc39.es/proposal-temporal/#sec-temporal-tointernaldurationrecordwith24hourdays>
    ///
    // spec(2025-06-23): https://github.com/tc39/proposal-temporal/tree/ed49b0b482981119c9b5e28b0686d877d4a9bae0
    #[allow(clippy::wrong_self_convention)]
    pub(crate) fn to_date_duration_record_without_time(&self) -> TemporalResult<DateDuration> {
        // 1. Let internalDuration be ToInternalDurationRecordWith24HourDays(duration).
        let internal_duration = self;

        // NOTE: days SHOULD be in range of an i64.
        // MAX_TIME_DURATION / NS_PER_DAY <= i64::MAX
        // 2. Let days be truncate(internalDuration.[[Time]] / nsPerDay).
        let days = (internal_duration.normalized_time_duration().0 / i128::from(NS_PER_DAY)) as i64;

        // 3. Return ! CreateDateDurationRecord(internalDuration.[[Date]].[[Years]], internalDuration.[[Date]].[[Months]], internalDuration.[[Date]].[[Weeks]], days).
        DateDuration::new(
            internal_duration.date().years,
            internal_duration.date().months,
            internal_duration.date().weeks,
            days,
        )
    }

    pub(crate) fn from_date_duration(date: DateDuration) -> TemporalResult<Self> {
        Self::new(date, TimeDuration::default())
    }

    pub(crate) fn date(&self) -> DateDuration {
        self.date
    }

    pub(crate) fn normalized_time_duration(&self) -> TimeDuration {
        self.norm
    }

    pub(crate) fn sign(&self) -> Sign {
        let date_sign = self.date.sign();
        if date_sign == Sign::Zero {
            self.norm.sign()
        } else {
            date_sign
        }
    }
}

// ==== Nudge Duration Rounding Functions ====

// Below implements the nudge rounding functionality for Duration.
//
// Generally, this rounding is implemented on a NormalizedDurationRecord,
// which is the reason the functionality lives below.

#[derive(Debug)]
struct NudgeRecord {
    normalized: InternalDurationRecord,
    total: Option<FiniteF64>,
    nudge_epoch_ns: i128,
    expanded: bool,
}

impl InternalDurationRecord {
    // TODO: Add assertion into impl.
    // TODO: Add unit tests specifically for nudge_calendar_unit if possible.
    fn nudge_calendar_unit(
        &self,
        sign: Sign,
        dest_epoch_ns: i128,
        dt: &PlainDateTime,
        time_zone: Option<(&TimeZone, &impl TimeZoneProvider)>, // ???
        options: ResolvedRoundingOptions,
    ) -> TemporalResult<NudgeRecord> {
        // NOTE: r2 may never be used...need to test.
        let (r1, r2, start_duration, end_duration) = match options.smallest_unit {
            // 1. If unit is "year", then
            Unit::Year => {
                // a. Let years be RoundNumberToIncrement(duration.[[Years]], increment, "trunc").
                let years = IncrementRounder::from_signed_num(
                    self.date().years,
                    options.increment.as_extended_increment(),
                )?
                .round(RoundingMode::Trunc);
                // b. Let r1 be years.
                let r1 = years;
                // c. Let r2 be years + increment × sign.
                let r2 = years
                    + i128::from(options.increment.get()) * i128::from(sign.as_sign_multiplier());
                // d. Let startDuration be ? CreateNormalizedDurationRecord(r1, 0, 0, 0, ZeroTimeDuration()).
                // e. Let endDuration be ? CreateNormalizedDurationRecord(r2, 0, 0, 0, ZeroTimeDuration()).
                (
                    r1,
                    r2,
                    DateDuration::new(
                        i64::try_from(r1).map_err(|_| TemporalError::range())?,
                        0,
                        0,
                        0,
                    )?,
                    DateDuration::new(
                        i64::try_from(r2).map_err(|_| TemporalError::range())?,
                        0,
                        0,
                        0,
                    )?,
                )
            }
            // 2. Else if unit is "month", then
            Unit::Month => {
                // a. Let months be RoundNumberToIncrement(duration.[[Months]], increment, "trunc").
                let months = IncrementRounder::from_signed_num(
                    self.date().months,
                    options.increment.as_extended_increment(),
                )?
                .round(RoundingMode::Trunc);
                // b. Let r1 be months.
                let r1 = months;
                // c. Let r2 be months + increment × sign.
                let r2 = months
                    + i128::from(options.increment.get()) * i128::from(sign.as_sign_multiplier());
                // d. Let startDuration be ? CreateNormalizedDurationRecord(duration.[[Years]], r1, 0, 0, ZeroTimeDuration()).
                // e. Let endDuration be ? CreateNormalizedDurationRecord(duration.[[Years]], r2, 0, 0, ZeroTimeDuration()).
                (
                    r1,
                    r2,
                    DateDuration::new(
                        self.date().years,
                        i64::try_from(r1).map_err(|_| TemporalError::range())?,
                        0,
                        0,
                    )?,
                    DateDuration::new(
                        self.date().years,
                        i64::try_from(r2).map_err(|_| TemporalError::range())?,
                        0,
                        0,
                    )?,
                )
            }
            // 3. Else if unit is "week", then
            Unit::Week => {
                // TODO: Reconcile potential overflow on years as i32. `ValidateDuration`
                // requires years, months, weeks to be abs(x) <= 2^32.
                //
                // Do we even care? This needs tests, but even a truncated i32::MAX is still
                // FAR TOO LARGE for adding to a duration and will throw at steps c-d. This
                // is only really an issue, because we are trying to optimize out floating
                // points, but it may really show that a Duration's max range is very very
                // very big. Too big. To be tested and determined.
                //
                // In other words, our year range is roughly +/- 280_000? Let's add 2^32 to
                // that. It won't overflow, right?

                // a. Let isoResult1 be BalanceISODate(dateTime.[[Year]] + duration.[[Years]],
                // dateTime.[[Month]] + duration.[[Months]], dateTime.[[Day]]).
                let iso_one = IsoDate::try_balance(
                    dt.iso_year() + self.date().years as i32,
                    i32::from(dt.iso_month()) + self.date().months as i32,
                    i64::from(dt.iso_day()),
                )?;

                // b. Let isoResult2 be BalanceISODate(dateTime.[[Year]] + duration.[[Years]], dateTime.[[Month]] +
                // duration.[[Months]], dateTime.[[Day]] + duration.[[Days]]).
                let iso_two = IsoDate::try_balance(
                    dt.iso_year() + self.date().years as i32,
                    i32::from(dt.iso_month()) + self.date().months as i32,
                    i64::from(dt.iso_day()) + self.date().days,
                )?;

                // c. Let weeksStart be ! CreateTemporalDate(isoResult1.[[Year]], isoResult1.[[Month]], isoResult1.[[Day]],
                // calendarRec.[[Receiver]]).
                let weeks_start = PlainDate::try_new(
                    iso_one.year,
                    iso_one.month,
                    iso_one.day,
                    dt.calendar().clone(),
                )?;

                // d. Let weeksEnd be ! CreateTemporalDate(isoResult2.[[Year]], isoResult2.[[Month]], isoResult2.[[Day]],
                // calendarRec.[[Receiver]]).
                let weeks_end = PlainDate::try_new(
                    iso_two.year,
                    iso_two.month,
                    iso_two.day,
                    dt.calendar().clone(),
                )?;

                // e. Let untilOptions be OrdinaryObjectCreate(null).
                // f. Perform ! CreateDataPropertyOrThrow(untilOptions, "largestUnit", "week").
                // g. Let untilResult be ? DifferenceDate(calendarRec, weeksStart, weeksEnd, untilOptions).

                let until_result = weeks_start.internal_diff_date(&weeks_end, Unit::Week)?;

                // h. Let weeks be RoundNumberToIncrement(duration.[[Weeks]] + untilResult.[[Weeks]], increment, "trunc").
                let weeks = IncrementRounder::from_signed_num(
                    self.date().weeks + until_result.weeks(),
                    options.increment.as_extended_increment(),
                )?
                .round(RoundingMode::Trunc);

                // i. Let r1 be weeks.
                let r1 = weeks;
                // j. Let r2 be weeks + increment × sign.
                let r2 = weeks
                    + i128::from(options.increment.get()) * i128::from(sign.as_sign_multiplier());
                // k. Let startDuration be ? CreateNormalizedDurationRecord(duration.[[Years]], duration.[[Months]], r1, 0, ZeroTimeDuration()).
                // l. Let endDuration be ? CreateNormalizedDurationRecord(duration.[[Years]], duration.[[Months]], r2, 0, ZeroTimeDuration()).
                (
                    r1,
                    r2,
                    DateDuration::new(
                        self.date().years,
                        self.date().months,
                        i64::try_from(r1).map_err(|_| TemporalError::range())?,
                        0,
                    )?,
                    DateDuration::new(
                        self.date().years,
                        self.date().months,
                        i64::try_from(r2).map_err(|_| TemporalError::range())?,
                        0,
                    )?,
                )
            }
            Unit::Day => {
                // 4. Else,
                // a. Assert: unit is "day".
                // b. Let days be RoundNumberToIncrement(duration.[[Days]], increment, "trunc").
                let days = IncrementRounder::from_signed_num(
                    self.date().days,
                    options.increment.as_extended_increment(),
                )?
                .round(RoundingMode::Trunc);
                // c. Let r1 be days.
                let r1 = days;
                // d. Let r2 be days + increment × sign.
                let r2 = days
                    + i128::from(options.increment.get()) * i128::from(sign.as_sign_multiplier());
                // e. Let startDuration be ? CreateNormalizedDurationRecord(duration.[[Years]], duration.[[Months]], duration.[[Weeks]], r1, ZeroTimeDuration()).
                // f. Let endDuration be ? CreateNormalizedDurationRecord(duration.[[Years]], duration.[[Months]], duration.[[Weeks]], r2, ZeroTimeDuration()).
                (
                    r1,
                    r2,
                    DateDuration::new(
                        self.date().years,
                        self.date().months,
                        self.date().weeks,
                        i64::try_from(r1).map_err(|_| TemporalError::range())?,
                    )?,
                    DateDuration::new(
                        self.date().years,
                        self.date().months,
                        self.date().weeks,
                        i64::try_from(r2).map_err(|_| TemporalError::range())?,
                    )?,
                )
            }
            _ => {
                debug_assert!(
                    false,
                    "Found unexpected unit {} in NudgeToCalendarUnit",
                    options.smallest_unit
                );
                return Err(TemporalError::assert()
                    .with_message("NudgeCalendarUnit invoked with unexpected unit"));
            }
        };

        // 7. Let start be ? CalendarDateAdd(calendar, isoDateTime.[[ISODate]], startDuration, constrain).
        let start = dt
            .calendar()
            .date_add(&dt.iso.date, &start_duration, Overflow::Constrain)?;
        // 8. Let end be ? CalendarDateAdd(calendar, isoDateTime.[[ISODate]], endDuration, constrain).
        let end = dt
            .calendar()
            .date_add(&dt.iso.date, &end_duration, Overflow::Constrain)?;
        // 9. Let startDateTime be CombineISODateAndTimeRecord(start, isoDateTime.[[Time]]).
        let start = IsoDateTime::new_unchecked(start.iso, dt.iso.time);
        // 10. Let endDateTime be CombineISODateAndTimeRecord(end, isoDateTime.[[Time]]).
        let end = IsoDateTime::new_unchecked(end.iso, dt.iso.time);

        // 12. Else,
        let (start_epoch_ns, end_epoch_ns) = if let Some((time_zone, provider)) = time_zone {
            // a. Let startEpochNs be ? GetEpochNanosecondsFor(timeZone, startDateTime, compatible).
            // b. Let endEpochNs be ? GetEpochNanosecondsFor(timeZone, endDateTime, compatible).
            let start_epoch_ns =
                time_zone.get_epoch_nanoseconds_for(start, Disambiguation::Compatible, provider)?;
            let end_epoch_ns =
                time_zone.get_epoch_nanoseconds_for(end, Disambiguation::Compatible, provider)?;
            (start_epoch_ns.ns, end_epoch_ns.ns)
        // 11. If timeZoneRec is unset, then
        } else {
            // a. Let startEpochNs be GetUTCEpochNanoseconds(start.[[Year]], start.[[Month]], start.[[Day]], start.[[Hour]], start.[[Minute]], start.[[Second]], start.[[Millisecond]], start.[[Microsecond]], start.[[Nanosecond]]).
            // b. Let endEpochNs be GetUTCEpochNanoseconds(end.[[Year]], end.[[Month]], end.[[Day]], end.[[Hour]], end.[[Minute]], end.[[Second]], end.[[Millisecond]], end.[[Microsecond]], end.[[Nanosecond]]).
            (start.as_nanoseconds(), end.as_nanoseconds())
        };

        // TODO: look into handling asserts
        // 13. If sign is 1, then
        // a. Assert: startEpochNs ≤ destEpochNs ≤ endEpochNs.
        // 14. Else,
        // a. Assert: endEpochNs ≤ destEpochNs ≤ startEpochNs.
        // 15. Assert: startEpochNs ≠ endEpochNs.

        // TODO: Don't use f64 below ...
        // NOTE(nekevss): Step 12..13 could be problematic...need tests
        // and verify, or completely change the approach involved.
        // TODO(nekevss): Validate that the `f64` casts here are valid in all scenarios
        // 16. Let progress be (destEpochNs - startEpochNs) / (endEpochNs - startEpochNs).
        // 17. Let total be r1 + progress × increment × sign.
        let progress =
            (dest_epoch_ns - start_epoch_ns.0) as f64 / (end_epoch_ns.0 - start_epoch_ns.0) as f64;
        let total = r1 as f64
            + progress * options.increment.get() as f64 * f64::from(sign.as_sign_multiplier());

        // 14. NOTE: The above two steps cannot be implemented directly using floating-point arithmetic.
        // This division can be implemented as if constructing Normalized Time Duration Records for the denominator
        // and numerator of total and performing one division operation with a floating-point result.
        // 15. Let roundedUnit be ApplyUnsignedRoundingMode(total, r1, r2, unsignedRoundingMode).
        let rounded_unit =
            IncrementRounder::from_signed_num(total, options.increment.as_extended_increment())?
                .round(options.rounding_mode);

        // 16. If roundedUnit - total < 0, let roundedSign be -1; else let roundedSign be 1.
        // 19. Return Duration Nudge Result Record { [[Duration]]: resultDuration, [[Total]]: total, [[NudgedEpochNs]]: nudgedEpochNs, [[DidExpandCalendarUnit]]: didExpandCalendarUnit }.
        // 17. If roundedSign = sign, then
        if rounded_unit == r2 {
            // a. Let didExpandCalendarUnit be true.
            // b. Let resultDuration be endDuration.
            // c. Let nudgedEpochNs be endEpochNs.
            Ok(NudgeRecord {
                normalized: InternalDurationRecord::new(end_duration, TimeDuration::default())?,
                total: Some(FiniteF64::try_from(total)?),
                nudge_epoch_ns: end_epoch_ns.0,
                expanded: true,
            })
        // 18. Else,
        } else {
            // a. Let didExpandCalendarUnit be false.
            // b. Let resultDuration be startDuration.
            // c. Let nudgedEpochNs be startEpochNs.
            Ok(NudgeRecord {
                normalized: InternalDurationRecord::new(start_duration, TimeDuration::default())?,
                total: Some(FiniteF64::try_from(total)?),
                nudge_epoch_ns: start_epoch_ns.0,
                expanded: false,
            })
        }
    }

    // TODO: Clean up
    #[inline]
    fn nudge_to_zoned_time(
        &self,
        sign: Sign,
        dt: &PlainDateTime,
        time_zone: &TimeZone,
        options: ResolvedRoundingOptions,
        provider: &impl TimeZoneProvider,
    ) -> TemporalResult<NudgeRecord> {
        let d = self.date();
        // 1.Let start be ? CalendarDateAdd(calendar, isoDateTime.[[ISODate]], duration.[[Date]], constrain).
        let start = dt
            .calendar()
            .date_add(&dt.iso.date, &d, Overflow::Constrain)?;
        // 2. Let startDateTime be CombineISODateAndTimeRecord(start, isoDateTime.[[Time]]).
        let start_dt = IsoDateTime::new_unchecked(start.iso, dt.iso.time);

        // 3. Let endDate be BalanceISODate(start.[[Year]], start.[[Month]], start.[[Day]] + sign).
        let end_date = IsoDate::balance(
            start.iso_year(),
            start.iso_month().into(),
            // Use sign_multiplier here, not sign as i8, since spec wants sign to be nonzero (0 => +1)
            start.iso_day() as i32 + i32::from(sign.as_sign_multiplier()),
        );

        // 4. Let endDateTime be CombineISODateAndTimeRecord(endDate, isoDateTime.[[Time]]).
        let end_dt = IsoDateTime::new_unchecked(end_date, dt.iso.time);
        // 5. Let startEpochNs be ? GetEpochNanosecondsFor(timeZone, startDateTime, compatible).
        let start =
            time_zone.get_epoch_nanoseconds_for(start_dt, Disambiguation::Compatible, provider)?;
        // 6. Let endEpochNs be ? GetEpochNanosecondsFor(timeZone, endDateTime, compatible).
        let end =
            time_zone.get_epoch_nanoseconds_for(end_dt, Disambiguation::Compatible, provider)?;
        // 7. Let daySpan be TimeDurationFromEpochNanosecondsDifference(endEpochNs, startEpochNs).
        let day_span = TimeDuration::from_nanosecond_difference(end.ns.0, start.ns.0)?;
        // 8. Assert: TimeDurationSign(daySpan) = sign.
        // 9. Let unitLength be the value in the "Length in Nanoseconds" column of the row of Table 21 whose "Value" column contains unit.
        let unit_length = options.smallest_unit.as_nanoseconds().temporal_unwrap()?;
        // 10. Let roundedTimeDuration be ? RoundTimeDurationToIncrement(duration.[[Time]], increment × unitLength, roundingMode).
        let rounded_time = self.norm.round_inner(
            unit_length
                .checked_mul(options.increment.as_extended_increment())
                .temporal_unwrap()?,
            options.rounding_mode,
        )?;
        // 11. Let beyondDaySpan be ! AddTimeDuration(roundedTimeDuration, -daySpan).
        let beyond_day_span = rounded_time.checked_add(day_span.negate().0)?;
        // 12. If TimeDurationSign(beyondDaySpan) ≠ -sign, then
        let (expanded, day_delta, rounded_time, nudge_ns) =
            if beyond_day_span.sign() != sign.negate() {
                // a. Let didRoundBeyondDay be true.
                // b. Let dayDelta be sign.
                // c. Set roundedTimeDuration to ? RoundTimeDurationToIncrement(beyondDaySpan, increment × unitLength, roundingMode).
                let rounded_time = beyond_day_span.round_inner(
                    unit_length
                        .checked_mul(options.increment.as_extended_increment())
                        .temporal_unwrap()?,
                    options.rounding_mode,
                )?;
                // d. Let nudgedEpochNs be AddTimeDurationToEpochNanoseconds(roundedTimeDuration, endEpochNs).
                let nudged_ns = rounded_time.checked_add(end.ns.0)?;
                (true, sign as i8, rounded_time, nudged_ns)
            // 13. Else,
            } else {
                // a. Let didRoundBeyondDay be false.
                // b. Let dayDelta be 0.
                // c. Let nudgedEpochNs be AddTimeDurationToEpochNanoseconds(roundedTimeDuration, startEpochNs).
                let nudge_ns = rounded_time.checked_add(start.ns.0)?;
                (false, 0, rounded_time, nudge_ns)
            };
        // 14. Let dateDuration be ! AdjustDateDurationRecord(duration.[[Date]], duration.[[Date]].[[Days]] + dayDelta).
        let date = DateDuration::new(
            self.date.years,
            self.date.months,
            self.date.weeks,
            self.date
                .days
                .checked_add(day_delta.into())
                .ok_or(TemporalError::range())?,
        )?;
        // 15. Let resultDuration be CombineDateAndTimeDuration(dateDuration, roundedTimeDuration).
        let normalized = InternalDurationRecord::new(date, rounded_time)?;
        // 16. Return Duration Nudge Result Record { [[Duration]]: resultDuration, [[NudgedEpochNs]]: nudgedEpochNs, [[DidExpandCalendarUnit]]: didRoundBeyondDay }.
        Ok(NudgeRecord {
            normalized,
            nudge_epoch_ns: nudge_ns.0,
            total: None,
            expanded,
        })
    }

    #[inline]
    fn nudge_to_day_or_time(
        &self,
        dest_epoch_ns: i128,
        options: ResolvedRoundingOptions,
    ) -> TemporalResult<NudgeRecord> {
        // 1. Let timeDuration be ! Add24HourDaysToTimeDuration(duration.[[Time]], duration.[[Date]].[[Days]]).
        let time_duration = self.normalized_time_duration().add_days(self.date().days)?;
        // 2. Let unitLength be the value in the "Length in Nanoseconds" column of the row of Table 21 whose "Value" column contains smallestUnit.
        let unit_length = options.smallest_unit.as_nanoseconds().temporal_unwrap()?;
        // 3. Let roundedTime be ? RoundTimeDurationToIncrement(timeDuration, unitLength × increment, roundingMode).
        let rounded_time = time_duration.round_inner(
            unit_length
                .checked_mul(options.increment.as_extended_increment())
                .temporal_unwrap()?,
            options.rounding_mode,
        )?;

        // 4. Let diffTime be ! AddTimeDuration(roundedTime, -timeDuration).
        let diff_time = rounded_time.checked_sub(&time_duration)?;

        // 5. Let wholeDays be truncate(TotalTimeDuration(timeDuration, day)).
        let whole_days = time_duration.truncated_divide(NS_PER_DAY) as i64;

        // 6. Let roundedWholeDays be truncate(TotalTimeDuration(roundedTime, day)).
        let rounded_whole_days = rounded_time.truncated_divide(NS_PER_DAY) as i64;
        // 7. Let dayDelta be roundedWholeDays - wholeDays.
        let delta = rounded_whole_days - whole_days;
        // 8. If dayDelta < 0, let dayDeltaSign be -1; else if dayDelta > 0, let dayDeltaSign be 1; else let dayDeltaSign be 0.
        // 9. If dayDeltaSign = TimeDurationSign(timeDuration), let didExpandDays be true; else let didExpandDays be false.
        let did_expand_days = delta.signum() as i8 == time_duration.sign() as i8;
        // 10. Let nudgedEpochNs be AddTimeDurationToEpochNanoseconds(diffTime, destEpochNs).
        let nudged_ns = diff_time.0 + dest_epoch_ns;

        // 11. Let days be 0.
        let mut days = 0;
        // 12. Let remainder be roundedTime.
        let mut remainder = rounded_time;
        // 13. If TemporalUnitCategory(largestUnit) is date, then
        if options.largest_unit.is_date_unit() {
            // a. Set days to roundedWholeDays.
            days = rounded_whole_days;
            // b. Set remainder to ! AddTimeDuration(roundedTime, TimeDurationFromComponents(-roundedWholeDays * HoursPerDay, 0, 0, 0, 0, 0)).
            remainder = rounded_time.add(TimeDuration::from_components(
                -rounded_whole_days * 24,
                0,
                0,
                0,
                0,
                0,
            ))?;
        }

        // 14. Let dateDuration be ! AdjustDateDurationRecord(duration.[[Date]], days).
        let date_duration = self.date().adjust(days, None, None)?;
        // 15. Let resultDuration be CombineDateAndTimeDuration(dateDuration, remainder).
        let result_duration = Self::combine(date_duration, remainder);
        // 16. Return Duration Nudge Result Record { [[Duration]]: resultDuration, [[NudgedEpochNs]]: nudgedEpochNs, [[DidExpandCalendarUnit]]: didExpandDays }.
        Ok(NudgeRecord {
            normalized: result_duration,
            total: None,
            nudge_epoch_ns: nudged_ns,
            expanded: did_expand_days,
        })
    }

    /// `7.5.36 BubbleRelativeDuration ( sign, duration, nudgedEpochNs, isoDateTime, timeZone, calendar, largestUnit, smallestUnit )`
    ///
    /// Spec: <https://tc39.es/proposal-temporal/#sec-temporal-bubblerelativeduration>
    //
    // spec(2025-05-28): https://github.com/tc39/proposal-temporal/tree/69001e954c70e29ba3d2e6433bc7ece2a037377a
    #[inline]
    #[allow(clippy::too_many_arguments)]
    fn bubble_relative_duration(
        &self,
        sign: Sign,
        nudged_epoch_ns: i128,
        iso_date_time: &IsoDateTime,
        time_zone: Option<(&TimeZone, &impl TimeZoneProvider)>,
        calendar: &Calendar,
        largest_unit: Unit,
        smallest_unit: Unit,
    ) -> TemporalResult<InternalDurationRecord> {
        let mut duration = *self;

        // 1. If smallestUnit is largestUnit, return duration.
        if smallest_unit == largest_unit {
            return Ok(duration);
        }

        // 2. Let largestUnitIndex be the ordinal index of the row of Table 21 whose "Value" column contains largestUnit.
        let largest_unit_index = largest_unit.table_index()?;

        // 3. Let smallestUnitIndex be the ordinal index of the row of Table 21 whose "Value" column contains smallestUnit.
        let smallest_unit_index = smallest_unit.table_index()?;

        // 4. Let unitIndex be smallestUnitIndex - 1.
        // 5. Let done be false.
        // 6. Repeat, while unitIndex ≥ largestUnitIndex and done is false,
        //     a. Let unit be the value in the "Value" column of Table 21 in the row whose ordinal index is unitIndex.
        // The caller is able to set smallest_unit_index to `day` here, which would result in a backwards range get
        // We clamp to handle that case
        let clamped_upper_bound = cmp::max(smallest_unit_index, largest_unit_index);
        let unit_values = UNIT_VALUE_TABLE
            .get(largest_unit_index..clamped_upper_bound)
            .temporal_unwrap()?;
        for unit in unit_values.iter().rev().copied() {
            // b. If unit is not week, or largestUnit is week, then
            if unit != Unit::Week || largest_unit == Unit::Week {
                let end_duration = match unit {
                    // i. If unit is year, then
                    Unit::Year => {
                        // 1. Let years be duration.[[Date]].[[Years]] + sign.
                        let years = self
                            .date()
                            .years
                            .checked_add(sign.as_sign_multiplier().into())
                            .ok_or(TemporalError::range())?;

                        // 2. Let endDuration be ? CreateDateDurationRecord(years, 0, 0, 0).
                        DateDuration::new(years, 0, 0, 0)?
                    }
                    // ii. Else if unit is month, then
                    Unit::Month => {
                        // 1. Let months be duration.[[Date]].[[Months]] + sign.
                        let months = self
                            .date()
                            .months
                            .checked_add(sign.as_sign_multiplier().into())
                            .ok_or(TemporalError::range())?;

                        // 2. Let endDuration be ? AdjustDateDurationRecord(duration.[[Date]], 0, 0, months).
                        duration.date().adjust(0, Some(0), Some(months))?
                    }
                    // iii. Else,
                    unit => {
                        // 1. Assert: unit is week.
                        debug_assert!(unit == Unit::Week);

                        // 2. Let weeks be duration.[[Date]].[[Weeks]] + sign.
                        let weeks = self
                            .date()
                            .weeks
                            .checked_add(sign.as_sign_multiplier().into())
                            .ok_or(TemporalError::range())?;

                        // 3. Let endDuration be ? AdjustDateDurationRecord(duration.[[Date]], 0, weeks).
                        duration.date().adjust(0, Some(weeks), None)?
                    }
                };

                // iv. Let end be ? CalendarDateAdd(calendar, isoDateTime.[[ISODate]], endDuration, constrain).
                let end =
                    calendar.date_add(&iso_date_time.date, &end_duration, Overflow::Constrain)?;

                // v. Let endDateTime be CombineISODateAndTimeRecord(end, isoDateTime.[[Time]]).
                let end_date_time = IsoDateTime::new_unchecked(end.iso, iso_date_time.time);

                let end_epoch_ns = match time_zone {
                    // vi. If timeZone is unset, then
                    None => {
                        // 1. Let endEpochNs be GetUTCEpochNanoseconds(endDateTime).
                        end_date_time.as_nanoseconds()
                    }
                    // vii. Else,
                    Some((time_zone, time_zone_provider)) => {
                        // 1. Let endEpochNs be ? GetEpochNanosecondsFor(timeZone, endDateTime, compatible).
                        time_zone
                            .get_epoch_nanoseconds_for(
                                end_date_time,
                                Disambiguation::Compatible,
                                time_zone_provider,
                            )?
                            .ns
                    }
                };

                // viii. Let beyondEnd be nudgedEpochNs - endEpochNs.
                let beyond_end = nudged_epoch_ns - end_epoch_ns.as_i128();

                // ix. If beyondEnd < 0, let beyondEndSign be -1; else if beyondEnd > 0, let beyondEndSign be 1; else let beyondEndSign be 0.
                let beyound_end_sign = beyond_end.signum();

                // x. If beyondEndSign ≠ -sign, then
                if beyound_end_sign != -i128::from(sign.as_sign_multiplier()) {
                    // 1. Set duration to CombineDateAndTimeDuration(endDuration, 0).
                    duration = InternalDurationRecord::from_date_duration(end_duration)?;
                } else {
                    // 1. Set done to true.
                    break;
                }
            }

            // c. Set unitIndex to unitIndex - 1.
        }

        // 7. Return duration.
        Ok(duration)
    }

    /// `7.5.37 RoundRelativeDuration ( duration, destEpochNs, isoDateTime, timeZone, calendar, largestUnit, increment, smallestUnit, roundingMode )`
    ///
    /// Spec: <https://tc39.es/proposal-temporal/#sec-temporal-roundrelativeduration>
    //
    // spec(2025-05-29): https://github.com/tc39/proposal-temporal/tree/c150e7135c56afc9114032e93b53ac49f980d254
    //
    // TODO: Potentially revisit and optimize
    #[inline]
    pub(crate) fn round_relative_duration(
        &self,
        dest_epoch_ns: i128,
        dt: &PlainDateTime,
        time_zone: Option<(&TimeZone, &impl TimeZoneProvider)>,
        options: ResolvedRoundingOptions,
    ) -> TemporalResult<InternalDurationRecord> {
        let duration = *self;

        // 1. Let irregularLengthUnit be false.
        // 2. If IsCalendarUnit(smallestUnit) is true, set irregularLengthUnit to true.
        // 3. If timeZone is not unset and smallestUnit is day, set irregularLengthUnit to true.
        let irregular_length_unit = options.smallest_unit.is_calendar_unit()
            || (time_zone.is_some() && options.smallest_unit == Unit::Day);

        // 4. If InternalDurationSign(duration) < 0, let sign be -1; else let sign be 1.
        let sign = duration.sign();

        // 5. If irregularLengthUnit is true, then
        let nudge_result = if irregular_length_unit {
            // a. Let record be ? NudgeToCalendarUnit(sign, duration, destEpochNs, isoDateTime, timeZone, calendar, increment, smallestUnit, roundingMode).
            // b. Let nudgeResult be record.[[NudgeResult]].
            duration.nudge_calendar_unit(sign, dest_epoch_ns, dt, time_zone, options)?
        } else if let Some((time_zone, time_zone_provider)) = time_zone {
            // 6. Else if timeZone is not unset, then
            //      a. Let nudgeResult be ? NudgeToZonedTime(sign, duration, isoDateTime, timeZone, calendar, increment, smallestUnit, roundingMode).
            duration.nudge_to_zoned_time(sign, dt, time_zone, options, time_zone_provider)?
        } else {
            // 7. Else,
            //      a. Let nudgeResult be ? NudgeToDayOrTime(duration, destEpochNs, largestUnit, increment, smallestUnit, roundingMode).
            duration.nudge_to_day_or_time(dest_epoch_ns, options)?
        };

        // 8. Set duration to nudgeResult.[[Duration]].
        let mut duration = nudge_result.normalized;

        // 9. If nudgeResult.[[DidExpandCalendarUnit]] is true and smallestUnit is not week, then
        if nudge_result.expanded && options.smallest_unit != Unit::Week {
            // a. Let startUnit be LargerOfTwoTemporalUnits(smallestUnit, day).
            let start_unit = Unit::larger(options.smallest_unit, Unit::Day)?;

            // b. Set duration to ? BubbleRelativeDuration(sign, duration, nudgeResult.[[NudgedEpochNs]], isoDateTime, timeZone, calendar, largestUnit, startUnit).
            duration = duration.bubble_relative_duration(
                sign,
                nudge_result.nudge_epoch_ns,
                &dt.iso,
                time_zone,
                dt.calendar(),
                options.largest_unit,
                start_unit,
            )?;
        }

        // 10. Return duration.
        Ok(duration)
    }

    // 7.5.38 TotalRelativeDuration ( duration, destEpochNs, isoDateTime, timeZone, calendar, unit )
    pub(crate) fn total_relative_duration(
        &self,
        dest_epoch_ns: i128,
        dt: &PlainDateTime,
        time_zone: Option<(&TimeZone, &impl TimeZoneProvider)>,
        unit: Unit,
    ) -> TemporalResult<FiniteF64> {
        // 1. If IsCalendarUnit(unit) is true, or timeZone is not unset and unit is day, then
        if unit.is_calendar_unit() || (time_zone.is_some() && unit == Unit::Day) {
            // a. Let sign be InternalDurationSign(duration).
            let sign = self.sign();
            // b. Let record be ? NudgeToCalendarUnit(sign, duration, destEpochNs, isoDateTime, timeZone, calendar, 1, unit, trunc).
            let record = self.nudge_calendar_unit(
                sign,
                dest_epoch_ns,
                dt,
                time_zone,
                ResolvedRoundingOptions {
                    largest_unit: unit,
                    smallest_unit: unit,
                    increment: RoundingIncrement::default(),
                    rounding_mode: RoundingMode::Trunc,
                },
            )?;

            // c. Return record.[[Total]].
            return record.total.temporal_unwrap();
        }
        // 2. Let timeDuration be ! Add24HourDaysToTimeDuration(duration.[[Time]], duration.[[Date]].[[Days]]).
        let time_duration = self
            .normalized_time_duration()
            .add_days(self.date().days.as_())?;
        // Return TotalTimeDuration(timeDuration, unit).
        time_duration.total(unit)
    }
}

mod tests {
    #[test]
    fn validate_seconds_cast() {
        let max_seconds = super::MAX_TIME_DURATION.div_euclid(1_000_000_000);
        assert!(max_seconds <= i64::MAX.into())
    }

    // TODO: test f64 cast.
}
