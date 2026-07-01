// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use zerovec::{
    maps::ZeroMapKV,
    ule::{AsULE, UleError, ULE},
};

use crate::dimension::provider::units::essentials::CompoundCount;

#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq, Debug)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
#[repr(u8)]
pub enum PowerValue {
    Two,
    Three,
}

#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq, Debug)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
pub enum PatternKey {
    Binary(u8),
    Decimal(i8),
    Power {
        power: PowerValue,
        count: CompoundCount,
    },
}

/// [`PatternKeyULE`] is a type optimized for efficient storage and
/// deserialization of [`PatternKey`] using the `ZeroVec` model.
///
/// The serialization model packages the pattern item in a single byte.
///
/// The first two bits (b7 & b6) determine the variant of the pattern key:
/// - `00`: `Binary`
/// - `01`: `Decimal`
/// - `10`: `Power`
/// - `11`: Forbidden
///
/// The next 6 bits (b5 to b0) determine the value of the pattern key:
/// - For `Binary`, the value is mapped directly to the pattern value.
/// - For `Decimal`:
///     - b5 is determining the sign of the value. if b5 is 0, the value is positive. if b5 is 1, the value is negative.
///     - b4 to b0 are determining the magnitude of the value.
/// - For `Power`:
///     - b5 and b4 represent the power value, which can be `10` to represent `Two` and `11` to represent `Three`.
///     - b3 to b0 represent the count value, which can be:
///         - `0000`: Zero
///         - `0001`: One
///         - `0010`: Two
///         - `0011`: Few
///         - `0100`: Many
///         - `0101`: Other
///     - Note: In the `Power` case, b3 is always 0, and when b2 is 1, b1 must be 0.
#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq, Debug)]
pub struct PatternKeyULE(u8);

// Safety (based on the safety checklist on the ULE trait):
//  1. PatternKeyULE does not include any uninitialized or padding bytes.
//     (achieved by `#[repr(transparent)]` on a ULE type)
//  2. PatternKeyULE is aligned to 1 byte.
//     (achieved by `#[repr(transparent)]` on a ULE type)
//  3. The impl of validate_bytes() returns an error if any byte is not valid.
//  4. The impl of validate_bytes() returns an error if there are extra bytes.
//  5. The other ULE methods use the default impl.
//  6. PatternKeyULE byte equality is semantic equality.
unsafe impl ULE for PatternKeyULE {
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        for &byte in bytes.iter() {
            // Ensure the first two bits (b7 & b6) are not 11.
            if (byte & 0b1100_0000) == 0b1100_0000 {
                return Err(UleError::parse::<Self>());
            }

            // For the `Power` variant:
            //      b5 & b4 must be 10 or 11. (this means that b5 must be 1)
            //      b3 must be 0.
            //      When b2 is 1, b1 must be 0.
            if (byte & 0b1100_0000) == 0b1000_0000 {
                // b5 must be 1
                if (byte & 0b0010_0000) == 0 {
                    return Err(UleError::parse::<Self>());
                }

                // b3 must be 0
                if (byte & 0b0000_1000) != 0 {
                    return Err(UleError::parse::<Self>());
                }

                // If b2 is 1, b1 must be 0
                if (byte & 0b0000_0100) != 0 && (byte & 0b0000_0010) != 0 {
                    return Err(UleError::parse::<Self>());
                }
            }
        }

        Ok(())
    }
}

impl AsULE for PatternKey {
    type ULE = PatternKeyULE;

    fn to_unaligned(self) -> Self::ULE {
        let byte = match self {
            PatternKey::Binary(value) => value,
            PatternKey::Decimal(value) => {
                let sign = if value < 0 { 0b0010_0000 } else { 0 };
                debug_assert!(value > -32 && value < 32);
                (0b01 << 6) | sign | (value.unsigned_abs() & 0b0001_1111)
            }
            PatternKey::Power { power, count } => {
                let power_bits = {
                    match power {
                        PowerValue::Two => 0b10 << 4,
                        PowerValue::Three => 0b11 << 4,
                    }
                };
                // Combine the bits to form the final byte
                (0b10 << 6) | power_bits | count as u8
            }
        };

        PatternKeyULE(byte)
    }

    fn from_unaligned(unaligned: Self::ULE) -> Self {
        let byte = unaligned.0;

        let variant = (byte & 0b1100_0000) >> 6;
        let value = byte & 0b0011_1111;

        match variant {
            0b00 => PatternKey::Binary(value),
            0b01 => match value & 0b0010_0000 {
                0b0000_0000 => PatternKey::Decimal(value as i8),
                0b0010_0000 => PatternKey::Decimal(-((value & 0b0001_1111) as i8)),
                _ => unreachable!(),
            },
            0b10 => {
                let power = match value & 0b0011_0000 {
                    0b0010_0000 => PowerValue::Two,
                    0b0011_0000 => PowerValue::Three,
                    _ => unreachable!(),
                };
                let count = value & 0b0000_1111;
                PatternKey::Power {
                    power,
                    count: count.into(),
                }
            }
            _ => unreachable!(),
        }
    }
}

impl<'a> ZeroMapKV<'a> for PatternKey {
    type Container = zerovec::ZeroVec<'a, PatternKey>;
    type Slice = zerovec::ZeroSlice<PatternKey>;
    type GetType = <PatternKey as AsULE>::ULE;
    type OwnedType = PatternKey;
}

#[test]
fn test_pattern_key_ule() {
    use PowerValue::{Three, Two};

    let binary = PatternKey::Binary(0b0000_1111);
    let binary_ule = binary.to_unaligned();
    PatternKeyULE::validate_bytes(&[binary_ule.0]).unwrap();
    assert_eq!(binary_ule.0, 0b0000_1111);

    let decimal = PatternKey::Decimal(0b0000_1111);
    let decimal_ule = decimal.to_unaligned();
    PatternKeyULE::validate_bytes(&[decimal_ule.0]).unwrap();
    assert_eq!(decimal_ule.0, 0b0100_1111);

    let power2 = PatternKey::Power {
        power: Two,
        count: CompoundCount::Two,
    };
    let power2_ule = power2.to_unaligned();
    PatternKeyULE::validate_bytes(&[power2_ule.0]).unwrap();
    assert_eq!(power2_ule.0, 0b1010_0010);

    let power3 = PatternKey::Power {
        power: Three,
        count: CompoundCount::Two,
    };
    let power3_ule = power3.to_unaligned();
    PatternKeyULE::validate_bytes(&[power3_ule.0]).unwrap();
    assert_eq!(power3_ule.0, 0b1011_0010);

    let binary = PatternKey::from_unaligned(binary_ule);
    assert_eq!(binary, PatternKey::Binary(0b0000_1111));

    let decimal = PatternKey::from_unaligned(decimal_ule);
    assert_eq!(decimal, PatternKey::Decimal(0b0000_1111));

    let power2 = PatternKey::from_unaligned(power2_ule);
    assert_eq!(
        power2,
        PatternKey::Power {
            power: Two,
            count: CompoundCount::Two,
        }
    );

    let power3 = PatternKey::from_unaligned(power3_ule);
    assert_eq!(
        power3,
        PatternKey::Power {
            power: Three,
            count: CompoundCount::Two,
        }
    );

    let decimal_neg_1 = PatternKey::Decimal(-1);
    let decimal_neg_1_ule = decimal_neg_1.to_unaligned();
    assert_eq!(decimal_neg_1_ule.0, 0b0110_0001);

    let decimal_neg_1 = PatternKey::from_unaligned(decimal_neg_1_ule);
    assert_eq!(decimal_neg_1, PatternKey::Decimal(-1));

    // Test invalid bytes
    let unvalidated_bytes = [0b1100_0000];
    assert_eq!(
        PatternKeyULE::validate_bytes(&unvalidated_bytes),
        Err(UleError::parse::<PatternKeyULE>())
    );

    let unvalidated_bytes = [0b1000_0000];
    assert_eq!(
        PatternKeyULE::validate_bytes(&unvalidated_bytes),
        Err(UleError::parse::<PatternKeyULE>())
    );
}
