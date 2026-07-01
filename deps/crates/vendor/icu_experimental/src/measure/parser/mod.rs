// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

pub mod ids;
pub(crate) mod power;
pub(crate) mod si_prefix;

use crate::measure::measureunit::MeasureUnit;
use displaydoc::Display;
use ids::CLDR_IDS_TRIE;
use power::get_power;
use si_prefix::get_si_prefix;

use super::provider::si_prefix::{Base, SiPrefix};
use super::provider::single_unit::SingleUnit;
use super::single_unit_vec::SingleUnitVec;

#[derive(Display, Debug, Copy, Clone, PartialEq)]
#[displaydoc("The unit is not valid")]
/// The unit is not valid.
/// This can occur if the unit ID does not adhere to the CLDR specification.
/// For example, `meter` is a valid unit ID, but `metre` is not.
#[non_exhaustive]
pub struct InvalidUnitError;

impl MeasureUnit {
    /// Parses a CLDR unit identifier and returns a [`MeasureUnit`].
    /// Examples include: `meter`, `foot`, `meter-per-second`, `meter-per-square-second`, `meter-per-square-second-per-second`, etc.
    /// Returns:
    ///    - `Ok(MeasureUnit)` if the identifier is valid.
    ///    - `Err(InvalidUnitError)` if the identifier is invalid.
    #[inline]
    pub fn try_from_str(s: &str) -> Result<MeasureUnit, InvalidUnitError> {
        Self::try_from_utf8(s.as_bytes())
    }

    /// See [`Self::try_from_str`]
    pub fn try_from_utf8(mut code_units: &[u8]) -> Result<MeasureUnit, InvalidUnitError> {
        if code_units.starts_with(b"-") || code_units.ends_with(b"-") {
            return Err(InvalidUnitError);
        }

        let mut constant_denominator = 0;
        let mut single_units = SingleUnitVec::Empty;
        let mut sign = 1;
        while !code_units.is_empty() {
            // First: extract the power.
            let (power, identifier_part_without_power) = Self::power(code_units)?;

            // Second: extract the si_prefix and the unit_id.
            let (si_prefix, unit_id, identifier_part_without_unit_id) =
                match Self::unit_id(identifier_part_without_power) {
                    Ok((unit_id, identifier_part_without_unit_id)) => (
                        SiPrefix {
                            power: 0,
                            base: Base::Decimal,
                        },
                        unit_id,
                        identifier_part_without_unit_id,
                    ),
                    Err(_) => {
                        let (si_prefix, identifier_part_without_si_prefix) =
                            Self::si_prefix(identifier_part_without_power);
                        let (unit_id, identifier_part_without_unit_id) =
                            match Self::unit_id(identifier_part_without_si_prefix) {
                                Ok((unit_id, identifier_part_without_unit_id)) => {
                                    (unit_id, identifier_part_without_unit_id)
                                }
                                // If the sign is negative, this means that the identifier may contain more than one `per-` keyword.
                                Err(_) if sign == 1 => {
                                    if let Some(remain) = code_units.strip_prefix(b"per-") {
                                        // First time locating `per-` keyword.
                                        sign = -1;
                                        code_units = remain;

                                        // Extract the constant denominator if present.
                                        let mut split = remain.splitn(2, |c| *c == b'-');
                                        if let Some(possible_constant_denominator) = split.next() {
                                            // Try to parse the possible constant denominator as a u64.
                                            if let Some(parsed_denominator) =
                                                core::str::from_utf8(possible_constant_denominator)
                                                    .ok()
                                                    .and_then(|s| s.parse::<f64>().ok())
                                                    .and_then(|num| {
                                                        if num > u64::MAX as f64 {
                                                            None
                                                        } else {
                                                            Some(num as u64)
                                                        }
                                                    })
                                            {
                                                constant_denominator = parsed_denominator;
                                                code_units = split.next().unwrap_or(&[]);
                                            }
                                        }

                                        continue;
                                    }

                                    return Err(InvalidUnitError);
                                }
                                Err(e) => return Err(e),
                            };
                        (si_prefix, unit_id, identifier_part_without_unit_id)
                    }
                };

            single_units.push(SingleUnit {
                power: sign * power as i8,
                si_prefix,
                unit_id,
            });

            code_units = match identifier_part_without_unit_id.strip_prefix(b"-") {
                Some(remainder) => remainder,
                None if identifier_part_without_unit_id.is_empty() => {
                    identifier_part_without_unit_id
                }
                None => return Err(InvalidUnitError),
            };
        }

        // TODO: shall we allow units without any single units?
        // There is no unit without any valid single units.
        if single_units.as_slice().is_empty() {
            return Err(InvalidUnitError);
        }

        Ok(MeasureUnit {
            id: None,
            single_units,
            constant_denominator,
        })
    }

    /// Retrieves the unit identifier from the given byte slice.
    ///
    /// # Returns
    /// - `Ok((unit_id, remaining_part))`: If the unit id is successfully found, where `unit_id` is the identifier and `remaining_part` is the slice without the unit name and any leading `-` if present.
    /// - `Err(InvalidUnitError)`: If the unit id is not found in the provided slice.
    fn unit_id(part: &[u8]) -> Result<(u16, &[u8]), InvalidUnitError> {
        let mut cursor = CLDR_IDS_TRIE.cursor();
        let mut longest_match = Err(InvalidUnitError);

        for (i, byte) in part.iter().enumerate() {
            cursor.step(*byte);
            if cursor.is_empty() {
                break;
            }
            if let Some(value) = cursor.take_value() {
                longest_match = Ok((value as u16, &part[i + 1..]));
            }
        }
        longest_match
    }

    /// Retrieves the power from the given byte slice.
    ///
    /// # Returns
    /// - `Ok((power, remaining_part))`: If the power is successfully found, where `power` is the power and `remaining_part` is the slice without the power and any leading `-` if present.
    /// - `Err(InvalidUnitError)`: If the power is not found in the provided slice.
    fn power(part: &[u8]) -> Result<(u8, &[u8]), InvalidUnitError> {
        let (power, part_without_power) = get_power(part);

        // If the power is not found, return the part as it is.
        if part_without_power.len() == part.len() {
            return Ok((power, part));
        }

        // If the power is found, this means that the part must start with the `-` sign.
        match part_without_power.strip_prefix(b"-") {
            Some(part_without_power) => Ok((power, part_without_power)),
            None => Err(InvalidUnitError),
        }
    }

    /// Retrieves the SI prefix from the given byte slice.
    ///
    /// # Returns
    /// - `(SiPrefix, &[u8])`: If the prefix is successfully found, where `prefix` is the prefix and `remaining_part` is the slice without the prefix.
    /// - `(SiPrefix, &[u8])`: If the prefix is not found, the function will return `(SiPrefix { power: 0, base: Base::Decimal }, part)`.
    fn si_prefix(part: &[u8]) -> (SiPrefix, &[u8]) {
        let (si_prefix, part_without_si_prefix) = get_si_prefix(part);
        if part_without_si_prefix.len() == part.len() {
            return (si_prefix, part);
        }

        match part_without_si_prefix.strip_prefix(b"-") {
            Some(part_without_dash) => (si_prefix, part_without_dash),
            None => (si_prefix, part_without_si_prefix),
        }
    }
}

#[cfg(test)]
mod tests {
    use crate::measure::measureunit::MeasureUnit;

    #[test]
    fn test_parser_cases() {
        let test_cases = vec![
            ("meter-per-square-second", 2, 0),
            ("portion-per-1e9", 1, 1_000_000_000),
            ("portion-per-1000000000", 1, 1_000_000_000),
            ("liter-per-100-kilometer", 2, 100),
        ];

        for (input, expected_len, expected_denominator) in test_cases {
            let measure_unit = MeasureUnit::try_from_str(input).unwrap();
            assert_eq!(measure_unit.single_units().len(), expected_len);
            assert_eq!(measure_unit.constant_denominator, expected_denominator);
        }
    }

    #[test]
    fn test_invlalid_unit_ids() {
        let test_cases = vec![
            "kilo",
            "kilokilo",
            "onekilo",
            "meterkilo",
            "meter-kilo",
            "k",
            "meter-",
            "meter+",
            "-meter",
            "+meter",
            "-kilometer",
            "+kilometer",
            "-pow2-meter",
            "+pow2-meter",
            "p2-meter",
            "p4-meter",
            "+",
            "-",
            "-mile",
            "-and-mile",
            "-per-mile",
            "one",
            "one-one",
            "one-per-mile",
            "one-per-cubic-centimeter",
            "square--per-meter",
            "metersecond", // Must have a compound part between single units
            // Negative powers not supported in mixed units yet. TODO(CLDR-13701).
            "per-hour-and-hertz",
            "hertz-and-per-hour",
            // Compound units not supported in mixed units yet. TODO(CLDR-13701).
            "kilonewton-meter-and-newton-meter",
            // Invalid units due to invalid constant denominator
            "meter-per--20-second",
            "meter-per-1000-1e9-second",
            "meter-per-1e19-second",
            "per-1000",
            "meter-per-1000-1000",
            "meter-per-1000-second-1000-kilometer",
            "1000-meter",
            "meter-1000",
            "meter-per-1000-1000",
            "meter-per-1000-second-1000-kilometer",
            "per-1000-and-per-1000",
            "liter-per-kilometer-100",
        ];

        for input in test_cases {
            // TODO(Uicode-org/icu4x#6271):
            //      This is invalid, but because `100-kilometer` is a valid unit, it is not rejected.
            //      This should be fixed in CLDR.
            if input == "meter-per-100-100-kilometer" {
                continue;
            }

            let measure_unit = MeasureUnit::try_from_str(input);
            if measure_unit.is_ok() {
                println!("OK:  {input}");
                continue;
            }
            assert!(measure_unit.is_err());
        }
    }
}
