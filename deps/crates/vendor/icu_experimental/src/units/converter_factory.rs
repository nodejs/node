// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::measure::measureunit::MeasureUnit;
use crate::measure::provider::single_unit::SingleUnit;
use crate::units::ratio::IcuRatio;
use crate::units::{
    converter::{
        OffsetConverter, ProportionalConverter, ReciprocalConverter, UnitsConverter,
        UnitsConverterInner,
    },
    provider::Sign,
};
use crate::units::{provider, InvalidConversionError};

use icu_provider::prelude::*;
use icu_provider::DataError;
use litemap::LiteMap;
use num_traits::Pow;
use num_traits::{One, Zero};
use zerovec::ZeroSlice;

use super::convertible::Convertible;
/// `ConverterFactory` is responsible for creating converters.
#[derive(Debug)]
pub struct ConverterFactory {
    /// Contains the necessary data for the conversion factory.
    payload: DataPayload<provider::UnitsInfoV1>,
}

impl From<Sign> for num_bigint::Sign {
    fn from(val: Sign) -> Self {
        match val {
            Sign::Positive => num_bigint::Sign::Plus,
            Sign::Negative => num_bigint::Sign::Minus,
        }
    }
}

#[cfg(feature = "compiled_data")]
impl Default for ConverterFactory {
    fn default() -> Self {
        Self::new()
    }
}

impl ConverterFactory {
    icu_provider::gen_buffer_data_constructors!(
        () -> error: DataError,
        functions: [
            new: skip,
            try_new_with_buffer_provider,
            try_new_unstable,
            Self,
        ]
    );

    /// Creates a new [`ConverterFactory`] from compiled data.
    ///
    /// ✨ *Enabled with the `compiled_data` Cargo feature.*
    ///
    /// [📚 Help choosing a constructor](icu_provider::constructors)
    #[cfg(feature = "compiled_data")]
    pub const fn new() -> Self {
        Self {
            payload: DataPayload::from_static_ref(crate::provider::Baked::SINGLETON_UNITS_INFO_V1),
        }
    }

    #[doc = icu_provider::gen_buffer_unstable_docs!(UNSTABLE, Self::new)]
    pub fn try_new_unstable<D>(provider: &D) -> Result<Self, DataError>
    where
        D: ?Sized + DataProvider<provider::UnitsInfoV1>,
    {
        let payload = provider.load(DataRequest::default())?.payload;

        Ok(Self { payload })
    }

    /// Calculates the offset between two units by performing the following steps:
    /// 1. Identify the conversion rate from the first unit to the base unit as `ConversionRate1`: N1/D1 with an Offset1: OffsetN1/OffsetD1.
    /// 2. Identify the conversion rate from the second unit to the base unit as `ConversionRate2`: N2/D2 with an Offset2: OffsetN2/OffsetD2.
    /// 3. The conversion from the base unit to the second unit is represented by `ConversionRateBaseToUnit2`: (D2/N2) and an `OffsetBaseToUnit2`: - (OffsetN2/OffsetD2) * (D2/N2).
    /// 4. To convert a value V from the first unit to the second unit, first convert V to the base unit using `ConversionRate1`:
    ///    (V * N1/D1) + OffsetN1/OffsetD1, referred to as `V_TO_Base`.
    /// 5. Then, convert `V_TO_Base` to the second unit using the formula: CR: (D2/N2) and Offset: - (OffsetN2/OffsetD2) * (D2/N2).
    ///    The result is: (`V_TO_Base` * (D2/N2)) - (OffsetN2/OffsetD2) * (D2/N2).
    /// 6. By inserting `V_TO_Base` from step 4 into step 5, the equation becomes:
    ///    (((V * N1/D1) + OffsetN1/OffsetD1) * D2/N2) - (OffsetN2/OffsetD2) * (D2/N2).
    /// 7. Simplifying the equation gives:
    ///    (V * (N1/D1) * (D2/N2)) + (OffsetN1/OffsetD1 * (D2/N2)) - (OffsetN2/OffsetD2) * (D2/N2).
    /// 8. Focusing on the constants to find the offset formula, we get:
    ///    Offset = ((OffsetN1/OffsetD1) - (OffsetN2/OffsetD2)) * (D2/N2),
    ///    which simplifies to: Offset = (Offset1 - Offset2) * (1/ConversionRate2).
    ///
    /// NOTE:
    ///   - An offset can be calculated if both the input and output units are simple.
    ///   - A unit is considered simple if it is made up of a single unit, with a power of 1 and an SI prefix power of 0.
    ///     - For example:
    ///         - `meter` and `foot` are simple units.
    ///         - `square-meter` and `square-foot` are simple units.
    ///         - `meter-per-second` and `foot-per-second` are not simple units.
    fn compute_offset(
        &self,
        input_unit: &MeasureUnit,
        output_unit: &MeasureUnit,
    ) -> Option<IcuRatio> {
        let &[input_unit] = input_unit.single_units() else {
            return Some(IcuRatio::zero());
        };

        let &[output_unit] = output_unit.single_units() else {
            return Some(IcuRatio::zero());
        };

        if !(input_unit.power == 1
            && output_unit.power == 1
            && input_unit.si_prefix.power == 0
            && output_unit.si_prefix.power == 0)
        {
            return Some(IcuRatio::zero());
        }

        let input_conversion_info = self
            .payload
            .get()
            .conversion_info_by_unit_id(input_unit.unit_id);
        debug_assert!(
            input_conversion_info.is_some(),
            "Failed to get input conversion info"
        );
        let input_conversion_info = input_conversion_info?;

        let output_conversion_info = self
            .payload
            .get()
            .conversion_info_by_unit_id(output_unit.unit_id);
        debug_assert!(
            output_conversion_info.is_some(),
            "Failed to get output conversion info"
        );
        let output_conversion_info = output_conversion_info?;

        let input_offset = input_conversion_info.offset_as_ratio();
        let output_offset = output_conversion_info.offset_as_ratio();

        if input_offset.is_zero() && output_offset.is_zero() {
            return Some(IcuRatio::zero());
        }

        let output_conversion_rate_recip = output_conversion_info.factor_as_ratio().recip();
        Some((input_offset - output_offset) * output_conversion_rate_recip)
    }

    /// Checks whether the given units are reciprocal.
    /// If they are not reciprocal, it implies that the units are proportional.
    /// NOTE:
    ///   If the units are neither proportional nor reciprocal, the function will return `None`,
    ///   indicating that the units are incompatible.
    fn is_reciprocal(
        &self,
        unit1: &MeasureUnit,
        unit2: &MeasureUnit,
    ) -> Result<bool, InvalidConversionError> {
        /// A struct that contains the sum and difference of base unit powers.
        /// For example:
        ///     For the input unit `meter-per-second`, the base units are `meter` (power: 1) and `second` (power: -1).
        ///     For the output unit `foot-per-second`, the base units are `meter` (power: 1) and `second` (power: -1).
        ///     The differences are: meter: 1 - 1 = 0, second: -1 - (-1) = 0.
        ///     The sums are: meter: 1 + 1 = 2, second: -1 + (-1) = -2.
        ///     If all the sums are zeros, then the units are reciprocal.
        ///     If all the diffs are zeros, then the units are convertible.
        ///     If none of the above, then the units are not convertible.
        #[derive(Debug)]
        struct PowersInfo {
            diffs: i16,
            sums: i16,
        }

        /// Inserts composite units into the map by decomposing them into their basic units.
        /// NOTE:
        ///     This process involves iterating through the basic units of the provided composite units.
        ///     For example, `newton` is composed of the basic units: `gram`, `meter`, and `second` (each with its own power and SI prefix).
        fn insert_composite_units(
            factory: &ConverterFactory,
            single_units: &[SingleUnit],
            sign: i16,
            map: &mut LiteMap<u16, PowersInfo>,
        ) -> Result<(), InvalidConversionError> {
            for single_unit in single_units {
                let conversion_info = factory
                    .payload
                    .get()
                    .conversion_info_by_unit_id(single_unit.unit_id);

                debug_assert!(conversion_info.is_some(), "Failed to get convert info");

                match conversion_info {
                    Some(items) => {
                        insert_base_units(items.basic_units(), single_unit.power as i16, sign, map)
                    }
                    None => return Err(InvalidConversionError),
                }
            }

            Ok(())
        }

        /// Inserting the basic units into the map.
        /// NOTE:   
        ///     The base units should be multiplied by the original power.
        ///     For example, `square-foot` , the base unit is `meter` with power 1.
        ///     Thus, the inserted power should be `1 * 2 = 2`.
        fn insert_base_units(
            basic_units: &ZeroSlice<SingleUnit>,
            original_power: i16,
            sign: i16,
            map: &mut LiteMap<u16, PowersInfo>,
        ) {
            for item in basic_units.iter() {
                let item_power = (item.power as i16) * original_power;
                let signed_item_power = item_power * sign;
                if let Some(powers) = map.get_mut(&item.unit_id) {
                    powers.diffs += signed_item_power;
                    powers.sums += item_power;
                } else {
                    map.insert(
                        item.unit_id,
                        PowersInfo {
                            diffs: (signed_item_power),
                            sums: (item_power),
                        },
                    );
                }
            }
        }

        let mut map = LiteMap::new();
        for (single_units, sign) in [(&unit1.single_units, 1), (&unit2.single_units, -1)] {
            insert_composite_units(self, single_units.as_slice(), sign, &mut map)?;
        }

        let (power_sums_are_zero, power_diffs_are_zero) =
            map.values()
                .fold((true, true), |(sums, diffs), powers_info| {
                    (
                        sums && powers_info.sums == 0,
                        diffs && powers_info.diffs == 0,
                    )
                });

        if power_diffs_are_zero {
            Ok(false)
        } else if power_sums_are_zero {
            Ok(true)
        } else {
            Err(InvalidConversionError)
        }
    }

    fn compute_conversion_term(&self, unit_item: SingleUnit, sign: i8) -> Option<IcuRatio> {
        let conversion_info = self
            .payload
            .get()
            .conversion_info_by_unit_id(unit_item.unit_id);
        debug_assert!(conversion_info.is_some(), "Failed to get conversion info");
        let conversion_info = conversion_info?;

        let mut conversion_info_factor = conversion_info.factor_as_ratio();
        conversion_info_factor *= &unit_item.si_prefix;
        conversion_info_factor = conversion_info_factor.pow((unit_item.power * sign) as i32);
        Some(conversion_info_factor)
    }

    /// Creates a converter for converting between two single or compound units.
    /// For example:
    ///    1 - `meter` to `foot` --> Simple
    ///    2 - `kilometer-per-hour` to `mile-per-hour` --> Compound
    ///    3 - `mile-per-gallon` to `liter-per-100-kilometer` --> Reciprocal
    ///    4 - `celsius` to `fahrenheit` --> Needs an offset
    ///
    /// NOTE:
    ///    This converter does not support conversions between mixed units,
    ///    such as, from "meter" to "foot-and-inch".
    pub fn converter<T: Convertible>(
        &self,
        input_unit: &MeasureUnit,
        output_unit: &MeasureUnit,
    ) -> Option<UnitsConverter<T>> {
        let is_reciprocal = match self.is_reciprocal(input_unit, output_unit) {
            Ok(is_reciprocal) => is_reciprocal,
            Err(InvalidConversionError) => return None,
        };

        // Determine the sign of the powers of the units from root to unit2.
        let root_to_unit2_direction_sign = if is_reciprocal { 1 } else { -1 };

        let mut conversion_rate = IcuRatio::one();
        for &input_single_unit in input_unit.single_units.as_slice() {
            conversion_rate *= Self::compute_conversion_term(self, input_single_unit, 1)?;
        }

        if input_unit.constant_denominator() != 0 {
            conversion_rate /= IcuRatio::from_integer(input_unit.constant_denominator());
        }

        for &output_single_unit in output_unit.single_units.as_slice() {
            conversion_rate *= Self::compute_conversion_term(
                self,
                output_single_unit,
                root_to_unit2_direction_sign,
            )?;
        }

        if output_unit.constant_denominator() != 0 {
            if is_reciprocal {
                conversion_rate /= IcuRatio::from_integer(output_unit.constant_denominator());
            } else {
                conversion_rate *= IcuRatio::from_integer(output_unit.constant_denominator());
            }
        }

        let offset = self.compute_offset(input_unit, output_unit)?;

        if is_reciprocal && !offset.is_zero() {
            debug_assert!(
                false,
                "The units are reciprocal, but the offset is not zero. This is should not happen!.",
            );
            return None;
        }

        let conversion_rate = T::from_ratio_bigint(conversion_rate.get_ratio())?;
        let proportional = ProportionalConverter { conversion_rate };

        if is_reciprocal {
            Some(UnitsConverter(UnitsConverterInner::Reciprocal(
                ReciprocalConverter { proportional },
            )))
        } else if offset.is_zero() {
            Some(UnitsConverter(UnitsConverterInner::Proportional(
                proportional,
            )))
        } else {
            let offset = T::from_ratio_bigint(offset.get_ratio())?;
            Some(UnitsConverter(UnitsConverterInner::Offset(
                OffsetConverter {
                    proportional,
                    offset,
                },
            )))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::ConverterFactory;
    use crate::measure::measureunit::MeasureUnit;

    #[test]
    fn test_converter_factory() {
        let factory = ConverterFactory::new();
        let input_unit = MeasureUnit::try_from_str("meter").unwrap();
        let output_unit = MeasureUnit::try_from_str("foot").unwrap();
        let converter = factory.converter::<f64>(&input_unit, &output_unit).unwrap();
        let result = converter.convert(&1000.0);
        assert!(
            ((result - 3280.84) / 3280.84).abs() < 0.00001,
            "The relative difference between the result and the expected value is too large: {}",
            ((result - 3280.84) / 3280.84).abs()
        );
    }

    #[test]
    fn test_converter_factory_with_constant_denominator() {
        let factory = ConverterFactory::new();
        let input_unit = MeasureUnit::try_from_str("liter-per-100-kilometer").unwrap();
        let output_unit = MeasureUnit::try_from_str("mile-per-gallon").unwrap();
        let converter = factory.converter::<f64>(&input_unit, &output_unit).unwrap();
        let result = converter.convert(&1.0);
        assert!(
            ((result - 235.21) / 235.21).abs() < 0.0001,
            "The relative difference between the result and the expected value is too large: {}",
            ((result - 235.21) / 235.21).abs()
        );
    }

    #[test]
    fn test_converter_factory_with_offset() {
        let factory = ConverterFactory::new();
        let input_unit = MeasureUnit::try_from_str("celsius").unwrap();
        let output_unit = MeasureUnit::try_from_str("fahrenheit").unwrap();
        let converter = factory.converter::<f64>(&input_unit, &output_unit).unwrap();
        let result = converter.convert(&0.0);
        assert!(
            ((result - 32.0) / 32.0).abs() < 0.00001,
            "The relative difference between the result and the expected value is too large: {}",
            ((result - 32.0) / 32.0).abs()
        );
    }
}
