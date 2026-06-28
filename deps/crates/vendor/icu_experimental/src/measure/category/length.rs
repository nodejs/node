// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![cfg(feature = "compiled_data")]
use crate::measure::{
    category::{CategorizedMeasureUnit, Length},
    measureunit::MeasureUnit,
    provider::{
        si_prefix::{Base, SiPrefix},
        single_unit::SingleUnit,
    },
    single_unit_vec::SingleUnitVec,
};

impl Length {
    /// Returns a [`MeasureUnit`] representing length in meters.
    pub fn meter() -> CategorizedMeasureUnit<Length> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("meter"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 1,
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
    fn test_length_category() {
        let meter = Length::meter();
        let meter_parsed = MeasureUnit::try_from_str("meter").unwrap();
        assert_eq!(meter.unit, meter_parsed);
    }
}
