#![doc = include_str!("../../doc/slice/iter.md")]

use core::{
	cmp,
	fmt::{
		self,
		Debug,
		Formatter,
	},
	iter::{
		FusedIterator,
		Map,
	},
	marker::PhantomData,
	mem,
};

use wyz::comu::{
	Const,
	Mut,
};

use super::{
	BitSlice,
	BitSliceIndex,
};
use crate::{
	order::{
		BitOrder,
		Lsb0,
		Msb0,
	},
	ptr::{
		BitPtrRange,
		BitRef,
	},
	store::BitStore,
};

/// [Original](https://doc.rust-lang.org/core/iter/trait.IntoIterator.html#impl-IntoIterator-1)
#[cfg(not(tarpaulin_include))]
impl<'a, T, O> IntoIterator for &'a BitSlice<T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	type IntoIter = Iter<'a, T, O>;
	type Item = <Self::IntoIter as Iterator>::Item;

	#[inline]
	fn into_iter(self) -> Self::IntoIter {
		Iter::new(self)
	}
}

/// [Original](https://doc.rust-lang.org/core/iter/trait.IntoIterator.html#impl-IntoIterator-3)
#[cfg(not(tarpaulin_include))]
impl<'a, T, O> IntoIterator for &'a mut BitSlice<T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	type IntoIter = IterMut<'a, T, O>;
	type Item = <Self::IntoIter as Iterator>::Item;

	#[inline]
	fn into_iter(self) -> Self::IntoIter {
		IterMut::new(self)
	}
}

#[repr(transparent)]
#[doc = include_str!("../../doc/slice/iter/Iter.md")]
pub struct Iter<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// A dual-pointer range of the bit-slice undergoing iteration.
	///
	/// This structure stores two fully-decode pointers to the first live and
	/// first dead bits, trading increased size (three words instead of two) for
	/// faster performance when iterating.
	range: BitPtrRange<Const, T, O>,
	/// `Iter` is semantically equivalent to a `&BitSlice`.
	_ref:  PhantomData<&'a BitSlice<T, O>>,
}

impl<'a, T, O> Iter<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub(super) fn new(slice: &'a BitSlice<T, O>) -> Self {
		Self {
			range: slice.as_bitptr_range(),
			_ref:  PhantomData,
		}
	}

	/// Views the currently unyielded bit-slice.
	///
	/// Because the iterator is a shared view, the returned bit-slice does not
	/// cause a lifetime conflict, and the iterator can continue to be used
	/// while it exists.
	///
	/// ## Original
	///
	/// [`Iter::as_slice`](core::slice::Iter::as_slice)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 0, 1, 1];
	/// let mut iter = bits.iter();
	///
	/// assert_eq!(iter.as_bitslice(), bits![0, 0, 1, 1]);
	/// assert!(!*iter.nth(1).unwrap());
	/// assert_eq!(iter.as_bitslice(), bits![1, 1]);
	/// ```
	#[inline]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_bitslice(&self) -> &'a BitSlice<T, O> {
		unsafe { self.range.clone().into_bitspan().into_bitslice_ref() }
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_slice(&self) -> &'a BitSlice<T, O> {
		self.as_bitslice()
	}

	/// Adapts the iterator to yield regular `&bool` references rather than the
	/// [proxy reference][0].
	///
	/// This allows the iterator to be used in APIs that expect ordinary
	/// references. It reads from the proxy and provides an equivalent
	/// `&'static bool`. The address value of the yielded reference is not
	/// related to the addresses covered by the `BitSlice` buffer in any way.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1];
	/// let mut iter = bits.iter().by_refs();
	/// assert_eq!(iter.next(), Some(&false));
	/// assert_eq!(iter.next(), Some(&true));
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// [0]: crate::ptr::BitRef
	#[inline]
	pub fn by_refs(self) -> BitRefIter<'a, T, O> {
		self.by_vals().map(|bit| match bit {
			true => &true,
			false => &false,
		})
	}

	/// Adapts the iterator to yield `bool` values rather than the
	/// [proxy reference][0].
	///
	/// This allows the iterator to be used in APIs that expect direct values.
	/// It dereferences the proxy and yields the referent `bool` directly. It
	/// replaces `Iterator::copied`, which is not available on this type.
	///
	/// ## Original
	///
	/// [`Iterator::copied`](core::iter::Iterator::copied)
	///
	/// ## Performance
	///
	/// This bypasses the construction of a `BitRef` for each yielded bit. Do
	/// not use `bits.as_bitptr_range().map(|bp| unsafe { bp.read() })` in a
	/// misguided attempt to eke out some additional performance in your code.
	///
	/// This iterator is already the fastest possible walk across a bit-slice.
	/// You do not need to beat it.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1];
	/// let mut iter = bits.iter().by_vals();
	/// assert_eq!(iter.next(), Some(false));
	/// assert_eq!(iter.next(), Some(true));
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// [0]: crate::ptr::BitRef
	#[inline]
	pub fn by_vals(self) -> BitValIter<'a, T, O> {
		BitValIter {
			range: self.range,
			_life: PhantomData,
		}
	}

	/// Yields `bool` values directly, rather than [proxy references][0].
	///
	/// The original slice iterator yields true `&bool`, and as such allows
	/// [`Iterator::copied`] to exist. This iterator does not satisfy the bounds
	/// for that method, so `.copied()` is provided as an inherent in order to
	/// maintain source compatibility. Prefer [`.by_vals()`] instead, which
	/// avoids the name collision while still making clear that it yields `bool`
	/// values.
	///
	/// [`Iterator::copied`]: core::iter::Iterator::copied
	/// [`.by_vals()`]: Self::by_vals
	/// [0]: crate::ptr::BitRef
	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "`Iterator::copied` does not exist on this type. Use \
	                `.by_vals()` instead"]
	pub fn copied(self) -> BitValIter<'a, T, O> {
		self.by_vals()
	}
}

/// [Original](https://doc.rust-lang.org/core/slice/struct.Iter.html#impl-Clone)
#[cfg(not(tarpaulin_include))]
impl<T, O> Clone for Iter<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn clone(&self) -> Self {
		Self {
			range: self.range.clone(),
			..*self
		}
	}
}

/// [Original](https://doc.rust-lang.org/core/slice/struct.Iter.html#impl-AsRef%3C%5BT%5D%3E)
#[cfg(not(tarpaulin_include))]
impl<T, O> AsRef<BitSlice<T, O>> for Iter<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_ref(&self) -> &BitSlice<T, O> {
		self.as_bitslice()
	}
}

/// [Original](https://doc.rust-lang.org/core/slice/struct.Iter.html#impl-Debug)
#[cfg(not(tarpaulin_include))]
impl<T, O> Debug for Iter<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_tuple("Iter").field(&self.as_bitslice()).finish()
	}
}

#[repr(transparent)]
#[doc = include_str!("../../doc/slice/iter/IterMut.md")]
pub struct IterMut<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// A dual-pointer range of the bit-slice undergoing iteration.
	///
	/// This structure stores two fully-decode pointers to the first live and
	/// first dead bits, trading increased size (three words instead of two) for
	/// faster performance when iterating.
	range: BitPtrRange<Mut, T::Alias, O>,
	/// `IterMut` is semantically equivalent to an aliased `&mut BitSlice`.
	_ref:  PhantomData<&'a mut BitSlice<T::Alias, O>>,
}

impl<'a, T, O> IterMut<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub(super) fn new(slice: &'a mut BitSlice<T, O>) -> Self {
		Self {
			range: slice.alias_mut().as_mut_bitptr_range(),
			_ref:  PhantomData,
		}
	}

	/// Views the underlying bit-slice as a subslice of the original data.
	///
	/// This consumes the iterator in order to avoid creating aliasing
	/// references between the returned subslice (which has the original
	/// lifetime, and is not borrowed from the iterator) and the proxies the
	/// iterator produces.
	///
	/// ## Original
	///
	/// [`IterMut::into_slice`](core::slice::IterMut::into_slice)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 0, 1, 1];
	/// let mut iter = bits.iter_mut();
	///
	/// *iter.next().unwrap() = true;
	/// assert_eq!(iter.into_bitslice(), bits![0, 1, 1]);
	/// assert!(bits[0]);
	/// ```
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn into_bitslice(self) -> &'a mut BitSlice<T::Alias, O> {
		unsafe { self.range.into_bitspan().into_bitslice_mut() }
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.into_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn into_slice(self) -> &'a mut BitSlice<T::Alias, O> {
		self.into_bitslice()
	}

	/// Views the remaining bit-slice that has not yet been iterated.
	///
	/// This borrows the iterator’s own lifetime, preventing it from being used
	/// while the bit-slice view exists and thus ensuring that no aliasing
	/// references are created. Bits that the iterator has already yielded are
	/// not included in the produced bit-slice.
	///
	/// ## Original
	///
	/// [`IterMut::as_slice`](core::slice::IterMut::as_slice)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0; 4];
	/// let mut iter = bits.iter_mut();
	///
	/// *iter.next().unwrap() = true;
	/// assert_eq!(iter.as_bitslice(), bits![0; 3]);
	/// *iter.next().unwrap() = true;
	/// assert_eq!(iter.as_bitslice(), bits![0; 2]);
	///
	/// assert_eq!(bits, bits![1, 1, 0, 0]);
	/// ```
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn as_bitslice(&self) -> &BitSlice<T::Alias, O> {
		unsafe { self.range.clone().into_bitspan().into_bitslice_ref() }
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_slice(&self) -> &BitSlice<T::Alias, O> {
		self.as_bitslice()
	}
}

/// [Original](https://doc.rust-lang.org/core/slice/struct.IterMut.html#impl-AsRef%3C%5BT%5D%3E)
#[cfg(not(tarpaulin_include))]
impl<T, O> AsRef<BitSlice<T::Alias, O>> for IterMut<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_ref(&self) -> &BitSlice<T::Alias, O> {
		self.as_bitslice()
	}
}

/// [Original](https://doc.rust-lang.org/core/slice/struct.IterMut.html#impl-Debug)
#[cfg(not(tarpaulin_include))]
impl<T, O> Debug for IterMut<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_tuple("IterMut")
			.field(&self.as_bitslice())
			.finish()
	}
}

/// `Iter` and `IterMut` have very nearly the same implementation text.
macro_rules! iter {
	($($iter:ident => $item:ty);+ $(;)?) => { $(
		/// [Original](https://doc.rust-lang.org/core/slice/struct.Iter.html#impl-Iterator) and
		/// [Original](https://doc.rust-lang.org/core/slice/struct.IterMut.html#impl-Iterator)
		impl<'a, T, O> Iterator for $iter<'a, T, O>
		where
			T: 'a + BitStore,
			O: BitOrder,
		{
			type Item = $item;

			#[inline]
			fn next(&mut self) -> Option<Self::Item> {
				self.range.next().map(|bp| unsafe { BitRef::from_bitptr(bp) })
			}

			#[inline]
			fn nth(&mut self, n: usize) -> Option<Self::Item> {
				self.range.nth(n).map(|bp| unsafe { BitRef::from_bitptr(bp) })
			}

			easy_iter!();
		}

		/// [Original](https://doc.rust-lang.org/core/slice/struct.Iter.html#impl-DoubleEndedIterator) and
		/// [Original](https://doc.rust-lang.org/core/slice/struct.IterMut.html#impl-DoubleEndedIterator)
		impl<'a, T, O> DoubleEndedIterator for $iter<'a, T, O>
		where
			T: 'a + BitStore,
			O: BitOrder,
		{
			#[inline]
			fn next_back(&mut self) -> Option<Self::Item> {
				self.range
					.next_back()
					.map(|bp| unsafe { BitRef::from_bitptr(bp) })
			}

			#[inline]
			fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
				self.range
					.nth_back(n)
					.map(|bp| unsafe { BitRef::from_bitptr(bp) })
			}
		}

		/// [Original](https://doc.rust-lang.org/core/slice/struct.Iter.html#impl-ExactSizeIterator) and
		/// [Original](https://doc.rust-lang.org/core/slice/struct.IterMut.html#impl-ExactSizeIterator)
		impl<T, O> ExactSizeIterator for $iter<'_, T, O>
		where
			T: BitStore,
			O: BitOrder,
		{
			#[inline]
			fn len(&self) -> usize {
				self.range.len()
			}
		}

		/// [Original](https://doc.rust-lang.org/core/slice/struct.Iter.html#impl-FusedIterator) and
		/// [Original](https://doc.rust-lang.org/core/slice/struct.IterMut.html#impl-FusedIterator)
		impl<T, O> FusedIterator for $iter<'_, T, O>
		where
			T: BitStore,
			O: BitOrder,
		{
		}

		/// [Original](https://doc.rust-lang.org/core/slice/struct.Iter.html#impl-Send) and
		/// [Original](https://doc.rust-lang.org/core/slice/struct.IterMut.html#impl-Send)
		// #[allow(clippy::non_send_fields_in_send_ty)]
		unsafe impl<'a, T, O> Send for $iter<'a, T, O>
		where
			T: BitStore,
			O: BitOrder,
			&'a mut BitSlice<T, O>: Send,
		{
		}

		/// [Original](https://doc.rust-lang.org/core/slice/struct.Iter.html#impl-Sync) and
		/// [Original](https://doc.rust-lang.org/core/slice/struct.IterMut.html#impl-Sync)
		unsafe impl<T, O> Sync for $iter<'_, T, O>
		where
			T: BitStore,
			O: BitOrder,
			BitSlice<T, O>: Sync,
		{
		}
	)+ };
}

iter! {
	Iter => <usize as BitSliceIndex<'a, T, O>>::Immut;
	IterMut => <usize as BitSliceIndex<'a, T::Alias, O>>::Mut;
}

/// Builds an iterator implementation for grouping iterators.
macro_rules! group {
	//  The iterator and its yielded type.
	($iter:ident => $item:ty {
		//  The eponymous functions from the iterator traits.
		$next:item
		$nth:item
		$next_back:item
		$nth_back:item
		$len:item
	}) => {
		impl<'a, T, O> Iterator for $iter<'a, T, O>
		where
			T: 'a + BitStore,
			O: BitOrder,
		{
			type Item = $item;

			#[inline]
			$next

			#[inline]
			$nth

			easy_iter!();
		}

		impl<T, O> DoubleEndedIterator for $iter<'_, T, O>
		where
			T: BitStore,
			O: BitOrder,
		{
			#[inline]
			$next_back

			#[inline]
			$nth_back
		}

		impl<T, O> ExactSizeIterator for $iter<'_, T, O>
		where
			T: BitStore,
			O: BitOrder,
		{
			#[inline]
			$len
		}

		impl<T, O> FusedIterator for $iter<'_, T, O>
		where
			T: BitStore,
			O: BitOrder,
		{
		}
	};
}

/// An iterator over `BitSlice` that yields `&bool` directly.
pub type BitRefIter<'a, T, O> = Map<BitValIter<'a, T, O>, fn(bool) -> &'a bool>;

/// An iterator over `BitSlice` that yields `bool` directly.
pub struct BitValIter<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The start and end bit-pointers in the iteration region.
	range: BitPtrRange<Const, T, O>,
	/// Hold the lifetime of the source region, so that this does not cause UAF.
	_life: PhantomData<&'a BitSlice<T, O>>,
}

group!(BitValIter => bool {
	fn next(&mut self) -> Option<Self::Item> {
		self.range.next().map(|bp| unsafe { bp.read() })
	}

	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		self.range.nth(n).map(|bp| unsafe { bp.read() })
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		self.range.next_back().map(|bp| unsafe { bp.read() })
	}

	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		self.range.nth_back(n).map(|bp| unsafe { bp.read() })
	}

	fn len(&self) -> usize {
		self.range.len()
	}
});

#[derive(Clone, Debug)]
#[doc = include_str!("../../doc/slice/iter/Windows.md")]
pub struct Windows<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The source bit-slice.
	slice: &'a BitSlice<T, O>,
	/// The width of the produced windows.
	width: usize,
}

group!(Windows => &'a BitSlice<T, O> {
	fn next(&mut self) -> Option<Self::Item> {
		if self.width > self.slice.len() {
			self.slice = Default::default();
			return None;
		}
		unsafe {
			let out = self.slice.get_unchecked(.. self.width);
			self.slice = self.slice.get_unchecked(1 ..);
			Some(out)
		}
	}

	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let (end, ovf) = self.width.overflowing_add(n);
		if end > self.slice.len() || ovf {
			self.slice = Default::default();
			return None;
		}
		unsafe {
			let out = self.slice.get_unchecked(n .. end);
			self.slice = self.slice.get_unchecked(n + 1 ..);
			Some(out)
		}
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		let len = self.slice.len();
		if self.width > len {
			self.slice = Default::default();
			return None;
		}
		unsafe {
			let out = self.slice.get_unchecked(len - self.width ..);
			self.slice = self.slice.get_unchecked(.. len - 1);
			Some(out)
		}
	}

	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		let (end, ovf) = self.slice.len().overflowing_sub(n);
		if end < self.width || ovf {
			self.slice = Default::default();
			return None;
		}
		unsafe {
			let out = self.slice.get_unchecked(end - self.width .. end);
			self.slice = self.slice.get_unchecked(.. end - 1);
			Some(out)
		}
	}

	fn len(&self) -> usize {
		let len = self.slice.len();
		if self.width > len {
			0
		}
		else {
			len - self.width + 1
		}
	}
});

#[derive(Clone, Debug)]
#[doc = include_str!("../../doc/slice/iter/Chunks.md")]
pub struct Chunks<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The source bit-slice.
	slice: &'a BitSlice<T, O>,
	/// The width of the produced chunks.
	width: usize,
}

group!(Chunks => &'a BitSlice<T, O> {
	fn next(&mut self) -> Option<Self::Item> {
		let len = self.slice.len();
		if len == 0 {
			return None;
		}
		let mid = cmp::min(len, self.width);
		let (out, rest) = unsafe { self.slice.split_at_unchecked(mid) };
		self.slice = rest;
		Some(out)
	}

	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let len = self.slice.len();
		let (start, ovf) = n.overflowing_mul(self.width);
		if start >= len || ovf {
			self.slice = Default::default();
			return None;
		}
		let split = start.checked_add(self.width)
			.map(|mid| cmp::min(mid, len))
			.unwrap_or(len);
		unsafe {
			let (head, rest) = self.slice.split_at_unchecked(split);
			self.slice = rest;
			Some(head.get_unchecked(start ..))
		}
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		match self.slice.len() {
			0 => None,
			len => {
				//  Determine if the back chunk is a remnant or a whole chunk.
				let rem = len % self.width;
				let size = if rem == 0 { self.width } else { rem };
				let (rest, out)
					= unsafe { self.slice.split_at_unchecked(len - size) };
				self.slice = rest;
				Some(out)
			},
		}
	}

	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		let len = self.len();
		if n >= len {
			self.slice = Default::default();
			return None;
		}
		let start = (len - 1 - n) * self.width;
		let width = cmp::min(start + self.width, self.slice.len());
		let (rest, out) = unsafe {
			self.slice
				.get_unchecked(.. start + width)
				.split_at_unchecked(start)
		};
		self.slice = rest;
		Some(out)
	}

	fn len(&self) -> usize {
		match self.slice.len() {
			0 => 0,
			len => {
				let (n, r) = (len / self.width, len % self.width);
				n + (r > 0) as usize
			},
		}
	}
});

#[derive(Debug)]
#[doc = include_str!("../../doc/slice/iter/ChunksMut.md")]
pub struct ChunksMut<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The source bit-slice, marked with the alias tainting.
	slice: &'a mut BitSlice<T::Alias, O>,
	/// The width of the produced chunks.
	width: usize,
}

group!(ChunksMut => &'a mut BitSlice<T::Alias, O> {
	fn next(&mut self) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		let len = slice.len();
		if len == 0 {
			return None;
		}
		let mid = cmp::min(len, self.width);
		let (out, rest) = unsafe { slice.split_at_unchecked_mut_noalias(mid) };
		self.slice = rest;
		Some(out)
	}

	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		let len = slice.len();
		let (start, ovf) = n.overflowing_mul(self.width);
		if start >= len || ovf {
			return None;
		}
		let (out, rest) = unsafe {
			slice
				.get_unchecked_mut(start ..)
				.split_at_unchecked_mut_noalias(cmp::min(len - start, self.width))
		};
		self.slice = rest;
		Some(out)
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		match slice.len() {
			0 => None,
			len => {
				let rem = len % self.width;
				let size = if rem == 0 { self.width } else { rem };
				let mid = len - size;
				let (rest, out)
					= unsafe { slice.split_at_unchecked_mut_noalias(mid) };
				self.slice = rest;
				Some(out)
			},
		}
	}

	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		let len = self.len();
		let slice = mem::take(&mut self.slice);
		if n >= len {
			return None;
		}
		let start = (len - 1 - n) * self.width;
		let width = cmp::min(start + self.width, slice.len());
		let (rest, out) = unsafe {
			slice
				.get_unchecked_mut(.. start + width)
				.split_at_unchecked_mut_noalias(start)
		};
		self.slice = rest;
		Some(out)
	}

	fn len(&self) -> usize {
		match self.slice.len() {
			0 => 0,
			len => {
				let (n, r) = (len / self.width, len % self.width);
				n + (r > 0) as usize
			},
		}
	}
});

#[derive(Clone, Debug)]
#[doc = include_str!("../../doc/slice/iter/ChunksExact.md")]
pub struct ChunksExact<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The source bit-slice.
	slice: &'a BitSlice<T, O>,
	/// Any remnant of the source bit-slice that will not be yielded as a chunk.
	extra: &'a BitSlice<T, O>,
	/// The width of the produced chunks.
	width: usize,
}

impl<'a, T, O> ChunksExact<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub(super) fn new(slice: &'a BitSlice<T, O>, width: usize) -> Self {
		assert_ne!(width, 0, "Chunk width cannot be 0");
		let len = slice.len();
		let rem = len % width;
		let (slice, extra) = unsafe { slice.split_at_unchecked(len - rem) };
		Self {
			slice,
			extra,
			width,
		}
	}

	/// Gets the remnant bit-slice that the iterator will not yield.
	///
	/// ## Original
	///
	/// [`ChunksExact::remainder`](core::slice::ChunksExact::remainder)
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn remainder(&self) -> &'a BitSlice<T, O> {
		self.extra
	}
}

group!(ChunksExact => &'a BitSlice<T, O> {
	fn next(&mut self) -> Option<Self::Item> {
		if self.slice.len() < self.width {
			return None;
		}
		let (out, rest) = unsafe { self.slice.split_at_unchecked(self.width) };
		self.slice = rest;
		Some(out)
	}

	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let (start, ovf) = n.overflowing_mul(self.width);
		if start >= self.slice.len() || ovf {
			self.slice = Default::default();
			return None;
		}
		let (out, rest) = unsafe {
			self.slice
				.get_unchecked(start ..)
				.split_at_unchecked(self.width)
		};
		self.slice = rest;
		Some(out)
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		let len = self.slice.len();
		if len < self.width {
			return None;
		}
		let (rest, out) =
			unsafe { self.slice.split_at_unchecked(len - self.width) };
		self.slice = rest;
		Some(out)
	}

	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		let len = self.len();
		if n >= len {
			self.slice = Default::default();
			return None;
		}
		let end = (len - n) * self.width;
		let (rest, out) = unsafe {
			self.slice
				.get_unchecked(.. end)
				.split_at_unchecked(end - self.width)
		};
		self.slice = rest;
		Some(out)
	}

	fn len(&self) -> usize {
		self.slice.len() / self.width
	}
});

#[derive(Debug)]
#[doc = include_str!("../../doc/slice/iter/ChunksExactMut.md")]
pub struct ChunksExactMut<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The source bit-slice, marked with the alias tainting.
	slice: &'a mut BitSlice<T::Alias, O>,
	/// Any remnant of the source bit-slice that will not be yielded as a chunk.
	extra: &'a mut BitSlice<T::Alias, O>,
	/// The width of the produced chunks.
	width: usize,
}

impl<'a, T, O> ChunksExactMut<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub(super) fn new(slice: &'a mut BitSlice<T, O>, width: usize) -> Self {
		assert_ne!(width, 0, "Chunk width cannot be 0");
		let len = slice.len();
		let rem = len % width;
		let (slice, extra) = unsafe { slice.split_at_unchecked_mut(len - rem) };
		Self {
			slice,
			extra,
			width,
		}
	}

	/// Consumes the iterator, returning the remnant bit-slice that it will not
	/// yield.
	///
	/// ## Original
	///
	/// [`ChunksExactMut::into_remainder`][0]
	///
	/// [0]: core::slice::ChunksExactMut::into_remainder
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn into_remainder(self) -> &'a mut BitSlice<T::Alias, O> {
		self.extra
	}

	/// Takes the remnant bit-slice out of the iterator.
	///
	/// The first time this is called, it will produce the remnant; on each
	/// subsequent call, it will produce an empty bit-slice.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0; 5];
	/// let mut chunks = bits.chunks_exact_mut(3);
	///
	/// assert_eq!(chunks.take_remainder(), bits![0; 2]);
	/// assert!(chunks.take_remainder().is_empty());
	/// ```
	#[inline]
	pub fn take_remainder(&mut self) -> &'a mut BitSlice<T::Alias, O> {
		mem::take(&mut self.extra)
	}
}

group!(ChunksExactMut => &'a mut BitSlice<T::Alias, O> {
	fn next(&mut self) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		if slice.len() < self.width {
			return None;
		}
		let (out, rest) =
			unsafe { slice.split_at_unchecked_mut_noalias(self.width) };
		self.slice = rest;
		Some(out)
	}

	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		let (start, ovf) = n.overflowing_mul(self.width);
		if start + self.width >= slice.len() || ovf {
			return None;
		}
		let (out, rest) = unsafe {
			slice.get_unchecked_mut(start ..)
				.split_at_unchecked_mut_noalias(self.width)
		};
		self.slice = rest;
		Some(out)
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		let len = slice.len();
		if len < self.width {
			return None;
		}
		let (rest, out) =
			unsafe { slice.split_at_unchecked_mut_noalias(len - self.width) };
		self.slice = rest;
		Some(out)
	}

	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		let len = self.len();
		let slice = mem::take(&mut self.slice);
		if n >= len {
			return None;
		}
		let end = (len - n) * self.width;
		let (rest, out) = unsafe {
			slice.get_unchecked_mut(.. end)
				.split_at_unchecked_mut_noalias(end - self.width)
		};
		self.slice = rest;
		Some(out)
	}

	fn len(&self) -> usize {
		self.slice.len() / self.width
	}
});

#[derive(Clone, Debug)]
#[doc = include_str!("../../doc/slice/iter/RChunks.md")]
pub struct RChunks<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The source bit-slice.
	slice: &'a BitSlice<T, O>,
	/// The width of the produced chunks.
	width: usize,
}

group!(RChunks => &'a BitSlice<T, O> {
	fn next(&mut self) -> Option<Self::Item> {
		let len = self.slice.len();
		if len == 0 {
			return None;
		}
		let mid = len - cmp::min(len, self.width);
		let (rest, out) = unsafe { self.slice.split_at_unchecked(mid) };
		self.slice = rest;
		Some(out)
	}

	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let len = self.slice.len();
		let (num, ovf) = n.overflowing_mul(self.width);
		if num >= len || ovf {
			self.slice = Default::default();
			return None;
		}
		let end = len - num;
		let mid = end.saturating_sub(self.width);
		let (rest, out) = unsafe {
			self.slice
				.get_unchecked(.. end)
				.split_at_unchecked(mid)
		};
		self.slice = rest;
		Some(out)
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		match self.slice.len() {
			0 => None,
			n => {
				let rem = n % self.width;
				let len = if rem == 0 { self.width } else { rem };
				let (out, rest) = unsafe { self.slice.split_at_unchecked(len) };
				self.slice = rest;
				Some(out)
			},
		}
	}

	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		let len = self.len();
		if n >= len {
			self.slice = Default::default();
			return None;
		}
		/* Taking from the back of a reverse iterator means taking from the
		front of the slice.

		`len` gives us the total number of subslices remaining. In order to find
		the partition point, we need to subtract `n - 1` full subslices from
		that count (because the back slice of the iteration might not be full),
		compute their bit width, and offset *that* from the end of the memory
		region. This gives us the zero-based index of the partition point
		between what is returned and what is retained.

		The `part ..` section of the slice is retained, and the very end of the
		`.. part` section is returned. The head section is split at no less than
		`self.width` bits below the end marker (this could be the partial
		section, so a wrapping subtraction cannot be used), and `.. start` is
		discarded.

		Source:
		https://doc.rust-lang.org/1.43.0/src/core/slice/mod.rs.html#5141-5156
		*/
		let from_end = (len - 1 - n) * self.width;
		let end = self.slice.len() - from_end;
		let start = end.saturating_sub(self.width);
		let (out, rest) = unsafe { self.slice.split_at_unchecked(end) };
		self.slice = rest;
		Some(unsafe { out.get_unchecked(start ..) })
	}

	fn len(&self) -> usize {
		match self.slice.len() {
			0 => 0,
			len => {
				let (n, r) = (len / self.width, len % self.width);
				n + (r > 0) as usize
			},
		}
	}
});

#[derive(Debug)]
#[doc = include_str!("../../doc/slice/iter/RChunksMut.md")]
pub struct RChunksMut<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The source bit-slice, marked with the alias tainting.
	slice: &'a mut BitSlice<T::Alias, O>,
	/// The width of the produced chunks.
	width: usize,
}

group!(RChunksMut => &'a mut BitSlice<T::Alias, O> {
	fn next(&mut self) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		let len = slice.len();
		if len == 0 {
			return None;
		}
		let mid = len - cmp::min(len, self.width);
		let (rest, out) = unsafe { slice.split_at_unchecked_mut_noalias(mid) };
		self.slice = rest;
		Some(out)
	}

	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		let len = slice.len();
		let (num, ovf) = n.overflowing_mul(self.width);
		if num >= len || ovf {
			return None;
		}
		let end = len - num;
		let mid = end.saturating_sub(self.width);
		let (rest, out) = unsafe {
			slice.get_unchecked_mut(.. end)
				.split_at_unchecked_mut_noalias(mid)
		};
		self.slice = rest;
		Some(out)
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		match slice.len() {
			0 => None,
			n => {
				let rem = n % self.width;
				let len = if rem == 0 { self.width } else { rem };
				let (out, rest) =
					unsafe { slice.split_at_unchecked_mut_noalias(len) };
				self.slice = rest;
				Some(out)
			},
		}
	}

	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		let len = self.len();
		let slice = mem::take(&mut self.slice);
		if n >= len {
			return None;
		}
		let from_end = (len - 1 - n) * self.width;
		let end = slice.len() - from_end;
		let start = end.saturating_sub(self.width);
		let (out, rest) = unsafe { slice.split_at_unchecked_mut_noalias(end) };
		self.slice = rest;
		Some(unsafe { out.get_unchecked_mut(start ..) })
	}

	fn len(&self) -> usize {
		match self.slice.len() {
			0 => 0,
			len => {
				let (n, r) = (len / self.width, len % self.width);
				n + (r > 0) as usize
			},
		}
	}
});

#[derive(Clone, Debug)]
#[doc = include_str!("../../doc/slice/iter/RChunksExact.md")]
pub struct RChunksExact<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The source bit-slice.
	slice: &'a BitSlice<T, O>,
	/// Any remnant of the source bit-slice that will not be yielded as a chunk.
	extra: &'a BitSlice<T, O>,
	/// The width of the produced chunks.
	width: usize,
}

impl<'a, T, O> RChunksExact<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub(super) fn new(slice: &'a BitSlice<T, O>, width: usize) -> Self {
		assert_ne!(width, 0, "Chunk width cannot be 0");
		let (extra, slice) =
			unsafe { slice.split_at_unchecked(slice.len() % width) };
		Self {
			slice,
			extra,
			width,
		}
	}

	/// Gets the remnant bit-slice that the iterator will not yield.
	///
	/// ## Original
	///
	/// [`RChunksExact::remainder`](core::slice::RChunksExact::remainder)
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn remainder(&self) -> &'a BitSlice<T, O> {
		self.extra
	}
}

group!(RChunksExact => &'a BitSlice<T, O> {
	fn next(&mut self) -> Option<Self::Item> {
		let len = self.slice.len();
		if len < self.width {
			return None;
		}
		let (rest, out) =
			unsafe { self.slice.split_at_unchecked(len - self.width) };
		self.slice = rest;
		Some(out)
	}

	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let len = self.slice.len();
		let (split, ovf) = n.overflowing_mul(self.width);
		if split >= len || ovf {
			self.slice = Default::default();
			return None;
		}
		let end = len - split;
		let (rest, out) = unsafe {
			self.slice
				.get_unchecked(.. end)
				.split_at_unchecked(end - self.width)
		};
		self.slice = rest;
		Some(out)
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		if self.slice.len() < self.width {
			return None;
		}
		let (out, rest) = unsafe { self.slice.split_at_unchecked(self.width) };
		self.slice = rest;
		Some(out)
	}

	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		let len = self.slice.len();
		let (start, ovf) = n.overflowing_mul(self.width);
		if start >= len || ovf {
			self.slice = Default::default();
			return None;
		}
		//  At this point, `start` is at least `self.width` less than `len`.
		let (out, rest) = unsafe {
			self.slice.get_unchecked(start ..).split_at_unchecked(self.width)
		};
		self.slice = rest;
		Some(out)
	}

	fn len(&self) -> usize {
		self.slice.len() / self.width
	}
});

#[derive(Debug)]
#[doc = include_str!("../../doc/slice/iter/RChunksExactMut.md")]
pub struct RChunksExactMut<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The source bit-slice, marked with the alias tainting.
	slice: &'a mut BitSlice<T::Alias, O>,
	/// Any remnant of the source bit-slice that will not be yielded as a chunk.
	extra: &'a mut BitSlice<T::Alias, O>,
	/// The width of the produced chunks.
	width: usize,
}

impl<'a, T, O> RChunksExactMut<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub(super) fn new(slice: &'a mut BitSlice<T, O>, width: usize) -> Self {
		assert_ne!(width, 0, "Chunk width cannot be 0");
		let (extra, slice) =
			unsafe { slice.split_at_unchecked_mut(slice.len() % width) };
		Self {
			slice,
			extra,
			width,
		}
	}

	/// Consumes the iterator, returning the remnant bit-slice that it will not
	/// yield.
	///
	/// ## Original
	///
	/// [`RChunksExactMut::into_remainder`][0]
	///
	/// [0]: core::slice::RChunksExactMut::into_remainder
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn into_remainder(self) -> &'a mut BitSlice<T::Alias, O> {
		self.extra
	}

	/// Takes the remnant bit-slice out of the iterator.
	///
	/// The first time this is called, it will produce the remnant; on each
	/// subsequent call, it will produce an empty bit-slice.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0; 5];
	/// let mut chunks = bits.rchunks_exact_mut(3);
	///
	/// assert_eq!(chunks.take_remainder(), bits![0; 2]);
	/// assert!(chunks.take_remainder().is_empty());
	/// ```
	#[inline]
	pub fn take_remainder(&mut self) -> &'a mut BitSlice<T::Alias, O> {
		mem::take(&mut self.extra)
	}
}

group!(RChunksExactMut => &'a mut BitSlice<T::Alias, O> {
	fn next(&mut self) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		let len = slice.len();
		if len < self.width {
			return None;
		}
		let (rest, out) =
			unsafe { slice.split_at_unchecked_mut_noalias(len - self.width) };
		self.slice = rest;
		Some(out)
	}

	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		let len = slice.len();
		let (split, ovf) = n.overflowing_mul(self.width);
		if split >= len || ovf {
			return None;
		}
		let end = len - split;
		let (rest, out) = unsafe {
			slice.get_unchecked_mut(.. end)
				.split_at_unchecked_mut_noalias(end - self.width)
		};
		self.slice = rest;
		Some(out)
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		if slice.len() < self.width {
			return None;
		}
		let (out, rest) =
			unsafe { slice.split_at_unchecked_mut_noalias(self.width) };
		self.slice = rest;
		Some(out)
	}

	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		let slice = mem::take(&mut self.slice);
		let len = slice.len();
		let (start, ovf) = n.overflowing_mul(self.width);
		if start >= len || ovf {
			return None;
		}
		//  At this point, `start` is at least `self.width` less than `len`.
		let (out, rest) = unsafe {
			slice.get_unchecked_mut(start ..)
				.split_at_unchecked_mut_noalias(self.width)
		};
		self.slice = rest;
		Some(out)
	}

	fn len(&self) -> usize {
		self.slice.len() / self.width
	}
});

/// Creates the `new` function for the easy grouping iterators.
macro_rules! new_group {
	($($t:ident $($m:ident)? $(.$a:ident())?),+ $(,)?) => { $(
		impl<'a, T, O> $t<'a, T, O>
		where
			T: 'a + BitStore,
			O: BitOrder,
		{
			#[inline]
			#[allow(missing_docs, clippy::missing_docs_in_private_items)]
			pub(super) fn new(
				slice: &'a $($m)? BitSlice<T, O>,
				width: usize,
			) -> Self {
				assert_ne!(width, 0, "view width cannot be 0");
				let slice = slice$(.$a())?;
				Self { slice, width }
			}
		}
	)+ };
}

new_group! {
	Windows,
	Chunks,
	ChunksMut mut .alias_mut(),
	RChunks,
	RChunksMut mut .alias_mut(),
}

/// Creates splitting iterators.
macro_rules! split {
	(
		$iter:ident =>
		$item:ty
		$(where $alias:ident)? { $next:item $next_back:item }
	) => {
		impl<'a, T, O, P> $iter<'a, T, O, P>
		where
			T: 'a + BitStore,
			O: BitOrder,
			P: FnMut(usize, &bool) -> bool,
		{
			pub(super) fn new(slice: $item, pred: P) -> Self {
				Self {
					slice,
					pred,
					done: false,
				}
			}
		}

		impl<T, O, P> Debug for $iter<'_, T, O, P>
		where
			T: BitStore,
			O: BitOrder,
			P: FnMut(usize, &bool) -> bool,
		{
			#[inline]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				fmt.debug_struct(stringify!($iter))
					.field("slice", &self.slice)
					.field("done", &self.done)
					.finish()
			}
		}

		impl<'a, T, O, P> Iterator for $iter<'a, T, O, P>
		where
			T: 'a + BitStore,
			O: BitOrder,
			P: FnMut(usize, &bool) -> bool,
		{
			type Item = $item;

			#[inline]
			$next

			#[inline]
			fn size_hint(&self) -> (usize, Option<usize>) {
				if self.done {
					(0, Some(0))
				}
				else {
					(1, Some(self.slice.len() + 1))
				}
			}
		}

		impl<'a, T, O, P> DoubleEndedIterator for $iter<'a, T, O, P>
		where
			T: 'a + BitStore,
			O: BitOrder,
			P: FnMut(usize, &bool) -> bool,
		{
			#[inline]
			$next_back
		}

		impl<'a, T, O, P> FusedIterator for $iter<'a, T, O, P>
		where
			T: 'a + BitStore,
			O: BitOrder,
			P: FnMut(usize, &bool) -> bool,
		{
		}

		impl<'a, T, O, P> SplitIter for $iter<'a, T, O, P>
		where
			T: 'a + BitStore,
			O: BitOrder,
			P: FnMut(usize, &bool) -> bool,
		{
			#[inline]
			fn finish(&mut self) -> Option<Self::Item> {
				if self.done {
					None
				}
				else {
					self.done = true;
					Some(mem::take(&mut self.slice))
				}
			}
		}
	};
}

#[derive(Clone)]
#[doc = include_str!("../../doc/slice/iter/Split.md")]
pub struct Split<'a, T, O, P>
where
	T: 'a + BitStore,
	O: BitOrder,
	P: FnMut(usize, &bool) -> bool,
{
	/// The [`BitSlice`] being split.
	///
	/// [`BitSlice`]: crate::slice::BitSlice
	slice: &'a BitSlice<T, O>,
	/// The function used to test whether a split should occur.
	pred:  P,
	/// Whether the split is finished.
	done:  bool,
}

split!(Split => &'a BitSlice<T, O> {
	fn next(&mut self) -> Option<Self::Item> {
		if self.done {
			return None;
		}
		match self.slice
			.iter()
			.by_refs()
			.enumerate()
			.position(|(idx, bit)| (self.pred)(idx, bit))
		{
			None => self.finish(),
			Some(idx) => unsafe {
				let out = self.slice.get_unchecked(.. idx);
				self.slice = self.slice.get_unchecked(idx + 1 ..);
				Some(out)
			},
		}
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		if self.done {
			return None;
		}
		match self.slice
			.iter()
			.by_refs()
			.enumerate()
			.rposition(|(idx, bit)| (self.pred)(idx, bit))
		{
			None => self.finish(),
			Some(idx) => unsafe {
				let out = self.slice.get_unchecked(idx + 1 ..);
				self.slice = self.slice.get_unchecked(.. idx);
				Some(out)
			},
		}
	}
});

#[doc = include_str!("../../doc/slice/iter/SplitMut.md")]
pub struct SplitMut<'a, T, O, P>
where
	T: 'a + BitStore,
	O: BitOrder,
	P: FnMut(usize, &bool) -> bool,
{
	/// The source bit-slice, marked with the alias tainting.
	slice: &'a mut BitSlice<T::Alias, O>,
	/// The function that tests each bit for whether it is a split point.
	pred:  P,
	/// Marks whether iteration has concluded, without emptying the `slice`.
	done:  bool,
}

split!(SplitMut => &'a mut BitSlice<T::Alias, O> {
	fn next(&mut self) -> Option<Self::Item> {
		if self.done {
			return None;
		}
		let idx_opt = {
			let pred = &mut self.pred;
			self.slice
				.iter()
				.by_refs()
				.enumerate()
				.position(|(idx, bit)| (pred)(idx, bit))
		};
		match idx_opt
		{
			None => self.finish(),
			Some(idx) => unsafe {
				let slice = mem::take(&mut self.slice);
				let (out, rest) = slice.split_at_unchecked_mut_noalias(idx);
				self.slice = rest.get_unchecked_mut(1 ..);
				Some(out)
			},
		}
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		if self.done {
			return None;
		}
		let idx_opt = {
			let pred = &mut self.pred;
			self.slice
				.iter()
				.by_refs()
				.enumerate()
				.rposition(|(idx, bit)| (pred)(idx, bit))
		};
		match idx_opt
		{
			None => self.finish(),
			Some(idx) => unsafe {
				let slice = mem::take(&mut self.slice);
				let (rest, out) = slice.split_at_unchecked_mut_noalias(idx);
				self.slice = rest;
				Some(out.get_unchecked_mut(1 ..))
			},
		}
	}
});

#[derive(Clone)]
#[doc = include_str!("../../doc/slice/iter/SplitInclusive.md")]
pub struct SplitInclusive<'a, T, O, P>
where
	T: 'a + BitStore,
	O: BitOrder,
	P: FnMut(usize, &bool) -> bool,
{
	/// The source bit-slice.
	slice: &'a BitSlice<T, O>,
	/// The function that tests each bit for whether it is a split point.
	pred:  P,
	/// Marks whether iteration has concluded, without emptying the `slice`.
	done:  bool,
}

split!(SplitInclusive => &'a BitSlice<T, O> {
	fn next(&mut self) -> Option<Self::Item> {
		if self.done {
			return None;
		}
		let len = self.slice.len();
		let idx = self.slice.iter()
			.by_refs()
			.enumerate()
			.position(|(idx, bit)| (self.pred)(idx, bit))
			.map(|idx| idx + 1)
			.unwrap_or(len);
		if idx == len {
			self.done = true;
		}
		let (out, rest) = unsafe { self.slice.split_at_unchecked(idx) };
		self.slice = rest;
		Some(out)
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		if self.done {
			return None;
		}

		let idx = if self.slice.is_empty() {
			0
		}
		else {
			unsafe { self.slice.get_unchecked(.. self.slice.len() - 1) }
				.iter()
				.by_refs()
				.enumerate()
				.rposition(|(idx, bit)| (self.pred)(idx, bit))
				.map(|idx| idx + 1)
				.unwrap_or(0)
		};
		if idx == 0 {
			self.done = true;
		}
		let (rest, out) = unsafe { self.slice.split_at_unchecked(idx) };
		self.slice = rest;
		Some(out)
	}
});

#[doc = include_str!("../../doc/slice/iter/SplitInclusiveMut.md")]
pub struct SplitInclusiveMut<'a, T, O, P>
where
	T: 'a + BitStore,
	O: BitOrder,
	P: FnMut(usize, &bool) -> bool,
{
	/// The source bit-slice, marked with the alias tainting.
	slice: &'a mut BitSlice<T::Alias, O>,
	/// The function that tests each bit for whether it is a split point.
	pred:  P,
	/// Marks whether iteration has concluded, without emptying the `slice`.
	done:  bool,
}

split!(SplitInclusiveMut => &'a mut BitSlice<T::Alias, O> {
	fn next(&mut self) -> Option<Self::Item> {
		if self.done {
			return None;
		}
		let pred = &mut self.pred;
		let len = self.slice.len();
		let idx = self.slice.iter()
			.by_refs()
			.enumerate()
			.position(|(idx, bit)| (pred)(idx, bit))
			.map(|idx| idx + 1)
			.unwrap_or(len);
		if idx == len {
			self.done = true;
		}
		let (out, rest) = unsafe {
			mem::take(&mut self.slice)
				.split_at_unchecked_mut_noalias(idx)
		};
		self.slice = rest;
		Some(out)
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		if self.done {
			return None;
		}
		let pred = &mut self.pred;
		let idx = if self.slice.is_empty() {
			0
		}
		else {
			unsafe { self.slice.get_unchecked(.. self.slice.len() - 1) }
				.iter()
				.by_refs()
				.enumerate()
				.rposition(|(idx, bit)| (pred)(idx, bit))
				.map(|idx| idx + 1)
				.unwrap_or(0)
		};
		if idx == 0 {
			self.done = true;
		}
		let (rest, out) = unsafe {
			mem::take(&mut self.slice)
				.split_at_unchecked_mut_noalias(idx)
		};
		self.slice = rest;
		Some(out)
	}
});

#[derive(Clone)]
#[doc = include_str!("../../doc/slice/iter/RSplit.md")]
pub struct RSplit<'a, T, O, P>
where
	T: 'a + BitStore,
	O: BitOrder,
	P: FnMut(usize, &bool) -> bool,
{
	/// The source bit-slice.
	slice: &'a BitSlice<T, O>,
	/// The function that tests each bit for whether it is a split point.
	pred:  P,
	/// Marks whether iteration has concluded, without emptying the `slice`.
	done:  bool,
}

split!(RSplit => &'a BitSlice<T, O> {
	fn next(&mut self) -> Option<Self::Item> {
		let mut split = Split::<'a, T, O, &mut P> {
			slice: mem::take(&mut self.slice),
			pred: &mut self.pred,
			done: self.done,
		};
		let out = split.next_back();
		let Split { slice, done, .. } = split;
		self.slice = slice;
		self.done = done;
		out
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		let mut split = Split::<'a, T, O, &mut P> {
			slice: mem::take(&mut self.slice),
			pred: &mut self.pred,
			done: self.done,
		};
		let out = split.next();
		let Split { slice, done, .. } = split;
		self.slice = slice;
		self.done = done;
		out
	}
});

#[doc = include_str!("../../doc/slice/iter/RSplitMut.md")]
pub struct RSplitMut<'a, T, O, P>
where
	T: 'a + BitStore,
	O: BitOrder,
	P: FnMut(usize, &bool) -> bool,
{
	/// The source bit-slice, marked with the alias tainting.
	slice: &'a mut BitSlice<T::Alias, O>,
	/// The function that tests each bit for whether it is a split point.
	pred:  P,
	/// Marks whether iteration has concluded, without emptying the `slice`.
	done:  bool,
}

split!(RSplitMut => &'a mut BitSlice<T::Alias, O> {
	fn next(&mut self) -> Option<Self::Item> {
		let mut split = SplitMut::<'a, T, O, &mut P> {
			slice: mem::take(&mut self.slice),
			pred: &mut self.pred,
			done: self.done,
		};
		let out = split.next_back();
		let SplitMut { slice, done, .. } = split;
		self.slice = slice;
		self.done = done;
		out
	}

	fn next_back(&mut self) -> Option<Self::Item> {
		let mut split = SplitMut::<'a, T, O, &mut P> {
			slice: mem::take(&mut self.slice),
			pred: &mut self.pred,
			done: self.done,
		};
		let out = split.next();
		let SplitMut { slice, done, .. } = split;
		self.slice = slice;
		self.done = done;
		out
	}
});

/// [Original](https://github.com/rust-lang/rust/blob/95750ae/library/core/src/slice/iter.rs#L318-L325)
trait SplitIter: DoubleEndedIterator {
	/// Marks the underlying iterator as complete, and extracts the remaining
	/// portion of the bit-slice.
	fn finish(&mut self) -> Option<Self::Item>;
}

#[derive(Clone)]
#[doc = include_str!("../../doc/slice/iter/SplitN.md")]
pub struct SplitN<'a, T, O, P>
where
	T: 'a + BitStore,
	O: BitOrder,
	P: FnMut(usize, &bool) -> bool,
{
	/// The interior splitter.
	inner: Split<'a, T, O, P>,
	/// The number of permissible splits remaining.
	count: usize,
}

#[doc = include_str!("../../doc/slice/iter/SplitNMut.md")]
pub struct SplitNMut<'a, T, O, P>
where
	T: 'a + BitStore,
	O: BitOrder,
	P: FnMut(usize, &bool) -> bool,
{
	/// The interior splitter.
	inner: SplitMut<'a, T, O, P>,
	/// The number of permissible splits remaining.
	count: usize,
}

#[derive(Clone)]
#[doc = include_str!("../../doc/slice/iter/RSplitN.md")]
pub struct RSplitN<'a, T, O, P>
where
	T: 'a + BitStore,
	O: BitOrder,
	P: FnMut(usize, &bool) -> bool,
{
	/// The interior splitter.
	inner: RSplit<'a, T, O, P>,
	/// The number of permissible splits remaining.
	count: usize,
}

#[doc = include_str!("../../doc/slice/iter/RSplitNMut.md")]
pub struct RSplitNMut<'a, T, O, P>
where
	T: 'a + BitStore,
	O: BitOrder,
	P: FnMut(usize, &bool) -> bool,
{
	/// The interior splitter.
	inner: RSplitMut<'a, T, O, P>,
	/// The number of permissible splits remaining.
	count: usize,
}

/// Creates a splitting iterator with a maximum number of attempts.
macro_rules! split_n {
	($(
		$outer:ident => $inner:ident => $item:ty $(where $alias:ident)?
	);+ $(;)?) => { $(
		impl<'a, T, O, P> $outer<'a, T, O, P>
		where
			T: 'a + BitStore,
			O: BitOrder,
			P: FnMut(usize, &bool) -> bool,
		{
			#[inline]
			#[allow(missing_docs, clippy::missing_docs_in_private_items)]
			pub(super) fn new(
				slice: $item,
				pred: P,
				count: usize,
			) -> Self {
				Self {
					inner: <$inner<'a, T, O, P>>::new(slice, pred),
					count,
				}
			}
		}

		impl<T, O, P> Debug for $outer<'_, T, O, P>
		where
			T: BitStore,
			O: BitOrder,
			P: FnMut(usize, &bool) -> bool
		{
			#[inline]
			fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
				fmt.debug_struct(stringify!($outer))
					.field("slice", &self.inner.slice)
					.field("count", &self.count)
					.finish()
			}
		}

		impl<'a, T, O, P> Iterator for $outer<'a, T, O, P>
		where
			T: 'a + BitStore,
			O: BitOrder,
			P: FnMut(usize, &bool) -> bool,
			$( T::$alias: radium::Radium<<<T as BitStore>::Alias as BitStore>::Mem>, )?
		{
			type Item = <$inner <'a, T, O, P> as Iterator>::Item;

			#[inline]
			fn next(&mut self) -> Option<Self::Item> {
				match self.count {
					0 => None,
					1 => {
						self.count -= 1;
						self.inner.finish()
					},
					_ => {
						self.count -= 1;
						self.inner.next()
					},
				}
			}

			#[inline]
			fn size_hint(&self) -> (usize, Option<usize>) {
				let (low, hi) = self.inner.size_hint();
				(low, hi.map(|h| cmp::min(h, self.count)).or(Some(self.count)))
			}
		}

		impl<T, O, P> FusedIterator for $outer<'_, T, O, P>
		where
			T: BitStore,
			O: BitOrder,
			P: FnMut(usize, &bool) -> bool,
			$( T::$alias: radium::Radium<<<T as BitStore>::Alias as BitStore>::Mem>, )?
		{
		}
	)+ };
}

split_n! {
	SplitN => Split => &'a BitSlice<T, O>;
	SplitNMut => SplitMut => &'a mut BitSlice<T::Alias, O>;
	RSplitN => RSplit => &'a BitSlice<T, O>;
	RSplitNMut => RSplitMut => &'a mut BitSlice<T::Alias, O>;
}

#[derive(Clone, Copy, Debug, Eq, Ord, PartialEq, PartialOrd)]
#[doc = include_str!("../../doc/slice/iter/IterOnes.md")]
pub struct IterOnes<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The remaining bit-slice whose `1` bits are to be found.
	inner: &'a BitSlice<T, O>,
	/// The offset from the front of the original bit-slice to the current
	/// `.inner`.
	front: usize,
}

impl<'a, T, O> IterOnes<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	#[inline]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub(super) fn new(slice: &'a BitSlice<T, O>) -> Self {
		Self {
			inner: slice,
			front: 0,
		}
	}
}

impl<T, O> Default for IterOnes<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn default() -> Self {
		Self {
			inner: Default::default(),
			front: 0,
		}
	}
}

impl<T, O> Iterator for IterOnes<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Item = usize;

	easy_iter!();

	#[inline]
	fn next(&mut self) -> Option<Self::Item> {
		let pos = if let Some(bits) = self.inner.coerce::<T, Lsb0>() {
			bits.sp_first_one()
		}
		else if let Some(bits) = self.inner.coerce::<T, Msb0>() {
			bits.sp_first_one()
		}
		else {
			self.inner.iter().by_vals().position(|b| b)
		};

		match pos {
			Some(n) => {
				//  Split at the index *past* the discovered bit. This is always
				//  safe, as `split_at(len)` produces `(self, [])`.
				let (_, rest) = unsafe { self.inner.split_at_unchecked(n + 1) };
				self.inner = rest;
				let out = self.front + n;
				//  Search resumes from the next index after the found position.
				self.front = out + 1;
				Some(out)
			},
			None => {
				*self = Default::default();
				None
			},
		}
	}
}

impl<T, O> DoubleEndedIterator for IterOnes<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn next_back(&mut self) -> Option<Self::Item> {
		let pos = if let Some(bits) = self.inner.coerce::<T, Lsb0>() {
			bits.sp_last_one()
		}
		else if let Some(bits) = self.inner.coerce::<T, Msb0>() {
			bits.sp_last_one()
		}
		else {
			self.inner.iter().by_vals().rposition(|b| b)
		};

		match pos {
			Some(n) => {
				let (rest, _) = unsafe { self.inner.split_at_unchecked(n) };
				self.inner = rest;
				Some(self.front + n)
			},
			None => {
				*self = Default::default();
				None
			},
		}
	}
}

impl<T, O> ExactSizeIterator for IterOnes<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn len(&self) -> usize {
		self.inner.count_ones()
	}
}

impl<T, O> FusedIterator for IterOnes<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

#[doc = include_str!("../../doc/slice/iter/IterZeros.md")]
#[derive(Clone, Copy, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub struct IterZeros<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// The remaining bit-slice whose `0` bits are to be found.
	inner: &'a BitSlice<T, O>,
	/// The offset from the front of the original bit-slice to the current
	/// `.inner`.
	front: usize,
}

impl<'a, T, O> IterZeros<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub(super) fn new(slice: &'a BitSlice<T, O>) -> Self {
		Self {
			inner: slice,
			front: 0,
		}
	}
}

impl<T, O> Default for IterZeros<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn default() -> Self {
		Self {
			inner: Default::default(),
			front: 0,
		}
	}
}

impl<T, O> Iterator for IterZeros<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Item = usize;

	easy_iter!();

	#[inline]
	fn next(&mut self) -> Option<Self::Item> {
		let pos = if let Some(bits) = self.inner.coerce::<T, Lsb0>() {
			bits.sp_first_zero()
		}
		else if let Some(bits) = self.inner.coerce::<T, Msb0>() {
			bits.sp_first_zero()
		}
		else {
			self.inner.iter().by_vals().position(|b| !b)
		};

		match pos {
			Some(n) => {
				let (_, rest) = unsafe { self.inner.split_at_unchecked(n + 1) };
				self.inner = rest;
				let out = self.front + n;
				self.front = out + 1;
				Some(out)
			},
			None => {
				*self = Default::default();
				None
			},
		}
	}
}

impl<T, O> DoubleEndedIterator for IterZeros<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn next_back(&mut self) -> Option<Self::Item> {
		let pos = if let Some(bits) = self.inner.coerce::<T, Lsb0>() {
			bits.sp_last_zero()
		}
		else if let Some(bits) = self.inner.coerce::<T, Msb0>() {
			bits.sp_last_zero()
		}
		else {
			self.inner.iter().by_vals().rposition(|b| !b)
		};

		match pos {
			Some(n) => {
				let (rest, _) = unsafe { self.inner.split_at_unchecked(n) };
				self.inner = rest;
				Some(self.front + n)
			},
			None => {
				*self = Default::default();
				None
			},
		}
	}
}

impl<T, O> ExactSizeIterator for IterZeros<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn len(&self) -> usize {
		self.inner.count_zeros()
	}
}

impl<T, O> FusedIterator for IterZeros<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

/* This macro has some very obnoxious call syntax that is necessary to handle
the different iteration protocols used above.

The `Split` iterators are not `DoubleEndedIterator` or `ExactSizeIterator`, and
must be excluded from those implementations. However, bounding on `DEI` causes
`.next_back()` and `.nth_back()` to return opaque associated types, rather than
the return type from the directly-resolved signatures. As such, the item type of
the source iterator must also be provided so that methods on it can be named.
*/
/// Creates wrappers that unsafely remove one layer of `::Alias` tainting.
macro_rules! noalias {
	($(
		$from:ident $(($p:ident))?
		=> $alias:ty
		=> $to:ident
		=> $item:ty
		=> $map:path;
	)+) => { $(
		#[repr(transparent)]
		#[doc = include_str!("../../doc/slice/iter/NoAlias.md")]
		pub struct $to<'a, T, O$(, $p)?>
		where
			T: 'a + BitStore,
			O: BitOrder,
			$($p: FnMut(usize, &bool) -> bool,)?
		{
			/// The actual iterator that this wraps.
			inner: $from<'a, T, O$(, $p)?>,
		}

		impl<'a, T, O$(, $p)?> $from<'a, T, O$(, $p)?>
		where
			T: 'a + BitStore,
			O: BitOrder,
			$($p: FnMut(usize, &bool) -> bool,)?
		{
			/// Removes a layer of `::Alias` tainting from the yielded item.
			///
			/// ## Safety
			///
			/// You *must* consume the adapted iterator in a loop that does not
			/// allow multiple yielded items to exist in the same scope. Each
			/// yielded item must have a completely non-overlapping lifetime
			/// from all the others.
			///
			/// The items yielded by this iterator will not have an additional
			/// alias marker applied to them, so their use in an iteration
			/// sequence will not be penalized when the surrounding code ensures
			/// that each item yielded by the iterator is destroyed before the
			/// next is produced.
			///
			/// This adapter does **not** convert the iterator to use the
			/// [`T::Mem`] raw underlying type, as it can be applied to an
			/// iterator over an already-aliased bit-slice and must preserve the
			/// initial condition. Its *only* effect is to remove the additional
			/// [`T::Alias`] marker imposed by the mutable iterators.
			///
			/// Violating this requirement causes memory-unsafety and breaks
			/// Rust’s data-race guarantees.
			///
			/// [`T::Alias`]: crate::store::BitStore::Alias
			/// [`T::Mem`]: crate::store::BitStore::Mem
			#[inline]
			#[must_use = "You must consume this object, preferably immediately \
			              upon creation"]
			pub unsafe fn remove_alias(self) -> $to<'a, T, O$(, $p)?> {
				$to { inner: self }
			}
		}

		impl<'a, T, O$(, $p)?> Iterator for $to<'a, T, O$(, $p)?>
		where
			T: 'a + BitStore,
			O: BitOrder,
			$($p: FnMut(usize, &bool) -> bool,)?
		{
			type Item = $item;

			#[inline]
			fn next(&mut self) -> Option<Self::Item> {
				self.inner.next().map(|item| unsafe { $map(item) })
			}

			#[inline]
			fn nth(&mut self, n: usize) -> Option<Self::Item> {
				self.inner.nth(n).map(|item| unsafe { $map(item) })
			}

			#[inline]
			fn size_hint(&self) -> (usize, Option<usize>) {
				self.inner.size_hint()
			}

			#[inline]
			fn count(self) -> usize {
				self.inner.count()
			}

			#[inline]
			fn last(self) -> Option<Self::Item> {
				self.inner.last().map(|item| unsafe { $map(item) })
			}
		}

		impl<'a, T, O$(, $p)?> DoubleEndedIterator for $to<'a, T, O$(, $p)?>
		where
			T: 'a + BitStore,
			O: BitOrder,
			$($p: FnMut(usize, &bool) -> bool,)?
			$from<'a, T, O$(, $p)?>: DoubleEndedIterator<Item = $alias>,
		{
			#[inline]
			fn next_back(&mut self) -> Option<Self::Item> {
				self.inner.next_back().map(|item| unsafe { $map(item) })
			}

			#[inline]
			fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
				self.inner.nth_back(n).map(|item| unsafe { $map(item) })
			}
		}

		impl<'a, T, O$(, $p)?> ExactSizeIterator for $to<'a, T, O$(, $p)?>
		where
			T: 'a + BitStore,
			O: BitOrder,
			$($p: FnMut(usize, &bool) -> bool,)?
			$from<'a, T, O$(, $p)?>: ExactSizeIterator,
		{
			#[inline]
			fn len(&self) -> usize {
				self.inner.len()
			}
		}

		impl<'a, T, O$(, $p)?> FusedIterator for $to<'a, T, O$(, $p)?>
		where
			T: 'a + BitStore,
			O: BitOrder,
			$($p: FnMut(usize, &bool) -> bool,)?
			$from<'a, T, O$(, $p)?>: FusedIterator,
		{
		}
	)+ };
}

noalias! {
	IterMut => <usize as BitSliceIndex<'a, T::Alias, O>>::Mut
	=> IterMutNoAlias => <usize as BitSliceIndex<'a, T, O>>::Mut
	=> BitRef::remove_alias;

	ChunksMut => &'a mut BitSlice<T::Alias, O>
	=> ChunksMutNoAlias => &'a mut BitSlice<T, O>
	=> BitSlice::unalias_mut;

	ChunksExactMut => &'a mut BitSlice<T::Alias, O>
	=> ChunksExactMutNoAlias => &'a mut BitSlice<T, O>
	=> BitSlice::unalias_mut;

	RChunksMut => &'a mut BitSlice<T::Alias, O>
	=> RChunksMutNoAlias => &'a mut BitSlice<T, O>
	=> BitSlice::unalias_mut;

	RChunksExactMut => &'a mut BitSlice<T::Alias, O>
	=> RChunksExactMutNoAlias => &'a mut BitSlice<T, O>
	=> BitSlice::unalias_mut;

	SplitMut (P) => &'a mut BitSlice<T::Alias, O>
	=> SplitMutNoAlias => &'a mut BitSlice<T, O>
	=> BitSlice::unalias_mut;

	SplitInclusiveMut (P) => &'a mut BitSlice<T::Alias, O>
	=> SplitInclusiveMutNoAlias => &'a mut BitSlice<T, O>
	=> BitSlice::unalias_mut;

	RSplitMut (P) => &'a mut BitSlice<T::Alias, O>
	=> RSplitMutNoAlias => &'a mut BitSlice<T, O>
	=> BitSlice::unalias_mut;

	SplitNMut (P) => &'a mut BitSlice<T::Alias, O>
	=> SplitNMutNoAlias => &'a mut BitSlice<T, O>
	=> BitSlice::unalias_mut;

	RSplitNMut (P) => &'a mut BitSlice<T::Alias, O>
	=> RSplitNMutNoAlias => &'a mut BitSlice<T, O>
	=> BitSlice::unalias_mut;
}

impl<'a, T, O> ChunksExactMutNoAlias<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// See [`ChunksExactMut::into_remainder()`][0].
	///
	/// [0]: crate::slice::ChunksExactMut::into_remainder
	#[inline]
	pub fn into_remainder(self) -> &'a mut BitSlice<T, O> {
		unsafe { BitSlice::unalias_mut(self.inner.into_remainder()) }
	}

	/// See [`ChunksExactMut::take_remainder()`][0]
	///
	/// [0]: crate::slice::ChunksExactMut::take_remainder
	#[inline]
	pub fn take_remainder(&mut self) -> &'a mut BitSlice<T, O> {
		unsafe { BitSlice::unalias_mut(self.inner.take_remainder()) }
	}
}

impl<'a, T, O> RChunksExactMutNoAlias<'a, T, O>
where
	T: 'a + BitStore,
	O: BitOrder,
{
	/// See [`RChunksExactMut::into_remainder()`][0]
	///
	/// [0]: crate::slice::RChunksExactMut::into_remainder
	#[inline]
	pub fn into_remainder(self) -> &'a mut BitSlice<T, O> {
		unsafe { BitSlice::unalias_mut(self.inner.into_remainder()) }
	}

	/// See [`RChunksExactMut::take_remainder()`][0]
	///
	/// [0]:  crate::slice::RChunksExactMut::take_remainder
	#[inline]
	pub fn take_remainder(&mut self) -> &'a mut BitSlice<T, O> {
		unsafe { BitSlice::unalias_mut(self.inner.take_remainder()) }
	}
}
