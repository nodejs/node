#![doc = include_str!("../../doc/boxed/iter.md")]

use core::{
	fmt::{
		self,
		Debug,
		Formatter,
	},
	iter::FusedIterator,
	ops::Range,
};

use super::BitBox;
use crate::{
	order::{
		BitOrder,
		Lsb0,
	},
	slice::BitSlice,
	store::BitStore,
};

/// [Original](alloc::vec::IntoIter)
impl<T, O> IntoIterator for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type IntoIter = IntoIter<T, O>;
	type Item = bool;

	#[inline]
	fn into_iter(self) -> Self::IntoIter {
		IntoIter::new(self)
	}
}

/** An iterator over a `BitBox`.

## Original

[`vec::IntoIter`](alloc::vec::IntoIter)
**/
pub struct IntoIter<T = usize, O = Lsb0>
where
	T: BitStore,
	O: BitOrder,
{
	/// The original `BitBox`, kept so it can correctly drop.
	_buf: BitBox<T, O>,
	/// A range of indices yet to be iterated.
	//  TODO(myrrlyn): Race this against `BitPtrRange<Mut, T, O>`.
	iter: Range<usize>,
}

impl<T, O> IntoIter<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Wraps a bit-array in an iterator view. This is irreversible.
	#[inline]
	fn new(this: BitBox<T, O>) -> Self {
		let iter = 0 .. this.len();
		Self { _buf: this, iter }
	}

	/// Views the remaining unyielded bits as a bit-slice.
	///
	/// ## Original
	///
	/// [`IntoIter::as_slice`](alloc::vec::IntoIter::as_slice)
	#[inline]
	pub fn as_bitslice(&self) -> &BitSlice<T, O> {
		//  While the memory is never actually deïnitialized, this is still a
		//  good habit to do.
		unsafe {
			self._buf
				.as_bitptr()
				.add(self.iter.start)
				.span_unchecked(self.iter.len())
				.into_bitslice_ref()
		}
	}

	#[inline]
	#[doc(hidden)]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_slice(&self) -> &BitSlice<T, O> {
		self.as_bitslice()
	}

	/// Views the remaining unyielded bits as a mutable bit-slice.
	///
	/// ## Original
	///
	/// [`IntoIter::as_mut_slice`](alloc::vec::IntoIter::as_mut_slice)
	#[inline]
	pub fn as_mut_bitslice(&mut self) -> &mut BitSlice<T, O> {
		unsafe {
			self._buf
				.as_mut_bitptr()
				.add(self.iter.start)
				.span_unchecked(self.iter.len())
				.into_bitslice_mut()
		}
	}

	#[inline]
	#[doc(hidden)]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_mut_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_mut_slice(&mut self) -> &mut BitSlice<T, O> {
		self.as_mut_bitslice()
	}
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.IntoIter.html#impl-AsRef%3C%5BT%5D%3E)
#[cfg(not(tarpaulin_include))]
impl<T, O> AsRef<BitSlice<T, O>> for IntoIter<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_ref(&self) -> &BitSlice<T, O> {
		self.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> Clone for IntoIter<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn clone(&self) -> Self {
		Self {
			_buf: self._buf.clone(),
			iter: self.iter.clone(),
		}
	}
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.IntoIter.html#impl-Debug)
#[cfg(not(tarpaulin_include))]
impl<T, O> Debug for IntoIter<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_tuple("IntoIter")
			.field(&self.as_bitslice())
			.finish()
	}
}

impl<T, O> Iterator for IntoIter<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Item = bool;

	easy_iter!();

	#[inline]
	fn next(&mut self) -> Option<Self::Item> {
		self.iter
			.next()
			.map(|idx| unsafe { self._buf.as_bitptr().add(idx).read() })
	}

	#[inline]
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		self.iter
			.nth(n)
			.map(|idx| unsafe { self._buf.as_bitptr().add(idx).read() })
	}
}

impl<T, O> DoubleEndedIterator for IntoIter<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn next_back(&mut self) -> Option<Self::Item> {
		self.iter
			.next_back()
			.map(|idx| unsafe { self._buf.as_bitptr().add(idx).read() })
	}

	#[inline]
	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		self.iter
			.nth_back(n)
			.map(|idx| unsafe { self._buf.as_bitptr().add(idx).read() })
	}
}

impl<T, O> ExactSizeIterator for IntoIter<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn len(&self) -> usize {
		self.iter.len()
	}
}

impl<T, O> FusedIterator for IntoIter<T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.IntoIter.html#impl-Send)
// #[allow(clippy::non_send_fields_in_send_ty)]
unsafe impl<T, O> Send for IntoIter<T, O>
where
	T: BitStore + Sync,
	O: BitOrder,
{
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.IntoIter.html#impl-Sync)
unsafe impl<T, O> Sync for IntoIter<T, O>
where
	T: BitStore + Sync,
	O: BitOrder,
{
}
