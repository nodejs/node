//! Native implementation of the `Temporal` options.
//!
//! Temporal has various instances where user's can define options for how an
//! operation may be completed.

use crate::parsers::Precision;
use crate::TemporalUnwrap;
use crate::{error::ErrorMessage, TemporalError, TemporalResult, MS_PER_DAY, NS_PER_DAY};
use core::num::NonZeroU128;
use core::ops::Add;
use core::{fmt, str::FromStr};

mod increment;
mod relative_to;

pub use increment::RoundingIncrement;
pub use relative_to::RelativeTo;

// ==== RoundingOptions / DifferenceSettings ====

#[derive(Debug, Clone, Copy)]
pub(crate) enum DifferenceOperation {
    Until,
    Since,
}

#[derive(Debug, Default)]
pub struct ToStringRoundingOptions {
    pub precision: Precision,
    pub smallest_unit: Option<Unit>,
    pub rounding_mode: Option<RoundingMode>,
}

#[derive(Debug)]
pub(crate) struct ResolvedToStringRoundingOptions {
    pub(crate) precision: Precision,
    pub(crate) smallest_unit: Unit,
    pub(crate) rounding_mode: RoundingMode,
    pub(crate) increment: RoundingIncrement,
}

impl ToStringRoundingOptions {
    pub(crate) fn resolve(&self) -> TemporalResult<ResolvedToStringRoundingOptions> {
        let rounding_mode = self.rounding_mode.unwrap_or(RoundingMode::Trunc);
        match self.smallest_unit {
            Some(Unit::Minute) => Ok(ResolvedToStringRoundingOptions {
                precision: Precision::Minute,
                smallest_unit: Unit::Minute,
                rounding_mode,
                increment: RoundingIncrement::ONE,
            }),
            Some(Unit::Second) => Ok(ResolvedToStringRoundingOptions {
                precision: Precision::Digit(0),
                smallest_unit: Unit::Second,
                rounding_mode,
                increment: RoundingIncrement::ONE,
            }),
            Some(Unit::Millisecond) => Ok(ResolvedToStringRoundingOptions {
                precision: Precision::Digit(3),
                smallest_unit: Unit::Millisecond,
                rounding_mode,
                increment: RoundingIncrement::ONE,
            }),
            Some(Unit::Microsecond) => Ok(ResolvedToStringRoundingOptions {
                precision: Precision::Digit(6),
                smallest_unit: Unit::Microsecond,
                rounding_mode,
                increment: RoundingIncrement::ONE,
            }),
            Some(Unit::Nanosecond) => Ok(ResolvedToStringRoundingOptions {
                precision: Precision::Digit(9),
                smallest_unit: Unit::Nanosecond,
                rounding_mode,
                increment: RoundingIncrement::ONE,
            }),
            None => match self.precision {
                Precision::Auto => Ok(ResolvedToStringRoundingOptions {
                    precision: Precision::Auto,
                    smallest_unit: Unit::Nanosecond,
                    rounding_mode,
                    increment: RoundingIncrement::ONE,
                }),
                Precision::Digit(0) => Ok(ResolvedToStringRoundingOptions {
                    precision: Precision::Digit(0),
                    smallest_unit: Unit::Second,
                    rounding_mode,
                    increment: RoundingIncrement::ONE,
                }),
                Precision::Digit(d) if (1..=3).contains(&d) => {
                    Ok(ResolvedToStringRoundingOptions {
                        precision: Precision::Digit(d),
                        smallest_unit: Unit::Millisecond,
                        rounding_mode,
                        increment: RoundingIncrement::try_new(10_u32.pow(3 - d as u32))
                            .temporal_unwrap()?,
                    })
                }
                Precision::Digit(d) if (4..=6).contains(&d) => {
                    Ok(ResolvedToStringRoundingOptions {
                        precision: Precision::Digit(d),
                        smallest_unit: Unit::Microsecond,
                        rounding_mode,
                        increment: RoundingIncrement::try_new(10_u32.pow(6 - d as u32))
                            .temporal_unwrap()?,
                    })
                }
                Precision::Digit(d) if (7..=9).contains(&d) => {
                    Ok(ResolvedToStringRoundingOptions {
                        precision: Precision::Digit(d),
                        smallest_unit: Unit::Nanosecond,
                        rounding_mode,
                        increment: RoundingIncrement::try_new(10_u32.pow(9 - d as u32))
                            .temporal_unwrap()?,
                    })
                }
                _ => Err(TemporalError::range()
                    .with_enum(ErrorMessage::FractionalDigitsPrecisionInvalid)),
            },
            _ => Err(TemporalError::range().with_enum(ErrorMessage::SmallestUnitNotTimeUnit)),
        }
    }
}

#[non_exhaustive]
#[derive(Debug, Default, Clone, Copy)]
pub struct DifferenceSettings {
    pub largest_unit: Option<Unit>,
    pub smallest_unit: Option<Unit>,
    pub rounding_mode: Option<RoundingMode>,
    pub increment: Option<RoundingIncrement>,
}

#[non_exhaustive]
#[derive(Debug, Clone, Copy)]
pub struct RoundingOptions {
    pub largest_unit: Option<Unit>,
    pub smallest_unit: Option<Unit>,
    pub rounding_mode: Option<RoundingMode>,
    pub increment: Option<RoundingIncrement>,
}

// Note: Specification does not clearly state a default, but
// having both largest and smallest unit None would auto throw.

impl Default for RoundingOptions {
    fn default() -> Self {
        Self {
            largest_unit: Some(Unit::Auto),
            smallest_unit: None,
            rounding_mode: None,
            increment: None,
        }
    }
}

/// Internal options object that represents the resolved rounding options.
#[derive(Debug, Clone, Copy)]
pub(crate) struct ResolvedRoundingOptions {
    pub(crate) largest_unit: Unit,
    pub(crate) smallest_unit: Unit,
    pub(crate) increment: RoundingIncrement,
    pub(crate) rounding_mode: RoundingMode,
}

impl ResolvedRoundingOptions {
    pub(crate) fn from_to_string_options(options: &ResolvedToStringRoundingOptions) -> Self {
        Self {
            largest_unit: Unit::Auto,
            smallest_unit: options.smallest_unit,
            increment: options.increment,
            rounding_mode: options.rounding_mode,
        }
    }

    pub(crate) fn from_diff_settings(
        options: DifferenceSettings,
        operation: DifferenceOperation,
        unit_group: UnitGroup,
        fallback_largest: Unit,
        fallback_smallest: Unit,
    ) -> TemporalResult<Self> {
        // 1. NOTE: The following steps read options and perform independent validation in alphabetical order.
        // 2. Let largestUnit be ? GetUnitValuedOption(options, "largestUnit", unitGroup, auto).
        unit_group.validate_unit(options.largest_unit, Some(Unit::Auto))?;

        // 4. Let resolvedOptions be ? SnapshotOwnProperties(? GetOptionsObject(options), null).
        // 5. Let settings be ? GetDifferenceSettings(operation, resolvedOptions, DATE, « », "day", "day").

        // 3. If disallowedUnits contains largestUnit, throw a RangeError exception.
        // 4. Let roundingIncrement be ? GetRoundingIncrementOption(options).
        let increment = options.increment.unwrap_or_default();
        // 5. Let roundingMode be ? GetRoundingModeOption(options, trunc).
        // 6. If operation is since, then
        // a. Set roundingMode to NegateRoundingMode(roundingMode).
        let rounding_mode = match operation {
            DifferenceOperation::Since => options
                .rounding_mode
                .unwrap_or(RoundingMode::Trunc)
                .negate(),
            DifferenceOperation::Until => options.rounding_mode.unwrap_or(RoundingMode::Trunc),
        };
        // 7. Let smallestUnit be ? GetUnitValuedOption(options, "smallestUnit", unitGroup, fallbackSmallestUnit).
        let smallest_unit = options.smallest_unit.unwrap_or(fallback_smallest);
        // 8. If disallowedUnits contains smallestUnit, throw a RangeError exception.
        unit_group.validate_unit(options.smallest_unit, None)?;
        // 9. Let defaultLargestUnit be LargerOfTwoUnits(smallestLargestDefaultUnit, smallestUnit).
        let default_largest_unit = smallest_unit.max(fallback_largest);
        // 10. If largestUnit is auto, set largestUnit to defaultLargestUnit.
        let mut largest_unit = options.largest_unit.unwrap_unit_or(default_largest_unit);
        if largest_unit == Unit::Auto {
            largest_unit = default_largest_unit;
        }
        // 11. If LargerOfTwoUnits(largestUnit, smallestUnit) is not largestUnit, throw a RangeError exception.
        if largest_unit < smallest_unit {
            return Err(
                TemporalError::range().with_enum(ErrorMessage::SmallestUnitLargerThanLargestUnit)
            );
        }

        // 12. Let maximum be MaximumTemporalDurationRoundingIncrement(smallestUnit).
        let maximum = smallest_unit.to_maximum_rounding_increment();
        // 13. If maximum is not unset, perform ? ValidateTemporalRoundingIncrement(roundingIncrement, maximum, false).
        if let Some(max) = maximum {
            increment.validate(max.into(), false)?;
        }
        // 14. Return the Record { [[SmallestUnit]]: smallestUnit, [[LargestUnit]]: largestUnit, [[RoundingMode]]:
        // roundingMode, [[RoundingIncrement]]: roundingIncrement,  }.
        Ok(ResolvedRoundingOptions {
            largest_unit,
            smallest_unit,
            increment,
            rounding_mode,
        })
    }

    // NOTE: Should the GetUnitValuedOption check be integrated into these validations.
    pub(crate) fn from_datetime_options(options: RoundingOptions) -> TemporalResult<Self> {
        let increment = options.increment.unwrap_or_default();
        let rounding_mode = options.rounding_mode.unwrap_or_default();
        let smallest_unit =
            UnitGroup::Time.validate_required_unit(options.smallest_unit, Some(Unit::Day))?;
        let (maximum, inclusive) = if smallest_unit == Unit::Day {
            (1, true)
        } else {
            let maximum = smallest_unit
                .to_maximum_rounding_increment()
                .ok_or(TemporalError::range().with_enum(ErrorMessage::SmallestUnitNotTimeUnit))?;
            (maximum, false)
        };

        increment.validate(maximum.into(), inclusive)?;

        Ok(Self {
            largest_unit: Unit::Auto,
            smallest_unit,
            increment,
            rounding_mode,
        })
    }

    pub(crate) fn from_time_options(options: RoundingOptions) -> TemporalResult<Self> {
        let Some(smallest_unit) = options.smallest_unit else {
            return Err(TemporalError::range().with_enum(ErrorMessage::SmallestUnitIsRequired));
        };
        let increment = options.increment.unwrap_or(RoundingIncrement::ONE);
        let rounding_mode = options.rounding_mode.unwrap_or(RoundingMode::HalfExpand);

        let max = smallest_unit
            .to_maximum_rounding_increment()
            .ok_or_else(|| {
                TemporalError::range().with_enum(ErrorMessage::SmallestUnitNotTimeUnit)
            })?;

        // Safety (nekevss): to_rounding_increment returns a value in the range of a u32.
        increment.validate(u64::from(max), false)?;

        Ok(ResolvedRoundingOptions {
            largest_unit: Unit::Auto,
            increment,
            smallest_unit,
            rounding_mode,
        })
    }

    pub(crate) fn from_instant_options(options: RoundingOptions) -> TemporalResult<Self> {
        let increment = options.increment.unwrap_or_default();
        let rounding_mode = options.rounding_mode.unwrap_or_default();
        let smallest_unit = UnitGroup::Time.validate_required_unit(options.smallest_unit, None)?;
        let maximum = match smallest_unit {
            Unit::Hour => 24u64,
            Unit::Minute => 24 * 60,
            Unit::Second => 24 * 3600,
            Unit::Millisecond => MS_PER_DAY as u64,
            Unit::Microsecond => MS_PER_DAY as u64 * 1000,
            Unit::Nanosecond => NS_PER_DAY,
            _ => return Err(TemporalError::range().with_enum(ErrorMessage::RoundToUnitInvalid)),
        };

        increment.validate(maximum, true)?;

        Ok(Self {
            largest_unit: Unit::Auto,
            smallest_unit,
            increment,
            rounding_mode,
        })
    }

    pub(crate) fn is_noop(&self) -> bool {
        self.smallest_unit == Unit::Nanosecond && self.increment == RoundingIncrement::ONE
    }
}

// ==== Options enums and methods ====

#[derive(Debug, Clone, Copy)]
pub enum UnitGroup {
    Date,
    Time,
    DateTime,
}

impl UnitGroup {
    pub fn validate_required_unit(
        self,
        unit: Option<Unit>,
        extra_unit: Option<Unit>,
    ) -> TemporalResult<Unit> {
        let Some(unit) = unit else {
            return Err(TemporalError::range().with_enum(ErrorMessage::UnitRequired));
        };
        self.validate_unit(Some(unit), extra_unit)?;
        Ok(unit)
    }

    /// Note: this always rejects Auto unless extra_unit is specified
    ///
    /// <https://tc39.es/proposal-temporal/#sec-temporal-validatetemporalunitvaluedoption>
    pub fn validate_unit(self, unit: Option<Unit>, extra_unit: Option<Unit>) -> TemporalResult<()> {
        match self {
            _ if unit == extra_unit => Ok(()),
            UnitGroup::Date => match unit {
                Some(unit) if unit.is_date_unit() => Ok(()),
                None => Ok(()),
                _ => Err(TemporalError::range().with_enum(ErrorMessage::UnitNotDate)),
            },
            UnitGroup::Time => match unit {
                Some(unit) if unit.is_time_unit() => Ok(()),
                None => Ok(()),
                _ => Err(TemporalError::range().with_enum(ErrorMessage::UnitNotTime)),
            },
            UnitGroup::DateTime if unit != Some(Unit::Auto) => Ok(()),
            _ => Err(TemporalError::range().with_enum(ErrorMessage::UnitNoAutoDuringComparison)),
        }
    }
}

/// `Table 21: Temporal units by descending magnitude`
///
/// Subset of the spec table containing only the value column.
///
/// Spec: <https://tc39.es/proposal-temporal/#table-temporal-units>
//
// Spec last accessed: 2025-05-16, <https://github.com/tc39/proposal-temporal/tree/c150e7135c56afc9114032e93b53ac49f980d254>
pub(crate) const UNIT_VALUE_TABLE: [Unit; 10] = [
    Unit::Year,
    Unit::Month,
    Unit::Week,
    Unit::Day,
    Unit::Hour,
    Unit::Minute,
    Unit::Second,
    Unit::Millisecond,
    Unit::Microsecond,
    Unit::Nanosecond,
];

// TODO: Need to decide whether to make auto default or remove. Blocker was one
// of Duration::round / Duration::total
/// The relevant unit that should be used for the operation that
/// this option is provided as a value.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Unit {
    /// The `Auto` unit
    Auto = 0,
    /// The `Nanosecond` unit
    Nanosecond,
    /// The `Microsecond` unit
    Microsecond,
    /// The `Millisecond` unit
    Millisecond,
    /// The `Second` unit
    Second,
    /// The `Minute` unit
    Minute,
    /// The `Hour` unit
    Hour,
    /// The `Day` unit
    Day,
    /// The `Week` unit
    Week,
    /// The `Month` unit
    Month,
    /// The `Year` unit
    Year,
}

impl Unit {
    #[inline]
    #[must_use]
    /// Returns the `MaximumRoundingIncrement` for the current `Unit`.
    pub fn to_maximum_rounding_increment(self) -> Option<u32> {
        use Unit::{
            Auto, Day, Hour, Microsecond, Millisecond, Minute, Month, Nanosecond, Second, Week,
            Year,
        };
        // 1. If unit is "year", "month", "week", or "day", then
        // a. Return undefined.
        // 2. If unit is "hour", then
        // a. Return 24.
        // 3. If unit is "minute" or "second", then
        // a. Return 60.
        // 4. Assert: unit is one of "millisecond", "microsecond", or "nanosecond".
        // 5. Return 1000.
        let max = match self {
            Year | Month | Week | Day => return None,
            Hour => 24,
            Minute | Second => 60,
            Millisecond | Microsecond | Nanosecond => 1000,
            Auto => {
                debug_assert!(false, "Auto units should be resolved by this point");
                return None;
            }
        };

        Some(max)
    }

    // TODO: potentiall use a u64
    /// Returns the `Nanosecond amount for any given value.`
    #[must_use]
    pub const fn as_nanoseconds(&self) -> Option<NonZeroU128> {
        use Unit::{
            Auto, Day, Hour, Microsecond, Millisecond, Minute, Month, Nanosecond, Second, Week,
            Year,
        };
        match self {
            Year | Month | Week | Auto => None,
            Day => NonZeroU128::new(NS_PER_DAY as u128),
            Hour => NonZeroU128::new(3_600_000_000_000),
            Minute => NonZeroU128::new(60_000_000_000),
            Second => NonZeroU128::new(1_000_000_000),
            Millisecond => NonZeroU128::new(1_000_000),
            Microsecond => NonZeroU128::new(1_000),
            Nanosecond => NonZeroU128::new(1),
        }
    }

    #[inline]
    #[must_use]
    pub fn is_calendar_unit(&self) -> bool {
        use Unit::{Month, Week, Year};
        matches!(self, Year | Month | Week)
    }

    #[inline]
    #[must_use]
    pub fn is_date_unit(&self) -> bool {
        use Unit::{Day, Month, Week, Year};
        matches!(self, Day | Year | Month | Week)
    }

    #[inline]
    #[must_use]
    pub fn is_time_unit(&self) -> bool {
        use Unit::{Hour, Microsecond, Millisecond, Minute, Nanosecond, Second};
        matches!(
            self,
            Hour | Minute | Second | Millisecond | Microsecond | Nanosecond
        )
    }

    /// `13.19 LargerOfTwoTemporalUnits ( u1, u2 )`
    ///
    /// Spec: <https://tc39.es/proposal-temporal/#sec-temporal-largeroftwotemporalunits>
    //
    // Spec last accessed: 2025-05-16, <https://github.com/tc39/proposal-temporal/tree/c150e7135c56afc9114032e93b53ac49f980d254>
    #[inline]
    pub fn larger(u1: Unit, u2: Unit) -> TemporalResult<Unit> {
        // 1. For each row of Table 21, except the header row, in table order, do
        //     a. Let unit be the value in the "Value" column of the row.
        for unit in UNIT_VALUE_TABLE {
            // b. If u1 is unit, return unit.
            if u1 == unit {
                return Ok(unit);
            }
            // c. If u2 is unit, return unit.
            if u2 == unit {
                return Ok(unit);
            }
        }

        // NOTE(HalidOdat): deviation from specification.
        Err(TemporalError::assert().with_enum(ErrorMessage::UnitNoAutoDuringComparison))
    }

    /// Helper method for getting the index into the [`UNIT_VALUE_TABLE`].
    ///
    /// # Error
    ///
    /// If the given [`Unit`] is [`Unit::Auto`].
    pub(crate) fn table_index(&self) -> TemporalResult<usize> {
        // Taken from: <https://tc39.es/proposal-temporal/#sec-temporal-bubblerelativeduration>
        //
        // spec(2025-05-28): https://github.com/tc39/proposal-temporal/tree/69001e954c70e29ba3d2e6433bc7ece2a037377a
        //
        // 2. Let largestUnitIndex be the ordinal index of the row of Table 21 whose "Value" column contains largestUnit.
        // 3. Let smallestUnitIndex be the ordinal index of the row of Table 21 whose "Value" column contains smallestUnit.
        for (i, unit) in UNIT_VALUE_TABLE.iter().enumerate() {
            if self == unit {
                return Ok(i);
            }
        }

        Err(TemporalError::assert().with_enum(ErrorMessage::UnitNoAutoDuringComparison))
    }
}

trait UnwrapUnit {
    type Result;
    fn unwrap_unit_or(self, unit: Unit) -> Self::Result;
}

impl UnwrapUnit for Option<Unit> {
    type Result = Unit;
    fn unwrap_unit_or(self, unit: Unit) -> Self::Result {
        if self == Some(Unit::Auto) {
            return unit;
        }
        self.unwrap_or(unit)
    }
}

impl From<usize> for Unit {
    fn from(value: usize) -> Self {
        match value {
            10 => Self::Year,
            9 => Self::Month,
            8 => Self::Week,
            7 => Self::Day,
            6 => Self::Hour,
            5 => Self::Minute,
            4 => Self::Second,
            3 => Self::Millisecond,
            2 => Self::Microsecond,
            1 => Self::Nanosecond,
            _ => Self::Auto,
        }
    }
}

impl Add<usize> for Unit {
    type Output = Unit;

    fn add(self, rhs: usize) -> Self::Output {
        Unit::from(self as usize + rhs)
    }
}

/// A parsing error for `Unit`
#[derive(Debug, Clone, Copy)]
pub struct ParseUnitError;

impl fmt::Display for ParseUnitError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str("provided string was not a valid Unit")
    }
}

impl FromStr for Unit {
    type Err = ParseUnitError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "auto" => Ok(Self::Auto),
            "year" | "years" => Ok(Self::Year),
            "month" | "months" => Ok(Self::Month),
            "week" | "weeks" => Ok(Self::Week),
            "day" | "days" => Ok(Self::Day),
            "hour" | "hours" => Ok(Self::Hour),
            "minute" | "minutes" => Ok(Self::Minute),
            "second" | "seconds" => Ok(Self::Second),
            "millisecond" | "milliseconds" => Ok(Self::Millisecond),
            "microsecond" | "microseconds" => Ok(Self::Microsecond),
            "nanosecond" | "nanoseconds" => Ok(Self::Nanosecond),
            _ => Err(ParseUnitError),
        }
    }
}

impl fmt::Display for Unit {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::Auto => "auto",
            Self::Year => "year",
            Self::Month => "month",
            Self::Week => "week",
            Self::Day => "day",
            Self::Hour => "hour",
            Self::Minute => "minute",
            Self::Second => "second",
            Self::Millisecond => "millsecond",
            Self::Microsecond => "microsecond",
            Self::Nanosecond => "nanosecond",
        }
        .fmt(f)
    }
}

/// `Overflow` can also be used as an
/// assignment overflow and consists of the "constrain"
/// and "reject" options.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub enum Overflow {
    /// Constrain option
    #[default]
    Constrain,
    /// Constrain option
    Reject,
}

/// A parsing error for `ArithemeticOverflow`
#[derive(Debug, Clone, Copy)]
pub struct ParseOverflowError;

impl fmt::Display for ParseOverflowError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str("provided string was not a valid overflow value")
    }
}

impl FromStr for Overflow {
    type Err = ParseOverflowError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "constrain" => Ok(Self::Constrain),
            "reject" => Ok(Self::Reject),
            _ => Err(ParseOverflowError),
        }
    }
}

impl fmt::Display for Overflow {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::Constrain => "constrain",
            Self::Reject => "reject",
        }
        .fmt(f)
    }
}

/// The disambiguation options for an instant.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Disambiguation {
    /// Compatible option
    ///
    /// This is the default according to GetTemporalDisambiguationOption
    #[default]
    Compatible,
    /// Earlier option
    Earlier,
    /// Later option
    Later,
    /// Reject option
    Reject,
}

/// A parsing error on `InstantDisambiguation` options.
#[derive(Debug, Clone, Copy)]
pub struct ParseDisambiguationError;

impl fmt::Display for ParseDisambiguationError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str("provided string was not a valid instant disambiguation value")
    }
}

impl FromStr for Disambiguation {
    type Err = ParseDisambiguationError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "compatible" => Ok(Self::Compatible),
            "earlier" => Ok(Self::Earlier),
            "later" => Ok(Self::Later),
            "reject" => Ok(Self::Reject),
            _ => Err(ParseDisambiguationError),
        }
    }
}

impl fmt::Display for Disambiguation {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::Compatible => "compatible",
            Self::Earlier => "earlier",
            Self::Later => "later",
            Self::Reject => "reject",
        }
        .fmt(f)
    }
}

/// Offset disambiguation options.
#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
pub enum OffsetDisambiguation {
    /// Use option
    Use,
    /// Prefer option
    Prefer,
    /// Ignore option
    Ignore,
    /// Reject option
    Reject,
}

/// A parsing error for `OffsetDisambiguation` parsing.
#[derive(Debug, Clone, Copy)]
pub struct ParseOffsetDisambiguationError;

impl fmt::Display for ParseOffsetDisambiguationError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str("provided string was not a valid offset disambiguation value")
    }
}

impl FromStr for OffsetDisambiguation {
    type Err = ParseOffsetDisambiguationError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "use" => Ok(Self::Use),
            "prefer" => Ok(Self::Prefer),
            "ignore" => Ok(Self::Ignore),
            "reject" => Ok(Self::Reject),
            _ => Err(ParseOffsetDisambiguationError),
        }
    }
}

impl fmt::Display for OffsetDisambiguation {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::Use => "use",
            Self::Prefer => "prefer",
            Self::Ignore => "ignore",
            Self::Reject => "reject",
        }
        .fmt(f)
    }
}

// TODO: Figure out what to do with intl's RoundingMode

/// Declares the specified `RoundingMode` for the operation.
#[derive(Debug, Copy, Clone, Default)]
pub enum RoundingMode {
    /// Ceil RoundingMode
    Ceil,
    /// Floor RoundingMode
    Floor,
    /// Expand RoundingMode
    Expand,
    /// Truncate RoundingMode
    Trunc,
    /// HalfCeil RoundingMode
    HalfCeil,
    /// HalfFloor RoundingMode
    HalfFloor,
    /// HalfExpand RoundingMode - Default
    #[default]
    HalfExpand,
    /// HalfTruncate RoundingMode
    HalfTrunc,
    /// HalfEven RoundingMode
    HalfEven,
}

/// The `UnsignedRoundingMode`
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UnsignedRoundingMode {
    /// `Infinity` `RoundingMode`
    Infinity,
    /// `Zero` `RoundingMode`
    Zero,
    /// `HalfInfinity` `RoundingMode`
    HalfInfinity,
    /// `HalfZero` `RoundingMode`
    HalfZero,
    /// `HalfEven` `RoundingMode`
    HalfEven,
}

impl RoundingMode {
    #[inline]
    #[must_use]
    /// Negates the current `RoundingMode`.
    pub const fn negate(self) -> Self {
        use RoundingMode::{
            Ceil, Expand, Floor, HalfCeil, HalfEven, HalfExpand, HalfFloor, HalfTrunc, Trunc,
        };

        match self {
            Ceil => Self::Floor,
            Floor => Self::Ceil,
            HalfCeil => Self::HalfFloor,
            HalfFloor => Self::HalfCeil,
            Trunc => Self::Trunc,
            Expand => Self::Expand,
            HalfTrunc => Self::HalfTrunc,
            HalfExpand => Self::HalfExpand,
            HalfEven => Self::HalfEven,
        }
    }

    #[inline]
    #[must_use]
    /// Returns the `UnsignedRoundingMode`
    pub const fn get_unsigned_round_mode(self, is_positive: bool) -> UnsignedRoundingMode {
        use RoundingMode::{
            Ceil, Expand, Floor, HalfCeil, HalfEven, HalfExpand, HalfFloor, HalfTrunc, Trunc,
        };

        match self {
            Ceil if is_positive => UnsignedRoundingMode::Infinity,
            Ceil | Trunc => UnsignedRoundingMode::Zero,
            Floor if is_positive => UnsignedRoundingMode::Zero,
            Floor | Expand => UnsignedRoundingMode::Infinity,
            HalfCeil if is_positive => UnsignedRoundingMode::HalfInfinity,
            HalfCeil | HalfTrunc => UnsignedRoundingMode::HalfZero,
            HalfFloor if is_positive => UnsignedRoundingMode::HalfZero,
            HalfFloor | HalfExpand => UnsignedRoundingMode::HalfInfinity,
            HalfEven => UnsignedRoundingMode::HalfEven,
        }
    }
}

impl FromStr for RoundingMode {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "ceil" => Ok(Self::Ceil),
            "floor" => Ok(Self::Floor),
            "expand" => Ok(Self::Expand),
            "trunc" => Ok(Self::Trunc),
            "halfCeil" => Ok(Self::HalfCeil),
            "halfFloor" => Ok(Self::HalfFloor),
            "halfExpand" => Ok(Self::HalfExpand),
            "halfTrunc" => Ok(Self::HalfTrunc),
            "halfEven" => Ok(Self::HalfEven),
            _ => Err(TemporalError::range().with_enum(ErrorMessage::RoundingModeInvalid)),
        }
    }
}

impl fmt::Display for RoundingMode {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::Ceil => "ceil",
            Self::Floor => "floor",
            Self::Expand => "expand",
            Self::Trunc => "trunc",
            Self::HalfCeil => "halfCeil",
            Self::HalfFloor => "halfFloor",
            Self::HalfExpand => "halfExpand",
            Self::HalfTrunc => "halfTrunc",
            Self::HalfEven => "halfEven",
        }
        .fmt(f)
    }
}

/// values for `CalendarName`, whether to show the calendar in toString() methods
/// <https://tc39.es/proposal-temporal/#sec-temporal-gettemporalshowcalendarnameoption>
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum DisplayCalendar {
    #[default]
    /// `Auto` option
    Auto,
    /// `Always` option
    Always,
    /// `Never` option
    Never,
    // `Critical` option
    Critical,
}

impl fmt::Display for DisplayCalendar {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            DisplayCalendar::Auto => "auto",
            DisplayCalendar::Always => "always",
            DisplayCalendar::Never => "never",
            DisplayCalendar::Critical => "critical",
        }
        .fmt(f)
    }
}

impl FromStr for DisplayCalendar {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "auto" => Ok(Self::Auto),
            "always" => Ok(Self::Always),
            "never" => Ok(Self::Never),
            "critical" => Ok(Self::Critical),
            _ => Err(TemporalError::range().with_enum(ErrorMessage::CalendarNameInvalid)),
        }
    }
}

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum DisplayOffset {
    #[default]
    Auto,
    Never,
}

impl fmt::Display for DisplayOffset {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            DisplayOffset::Auto => "auto",
            DisplayOffset::Never => "never",
        }
        .fmt(f)
    }
}

impl FromStr for DisplayOffset {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "auto" => Ok(Self::Auto),
            "never" => Ok(Self::Never),
            _ => Err(TemporalError::range().with_enum(ErrorMessage::OffsetOptionInvalid)),
        }
    }
}

#[derive(Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum DisplayTimeZone {
    #[default]
    /// `Auto` option
    Auto,
    /// `Never` option
    Never,
    // `Critical` option
    Critical,
}

impl fmt::Display for DisplayTimeZone {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            DisplayTimeZone::Auto => "auto",
            DisplayTimeZone::Never => "never",
            DisplayTimeZone::Critical => "critical",
        }
        .fmt(f)
    }
}

impl FromStr for DisplayTimeZone {
    type Err = TemporalError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s {
            "auto" => Ok(Self::Auto),
            "never" => Ok(Self::Never),
            "critical" => Ok(Self::Critical),
            _ => Err(TemporalError::range().with_enum(ErrorMessage::TimeZoneNameInvalid)),
        }
    }
}
