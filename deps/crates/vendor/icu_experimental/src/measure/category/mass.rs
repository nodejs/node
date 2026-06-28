// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#![cfg(feature = "compiled_data")]
use crate::measure::{
    category::{CategorizedMeasureUnit, Mass},
    measureunit::MeasureUnit,
    provider::{
        si_prefix::{Base, SiPrefix},
        single_unit::SingleUnit,
    },
    single_unit_vec::SingleUnitVec,
};

impl Mass {
    /// Returns a [`MeasureUnit`] representing mass in grams.
    pub fn gram() -> CategorizedMeasureUnit<Mass> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("gram"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 1,
                    si_prefix: SiPrefix {
                        power: 0,
                        base: Base::Decimal,
                    },
                    unit_id: *crate::provider::Baked::UNIT_IDS_V1_UND_GRAM,
                }),
                constant_denominator: 0,
            },
        }
    }

    /// Returns a [`MeasureUnit`] representing mass in kilograms.
    pub fn kilogram() -> CategorizedMeasureUnit<Mass> {
        CategorizedMeasureUnit {
            _category: core::marker::PhantomData,
            unit: MeasureUnit {
                id: Some("kilogram"),
                single_units: SingleUnitVec::One(SingleUnit {
                    power: 1,
                    si_prefix: SiPrefix {
                        power: 0,
                        base: Base::Decimal,
                    },
                    unit_id: *crate::provider::Baked::UNIT_IDS_V1_UND_KILOGRAM,
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
    fn test_mass_category() {
        let gram = Mass::gram();
        let gram_parsed = MeasureUnit::try_from_str("gram").unwrap();
        assert_eq!(gram.unit, gram_parsed);

        let kilogram = Mass::kilogram();
        let kilogram_parsed = MeasureUnit::try_from_str("kilogram").unwrap();
        assert_eq!(kilogram.unit, kilogram_parsed);
    }
}
