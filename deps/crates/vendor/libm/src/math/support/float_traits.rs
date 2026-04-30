#![allow(unknown_lints)] // FIXME(msrv) we shouldn't need this

use core::{fmt, mem, ops};

use super::int_traits::{CastFrom, Int, MinInt};

/// Trait for some basic operations on floats
// #[allow(dead_code)]
pub trait Float:
    Copy
    + fmt::Debug
    + PartialEq
    + PartialOrd
    + ops::AddAssign
    + ops::MulAssign
    + ops::Add<Output = Self>
    + ops::Sub<Output = Self>
    + ops::Mul<Output = Self>
    + ops::Div<Output = Self>
    + ops::Rem<Output = Self>
    + ops::Neg<Output = Self>
    + 'static
{
    /// A uint of the same width as the float
    type Int: Int<OtherSign = Self::SignedInt, Unsigned = Self::Int>;

    /// A int of the same width as the float
    type SignedInt: Int
        + MinInt<OtherSign = Self::Int, Unsigned = Self::Int>
        + ops::Neg<Output = Self::SignedInt>;

    const ZERO: Self;
    const NEG_ZERO: Self;
    const ONE: Self;
    const NEG_ONE: Self;
    const INFINITY: Self;
    const NEG_INFINITY: Self;
    const NAN: Self;
    const NEG_NAN: Self;
    const MAX: Self;
    const MIN: Self;
    const EPSILON: Self;
    const PI: Self;
    const NEG_PI: Self;
    const FRAC_PI_2: Self;

    const MIN_POSITIVE_NORMAL: Self;

    /// The bitwidth of the float type
    const BITS: u32;

    /// The bitwidth of the significand
    const SIG_BITS: u32;

    /// The bitwidth of the exponent
    const EXP_BITS: u32 = Self::BITS - Self::SIG_BITS - 1;

    /// The saturated (maximum bitpattern) value of the exponent, i.e. the infinite
    /// representation.
    ///
    /// This shifted fully right, use `EXP_MASK` for the shifted value.
    const EXP_SAT: u32 = (1 << Self::EXP_BITS) - 1;

    /// The exponent bias value
    const EXP_BIAS: u32 = Self::EXP_SAT >> 1;

    /// Maximum unbiased exponent value.
    const EXP_MAX: i32 = Self::EXP_BIAS as i32;

    /// Minimum *NORMAL* unbiased exponent value.
    const EXP_MIN: i32 = -(Self::EXP_MAX - 1);

    /// Minimum subnormal exponent value.
    const EXP_MIN_SUBNORM: i32 = Self::EXP_MIN - Self::SIG_BITS as i32;

    /// A mask for the sign bit
    const SIGN_MASK: Self::Int;

    /// A mask for the significand
    const SIG_MASK: Self::Int;

    /// A mask for the exponent
    const EXP_MASK: Self::Int;

    /// The implicit bit of the float format
    const IMPLICIT_BIT: Self::Int;

    /// Returns `self` transmuted to `Self::Int`
    fn to_bits(self) -> Self::Int;

    /// Returns `self` transmuted to `Self::SignedInt`
    #[allow(dead_code)]
    fn to_bits_signed(self) -> Self::SignedInt {
        self.to_bits().signed()
    }

    /// Check bitwise equality.
    #[allow(dead_code)]
    fn biteq(self, rhs: Self) -> bool {
        self.to_bits() == rhs.to_bits()
    }

    /// Checks if two floats have the same bit representation. *Except* for NaNs! NaN can be
    /// represented in multiple different ways.
    ///
    /// This method returns `true` if two NaNs are compared. Use [`biteq`](Self::biteq) instead
    /// if `NaN` should not be treated separately.
    #[allow(dead_code)]
    fn eq_repr(self, rhs: Self) -> bool {
        if self.is_nan() && rhs.is_nan() {
            true
        } else {
            self.biteq(rhs)
        }
    }

    /// Returns true if the value is NaN.
    fn is_nan(self) -> bool;

    /// Returns true if the value is +inf or -inf.
    fn is_infinite(self) -> bool;

    /// Returns true if the sign is negative. Extracts the sign bit regardless of zero or NaN.
    fn is_sign_negative(self) -> bool;

    /// Returns true if the sign is positive. Extracts the sign bit regardless of zero or NaN.
    fn is_sign_positive(self) -> bool {
        !self.is_sign_negative()
    }

    /// Returns if `self` is subnormal.
    #[allow(dead_code)]
    fn is_subnormal(self) -> bool {
        (self.to_bits() & Self::EXP_MASK) == Self::Int::ZERO
    }

    /// Returns the exponent, not adjusting for bias, not accounting for subnormals or zero.
    fn ex(self) -> u32 {
        u32::cast_from(self.to_bits() >> Self::SIG_BITS) & Self::EXP_SAT
    }

    /// Extract the exponent and adjust it for bias, not accounting for subnormals or zero.
    fn exp_unbiased(self) -> i32 {
        self.ex().signed() - (Self::EXP_BIAS as i32)
    }

    /// Returns the significand with no implicit bit (or the "fractional" part)
    #[allow(dead_code)]
    fn frac(self) -> Self::Int {
        self.to_bits() & Self::SIG_MASK
    }

    /// Returns a `Self::Int` transmuted back to `Self`
    fn from_bits(a: Self::Int) -> Self;

    /// Constructs a `Self` from its parts. Inputs are treated as bits and shifted into position.
    fn from_parts(negative: bool, exponent: u32, significand: Self::Int) -> Self {
        let sign = if negative {
            Self::Int::ONE
        } else {
            Self::Int::ZERO
        };
        Self::from_bits(
            (sign << (Self::BITS - 1))
                | (Self::Int::cast_from(exponent & Self::EXP_SAT) << Self::SIG_BITS)
                | (significand & Self::SIG_MASK),
        )
    }

    #[allow(dead_code)]
    fn abs(self) -> Self;

    /// Returns a number composed of the magnitude of self and the sign of sign.
    fn copysign(self, other: Self) -> Self;

    /// Fused multiply add, rounding once.
    fn fma(self, y: Self, z: Self) -> Self;

    /// Returns (normalized exponent, normalized significand)
    #[allow(dead_code)]
    fn normalize(significand: Self::Int) -> (i32, Self::Int);

    /// Returns a number that represents the sign of self.
    #[allow(dead_code)]
    fn signum(self) -> Self {
        if self.is_nan() {
            self
        } else {
            Self::ONE.copysign(self)
        }
    }
}

/// Access the associated `Int` type from a float (helper to avoid ambiguous associated types).
pub type IntTy<F> = <F as Float>::Int;

macro_rules! float_impl {
    (
        $ty:ident,
        $ity:ident,
        $sity:ident,
        $bits:expr,
        $significand_bits:expr,
        $from_bits:path,
        $to_bits:path,
        $fma_fn:ident,
        $fma_intrinsic:ident
    ) => {
        impl Float for $ty {
            type Int = $ity;
            type SignedInt = $sity;

            const ZERO: Self = 0.0;
            const NEG_ZERO: Self = -0.0;
            const ONE: Self = 1.0;
            const NEG_ONE: Self = -1.0;
            const INFINITY: Self = Self::INFINITY;
            const NEG_INFINITY: Self = Self::NEG_INFINITY;
            const NAN: Self = Self::NAN;
            // NAN isn't guaranteed to be positive but it usually is. We only use this for
            // tests.
            const NEG_NAN: Self = $from_bits($to_bits(Self::NAN) | Self::SIGN_MASK);
            const MAX: Self = -Self::MIN;
            // Sign bit set, saturated mantissa, saturated exponent with last bit zeroed
            const MIN: Self = $from_bits(Self::Int::MAX & !(1 << Self::SIG_BITS));
            const EPSILON: Self = <$ty>::EPSILON;

            // Exponent is a 1 in the LSB
            const MIN_POSITIVE_NORMAL: Self = $from_bits(1 << Self::SIG_BITS);

            const PI: Self = core::$ty::consts::PI;
            const NEG_PI: Self = -Self::PI;
            const FRAC_PI_2: Self = core::$ty::consts::FRAC_PI_2;

            const BITS: u32 = $bits;
            const SIG_BITS: u32 = $significand_bits;

            const SIGN_MASK: Self::Int = 1 << (Self::BITS - 1);
            const SIG_MASK: Self::Int = (1 << Self::SIG_BITS) - 1;
            const EXP_MASK: Self::Int = !(Self::SIGN_MASK | Self::SIG_MASK);
            const IMPLICIT_BIT: Self::Int = 1 << Self::SIG_BITS;

            fn to_bits(self) -> Self::Int {
                self.to_bits()
            }
            fn is_nan(self) -> bool {
                self.is_nan()
            }
            fn is_infinite(self) -> bool {
                self.is_infinite()
            }
            fn is_sign_negative(self) -> bool {
                self.is_sign_negative()
            }
            fn from_bits(a: Self::Int) -> Self {
                Self::from_bits(a)
            }
            fn abs(self) -> Self {
                cfg_if! {
                    // FIXME(msrv): `abs` is available in `core` starting with 1.85.
                    if #[cfg(intrinsics_enabled)] {
                        self.abs()
                    } else {
                        super::super::generic::fabs(self)
                    }
                }
            }
            fn copysign(self, other: Self) -> Self {
                cfg_if! {
                    // FIXME(msrv): `copysign` is available in `core` starting with 1.85.
                    if #[cfg(intrinsics_enabled)] {
                        self.copysign(other)
                    } else {
                        super::super::generic::copysign(self, other)
                    }
                }
            }
            fn fma(self, y: Self, z: Self) -> Self {
                cfg_if! {
                    // fma is not yet available in `core`
                    if #[cfg(intrinsics_enabled)] {
                        unsafe{ core::intrinsics::$fma_intrinsic(self, y, z) }
                    } else {
                        super::super::$fma_fn(self, y, z)
                    }
                }
            }
            fn normalize(significand: Self::Int) -> (i32, Self::Int) {
                let shift = significand.leading_zeros().wrapping_sub(Self::EXP_BITS);
                (
                    1i32.wrapping_sub(shift as i32),
                    significand << shift as Self::Int,
                )
            }
        }
    };
}

#[cfg(f16_enabled)]
float_impl!(
    f16,
    u16,
    i16,
    16,
    10,
    f16::from_bits,
    f16::to_bits,
    fmaf16,
    fmaf16
);
float_impl!(
    f32,
    u32,
    i32,
    32,
    23,
    f32_from_bits,
    f32_to_bits,
    fmaf,
    fmaf32
);
float_impl!(
    f64,
    u64,
    i64,
    64,
    52,
    f64_from_bits,
    f64_to_bits,
    fma,
    fmaf64
);
#[cfg(f128_enabled)]
float_impl!(
    f128,
    u128,
    i128,
    128,
    112,
    f128::from_bits,
    f128::to_bits,
    fmaf128,
    fmaf128
);

/* FIXME(msrv): vendor some things that are not const stable at our MSRV */

/// `f32::from_bits`
#[allow(unnecessary_transmutes)] // lint appears in newer versions of Rust
pub const fn f32_from_bits(bits: u32) -> f32 {
    // SAFETY: POD cast with no preconditions
    unsafe { mem::transmute::<u32, f32>(bits) }
}

/// `f32::to_bits`
#[allow(unnecessary_transmutes)] // lint appears in newer versions of Rust
pub const fn f32_to_bits(x: f32) -> u32 {
    // SAFETY: POD cast with no preconditions
    unsafe { mem::transmute::<f32, u32>(x) }
}

/// `f64::from_bits`
#[allow(unnecessary_transmutes)] // lint appears in newer versions of Rust
pub const fn f64_from_bits(bits: u64) -> f64 {
    // SAFETY: POD cast with no preconditions
    unsafe { mem::transmute::<u64, f64>(bits) }
}

/// `f64::to_bits`
#[allow(unnecessary_transmutes)] // lint appears in newer versions of Rust
pub const fn f64_to_bits(x: f64) -> u64 {
    // SAFETY: POD cast with no preconditions
    unsafe { mem::transmute::<f64, u64>(x) }
}

/// Trait for floats twice the bit width of another integer.
pub trait DFloat: Float {
    /// Float that is half the bit width of the floatthis trait is implemented for.
    type H: HFloat<D = Self>;

    /// Narrow the float type.
    fn narrow(self) -> Self::H;
}

/// Trait for floats half the bit width of another float.
pub trait HFloat: Float {
    /// Float that is double the bit width of the float this trait is implemented for.
    type D: DFloat<H = Self>;

    /// Widen the float type.
    fn widen(self) -> Self::D;
}

macro_rules! impl_d_float {
    ($($X:ident $D:ident),*) => {
        $(
            impl DFloat for $D {
                type H = $X;

                fn narrow(self) -> Self::H {
                    self as $X
                }
            }
        )*
    };
}

macro_rules! impl_h_float {
    ($($H:ident $X:ident),*) => {
        $(
            impl HFloat for $H {
                type D = $X;

                fn widen(self) -> Self::D {
                    self as $X
                }
            }
        )*
    };
}

impl_d_float!(f32 f64);
#[cfg(f16_enabled)]
impl_d_float!(f16 f32);
#[cfg(f128_enabled)]
impl_d_float!(f64 f128);

impl_h_float!(f32 f64);
#[cfg(f16_enabled)]
impl_h_float!(f16 f32);
#[cfg(f128_enabled)]
impl_h_float!(f64 f128);

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[cfg(f16_enabled)]
    fn check_f16() {
        // Constants
        assert_eq!(f16::EXP_SAT, 0b11111);
        assert_eq!(f16::EXP_BIAS, 15);
        assert_eq!(f16::EXP_MAX, 15);
        assert_eq!(f16::EXP_MIN, -14);
        assert_eq!(f16::EXP_MIN_SUBNORM, -24);

        // `exp_unbiased`
        assert_eq!(f16::FRAC_PI_2.exp_unbiased(), 0);
        assert_eq!((1.0f16 / 2.0).exp_unbiased(), -1);
        assert_eq!(f16::MAX.exp_unbiased(), 15);
        assert_eq!(f16::MIN.exp_unbiased(), 15);
        assert_eq!(f16::MIN_POSITIVE.exp_unbiased(), -14);
        // This is a convenience method and not ldexp, `exp_unbiased` does not return correct
        // results for zero and subnormals.
        assert_eq!(f16::ZERO.exp_unbiased(), -15);
        assert_eq!(f16::from_bits(0x1).exp_unbiased(), -15);
        assert_eq!(f16::MIN_POSITIVE, f16::MIN_POSITIVE_NORMAL);

        // `from_parts`
        assert_biteq!(f16::from_parts(true, f16::EXP_BIAS, 0), -1.0f16);
        assert_biteq!(f16::from_parts(false, 0, 1), f16::from_bits(0x1));
    }

    #[test]
    fn check_f32() {
        // Constants
        assert_eq!(f32::EXP_SAT, 0b11111111);
        assert_eq!(f32::EXP_BIAS, 127);
        assert_eq!(f32::EXP_MAX, 127);
        assert_eq!(f32::EXP_MIN, -126);
        assert_eq!(f32::EXP_MIN_SUBNORM, -149);

        // `exp_unbiased`
        assert_eq!(f32::FRAC_PI_2.exp_unbiased(), 0);
        assert_eq!((1.0f32 / 2.0).exp_unbiased(), -1);
        assert_eq!(f32::MAX.exp_unbiased(), 127);
        assert_eq!(f32::MIN.exp_unbiased(), 127);
        assert_eq!(f32::MIN_POSITIVE.exp_unbiased(), -126);
        // This is a convenience method and not ldexp, `exp_unbiased` does not return correct
        // results for zero and subnormals.
        assert_eq!(f32::ZERO.exp_unbiased(), -127);
        assert_eq!(f32::from_bits(0x1).exp_unbiased(), -127);
        assert_eq!(f32::MIN_POSITIVE, f32::MIN_POSITIVE_NORMAL);

        // `from_parts`
        assert_biteq!(f32::from_parts(true, f32::EXP_BIAS, 0), -1.0f32);
        assert_biteq!(
            f32::from_parts(false, 10 + f32::EXP_BIAS, 0),
            hf32!("0x1p10")
        );
        assert_biteq!(f32::from_parts(false, 0, 1), f32::from_bits(0x1));
    }

    #[test]
    fn check_f64() {
        // Constants
        assert_eq!(f64::EXP_SAT, 0b11111111111);
        assert_eq!(f64::EXP_BIAS, 1023);
        assert_eq!(f64::EXP_MAX, 1023);
        assert_eq!(f64::EXP_MIN, -1022);
        assert_eq!(f64::EXP_MIN_SUBNORM, -1074);

        // `exp_unbiased`
        assert_eq!(f64::FRAC_PI_2.exp_unbiased(), 0);
        assert_eq!((1.0f64 / 2.0).exp_unbiased(), -1);
        assert_eq!(f64::MAX.exp_unbiased(), 1023);
        assert_eq!(f64::MIN.exp_unbiased(), 1023);
        assert_eq!(f64::MIN_POSITIVE.exp_unbiased(), -1022);
        // This is a convenience method and not ldexp, `exp_unbiased` does not return correct
        // results for zero and subnormals.
        assert_eq!(f64::ZERO.exp_unbiased(), -1023);
        assert_eq!(f64::from_bits(0x1).exp_unbiased(), -1023);
        assert_eq!(f64::MIN_POSITIVE, f64::MIN_POSITIVE_NORMAL);

        // `from_parts`
        assert_biteq!(f64::from_parts(true, f64::EXP_BIAS, 0), -1.0f64);
        assert_biteq!(
            f64::from_parts(false, 10 + f64::EXP_BIAS, 0),
            hf64!("0x1p10")
        );
        assert_biteq!(f64::from_parts(false, 0, 1), f64::from_bits(0x1));
    }

    #[test]
    #[cfg(f128_enabled)]
    fn check_f128() {
        // Constants
        assert_eq!(f128::EXP_SAT, 0b111111111111111);
        assert_eq!(f128::EXP_BIAS, 16383);
        assert_eq!(f128::EXP_MAX, 16383);
        assert_eq!(f128::EXP_MIN, -16382);
        assert_eq!(f128::EXP_MIN_SUBNORM, -16494);

        // `exp_unbiased`
        assert_eq!(f128::FRAC_PI_2.exp_unbiased(), 0);
        assert_eq!((1.0f128 / 2.0).exp_unbiased(), -1);
        assert_eq!(f128::MAX.exp_unbiased(), 16383);
        assert_eq!(f128::MIN.exp_unbiased(), 16383);
        assert_eq!(f128::MIN_POSITIVE.exp_unbiased(), -16382);
        // This is a convenience method and not ldexp, `exp_unbiased` does not return correct
        // results for zero and subnormals.
        assert_eq!(f128::ZERO.exp_unbiased(), -16383);
        assert_eq!(f128::from_bits(0x1).exp_unbiased(), -16383);
        assert_eq!(f128::MIN_POSITIVE, f128::MIN_POSITIVE_NORMAL);

        // `from_parts`
        assert_biteq!(f128::from_parts(true, f128::EXP_BIAS, 0), -1.0f128);
        assert_biteq!(f128::from_parts(false, 0, 1), f128::from_bits(0x1));
    }
}
