/*! `fun`damental `ty`pes

This crate provides trait unification of the Rust fundamental items, allowing
users to declare the behavior they want from a number without committing to a
single particular numeric type.

The number types can be categorized along two axes: behavior and width. Traits
for each axis and group on that axis are provided:

## Numeric Categories

The most general category is represented by the trait [`Numeric`]. It is
implemented by all the numeric fundamentals, and includes only the traits that
they all implement. This is an already-large amount: basic memory management,
comparison, rendering, and numeric arithmetic.

The numbers are then split into [`Floating`] and [`Integral`]. The former fills
out the API of `f32` and `f64`, while the latter covers all of the `iN` and `uN`
numbers.

Lastly, [`Integral`] splits further, into [`Signed`] and [`Unsigned`]. These
provide the last specializations unique to the differences between `iN` and
`uN`.

## Width Categories

Every number implements the trait `IsN` for the `N` of its bit width. `isize`
and `usize` implement the trait that matches their width on the target platform.

In addition, the trait groups `AtLeastN` and `AtMostN` enable clamping the range
of acceptable widths to lower or upper bounds. These traits are equivalent to
`mem::size_of::<T>() >= N` and `mem::size_of::<T>() <= N`, respectively.

[`Floating`]: trait.Floating.html
[`Integral`]: trait.Integral.html
[`Numeric`]: trait.Numeric.html
[`Signed`]: trait.Signed.html
[`Unsigned`]: trait.Unsigned.html
!*/

#![cfg_attr(not(feature = "std"), no_std)]
#![deny(unconditional_recursion)]

use core::{
	convert::{
		TryFrom,
		TryInto,
	},
	fmt::{
		Binary,
		Debug,
		Display,
		LowerExp,
		LowerHex,
		Octal,
		UpperExp,
		UpperHex,
	},
	hash::Hash,
	iter::{
		Product,
		Sum,
	},
	num::{
		FpCategory,
		ParseIntError,
	},
	ops::{
		Add,
		AddAssign,
		BitAnd,
		BitAndAssign,
		BitOr,
		BitOrAssign,
		BitXor,
		BitXorAssign,
		Div,
		DivAssign,
		Mul,
		MulAssign,
		Neg,
		Not,
		Rem,
		RemAssign,
		Shl,
		ShlAssign,
		Shr,
		ShrAssign,
		Sub,
		SubAssign,
	},
	str::FromStr,
};

/// Declare that a type is one of the language fundamental types.
pub trait Fundamental:
	'static
	+ Sized
	+ Send
	+ Sync
	+ Unpin
	+ Clone
	+ Copy
	+ Default
	+ FromStr
	//  cmp
	+ PartialEq<Self>
	+ PartialOrd<Self>
	//  fmt
	+ Debug
	+ Display
	{
		/// Tests `self != 0`.
		fn as_bool(self) -> bool;

		/// Represents `self` as a Unicode Scalar Value, if possible.
		fn as_char(self) -> Option<char>;

		/// Performs `self as i8`.
		fn as_i8(self) -> i8;

		/// Performs `self as i16`.
		fn as_i16(self) -> i16;

		/// Performs `self as i32`.
		fn as_i32(self) -> i32;

		/// Performs `self as i64`.
		fn as_i64(self) -> i64;

		/// Performs `self as i128`.
		fn as_i128(self) -> i128;

		/// Performs `self as isize`.
		fn as_isize(self) -> isize;

		/// Performs `self as u8`.
		fn as_u8(self) -> u8;

		/// Performs `self as u16`.
		fn as_u16(self) -> u16;

		/// Performs `self as u32`.
		fn as_u32(self) -> u32;

		/// Performs `self as u64`.
		fn as_u64(self) -> u64;

		/// Performs `self as u128`.
		fn as_u128(self) -> u128;

		/// Performs `self as usize`.
		fn as_usize(self) -> usize;

		/// Performs `self as f32`.
		fn as_f32(self) -> f32;

		/// Performs `self as f64`.
		fn as_f64(self) -> f64;
	}

/// Declare that a type is an abstract number.
///
/// This unifies all of the signed-integer, unsigned-integer, and floating-point
/// types.
pub trait Numeric:
	Fundamental
	//  iter
	+ Product<Self>
	+ for<'a> Product<&'a Self>
	+ Sum<Self>
	+ for<'a> Sum<&'a Self>
	//  numeric ops
	+ Add<Self, Output = Self>
	+ for<'a> Add<&'a Self, Output = Self>
	+ AddAssign<Self>
	+ for<'a> AddAssign<&'a Self>
	+ Sub<Self, Output = Self>
	+ for<'a> Sub<&'a Self, Output = Self>
	+ SubAssign<Self>
	+ for<'a> SubAssign<&'a Self>
	+ Mul<Self, Output = Self>
	+ for<'a> Mul<&'a Self, Output = Self>
	+ MulAssign<Self>
	+ for<'a> MulAssign<&'a Self>
	+ Div<Self, Output = Self>
	+ for<'a> Div<&'a Self, Output = Self>
	+ DivAssign<Self>
	+ for<'a> DivAssign<&'a Self>
	+ Rem<Self, Output = Self>
	+ for<'a> Rem<&'a Self, Output = Self>
	+ RemAssign<Self>
	+ for<'a> RemAssign<&'a Self>
{
	/// The `[u8; N]` byte array that stores values of `Self`.
	type Bytes;

	/// Return the memory representation of this number as a byte array in
	/// big-endian (network) byte order.
	fn to_be_bytes(self) -> Self::Bytes;

	/// Return the memory representation of this number as a byte array in
	/// little-endian byte order.
	fn to_le_bytes(self) -> Self::Bytes;

	/// Return the memory representation of this number as a byte array in
	/// native byte order.
	fn to_ne_bytes(self) -> Self::Bytes;

	/// Create a numeric value from its representation as a byte array in big
	/// endian.
	fn from_be_bytes(bytes: Self::Bytes) -> Self;

	/// Create a numeric value from its representation as a byte array in little
	/// endian.
	fn from_le_bytes(bytes: Self::Bytes) -> Self;

	/// Create a numeric value from its memory representation as a byte array in
	/// native endianness.
	fn from_ne_bytes(bytes: Self::Bytes) -> Self;
}

/// Declare that a type is a fixed-point integer.
///
/// This unifies all of the signed and unsigned integral types.
pub trait Integral:
	Numeric
	+ Hash
	+ Eq
	+ Ord
	+ Binary
	+ LowerHex
	+ UpperHex
	+ Octal
	+ BitAnd<Self, Output = Self>
	+ for<'a> BitAnd<&'a Self, Output = Self>
	+ BitAndAssign<Self>
	+ for<'a> BitAndAssign<&'a Self>
	+ BitOr<Self, Output = Self>
	+ for<'a> BitOr<&'a Self, Output = Self>
	+ BitOrAssign<Self>
	+ for<'a> BitOrAssign<&'a Self>
	+ BitXor<Self, Output = Self>
	+ for<'a> BitXor<&'a Self, Output = Self>
	+ BitXorAssign<Self>
	+ for<'a> BitXorAssign<&'a Self>
	+ Not<Output = Self>
	+ TryFrom<i8>
	+ TryFrom<u8>
	+ TryFrom<i16>
	+ TryFrom<u16>
	+ TryFrom<i32>
	+ TryFrom<u32>
	+ TryFrom<i64>
	+ TryFrom<u64>
	+ TryFrom<i128>
	+ TryFrom<u128>
	+ TryFrom<isize>
	+ TryFrom<usize>
	+ TryInto<i8>
	+ TryInto<u8>
	+ TryInto<i16>
	+ TryInto<u16>
	+ TryInto<i32>
	+ TryInto<u32>
	+ TryInto<i64>
	+ TryInto<u64>
	+ TryInto<i128>
	+ TryInto<u128>
	+ TryInto<isize>
	+ TryInto<usize>
	+ Shl<Self, Output = Self>
	+ for<'a> Shl<&'a Self, Output = Self>
	+ ShlAssign<Self>
	+ for<'a> ShlAssign<&'a Self>
	+ Shr<Self, Output = Self>
	+ for<'a> Shr<&'a Self, Output = Self>
	+ ShrAssign<Self>
	+ for<'a> ShrAssign<&'a Self>
	+ Shl<i8, Output = Self>
	+ for<'a> Shl<&'a i8, Output = Self>
	+ ShlAssign<i8>
	+ for<'a> ShlAssign<&'a i8>
	+ Shr<i8, Output = Self>
	+ for<'a> Shr<&'a i8, Output = Self>
	+ ShrAssign<i8>
	+ for<'a> ShrAssign<&'a i8>
	+ Shl<u8, Output = Self>
	+ for<'a> Shl<&'a u8, Output = Self>
	+ ShlAssign<u8>
	+ for<'a> ShlAssign<&'a u8>
	+ Shr<u8, Output = Self>
	+ for<'a> Shr<&'a u8, Output = Self>
	+ ShrAssign<u8>
	+ for<'a> ShrAssign<&'a u8>
	+ Shl<i16, Output = Self>
	+ for<'a> Shl<&'a i16, Output = Self>
	+ ShlAssign<i16>
	+ for<'a> ShlAssign<&'a i16>
	+ Shr<i16, Output = Self>
	+ for<'a> Shr<&'a i16, Output = Self>
	+ ShrAssign<i16>
	+ for<'a> ShrAssign<&'a i16>
	+ Shl<u16, Output = Self>
	+ for<'a> Shl<&'a u16, Output = Self>
	+ ShlAssign<u16>
	+ for<'a> ShlAssign<&'a u16>
	+ Shr<u16, Output = Self>
	+ for<'a> Shr<&'a u16, Output = Self>
	+ ShrAssign<u16>
	+ for<'a> ShrAssign<&'a u16>
	+ Shl<i32, Output = Self>
	+ for<'a> Shl<&'a i32, Output = Self>
	+ ShlAssign<i32>
	+ for<'a> ShlAssign<&'a i32>
	+ Shr<i32, Output = Self>
	+ for<'a> Shr<&'a i32, Output = Self>
	+ ShrAssign<i32>
	+ for<'a> ShrAssign<&'a i32>
	+ Shl<u32, Output = Self>
	+ for<'a> Shl<&'a u32, Output = Self>
	+ ShlAssign<u32>
	+ for<'a> ShlAssign<&'a u32>
	+ Shr<u32, Output = Self>
	+ for<'a> Shr<&'a u32, Output = Self>
	+ ShrAssign<u32>
	+ for<'a> ShrAssign<&'a u32>
	+ Shl<i64, Output = Self>
	+ for<'a> Shl<&'a i64, Output = Self>
	+ ShlAssign<i64>
	+ for<'a> ShlAssign<&'a i64>
	+ Shr<i64, Output = Self>
	+ for<'a> Shr<&'a i64, Output = Self>
	+ ShrAssign<i64>
	+ for<'a> ShrAssign<&'a i64>
	+ Shl<u64, Output = Self>
	+ for<'a> Shl<&'a u64, Output = Self>
	+ ShlAssign<u64>
	+ for<'a> ShlAssign<&'a u64>
	+ Shr<u64, Output = Self>
	+ for<'a> Shr<&'a u64, Output = Self>
	+ ShrAssign<u64>
	+ for<'a> ShrAssign<&'a u64>
	+ Shl<i128, Output = Self>
	+ for<'a> Shl<&'a i128, Output = Self>
	+ ShlAssign<i128>
	+ for<'a> ShlAssign<&'a i128>
	+ Shr<i128, Output = Self>
	+ for<'a> Shr<&'a i128, Output = Self>
	+ ShrAssign<i128>
	+ for<'a> ShrAssign<&'a i128>
	+ Shl<u128, Output = Self>
	+ for<'a> Shl<&'a u128, Output = Self>
	+ ShlAssign<u128>
	+ for<'a> ShlAssign<&'a u128>
	+ Shr<u128, Output = Self>
	+ for<'a> Shr<&'a u128, Output = Self>
	+ ShrAssign<u128>
	+ for<'a> ShrAssign<&'a u128>
	+ Shl<isize, Output = Self>
	+ for<'a> Shl<&'a isize, Output = Self>
	+ ShlAssign<isize>
	+ for<'a> ShlAssign<&'a isize>
	+ Shr<isize, Output = Self>
	+ for<'a> Shr<&'a isize, Output = Self>
	+ ShrAssign<isize>
	+ for<'a> ShrAssign<&'a isize>
	+ Shl<usize, Output = Self>
	+ for<'a> Shl<&'a usize, Output = Self>
	+ ShlAssign<usize>
	+ for<'a> ShlAssign<&'a usize>
	+ Shr<usize, Output = Self>
	+ for<'a> Shr<&'a usize, Output = Self>
	+ ShrAssign<usize>
	+ for<'a> ShrAssign<&'a usize>
{
	/// The type’s zero value.
	const ZERO: Self;

	/// The type’s step value.
	const ONE: Self;

	/// The type’s minimum value. This is zero for unsigned integers.
	const MIN: Self;

	/// The type’s maximum value.
	const MAX: Self;

	/// The size of this type in bits.
	const BITS: u32;

	/// Returns the smallest value that can be represented by this integer type.
	fn min_value() -> Self;

	/// Returns the largest value that can be represented by this integer type.
	fn max_value() -> Self;

	/// Converts a string slice in a given base to an integer.
	///
	/// The string is expected to be an optional `+` or `-` sign followed by
	/// digits. Leading and trailing whitespace represent an error. Digits are a
	/// subset of these characters, depending on `radix`:
	///
	/// - `0-9`
	/// - `a-z`
	/// - `A-Z`
	///
	/// # Panics
	///
	/// This function panics if `radix` is not in the range from 2 to 36.
	fn from_str_radix(src: &str, radix: u32) -> Result<Self, ParseIntError>;

	/// Returns the number of ones in the binary representation of `self`.
	fn count_ones(self) -> u32;

	/// Returns the number of zeros in the binary representation of `self`.
	fn count_zeros(self) -> u32;

	/// Returns the number of leading zeros in the binary representation of
	/// `self`.
	fn leading_zeros(self) -> u32;

	/// Returns the number of trailing zeros in the binary representation of
	/// `self`.
	fn trailing_zeros(self) -> u32;

	/// Returns the number of leading ones in the binary representation of
	/// `self`.
	fn leading_ones(self) -> u32;

	/// Returns the number of trailing ones in the binary representation of
	/// `self`.
	fn trailing_ones(self) -> u32;

	/// Shifts the bits to the left by a specified amount, `n`, wrapping the
	/// truncated bits to the end of the resulting integer.
	///
	/// Please note this isn’t the same operation as the `<<` shifting operator!
	fn rotate_left(self, n: u32) -> Self;

	/// Shifts the bits to the right by a specified amount, `n`, wrapping the
	/// truncated bits to the beginning of the resulting integer.
	///
	/// Please note this isn’t the same operation as the `>>` shifting operator!
	fn rotate_right(self, n: u32) -> Self;

	/// Reverses the byte order of the integer.
	fn swap_bytes(self) -> Self;

	/// Reverses the bit pattern of the integer.
	fn reverse_bits(self) -> Self;

	/// Converts an integer from big endian to the target’s endianness.
	///
	/// On big endian this is a no-op. On little endian the bytes are swapped.
	#[allow(clippy::wrong_self_convention)]
	fn from_be(self) -> Self;

	/// Converts an integer frm little endian to the target’s endianness.
	///
	/// On little endian this is a no-op. On big endian the bytes are swapped.
	#[allow(clippy::wrong_self_convention)]
	fn from_le(self) -> Self;

	/// Converts `self` to big endian from the target’s endianness.
	///
	/// On big endian this is a no-op. On little endian the bytes are swapped.
	fn to_be(self) -> Self;

	/// Converts `self` to little endian from the target’s endianness.
	///
	/// On little endian this is a no-op. On big endian the bytes are swapped.
	fn to_le(self) -> Self;

	/// Checked integer addition. Computes `self + rhs`, returning `None` if
	/// overflow occurred.
	fn checked_add(self, rhs: Self) -> Option<Self>;

	/// Checked integer subtraction. Computes `self - rhs`, returning `None` if
	/// overflow occurred.
	fn checked_sub(self, rhs: Self) -> Option<Self>;

	/// Checked integer multiplication. Computes `self * rhs`, returning `None`
	/// if overflow occurred.
	fn checked_mul(self, rhs: Self) -> Option<Self>;

	/// Checked integer division. Computes `self / rhs`, returning `None` if
	/// `rhs == 0` or the division results in overflow.
	fn checked_div(self, rhs: Self) -> Option<Self>;

	/// Checked Euclidean division. Computes `self.div_euclid(rhs)`, returning
	/// `None` if `rhs == 0` or the division results in overflow.
	fn checked_div_euclid(self, rhs: Self) -> Option<Self>;

	/// Checked integer remainder. Computes `self % rhs`, returning `None` if
	/// `rhs == 0` or the division results in overflow.
	fn checked_rem(self, rhs: Self) -> Option<Self>;

	/// Checked Euclidean remainder. Computes `self.rem_euclid(rhs)`, returning
	/// `None` if `rhs == 0` or the division results in overflow.
	fn checked_rem_euclid(self, rhs: Self) -> Option<Self>;

	/// Checked negation. Computes `-self`, returning `None` if `self == MIN`.
	///
	/// Note that negating any positive integer will overflow.
	fn checked_neg(self) -> Option<Self>;

	/// Checked shift left. Computes `self << rhs`, returning `None` if `rhs` is
	/// larger than or equal to the number of bits in `self`.
	fn checked_shl(self, rhs: u32) -> Option<Self>;

	/// Checked shift right. Computes `self >> rhs`, returning `None` if `rhs`
	/// is larger than or equal to the number of bits in `self`.
	fn checked_shr(self, rhs: u32) -> Option<Self>;

	/// Checked exponentiation. Computes `self.pow(exp)`, returning `None` if
	/// overflow occurred.
	fn checked_pow(self, rhs: u32) -> Option<Self>;

	/// Saturating integer addition. Computes `self + rhs`, saturating at the
	/// numeric bounds instead of overflowing.
	fn saturating_add(self, rhs: Self) -> Self;

	/// Saturating integer subtraction. Computes `self - rhs`, saturating at the
	/// numeric bounds instead of overflowing.
	fn saturating_sub(self, rhs: Self) -> Self;

	/// Saturating integer multiplication. Computes `self * rhs`, saturating at
	/// the numeric bounds instead of overflowing.
	fn saturating_mul(self, rhs: Self) -> Self;

	/// Saturating integer exponentiation. Computes `self.pow(exp)`, saturating
	/// at the numeric bounds instead of overflowing.
	fn saturating_pow(self, rhs: u32) -> Self;

	/// Wrapping (modular) addition. Computes `self + rhs`, wrapping around at
	/// the boundary of the type.
	fn wrapping_add(self, rhs: Self) -> Self;

	/// Wrapping (modular) subtraction. Computes `self - rhs`, wrapping around
	/// at the boundary of the type.
	fn wrapping_sub(self, rhs: Self) -> Self;

	/// Wrapping (modular) multiplication. Computes `self * rhs`, wrapping
	/// around at the boundary of the type.
	fn wrapping_mul(self, rhs: Self) -> Self;

	/// Wrapping (modular) division. Computes `self / rhs`, wrapping around at
	/// the boundary of the type.
	///
	/// # Signed Integers
	///
	/// The only case where such wrapping can occur is when one divides
	/// `MIN / -1` on a signed type (where `MIN` is the negative minimal value
	/// for the type); this is equivalent to `-MIN`, a positive value that is
	/// too large to represent in the type. In such a case, this function
	/// returns `MIN` itself.
	///
	/// # Unsigned Integers
	///
	/// Wrapping (modular) division. Computes `self / rhs`. Wrapped division on
	/// unsigned types is just normal division. There’s no way wrapping could
	/// ever happen. This function exists, so that all operations are accounted
	/// for in the wrapping operations.
	///
	/// # Panics
	///
	/// This function will panic if `rhs` is 0.
	fn wrapping_div(self, rhs: Self) -> Self;

	/// Wrapping Euclidean division. Computes `self.div_euclid(rhs)`, wrapping
	/// around at the boundary of the type.
	///
	/// # Signed Types
	///
	/// Wrapping will only occur in `MIN / -1` on a signed type (where `MIN` is
	/// the negative minimal value for the type). This is equivalent to `-MIN`,
	/// a positive value that is too large to represent in the type. In this
	/// case, this method returns `MIN` itself.
	///
	/// # Unsigned Types
	///
	/// Wrapped division on unsigned types is just normal division. There’s no
	/// way wrapping could ever happen. This function exists, so that all
	/// operations are accounted for in the wrapping operations. Since, for the
	/// positive integers, all common definitions of division are equal, this is
	/// exactly equal to `self.wrapping_div(rhs)`.
	///
	/// # Panics
	///
	/// This function will panic if `rhs` is 0.
	fn wrapping_div_euclid(self, rhs: Self) -> Self;

	/// Wrapping (modular) remainder. Computes `self % rhs`, wrapping around at
	/// the boundary of the type.
	///
	/// # Signed Integers
	///
	/// Such wrap-around never actually occurs mathematically; implementation
	/// artifacts make `x % y` invalid for `MIN / -1` on a signed type (where
	/// `MIN` is the negative minimal value). In such a case, this function
	/// returns `0`.
	///
	/// # Unsigned Integers
	///
	/// Wrapped remainder calculation on unsigned types is just the regular
	/// remainder calculation. There’s no way wrapping could ever happen. This
	/// function exists, so that all operations are accounted for in the
	/// wrapping operations.
	///
	/// # Panics
	///
	/// This function will panic if `rhs` is 0.
	fn wrapping_rem(self, rhs: Self) -> Self;

	/// Wrapping Euclidean remainder. Computes `self.rem_euclid(rhs)`, wrapping
	/// around at the boundary of the type.
	///
	/// # Signed Integers
	///
	/// Wrapping will only occur in `MIN % -1` on a signed type (where `MIN` is
	/// the negative minimal value for the type). In this case, this method
	/// returns 0.
	///
	/// # Unsigned Integers
	///
	/// Wrapped modulo calculation on unsigned types is just the regular
	/// remainder calculation. There’s no way wrapping could ever happen. This
	/// function exists, so that all operations are accounted for in the
	/// wrapping operations. Since, for the positive integers, all common
	/// definitions of division are equal, this is exactly equal to
	/// `self.wrapping_rem(rhs)`.
	///
	/// # Panics
	///
	/// This function will panic if `rhs` is 0.
	fn wrapping_rem_euclid(self, rhs: Self) -> Self;

	/// Wrapping (modular) negation. Computes `-self`, wrapping around at the
	/// boundary of the type.
	///
	/// # Signed Integers
	///
	/// The  only case where such wrapping can occur is when one negates `MIN`
	/// on a signed type (where `MIN` is the negative minimal value for the
	/// type); this is a positive value that is too large to represent in the
	/// type. In such a case, this function returns `MIN` itself.
	///
	/// # Unsigned Integers
	///
	/// Since unsigned types do not have negative equivalents all applications
	/// of this function will wrap (except for `-0`). For values smaller than
	/// the corresponding signed type’s maximum the result is the same as
	/// casting the corresponding signed value. Any larger values are equivalent
	/// to `MAX + 1 - (val - MAX - 1)` where `MAX` is the corresponding signed
	/// type’s maximum.
	fn wrapping_neg(self) -> Self;

	/// Panic-free bitwise shift-left; yields `self << mask(rhs)`, where `mask`
	/// removes any high-order bits of `rhs` that would cause the shift to
	/// exceed the bit-width of the type.
	///
	/// Note that this is not the same as a rotate-left; the RHS of a wrapping
	/// shift-left is restricted to the range of the type, rather than the bits
	/// shifted out of the LHS being returned to the other end. The primitive
	/// integer types all implement a `rotate_left` function, which may be what
	/// you want instead.
	fn wrapping_shl(self, rhs: u32) -> Self;

	/// Panic-free bitwise shift-right; yields `self >> mask(rhs)`, where `mask`
	/// removes any high-order bits of `rhs` that would cause the shift to
	/// exceed the bit-width of the type.
	///
	/// Note that this is not the same as a rotate-right; the RHS of a wrapping
	/// shift-right is restricted to the range of the type, rather than the bits
	/// shifted out of the LHS being returned to the other end. The primitive
	/// integer types all implement a `rotate_right` function, which may be what
	/// you want instead.
	fn wrapping_shr(self, rhs: u32) -> Self;

	/// Wrapping (modular) exponentiation. Computes `self.pow(exp)`, wrapping
	/// around at the boundary of the type.
	fn wrapping_pow(self, rhs: u32) -> Self;

	/// Calculates `self + rhs`
	///
	/// Returns a tuple of the addition along with a boolean indicating whether
	/// an arithmetic overflow would occur. If an overflow would have occurred
	/// then the wrapped value is returned.
	fn overflowing_add(self, rhs: Self) -> (Self, bool);

	/// Calculates `self - rhs`
	///
	/// Returns a tuple of the subtraction along with a boolean indicating
	/// whether an arithmetic overflow would occur. If an overflow would have
	/// occurred then the wrapped value is returned.
	fn overflowing_sub(self, rhs: Self) -> (Self, bool);

	/// Calculates the multiplication of `self` and `rhs`.
	///
	/// Returns a tuple of the multiplication along with a boolean indicating
	/// whether an arithmetic overflow would occur. If an overflow would have
	/// occurred then the wrapped value is returned.
	fn overflowing_mul(self, rhs: Self) -> (Self, bool);

	/// Calculates the divisor when `self` is divided by `rhs`.
	///
	/// Returns a tuple of the divisor along with a boolean indicating whether
	/// an arithmetic overflow would occur. If an overflow would occur then self
	/// is returned.
	///
	/// # Panics
	///
	/// This function will panic if `rhs` is 0.
	fn overflowing_div(self, rhs: Self) -> (Self, bool);

	/// Calculates the quotient of Euclidean division `self.div_euclid(rhs)`.
	///
	/// Returns a tuple of the divisor along with a boolean indicating whether
	/// an arithmetic overflow would occur. If an overflow would occur then self
	/// is returned.
	///
	/// # Panics
	///
	/// This function will panic if `rhs` is 0.
	fn overflowing_div_euclid(self, rhs: Self) -> (Self, bool);

	/// Calculates the remainder when `self` is divided by `rhs`.
	///
	/// Returns a tuple of the remainder after dividing along with a boolean
	/// indicating whether an arithmetic overflow would occur. If an overflow
	/// would occur then 0 is returned.
	///
	/// # Panics
	///
	/// This function will panic if `rhs` is 0.
	fn overflowing_rem(self, rhs: Self) -> (Self, bool);

	/// Overflowing Euclidean remainder. Calculates `self.rem_euclid(rhs)`.
	///
	/// Returns a tuple of the remainder after dividing along with a boolean
	/// indicating whether an arithmetic overflow would occur. If an overflow
	/// would occur then 0 is returned.
	///
	/// # Panics
	///
	/// This function will panic if rhs is 0.
	fn overflowing_rem_euclid(self, rhs: Self) -> (Self, bool);

	/// Negates self, overflowing if this is equal to the minimum value.
	///
	/// Returns a tuple of the negated version of self along with a boolean
	/// indicating whether an overflow happened. If `self` is the minimum value
	/// (e.g., `i32::MIN` for values of type `i32`), then the minimum value will
	/// be returned again and `true` will be returned for an overflow happening.
	fn overflowing_neg(self) -> (Self, bool);

	/// Shifts self left by `rhs` bits.
	///
	/// Returns a tuple of the shifted version of self along with a boolean
	/// indicating whether the shift value was larger than or equal to the
	/// number of bits. If the shift value is too large, then value is masked
	/// (N-1) where N is the number of bits, and this value is then used to
	/// perform the shift.
	fn overflowing_shl(self, rhs: u32) -> (Self, bool);

	/// Shifts self right by `rhs` bits.
	///
	/// Returns a tuple of the shifted version of self along with a boolean
	/// indicating whether the shift value was larger than or equal to the
	/// number of bits. If the shift value is too large, then value is masked
	/// (N-1) where N is the number of bits, and this value is then used to
	/// perform the shift.
	fn overflowing_shr(self, rhs: u32) -> (Self, bool);

	/// Raises self to the power of `exp`, using exponentiation by squaring.
	///
	/// Returns a tuple of the exponentiation along with a bool indicating
	/// whether an overflow happened.
	fn overflowing_pow(self, rhs: u32) -> (Self, bool);

	/// Raises self to the power of `exp`, using exponentiation by squaring.
	fn pow(self, rhs: u32) -> Self;

	/// Calculates the quotient of Euclidean division of self by rhs.
	///
	/// This computes the integer `n` such that
	/// `self = n * rhs + self.rem_euclid(rhs)`, with
	/// `0 <= self.rem_euclid(rhs) < rhs`.
	///
	/// In other words, the result is `self / rhs` rounded to the integer `n`
	/// such that `self >= n * rhs`. If `self > 0`, this is equal to round
	/// towards zero (the default in Rust); if `self < 0`, this is equal to
	/// round towards +/- infinity.
	///
	/// # Panics
	///
	/// This function will panic if `rhs` is 0 or the division results in
	/// overflow.
	fn div_euclid(self, rhs: Self) -> Self;

	/// Calculates the least nonnegative remainder of `self (mod rhs)`.
	///
	/// This is done as if by the Euclidean division algorithm -- given
	/// `r = self.rem_euclid(rhs)`, `self = rhs * self.div_euclid(rhs) + r`, and
	/// `0 <= r < abs(rhs)`.
	///
	/// # Panics
	///
	/// This function will panic if `rhs` is 0 or the division results in
	/// overflow.
	fn rem_euclid(self, rhs: Self) -> Self;
}

/// Declare that a type is a signed integer.
pub trait Signed: Integral + Neg {
	/// Checked absolute value. Computes `self.abs()`, returning `None` if
	/// `self == MIN`.
	fn checked_abs(self) -> Option<Self>;

	/// Wrapping (modular) absolute value. Computes `self.abs()`, wrapping
	/// around at the boundary of the type.
	///
	/// The only case where such wrapping can occur is when one takes the
	/// absolute value of the negative minimal value for the type this is a
	/// positive value that is too large to represent in the type. In such a
	/// case, this function returns `MIN` itself.
	fn wrapping_abs(self) -> Self;

	/// Computes the absolute value of `self`.
	///
	/// Returns a tuple of the absolute version of self along with a boolean
	/// indicating whether an overflow happened. If self is the minimum value
	/// (e.g., iN::MIN for values of type iN), then the minimum value will be
	/// returned again and true will be returned for an overflow happening.
	fn overflowing_abs(self) -> (Self, bool);

	//// Computes the absolute value of self.
	///
	/// # Overflow behavior
	///
	/// The absolute value of `iN::min_value()` cannot be represented as an
	/// `iN`, and attempting to calculate it will cause an overflow. This means
	/// that code in debug mode will trigger a panic on this case and optimized
	/// code will return `iN::min_value()` without a panic.
	fn abs(self) -> Self;

	/// Returns a number representing sign of `self`.
	///
	/// - `0` if the number is zero
	/// - `1` if the number is positive
	/// - `-1` if the number is negative
	fn signum(self) -> Self;

	/// Returns `true` if `self` is positive and `false` if the number is zero
	/// or negative.
	fn is_positive(self) -> bool;

	/// Returns `true` if `self` is negative and `false` if the number is zero
	/// or positive.
	fn is_negative(self) -> bool;
}

/// Declare that a type is an unsigned integer.
pub trait Unsigned: Integral {
	/// Returns `true` if and only if `self == 2^k` for some `k`.
	fn is_power_of_two(self) -> bool;

	/// Returns the smallest power of two greater than or equal to `self`.
	///
	/// When return value overflows (i.e., `self > (1 << (N-1))` for type `uN`),
	/// it panics in debug mode and return value is wrapped to 0 in release mode
	/// (the only situation in which method can return 0).
	fn next_power_of_two(self) -> Self;

	/// Returns the smallest power of two greater than or equal to `n`. If the
	/// next power of two is greater than the type’s maximum value, `None` is
	/// returned, otherwise the power of two is wrapped in `Some`.
	fn checked_next_power_of_two(self) -> Option<Self>;
}

/// Declare that a type is a floating-point number.
pub trait Floating:
	Numeric
	+ LowerExp
	+ UpperExp
	+ Neg
	+ From<f32>
	+ From<i8>
	+ From<i16>
	+ From<u8>
	+ From<u16>
{
	/// The unsigned integer type of the same width as `Self`.
	type Raw: Unsigned;

	/// The radix or base of the internal representation of `f32`.
	const RADIX: u32;

	/// Number of significant digits in base 2.
	const MANTISSA_DIGITS: u32;

	/// Approximate number of significant digits in base 10.
	const DIGITS: u32;

	/// [Machine epsilon] value for `f32`.
	///
	/// This is the difference between `1.0` and the next larger representable
	/// number.
	///
	/// [Machine epsilon]: https://en.wikipedia.org/wiki/Machine_epsilon
	const EPSILON: Self;

	/// Smallest finite `f32` value.
	const MIN: Self;

	/// Smallest positive normal `f32` value.
	const MIN_POSITIVE: Self;

	/// Largest finite `f32` value.
	const MAX: Self;

	/// One greater than the minimum possible normal power of 2 exponent.
	const MIN_EXP: i32;

	/// Maximum possible power of 2 exponent.
	const MAX_EXP: i32;

	/// Minimum possible normal power of 10 exponent.
	const MIN_10_EXP: i32;

	/// Maximum possible power of 10 exponent.
	const MAX_10_EXP: i32;

	/// Not a Number (NaN).
	const NAN: Self;

	/// Infinity (∞).
	const INFINITY: Self;

	/// Negative infinity (−∞).
	const NEG_INFINITY: Self;

	/// Archimedes' constant (π)
	const PI: Self;

	/// π/2
	const FRAC_PI_2: Self;

	/// π/3
	const FRAC_PI_3: Self;

	/// π/4
	const FRAC_PI_4: Self;

	/// π/6
	const FRAC_PI_6: Self;

	/// π/8
	const FRAC_PI_8: Self;

	/// 1/π
	const FRAC_1_PI: Self;

	/// 2/π
	const FRAC_2_PI: Self;

	/// 2/sqrt(π)
	const FRAC_2_SQRT_PI: Self;

	/// sqrt(2)
	const SQRT_2: Self;

	/// 1/sqrt(2)
	const FRAC_1_SQRT_2: Self;

	/// Euler’s number (e)
	const E: Self;

	/// log<sub>2</sub>(e)
	const LOG2_E: Self;

	/// log<sub>10</sub>(e)
	const LOG10_E: Self;

	/// ln(2)
	const LN_2: Self;

	/// ln(10)
	const LN_10: Self;

	//  These functions are only available in `std`, because they rely on the
	//  system math library `libm` which is not provided by `core`.

	/// Returns the largest integer less than or equal to a number.
	#[cfg(feature = "std")]
	fn floor(self) -> Self;

	/// Returns the smallest integer greater than or equal to a number.
	#[cfg(feature = "std")]
	fn ceil(self) -> Self;

	/// Returns the nearest integer to a number. Round half-way cases away from
	/// `0.0`.
	#[cfg(feature = "std")]
	fn round(self) -> Self;

	/// Returns the integer part of a number.
	#[cfg(feature = "std")]
	fn trunc(self) -> Self;

	/// Returns the fractional part of a number.
	#[cfg(feature = "std")]
	fn fract(self) -> Self;

	/// Computes the absolute value of `self`. Returns `NAN` if the
	/// number is `NAN`.
	#[cfg(feature = "std")]
	fn abs(self) -> Self;

	/// Returns a number that represents the sign of `self`.
	///
	/// - `1.0` if the number is positive, `+0.0` or `INFINITY`
	/// - `-1.0` if the number is negative, `-0.0` or `NEG_INFINITY`
	/// - `NAN` if the number is `NAN`
	#[cfg(feature = "std")]
	fn signum(self) -> Self;

	/// Returns a number composed of the magnitude of `self` and the sign of
	/// `sign`.
	///
	/// Equal to `self` if the sign of `self` and `sign` are the same, otherwise
	/// equal to `-self`. If `self` is a `NAN`, then a `NAN` with the sign of
	/// `sign` is returned.
	#[cfg(feature = "std")]
	fn copysign(self, sign: Self) -> Self;

	/// Fused multiply-add. Computes `(self * a) + b` with only one rounding
	/// error, yielding a more accurate result than an un-fused multiply-add.
	///
	/// Using `mul_add` can be more performant than an un-fused multiply-add if
	/// the target architecture has a dedicated `fma` CPU instruction.
	#[cfg(feature = "std")]
	fn mul_add(self, a: Self, b: Self) -> Self;

	/// Calculates Euclidean division, the matching method for `rem_euclid`.
	///
	/// This computes the integer `n` such that
	/// `self = n * rhs + self.rem_euclid(rhs)`.
	/// In other words, the result is `self / rhs` rounded to the integer `n`
	/// such that `self >= n * rhs`.
	#[cfg(feature = "std")]
	fn div_euclid(self, rhs: Self) -> Self;

	/// Calculates the least nonnegative remainder of `self (mod rhs)`.
	///
	/// In particular, the return value `r` satisfies `0.0 <= r < rhs.abs()` in
	/// most cases. However, due to a floating point round-off error it can
	/// result in `r == rhs.abs()`, violating the mathematical definition, if
	/// `self` is much smaller than `rhs.abs()` in magnitude and `self < 0.0`.
	/// This result is not an element of the function's codomain, but it is the
	/// closest floating point number in the real numbers and thus fulfills the
	/// property `self == self.div_euclid(rhs) * rhs + self.rem_euclid(rhs)`
	/// approximatively.
	#[cfg(feature = "std")]
	fn rem_euclid(self, rhs: Self) -> Self;

	/// Raises a number to an integer power.
	///
	/// Using this function is generally faster than using `powf`
	#[cfg(feature = "std")]
	fn powi(self, n: i32) -> Self;

	/// Raises a number to a floating point power.
	#[cfg(feature = "std")]
	fn powf(self, n: Self) -> Self;

	/// Returns the square root of a number.
	///
	/// Returns NaN if `self` is a negative number.
	#[cfg(feature = "std")]
	fn sqrt(self) -> Self;

	/// Returns `e^(self)`, (the exponential function).
	#[cfg(feature = "std")]
	fn exp(self) -> Self;

	/// Returns `2^(self)`.
	#[cfg(feature = "std")]
	fn exp2(self) -> Self;

	/// Returns the natural logarithm of the number.
	#[cfg(feature = "std")]
	fn ln(self) -> Self;

	/// Returns the logarithm of the number with respect to an arbitrary base.
	///
	/// The result may not be correctly rounded owing to implementation details;
	/// `self.log2()` can produce more accurate results for base 2, and
	/// `self.log10()` can produce more accurate results for base 10.
	#[cfg(feature = "std")]
	fn log(self, base: Self) -> Self;

	/// Returns the base 2 logarithm of the number.
	#[cfg(feature = "std")]
	fn log2(self) -> Self;

	/// Returns the base 10 logarithm of the number.
	#[cfg(feature = "std")]
	fn log10(self) -> Self;

	/// Returns the cubic root of a number.
	#[cfg(feature = "std")]
	fn cbrt(self) -> Self;

	/// Computes the sine of a number (in radians).
	#[cfg(feature = "std")]
	fn hypot(self, other: Self) -> Self;

	/// Computes the sine of a number (in radians).
	#[cfg(feature = "std")]
	fn sin(self) -> Self;

	/// Computes the cosine of a number (in radians).
	#[cfg(feature = "std")]
	fn cos(self) -> Self;

	/// Computes the tangent of a number (in radians).
	#[cfg(feature = "std")]
	fn tan(self) -> Self;

	/// Computes the arcsine of a number. Return value is in radians in the
	/// range [-pi/2, pi/2] or NaN if the number is outside the range [-1, 1].
	#[cfg(feature = "std")]
	fn asin(self) -> Self;

	/// Computes the arccosine of a number. Return value is in radians in the
	/// range [0, pi] or NaN if the number is outside the range [-1, 1].
	#[cfg(feature = "std")]
	fn acos(self) -> Self;

	/// Computes the arctangent of a number. Return value is in radians in the
	/// range [-pi/2, pi/2];
	#[cfg(feature = "std")]
	fn atan(self) -> Self;

	/// Computes the four quadrant arctangent of `self` (`y`) and `other` (`x`)
	/// in radians.
	///
	/// - `x = 0`, `y = 0`: `0`
	/// - `x >= 0`: `arctan(y/x)` -> `[-pi/2, pi/2]`
	/// - `y >= 0`: `arctan(y/x) + pi` -> `(pi/2, pi]`
	/// - `y < 0`: `arctan(y/x) - pi` -> `(-pi, -pi/2)`
	#[cfg(feature = "std")]
	fn atan2(self, other: Self) -> Self;

	/// Simultaneously computes the sine and cosine of the number, `x`. Returns
	/// `(sin(x), cos(x))`.
	#[cfg(feature = "std")]
	fn sin_cos(self) -> (Self, Self);

	/// Returns `e^(self) - 1` in a way that is accurate even if the number is
	/// close to zero.
	#[cfg(feature = "std")]
	fn exp_m1(self) -> Self;

	/// Returns `ln(1+n)` (natural logarithm) more accurately than if the
	/// operations were performed separately.
	#[cfg(feature = "std")]
	fn ln_1p(self) -> Self;

	/// Hyperbolic sine function.
	#[cfg(feature = "std")]
	fn sinh(self) -> Self;

	/// Hyperbolic cosine function.
	#[cfg(feature = "std")]
	fn cosh(self) -> Self;

	/// Hyperbolic tangent function.
	#[cfg(feature = "std")]
	fn tanh(self) -> Self;

	/// Inverse hyperbolic sine function.
	#[cfg(feature = "std")]
	fn asinh(self) -> Self;

	/// Inverse hyperbolic cosine function.
	#[cfg(feature = "std")]
	fn acosh(self) -> Self;

	/// Inverse hyperbolic tangent function.
	#[cfg(feature = "std")]
	fn atanh(self) -> Self;

	/// Returns `true` if this value is `NaN`.
	fn is_nan(self) -> bool;

	/// Returns `true` if this value is positive infinity or negative infinity,
	/// and `false` otherwise.
	fn is_infinite(self) -> bool;

	/// Returns `true` if this number is neither infinite nor `NaN`.
	fn is_finite(self) -> bool;

	/// Returns `true` if the number is neither zero, infinite, [subnormal], or
	/// `NaN`.
	///
	/// [subnormal]: https://en.wixipedia.org/wiki/Denormal_number
	fn is_normal(self) -> bool;

	/// Returns the floating point category of the number. If only one property
	/// is going to be tested, it is generally faster to use the specific
	/// predicate instead.
	fn classify(self) -> FpCategory;

	/// Returns `true` if `self` has a positive sign, including `+0.0`, `NaN`s
	/// with positive sign bit and positive infinity.
	fn is_sign_positive(self) -> bool;

	/// Returns `true` if `self` has a negative sign, including `-0.0`, `NaN`s
	/// with negative sign bit and negative infinity.
	fn is_sign_negative(self) -> bool;

	/// Takes the reciprocal (inverse) of a number, `1/x`.
	fn recip(self) -> Self;

	/// Converts radians to degrees.
	fn to_degrees(self) -> Self;

	/// Converts degrees to radians.
	fn to_radians(self) -> Self;

	/// Returns the maximum of the two numbers.
	fn max(self, other: Self) -> Self;

	/// Returns the minimum of the two numbers.
	fn min(self, other: Self) -> Self;

	/// Raw transmutation to `u32`.
	///
	/// This is currently identical to `transmute::<f32, u32>(self)` on all
	/// platforms.
	///
	/// See `from_bits` for some discussion of the portability of this operation
	/// (there are almost no issues).
	///
	/// Note that this function is distinct from `as` casting, which attempts to
	/// preserve the *numeric* value, and not the bitwise value.
	fn to_bits(self) -> Self::Raw;

	/// Raw transmutation from `u32`.
	///
	/// This is currently identical to `transmute::<u32, f32>(v)` on all
	/// platforms. It turns out this is incredibly portable, for two reasons:
	///
	/// - Floats and Ints have the same endianness on all supported platforms.
	/// - IEEE-754 very precisely specifies the bit layout of floats.
	///
	/// However there is one caveat: prior to the 2008 version of IEEE-754, how
	/// to interpret the NaN signaling bit wasn't actually specified. Most
	/// platforms (notably x86 and ARM) picked the interpretation that was
	/// ultimately standardized in 2008, but some didn't (notably MIPS). As a
	/// result, all signaling NaNs on MIPS are quiet NaNs on x86, and
	/// vice-versa.
	///
	/// Rather than trying to preserve signaling-ness cross-platform, this
	/// implementation favors preserving the exact bits. This means that
	/// any payloads encoded in NaNs will be preserved even if the result of
	/// this method is sent over the network from an x86 machine to a MIPS one.
	///
	/// If the results of this method are only manipulated by the same
	/// architecture that produced them, then there is no portability concern.
	///
	/// If the input isn't NaN, then there is no portability concern.
	///
	/// If you don't care about signalingness (very likely), then there is no
	/// portability concern.
	///
	/// Note that this function is distinct from `as` casting, which attempts to
	/// preserve the *numeric* value, and not the bitwise value.
	fn from_bits(bits: Self::Raw) -> Self;
}

/// Declare that a type is exactly eight bits wide.
pub trait Is8: Numeric {}

/// Declare that a type is exactly sixteen bits wide.
pub trait Is16: Numeric {}

/// Declare that a type is exactly thirty-two bits wide.
pub trait Is32: Numeric {}

/// Declare that a type is exactly sixty-four bits wide.
pub trait Is64: Numeric {}

/// Declare that a type is exactly one hundred twenty-eight bits wide.
pub trait Is128: Numeric {}

/// Declare that a type is eight or more bits wide.
pub trait AtLeast8: Numeric {}

/// Declare that a type is sixteen or more bits wide.
pub trait AtLeast16: Numeric {}

/// Declare that a type is thirty-two or more bits wide.
pub trait AtLeast32: Numeric {}

/// Declare that a type is sixty-four or more bits wide.
pub trait AtLeast64: Numeric {}

/// Declare that a type is one hundred twenty-eight or more bits wide.
pub trait AtLeast128: Numeric {}

/// Declare that a type is eight or fewer bits wide.
pub trait AtMost8: Numeric {}

/// Declare that a type is sixteen or fewer bits wide.
pub trait AtMost16: Numeric {}

/// Declare that a type is thirty-two or fewer bits wide.
pub trait AtMost32: Numeric {}

/// Declare that a type is sixty-four or fewer bits wide.
pub trait AtMost64: Numeric {}

/// Declare that a type is one hundred twenty-eight or fewer bits wide.
pub trait AtMost128: Numeric {}

/// Creates new wrapper functions that forward to inherent items of the same
/// name and signature.
macro_rules! func {
	(
		$(@$std:literal)?
		$name:ident (self$(, $arg:ident: $t:ty)*) $(-> $ret:ty)?;
		$($tt:tt)*
	) => {
		$(#[cfg(feature = $std)])?
		fn $name(self$(, $arg: $t)*) $(-> $ret)?
		{
			<Self>::$name(self$(, $arg)*)
		}

		func!($($tt)*);
	};
	(
		$(@$std:literal)?
		$name:ident(&self$(, $arg:ident: $t:ty)*) $(-> $ret:ty)?;
		$($tt:tt)*
	) => {
		$(#[cfg(feature = $std)])?
		fn $name(&self$(, $arg: $t)*) $(-> $ret)?
		{
			<Self>::$name(&self$(, $arg )*)
		}

		func!($($tt)*);
	};
	(
		$(@$std:literal)?
		$name:ident(&mut self$(, $arg:ident: $t:ty)*) $(-> $ret:ty)?;
		$($tt:tt)*
	) => {
		$(#[cfg(feature = $std)])?
		fn $name(&mut self$(, $arg: $t)*) $(-> $ret)?
		{
			<Self>::$name(&mut self$(, $arg)*)
		}

		func!($($tt)*);
	};
	(
		$(@$std:literal)?
		$name:ident($($arg:ident: $t:ty),* $(,)?) $(-> $ret:ty)?;
		$($tt:tt)*
	) => {
		$(#[cfg(feature = $std)])?
		fn $name($($arg: $t),*) $(-> $ret)?
		{
			<Self>::$name($($arg),*)
		}

		func!($($tt)*);
	};
	() => {};
}

macro_rules! impl_for {
	( Fundamental => $($t:ty => $is_zero:expr),+ $(,)? ) => { $(
		impl Fundamental for $t {
			#[inline(always)]
			#[allow(clippy::redundant_closure_call)]
			fn as_bool(self) -> bool { ($is_zero)(self) }

			#[inline(always)]
			fn as_char(self) -> Option<char> {
				core::char::from_u32(self as u32)
			}

			#[inline(always)]
			fn as_i8(self) -> i8 { self as i8 }

			#[inline(always)]
			fn as_i16(self) -> i16 { self as i16 }

			#[inline(always)]
			fn as_i32(self) -> i32 { self as i32 }

			#[inline(always)]
			fn as_i64(self) -> i64 { self as i64 }

			#[inline(always)]
			fn as_i128(self) -> i128 { self as i128 }

			#[inline(always)]
			fn as_isize(self) -> isize { self as isize }

			#[inline(always)]
			fn as_u8(self) -> u8 { self as u8 }

			#[inline(always)]
			fn as_u16(self) -> u16 { self as u16 }

			#[inline(always)]
			fn as_u32(self) -> u32 { self as u32 }

			#[inline(always)]
			fn as_u64(self) -> u64 { self as u64 }

			#[inline(always)]
			fn as_u128(self) ->u128 { self as u128 }

			#[inline(always)]
			fn as_usize(self) -> usize { self as usize }

			#[inline(always)]
			fn as_f32(self) -> f32 { self as u32 as f32 }

			#[inline(always)]
			fn as_f64(self) -> f64 { self as u64 as f64 }
		}
	)+ };
	( Numeric => $($t:ty),+ $(,)? ) => { $(
		impl Numeric for $t {
			type Bytes = [u8; core::mem::size_of::<Self>()];

			func! {
				to_be_bytes(self) -> Self::Bytes;
				to_le_bytes(self) -> Self::Bytes;
				to_ne_bytes(self) -> Self::Bytes;
				from_be_bytes(bytes: Self::Bytes) -> Self;
				from_le_bytes(bytes: Self::Bytes) -> Self;
				from_ne_bytes(bytes: Self::Bytes) -> Self;
			}
		}
	)+ };
	( Integral => $($t:ty),+ $(,)? ) => { $(
		impl Integral for $t {
			const ZERO: Self = 0;
			const ONE: Self = 1;
			const MIN: Self = <Self>::min_value();
			const MAX: Self = <Self>::max_value();

			const BITS: u32 = <Self>::BITS;

			func! {
				min_value() -> Self;
				max_value() -> Self;
				from_str_radix(src: &str, radix: u32) -> Result<Self, ParseIntError>;
				count_ones(self) -> u32;
				count_zeros(self) -> u32;
				leading_zeros(self) -> u32;
				trailing_zeros(self) -> u32;
				leading_ones(self) -> u32;
				trailing_ones(self) -> u32;
				rotate_left(self, n: u32) -> Self;
				rotate_right(self, n: u32) -> Self;
				swap_bytes(self) -> Self;
				reverse_bits(self) -> Self;
				from_be(self) -> Self;
				from_le(self) -> Self;
				to_be(self) -> Self;
				to_le(self) -> Self;
				checked_add(self, rhs: Self) -> Option<Self>;
				checked_sub(self, rhs: Self) -> Option<Self>;
				checked_mul(self, rhs: Self) -> Option<Self>;
				checked_div(self, rhs: Self) -> Option<Self>;
				checked_div_euclid(self, rhs: Self) -> Option<Self>;
				checked_rem(self, rhs: Self) -> Option<Self>;
				checked_rem_euclid(self, rhs: Self) -> Option<Self>;
				checked_neg(self) -> Option<Self>;
				checked_shl(self, rhs: u32) -> Option<Self>;
				checked_shr(self, rhs: u32) -> Option<Self>;
				checked_pow(self, rhs: u32) -> Option<Self>;
				saturating_add(self, rhs: Self) -> Self;
				saturating_sub(self, rhs: Self) -> Self;
				saturating_mul(self, rhs: Self) -> Self;
				saturating_pow(self, rhs: u32) -> Self;
				wrapping_add(self, rhs: Self) -> Self;
				wrapping_sub(self, rhs: Self) -> Self;
				wrapping_mul(self, rhs: Self) -> Self;
				wrapping_div(self, rhs: Self) -> Self;
				wrapping_div_euclid(self, rhs: Self) -> Self;
				wrapping_rem(self, rhs: Self) -> Self;
				wrapping_rem_euclid(self, rhs: Self) -> Self;
				wrapping_neg(self) -> Self;
				wrapping_shl(self, rhs: u32) -> Self;
				wrapping_shr(self, rhs: u32) -> Self;
				wrapping_pow(self, rhs: u32) -> Self;
				overflowing_add(self, rhs: Self) -> (Self, bool);
				overflowing_sub(self, rhs: Self) -> (Self, bool);
				overflowing_mul(self, rhs: Self) -> (Self, bool);
				overflowing_div(self, rhs: Self) -> (Self, bool);
				overflowing_div_euclid(self, rhs: Self) -> (Self, bool);
				overflowing_rem(self, rhs: Self) -> (Self, bool);
				overflowing_rem_euclid(self, rhs: Self) -> (Self, bool);
				overflowing_neg(self) -> (Self, bool);
				overflowing_shl(self, rhs: u32) -> (Self, bool);
				overflowing_shr(self, rhs: u32) -> (Self, bool);
				overflowing_pow(self, rhs: u32) -> (Self, bool);
				pow(self, rhs: u32) -> Self;
				div_euclid(self, rhs: Self) -> Self;
				rem_euclid(self, rhs: Self) -> Self;
			}
		}
	)+ };
	( Signed => $($t:ty),+ $(,)? ) => { $(
		impl Signed for $t {
			func! {
				checked_abs(self) -> Option<Self>;
				wrapping_abs(self) -> Self;
				overflowing_abs(self) -> (Self, bool);
				abs(self) -> Self;
				signum(self) -> Self;
				is_positive(self) -> bool;
				is_negative(self) -> bool;
			}
		}
	)+ };
	( Unsigned => $($t:ty),+ $(,)? ) => { $(
		impl Unsigned for $t {
			func! {
				is_power_of_two(self) -> bool;
				next_power_of_two(self) -> Self;
				checked_next_power_of_two(self) -> Option<Self>;
			}
		}
	)+ };
	( Floating => $($t:ident | $u:ty),+ $(,)? ) => { $(
		impl Floating for $t {
			type Raw = $u;

			const RADIX: u32 = core::$t::RADIX;
			const MANTISSA_DIGITS: u32 = core::$t::MANTISSA_DIGITS;
			const DIGITS: u32 = core::$t::DIGITS;
			const EPSILON: Self = core::$t::EPSILON;
			const MIN: Self = core::$t::MIN;
			const MIN_POSITIVE: Self = core::$t::MIN_POSITIVE;
			const MAX: Self = core::$t::MAX;
			const MIN_EXP: i32 = core::$t::MIN_EXP;
			const MAX_EXP: i32 = core::$t::MAX_EXP;
			const MIN_10_EXP: i32 = core::$t::MIN_10_EXP;
			const MAX_10_EXP: i32 = core::$t::MAX_10_EXP;
			const NAN: Self = core::$t::NAN;
			const INFINITY: Self = core::$t::INFINITY;
			const NEG_INFINITY: Self = core::$t::NEG_INFINITY;

			const PI: Self = core::$t::consts::PI;
			const FRAC_PI_2: Self = core::$t::consts::FRAC_PI_2;
			const FRAC_PI_3: Self = core::$t::consts::FRAC_PI_3;
			const FRAC_PI_4: Self = core::$t::consts::FRAC_PI_4;
			const FRAC_PI_6: Self = core::$t::consts::FRAC_PI_6;
			const FRAC_PI_8: Self = core::$t::consts::FRAC_PI_8;
			const FRAC_1_PI: Self = core::$t::consts::FRAC_1_PI;
			const FRAC_2_PI: Self = core::$t::consts::FRAC_2_PI;
			const FRAC_2_SQRT_PI: Self = core::$t::consts::FRAC_2_SQRT_PI;
			const SQRT_2: Self = core::$t::consts::SQRT_2;
			const FRAC_1_SQRT_2: Self = core::$t::consts::FRAC_1_SQRT_2;
			const E: Self = core::$t::consts::E;
			const LOG2_E: Self = core::$t::consts::LOG2_E;
			const LOG10_E: Self = core::$t::consts::LOG10_E;
			const LN_2: Self = core::$t::consts::LN_2;
			const LN_10: Self = core::$t::consts::LN_10;

			func! {
				@"std" floor(self) -> Self;
				@"std" ceil(self) -> Self;
				@"std" round(self) -> Self;
				@"std" trunc(self) -> Self;
				@"std" fract(self) -> Self;
				@"std" abs(self) -> Self;
				@"std" signum(self) -> Self;
				@"std" copysign(self, sign: Self) -> Self;
				@"std" mul_add(self, a: Self, b: Self) -> Self;
				@"std" div_euclid(self, rhs: Self) -> Self;
				@"std" rem_euclid(self, rhs: Self) -> Self;
				@"std" powi(self, n: i32) -> Self;
				@"std" powf(self, n: Self) -> Self;
				@"std" sqrt(self) -> Self;
				@"std" exp(self) -> Self;
				@"std" exp2(self) -> Self;
				@"std" ln(self) -> Self;
				@"std" log(self, base: Self) -> Self;
				@"std" log2(self) -> Self;
				@"std" log10(self) -> Self;
				@"std" cbrt(self) -> Self;
				@"std" hypot(self, other: Self) -> Self;
				@"std" sin(self) -> Self;
				@"std" cos(self) -> Self;
				@"std" tan(self) -> Self;
				@"std" asin(self) -> Self;
				@"std" acos(self) -> Self;
				@"std" atan(self) -> Self;
				@"std" atan2(self, other: Self) -> Self;
				@"std" sin_cos(self) -> (Self, Self);
				@"std" exp_m1(self) -> Self;
				@"std" ln_1p(self) -> Self;
				@"std" sinh(self) -> Self;
				@"std" cosh(self) -> Self;
				@"std" tanh(self) -> Self;
				@"std" asinh(self) -> Self;
				@"std" acosh(self) -> Self;
				@"std" atanh(self) -> Self;
				is_nan(self) -> bool;
				is_infinite(self) -> bool;
				is_finite(self) -> bool;
				is_normal(self) -> bool;
				classify(self) -> FpCategory;
				is_sign_positive(self) -> bool;
				is_sign_negative(self) -> bool;
				recip(self) -> Self;
				to_degrees(self) -> Self;
				to_radians(self) -> Self;
				max(self, other: Self) -> Self;
				min(self, other: Self) -> Self;
				to_bits(self) -> Self::Raw;
				from_bits(bits: Self::Raw) -> Self;
			}
		}
	)+ };
	( $which:ty => $($t:ty),+ $(,)? ) => { $(
		impl $which for $t {}
	)+ };
}

impl_for!(Fundamental =>
	bool => |this: bool| !this,
	char => |this| this != '\0',
	i8 => |this| this != 0,
	i16 => |this| this != 0,
	i32 => |this| this != 0,
	i64 => |this| this != 0,
	i128 => |this| this != 0,
	isize => |this| this != 0,
	u8 => |this| this != 0,
	u16 => |this| this != 0,
	u32 => |this| this != 0,
	u64 => |this| this != 0,
	u128 => |this| this != 0,
	usize => |this| this != 0,
	f32 => |this: f32| (-Self::EPSILON ..= Self::EPSILON).contains(&this),
	f64 => |this: f64| (-Self::EPSILON ..= Self::EPSILON).contains(&this),
);
impl_for!(Numeric => i8, i16, i32, i64, i128, isize, u8, u16, u32, u64, u128, usize, f32, f64);
impl_for!(Integral => i8, i16, i32, i64, i128, isize, u8, u16, u32, u64, u128, usize);
impl_for!(Signed => i8, i16, i32, i64, i128, isize);
impl_for!(Unsigned => u8, u16, u32, u64, u128, usize);
impl_for!(Floating => f32 | u32, f64 | u64);

impl_for!(Is8 => i8, u8);
impl_for!(Is16 => i16, u16);
impl_for!(Is32 => i32, u32, f32);
impl_for!(Is64 => i64, u64, f64);
impl_for!(Is128 => i128, u128);

#[cfg(target_pointer_width = "16")]
impl_for!(Is16 => isize, usize);

#[cfg(target_pointer_width = "32")]
impl_for!(Is32 => isize, usize);

#[cfg(target_pointer_width = "64")]
impl_for!(Is64 => isize, usize);

impl_for!(AtLeast8 => i8, i16, i32, i64, i128, isize, u8, u16, u32, u64, u128, usize, f32, f64);
impl_for!(AtLeast16 => i16, i32, i64, i128, u16, u32, u64, u128, f32, f64);
impl_for!(AtLeast32 => i32, i64, i128, u32, u64, u128, f32, f64);
impl_for!(AtLeast64 => i64, i128, u64, u128, f64);
impl_for!(AtLeast128 => i128, u128);

#[cfg(any(
	target_pointer_width = "16",
	target_pointer_width = "32",
	target_pointer_width = "64"
))]
impl_for!(AtLeast16 => isize, usize);

#[cfg(any(target_pointer_width = "32", target_pointer_width = "64"))]
impl_for!(AtLeast32 => isize, usize);

#[cfg(target_pointer_width = "64")]
impl_for!(AtLeast64 => isize, usize);

impl_for!(AtMost8 => i8, u8);
impl_for!(AtMost16 => i8, i16, u8, u16);
impl_for!(AtMost32 => i8, i16, i32, u8, u16, u32, f32);
impl_for!(AtMost64 => i8, i16, i32, i64, isize, u8, u16, u32, u64, usize, f32, f64);
impl_for!(AtMost128 => i8, i16, i32, i64, i128, isize, u8, u16, u32, u64, u128, usize, f32, f64);

#[cfg(target_pointer_width = "16")]
impl_for!(AtMost16 => isize, usize);

#[cfg(any(target_pointer_width = "16", target_pointer_width = "32"))]
impl_for!(AtMost32 => isize, usize);

#[cfg(test)]
mod tests {
	use super::*;
	use static_assertions::*;

	assert_impl_all!(bool: Fundamental);
	assert_impl_all!(char: Fundamental);

	assert_impl_all!(i8: Integral, Signed, Is8);
	assert_impl_all!(i16: Integral, Signed, Is16);
	assert_impl_all!(i32: Integral, Signed, Is32);
	assert_impl_all!(i64: Integral, Signed, Is64);
	assert_impl_all!(i128: Integral, Signed, Is128);
	assert_impl_all!(isize: Integral, Signed);

	assert_impl_all!(u8: Integral, Unsigned, Is8);
	assert_impl_all!(u16: Integral, Unsigned, Is16);
	assert_impl_all!(u32: Integral, Unsigned, Is32);
	assert_impl_all!(u64: Integral, Unsigned, Is64);
	assert_impl_all!(u128: Integral, Unsigned, Is128);
	assert_impl_all!(usize: Integral, Unsigned);

	assert_impl_all!(f32: Floating, Is32);
	assert_impl_all!(f64: Floating, Is64);
}
