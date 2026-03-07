#![doc = include_str!("../doc/array.md")]

use core::marker::PhantomData;

use crate::{
	mem,
	order::{
		BitOrder,
		Lsb0,
	},
	slice::BitSlice,
	view::BitViewSized,
};

mod api;
mod iter;
mod ops;
mod tests;
mod traits;

pub use self::iter::IntoIter;

#[repr(transparent)]
#[doc = include_str!("../doc/array/BitArray.md")]
pub struct BitArray<A = [usize; 1], O = Lsb0>
where
	A: BitViewSized,
	O: BitOrder,
{
	/// The ordering of bits within an `A::Store` element.
	pub _ord: PhantomData<O>,
	/// The wrapped data buffer.
	pub data: A,
}

impl<A, O> BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	/// A bit-array with all bits initialized to zero.
	pub const ZERO: Self = Self {
		_ord: PhantomData,
		data: A::ZERO,
	};

	/// Wraps an existing buffer as a bit-array.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let data = [0u16, 1, 2, 3];
	/// let bits = BitArray::<_, Msb0>::new(data);
	/// assert_eq!(bits.len(), 64);
	/// ```
	#[inline]
	pub fn new(data: A) -> Self {
		Self { data, ..Self::ZERO }
	}

	/// Removes the bit-array wrapper, returning the contained buffer.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bitarr![0; 30];
	/// let native: [usize; 1] = bits.into_inner();
	/// ```
	#[inline]
	pub fn into_inner(self) -> A {
		self.data
	}

	/// Explicitly views the bit-array as a bit-slice.
	#[inline]
	pub fn as_bitslice(&self) -> &BitSlice<A::Store, O> {
		self.data.view_bits::<O>()
	}

	/// Explicitly views the bit-array as a mutable bit-slice.
	#[inline]
	pub fn as_mut_bitslice(&mut self) -> &mut BitSlice<A::Store, O> {
		self.data.view_bits_mut::<O>()
	}

	/// Views the bit-array as a slice of its underlying memory elements.
	#[inline]
	pub fn as_raw_slice(&self) -> &[A::Store] {
		self.data.as_raw_slice()
	}

	/// Views the bit-array as a mutable slice of its underlying memory
	/// elements.
	#[inline]
	pub fn as_raw_mut_slice(&mut self) -> &mut [A::Store] {
		self.data.as_raw_mut_slice()
	}

	/// Gets the length (in bits) of the bit-array.
	///
	/// This method is a compile-time constant.
	#[inline]
	pub fn len(&self) -> usize {
		mem::bits_of::<A>()
	}

	/// Tests whether the array is empty.
	///
	/// This method is a compile-time constant.
	#[inline]
	pub fn is_empty(&self) -> bool {
		mem::bits_of::<A>() == 0
	}
}
