// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![cfg(feature = "compiled_data")]
use crate::measure::{
    category::{CategorizedMeasureUnit, Duration},
    measureunit::MeasureUnit,
    provider::{
        si_prefix::{Base, SiPrefix},
        single_unit::SingleUnit,
    },
    single_unit_vec::SingleUnitVec,
};

impl Duration {
    /// Returns a [`MeasureUnit`] representing duration in milliseconds.
    pub fn millisecond() -> CategorizedMeasureUnit<Duration> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("millisecond"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 1,
                    si_prefix: SiPrefix {
                        power: -3,
                        base: Base::Decimal,
                    },
                    unit_id: *crate::provider::Baked::UNIT_IDS_V1_UND_SECOND,
                }),
                constant_denominator: 0,
            },
        }
    }

    /// Returns a [`MeasureUnit`] representing duration in seconds.
    pub fn second() -> CategorizedMeasureUnit<Duration> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("second"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 1,
                    si_prefix: SiPrefix {
                        power: 0,
                        base: Base::Decimal,
                    },
                    unit_id: *crate::provider::Baked::UNIT_IDS_V1_UND_SECOND,
                }),
                constant_denominator: 0,
            },
        }
    }

    /// Returns a [`MeasureUnit`] representing duration in minutes.
    pub fn minute() -> CategorizedMeasureUnit<Duration> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("minute"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 1,
                    si_prefix: SiPrefix {
                        power: 0,
                        base: Base::Decimal,
                    },
                    unit_id: *crate::provider::Baked::UNIT_IDS_V1_UND_MINUTE,
                }),
                constant_denominator: 0,
            },
        }
    }

    /// Returns a [`MeasureUnit`] representing duration in hours.
    pub fn hour() -> CategorizedMeasureUnit<Duration> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("hour"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 1,
                    si_prefix: SiPrefix {
                        power: 0,
                        base: Base::Decimal,
                    },
                    unit_id: *crate::provider::Baked::UNIT_IDS_V1_UND_HOUR,
                }),
                constant_denominator: 0,
            },
        }
    }

    /// Returns a [`MeasureUnit`] representing duration in days.
    pub fn day() -> CategorizedMeasureUnit<Duration> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("day"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 1,
                    si_prefix: SiPrefix {
                        power: 0,
                        base: Base::Decimal,
                    },
                    unit_id: *crate::provider::Baked::UNIT_IDS_V1_UND_DAY,
                }),
                constant_denominator: 0,
            },
        }
    }

    /// Returns a [`MeasureUnit`] representing duration in weeks.
    pub fn week() -> CategorizedMeasureUnit<Duration> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("week"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 1,
                    si_prefix: SiPrefix {
                        power: 0,
                        base: Base::Decimal,
                    },
                    unit_id: *crate::provider::Baked::UNIT_IDS_V1_UND_WEEK,
                }),
                constant_denominator: 0,
            },
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::measure::measureunit::MeasureUnit;

    #[test]
    fn test_duration_category() {
        let millisecond = Duration::millisecond();
        let millisecond_parsed = MeasureUnit::try_from_str("millisecond").unwrap();
        assert_eq!(millisecond.unit, millisecond_parsed);

        let second = Duration::second();
        let second_parsed = MeasureUnit::try_from_str("second").unwrap();
        assert_eq!(second.unit, second_parsed);

        let minute = Duration::minute();
        let minute_parsed = MeasureUnit::try_from_str("minute").unwrap();
        assert_eq!(minute.unit, minute_parsed);

        let hour = Duration::hour();
        let hour_parsed = MeasureUnit::try_from_str("hour").unwrap();
        assert_eq!(hour.unit, hour_parsed);

        let day = Duration::day();
        let day_parsed = MeasureUnit::try_from_str("day").unwrap();
        assert_eq!(day.unit, day_parsed);

        let week = Duration::week();
        let week_parsed = MeasureUnit::try_from_str("week").unwrap();
        assert_eq!(week.unit, week_parsed);
    }
}
