//! This module implements the internal ISO field records.
//!
//! While these are public structs, the records are primarily
//! meant for internal Temporal calculations or calling `Calendar`
//! methods. Prefer using `PlainDateTime`, `PlainDate`, or `PlainTime`
//!
//! The three main types of records are:
//!   - `IsoDateTime`
//!   - `IsoDate`
//!   - `IsoTime`
//!
//! ## `IsoDate`
//!
//! An `IsoDate` represents the `[[ISOYear]]`, `[[ISOMonth]]`, and `[[ISODay]]` internal slots.
//!
//! ## `IsoTime`
//!
//! An `IsoTime` represents the `[[ISOHour]]`, `[[ISOMinute]]`, `[[ISOsecond]]`, `[[ISOmillisecond]]`,
//! `[[ISOmicrosecond]]`, and `[[ISOnanosecond]]` internal slots.
//!
//! ## `IsoDateTime`
//!
//! An `IsoDateTime` has the internal slots of both an `IsoDate` and `IsoTime`.

use core::num::NonZeroU128;
use ixdtf::records::TimeRecord;

use crate::{
    builtins::core::{
        calendar::Calendar,
        duration::{
            normalized::{InternalDurationRecord, TimeDuration},
            DateDuration,
        },
        PartialTime, PlainDate,
    },
    error::{ErrorMessage, TemporalError},
    options::{Overflow, ResolvedRoundingOptions, Unit},
    rounding::IncrementRounder,
    unix_time::EpochNanoseconds,
    utils, TemporalResult, TemporalUnwrap, NS_PER_DAY,
};
use icu_calendar::{Date as IcuDate, Iso};
use num_traits::{cast::FromPrimitive, Euclid};

/// `IsoDateTime` is the record of the `IsoDate` and `IsoTime` internal slots.
#[non_exhaustive]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct IsoDateTime {
    /// The `IsoDate` fields.
    pub date: IsoDate,
    /// The `IsoTime` fields.
    pub time: IsoTime,
}

impl IsoDateTime {
    /// Creates a new `IsoDateTime` without any validaiton.
    pub(crate) fn new_unchecked(date: IsoDate, time: IsoTime) -> Self {
        Self { date, time }
    }

    /// Creates a new validated `IsoDateTime` that is within valid limits.
    pub fn new(date: IsoDate, time: IsoTime) -> TemporalResult<Self> {
        if !iso_dt_within_valid_limits(date, &time) {
            return Err(
                TemporalError::range().with_message("IsoDateTime not within a valid range.")
            );
        }
        Ok(Self::new_unchecked(date, time))
    }

    /// <https://tc39.es/proposal-temporal/#sec-temporal-isodatetimewithinlimits>
    pub fn check_within_limits(&self) -> TemporalResult<()> {
        if !iso_dt_within_valid_limits(self.date, &self.time) {
            return Err(
                TemporalError::range().with_message("IsoDateTime not within a valid range.")
            );
        }
        Ok(())
    }

    // NOTE: The below assumes that nanos is from an `Instant` and thus in a valid range. -> Needs validation.
    //
    // TODO: Move away from offset use of f64
    /// Creates an `IsoDateTime` from a `BigInt` of epochNanoseconds.
    #[allow(clippy::neg_cmp_op_on_partial_ord)]
    pub(crate) fn from_epoch_nanos(epoch_nanoseconds: &EpochNanoseconds, offset: i64) -> Self {
        // Skip the assert as nanos should be validated by Instant.
        // TODO: Determine whether value needs to be validated as integral.
        // Get the component ISO parts

        // 2. Let remainderNs be epochNanoseconds modulo 10^6.
        let remainder_nanos = epoch_nanoseconds.0.rem_euclid(1_000_000);

        // 3. Let epochMilliseconds be 𝔽((epochNanoseconds - remainderNs) / 10^6).
        let epoch_millis = (epoch_nanoseconds.0 - remainder_nanos).div_euclid(1_000_000) as i64;

        let (year, month, day) = utils::ymd_from_epoch_milliseconds(epoch_millis);

        // 7. Let hour be ℝ(! HourFromTime(epochMilliseconds)).
        let hour = epoch_millis.div_euclid(3_600_000).rem_euclid(24);
        // 8. Let minute be ℝ(! MinFromTime(epochMilliserhs)conds)).
        let minute = epoch_millis.div_euclid(60_000).rem_euclid(60);
        // 9. Let second be ℝ(! SecFromTime(epochMilliseconds)).
        let second = epoch_millis.div_euclid(1000).rem_euclid(60);
        // 10. Let millisecond be ℝ(! msFromTime(epochMilliseconds)).
        let millis = epoch_millis.rem_euclid(1000);

        // 11. Let microsecond be floor(remainderNs / 1000).
        let micros = remainder_nanos.div_euclid(1_000) as i64;
        // 12. Assert: microsecond < 1000.
        debug_assert!(micros < 1000);
        // 13. Let nanosecond be remainderNs modulo 1000.
        let nanos = remainder_nanos.rem_euclid(1000) as i64;

        Self::balance(
            year,
            i32::from(month),
            i32::from(day),
            hour,
            minute,
            second,
            millis,
            micros.into(),
            (nanos + offset).into(),
        )
    }

    #[allow(clippy::too_many_arguments)]
    pub(crate) fn balance(
        year: i32,
        month: i32,
        day: i32,
        hour: i64,
        minute: i64,
        second: i64,
        millisecond: i64,
        microsecond: i128,
        nanosecond: i128,
    ) -> Self {
        let (overflow_day, time) =
            IsoTime::balance(hour, minute, second, millisecond, microsecond, nanosecond);
        // TODO: Address truncation with `try_balance`
        let date = IsoDate::balance(year, month, day + overflow_day as i32);
        Self::new_unchecked(date, time)
    }

    /// Returns whether the `IsoDateTime` is within valid limits.
    pub(crate) fn is_within_limits(&self) -> bool {
        iso_dt_within_valid_limits(self.date, &self.time)
    }

    /// Returns this `IsoDateTime` in nanoseconds
    pub fn as_nanoseconds(&self) -> EpochNanoseconds {
        utc_epoch_nanos(self.date, &self.time)
    }

    pub(crate) fn round(&self, resolved_options: ResolvedRoundingOptions) -> TemporalResult<Self> {
        let (rounded_days, rounded_time) = self.time.round(resolved_options)?;
        let balance_result = IsoDate::try_balance(
            self.date.year,
            self.date.month.into(),
            i64::from(self.date.day) + rounded_days,
        )?;
        Self::new(balance_result, rounded_time)
    }

    // TODO: UPDATE TO CURRENT SPECIFICATION
    // TODO: Determine whether to provide an options object...seems duplicative.
    /// 5.5.11 DifferenceISODateTime ( y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1, y2, mon2, d2, h2, min2, s2, ms2, mus2, ns2, calendarRec, largestUnit, options )
    pub(crate) fn diff(
        &self,
        other: &Self,
        calendar: &Calendar,
        largest_unit: Unit,
    ) -> TemporalResult<InternalDurationRecord> {
        // 1. Assert: ISODateTimeWithinLimits(y1, mon1, d1, h1, min1, s1, ms1, mus1, ns1) is true.
        // 2. Assert: ISODateTimeWithinLimits(y2, mon2, d2, h2, min2, s2, ms2, mus2, ns2) is true.
        // 3. Assert: If y1 ≠ y2, and mon1 ≠ mon2, and d1 ≠ d2, and LargerOfTwoUnits(largestUnit, "day")
        // is not "day", CalendarMethodsRecordHasLookedUp(calendarRec, date-until) is true.

        // 4. Let timeDuration be DifferenceTime(h1, min1, s1, ms1, mus1, ns1, h2, min2, s2, ms2, mus2, ns2).
        let mut time_duration = self.time.diff(&other.time);

        // 5. Let timeSign be TimeDurationSign(timeDuration).
        let time_sign = time_duration.sign() as i8;

        // 6. Let dateSign be CompareISODate(y2, mon2, d2, y1, mon1, d1).
        let date_sign = other.date.cmp(&self.date) as i32;
        // 7. Let adjustedDate be CreateISODateRecord(y2, mon2, d2).
        let mut adjusted_date = other.date;

        // 8. If timeSign = -dateSign, then
        if i32::from(time_sign) == -date_sign {
            // a. Set adjustedDate to BalanceISODate(adjustedDate.[[Year]], adjustedDate.[[Month]], adjustedDate.[[Day]] + timeSign).
            adjusted_date = IsoDate::balance(
                adjusted_date.year,
                i32::from(adjusted_date.month),
                i32::from(adjusted_date.day) + i32::from(time_sign),
            );
            // b. Set timeDuration to ? Add24HourDaysToTimeDuration(timeDuration, -timeSign).
            time_duration = time_duration.add_days(-i64::from(time_sign))?;
        }

        // 9. Let date1 be ! CreateTemporalDate(y1, mon1, d1, calendarRec.[[Receiver]]).
        let date_one = PlainDate::new_unchecked(self.date, calendar.clone());
        // 10. Let date2 be ! CreateTemporalDate(adjustedDate.[[Year]], adjustedDate.[[Month]],
        // adjustedDate.[[Day]], calendarRec.[[Receiver]]).
        let date_two = PlainDate::try_new(
            adjusted_date.year,
            adjusted_date.month,
            adjusted_date.day,
            calendar.clone(),
        )?;

        // 11. Let dateLargestUnit be LargerOfTwoUnits("day", largestUnit).
        // 12. Let untilOptions be ! SnapshotOwnProperties(options, null).
        let date_largest_unit = largest_unit.max(Unit::Day);

        // 13. Perform ! CreateDataPropertyOrThrow(untilOptions, "largestUnit", dateLargestUnit).
        // 14. Let dateDifference be ? DifferenceDate(calendarRec, date1, date2, untilOptions).
        let date_diff = date_one.internal_diff_date(&date_two, date_largest_unit)?;

        // 16. If largestUnit is not dateLargestUnit, then
        let days = if largest_unit == date_largest_unit {
            // 15. Let days be dateDifference.[[Days]].
            date_diff.days()
        } else {
            // a. Set timeDuration to ? Add24HourDaysToTimeDuration(timeDuration, dateDifference.[[Days]]).
            time_duration = time_duration.add_days(date_diff.days())?;
            // b. Set days to 0.
            0
        };

        // 17. Return ? CreateNormalizedDurationRecord(dateDifference.[[Years]], dateDifference.[[Months]], dateDifference.[[Weeks]], days, timeDuration).
        InternalDurationRecord::new(
            DateDuration::new_unchecked(
                date_diff.years(),
                date_diff.months(),
                date_diff.weeks(),
                days,
            ),
            time_duration,
        )
    }
}

// ==== `IsoDate` section ====

/// `IsoDate` serves as a record for the `[[ISOYear]]`, `[[ISOMonth]]`,
/// and `[[ISODay]]` internal fields.
///
/// These fields are used for the `Temporal.PlainDate` object, the
/// `Temporal.YearMonth` object, and the `Temporal.MonthDay` object.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, PartialOrd, Ord)]
pub struct IsoDate {
    /// An ISO year within a range -271821..=275760
    pub year: i32,
    /// An ISO month within a valid range 1..=12
    pub month: u8,
    /// An ISO day within a valid range of 1..=31
    pub day: u8,
}

impl IsoDate {
    /// Creates a new `IsoDate` without determining the validity.
    pub(crate) const fn new_unchecked(year: i32, month: u8, day: u8) -> Self {
        Self { year, month, day }
    }

    pub(crate) fn regulate(
        year: i32,
        month: u8,
        day: u8,
        overflow: Overflow,
    ) -> TemporalResult<Self> {
        match overflow {
            Overflow::Constrain => {
                let month = month.clamp(1, 12);
                let day = constrain_iso_day(year, month, day);
                // NOTE: Values are clamped in a u8 range.
                Ok(Self::new_unchecked(year, month, day))
            }
            Overflow::Reject => {
                if !is_valid_date(year, month, day) {
                    return Err(TemporalError::range().with_message("not a valid ISO date."));
                }
                // NOTE: Values have been verified to be in a u8 range.
                Ok(Self::new_unchecked(year, month, day))
            }
        }
    }

    /// <https://tc39.es/proposal-temporal/#sec-temporal-isodatetimewithinlimits>
    pub fn check_within_limits(self) -> TemporalResult<()> {
        if !iso_dt_within_valid_limits(self, &IsoTime::noon()) {
            return Err(TemporalError::range().with_message("IsoDate not within a valid range."));
        }
        Ok(())
    }

    // <https://tc39.es/proposal-temporal/#sec-temporal-isvalidisodate>
    pub fn check_validity(&self) -> TemporalResult<()> {
        if !self.is_valid() {
            return Err(
                TemporalError::range().with_message("IsoDateTime does not have valid fields.")
            );
        }
        Ok(())
    }

    pub(crate) fn new_with_overflow(
        year: i32,
        month: u8,
        day: u8,
        overflow: Overflow,
    ) -> TemporalResult<Self> {
        let date = Self::regulate(year, month, day, overflow)?;
        if !iso_dt_within_valid_limits(date, &IsoTime::noon()) {
            return Err(TemporalError::range().with_enum(ErrorMessage::DateOutOfRange));
        }
        Ok(date)
    }

    /// Create a balance date while rejecting invalid intermediates
    pub(crate) fn try_balance(year: i32, month: i32, day: i64) -> TemporalResult<Self> {
        let epoch_days = iso_date_to_epoch_days(year, month, 1) + day - 1;
        if (MAX_EPOCH_DAYS) < epoch_days.abs() {
            return Err(TemporalError::range().with_message("epoch days exceed maximum range."));
        }
        // NOTE The cast is to i32 is safe due to MAX_EPOCH_DAYS check
        let ms = utils::epoch_days_to_epoch_ms(epoch_days, 0);
        let (year, month, day) = utils::ymd_from_epoch_milliseconds(ms);
        Ok(Self::new_unchecked(year, month, day))
    }

    /// Create a balanced `IsoDate`
    ///
    /// Equivalent to `BalanceISODate`.
    pub(crate) fn balance(year: i32, month: i32, day: i32) -> Self {
        let epoch_days = iso_date_to_epoch_days(year, month, day);
        let ms = utils::epoch_days_to_epoch_ms(epoch_days, 0);
        let (year, month, day) = utils::ymd_from_epoch_milliseconds(ms);
        Self::new_unchecked(year, month, day)
    }

    pub(crate) fn is_valid_day_range(&self) -> TemporalResult<()> {
        if self.to_epoch_days().abs() > 100_000_000 {
            return Err(TemporalError::range().with_message("Not in a valid ISO day range."));
        }
        Ok(())
    }

    /// Returns this `IsoDate` in nanoseconds.
    #[inline]
    pub(crate) fn as_nanoseconds(&self) -> EpochNanoseconds {
        utc_epoch_nanos(*self, &IsoTime::default())
    }

    /// Functionally the same as Date's abstract operation `MakeDay`
    ///
    /// Equivalent to `IsoDateToEpochDays`
    #[inline]
    pub(crate) fn to_epoch_days(self) -> i64 {
        utils::epoch_days_from_gregorian_date(self.year, self.month, self.day)
    }

    /// Returns if the current `IsoDate` is valid.
    pub(crate) fn is_valid(self) -> bool {
        is_valid_date(self.year, self.month, self.day)
    }

    /// Returns the resulting `IsoDate` from adding a provided `Duration` to this `IsoDate`
    pub(crate) fn add_date_duration(
        self,
        duration: &DateDuration,
        overflow: Overflow,
    ) -> TemporalResult<Self> {
        // 1. Assert: year, month, day, years, months, weeks, and days are integers.
        // 2. Assert: overflow is either "constrain" or "reject".
        // 3. Let intermediate be ! BalanceISOYearMonth(year + years, month + months).
        let intermediate = balance_iso_year_month_with_clamp(
            i64::from(self.year) + duration.years,
            i64::from(self.month) + duration.months,
        );

        // 4. Let intermediate be ? RegulateISODate(intermediate.[[Year]], intermediate.[[Month]], day, overflow).
        let intermediate =
            Self::new_with_overflow(intermediate.0, intermediate.1, self.day, overflow)?;

        // 5. Set days to days + 7 × weeks.
        let additional_days = duration.days + (7 * duration.weeks);

        // 6. Let d be intermediate.[[Day]] + days.
        let intermediate_days = i64::from(intermediate.day) + additional_days;

        // 7. Return BalanceISODate(intermediate.[[Year]], intermediate.[[Month]], d).
        Self::try_balance(
            intermediate.year,
            intermediate.month.into(),
            intermediate_days,
        )
    }

    pub(crate) fn diff_iso_date(
        &self,
        other: &Self,
        largest_unit: Unit,
    ) -> TemporalResult<DateDuration> {
        // 1. Assert: IsValidISODate(y1, m1, d1) is true.
        // 2. Assert: IsValidISODate(y2, m2, d2) is true.
        // 3. Let sign be -CompareISODate(y1, m1, d1, y2, m2, d2).
        let sign = -(self.cmp(other) as i8);
        // 4. If sign = 0, return ! CreateDateDurationRecord(0, 0, 0, 0).
        if sign == 0 {
            return Ok(DateDuration::default());
        };

        // 5. Let years be 0.
        let mut years = 0;
        let mut months = 0;
        // 6. If largestUnit is "year", then
        if largest_unit == Unit::Year || largest_unit == Unit::Month {
            // others.year - self.year is adopted from temporal-proposal/polyfill as it saves iterations.
            // a. Let candidateYears be sign.
            let mut candidate_years: i32 = other.year - self.year;
            if candidate_years != 0 {
                candidate_years -= i32::from(sign);
            }
            // b. Repeat, while ISODateSurpasses(sign, y1 + candidateYears, m1, d1, y2, m2, d2) is false,
            while !iso_date_surpasses(
                &IsoDate::new_unchecked(self.year + candidate_years, self.month, self.day),
                other,
                sign,
            ) {
                // i. Set years to candidateYears.
                years = candidate_years;
                // ii. Set candidateYears to candidateYears + sign.
                candidate_years += i32::from(sign);
            }

            // 7. Let months be 0.
            // 8. If largestUnit is "year" or largestUnit is "month", then
            // a. Let candidateMonths be sign.
            let mut candidate_months: i32 = sign.into();
            // b. Let intermediate be BalanceISOYearMonth(y1 + years, m1 + candidateMonths).
            let mut intermediate =
                balance_iso_year_month(self.year + years, i32::from(self.month) + candidate_months);
            // c. Repeat, while ISODateSurpasses(sign, intermediate.[[Year]], intermediate.[[Month]], d1, y2, m2, d2) is false,
            // Safety: balance_iso_year_month should always return a month value from 1..=12
            while !iso_date_surpasses(
                &IsoDate::new_unchecked(intermediate.0, intermediate.1 as u8, self.day),
                other,
                sign,
            ) {
                // i. Set months to candidateMonths.
                months = candidate_months;
                // ii. Set candidateMonths to candidateMonths + sign.
                candidate_months += i32::from(sign);
                // iii. Set intermediate to BalanceISOYearMonth(intermediate.[[Year]], intermediate.[[Month]] + sign).
                intermediate = balance_iso_year_month(
                    intermediate.0,
                    i32::from(intermediate.1) + i32::from(sign),
                );
            }

            if largest_unit == Unit::Month {
                months += years * 12;
                years = 0;
            }
        }

        // 9. Set intermediate to BalanceISOYearMonth(y1 + years, m1 + months).
        let intermediate =
            balance_iso_year_month(self.year + years, i32::from(self.month) + months);
        // 10. Let constrained be ! RegulateISODate(intermediate.[[Year]], intermediate.[[Month]], d1, "constrain").
        let constrained = Self::new_with_overflow(
            intermediate.0,
            intermediate.1,
            self.day,
            Overflow::Constrain,
        )?;

        // NOTE: Below is adapted from the polyfill. Preferring this as it avoids looping.
        // 11. Let weeks be 0.
        let days = utils::epoch_days_from_gregorian_date(other.year, other.month, other.day)
            - utils::epoch_days_from_gregorian_date(
                constrained.year,
                constrained.month,
                constrained.day,
            );

        let (weeks, days) = if largest_unit == Unit::Week {
            (days / 7, days % 7)
        } else {
            (0, days)
        };

        // 17. Return ! CreateDateDurationRecord(years, months, weeks, days).
        DateDuration::new(years as i64, months as i64, weeks, days)
    }
}

impl IsoDate {
    /// Creates `[[ISOYear]]`, `[[isoMonth]]`, `[[isoDay]]` fields from `ICU4X`'s `Date<Iso>` struct.
    pub(crate) fn to_icu4x(self) -> IcuDate<Iso> {
        let d = IcuDate::try_new_iso(self.year, self.month, self.day);
        debug_assert!(d.is_ok(), "ICU4X ISODate conversion must not fail");
        d.unwrap_or_else(|_| IcuDate::from_rata_die(icu_calendar::types::RataDie::new(0), Iso))
    }
}

// ==== `IsoTime` section ====

/// An `IsoTime` record that contains `Temporal`'s
/// time slots.
#[non_exhaustive]
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct IsoTime {
    /// A valid hour value between 0..=23
    pub hour: u8, // 0..=23
    /// A valid minute value between 0..=59
    pub minute: u8, // 0..=59
    /// A valid second value between 0..=59
    pub second: u8, // 0..=59
    /// A valid millisecond value between 0..=999
    pub millisecond: u16, // 0..=999
    /// A valid microsecond value between 0..=999
    pub microsecond: u16, // 0..=999
    /// A valid nanosecond value between 0..=999
    pub nanosecond: u16, // 0..=999
}

impl IsoTime {
    /// Creates a new `IsoTime` without any validation.
    pub(crate) fn new_unchecked(
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
    ) -> Self {
        Self {
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
        }
    }

    /// Creates a new regulated `IsoTime`.
    pub fn new(
        hour: u8,
        minute: u8,
        second: u8,
        millisecond: u16,
        microsecond: u16,
        nanosecond: u16,
        overflow: Overflow,
    ) -> TemporalResult<IsoTime> {
        match overflow {
            Overflow::Constrain => {
                let h = hour.clamp(0, 23);
                let min = minute.clamp(0, 59);
                let sec = second.clamp(0, 59);
                let milli = millisecond.clamp(0, 999);
                let micro = microsecond.clamp(0, 999);
                let nano = nanosecond.clamp(0, 999);
                Ok(Self::new_unchecked(h, min, sec, milli, micro, nano))
            }
            Overflow::Reject => {
                if !is_valid_time(hour, minute, second, millisecond, microsecond, nanosecond) {
                    return Err(TemporalError::range().with_message("IsoTime is not valid"));
                };
                Ok(Self::new_unchecked(
                    hour,
                    minute,
                    second,
                    millisecond,
                    microsecond,
                    nanosecond,
                ))
            }
        }
    }

    /// Creates a new `Time` with the fields provided from a `PartialTime`.
    #[inline]
    pub(crate) fn with(&self, partial: PartialTime, overflow: Overflow) -> TemporalResult<Self> {
        let hour = partial.hour.unwrap_or(self.hour);
        let minute = partial.minute.unwrap_or(self.minute);
        let second = partial.second.unwrap_or(self.second);
        let millisecond = partial.millisecond.unwrap_or(self.millisecond);
        let microsecond = partial.microsecond.unwrap_or(self.microsecond);
        let nanosecond = partial.nanosecond.unwrap_or(self.nanosecond);
        Self::new(
            hour,
            minute,
            second,
            millisecond,
            microsecond,
            nanosecond,
            overflow,
        )
    }

    /// Returns an `IsoTime` set to 12:00:00
    pub(crate) const fn noon() -> Self {
        Self {
            hour: 12,
            minute: 0,
            second: 0,
            millisecond: 0,
            microsecond: 0,
            nanosecond: 0,
        }
    }

    /// Returns an `IsoTime` based off parse components.
    pub(crate) fn from_time_record(time_record: TimeRecord) -> TemporalResult<Self> {
        let second = time_record.second.clamp(0, 59);
        let fractional_seconds = time_record
            .fraction
            .map(|x| {
                x.to_nanoseconds().ok_or(
                    TemporalError::range()
                        .with_enum(ErrorMessage::FractionalTimeMoreThanNineDigits),
                )
            })
            .transpose()?
            .unwrap_or(0);

        let (millisecond, rem) = fractional_seconds.div_rem_euclid(&1_000_000);
        let (micros, nanos) = rem.div_rem_euclid(&1_000);

        Self::new(
            time_record.hour,
            time_record.minute,
            second,
            millisecond as u16,
            micros as u16,
            nanos as u16,
            Overflow::Reject,
        )
    }

    /// Balances and creates a new `IsoTime` with `day` overflow from the provided values.
    pub(crate) fn balance(
        hour: i64,
        minute: i64,
        second: i64,
        millisecond: i64,
        microsecond: i128,
        nanosecond: i128,
    ) -> (i64, Self) {
        // 1. Set microsecond to microsecond + floor(nanosecond / 1000).
        // 2. Set nanosecond to nanosecond modulo 1000.
        let (quotient, nanosecond) = (nanosecond.div_euclid(1000), nanosecond.rem_euclid(1000));
        let microsecond = microsecond + quotient;

        // 3. Set millisecond to millisecond + floor(microsecond / 1000).
        // 4. Set microsecond to microsecond modulo 1000.
        let (quotient, microsecond) = (microsecond.div_euclid(1000), microsecond.rem_euclid(1000));
        let millisecond = millisecond + quotient as i64;

        // 5. Set second to second + floor(millisecond / 1000).
        // 6. Set millisecond to millisecond modulo 1000.
        let (quotient, millisecond) = div_mod(millisecond, 1000);
        let second = second + quotient;

        // 7. Set minute to minute + floor(second / 60).
        // 8. Set second to second modulo 60.
        let (quotient, second) = div_mod(second, 60);
        let minute = minute + quotient;

        // 9. Set hour to hour + floor(minute / 60).
        // 10. Set minute to minute modulo 60.
        let (quotient, minute) = div_mod(minute, 60);
        let hour = hour + quotient;

        // 11. Let days be floor(hour / 24).
        // 12. Set hour to hour modulo 24.
        let (days, hour) = div_mod(hour, 24);

        let time = Self::new_unchecked(
            hour as u8,
            minute as u8,
            second as u8,
            millisecond as u16,
            microsecond as u16,
            nanosecond as u16,
        );

        (days, time)
    }

    /// Difference this `IsoTime` against another and returning a `TimeDuration`.
    pub(crate) fn diff(&self, other: &Self) -> TimeDuration {
        let h = i64::from(other.hour) - i64::from(self.hour);
        let m = i64::from(other.minute) - i64::from(self.minute);
        let s = i64::from(other.second) - i64::from(self.second);
        let ms = i64::from(other.millisecond) - i64::from(self.millisecond);
        let mis = i128::from(other.microsecond) - i128::from(self.microsecond);
        let ns = i128::from(other.nanosecond) - i128::from(self.nanosecond);

        TimeDuration::from_components(h, m, s, ms, mis, ns)
    }

    // NOTE (nekevss): Specification seemed to be off / not entirely working, so the below was adapted from the
    // temporal-polyfill
    // TODO: DayLengthNS can probably be a u64, but keep as is for now and optimize.
    /// Rounds the current `IsoTime` according to the provided settings.
    pub(crate) fn round(
        &self,
        resolved_options: ResolvedRoundingOptions,
    ) -> TemporalResult<(i64, Self)> {
        // 1. If unit is "day" or "hour", then
        let quantity = match resolved_options.smallest_unit {
            Unit::Day | Unit::Hour => {
                // a. Let quantity be ((((hour × 60 + minute) × 60 + second) × 1000 + millisecond)
                // × 1000 + microsecond) × 1000 + nanosecond.
                let minutes = i128::from(self.hour) * 60 + i128::from(self.minute);
                let seconds = minutes * 60 + i128::from(self.second);
                let millis = seconds * 1000 + i128::from(self.millisecond);
                let micros = millis * 1000 + i128::from(self.microsecond);
                micros * 1000 + i128::from(self.nanosecond)
            }
            // 2. Else if unit is "minute", then
            Unit::Minute => {
                // a. Let quantity be (((minute × 60 + second) × 1000 + millisecond) × 1000 + microsecond) × 1000 + nanosecond.
                let seconds = i128::from(self.minute) * 60 + i128::from(self.second);
                let millis = seconds * 1000 + i128::from(self.millisecond);
                let micros = millis * 1000 + i128::from(self.microsecond);
                micros * 1000 + i128::from(self.nanosecond)
            }
            // 3. Else if unit is "second", then
            Unit::Second => {
                // a. Let quantity be ((second × 1000 + millisecond) × 1000 + microsecond) × 1000 + nanosecond.
                let millis = i128::from(self.second) * 1000 + i128::from(self.millisecond);
                let micros = millis * 1000 + i128::from(self.microsecond);
                micros * 1000 + i128::from(self.nanosecond)
            }
            // 4. Else if unit is "millisecond", then
            Unit::Millisecond => {
                // a. Let quantity be (millisecond × 1000 + microsecond) × 1000 + nanosecond.
                let micros = i128::from(self.millisecond) * 1000 + i128::from(self.microsecond);
                micros * 1000 + i128::from(self.nanosecond)
            }
            // 5. Else if unit is "microsecond", then
            Unit::Microsecond => {
                // a. Let quantity be microsecond × 1000 + nanosecond.
                i128::from(self.microsecond) * 1000 + i128::from(self.nanosecond)
            }
            // 6. Else,
            Unit::Nanosecond => {
                // a. Assert: unit is "nanosecond".
                // b. Let quantity be nanosecond.
                i128::from(self.nanosecond)
            }
            _ => {
                return Err(TemporalError::range()
                    .with_message("Invalid smallestUnit value for time rounding."))
            }
        };
        // 7. Let unitLength be the value in the "Length in Nanoseconds" column of the row of Table 22 whose "Singular" column contains unit.
        let length = NonZeroU128::new(
            resolved_options
                .smallest_unit
                .as_nanoseconds()
                .temporal_unwrap()?
                .into(),
        )
        .temporal_unwrap()?;

        let increment = resolved_options
            .increment
            .as_extended_increment()
            .checked_mul(length)
            .ok_or(TemporalError::range().with_message("increment exceeded valid range."))?;

        // 8. Let result be RoundNumberToIncrement(quantity, increment × unitLength, roundingMode) / unitLength.
        let result = IncrementRounder::<i128>::from_signed_num(quantity, increment)?
            .round(resolved_options.rounding_mode)
            / length.get() as i128;

        let result_i64 = i64::from_i128(result)
            .ok_or(TemporalError::range().with_message("round result valid range."))?;

        match resolved_options.smallest_unit {
            // 9. If unit is "day", then
            // a. Return Time Record { [[Days]]: result, [[Hour]]: 0, [[Minute]]: 0, [[Second]]: 0, [[Millisecond]]: 0, [[Microsecond]]: 0, [[Nanosecond]]: 0  }.
            Unit::Day => Ok((result_i64, Self::default())),
            // 10. If unit is "hour", then
            // a. Return BalanceTime(result, 0, 0, 0, 0, 0).
            Unit::Hour => Ok(Self::balance(result_i64, 0, 0, 0, 0, 0)),
            // 11. If unit is "minute", then
            // a. Return BalanceTime(hour, result, 0.0, 0.0, 0.0, 0).
            Unit::Minute => Ok(Self::balance(self.hour.into(), result_i64, 0, 0, 0, 0)),
            // 12. If unit is "second", then
            // a. Return BalanceTime(hour, minute, result, 0.0, 0.0, 0).
            Unit::Second => Ok(Self::balance(
                self.hour.into(),
                self.minute.into(),
                result_i64,
                0,
                0,
                0,
            )),
            // 13. If unit is "millisecond", then
            // a. Return BalanceTime(hour, minute, second, result, 0.0, 0).
            Unit::Millisecond => Ok(Self::balance(
                self.hour.into(),
                self.minute.into(),
                self.second.into(),
                result_i64,
                0,
                0,
            )),
            // 14. If unit is "microsecond", then
            // a. Return BalanceTime(hour, minute, second, millisecond, result, 0).
            Unit::Microsecond => Ok(Self::balance(
                self.hour.into(),
                self.minute.into(),
                self.second.into(),
                self.millisecond.into(),
                result_i64.into(),
                0,
            )),
            // 15. Assert: unit is "nanosecond".
            // 16. Return BalanceTime(hour, minute, second, millisecond, microsecond, result).
            Unit::Nanosecond => Ok(Self::balance(
                self.hour.into(),
                self.minute.into(),
                self.second.into(),
                self.millisecond.into(),
                self.microsecond.into(),
                result_i64.into(),
            )),
            _ => Err(TemporalError::assert()),
        }
    }

    pub(crate) fn add(&self, norm: TimeDuration) -> (i64, Self) {
        // 1. Set second to second + TimeDurationSeconds(norm).
        let seconds = i64::from(self.second) + norm.seconds();
        // 2. Set nanosecond to nanosecond + TimeDurationSubseconds(norm).
        let nanos = i32::from(self.nanosecond) + norm.subseconds();
        // 3. Return BalanceTime(hour, minute, second, millisecond, microsecond, nanosecond).
        Self::balance(
            self.hour.into(),
            self.minute.into(),
            seconds,
            self.millisecond.into(),
            self.microsecond.into(),
            nanos.into(),
        )
    }

    /// `IsoTimeToEpochMs`
    ///
    /// Note: This method is library specific and not in spec
    ///
    /// Functionally the same as Date's `MakeTime`
    pub(crate) fn to_epoch_ms(self) -> i64 {
        ((i64::from(self.hour) * utils::MS_PER_HOUR
            + i64::from(self.minute) * utils::MS_PER_MINUTE)
            + i64::from(self.second) * 1000i64)
            + i64::from(self.millisecond)
    }
}

// ==== `IsoDateTime` specific utility functions ====

const MAX_EPOCH_DAYS: i64 = 10i64.pow(8) + 1;

#[inline]
/// Utility function to determine if a `DateTime`'s components create a `DateTime` within valid limits
fn iso_dt_within_valid_limits(date: IsoDate, time: &IsoTime) -> bool {
    if utils::epoch_days_from_gregorian_date(date.year, date.month, date.day).abs() > MAX_EPOCH_DAYS
    {
        return false;
    }

    let ns = to_unchecked_epoch_nanoseconds(date, time);
    let max = crate::NS_MAX_INSTANT + i128::from(NS_PER_DAY);
    let min = crate::NS_MIN_INSTANT - i128::from(NS_PER_DAY);

    min < ns && max > ns
}

#[inline]
/// Utility function to convert a `IsoDate` and `IsoTime` values into epoch nanoseconds
fn utc_epoch_nanos(date: IsoDate, time: &IsoTime) -> EpochNanoseconds {
    let epoch_nanos = to_unchecked_epoch_nanoseconds(date, time);
    EpochNanoseconds::from(epoch_nanos)
}

#[inline]
fn to_unchecked_epoch_nanoseconds(date: IsoDate, time: &IsoTime) -> i128 {
    let ms = time.to_epoch_ms();
    let epoch_ms = utils::epoch_days_to_epoch_ms(date.to_epoch_days(), ms);
    epoch_ms as i128 * 1_000_000 + time.microsecond as i128 * 1_000 + time.nanosecond as i128
}

// ==== `IsoDate` specific utiltiy functions ====

/// Returns the Epoch days based off the given year, month, and day.
/// Note: Month should be 1 indexed
#[inline]
pub(crate) fn iso_date_to_epoch_days(year: i32, month: i32, day: i32) -> i64 {
    // 1. Let resolvedYear be year + floor(month / 12).
    let resolved_year = year + month.div_euclid(12);
    // 2. Let resolvedMonth be month modulo 12.
    let resolved_month = month.rem_euclid(12) as u8;
    // 3. Find a time t such that EpochTimeToEpochYear(t) is resolvedYear,
    // EpochTimeToMonthInYear(t) is resolvedMonth, and EpochTimeToDate(t) is 1.
    let epoch_days = utils::epoch_days_from_gregorian_date(resolved_year, resolved_month, 1);

    // 4. Return EpochTimeToDayNumber(t) + date - 1.
    epoch_days + day as i64 - 1
}

#[inline]
// Determines if the month and day are valid for the given year.
pub(crate) fn is_valid_date(year: i32, month: u8, day: u8) -> bool {
    if !(1..=12).contains(&month) {
        return false;
    }
    is_valid_iso_day(year, month, day)
}

#[inline]
/// Returns with the `this` surpasses `other`.
fn iso_date_surpasses(this: &IsoDate, other: &IsoDate, sign: i8) -> bool {
    this.cmp(other) as i8 * sign == 1
}

#[inline]
pub(crate) fn year_month_within_limits(year: i32, month: u8) -> bool {
    // 1. If isoDate.[[Year]] < -271821 or isoDate.[[Year]] > 275760, then
    if !(-271821..=275760).contains(&year) {
        // a. Return false.
        return false;
    // 2. If isoDate.[[Year]] = -271821 and isoDate.[[Month]] < 4, then
    } else if year == -271821 && month < 4 {
        // a. Return false.
        return false;
    // 3. If isoDate.[[Year]] = 275760 and isoDate.[[Month]] > 9, then
    } else if year == 275760 && month > 9 {
        // a. Return false.
        return false;
    }
    // 4. Return true.
    true
}

fn balance_iso_year_month_with_clamp(year: i64, month: i64) -> (i32, u8) {
    // 1. Assert: year and month are integers.
    // 2. Set year to year + floor((month - 1) / 12).
    let y = year + (month - 1).div_euclid(12);
    // 3. Set month to ((month - 1) modulo 12) + 1.
    let m = (month - 1).rem_euclid(12) + 1;
    // 4. Return the Record { [[Year]]: year, [[Month]]: month  }.
    (y.clamp(i32::MIN as i64, i32::MAX as i64) as i32, m as u8)
}

#[inline]
fn balance_iso_year_month(year: i32, month: i32) -> (i32, u8) {
    // 1. Assert: year and month are integers.
    // 2. Set year to year + floor((month - 1) / 12).
    let y = year + (month - 1).div_euclid(12);
    // 3. Set month to ((month - 1) modulo 12) + 1.
    let m = (month - 1).rem_euclid(12) + 1;
    // 4. Return the Record { [[Year]]: year, [[Month]]: month  }.
    (y, m as u8)
}

/// Note: month is 1 based.
#[inline]
pub(crate) fn constrain_iso_day(year: i32, month: u8, day: u8) -> u8 {
    let days_in_month = utils::iso_days_in_month(year, month);
    day.clamp(1, days_in_month)
}

#[inline]
pub(crate) fn is_valid_iso_day(year: i32, month: u8, day: u8) -> bool {
    let days_in_month = utils::iso_days_in_month(year, month);
    (1..=days_in_month).contains(&day)
}

// ==== `IsoTime` specific utilities ====

#[inline]
pub(crate) fn is_valid_time(hour: u8, minute: u8, second: u8, ms: u16, mis: u16, ns: u16) -> bool {
    if !(0..=23).contains(&hour) {
        return false;
    }

    let min_sec = 0..=59;
    if !min_sec.contains(&minute) || !min_sec.contains(&second) {
        return false;
    }

    let sub_second = 0..=999;
    sub_second.contains(&ms) && sub_second.contains(&mis) && sub_second.contains(&ns)
}

// NOTE(nekevss): Considering the below: Balance can probably be altered from f64.
#[inline]
fn div_mod(dividend: i64, divisor: i64) -> (i64, i64) {
    (dividend.div_euclid(divisor), dividend.rem_euclid(divisor))
}

impl From<timezone_provider::provider::IsoDateTime> for IsoDateTime {
    fn from(other: timezone_provider::provider::IsoDateTime) -> Self {
        Self::new_unchecked(
            IsoDate::new_unchecked(other.year, other.month, other.day),
            IsoTime::new_unchecked(
                other.hour,
                other.minute,
                other.second,
                other.millisecond,
                other.microsecond,
                other.nanosecond,
            ),
        )
    }
}

impl From<IsoDateTime> for timezone_provider::provider::IsoDateTime {
    fn from(other: IsoDateTime) -> Self {
        Self {
            year: other.date.year,
            month: other.date.month,
            day: other.date.day,
            hour: other.time.hour,
            minute: other.time.minute,
            second: other.time.second,
            millisecond: other.time.millisecond,
            microsecond: other.time.microsecond,
            nanosecond: other.time.nanosecond,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{iso_date_to_epoch_days, IsoDate};

    const MAX_DAYS_BASE: i64 = 100_000_000;

    #[test]
    fn icu4x_max_conversion_test() {
        // Test that the max ISO date does not panic on conversion
        let _ = IsoDate::new_unchecked(275_760, 9, 13).to_icu4x();
        // Test that the min ISO date does not panic on conversion
        let _ = IsoDate::new_unchecked(-271_821, 4, 20).to_icu4x();
    }

    #[test]
    fn iso_date_to_epoch_days_limits() {
        // Succeeds
        assert_eq!(iso_date_to_epoch_days(-271_821, 4, 20).abs(), MAX_DAYS_BASE);
        // Succeeds
        assert_eq!(
            iso_date_to_epoch_days(-271_821, 4, 19).abs(),
            MAX_DAYS_BASE + 1
        );
        // Fails
        assert_eq!(
            iso_date_to_epoch_days(-271_821, 4, 18).abs(),
            MAX_DAYS_BASE + 2
        );
        // Succeeds
        assert_eq!(iso_date_to_epoch_days(275_760, 9, 13).abs(), MAX_DAYS_BASE);
        // Succeeds
        assert_eq!(
            iso_date_to_epoch_days(275_760, 9, 14).abs(),
            MAX_DAYS_BASE + 1
        );
        // Fails
        assert_eq!(
            iso_date_to_epoch_days(275_760, 9, 15).abs(),
            MAX_DAYS_BASE + 2
        );
    }

    #[test]
    fn test_month_limits() {
        assert_eq!(iso_date_to_epoch_days(1970, 1, 1), 0);
        assert_eq!(iso_date_to_epoch_days(1969, 12, 31), -1);
    }
}
