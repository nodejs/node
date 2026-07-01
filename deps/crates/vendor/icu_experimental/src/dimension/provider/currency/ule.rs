// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use zerovec::{
    maps::ZeroMapKV,
    ule::{AsULE, UleError, ULE},
};

use crate::dimension::provider::currency::essentials::{
    CurrencyPatternConfig, PatternSelection, PlaceholderValue,
};

const NO_PLACEHOLDER: u16 = 0b0111_1111_1111; // decimal: 2047
const USE_ISO_CODE: u16 = 0b0111_1111_1110; // decimal: 2046

// TODO(#4013): Remove this constant once we have an invariant that the injecting text index is always less than 2046.
pub const MAX_PLACEHOLDER_INDEX: u16 = 0b0111_1111_1101; // decimal: 2045

/// [`CurrencyPatternConfigULE`] is a type optimized for efficient storing and
/// deserialization of [`CurrencyPatternConfig`] using the `ZeroVec` model.
///
/// The serialization model packages the pattern item in three bytes.
///
/// The first bit (b7) is used to determine the `short_pattern_selection`. If the bit is `0`, then, the value will be `Standard`.
/// If the bit is `1`, then, the value will be `StandardAlphaNextToNumber`.
///
/// The second bit (b6) is used to determine the `narrow_pattern_selection`. If the bit is `0`, then, the value will be `Standard`.
/// If the bit is `1`, then, the value will be `StandardAlphaNextToNumber`.
///
/// The next three bits (b5, b4 & b3) with the second byte is used to determine the `short_placeholder_value`.
/// The next three bits (b2, b1 & b0) with the third byte is used to determine the `narrow_placeholder_value`.
#[derive(Copy, Clone, Debug, PartialEq)]
#[repr(transparent)]
pub struct CurrencyPatternConfigULE([u8; 3]);

// Safety (based on the safety checklist on the ULE trait):
//  1. CurrencyPatternConfigULE does not include any uninitialized or padding bytes.
//     (achieved by `#[repr(transparent)]` on a ULE type)
//  2. CurrencyPatternConfigULE is aligned to 1 byte.
//     (achieved by `#[repr(transparent)]` on a ULE type)
//  3. The impl of validate_bytes() returns an error if any byte is not valid.
//  4. The impl of validate_bytes() returns an error if there are extra bytes.
//  5. The other ULE methods use the default impl.
//  6. CurrencyPatternConfigULE byte equality is semantic equality.
unsafe impl ULE for CurrencyPatternConfigULE {
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        if bytes.len() % 3 != 0 {
            return Err(UleError::length::<Self>(bytes.len()));
        }

        Ok(())
    }
}

const PATTERN_SHORT_SHIFT: u8 = 7;
const PATTERN_NARROW_SHIFT: u8 = 6;
const INDEX_SHORT_SHIFT: u8 = 3;
const INDEX_NARROW_SHIFT: u8 = 0;

impl AsULE for CurrencyPatternConfig {
    type ULE = CurrencyPatternConfigULE;

    #[inline]
    fn to_unaligned(self) -> Self::ULE {
        let mut first_byte_ule: u8 = 0;

        if self.short_pattern_selection == PatternSelection::StandardAlphaNextToNumber {
            first_byte_ule |= 0b1 << PATTERN_SHORT_SHIFT;
        }
        if self.narrow_pattern_selection == PatternSelection::StandardAlphaNextToNumber {
            first_byte_ule |= 0b1 << PATTERN_NARROW_SHIFT;
        }

        // For short_placeholder_value
        let [short_most_significant_byte, short_least_significant_byte_ule] =
            match self.short_placeholder_value {
                Some(PlaceholderValue::Index(index)) => index.to_be_bytes(),
                Some(PlaceholderValue::ISO) => USE_ISO_CODE.to_be_bytes(),
                None => NO_PLACEHOLDER.to_be_bytes(),
            };
        if short_most_significant_byte & 0b1111_1000 != 0 {
            panic!(
                "short_placeholder_value is too large {short_most_significant_byte}, {short_least_significant_byte_ule}"
            )
        }
        first_byte_ule |= short_most_significant_byte << INDEX_SHORT_SHIFT;

        // For narrow_placeholder_value
        let [narrow_most_significant_byte, narrow_least_significant_byte_ule] =
            match self.narrow_placeholder_value {
                Some(PlaceholderValue::Index(index)) => index.to_be_bytes(),
                Some(PlaceholderValue::ISO) => USE_ISO_CODE.to_be_bytes(),
                None => NO_PLACEHOLDER.to_be_bytes(),
            };
        if narrow_most_significant_byte & 0b1111_1000 != 0 {
            panic!(
                "narrow_placeholder_value is too large {narrow_most_significant_byte}, {narrow_least_significant_byte_ule}"
            )
        }
        first_byte_ule |= narrow_most_significant_byte << INDEX_NARROW_SHIFT;

        CurrencyPatternConfigULE([
            first_byte_ule,
            short_least_significant_byte_ule,
            narrow_least_significant_byte_ule,
        ])
    }

    #[inline]
    fn from_unaligned(unaligned: Self::ULE) -> Self {
        let [first_byte, second_byte, third_byte] = unaligned.0;

        let short_pattern_selection =
            if first_byte & (0b1 << PATTERN_SHORT_SHIFT) == 0b1 << PATTERN_SHORT_SHIFT {
                PatternSelection::StandardAlphaNextToNumber
            } else {
                PatternSelection::Standard
            };

        let narrow_pattern_selection =
            if first_byte & (0b1 << PATTERN_NARROW_SHIFT) == 0b1 << PATTERN_NARROW_SHIFT {
                PatternSelection::StandardAlphaNextToNumber
            } else {
                PatternSelection::Standard
            };
        let short_prefix = (first_byte & (0b111 << INDEX_SHORT_SHIFT)) >> INDEX_SHORT_SHIFT;
        let narrow_prefix = (first_byte & (0b111 << INDEX_NARROW_SHIFT)) >> INDEX_NARROW_SHIFT;

        let short_placeholder_value = ((short_prefix as u16) << 8) | second_byte as u16;
        let narrow_placeholder_value = ((narrow_prefix as u16) << 8) | third_byte as u16;

        let short_placeholder_value = match short_placeholder_value {
            NO_PLACEHOLDER => None,
            USE_ISO_CODE => Some(PlaceholderValue::ISO),
            index => {
                debug_assert!(index <= MAX_PLACEHOLDER_INDEX);
                Some(PlaceholderValue::Index(index))
            }
        };

        let narrow_placeholder_value = match narrow_placeholder_value {
            NO_PLACEHOLDER => None,
            USE_ISO_CODE => Some(PlaceholderValue::ISO),
            index => {
                debug_assert!(index <= MAX_PLACEHOLDER_INDEX);
                Some(PlaceholderValue::Index(index))
            }
        };

        CurrencyPatternConfig {
            short_pattern_selection,
            narrow_pattern_selection,
            short_placeholder_value,
            narrow_placeholder_value,
        }
    }
}

impl<'a> ZeroMapKV<'a> for CurrencyPatternConfig {
    type Container = zerovec::ZeroVec<'a, CurrencyPatternConfig>;
    type Slice = zerovec::ZeroSlice<CurrencyPatternConfig>;
    type GetType = <CurrencyPatternConfig as AsULE>::ULE;
    type OwnedType = CurrencyPatternConfig;
}
