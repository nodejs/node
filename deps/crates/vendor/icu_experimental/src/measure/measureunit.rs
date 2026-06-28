// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::{provider::single_unit::SingleUnit, single_unit_vec::SingleUnitVec};
use alloc::string::String;
use core::fmt::Write;

/// The [`MeasureUnit`] struct represents a processed CLDR compound unit.
/// Examples include:
///  1. `meter-per-second`
///  2. `square-meter`
///  3. `liter-per-100-kilometer`
///  4. `portion-per-1e9`
///  5. `square-meter` (Note: a single unit is a special case of a compound unit containing only one single unit.)
#[derive(Debug, Eq, Clone)]
pub struct MeasureUnit {
    // TODO: remove this field once we are using the short units name in the datagen to locate the units.
    /// The CLDR ID of the unit.
    pub id: Option<&'static str>,

    /// Contains the processed units.
    pub(crate) single_units: SingleUnitVec,

    /// Represents the constant denominator of this measure unit.
    ///
    /// Examples:
    ///   - For the unit `meter-per-second`, the constant denominator is `0`, because there is no denominator.
    ///   - For the unit `liter-per-100-kilometer`, the constant denominator is `100`.
    ///   - For the unit `portion-per-1e9`, the constant denominator is `1_000_000_000`.
    ///
    /// NOTE:
    ///   If the constant denominator is not set, the value defaults to `0`.
    pub(crate) constant_denominator: u64,
}

impl PartialEq for MeasureUnit {
    fn eq(&self, other: &Self) -> bool {
        // self.id is not part of the equality because if the user used the parser it will be NONE for now.
        // self.id == other.id
        self.single_units == other.single_units
            && self.constant_denominator == other.constant_denominator
    }
}

impl MeasureUnit {
    /// Returns a slice of the single units contained within this measure unit.
    pub fn single_units(&self) -> &[SingleUnit] {
        self.single_units.as_slice()
    }

    /// Returns the constant denominator of this measure unit.
    ///
    /// NOTE:
    ///   If the constant denominator is not set, a value of `0` is returned.
    pub fn constant_denominator(&self) -> u64 {
        self.constant_denominator
    }

    /// Returns a short representation of this measure unit as follows:
    /// 1. Each single unit will be represented by its short representation.
    /// 2. The constant denominator, at the beginning of the short representation, will be represented by its value prefixed with `C`.
    ///    2.1 If the constant denominator is greater than or equal to 1000 and has more than 3 trailing zeros, it will be represented in scientific notation.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_experimental::measure::measureunit::MeasureUnit;
    ///
    /// let measure_unit = MeasureUnit::try_from_str("meter").unwrap();
    /// let short_representation = measure_unit.generate_short_representation();
    /// assert_eq!(short_representation, "I85", "{}", "meter");
    ///
    /// let measure_unit = MeasureUnit::try_from_str("square-meter").unwrap();
    /// let short_representation = measure_unit.generate_short_representation();
    /// assert_eq!(short_representation, "P2I85", "{}", "square-meter");
    ///
    /// let measure_unit =
    ///     MeasureUnit::try_from_str("liter-per-100-kilometer").unwrap();
    /// let short_representation = measure_unit.generate_short_representation();
    /// assert_eq!(
    ///     short_representation, "C100I82P-1D3I85",
    ///     "{}",
    ///     "liter-per-100-kilometer"
    /// );
    /// ```
    pub fn generate_short_representation(&self) -> String {
        // Decomposes a number into its significant digits and counts the trailing zeros.
        fn decompose_number_and_trailing_zeros(mut n: u64) -> (u64, u8) {
            let mut zeros_count = 0;

            for (divisor, zeros) in [(100_000_000, 8), (10_000, 4), (100, 2), (10, 1)] {
                while n % divisor == 0 {
                    n /= divisor;
                    zeros_count += zeros;
                }
            }

            (n, zeros_count)
        }

        // Convert the constant to scientific notation if it is a power of 10 with more than 3 trailing zeros
        fn append_power_of_10_to_scientific(input: u64, buff: &mut String) {
            if input < 1000 {
                let _infallible = write!(buff, "{input}");
                return;
            }

            let (significant_digits, zeros_count) = decompose_number_and_trailing_zeros(input);

            if zeros_count > 3 {
                let _infallible = write!(buff, "{significant_digits}E{zeros_count}");
                return;
            }

            let _infallible = write!(buff, "{input}");
        }

        let mut short_representation = String::new();

        if self.constant_denominator > 0 {
            short_representation.push('C');
            append_power_of_10_to_scientific(self.constant_denominator, &mut short_representation);
        }

        self.single_units.as_slice().iter().for_each(|single_unit| {
            single_unit.append_short_representation(&mut short_representation)
        });

        short_representation
    }
}

#[cfg(test)]
mod tests {
    use crate::measure::measureunit::MeasureUnit;

    #[test]
    fn test_generate_short_representation() {
        let test_cases = vec![
            ("meter", "I85"),
            ("foot", "I50"),
            ("inch", "I66"),
            ("square-meter", "P2I85"),
            ("square-millimeter", "P2D-3I85"),
            ("micrometer", "D-6I85"),
            ("meter-per-second", "I85P-1I127"),
            ("liter-per-100-kilometer", "C100I82P-1D3I85"),
            ("portion-per-1e9", "C1E9I113"),
            ("per-10000000000-portion", "C1E10P-1I113"),
            ("liter-per-240000000000-kilometer", "C24E10I82P-1D3I85"),
            ("millimeter-per-square-microsecond", "D-3I85P-2D-6I127"),
        ];

        for (full_unit, expected_short) in test_cases {
            let measure_unit = MeasureUnit::try_from_str(full_unit).unwrap();
            let short_representation = measure_unit.generate_short_representation();
            assert_eq!(short_representation, expected_short, "{full_unit}");
        }
    }
}
