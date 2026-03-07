use core::fmt::Display;
use core::ops::{Add, BitAnd, BitOr, BitOrAssign, BitXorAssign, Div, Mul, Shl, Shr, Sub};

pub trait Float: Copy {
    const MANTISSA_DIGITS: u32;
    const MIN_10_EXP: i32;
    const MAX_10_EXP: i32;
    const MAX_DIGITS10: u32;
}

impl Float for f32 {
    const MANTISSA_DIGITS: u32 = Self::MANTISSA_DIGITS;
    const MIN_10_EXP: i32 = Self::MIN_10_EXP;
    const MAX_10_EXP: i32 = Self::MAX_10_EXP;
    const MAX_DIGITS10: u32 = 9;
}

impl Float for f64 {
    const MANTISSA_DIGITS: u32 = Self::MANTISSA_DIGITS;
    const MIN_10_EXP: i32 = Self::MIN_10_EXP;
    const MAX_10_EXP: i32 = Self::MAX_10_EXP;
    const MAX_DIGITS10: u32 = 17;
}

pub trait UInt:
    Copy
    + From<u8>
    + From<bool>
    + Add<Output = Self>
    + Sub<Output = Self>
    + Mul<Output = Self>
    + Div<Output = Self>
    + BitAnd<Output = Self>
    + BitOr<Output = Self>
    + Shl<u8, Output = Self>
    + Shl<i32, Output = Self>
    + Shl<u32, Output = Self>
    + Shr<i32, Output = Self>
    + Shr<u32, Output = Self>
    + BitOrAssign
    + BitXorAssign
    + PartialOrd
    + Into<u64>
    + Display
{
    type Signed: Ord;
    fn wrapping_sub(self, other: Self) -> Self;
    fn truncate(big: u64) -> Self;
    fn enlarge(small: u32) -> Self;
    fn to_signed(self) -> Self::Signed;
}

impl UInt for u32 {
    type Signed = i32;
    fn wrapping_sub(self, other: Self) -> Self {
        self.wrapping_sub(other)
    }
    fn truncate(big: u64) -> Self {
        big as u32
    }
    fn enlarge(small: u32) -> Self {
        small
    }
    fn to_signed(self) -> Self::Signed {
        self as i32
    }
}

impl UInt for u64 {
    type Signed = i64;
    fn wrapping_sub(self, other: Self) -> Self {
        self.wrapping_sub(other)
    }
    fn truncate(big: u64) -> Self {
        big
    }
    fn enlarge(small: u32) -> Self {
        u64::from(small)
    }
    fn to_signed(self) -> Self::Signed {
        self as i64
    }
}
