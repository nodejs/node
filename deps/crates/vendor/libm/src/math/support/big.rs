//! Integers used for wide operations, larger than `u128`.

#[cfg(test)]
mod tests;

use core::ops;

use super::{DInt, HInt, Int, MinInt};

const U128_LO_MASK: u128 = u64::MAX as u128;

/// A 256-bit unsigned integer represented as two 128-bit native-endian limbs.
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, Debug, PartialEq, PartialOrd)]
pub struct u256 {
    pub lo: u128,
    pub hi: u128,
}

impl u256 {
    #[cfg(any(test, feature = "unstable-public-internals"))]
    pub const MAX: Self = Self {
        lo: u128::MAX,
        hi: u128::MAX,
    };

    /// Reinterpret as a signed integer
    pub fn signed(self) -> i256 {
        i256 {
            lo: self.lo,
            hi: self.hi,
        }
    }
}

/// A 256-bit signed integer represented as two 128-bit native-endian limbs.
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, Debug, PartialEq, PartialOrd)]
pub struct i256 {
    pub lo: u128,
    pub hi: u128,
}

impl i256 {
    /// Reinterpret as an unsigned integer
    #[cfg(any(test, feature = "unstable-public-internals"))]
    pub fn unsigned(self) -> u256 {
        u256 {
            lo: self.lo,
            hi: self.hi,
        }
    }
}

impl MinInt for u256 {
    type OtherSign = i256;

    type Unsigned = u256;

    const SIGNED: bool = false;
    const BITS: u32 = 256;
    const ZERO: Self = Self { lo: 0, hi: 0 };
    const ONE: Self = Self { lo: 1, hi: 0 };
    const MIN: Self = Self { lo: 0, hi: 0 };
    const MAX: Self = Self {
        lo: u128::MAX,
        hi: u128::MAX,
    };
}

impl MinInt for i256 {
    type OtherSign = u256;

    type Unsigned = u256;

    const SIGNED: bool = false;
    const BITS: u32 = 256;
    const ZERO: Self = Self { lo: 0, hi: 0 };
    const ONE: Self = Self { lo: 1, hi: 0 };
    const MIN: Self = Self {
        lo: 0,
        hi: 1 << 127,
    };
    const MAX: Self = Self {
        lo: u128::MAX,
        hi: u128::MAX << 1,
    };
}

macro_rules! impl_common {
    ($ty:ty) => {
        impl ops::BitOr for $ty {
            type Output = Self;

            fn bitor(mut self, rhs: Self) -> Self::Output {
                self.lo |= rhs.lo;
                self.hi |= rhs.hi;
                self
            }
        }

        impl ops::Not for $ty {
            type Output = Self;

            fn not(mut self) -> Self::Output {
                self.lo = !self.lo;
                self.hi = !self.hi;
                self
            }
        }

        impl ops::Shl<u32> for $ty {
            type Output = Self;

            fn shl(self, _rhs: u32) -> Self::Output {
                unimplemented!("only used to meet trait bounds")
            }
        }
    };
}

impl_common!(i256);
impl_common!(u256);

impl ops::Add<Self> for u256 {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        let (lo, carry) = self.lo.overflowing_add(rhs.lo);
        let hi = self.hi.wrapping_add(carry as u128).wrapping_add(rhs.hi);

        Self { lo, hi }
    }
}

impl ops::Shr<u32> for u256 {
    type Output = Self;

    fn shr(mut self, rhs: u32) -> Self::Output {
        debug_assert!(rhs < Self::BITS, "attempted to shift right with overflow");
        if rhs >= Self::BITS {
            return Self::ZERO;
        }

        if rhs == 0 {
            return self;
        }

        if rhs < 128 {
            self.lo >>= rhs;
            self.lo |= self.hi << (128 - rhs);
        } else {
            self.lo = self.hi >> (rhs - 128);
        }

        if rhs < 128 {
            self.hi >>= rhs;
        } else {
            self.hi = 0;
        }

        self
    }
}

impl HInt for u128 {
    type D = u256;

    fn widen(self) -> Self::D {
        u256 { lo: self, hi: 0 }
    }

    fn zero_widen(self) -> Self::D {
        self.widen()
    }

    fn zero_widen_mul(self, rhs: Self) -> Self::D {
        let l0 = self & U128_LO_MASK;
        let l1 = rhs & U128_LO_MASK;
        let h0 = self >> 64;
        let h1 = rhs >> 64;

        let p_ll: u128 = l0.overflowing_mul(l1).0;
        let p_lh: u128 = l0.overflowing_mul(h1).0;
        let p_hl: u128 = h0.overflowing_mul(l1).0;
        let p_hh: u128 = h0.overflowing_mul(h1).0;

        let s0 = p_hl + (p_ll >> 64);
        let s1 = (p_ll & U128_LO_MASK) + (s0 << 64);
        let s2 = p_lh + (s1 >> 64);

        let lo = (p_ll & U128_LO_MASK) + (s2 << 64);
        let hi = p_hh + (s0 >> 64) + (s2 >> 64);

        u256 { lo, hi }
    }

    fn widen_mul(self, rhs: Self) -> Self::D {
        self.zero_widen_mul(rhs)
    }

    fn widen_hi(self) -> Self::D {
        self.widen() << <Self as MinInt>::BITS
    }
}

impl HInt for i128 {
    type D = i256;

    fn widen(self) -> Self::D {
        let mut ret = self.unsigned().zero_widen().signed();
        if self.is_negative() {
            ret.hi = u128::MAX;
        }
        ret
    }

    fn zero_widen(self) -> Self::D {
        self.unsigned().zero_widen().signed()
    }

    fn zero_widen_mul(self, rhs: Self) -> Self::D {
        self.unsigned().zero_widen_mul(rhs.unsigned()).signed()
    }

    fn widen_mul(self, _rhs: Self) -> Self::D {
        unimplemented!("signed i128 widening multiply is not used")
    }

    fn widen_hi(self) -> Self::D {
        self.widen() << <Self as MinInt>::BITS
    }
}

impl DInt for u256 {
    type H = u128;

    fn lo(self) -> Self::H {
        self.lo
    }

    fn hi(self) -> Self::H {
        self.hi
    }
}

impl DInt for i256 {
    type H = i128;

    fn lo(self) -> Self::H {
        self.lo as i128
    }

    fn hi(self) -> Self::H {
        self.hi as i128
    }
}
