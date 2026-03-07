#![doc = include_str!("../../doc/array/api.md")]

use super::BitArray;
use crate::{
	order::BitOrder,
	slice::BitSlice,
	view::BitViewSized,
};

impl<A, O> BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	/// Returns a bit-slice containing the entire bit-array. Equivalent to
	/// `&a[..]`.
	///
	/// Because `BitArray` can be viewed as a slice of bits or as a slice of
	/// elements with equal ease, you should switch to using [`.as_bitslice()`]
	/// or [`.as_raw_slice()`] to make your choice explicit.
	///
	/// ## Original
	///
	/// [`array::as_slice`](https://doc.rust-lang.org/std/primitive.array.html#method.as_slice)
	///
	/// [`.as_bitslice()`]: Self::as_bitslice
	/// [`.as_raw_slice()`]: Self::as_raw_slice
	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_bitslice()` or `.as_raw_slice()` instead"]
	pub fn as_slice(&self) -> &BitSlice<A::Store, O> {
		self.as_bitslice()
	}

	/// Returns a mutable bit-slice containing the entire bit-array. Equivalent
	/// to `&mut a[..]`.
	///
	/// Because `BitArray` can be viewed as a slice of bits or as a slice of
	/// elements with equal ease, you should switch to using
	/// [`.as_mut_bitslice()`] or [`.as_raw_mut_slice()`] to make your choice
	/// explicit.
	///
	/// ## Original
	///
	/// [`array::as_mut_slice`](https://doc.rust-lang.org/std/primitive.array.html#method.as_mut_slice)
	///
	/// [`.as_mut_bitslice()`]: Self::as_mut_bitslice
	/// [`.as_raw_mut_slice()`]: Self::as_raw_mut_slice
	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_mut_bitslice()` or `.as_raw_mut_slice()` instead"]
	pub fn as_mut_slice(&mut self) -> &mut BitSlice<A::Store, O> {
		self.as_mut_bitslice()
	}
}
