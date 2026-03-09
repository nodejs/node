#![doc = include_str!("../../doc/array/iter.md")]

use core::{
	fmt::{
		self,
		Debug,
		Formatter,
	},
	iter::FusedIterator,
	ops::Range,
};

use tap::Pipe;
use wyz::comu::Const;

use super::BitArray;
use crate::{
	mem,
	order::BitOrder,
	ptr::BitPtr,
	slice::BitSlice,
	view::BitViewSized,
};

/// [Original](https://doc.rust-lang.org/std/primitive.array.html#impl-IntoIterator)
impl<A, O> IntoIterator for BitArray<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	type IntoIter = IntoIter<A, O>;
	type Item = <IntoIter<A, O> as Iterator>::Item;

	#[inline]
	fn into_iter(self) -> Self::IntoIter {
		IntoIter::new(self)
	}
}

/// [Original](https://doc.rust-lang.org/std/primitive.array.html#impl-IntoIterator-1)
#[cfg(not(tarpaulin_include))]
impl<'a, A, O> IntoIterator for &'a BitArray<A, O>
where
	O: BitOrder,
	A: 'a + BitViewSized,
{
	type IntoIter = <&'a BitSlice<A::Store, O> as IntoIterator>::IntoIter;
	type Item = <&'a BitSlice<A::Store, O> as IntoIterator>::Item;

	#[inline]
	fn into_iter(self) -> Self::IntoIter {
		self.as_bitslice().into_iter()
	}
}

/// [Original](https://doc.rust-lang.org/std/primitive.array.html#impl-IntoIterator-2)
#[cfg(not(tarpaulin_include))]
impl<'a, A, O> IntoIterator for &'a mut BitArray<A, O>
where
	O: BitOrder,
	A: 'a + BitViewSized,
{
	type IntoIter = <&'a mut BitSlice<A::Store, O> as IntoIterator>::IntoIter;
	type Item = <&'a mut BitSlice<A::Store, O> as IntoIterator>::Item;

	#[inline]
	fn into_iter(self) -> Self::IntoIter {
		self.as_mut_bitslice().into_iter()
	}
}

#[derive(Clone)]
#[doc = include_str!("../../doc/array/IntoIter.md")]
pub struct IntoIter<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	/// The bit-array being iterated.
	array: BitArray<A, O>,
	/// The indices in `.array` that have not yet been yielded.
	///
	/// This range is always a strict subset of `0 .. self.array.len()`.
	alive: Range<usize>,
}

impl<A, O> IntoIter<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	/// Converts a bit-array into its iterator.
	///
	/// The [`.into_iter()`] method on bit-arrays forwards to this. While
	/// `BitArray` does deref to `&/mut BitSlice`, which also has
	/// `.into_iter()`, this behavior has always been present alongside
	/// `BitArray` and there is no legacy forwarding to preserve.
	///
	/// ## Original
	///
	/// [`IntoIter::new`](core::array::IntoIter::new)s
	#[inline]
	pub fn new(array: BitArray<A, O>) -> Self {
		Self {
			array,
			alive: 0 .. mem::bits_of::<A>(),
		}
	}

	/// Views the remaining unyielded bits in the iterator.
	///
	/// ## Original
	///
	/// [`IntoIter::as_slice`](core::array::IntoIter::as_slice)
	#[inline]
	pub fn as_bitslice(&self) -> &BitSlice<A::Store, O> {
		unsafe { self.array.as_bitslice().get_unchecked(self.alive.clone()) }
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_slice(&self) -> &BitSlice<A::Store, O> {
		self.as_bitslice()
	}

	/// Mutably views the remaining unyielded bits in the iterator.
	///
	/// ## Original
	///
	/// [`IntoIter::as_mut_slice`](core::array::IntoIter::as_mut_slice)
	#[inline]
	pub fn as_mut_bitslice(&mut self) -> &mut BitSlice<A::Store, O> {
		unsafe {
			self.array
				.as_mut_bitslice()
				.get_unchecked_mut(self.alive.clone())
		}
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_bitslice_mut()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_mut_slice(&mut self) -> &mut BitSlice<A::Store, O> {
		self.as_mut_bitslice()
	}

	/// Gets a bit from the bit-array.
	#[inline]
	fn get(&self, index: usize) -> bool {
		unsafe {
			self.array
				.as_raw_slice()
				.pipe(BitPtr::<Const, A::Store, O>::from_slice)
				.add(index)
				.read()
		}
	}
}

#[cfg(not(tarpaulin_include))]
impl<A, O> Debug for IntoIter<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_tuple("IntoIter")
			.field(&self.as_bitslice())
			.finish()
	}
}

impl<A, O> Iterator for IntoIter<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	type Item = bool;

	easy_iter!();

	#[inline]
	fn next(&mut self) -> Option<Self::Item> {
		self.alive.next().map(|idx| self.get(idx))
	}

	#[inline]
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		self.alive.nth(n).map(|idx| self.get(idx))
	}
}

impl<A, O> DoubleEndedIterator for IntoIter<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn next_back(&mut self) -> Option<Self::Item> {
		self.alive.next_back().map(|idx| self.get(idx))
	}

	#[inline]
	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		self.alive.nth_back(n).map(|idx| self.get(idx))
	}
}

impl<A, O> ExactSizeIterator for IntoIter<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn len(&self) -> usize {
		self.alive.len()
	}
}

impl<A, O> FusedIterator for IntoIter<A, O>
where
	A: BitViewSized,
	O: BitOrder,
{
}
