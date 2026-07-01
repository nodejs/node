// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![cfg(feature = "compiled_data")]
use crate::measure::{
    category::{CategorizedMeasureUnit, Volume},
    measureunit::MeasureUnit,
    provider::{
        si_prefix::{Base, SiPrefix},
        single_unit::SingleUnit,
    },
    single_unit_vec::SingleUnitVec,
};

impl Volume {
    /// Returns a [`MeasureUnit`] representing volume in cubic meters.
    pub fn cubic_meter() -> CategorizedMeasureUnit<Volume> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("cubic-meter"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 3,
                    si_prefix: SiPrefix {
                        power: 0,
                        base: Base::Decimal,
                    },
                    unit_id: *crate::provider::Baked::UNIT_IDS_V1_UND_METER,
                }),
                constant_denominator: 0,
            },
        }
    }

    /// Returns a [`MeasureUnit`] representing volume in liters.
    pub fn liter() -> CategorizedMeasureUnit<Volume> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("liter"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 1,
                    si_prefix: SiPrefix {
                        power: 0,
                        base: Base::Decimal,
                    },
                    unit_id: *crate::provider::Baked::UNIT_IDS_V1_UND_LITER,
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
    fn test_volume_category() {
        let cubic_meter = Volume::cubic_meter();
        let cubic_meter_parsed = MeasureUnit::try_from_str("cubic-meter").unwrap();
        assert_eq!(cubic_meter.unit, cubic_meter_parsed);

        let liter = Volume::liter();
        let liter_parsed = MeasureUnit::try_from_str("liter").unwrap();
        assert_eq!(liter.unit, liter_parsed);
    }
}
