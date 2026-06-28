// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use fixed_decimal::{CompactDecimal, Decimal, UnsignedDecimal};

const LIMIT_18_DIGITS: u64 = 1_000_000_000_000_000_000;

/// A full plural operands representation of a number. See [CLDR Plural Rules](https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules) for complete operands description.
///
/// Plural operands in compliance with [CLDR Plural Rules](https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules).
///
/// See [full operands description](https://unicode.org/reports/tr35/tr35-numbers.html#Operands).
///
/// # Data Types
///
/// The following types can be converted to [`PluralOperands`]:
///
/// - Integers, signed and unsigned
/// - Strings representing an arbitrary-precision decimal
/// - [`Decimal`]
///
/// This crate does not support selection from a floating-point number, because floats are not
/// capable of carrying trailing zeros, which are required for proper plural rule selection. For
/// example, in English, "1 star" has a different plural form than "1.0 stars", but this
/// distinction cannot be represented using a float. Clients should use [`Decimal`] instead.
///
/// # Examples
///
/// From int
///
/// ```
/// use icu::plurals::PluralOperands;
/// use icu::plurals::RawPluralOperands;
///
/// assert_eq!(
///     PluralOperands::from(RawPluralOperands {
///         i: 2,
///         v: 0,
///         w: 0,
///         f: 0,
///         t: 0,
///         c: 0,
///     }),
///     PluralOperands::from(2_usize)
/// );
/// ```
///
/// From &str
///
/// ```
/// use icu::plurals::PluralOperands;
/// use icu::plurals::RawPluralOperands;
///
/// assert_eq!(
///     Ok(PluralOperands::from(RawPluralOperands {
///         i: 123,
///         v: 2,
///         w: 2,
///         f: 45,
///         t: 45,
///         c: 0,
///     })),
///     "123.45".parse()
/// );
/// ```
///
/// From [`Decimal`]
///
/// ```
/// use fixed_decimal::Decimal;
/// use icu::plurals::PluralOperands;
/// use icu::plurals::RawPluralOperands;
///
/// assert_eq!(
///     PluralOperands::from(RawPluralOperands {
///         i: 123,
///         v: 2,
///         w: 2,
///         f: 45,
///         t: 45,
///         c: 0,
///     }),
///     (&{
///         let mut decimal = Decimal::from(12345);
///         decimal.multiply_pow10(-2);
///         decimal
///     })
///         .into()
/// );
/// ```
#[derive(Debug, Clone, Copy, PartialEq, Default)]
#[allow(clippy::exhaustive_structs)] // mostly stable, new operands may be added at the cadence of ICU's release cycle
pub struct PluralOperands {
    /// Integer value of input
    pub(crate) i: u64,
    /// Number of visible fraction digits with trailing zeros
    pub(crate) v: usize,
    /// Number of visible fraction digits without trailing zeros
    pub(crate) w: usize,
    /// Visible fraction digits with trailing zeros
    pub(crate) f: u64,
    /// Visible fraction digits without trailing zeros
    pub(crate) t: u64,
    /// Exponent of the power of 10 used in compact decimal formatting
    pub(crate) c: usize,
}

#[derive(displaydoc::Display, Debug, PartialEq, Eq)]
#[non_exhaustive]
#[cfg(feature = "datagen")]
pub enum OperandsError {
    /// Input to the Operands parsing was empty.
    #[displaydoc("Input to the Operands parsing was empty")]
    Empty,
    /// Input to the Operands parsing was invalid.
    #[displaydoc("Input to the Operands parsing was invalid")]
    Invalid,
}

#[cfg(feature = "datagen")]
impl core::error::Error for OperandsError {}

#[cfg(feature = "datagen")]
impl From<core::num::ParseIntError> for OperandsError {
    fn from(_: core::num::ParseIntError) -> Self {
        Self::Invalid
    }
}

#[cfg(feature = "datagen")]
impl core::str::FromStr for PluralOperands {
    type Err = OperandsError;

    fn from_str(input: &str) -> Result<Self, Self::Err> {
        fn get_exponent(input: &str) -> Result<(&str, usize), OperandsError> {
            if let Some((base, exponent)) = input.split_once('e') {
                Ok((base, exponent.parse()?))
            } else {
                Ok((input, 0))
            }
        }

        if input.is_empty() {
            return Err(OperandsError::Empty);
        }

        let abs_str = input.strip_prefix('-').unwrap_or(input);

        let (
            integer_digits,
            num_fraction_digits0,
            num_fraction_digits,
            fraction_digits0,
            fraction_digits,
            exponent,
        ) = if let Some((int_str, rest)) = abs_str.split_once('.') {
            let (dec_str, exponent) = get_exponent(rest)?;

            let integer_digits = u64::from_str(int_str)?;

            let dec_str_no_zeros = dec_str.trim_end_matches('0');

            let num_fraction_digits0 = dec_str.len();
            let num_fraction_digits = dec_str_no_zeros.len();

            let fraction_digits0 = u64::from_str(dec_str)?;
            let fraction_digits =
                if num_fraction_digits == 0 || num_fraction_digits == num_fraction_digits0 {
                    fraction_digits0
                } else {
                    u64::from_str(dec_str_no_zeros)?
                };

            (
                integer_digits,
                num_fraction_digits0,
                num_fraction_digits,
                fraction_digits0,
                fraction_digits,
                exponent,
            )
        } else {
            let (abs_str, exponent) = get_exponent(abs_str)?;
            let integer_digits = u64::from_str(abs_str)?;
            (integer_digits, 0, 0, 0, 0, exponent)
        };

        Ok(Self {
            i: integer_digits,
            v: num_fraction_digits0,
            w: num_fraction_digits,
            f: fraction_digits0,
            t: fraction_digits,
            c: exponent,
        })
    }
}

macro_rules! impl_integer_type {
    ($ty:ident) => {
        impl From<$ty> for PluralOperands {
            #[inline]
            #[allow(trivial_numeric_casts)]
            fn from(input: $ty) -> Self {
                Self {
                    i: input as u64,
                    v: 0,
                    w: 0,
                    f: 0,
                    t: 0,
                    c: 0,
                }
            }
        }
    };
    ($($ty:ident)+) => {
        $(impl_integer_type!($ty);)+
    };
}

macro_rules! impl_signed_integer_type {
    ($ty:ident) => {
        impl From<$ty> for PluralOperands {
            #[inline]
            fn from(input: $ty) -> Self {
                input.unsigned_abs().into()
            }
        }
    };
    ($($ty:ident)+) => {
        $(impl_signed_integer_type!($ty);)+
    };
}

impl_integer_type!(u8 u16 u32 u64 usize);
impl_signed_integer_type!(i8 i16 i32 i64 i128 isize);

impl From<u128> for PluralOperands {
    #[inline]
    fn from(input: u128) -> Self {
        let i = if input < LIMIT_18_DIGITS as u128 {
            input as u64
        } else {
            // Add LIMIT_18_DIGITS so that is_exactly_one and is_exactly_zero
            // do not return true for large magnitude numbers like 1e18+1.
            (input % LIMIT_18_DIGITS as u128) as u64 + LIMIT_18_DIGITS
        };
        Self {
            i,
            v: 0,
            w: 0,
            f: 0,
            t: 0,
            c: 0,
        }
    }
}

impl PluralOperands {
    fn from_significand_and_exponent(dec: &UnsignedDecimal, exp: u8) -> PluralOperands {
        let exp_i16 = i16::from(exp);

        let mag_range = dec.magnitude_range();
        let mag_high = (*mag_range.end() + exp_i16).clamp(0, 17);
        let mag_low = (*mag_range.start() + exp_i16).clamp(-18, 0);

        let mut i: u64 = 0;
        for magnitude in (0..=mag_high).rev() {
            i *= 10;
            i += dec.digit_at(magnitude - exp_i16) as u64;
        }

        if *mag_range.end() + exp_i16 > 17 {
            i += LIMIT_18_DIGITS;
        }

        let mut f: u64 = 0;
        let mut t: u64 = 0;
        let mut w: usize = 0;
        for magnitude in (mag_low..=-1).rev() {
            let digit = dec.digit_at(magnitude - exp_i16) as u64;
            f *= 10;
            f += digit;
            if digit != 0 {
                t = f;
                w = (-magnitude) as usize;
            }
        }

        Self {
            i,
            v: (-mag_low) as usize,
            w,
            f,
            t,
            c: usize::from(exp),
        }
    }

    /// Whether these [`PluralOperands`] are exactly the number 0, which might be a special case.
    pub(crate) fn is_exactly_zero(self) -> bool {
        self == Self {
            i: 0,
            v: 0,
            w: 0,
            f: 0,
            t: 0,
            c: 0,
        }
    }

    /// Whether these [`PluralOperands`] are exactly the number 1, which might be a special case.
    pub(crate) fn is_exactly_one(self) -> bool {
        self == Self {
            i: 1,
            v: 0,
            w: 0,
            f: 0,
            t: 0,
            c: 0,
        }
    }
}

impl From<&Decimal> for PluralOperands {
    /// Converts a [`fixed_decimal::Decimal`] to [`PluralOperands`]. Retains at most 18
    /// digits each from the integer and fraction parts.
    fn from(dec: &Decimal) -> Self {
        (&dec.absolute).into()
    }
}

impl From<&UnsignedDecimal> for PluralOperands {
    fn from(value: &UnsignedDecimal) -> Self {
        Self::from_significand_and_exponent(value, 0)
    }
}

impl From<&CompactDecimal> for PluralOperands {
    /// Converts a [`fixed_decimal::CompactDecimal`] to [`PluralOperands`]. Retains at most 18
    /// digits each from the integer and fraction parts.
    ///
    /// # Examples
    ///
    /// ```
    /// use fixed_decimal::CompactDecimal;
    /// use fixed_decimal::Decimal;
    /// use icu::locale::locale;
    /// use icu::plurals::PluralCategory;
    /// use icu::plurals::PluralOperands;
    /// use icu::plurals::PluralRules;
    /// use icu::plurals::RawPluralOperands;
    ///
    /// let fixed_decimal = "1000000.20".parse::<Decimal>().unwrap();
    /// let compact_decimal = "1.00000020c6".parse::<CompactDecimal>().unwrap();
    ///
    /// assert_eq!(
    ///     PluralOperands::from(RawPluralOperands {
    ///         i: 1000000,
    ///         v: 2,
    ///         w: 1,
    ///         f: 20,
    ///         t: 2,
    ///         c: 0,
    ///     }),
    ///     PluralOperands::from(&fixed_decimal)
    /// );
    ///
    /// assert_eq!(
    ///     PluralOperands::from(RawPluralOperands {
    ///         i: 1000000,
    ///         v: 2,
    ///         w: 1,
    ///         f: 20,
    ///         t: 2,
    ///         c: 6,
    ///     }),
    ///     PluralOperands::from(&compact_decimal)
    /// );
    ///
    /// let rules = PluralRules::try_new_cardinal(locale!("fr").into()).unwrap();
    /// assert_eq!(rules.category_for(&fixed_decimal), PluralCategory::Other);
    /// assert_eq!(rules.category_for(&compact_decimal), PluralCategory::Many);
    /// ```
    fn from(compact: &CompactDecimal) -> Self {
        Self::from_significand_and_exponent(&compact.significand().absolute, compact.exponent())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::str::FromStr;
    use fixed_decimal::Decimal;

    #[test]
    fn test_u128_overflow() {
        let limit_18 = LIMIT_18_DIGITS;

        // Small u128
        let val_small: u128 = 123;
        let ops = PluralOperands::from(val_small);
        assert_eq!(ops.i, 123);
        assert_eq!(ops.v, 0);

        // u128::MAX should end in 5.
        let val_max = u128::MAX;
        let ops = PluralOperands::from(val_max);

        assert!(ops.i >= limit_18, "i should be offset by LIMIT_18_DIGITS");
        assert_eq!(ops.i % 10, 5, "Mod 10 Check");
        assert_eq!(ops.i % 100, 55, "Mod 100 Check");

        // Just above limit
        let val_limit = limit_18 as u128;
        let ops = PluralOperands::from(val_limit);
        // limit % limit + limit = 0 + limit
        assert_eq!(ops.i, limit_18);

        let val_limit_plus_1 = val_limit + 1;
        let ops = PluralOperands::from(val_limit_plus_1);
        assert_eq!(ops.i, limit_18 + 1);
    }

    #[test]
    fn test_fixed_decimal_overflow() {
        let limit_18 = LIMIT_18_DIGITS;

        // Huge FixedDecimal
        let mut s = "1".to_string();
        for _ in 0..29 {
            s.push('0');
        }
        s.push('7');

        let dec = Decimal::from_str(&s).unwrap();
        let ops = PluralOperands::from(&dec);

        assert!(ops.i >= limit_18, "i should be offset");
        assert_eq!(ops.i % 10, 7, "Mod 10 check");
        assert_eq!(ops.i, limit_18 + 7);
    }

    #[test]
    fn test_i128_overflow() {
        let limit_18 = LIMIT_18_DIGITS;

        let val = i128::MIN;
        let ops = PluralOperands::from(val);

        assert!(ops.i >= limit_18);
        assert_eq!(ops.i % 10, 8);
    }
}
