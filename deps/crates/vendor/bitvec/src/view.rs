#![doc = include_str!("../doc/view.md")]

use core::slice;

use crate::{
	array::BitArray,
	order::BitOrder,
	ptr::BitSpanError,
	slice::BitSlice,
	store::BitStore,
};

#[doc = include_str!("../doc/view/BitView.md")]
pub trait BitView {
	/// The underlying element type.
	type Store: BitStore;

	/// Views a memory region as an immutable bit-slice.
	fn view_bits<O>(&self) -> &BitSlice<Self::Store, O>
	where O: BitOrder;

	/// Attempts to view a memory region as an immutable bit-slice.
	///
	/// This may return an error if `self` is too long to view as a bit-slice.
	fn try_view_bits<O>(
		&self,
	) -> Result<&BitSlice<Self::Store, O>, BitSpanError<Self::Store>>
	where O: BitOrder;

	/// Views a memory region as a mutable bit-slice.
	fn view_bits_mut<O>(&mut self) -> &mut BitSlice<Self::Store, O>
	where O: BitOrder;

	/// Attempts to view a memory region as a mutable bit-slice.
	///
	/// This may return an error if `self` is too long to view as a bit-slice.
	fn try_view_bits_mut<O>(
		&mut self,
	) -> Result<&mut BitSlice<Self::Store, O>, BitSpanError<Self::Store>>
	where O: BitOrder;
}

#[cfg(not(tarpaulin_include))]
impl<T> BitView for T
where T: BitStore
{
	type Store = Self;

	fn view_bits<O>(&self) -> &BitSlice<T, O>
	where O: BitOrder {
		BitSlice::from_element(self)
	}

	fn try_view_bits<O>(&self) -> Result<&BitSlice<T, O>, BitSpanError<T>>
	where O: BitOrder {
		Ok(BitSlice::from_element(self))
	}

	fn view_bits_mut<O>(&mut self) -> &mut BitSlice<T, O>
	where O: BitOrder {
		BitSlice::from_element_mut(self)
	}

	fn try_view_bits_mut<O>(
		&mut self,
	) -> Result<&mut BitSlice<T, O>, BitSpanError<T>>
	where O: BitOrder {
		Ok(BitSlice::from_element_mut(self))
	}
}

/// Note that overly-large slices may cause the conversions to fail.
#[cfg(not(tarpaulin_include))]
impl<T> BitView for [T]
where T: BitStore
{
	type Store = T;

	#[inline]
	fn view_bits<O>(&self) -> &BitSlice<T, O>
	where O: BitOrder {
		BitSlice::from_slice(self)
	}

	#[inline]
	fn try_view_bits<O>(&self) -> Result<&BitSlice<T, O>, BitSpanError<T>>
	where O: BitOrder {
		BitSlice::try_from_slice(self)
	}

	#[inline]
	fn view_bits_mut<O>(&mut self) -> &mut BitSlice<T, O>
	where O: BitOrder {
		BitSlice::from_slice_mut(self)
	}

	#[inline]
	fn try_view_bits_mut<O>(
		&mut self,
	) -> Result<&mut BitSlice<T, O>, BitSpanError<T>>
	where O: BitOrder {
		BitSlice::try_from_slice_mut(self)
	}
}

/// Note that overly-large arrays may cause the conversions to fail.
#[cfg(not(tarpaulin_include))]
impl<T, const N: usize> BitView for [T; N]
where T: BitStore
{
	type Store = T;

	#[inline]
	fn view_bits<O>(&self) -> &BitSlice<T, O>
	where O: BitOrder {
		BitSlice::from_slice(self)
	}

	#[inline]
	fn try_view_bits<O>(&self) -> Result<&BitSlice<T, O>, BitSpanError<T>>
	where O: BitOrder {
		BitSlice::try_from_slice(self)
	}

	#[inline]
	fn view_bits_mut<O>(&mut self) -> &mut BitSlice<T, O>
	where O: BitOrder {
		BitSlice::from_slice_mut(self)
	}

	#[inline]
	fn try_view_bits_mut<O>(
		&mut self,
	) -> Result<&mut BitSlice<T, O>, BitSpanError<T>>
	where O: BitOrder {
		BitSlice::try_from_slice_mut(self)
	}
}

/// Helper trait for scalars and arrays, but not slices.
pub trait BitViewSized: BitView + Sized {
	/// The zero constant.
	const ZERO: Self;

	/// Wraps `self` in a `BitArray`.
	#[inline]
	fn into_bitarray<O>(self) -> BitArray<Self, O>
	where O: BitOrder {
		BitArray::new(self)
	}

	/// Views the type as a slice of its elements.
	fn as_raw_slice(&self) -> &[Self::Store];

	/// Views the type as a mutable slice of its elements.
	fn as_raw_mut_slice(&mut self) -> &mut [Self::Store];
}

impl<T> BitViewSized for T
where T: BitStore
{
	const ZERO: Self = <T as BitStore>::ZERO;

	#[inline]
	fn as_raw_slice(&self) -> &[Self::Store] {
		slice::from_ref(self)
	}

	#[inline]
	fn as_raw_mut_slice(&mut self) -> &mut [Self::Store] {
		slice::from_mut(self)
	}
}

impl<T, const N: usize> BitViewSized for [T; N]
where T: BitStore
{
	const ZERO: Self = [T::ZERO; N];

	#[inline]
	fn as_raw_slice(&self) -> &[Self::Store] {
		&self[..]
	}

	#[inline]
	fn as_raw_mut_slice(&mut self) -> &mut [Self::Store] {
		&mut self[..]
	}
}

#[doc = include_str!("../doc/view/AsBits.md")]
pub trait AsBits<T>
where T: BitStore
{
	/// Views `self` as an immutable bit-slice region with the `O` ordering.
	fn as_bits<O>(&self) -> &BitSlice<T, O>
	where O: BitOrder;

	/// Attempts to view `self` as an immutable bit-slice region with the `O`
	/// ordering.
	///
	/// This may return an error if `self` is too long to view as a bit-slice.
	fn try_as_bits<O>(&self) -> Result<&BitSlice<T, O>, BitSpanError<T>>
	where O: BitOrder;
}

#[doc = include_str!("../doc/view/AsMutBits.md")]
pub trait AsMutBits<T>
where T: BitStore
{
	/// Views `self` as a mutable bit-slice region with the `O` ordering.
	fn as_mut_bits<O>(&mut self) -> &mut BitSlice<T, O>
	where O: BitOrder;

	/// Attempts to view `self` as a mutable bit-slice region with the `O`
	/// ordering.
	///
	/// This may return an error if `self` is too long to view as a bit-slice.
	fn try_as_mut_bits<O>(
		&mut self,
	) -> Result<&mut BitSlice<T, O>, BitSpanError<T>>
	where O: BitOrder;
}

#[cfg(not(tarpaulin_include))]
impl<A, T> AsBits<T> for A
where
	A: AsRef<[T]>,
	T: BitStore,
{
	#[inline]
	fn as_bits<O>(&self) -> &BitSlice<T, O>
	where O: BitOrder {
		self.as_ref().view_bits::<O>()
	}

	#[inline]
	fn try_as_bits<O>(&self) -> Result<&BitSlice<T, O>, BitSpanError<T>>
	where O: BitOrder {
		self.as_ref().try_view_bits::<O>()
	}
}

#[cfg(not(tarpaulin_include))]
impl<A, T> AsMutBits<T> for A
where
	A: AsMut<[T]>,
	T: BitStore,
{
	#[inline]
	fn as_mut_bits<O>(&mut self) -> &mut BitSlice<T, O>
	where O: BitOrder {
		self.as_mut().view_bits_mut::<O>()
	}

	#[inline]
	fn try_as_mut_bits<O>(
		&mut self,
	) -> Result<&mut BitSlice<T, O>, BitSpanError<T>>
	where O: BitOrder {
		self.as_mut().try_view_bits_mut::<O>()
	}
}

#[cfg(test)]
mod tests {
	use static_assertions::*;

	use super::*;
	use crate::prelude::*;

	#[test]
	fn implementations() {
		let mut byte = 0u8;
		let mut bytes = [0u8; 2];
		assert!(byte.view_bits::<LocalBits>().not_any());
		assert!(byte.view_bits_mut::<LocalBits>().not_any());
		assert!(bytes.view_bits::<LocalBits>().not_any());
		assert!(bytes.view_bits_mut::<LocalBits>().not_any());
		assert!(bytes[..].view_bits::<LocalBits>().not_any());
		assert!(bytes[..].view_bits_mut::<LocalBits>().not_any());

		let mut blank: [u8; 0] = [];
		assert!(blank.view_bits::<LocalBits>().is_empty());
		assert!(blank.view_bits_mut::<LocalBits>().is_empty());

		assert_eq!([0u32; 2].as_bits::<LocalBits>().len(), 64);
		assert_eq!([0u32; 2].as_mut_bits::<LocalBits>().len(), 64);

		assert_eq!(0usize.as_raw_slice().len(), 1);
		assert_eq!(0usize.as_raw_mut_slice().len(), 1);
		assert_eq!(0u32.into_bitarray::<LocalBits>().len(), 32);

		assert_impl_all!(
			[usize; 10]: AsBits<usize>,
			AsMutBits<usize>,
			BitViewSized,
			BitView,
		);
	}
}
