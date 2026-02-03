use core::{cmp, fmt, ops};

/// Minimal integer implementations needed on all integer types, including wide integers.
pub trait MinInt:
    Copy
    + fmt::Debug
    + ops::BitOr<Output = Self>
    + ops::Not<Output = Self>
    + ops::Shl<u32, Output = Self>
{
    /// Type with the same width but other signedness
    type OtherSign: MinInt;
    /// Unsigned version of Self
    type Unsigned: MinInt;

    /// If `Self` is a signed integer
    const SIGNED: bool;

    /// The bitwidth of the int type
    const BITS: u32;

    const ZERO: Self;
    const ONE: Self;
    const MIN: Self;
    const MAX: Self;
}

/// Access the associated `OtherSign` type from an int (helper to avoid ambiguous associated
/// types).
pub type OtherSign<I> = <I as MinInt>::OtherSign;

/// Trait for some basic operations on integers
#[allow(dead_code)]
pub trait Int:
    MinInt
    + fmt::Display
    + fmt::Binary
    + fmt::LowerHex
    + PartialEq
    + PartialOrd
    + ops::AddAssign
    + ops::SubAssign
    + ops::MulAssign
    + ops::DivAssign
    + ops::RemAssign
    + ops::BitAndAssign
    + ops::BitOrAssign
    + ops::BitXorAssign
    + ops::ShlAssign<i32>
    + ops::ShlAssign<u32>
    + ops::ShrAssign<u32>
    + ops::ShrAssign<i32>
    + ops::Add<Output = Self>
    + ops::Sub<Output = Self>
    + ops::Mul<Output = Self>
    + ops::Div<Output = Self>
    + ops::Rem<Output = Self>
    + ops::Shl<i32, Output = Self>
    + ops::Shl<u32, Output = Self>
    + ops::Shr<i32, Output = Self>
    + ops::Shr<u32, Output = Self>
    + ops::BitXor<Output = Self>
    + ops::BitAnd<Output = Self>
    + cmp::Ord
    + From<bool>
    + CastFrom<i32>
    + CastFrom<u16>
    + CastFrom<u32>
    + CastFrom<u8>
    + CastFrom<usize>
    + CastInto<i32>
    + CastInto<u16>
    + CastInto<u32>
    + CastInto<u8>
    + CastInto<usize>
{
    fn signed(self) -> OtherSign<Self::Unsigned>;
    fn unsigned(self) -> Self::Unsigned;
    fn from_unsigned(unsigned: Self::Unsigned) -> Self;
    fn abs(self) -> Self;

    fn from_bool(b: bool) -> Self;

    /// Prevents the need for excessive conversions between signed and unsigned
    fn logical_shr(self, other: u32) -> Self;

    /// Absolute difference between two integers.
    fn abs_diff(self, other: Self) -> Self::Unsigned;

    // copied from primitive integers, but put in a trait
    fn is_zero(self) -> bool;
    fn checked_add(self, other: Self) -> Option<Self>;
    fn checked_sub(self, other: Self) -> Option<Self>;
    fn wrapping_neg(self) -> Self;
    fn wrapping_add(self, other: Self) -> Self;
    fn wrapping_mul(self, other: Self) -> Self;
    fn wrapping_sub(self, other: Self) -> Self;
    fn wrapping_shl(self, other: u32) -> Self;
    fn wrapping_shr(self, other: u32) -> Self;
    fn rotate_left(self, other: u32) -> Self;
    fn overflowing_add(self, other: Self) -> (Self, bool);
    fn overflowing_sub(self, other: Self) -> (Self, bool);
    fn leading_zeros(self) -> u32;
    fn ilog2(self) -> u32;
}

macro_rules! int_impl_common {
    ($ty:ty) => {
        fn from_bool(b: bool) -> Self {
            b as $ty
        }

        fn logical_shr(self, other: u32) -> Self {
            Self::from_unsigned(self.unsigned().wrapping_shr(other))
        }

        fn is_zero(self) -> bool {
            self == Self::ZERO
        }

        fn checked_add(self, other: Self) -> Option<Self> {
            self.checked_add(other)
        }

        fn checked_sub(self, other: Self) -> Option<Self> {
            self.checked_sub(other)
        }

        fn wrapping_neg(self) -> Self {
            <Self>::wrapping_neg(self)
        }

        fn wrapping_add(self, other: Self) -> Self {
            <Self>::wrapping_add(self, other)
        }

        fn wrapping_mul(self, other: Self) -> Self {
            <Self>::wrapping_mul(self, other)
        }

        fn wrapping_sub(self, other: Self) -> Self {
            <Self>::wrapping_sub(self, other)
        }

        fn wrapping_shl(self, other: u32) -> Self {
            <Self>::wrapping_shl(self, other)
        }

        fn wrapping_shr(self, other: u32) -> Self {
            <Self>::wrapping_shr(self, other)
        }

        fn rotate_left(self, other: u32) -> Self {
            <Self>::rotate_left(self, other)
        }

        fn overflowing_add(self, other: Self) -> (Self, bool) {
            <Self>::overflowing_add(self, other)
        }

        fn overflowing_sub(self, other: Self) -> (Self, bool) {
            <Self>::overflowing_sub(self, other)
        }

        fn leading_zeros(self) -> u32 {
            <Self>::leading_zeros(self)
        }

        fn ilog2(self) -> u32 {
            // On our older MSRV, this resolves to the trait method. Which won't actually work,
            // but this is only called behind other gates.
            #[allow(clippy::incompatible_msrv)]
            <Self>::ilog2(self)
        }
    };
}

macro_rules! int_impl {
    ($ity:ty, $uty:ty) => {
        impl MinInt for $uty {
            type OtherSign = $ity;
            type Unsigned = $uty;

            const BITS: u32 = <Self as MinInt>::ZERO.count_zeros();
            const SIGNED: bool = Self::MIN != Self::ZERO;

            const ZERO: Self = 0;
            const ONE: Self = 1;
            const MIN: Self = <Self>::MIN;
            const MAX: Self = <Self>::MAX;
        }

        impl Int for $uty {
            fn signed(self) -> $ity {
                self as $ity
            }

            fn unsigned(self) -> Self {
                self
            }

            fn abs(self) -> Self {
                unimplemented!()
            }

            // It makes writing macros easier if this is implemented for both signed and unsigned
            #[allow(clippy::wrong_self_convention)]
            fn from_unsigned(me: $uty) -> Self {
                me
            }

            fn abs_diff(self, other: Self) -> Self {
                self.abs_diff(other)
            }

            int_impl_common!($uty);
        }

        impl MinInt for $ity {
            type OtherSign = $uty;
            type Unsigned = $uty;

            const BITS: u32 = <Self as MinInt>::ZERO.count_zeros();
            const SIGNED: bool = Self::MIN != Self::ZERO;

            const ZERO: Self = 0;
            const ONE: Self = 1;
            const MIN: Self = <Self>::MIN;
            const MAX: Self = <Self>::MAX;
        }

        impl Int for $ity {
            fn signed(self) -> Self {
                self
            }

            fn unsigned(self) -> $uty {
                self as $uty
            }

            fn abs(self) -> Self {
                self.abs()
            }

            fn from_unsigned(me: $uty) -> Self {
                me as $ity
            }

            fn abs_diff(self, other: Self) -> $uty {
                self.abs_diff(other)
            }

            int_impl_common!($ity);
        }
    };
}

int_impl!(isize, usize);
int_impl!(i8, u8);
int_impl!(i16, u16);
int_impl!(i32, u32);
int_impl!(i64, u64);
int_impl!(i128, u128);

/// Trait for integers twice the bit width of another integer. This is implemented for all
/// primitives except for `u8`, because there is not a smaller primitive.
pub trait DInt: MinInt {
    /// Integer that is half the bit width of the integer this trait is implemented for
    type H: HInt<D = Self>;

    /// Returns the low half of `self`
    fn lo(self) -> Self::H;
    /// Returns the high half of `self`
    fn hi(self) -> Self::H;
    /// Returns the low and high halves of `self` as a tuple
    fn lo_hi(self) -> (Self::H, Self::H) {
        (self.lo(), self.hi())
    }
    /// Constructs an integer using lower and higher half parts
    #[allow(unused)]
    fn from_lo_hi(lo: Self::H, hi: Self::H) -> Self {
        lo.zero_widen() | hi.widen_hi()
    }
}

/// Trait for integers half the bit width of another integer. This is implemented for all
/// primitives except for `u128`, because it there is not a larger primitive.
pub trait HInt: Int {
    /// Integer that is double the bit width of the integer this trait is implemented for
    type D: DInt<H = Self> + MinInt;

    // NB: some of the below methods could have default implementations (e.g. `widen_hi`), but for
    // unknown reasons this can cause infinite recursion when optimizations are disabled. See
    // <https://github.com/rust-lang/compiler-builtins/pull/707> for context.

    /// Widens (using default extension) the integer to have double bit width
    fn widen(self) -> Self::D;
    /// Widens (zero extension only) the integer to have double bit width. This is needed to get
    /// around problems with associated type bounds (such as `Int<Othersign: DInt>`) being unstable
    fn zero_widen(self) -> Self::D;
    /// Widens the integer to have double bit width and shifts the integer into the higher bits
    #[allow(unused)]
    fn widen_hi(self) -> Self::D;
    /// Widening multiplication with zero widening. This cannot overflow.
    fn zero_widen_mul(self, rhs: Self) -> Self::D;
    /// Widening multiplication. This cannot overflow.
    fn widen_mul(self, rhs: Self) -> Self::D;
}

macro_rules! impl_d_int {
    ($($X:ident $D:ident),*) => {
        $(
            impl DInt for $D {
                type H = $X;

                fn lo(self) -> Self::H {
                    self as $X
                }
                fn hi(self) -> Self::H {
                    (self >> <$X as MinInt>::BITS) as $X
                }
            }
        )*
    };
}

macro_rules! impl_h_int {
    ($($H:ident $uH:ident $X:ident),*) => {
        $(
            impl HInt for $H {
                type D = $X;

                fn widen(self) -> Self::D {
                    self as $X
                }
                fn zero_widen(self) -> Self::D {
                    (self as $uH) as $X
                }
                fn zero_widen_mul(self, rhs: Self) -> Self::D {
                    self.zero_widen().wrapping_mul(rhs.zero_widen())
                }
                fn widen_mul(self, rhs: Self) -> Self::D {
                    self.widen().wrapping_mul(rhs.widen())
                }
                fn widen_hi(self) -> Self::D {
                    (self as $X) << <Self as MinInt>::BITS
                }
            }
        )*
    };
}

impl_d_int!(u8 u16, u16 u32, u32 u64, u64 u128, i8 i16, i16 i32, i32 i64, i64 i128);
impl_h_int!(
    u8 u8 u16,
    u16 u16 u32,
    u32 u32 u64,
    u64 u64 u128,
    i8 u8 i16,
    i16 u16 i32,
    i32 u32 i64,
    i64 u64 i128
);

/// Trait to express (possibly lossy) casting of integers
pub trait CastInto<T: Copy>: Copy {
    /// By default, casts should be exact.
    fn cast(self) -> T;

    /// Call for casts that are expected to truncate.
    fn cast_lossy(self) -> T;
}

pub trait CastFrom<T: Copy>: Copy {
    /// By default, casts should be exact.
    fn cast_from(value: T) -> Self;

    /// Call for casts that are expected to truncate.
    fn cast_from_lossy(value: T) -> Self;
}

impl<T: Copy, U: CastInto<T> + Copy> CastFrom<U> for T {
    fn cast_from(value: U) -> Self {
        value.cast()
    }

    fn cast_from_lossy(value: U) -> Self {
        value.cast_lossy()
    }
}

macro_rules! cast_into {
    ($ty:ty) => {
        cast_into!($ty; usize, isize, u8, i8, u16, i16, u32, i32, u64, i64, u128, i128);
    };
    ($ty:ty; $($into:ty),*) => {$(
        impl CastInto<$into> for $ty {
            fn cast(self) -> $into {
                // All we can really do to enforce casting rules is check the rules when in
                // debug mode.
                #[cfg(not(feature = "compiler-builtins"))]
                debug_assert!(<$into>::try_from(self).is_ok(), "failed cast from {self}");
                self as $into
            }

            fn cast_lossy(self) -> $into {
                self as $into
            }
        }
    )*};
}

macro_rules! cast_into_float {
    ($ty:ty) => {
        #[cfg(f16_enabled)]
        cast_into_float!($ty; f16);

        cast_into_float!($ty; f32, f64);

        #[cfg(f128_enabled)]
        cast_into_float!($ty; f128);
    };
    ($ty:ty; $($into:ty),*) => {$(
        impl CastInto<$into> for $ty {
            fn cast(self) -> $into {
                #[cfg(not(feature = "compiler-builtins"))]
                debug_assert_eq!(self as $into as $ty, self, "inexact float cast");
                self as $into
            }

            fn cast_lossy(self) -> $into {
                self as $into
            }
        }
    )*};
}

cast_into!(usize);
cast_into!(isize);
cast_into!(u8);
cast_into!(i8);
cast_into!(u16);
cast_into!(i16);
cast_into!(u32);
cast_into!(i32);
cast_into!(u64);
cast_into!(i64);
cast_into!(u128);
cast_into!(i128);

cast_into_float!(i8);
cast_into_float!(i16);
cast_into_float!(i32);
cast_into_float!(i64);
cast_into_float!(i128);
