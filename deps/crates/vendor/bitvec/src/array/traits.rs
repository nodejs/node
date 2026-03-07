//! Additional trait implementations on bit-arrays.

use core::{
	borrow::{
		Borrow,
		BorrowMut,
	},
	cmp,
	convert::TryFrom,
	fmt::{
		self,
		Debug,
		Display,
		Formatter,
	},
	hash::{
		Hash,
		Hasher,
	},
	marker::Unpin,
};

use tap::TryConv;

use super::BitArray;
use crate::{
	index::BitIdx,
	mem,
	order::BitOrder,
	slice::BitSlice,
	store::BitStore,
	view::BitViewSized,
};

#[cfg(not(tarpaulin_include))]
impl<A, O> Borrow<BitSlice<A::Store, O>> for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn borrow(&self) -> &BitSlice<A::Store, O> {
		self.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<A, O> BorrowMut<BitSlice<A::Store, O>> for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn borrow_mut(&mut self) -> &mut BitSlice<A::Store, O> {
		self.as_mut_bitslice()
	}
}

impl<A, O> Clone for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn clone(&self) -> Self {
		let mut out = Self::ZERO;
		for (dst, src) in
			out.as_raw_mut_slice().iter_mut().zip(self.as_raw_slice())
		{
			dst.store_value(src.load_value());
		}
		out
	}
}

impl<A, O> Eq for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
}

#[cfg(not(tarpaulin_include))]
impl<A, O> Ord for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn cmp(&self, other: &Self) -> cmp::Ordering {
		self.as_bitslice().cmp(other.as_bitslice())
	}
}

#[cfg(not(tarpaulin_include))]
impl<O1, A, O2, T> PartialEq<BitArray<A, O2>> for BitSlice<T, O1>
where
	O1: BitOrder,
	O2: BitOrder,
	A: BitViewSized,
	T: BitStore,
{
	#[inline]
	fn eq(&self, other: &BitArray<A, O2>) -> bool {
		self == other.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<A, O, Rhs> PartialEq<Rhs> for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
	Rhs: ?Sized,
	BitSlice<A::Store, O>: PartialEq<Rhs>,
{
	#[inline]
	fn eq(&self, other: &Rhs) -> bool {
		self.as_bitslice() == other
	}
}

#[cfg(not(tarpaulin_include))]
impl<A, T, O> PartialOrd<BitArray<A, O>> for BitSlice<T, O>
where
	A: BitViewSized,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, other: &BitArray<A, O>) -> Option<cmp::Ordering> {
		self.partial_cmp(other.as_bitslice())
	}
}

#[cfg(not(tarpaulin_include))]
impl<A, O, Rhs> PartialOrd<Rhs> for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
	Rhs: ?Sized,
	BitSlice<A::Store, O>: PartialOrd<Rhs>,
{
	#[inline]
	fn partial_cmp(&self, other: &Rhs) -> Option<cmp::Ordering> {
		self.as_bitslice().partial_cmp(other)
	}
}

#[cfg(not(tarpaulin_include))]
impl<A, O> AsRef<BitSlice<A::Store, O>> for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn as_ref(&self) -> &BitSlice<A::Store, O> {
		self.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<A, O> AsMut<BitSlice<A::Store, O>> for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn as_mut(&mut self) -> &mut BitSlice<A::Store, O> {
		self.as_mut_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<A, O> From<A> for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn from(data: A) -> Self {
		Self::new(data)
	}
}

impl<A, O> TryFrom<&BitSlice<A::Store, O>> for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	type Error = TryFromBitSliceError;

	#[inline]
	fn try_from(src: &BitSlice<A::Store, O>) -> Result<Self, Self::Error> {
		src.try_conv::<&Self>().map(|this| this.clone())
	}
}

impl<A, O> TryFrom<&BitSlice<A::Store, O>> for &BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	type Error = TryFromBitSliceError;

	#[inline]
	fn try_from(src: &BitSlice<A::Store, O>) -> Result<Self, Self::Error> {
		TryFromBitSliceError::new::<A, O>(src).map(|()| unsafe {
			&*src
				.as_bitspan()
				.address()
				.to_const()
				.cast::<BitArray<A, O>>()
		})
	}
}

impl<A, O> TryFrom<&mut BitSlice<A::Store, O>> for &mut BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	type Error = TryFromBitSliceError;

	#[inline]
	fn try_from(src: &mut BitSlice<A::Store, O>) -> Result<Self, Self::Error> {
		TryFromBitSliceError::new::<A, O>(src).map(|()| unsafe {
			&mut *src
				.as_mut_bitspan()
				.address()
				.to_mut()
				.cast::<BitArray<A, O>>()
		})
	}
}

impl<A, O> Default for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn default() -> Self {
		Self::ZERO
	}
}

impl<A, O> Debug for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		self.as_bitspan().render(fmt, "Array", None)?;
		fmt.write_str(" ")?;
		Display::fmt(self, fmt)
	}
}

easy_fmt! {
	impl Binary
	impl Display
	impl LowerHex
	impl Octal
	impl UpperHex
	for BitArray
}

#[cfg(not(tarpaulin_include))]
impl<A, O> Hash for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn hash<H>(&self, hasher: &mut H)
	where H: Hasher {
		self.as_bitslice().hash(hasher);
	}
}

impl<A, O> Copy for BitArray<A, O>
where
	O: BitOrder,
	A: BitViewSized + Copy,
{
}

impl<A, O> Unpin for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
}

#[repr(transparent)]
#[derive(Clone, Copy, Eq, Ord, PartialEq, PartialOrd)]
#[doc = include_str!("../../doc/array/TryFromBitSliceError.md")]
pub struct TryFromBitSliceError(InnerError);

impl TryFromBitSliceError {
	/// Checks whether a bit-slice can be viewed as a bit-array.
	#[inline]
	fn new<A, O>(bits: &BitSlice<A::Store, O>) -> Result<(), Self>
	where
		O: BitOrder,
		A: BitViewSized,
	{
		InnerError::new::<A, O>(bits).map_err(Self)
	}
}

impl Debug for TryFromBitSliceError {
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.write_str("TryFromBitSliceError::")?;
		match self.0 {
			InnerError::UnequalLen { actual, expected } => {
				write!(fmt, "UnequalLen({} != {})", actual, expected)
			},
			InnerError::Misaligned => fmt.write_str("Misaligned"),
		}
	}
}

#[cfg(not(tarpaulin_include))]
impl Display for TryFromBitSliceError {
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		match self.0 {
			InnerError::UnequalLen { actual, expected } => write!(
				fmt,
				"bit-slice with length {} cannot be viewed as bit-array with \
				 length {}",
				actual, expected,
			),
			InnerError::Misaligned => fmt.write_str(
				"a bit-slice must begin at the front edge of a storage element \
				 in order to be viewed as a bit-array",
			),
		}
	}
}

#[cfg(feature = "std")]
impl std::error::Error for TryFromBitSliceError {}

/// Opaque error type for bit-slice to bit-array view conversions.
#[derive(Clone, Copy, Eq, Hash, Ord, PartialEq, PartialOrd)]
enum InnerError {
	/// A bit-slice did not match the length of the destination bit-array.
	UnequalLen {
		/// The length of the bit-slice that produced this error.
		actual:   usize,
		/// The length of the destination bit-array type.
		expected: usize,
	},
	/// A bit-slice did not begin at `BitIdx::MIN`.
	Misaligned,
}

impl InnerError {
	/// Checks whether a bit-slice is suitable to view as a bit-array.
	#[inline]
	fn new<A, O>(bits: &BitSlice<A::Store, O>) -> Result<(), Self>
	where
		O: BitOrder,
		A: BitViewSized,
	{
		let bitspan = bits.as_bitspan();
		let actual = bitspan.len();
		let expected = mem::bits_of::<A>();
		if actual != expected {
			return Err(Self::UnequalLen { actual, expected });
		}
		if bitspan.head() != BitIdx::<<A::Store as BitStore>::Mem>::MIN {
			return Err(Self::Misaligned);
		}
		Ok(())
	}
}
