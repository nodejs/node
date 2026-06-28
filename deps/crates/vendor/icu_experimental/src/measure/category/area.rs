// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![cfg(feature = "compiled_data")]
use crate::measure::{
    category::{Area, CategorizedMeasureUnit},
    measureunit::MeasureUnit,
    provider::{
        si_prefix::{Base, SiPrefix},
        single_unit::SingleUnit,
    },
    single_unit_vec::SingleUnitVec,
};

impl Area {
    /// Returns a [`MeasureUnit`] representing area in square meters.
    pub fn square_meter() -> CategorizedMeasureUnit<Area> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("square-meter"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 2,
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
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::measure::measureunit::MeasureUnit;

    #[test]
    fn test_area_category() {
        let square_meter = Area::square_meter();
        let square_meter_parsed = MeasureUnit::try_from_str("square-meter").unwrap();
        assert_eq!(square_meter.unit, square_meter_parsed);
    }
}
