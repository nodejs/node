// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::{
    ops::{Add, AddAssign, Div, DivAssign, Mul, MulAssign, Sub, SubAssign},
    str::FromStr,
};

use alloc::borrow::Cow;
use num_bigint::BigInt;
use num_rational::Ratio;
use num_traits::Signed;
use num_traits::{One, Pow, Zero};

use crate::measure::provider::si_prefix::{Base, SiPrefix};

// TODO: add test cases for IcuRatio.
// TODO: Make a decicion on whether to keep the `IcuRatio` public or not.
/// A ratio type that uses `BigInt` as the underlying type.
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct IcuRatio(Ratio<BigInt>);

/// The ratio string is invalid and cannot be parsed.
#[derive(Debug, PartialEq, displaydoc::Display)]
#[non_exhaustive]
pub enum RatioFromStrError {
    /// The ratio string is divided by zero.
    DivisionByZero,

    /// The ratio string contains multiple slashes.
    ///
    /// For example, "1/2/3".
    #[displaydoc("The ratio string contains multiple slashes")]
    MultipleSlashes,

    /// The ratio string contains non-numeric characters in fractions.
    ///
    /// For example, "1/2A".
    #[displaydoc("The ratio string contains non-numeric characters in fractions")]
    NonNumericCharactersInFractions,

    /// The ratio string contains multiple scientific notations.
    ///
    /// For example, "1.5E6E6".
    #[displaydoc("The ratio string contains multiple scientific notations")]
    MultipleScientificNotations,

    /// The ratio string contains multiple decimal points.
    ///
    /// For example, "1.5.6".
    #[displaydoc("The ratio string contains multiple decimal points")]
    MultipleDecimalPoints,

    /// The exponent part of the ratio string is not an integer.
    ///
    /// For example, "1.5E6.5".
    #[displaydoc("The exponent part of the ratio string is not an integer")]
    ExponentPartIsNotAnInteger,

    /// The ratio string is deficient in some other way.
    ParsingBigIntError(num_bigint::ParseBigIntError),
}

impl core::error::Error for RatioFromStrError {}

impl IcuRatio {
    /// Creates a new `IcuRatio` from the given numerator and denominator.
    pub fn from_big_ints(numerator: BigInt, denominator: BigInt) -> Self {
        Self(Ratio::new(numerator, denominator))
    }

    /// Returns the current [`IcuRatio`] as a [`Ratio`] of [`BigInt`].
    pub fn get_ratio(self) -> Ratio<BigInt> {
        self.0
    }

    /// Creates a new `IcuRatio` from the given integer.
    pub fn from_integer(value: u64) -> Self {
        Self(Ratio::from_integer(value.into()))
    }

    /// Returns the reciprocal of the ratio.
    /// For example, the reciprocal of 2/3 is 3/2.
    pub(crate) fn recip(&self) -> Self {
        Self(self.0.recip())
    }

    /// Returns the absolute value of the ratio.
    pub fn abs(&self) -> Self {
        Self(self.0.abs())
    }

    /// Returns a Ratio with value of 2.
    pub fn two() -> Self {
        Self(Ratio::from_integer(2.into()))
    }

    /// Returns a Ratio with value of 10.
    pub fn ten() -> Self {
        Self(Ratio::from_integer(10.into()))
    }

    /// Returns true if the ratio is negative.
    pub fn is_negative(&self) -> bool {
        self.0.is_negative()
    }
}

impl Mul for IcuRatio {
    type Output = Self;

    fn mul(self, rhs: Self) -> Self {
        Self(self.0 * rhs.0)
    }
}

impl Mul<&IcuRatio> for &IcuRatio {
    type Output = IcuRatio;

    fn mul(self, rhs: &IcuRatio) -> IcuRatio {
        IcuRatio(&self.0 * &rhs.0)
    }
}

impl MulAssign<IcuRatio> for IcuRatio {
    fn mul_assign(&mut self, rhs: IcuRatio) {
        self.0 *= rhs.0;
    }
}

impl MulAssign<&SiPrefix> for IcuRatio {
    fn mul_assign(&mut self, rhs: &SiPrefix) {
        match rhs.base {
            Base::Decimal => {
                *self *= IcuRatio::ten().pow(rhs.power as i32);
            }
            Base::Binary => {
                *self *= IcuRatio::two().pow(rhs.power as i32);
            }
        }
    }
}

impl Div for IcuRatio {
    type Output = Self;

    fn div(self, rhs: Self) -> Self {
        Self(self.0 / rhs.0)
    }
}

impl DivAssign for IcuRatio {
    fn div_assign(&mut self, rhs: Self) {
        self.0 /= rhs.0;
    }
}

impl Add for IcuRatio {
    type Output = Self;

    fn add(self, rhs: Self) -> Self {
        Self(self.0 + rhs.0)
    }
}

impl Add<&IcuRatio> for &IcuRatio {
    type Output = IcuRatio;

    fn add(self, rhs: &IcuRatio) -> IcuRatio {
        IcuRatio(&self.0 + &rhs.0)
    }
}

impl AddAssign for IcuRatio {
    fn add_assign(&mut self, rhs: Self) {
        self.0 += rhs.0;
    }
}

impl Sub for IcuRatio {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self {
        Self(self.0 - rhs.0)
    }
}

impl SubAssign for IcuRatio {
    fn sub_assign(&mut self, rhs: Self) {
        self.0 -= rhs.0;
    }
}

impl Pow<i32> for IcuRatio {
    type Output = Self;

    fn pow(self, exp: i32) -> Self {
        Self(self.0.pow(exp))
    }
}

impl Zero for IcuRatio {
    fn zero() -> Self {
        Self(Ratio::zero())
    }

    fn is_zero(&self) -> bool {
        self.0.is_zero()
    }
}

impl One for IcuRatio {
    fn one() -> Self {
        Self(Ratio::one())
    }

    fn is_one(&self) -> bool {
        self.0.is_one()
    }
}

impl From<Ratio<BigInt>> for IcuRatio {
    fn from(ratio: Ratio<BigInt>) -> Self {
        Self(ratio)
    }
}

impl From<u32> for IcuRatio {
    fn from(value: u32) -> Self {
        Self(Ratio::from_integer(value.into()))
    }
}

impl FromStr for IcuRatio {
    type Err = RatioFromStrError;

    /// Converts a string representation of a ratio into an `IcuRatio`.
    /// Supported string formats include:
    /// ```
    /// use core::str::FromStr;
    /// use icu::experimental::units::ratio::IcuRatio;
    /// use num_bigint::BigInt;
    /// use num_rational::Ratio;
    /// use num_traits::identities::Zero;
    ///
    /// // Fractional notation
    /// let ratio = IcuRatio::from_str("1/2").unwrap();
    /// assert_eq!(
    ///     ratio,
    ///     IcuRatio::from(Ratio::new(BigInt::from(1), BigInt::from(2)))
    /// );
    ///
    /// // Decimal notation
    /// let ratio = IcuRatio::from_str("1.5").unwrap();
    /// assert_eq!(
    ///     ratio,
    ///     IcuRatio::from(Ratio::new(BigInt::from(3), BigInt::from(2)))
    /// );
    ///
    /// // Scientific notation
    /// let ratio = IcuRatio::from_str("1.5E6").unwrap();
    /// assert_eq!(
    ///     ratio,
    ///     IcuRatio::from(Ratio::from_integer(BigInt::from(1500000)))
    /// );
    ///
    /// // Scientific notation with negative exponent
    /// let ratio = IcuRatio::from_str("1.5E-6").unwrap();
    /// assert_eq!(
    ///     ratio,
    ///     IcuRatio::from(Ratio::new(BigInt::from(15), BigInt::from(10000000)))
    /// );
    ///
    /// // Scientific notation with commas
    /// let ratio = IcuRatio::from_str("1,500E6").unwrap();
    /// assert_eq!(
    ///     ratio,
    ///     IcuRatio::from(Ratio::from_integer(BigInt::from(1500000000)))
    /// );
    ///
    /// // Integer notation
    /// let ratio = IcuRatio::from_str("1").unwrap();
    /// assert_eq!(ratio, IcuRatio::from(Ratio::from_integer(BigInt::from(1))));
    ///
    /// // Fractional notation with exponent
    /// let ratio = IcuRatio::from_str("1/2E5").unwrap();
    /// assert_eq!(
    ///     ratio,
    ///     IcuRatio::from(Ratio::from_integer(BigInt::from(50000)))
    /// );
    ///
    /// // Negative numbers in fractional notation
    /// let ratio = IcuRatio::from_str("-1/2").unwrap();
    /// assert_eq!(
    ///     ratio,
    ///     IcuRatio::from(Ratio::new(BigInt::from(-1), BigInt::from(2)))
    /// );
    ///
    /// // Negative numbers in decimal notation
    /// let ratio = IcuRatio::from_str("-1.5").unwrap();
    /// assert_eq!(
    ///     ratio,
    ///     IcuRatio::from(Ratio::new(BigInt::from(-3), BigInt::from(2)))
    /// );
    ///
    /// // Negative numbers in scientific notation
    /// let ratio = IcuRatio::from_str("-1.5E6").unwrap();
    /// assert_eq!(
    ///     ratio,
    ///     IcuRatio::from(Ratio::from_integer(BigInt::from(-1500000)))
    /// );
    ///
    /// // Negative numbers in scientific notation with commas
    /// let ratio = IcuRatio::from_str("-1,500E6").unwrap();
    /// assert_eq!(
    ///     ratio,
    ///     IcuRatio::from(Ratio::from_integer(BigInt::from(-1500000000)))
    /// );
    ///
    /// // Corner cases
    ///
    /// // Empty string
    /// let ratio = IcuRatio::from_str("").unwrap();
    /// assert_eq!(ratio, IcuRatio::zero());
    ///
    /// // Single dot
    /// let ratio = IcuRatio::from_str(".").unwrap();
    /// assert_eq!(ratio, IcuRatio::zero());
    ///
    /// // Single minus
    /// let ratio = IcuRatio::from_str("-").unwrap();
    /// assert_eq!(ratio, IcuRatio::zero());
    ///
    /// // Single plus
    /// let ratio = IcuRatio::from_str("+").unwrap();
    /// assert_eq!(ratio, IcuRatio::zero());
    ///
    /// // Only zeros after dot
    /// let ratio = IcuRatio::from_str(".000").unwrap();
    /// assert_eq!(ratio, IcuRatio::zero());
    ///
    /// // Error cases
    /// // Division by zero
    /// assert!(IcuRatio::from_str("1/0").is_err());
    ///
    /// // Multiple slashes
    /// assert!(IcuRatio::from_str("1/2/3").is_err());
    ///
    /// // Non-numeric characters in fractions
    /// assert!(IcuRatio::from_str("1/2A").is_err());
    ///
    /// // Multiple scientific notations
    /// assert!(IcuRatio::from_str("1.5E6E6").is_err());
    ///
    /// // Multiple decimal points
    /// assert!(IcuRatio::from_str("1.5.6").is_err());
    ///
    /// // Exponent part is not an integer
    /// assert!(IcuRatio::from_str("1.5E6.5").is_err());
    /// ```
    /// NOTE:
    ///    You can add as many commas as you want in the string, they will be disregarded.
    fn from_str(number_str: &str) -> Result<Self, Self::Err> {
        /// This function interprets numeric strings and converts them into an `IcuRatio` object, supporting:
        /// - Simple fractions: e.g., "1/2".
        /// - Decimals: e.g., "1.5".
        /// - Mixed fractions: e.g., "1.5/6", "1.4/5.6".
        ///
        /// Note: Scientific notation is not supported.
        fn parse_fraction(decimal: &str) -> Result<IcuRatio, RatioFromStrError> {
            let mut components = decimal.split('/');
            let numerator = components.next();
            let denominator = components.next();
            if components.next().is_some() {
                return Err(RatioFromStrError::MultipleSlashes);
            }

            let numerator = match numerator {
                Some(numerator) => parse_decimal(numerator)?,
                None => IcuRatio::zero(),
            };

            let denominator = match denominator {
                Some(denominator) => parse_decimal(denominator)?,
                None => IcuRatio::one(),
            };

            if denominator.is_zero() {
                return Err(RatioFromStrError::DivisionByZero);
            }

            Ok(numerator / denominator)
        }

        /// Parses a decimal number from a string input.
        ///
        /// Accepts input in various decimal formats, including:
        /// - Whole numbers: "1"
        /// - Decimal numbers: "1.5"
        ///
        /// An empty string input is interpreted as "0".
        ///
        /// Note: Fractional inputs are not supported in this context.
        fn parse_decimal(decimal: &str) -> Result<IcuRatio, RatioFromStrError> {
            let is_negative = decimal.starts_with('-');
            let mut dot_parts = decimal.split('.');
            let integer_part = dot_parts.next();
            let decimal_part = dot_parts.next();
            if dot_parts.next().is_some() {
                return Err(RatioFromStrError::MultipleDecimalPoints);
            }

            let integer_part = match integer_part {
                None | Some("") => IcuRatio::zero(),
                Some("-") | Some("+") => IcuRatio::zero(),
                Some(integer_part) => IcuRatio(
                    BigInt::from_str(integer_part)
                        .map_err(RatioFromStrError::ParsingBigIntError)?
                        .into(),
                ),
            };

            let decimal_part = match decimal_part {
                None | Some("") => return Ok(integer_part),
                Some(decimal_part) => {
                    let decimal_power = decimal_part.len() as i32;
                    let decimal_part = IcuRatio(
                        BigInt::from_str(decimal_part)
                            .map_err(RatioFromStrError::ParsingBigIntError)?
                            .into(),
                    );

                    decimal_part / IcuRatio::ten().pow(decimal_power) // Divide by 10^decimal_power
                }
            };

            Ok(if is_negative {
                integer_part - decimal_part
            } else {
                integer_part + decimal_part
            })
        }

        let number_str = if number_str.contains(',') {
            Cow::Owned(number_str.replace(',', ""))
        } else {
            Cow::Borrowed(number_str)
        };
        if number_str.is_empty() {
            return Ok(IcuRatio::zero());
        }

        let mut parts = number_str.split(['e', 'E']);
        let significand = parts.next();
        let exponent = parts.next();
        if parts.next().is_some() {
            return Err(RatioFromStrError::MultipleScientificNotations);
        }

        let significand = match significand {
            Some(significand) => parse_fraction(significand)?,
            None => IcuRatio::one(),
        };

        let exponent = match exponent {
            Some(exponent) => {
                let exponent = exponent
                    .parse::<i32>()
                    .map_err(|_| RatioFromStrError::ExponentPartIsNotAnInteger)?;
                IcuRatio::ten().pow(exponent)
            }
            None => IcuRatio::one(),
        };

        Ok(significand * exponent)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_icu_ratio_from_str() {
        let test_cases: &[(&str, Result<IcuRatio, RatioFromStrError>)] = &[
            ("1/2", Ok(IcuRatio::from_big_ints(1.into(), 2.into()))),
            ("1.5", Ok(IcuRatio::from_big_ints(3.into(), 2.into()))),
            (
                "-1.5",
                Ok(IcuRatio::from_big_ints(BigInt::from(-3), 2.into())),
            ),
            (
                "-.05",
                Ok(IcuRatio::from_big_ints(BigInt::from(-5), 100.into())),
            ),
            ("+.05", Ok(IcuRatio::from_big_ints(5.into(), 100.into()))),
            ("+1.5", Ok(IcuRatio::from_big_ints(15.into(), 10.into()))),
            (
                "1.5E6",
                Ok(IcuRatio::from_big_ints(1500000.into(), 1.into())),
            ),
            (
                "1.5E-6",
                Ok(IcuRatio::from_big_ints(15.into(), 10000000.into())),
            ),
            (
                "1,500E6",
                Ok(IcuRatio::from_big_ints(1500000000.into(), 1.into())),
            ),
            (
                "1.0005",
                Ok(IcuRatio::from_big_ints(10005.into(), 10000.into())),
            ),
            (".0005", Ok(IcuRatio::from_big_ints(5.into(), 10000.into()))),
            ("1", Ok(IcuRatio::from_big_ints(1.into(), 1.into()))),
            ("", Ok(IcuRatio::from_big_ints(0.into(), 1.into()))),
            (".", Ok(IcuRatio::from_big_ints(0.into(), 1.into()))),
            ("-", Ok(IcuRatio::from_big_ints(0.into(), 1.into()))),
            ("+", Ok(IcuRatio::from_big_ints(0.into(), 1.into()))),
            (".000", Ok(IcuRatio::from_big_ints(0.into(), 1.into()))),
            (
                "-1/2E5",
                Ok(IcuRatio::from_big_ints(BigInt::from(-50000), 1.into())),
            ),
            ("1/2E5", Ok(IcuRatio::from_big_ints(50000.into(), 1.into()))),
            // commas are neglected
            (",,,,", Ok(IcuRatio::from_big_ints(0.into(), 1.into()))),
            ("1/0", Err(RatioFromStrError::DivisionByZero)),
            ("1/2/3", Err(RatioFromStrError::MultipleSlashes)),
            // TODO: fix how to test the error returned as another error type.
            // ("1/2A", Err(RatioFromStrError::ParsingBigIntError()),
        ];

        for (input, expected) in test_cases.iter() {
            let actual = &IcuRatio::from_str(input);
            assert_eq!(actual, expected, "Values do not match for input: {input}");
        }
    }
}
