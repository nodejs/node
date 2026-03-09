#![doc = include_str!("../../doc/slice/api.md")]

use core::{
	cmp,
	ops::{
		Range,
		RangeFrom,
		RangeFull,
		RangeInclusive,
		RangeTo,
		RangeToInclusive,
	},
};

use wyz::{
	comu::{
		Const,
		Mut,
	},
	range::RangeExt,
};

use super::{
	BitSlice,
	Chunks,
	ChunksExact,
	ChunksExactMut,
	ChunksMut,
	Iter,
	IterMut,
	RChunks,
	RChunksExact,
	RChunksExactMut,
	RChunksMut,
	RSplit,
	RSplitMut,
	RSplitN,
	RSplitNMut,
	Split,
	SplitInclusive,
	SplitInclusiveMut,
	SplitMut,
	SplitN,
	SplitNMut,
	Windows,
};
#[cfg(feature = "alloc")]
use crate::vec::BitVec;
use crate::{
	array::BitArray,
	domain::Domain,
	mem::{
		self,
		BitRegister,
	},
	order::BitOrder,
	ptr::{
		BitPtr,
		BitRef,
		BitSpan,
		BitSpanError,
	},
	store::BitStore,
};

/// Port of the `[T]` inherent API.
impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Gets the number of bits in the bit-slice.
	///
	/// ## Original
	///
	/// [`slice::len`](https://doc.rust-lang.org/std/primitive.slice.html#method.len)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert_eq!(bits![].len(), 0);
	/// assert_eq!(bits![0; 10].len(), 10);
	/// ```
	#[inline]
	pub fn len(&self) -> usize {
		self.as_bitspan().len()
	}

	/// Tests if the bit-slice is empty (length zero).
	///
	/// ## Original
	///
	/// [`slice::is_empty`](https://doc.rust-lang.org/std/primitive.slice.html#method.is_empty)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!(bits![].is_empty());
	/// assert!(!bits![0; 10].is_empty());
	/// ```
	#[inline]
	pub fn is_empty(&self) -> bool {
		self.len() == 0
	}

	/// Gets a reference to the first bit of the bit-slice, or `None` if it is
	/// empty.
	///
	/// ## Original
	///
	/// [`slice::first`](https://doc.rust-lang.org/std/primitive.slice.html#method.first)
	///
	/// ## API Differences
	///
	/// `bitvec` uses a custom structure for both read-only and mutable
	/// references to `bool`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![1, 0, 0];
	/// assert_eq!(bits.first().as_deref(), Some(&true));
	///
	/// assert!(bits![].first().is_none());
	/// ```
	#[inline]
	pub fn first(&self) -> Option<BitRef<Const, T, O>> {
		self.get(0)
	}

	/// Gets a mutable reference to the first bit of the bit-slice, or `None` if
	/// it is empty.
	///
	/// ## Original
	///
	/// [`slice::first_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.first_mut)
	///
	/// ## API Differences
	///
	/// `bitvec` uses a custom structure for both read-only and mutable
	/// references to `bool`. This must be bound as `mut` in order to write
	/// through it.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0; 3];
	/// if let Some(mut first) = bits.first_mut() {
	///   *first = true;
	/// }
	/// assert_eq!(bits, bits![1, 0, 0]);
	///
	/// assert!(bits![mut].first_mut().is_none());
	/// ```
	#[inline]
	pub fn first_mut(&mut self) -> Option<BitRef<Mut, T, O>> {
		self.get_mut(0)
	}

	/// Splits the bit-slice into a reference to its first bit, and the rest of
	/// the bit-slice. Returns `None` when empty.
	///
	/// ## Original
	///
	/// [`slice::split_first`](https://doc.rust-lang.org/std/primitive.slice.html#method.split_first)
	///
	/// ## API Differences
	///
	/// `bitvec` uses a custom structure for both read-only and mutable
	/// references to `bool`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![1, 0, 0];
	/// let (first, rest) = bits.split_first().unwrap();
	/// assert_eq!(first, &true);
	/// assert_eq!(rest, bits![0; 2]);
	/// ```
	#[inline]
	pub fn split_first(&self) -> Option<(BitRef<Const, T, O>, &Self)> {
		match self.len() {
			0 => None,
			_ => unsafe {
				let (head, rest) = self.split_at_unchecked(1);
				Some((head.get_unchecked(0), rest))
			},
		}
	}

	/// Splits the bit-slice into mutable references of its first bit, and the
	/// rest of the bit-slice. Returns `None` when empty.
	///
	/// ## Original
	///
	/// [`slice::split_first_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.split_first_mut)
	///
	/// ## API Differences
	///
	/// `bitvec` uses a custom structure for both read-only and mutable
	/// references to `bool`. This must be bound as `mut` in order to write
	/// through it.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0; 3];
	/// if let Some((mut first, rest)) = bits.split_first_mut() {
	///   *first = true;
	///   assert_eq!(rest, bits![0; 2]);
	/// }
	/// assert_eq!(bits, bits![1, 0, 0]);
	/// ```
	#[inline]
	pub fn split_first_mut(
		&mut self,
	) -> Option<(BitRef<Mut, T::Alias, O>, &mut BitSlice<T::Alias, O>)> {
		match self.len() {
			0 => None,
			_ => unsafe {
				let (head, rest) = self.split_at_unchecked_mut(1);
				Some((head.get_unchecked_mut(0), rest))
			},
		}
	}

	/// Splits the bit-slice into a reference to its last bit, and the rest of
	/// the bit-slice. Returns `None` when empty.
	///
	/// ## Original
	///
	/// [`slice::split_last`](https://doc.rust-lang.org/std/primitive.slice.html#method.split_last)
	///
	/// ## API Differences
	///
	/// `bitvec` uses a custom structure for both read-only and mutable
	/// references to `bool`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 0, 1];
	/// let (last, rest) = bits.split_last().unwrap();
	/// assert_eq!(last, &true);
	/// assert_eq!(rest, bits![0; 2]);
	/// ```
	#[inline]
	pub fn split_last(&self) -> Option<(BitRef<Const, T, O>, &Self)> {
		match self.len() {
			0 => None,
			n => unsafe {
				let (rest, tail) = self.split_at_unchecked(n - 1);
				Some((tail.get_unchecked(0), rest))
			},
		}
	}

	/// Splits the bit-slice into mutable references to its last bit, and the
	/// rest of the bit-slice. Returns `None` when empty.
	///
	/// ## Original
	///
	/// [`slice::split_last_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.split_last_mut)
	///
	/// ## API Differences
	///
	/// `bitvec` uses a custom structure for both read-only and mutable
	/// references to `bool`. This must be bound as `mut` in order to write
	/// through it.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0; 3];
	/// if let Some((mut last, rest)) = bits.split_last_mut() {
	///   *last = true;
	///   assert_eq!(rest, bits![0; 2]);
	/// }
	/// assert_eq!(bits, bits![0, 0, 1]);
	/// ```
	#[inline]
	pub fn split_last_mut(
		&mut self,
	) -> Option<(BitRef<Mut, T::Alias, O>, &mut BitSlice<T::Alias, O>)> {
		match self.len() {
			0 => None,
			n => unsafe {
				let (rest, tail) = self.split_at_unchecked_mut(n - 1);
				Some((tail.get_unchecked_mut(0), rest))
			},
		}
	}

	/// Gets a reference to the last bit of the bit-slice, or `None` if it is
	/// empty.
	///
	/// ## Original
	///
	/// [`slice::last`](https://doc.rust-lang.org/std/primitive.slice.html#method.last)
	///
	/// ## API Differences
	///
	/// `bitvec` uses a custom structure for both read-only and mutable
	/// references to `bool`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 0, 1];
	/// assert_eq!(bits.last().as_deref(), Some(&true));
	///
	/// assert!(bits![].last().is_none());
	/// ```
	#[inline]
	pub fn last(&self) -> Option<BitRef<Const, T, O>> {
		match self.len() {
			0 => None,
			n => Some(unsafe { self.get_unchecked(n - 1) }),
		}
	}

	/// Gets a mutable reference to the last bit of the bit-slice, or `None` if
	/// it is empty.
	///
	/// ## Original
	///
	/// [`slice::last_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.last_mut)
	///
	/// ## API Differences
	///
	/// `bitvec` uses a custom structure for both read-only and mutable
	/// references to `bool`. This must be bound as `mut` in order to write
	/// through it.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0; 3];
	/// if let Some(mut last) = bits.last_mut() {
	///   *last = true;
	/// }
	/// assert_eq!(bits, bits![0, 0, 1]);
	///
	/// assert!(bits![mut].last_mut().is_none());
	/// ```
	#[inline]
	pub fn last_mut(&mut self) -> Option<BitRef<Mut, T, O>> {
		match self.len() {
			0 => None,
			n => Some(unsafe { self.get_unchecked_mut(n - 1) }),
		}
	}

	/// Gets a reference to a single bit or a subsection of the bit-slice,
	/// depending on the type of `index`.
	///
	/// - If given a `usize`, this produces a reference structure to the `bool`
	///   at the position.
	/// - If given any form of range, this produces a smaller bit-slice.
	///
	/// This returns `None` if the `index` departs the bounds of `self`.
	///
	/// ## Original
	///
	/// [`slice::get`](https://doc.rust-lang.org/std/primitive.slice.html#method.get)
	///
	/// ## API Differences
	///
	/// `BitSliceIndex` uses discrete types for immutable and mutable
	/// references, rather than a single referent type.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0];
	/// assert_eq!(bits.get(1).as_deref(), Some(&true));
	/// assert_eq!(bits.get(0 .. 2), Some(bits![0, 1]));
	/// assert!(bits.get(3).is_none());
	/// assert!(bits.get(0 .. 4).is_none());
	/// ```
	#[inline]
	pub fn get<'a, I>(&'a self, index: I) -> Option<I::Immut>
	where I: BitSliceIndex<'a, T, O> {
		index.get(self)
	}

	/// Gets a mutable reference to a single bit or a subsection of the
	/// bit-slice, depending on the type of `index`.
	///
	/// - If given a `usize`, this produces a reference structure to the `bool`
	///   at the position.
	/// - If given any form of range, this produces a smaller bit-slice.
	///
	/// This returns `None` if the `index` departs the bounds of `self`.
	///
	/// ## Original
	///
	/// [`slice::get_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.get_mut)
	///
	/// ## API Differences
	///
	/// `BitSliceIndex` uses discrete types for immutable and mutable
	/// references, rather than a single referent type.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0; 3];
	///
	/// *bits.get_mut(0).unwrap() = true;
	/// bits.get_mut(1 ..).unwrap().fill(true);
	/// assert_eq!(bits, bits![1; 3]);
	/// ```
	#[inline]
	pub fn get_mut<'a, I>(&'a mut self, index: I) -> Option<I::Mut>
	where I: BitSliceIndex<'a, T, O> {
		index.get_mut(self)
	}

	/// Gets a reference to a single bit or to a subsection of the bit-slice,
	/// without bounds checking.
	///
	/// This has the same arguments and behavior as [`.get()`], except that it
	/// does not check that `index` is in bounds.
	///
	/// ## Original
	///
	/// [`slice::get_unchecked`](https://doc.rust-lang.org/std/primitive.slice.html#method.get_unchecked)
	///
	/// ## Safety
	///
	/// You must ensure that `index` is within bounds (within the range `0 ..
	/// self.len()`), or this method will introduce memory safety and/or
	/// undefined behavior.
	///
	/// It is library-level undefined behavior to index beyond the length of any
	/// bit-slice, even if you **know** that the offset remains within an
	/// allocation as measured by Rust or LLVM.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let data = 0b0001_0010u8;
	/// let bits = &data.view_bits::<Lsb0>()[.. 3];
	///
	/// unsafe {
	///   assert!(bits.get_unchecked(1));
	///   assert!(bits.get_unchecked(4));
	/// }
	/// ```
	///
	/// [`.get()`]: Self::get
	#[inline]
	pub unsafe fn get_unchecked<'a, I>(&'a self, index: I) -> I::Immut
	where I: BitSliceIndex<'a, T, O> {
		index.get_unchecked(self)
	}

	/// Gets a mutable reference to a single bit or a subsection of the
	/// bit-slice, depending on the type of `index`.
	///
	/// This has the same arguments and behavior as [`.get_mut()`], except that
	/// it does not check that `index` is in bounds.
	///
	/// ## Original
	///
	/// [`slice::get_unchecked_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.get_unchecked_mut)
	///
	/// ## Safety
	///
	/// You must ensure that `index` is within bounds (within the range `0 ..
	/// self.len()`), or this method will introduce memory safety and/or
	/// undefined behavior.
	///
	/// It is library-level undefined behavior to index beyond the length of any
	/// bit-slice, even if you **know** that the offset remains within an
	/// allocation as measured by Rust or LLVM.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut data = 0u8;
	/// let bits = &mut data.view_bits_mut::<Lsb0>()[.. 3];
	///
	/// unsafe {
	///   bits.get_unchecked_mut(1).commit(true);
	///   bits.get_unchecked_mut(4 .. 6).fill(true);
	/// }
	/// assert_eq!(data, 0b0011_0010);
	/// ```
	///
	/// [`.get_mut()`]: Self::get_mut
	#[inline]
	pub unsafe fn get_unchecked_mut<'a, I>(&'a mut self, index: I) -> I::Mut
	where I: BitSliceIndex<'a, T, O> {
		index.get_unchecked_mut(self)
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_bitptr()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_ptr(&self) -> BitPtr<Const, T, O> {
		self.as_bitptr()
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_mut_bitptr()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_mut_ptr(&mut self) -> BitPtr<Mut, T, O> {
		self.as_mut_bitptr()
	}

	/// Produces a range of bit-pointers to each bit in the bit-slice.
	///
	/// This is a standard-library range, which has no real functionality for
	/// pointer types. You should prefer [`.as_bitptr_range()`] instead, as it
	/// produces a custom structure that provides expected ranging
	/// functionality.
	///
	/// ## Original
	///
	/// [`slice::as_ptr_range`](https://doc.rust-lang.org/std/primitive.slice.html#method.as_ptr_range)
	///
	/// [`.as_bitptr_range()`]: Self::as_bitptr_range
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn as_ptr_range(&self) -> Range<BitPtr<Const, T, O>> {
		self.as_bitptr_range().into_range()
	}

	/// Produces a range of mutable bit-pointers to each bit in the bit-slice.
	///
	/// This is a standard-library range, which has no real functionality for
	/// pointer types. You should prefer [`.as_mut_bitptr_range()`] instead, as
	/// it produces a custom structure that provides expected ranging
	/// functionality.
	///
	/// ## Original
	///
	/// [`slice::as_mut_ptr_range`](https://doc.rust-lang.org/std/primitive.slice.html#method.as_mut_ptr_range)
	///
	/// [`.as_mut_bitptr_range()`]: Self::as_mut_bitptr_range
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn as_mut_ptr_range(&mut self) -> Range<BitPtr<Mut, T, O>> {
		self.as_mut_bitptr_range().into_range()
	}

	/// Exchanges the bit values at two indices.
	///
	/// ## Original
	///
	/// [`slice::swap`](https://doc.rust-lang.org/std/primitive.slice.html#method.swap)
	///
	/// ## Panics
	///
	/// This panics if either `a` or `b` are out of bounds.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 1];
	/// bits.swap(0, 1);
	/// assert_eq!(bits, bits![1, 0]);
	/// ```
	#[inline]
	pub fn swap(&mut self, a: usize, b: usize) {
		let bounds = 0 .. self.len();
		self.assert_in_bounds(a, bounds.clone());
		self.assert_in_bounds(b, bounds);
		unsafe {
			self.swap_unchecked(a, b);
		}
	}

	/// Reverses the order of bits in a bit-slice.
	///
	/// ## Original
	///
	/// [`slice::reverse`](https://doc.rust-lang.org/std/primitive.slice.html#method.reverse)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 0, 1, 0, 1, 1, 0, 0, 1];
	/// bits.reverse();
	/// assert_eq!(bits, bits![1, 0, 0, 1, 1, 0, 1, 0, 0]);
	/// ```
	#[inline]
	pub fn reverse(&mut self) {
		let mut iter = self.as_mut_bitptr_range();
		while let (Some(a), Some(b)) = (iter.next(), iter.next_back()) {
			unsafe {
				crate::ptr::swap(a, b);
			}
		}
	}

	/// Produces an iterator over each bit in the bit-slice.
	///
	/// ## Original
	///
	/// [`slice::iter`](https://doc.rust-lang.org/std/primitive.slice.html#method.iter)
	///
	/// ## API Differences
	///
	/// This iterator yields proxy-reference structures, not `&bool`. It can be
	/// adapted to yield `&bool` with the [`.by_refs()`] method, or `bool` with
	/// [`.by_vals()`].
	///
	/// This iterator, and its adapters, are fast. Do not try to be more clever
	/// than them by abusing `.as_bitptr_range()`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0, 1];
	/// let mut iter = bits.iter();
	///
	/// assert!(!iter.next().unwrap());
	/// assert!( iter.next().unwrap());
	/// assert!( iter.next_back().unwrap());
	/// assert!(!iter.next_back().unwrap());
	/// assert!( iter.next().is_none());
	/// ```
	///
	/// [`.by_refs()`]: crate::slice::Iter::by_refs
	/// [`.by_vals()`]: crate::slice::Iter::by_vals
	#[inline]
	pub fn iter(&self) -> Iter<T, O> {
		Iter::new(self)
	}

	/// Produces a mutable iterator over each bit in the bit-slice.
	///
	/// ## Original
	///
	/// [`slice::iter_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.iter_mut)
	///
	/// ## API Differences
	///
	/// This iterator yields proxy-reference structures, not `&mut bool`. In
	/// addition, it marks each proxy as alias-tainted.
	///
	/// If you are using this in an ordinary loop and **not** keeping multiple
	/// yielded proxy-references alive at the same scope, you may use the
	/// [`.remove_alias()`] adapter to undo the alias marking.
	///
	/// This iterator is fast. Do not try to be more clever than it by abusing
	/// `.as_mut_bitptr_range()`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0; 4];
	/// let mut iter = bits.iter_mut();
	///
	/// iter.nth(1).unwrap().commit(true); // index 1
	/// iter.next_back().unwrap().commit(true); // index 3
	///
	/// assert!(iter.next().is_some()); // index 2
	/// assert!(iter.next().is_none()); // complete
	/// assert_eq!(bits, bits![0, 1, 0, 1]);
	/// ```
	///
	/// [`.remove_alias()`]: crate::slice::IterMut::remove_alias
	#[inline]
	pub fn iter_mut(&mut self) -> IterMut<T, O> {
		IterMut::new(self)
	}

	/// Iterates over consecutive windowing subslices in a bit-slice.
	///
	/// Windows are overlapping views of the bit-slice. Each window advances one
	/// bit from the previous, so in a bit-slice `[A, B, C, D, E]`, calling
	/// `.windows(3)` will yield `[A, B, C]`, `[B, C, D]`, and `[C, D, E]`.
	///
	/// ## Original
	///
	/// [`slice::windows`](https://doc.rust-lang.org/std/primitive.slice.html#method.windows)
	///
	/// ## Panics
	///
	/// This panics if `size` is `0`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0, 0, 1];
	/// let mut iter = bits.windows(3);
	///
	/// assert_eq!(iter.next(), Some(bits![0, 1, 0]));
	/// assert_eq!(iter.next(), Some(bits![1, 0, 0]));
	/// assert_eq!(iter.next(), Some(bits![0, 0, 1]));
	/// assert!(iter.next().is_none());
	/// ```
	#[inline]
	pub fn windows(&self, size: usize) -> Windows<T, O> {
		Windows::new(self, size)
	}

	/// Iterates over non-overlapping subslices of a bit-slice.
	///
	/// Unlike `.windows()`, the subslices this yields do not overlap with each
	/// other. If `self.len()` is not an even multiple of `chunk_size`, then the
	/// last chunk yielded will be shorter.
	///
	/// ## Original
	///
	/// [`slice::chunks`](https://doc.rust-lang.org/std/primitive.slice.html#method.chunks)
	///
	/// ## Sibling Methods
	///
	/// - [`.chunks_mut()`] has the same division logic, but each yielded
	///   bit-slice is mutable.
	/// - [`.chunks_exact()`] does not yield the final chunk if it is shorter
	///   than `chunk_size`.
	/// - [`.rchunks()`] iterates from the back of the bit-slice to the front,
	///   with the final, possibly-shorter, segment at the front edge.
	///
	/// ## Panics
	///
	/// This panics if `chunk_size` is `0`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0, 0, 1];
	/// let mut iter = bits.chunks(2);
	///
	/// assert_eq!(iter.next(), Some(bits![0, 1]));
	/// assert_eq!(iter.next(), Some(bits![0, 0]));
	/// assert_eq!(iter.next(), Some(bits![1]));
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// [`.chunks_exact()`]: Self::chunks_exact
	/// [`.chunks_mut()`]: Self::chunks_mut
	/// [`.rchunks()`]: Self::rchunks
	#[inline]
	pub fn chunks(&self, chunk_size: usize) -> Chunks<T, O> {
		Chunks::new(self, chunk_size)
	}

	/// Iterates over non-overlapping mutable subslices of a bit-slice.
	///
	/// Iterators do not require that each yielded item is destroyed before the
	/// next is produced. This means that each bit-slice yielded must be marked
	/// as aliased. If you are using this in a loop that does not collect
	/// multiple yielded subslices for the same scope, then you can remove the
	/// alias marking by calling the (`unsafe`) method [`.remove_alias()`] on
	/// the iterator.
	///
	/// ## Original
	///
	/// [`slice::chunks_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.chunks_mut)
	///
	/// ## Sibling Methods
	///
	/// - [`.chunks()`] has the same division logic, but each yielded bit-slice
	///   is immutable.
	/// - [`.chunks_exact_mut()`] does not yield the final chunk if it is
	///   shorter than `chunk_size`.
	/// - [`.rchunks_mut()`] iterates from the back of the bit-slice to the
	///   front, with the final, possibly-shorter, segment at the front edge.
	///
	/// ## Panics
	///
	/// This panics if `chunk_size` is `0`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut u8, Msb0; 0; 5];
	///
	/// for (idx, chunk) in unsafe {
	///   bits.chunks_mut(2).remove_alias()
	/// }.enumerate() {
	///   chunk.store(idx + 1);
	/// }
	/// assert_eq!(bits, bits![0, 1, 1, 0, 1]);
	/// //                     ^^^^  ^^^^  ^
	/// ```
	///
	/// [`.chunks()`]: Self::chunks
	/// [`.chunks_exact_mut()`]: Self::chunks_exact_mut
	/// [`.rchunks_mut()`]: Self::rchunks_mut
	/// [`.remove_alias()`]: crate::slice::ChunksMut::remove_alias
	#[inline]
	pub fn chunks_mut(&mut self, chunk_size: usize) -> ChunksMut<T, O> {
		ChunksMut::new(self, chunk_size)
	}

	/// Iterates over non-overlapping subslices of a bit-slice.
	///
	/// If `self.len()` is not an even multiple of `chunk_size`, then the last
	/// few bits are not yielded by the iterator at all. They can be accessed
	/// with the [`.remainder()`] method if the iterator is bound to a name.
	///
	/// ## Original
	///
	/// [`slice::chunks_exact`](https://doc.rust-lang.org/std/primitive.slice.html#method.chunks_exact)
	///
	/// ## Sibling Methods
	///
	/// - [`.chunks()`] yields any leftover bits at the end as a shorter chunk
	///   during iteration.
	/// - [`.chunks_exact_mut()`] has the same division logic, but each yielded
	///   bit-slice is mutable.
	/// - [`.rchunks_exact()`] iterates from the back of the bit-slice to the
	///   front, with the unyielded remainder segment at the front edge.
	///
	/// ## Panics
	///
	/// This panics if `chunk_size` is `0`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0, 0, 1];
	/// let mut iter = bits.chunks_exact(2);
	///
	/// assert_eq!(iter.next(), Some(bits![0, 1]));
	/// assert_eq!(iter.next(), Some(bits![0, 0]));
	/// assert!(iter.next().is_none());
	/// assert_eq!(iter.remainder(), bits![1]);
	/// ```
	///
	/// [`.chunks()`]: Self::chunks
	/// [`.chunks_exact_mut()`]: Self::chunks_exact_mut
	/// [`.rchunks_exact()`]: Self::rchunks_exact
	/// [`.remainder()`]: crate::slice::ChunksExact::remainder
	#[inline]
	pub fn chunks_exact(&self, chunk_size: usize) -> ChunksExact<T, O> {
		ChunksExact::new(self, chunk_size)
	}

	/// Iterates over non-overlapping mutable subslices of a bit-slice.
	///
	/// If `self.len()` is not an even multiple of `chunk_size`, then the last
	/// few bits are not yielded by the iterator at all. They can be accessed
	/// with the [`.into_remainder()`] method if the iterator is bound to a
	/// name.
	///
	/// Iterators do not require that each yielded item is destroyed before the
	/// next is produced. This means that each bit-slice yielded must be marked
	/// as aliased. If you are using this in a loop that does not collect
	/// multiple yielded subslices for the same scope, then you can remove the
	/// alias marking by calling the (`unsafe`) method [`.remove_alias()`] on
	/// the iterator.
	///
	/// ## Original
	///
	/// [`slice::chunks_exact_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.chunks_exact_mut)
	///
	/// ## Sibling Methods
	///
	/// - [`.chunks_mut()`] yields any leftover bits at the end as a shorter
	///   chunk during iteration.
	/// - [`.chunks_exact()`] has the same division logic, but each yielded
	///   bit-slice is immutable.
	/// - [`.rchunks_exact_mut()`] iterates from the back of the bit-slice
	///   forwards, with the unyielded remainder segment at the front edge.
	///
	/// ## Panics
	///
	/// This panics if `chunk_size` is `0`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut u8, Msb0; 0; 5];
	/// let mut iter = bits.chunks_exact_mut(2);
	///
	/// for (idx, chunk) in iter.by_ref().enumerate() {
	///   chunk.store(idx + 1);
	/// }
	/// iter.into_remainder().store(1u8);
	///
	/// assert_eq!(bits, bits![0, 1, 1, 0, 1]);
	/// //                       remainder ^
	/// ```
	///
	/// [`.chunks_exact()`]: Self::chunks_exact
	/// [`.chunks_mut()`]: Self::chunks_mut
	/// [`.into_remainder()`]: crate::slice::ChunksExactMut::into_remainder
	/// [`.rchunks_exact_mut()`]: Self::rchunks_exact_mut
	/// [`.remove_alias()`]: crate::slice::ChunksExactMut::remove_alias
	#[inline]
	pub fn chunks_exact_mut(
		&mut self,
		chunk_size: usize,
	) -> ChunksExactMut<T, O> {
		ChunksExactMut::new(self, chunk_size)
	}

	/// Iterates over non-overlapping subslices of a bit-slice, from the back
	/// edge.
	///
	/// Unlike `.chunks()`, this aligns its chunks to the back edge of `self`.
	/// If `self.len()` is not an even multiple of `chunk_size`, then the
	/// leftover partial chunk is `self[0 .. len % chunk_size]`.
	///
	/// ## Original
	///
	/// [`slice::rchunks`](https://doc.rust-lang.org/std/primitive.slice.html#method.rchunks)
	///
	/// ## Sibling Methods
	///
	/// - [`.rchunks_mut()`] has the same division logic, but each yielded
	///   bit-slice is mutable.
	/// - [`.rchunks_exact()`] does not yield the final chunk if it is shorter
	///   than `chunk_size`.
	/// - [`.chunks()`] iterates from the front of the bit-slice to the back,
	///   with the final, possibly-shorter, segment at the back edge.
	///
	/// ## Panics
	///
	/// This panics if `chunk_size` is `0`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0, 0, 1];
	/// let mut iter = bits.rchunks(2);
	///
	/// assert_eq!(iter.next(), Some(bits![0, 1]));
	/// assert_eq!(iter.next(), Some(bits![1, 0]));
	/// assert_eq!(iter.next(), Some(bits![0]));
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// [`.chunks()`]: Self::chunks
	/// [`.rchunks_exact()`]: Self::rchunks_exact
	/// [`.rchunks_mut()`]: Self::rchunks_mut
	#[inline]
	pub fn rchunks(&self, chunk_size: usize) -> RChunks<T, O> {
		RChunks::new(self, chunk_size)
	}

	/// Iterates over non-overlapping mutable subslices of a bit-slice, from the
	/// back edge.
	///
	/// Unlike `.chunks_mut()`, this aligns its chunks to the back edge of
	/// `self`. If `self.len()` is not an even multiple of `chunk_size`, then
	/// the leftover partial chunk is `self[0 .. len % chunk_size]`.
	///
	/// Iterators do not require that each yielded item is destroyed before the
	/// next is produced. This means that each bit-slice yielded must be marked
	/// as aliased. If you are using this in a loop that does not collect
	/// multiple yielded values for the same scope, then you can remove the
	/// alias marking by calling the (`unsafe`) method [`.remove_alias()`] on
	/// the iterator.
	///
	/// ## Original
	///
	/// [`slice::rchunks_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.rchunks_mut)
	///
	/// ## Sibling Methods
	///
	/// - [`.rchunks()`] has the same division logic, but each yielded bit-slice
	///   is immutable.
	/// - [`.rchunks_exact_mut()`] does not yield the final chunk if it is
	///   shorter than `chunk_size`.
	/// - [`.chunks_mut()`] iterates from the front of the bit-slice to the
	///   back, with the final, possibly-shorter, segment at the back edge.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut u8, Msb0; 0; 5];
	/// for (idx, chunk) in unsafe {
	///   bits.rchunks_mut(2).remove_alias()
	/// }.enumerate() {
	///   chunk.store(idx + 1);
	/// }
	/// assert_eq!(bits, bits![1, 1, 0, 0, 1]);
	/// //           remainder ^  ^^^^  ^^^^
	/// ```
	///
	/// [`.chunks_mut()`]: Self::chunks_mut
	/// [`.rchunks()`]: Self::rchunks
	/// [`.rchunks_exact_mut()`]: Self::rchunks_exact_mut
	/// [`.remove_alias()`]: crate::slice::RChunksMut::remove_alias
	#[inline]
	pub fn rchunks_mut(&mut self, chunk_size: usize) -> RChunksMut<T, O> {
		RChunksMut::new(self, chunk_size)
	}

	/// Iterates over non-overlapping subslices of a bit-slice, from the back
	/// edge.
	///
	/// If `self.len()` is not an even multiple of `chunk_size`, then the first
	/// few bits are not yielded by the iterator at all. They can be accessed
	/// with the [`.remainder()`] method if the iterator is bound to a name.
	///
	/// ## Original
	///
	/// [`slice::rchunks_exact`](https://doc.rust-lang.org/std/primitive.slice.html#method.rchunks_exact)
	///
	/// ## Sibling Methods
	///
	/// - [`.rchunks()`] yields any leftover bits at the front as a shorter
	///   chunk during iteration.
	/// - [`.rchunks_exact_mut()`] has the same division logic, but each yielded
	///   bit-slice is mutable.
	/// - [`.chunks_exact()`] iterates from the front of the bit-slice to the
	///   back, with the unyielded remainder segment at the back edge.
	///
	/// ## Panics
	///
	/// This panics if `chunk_size` is `0`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0, 0, 1];
	/// let mut iter = bits.rchunks_exact(2);
	///
	/// assert_eq!(iter.next(), Some(bits![0, 1]));
	/// assert_eq!(iter.next(), Some(bits![1, 0]));
	/// assert!(iter.next().is_none());
	/// assert_eq!(iter.remainder(), bits![0]);
	/// ```
	///
	/// [`.chunks_exact()`]: Self::chunks_exact
	/// [`.rchunks()`]: Self::rchunks
	/// [`.rchunks_exact_mut()`]: Self::rchunks_exact_mut
	/// [`.remainder()`]: crate::slice::RChunksExact::remainder
	#[inline]
	pub fn rchunks_exact(&self, chunk_size: usize) -> RChunksExact<T, O> {
		RChunksExact::new(self, chunk_size)
	}

	/// Iterates over non-overlapping mutable subslices of a bit-slice, from the
	/// back edge.
	///
	/// If `self.len()` is not an even multiple of `chunk_size`, then the first
	/// few bits are not yielded by the iterator at all. They can be accessed
	/// with the [`.into_remainder()`] method if the iterator is bound to a
	/// name.
	///
	/// Iterators do not require that each yielded item is destroyed before the
	/// next is produced. This means that each bit-slice yielded must be marked
	/// as aliased. If you are using this in a loop that does not collect
	/// multiple yielded subslices for the same scope, then you can remove the
	/// alias marking by calling the (`unsafe`) method [`.remove_alias()`] on
	/// the iterator.
	///
	/// ## Sibling Methods
	///
	/// - [`.rchunks_mut()`] yields any leftover bits at the front as a shorter
	///   chunk during iteration.
	/// - [`.rchunks_exact()`] has the same division logic, but each yielded
	///   bit-slice is immutable.
	/// - [`.chunks_exact_mut()`] iterates from the front of the bit-slice
	///   backwards, with the unyielded remainder segment at the back edge.
	///
	/// ## Panics
	///
	/// This panics if `chunk_size` is `0`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut u8, Msb0; 0; 5];
	/// let mut iter = bits.rchunks_exact_mut(2);
	///
	/// for (idx, chunk) in iter.by_ref().enumerate() {
	///   chunk.store(idx + 1);
	/// }
	/// iter.into_remainder().store(1u8);
	///
	/// assert_eq!(bits, bits![1, 1, 0, 0, 1]);
	/// //           remainder ^
	/// ```
	///
	/// [`.chunks_exact_mut()`]: Self::chunks_exact_mut
	/// [`.into_remainder()`]: crate::slice::RChunksExactMut::into_remainder
	/// [`.rchunks_exact()`]: Self::rchunks_exact
	/// [`.rchunks_mut()`]: Self::rchunks_mut
	/// [`.remove_alias()`]: crate::slice::RChunksExactMut::remove_alias
	#[inline]
	pub fn rchunks_exact_mut(
		&mut self,
		chunk_size: usize,
	) -> RChunksExactMut<T, O> {
		RChunksExactMut::new(self, chunk_size)
	}

	/// Splits a bit-slice in two parts at an index.
	///
	/// The returned bit-slices are `self[.. mid]` and `self[mid ..]`. `mid` is
	/// included in the right bit-slice, not the left.
	///
	/// If `mid` is `0` then the left bit-slice is empty; if it is `self.len()`
	/// then the right bit-slice is empty.
	///
	/// This method guarantees that even when either partition is empty, the
	/// encoded bit-pointer values of the bit-slice references is `&self[0]` and
	/// `&self[mid]`.
	///
	/// ## Original
	///
	/// [`slice::split_at`](https://doc.rust-lang.org/std/primitive.slice.html#method.split_at)
	///
	/// ## Panics
	///
	/// This panics if `mid` is greater than `self.len()`. It is allowed to be
	/// equal to the length, in which case the right bit-slice is simply empty.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 0, 0, 1, 1, 1];
	/// let base = bits.as_bitptr();
	///
	/// let (a, b) = bits.split_at(0);
	/// assert_eq!(unsafe { a.as_bitptr().offset_from(base) }, 0);
	/// assert_eq!(unsafe { b.as_bitptr().offset_from(base) }, 0);
	///
	/// let (a, b) = bits.split_at(6);
	/// assert_eq!(unsafe { b.as_bitptr().offset_from(base) }, 6);
	///
	/// let (a, b) = bits.split_at(3);
	/// assert_eq!(a, bits![0; 3]);
	/// assert_eq!(b, bits![1; 3]);
	/// ```
	#[inline]
	pub fn split_at(&self, mid: usize) -> (&Self, &Self) {
		self.assert_in_bounds(mid, 0 ..= self.len());
		unsafe { self.split_at_unchecked(mid) }
	}

	/// Splits a mutable bit-slice in two parts at an index.
	///
	/// The returned bit-slices are `self[.. mid]` and `self[mid ..]`. `mid` is
	/// included in the right bit-slice, not the left.
	///
	/// If `mid` is `0` then the left bit-slice is empty; if it is `self.len()`
	/// then the right bit-slice is empty.
	///
	/// This method guarantees that even when either partition is empty, the
	/// encoded bit-pointer values of the bit-slice references is `&self[0]` and
	/// `&self[mid]`.
	///
	/// ## Original
	///
	/// [`slice::split_at_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.split_at_mut)
	///
	/// ## API Differences
	///
	/// The end bits of the left half and the start bits of the right half might
	/// be stored in the same memory element. In order to avoid breaking
	/// `bitvec`’s memory-safety guarantees, both bit-slices are marked as
	/// `T::Alias`. This marking allows them to be used without interfering with
	/// each other when they interact with memory.
	///
	/// ## Panics
	///
	/// This panics if `mid` is greater than `self.len()`. It is allowed to be
	/// equal to the length, in which case the right bit-slice is simply empty.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut u8, Msb0; 0; 6];
	/// let base = bits.as_mut_bitptr();
	///
	/// let (a, b) = bits.split_at_mut(0);
	/// assert_eq!(unsafe { a.as_mut_bitptr().offset_from(base) }, 0);
	/// assert_eq!(unsafe { b.as_mut_bitptr().offset_from(base) }, 0);
	///
	/// let (a, b) = bits.split_at_mut(6);
	/// assert_eq!(unsafe { b.as_mut_bitptr().offset_from(base) }, 6);
	///
	/// let (a, b) = bits.split_at_mut(3);
	/// a.store(3);
	/// b.store(5);
	///
	/// assert_eq!(bits, bits![0, 1, 1, 1, 0, 1]);
	/// ```
	#[inline]
	pub fn split_at_mut(
		&mut self,
		mid: usize,
	) -> (&mut BitSlice<T::Alias, O>, &mut BitSlice<T::Alias, O>) {
		self.assert_in_bounds(mid, 0 ..= self.len());
		unsafe { self.split_at_unchecked_mut(mid) }
	}

	/// Iterates over subslices separated by bits that match a predicate. The
	/// matched bit is *not* contained in the yielded bit-slices.
	///
	/// ## Original
	///
	/// [`slice::split`](https://doc.rust-lang.org/std/primitive.slice.html#method.split)
	///
	/// ## API Differences
	///
	/// The predicate function receives the index being tested as well as the
	/// bit value at that index. This allows the predicate to have more than one
	/// bit of information about the bit-slice being traversed.
	///
	/// ## Sibling Methods
	///
	/// - [`.split_mut()`] has the same splitting logic, but each yielded
	///   bit-slice is mutable.
	/// - [`.split_inclusive()`] includes the matched bit in the yielded
	///   bit-slice.
	/// - [`.rsplit()`] iterates from the back of the bit-slice instead of the
	///   front.
	/// - [`.splitn()`] times out after `n` yields.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 1, 0];
	/// //                     ^
	/// let mut iter = bits.split(|pos, _bit| pos % 3 == 2);
	///
	/// assert_eq!(iter.next().unwrap(), bits![0, 1]);
	/// assert_eq!(iter.next().unwrap(), bits![0]);
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// If the first bit is matched, then an empty bit-slice will be the first
	/// item yielded by the iterator. Similarly, if the last bit in the
	/// bit-slice matches, then an empty bit-slice will be the last item
	/// yielded.
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 0, 1];
	/// //                     ^
	/// let mut iter = bits.split(|_pos, bit| *bit);
	///
	/// assert_eq!(iter.next().unwrap(), bits![0; 2]);
	/// assert!(iter.next().unwrap().is_empty());
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// If two matched bits are directly adjacent, then an empty bit-slice will
	/// be yielded between them:
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![1, 0, 0, 1];
	/// //                  ^  ^
	/// let mut iter = bits.split(|_pos, bit| !*bit);
	///
	/// assert_eq!(iter.next().unwrap(), bits![1]);
	/// assert!(iter.next().unwrap().is_empty());
	/// assert_eq!(iter.next().unwrap(), bits![1]);
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// [`.rsplit()`]: Self::rsplit
	/// [`.splitn()`]: Self::splitn
	/// [`.split_inclusive()`]: Self::split_inclusive
	/// [`.split_mut()`]: Self::split_mut
	#[inline]
	pub fn split<F>(&self, pred: F) -> Split<T, O, F>
	where F: FnMut(usize, &bool) -> bool {
		Split::new(self, pred)
	}

	/// Iterates over mutable subslices separated by bits that match a
	/// predicate. The matched bit is *not* contained in the yielded bit-slices.
	///
	/// Iterators do not require that each yielded item is destroyed before the
	/// next is produced. This means that each bit-slice yielded must be marked
	/// as aliased. If you are using this in a loop that does not collect
	/// multiple yielded subslices for the same scope, then you can remove the
	/// alias marking by calling the (`unsafe`) method [`.remove_alias()`] on
	/// the iterator.
	///
	/// ## Original
	///
	/// [`slice::split_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.split_mut)
	///
	/// ## API Differences
	///
	/// The predicate function receives the index being tested as well as the
	/// bit value at that index. This allows the predicate to have more than one
	/// bit of information about the bit-slice being traversed.
	///
	/// ## Sibling Methods
	///
	/// - [`.split()`] has the same splitting logic, but each yielded bit-slice
	///   is immutable.
	/// - [`.split_inclusive_mut()`] includes the matched bit in the yielded
	///   bit-slice.
	/// - [`.rsplit_mut()`] iterates from the back of the bit-slice instead of
	///   the front.
	/// - [`.splitn_mut()`] times out after `n` yields.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 0, 1, 0, 1, 0];
	/// //                         ^     ^
	/// for group in bits.split_mut(|_pos, bit| *bit) {
	///   group.set(0, true);
	/// }
	/// assert_eq!(bits, bits![1, 0, 1, 1, 1, 1]);
	/// ```
	///
	/// [`.remove_alias()`]: crate::slice::SplitMut::remove_alias
	/// [`.rsplit_mut()`]: Self::rsplit_mut
	/// [`.split()`]: Self::split
	/// [`.split_inclusive_mut()`]: Self::split_inclusive_mut
	/// [`.splitn_mut()`]: Self::splitn_mut
	#[inline]
	pub fn split_mut<F>(&mut self, pred: F) -> SplitMut<T, O, F>
	where F: FnMut(usize, &bool) -> bool {
		SplitMut::new(self.alias_mut(), pred)
	}

	/// Iterates over subslices separated by bits that match a predicate. Unlike
	/// `.split()`, this *does* include the matching bit as the last bit in the
	/// yielded bit-slice.
	///
	/// ## Original
	///
	/// [`slice::split_inclusive`](https://doc.rust-lang.org/std/primitive.slice.html#method.split_inclusive)
	///
	/// ## API Differences
	///
	/// The predicate function receives the index being tested as well as the
	/// bit value at that index. This allows the predicate to have more than one
	/// bit of information about the bit-slice being traversed.
	///
	/// ## Sibling Methods
	///
	/// - [`.split_inclusive_mut()`] has the same splitting logic, but each
	///   yielded bit-slice is mutable.
	/// - [`.split()`] does not include the matched bit in the yielded
	///   bit-slice.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 0, 1, 0, 1];
	/// //                     ^     ^
	/// let mut iter = bits.split_inclusive(|_pos, bit| *bit);
	///
	/// assert_eq!(iter.next().unwrap(), bits![0, 0, 1]);
	/// assert_eq!(iter.next().unwrap(), bits![0, 1]);
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// [`.split()`]: Self::split
	/// [`.split_inclusive_mut()`]: Self::split_inclusive_mut
	#[inline]
	pub fn split_inclusive<F>(&self, pred: F) -> SplitInclusive<T, O, F>
	where F: FnMut(usize, &bool) -> bool {
		SplitInclusive::new(self, pred)
	}

	/// Iterates over mutable subslices separated by bits that match a
	/// predicate. Unlike `.split_mut()`, this *does* include the matching bit
	/// as the last bit in the bit-slice.
	///
	/// Iterators do not require that each yielded item is destroyed before the
	/// next is produced. This means that each bit-slice yielded must be marked
	/// as aliased. If you are using this in a loop that does not collect
	/// multiple yielded subslices for the same scope, then you can remove the
	/// alias marking by calling the (`unsafe`) method [`.remove_alias()`] on
	/// the iterator.
	///
	/// ## Original
	///
	/// [`slice::split_inclusive_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.split_inclusive_mut)
	///
	/// ## API Differences
	///
	/// The predicate function receives the index being tested as well as the
	/// bit value at that index. This allows the predicate to have more than one
	/// bit of information about the bit-slice being traversed.
	///
	/// ## Sibling Methods
	///
	/// - [`.split_inclusive()`] has the same splitting logic, but each yielded
	///   bit-slice is immutable.
	/// - [`.split_mut()`] does not include the matched bit in the yielded
	///   bit-slice.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 0, 0, 0, 0];
	/// //                         ^
	/// for group in bits.split_inclusive_mut(|pos, _bit| pos % 3 == 2) {
	///   group.set(0, true);
	/// }
	/// assert_eq!(bits, bits![1, 0, 0, 1, 0]);
	/// ```
	///
	/// [`.remove_alias()`]: crate::slice::SplitInclusiveMut::remove_alias
	/// [`.split_inclusive()`]: Self::split_inclusive
	/// [`.split_mut()`]: Self::split_mut
	#[inline]
	pub fn split_inclusive_mut<F>(
		&mut self,
		pred: F,
	) -> SplitInclusiveMut<T, O, F>
	where
		F: FnMut(usize, &bool) -> bool,
	{
		SplitInclusiveMut::new(self.alias_mut(), pred)
	}

	/// Iterates over subslices separated by bits that match a predicate, from
	/// the back edge. The matched bit is *not* contained in the yielded
	/// bit-slices.
	///
	/// ## Original
	///
	/// [`slice::rsplit`](https://doc.rust-lang.org/std/primitive.slice.html#method.rsplit)
	///
	/// ## API Differences
	///
	/// The predicate function receives the index being tested as well as the
	/// bit value at that index. This allows the predicate to have more than one
	/// bit of information about the bit-slice being traversed.
	///
	/// ## Sibling Methods
	///
	/// - [`.rsplit_mut()`] has the same splitting logic, but each yielded
	///   bit-slice is mutable.
	/// - [`.split()`] iterates from the front of the bit-slice instead of the
	///   back.
	/// - [`.rsplitn()`] times out after `n` yields.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 1, 0];
	/// //                     ^
	/// let mut iter = bits.rsplit(|pos, _bit| pos % 3 == 2);
	///
	/// assert_eq!(iter.next().unwrap(), bits![0]);
	/// assert_eq!(iter.next().unwrap(), bits![0, 1]);
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// If the last bit is matched, then an empty bit-slice will be the first
	/// item yielded by the iterator. Similarly, if the first bit in the
	/// bit-slice matches, then an empty bit-slice will be the last item
	/// yielded.
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 0, 1];
	/// //                     ^
	/// let mut iter = bits.rsplit(|_pos, bit| *bit);
	///
	/// assert!(iter.next().unwrap().is_empty());
	/// assert_eq!(iter.next().unwrap(), bits![0; 2]);
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// If two yielded bits are directly adjacent, then an empty bit-slice will
	/// be yielded between them:
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![1, 0, 0, 1];
	/// //                  ^  ^
	/// let mut iter = bits.split(|_pos, bit| !*bit);
	///
	/// assert_eq!(iter.next().unwrap(), bits![1]);
	/// assert!(iter.next().unwrap().is_empty());
	/// assert_eq!(iter.next().unwrap(), bits![1]);
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// [`.rsplitn()`]: Self::rsplitn
	/// [`.rsplit_mut()`]: Self::rsplit_mut
	/// [`.split()`]: Self::split
	#[inline]
	pub fn rsplit<F>(&self, pred: F) -> RSplit<T, O, F>
	where F: FnMut(usize, &bool) -> bool {
		RSplit::new(self, pred)
	}

	/// Iterates over mutable subslices separated by bits that match a
	/// predicate, from the back. The matched bit is *not* contained in the
	/// yielded bit-slices.
	///
	/// Iterators do not require that each yielded item is destroyed before the
	/// next is produced. This means that each bit-slice yielded must be marked
	/// as aliased. If you are using this in a loop that does not collect
	/// multiple yielded subslices for the same scope, then you can remove the
	/// alias marking by calling the (`unsafe`) method [`.remove_alias()`] on
	/// the iterator.
	///
	/// ## Original
	///
	/// [`slice::rsplit_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.rsplit_mut)
	///
	/// ## API Differences
	///
	/// The predicate function receives the index being tested as well as the
	/// bit value at that index. This allows the predicate to have more than one
	/// bit of information about the bit-slice being traversed.
	///
	/// ## Sibling Methods
	///
	/// - [`.rsplit()`] has the same splitting logic, but each yielded bit-slice
	///   is immutable.
	/// - [`.split_mut()`] iterates from the front of the bit-slice to the back.
	/// - [`.rsplitn_mut()`] iterates from the front of the bit-slice to the
	///   back.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 0, 1, 0, 1, 0];
	/// //                         ^     ^
	/// for group in bits.rsplit_mut(|_pos, bit| *bit) {
	///   group.set(0, true);
	/// }
	/// assert_eq!(bits, bits![1, 0, 1, 1, 1, 1]);
	/// ```
	///
	/// [`.remove_alias()`]: crate::slice::RSplitMut::remove_alias
	/// [`.rsplit()`]: Self::rsplit
	/// [`.rsplitn_mut()`]: Self::rsplitn_mut
	/// [`.split_mut()`]: Self::split_mut
	#[inline]
	pub fn rsplit_mut<F>(&mut self, pred: F) -> RSplitMut<T, O, F>
	where F: FnMut(usize, &bool) -> bool {
		RSplitMut::new(self.alias_mut(), pred)
	}

	/// Iterates over subslices separated by bits that match a predicate, giving
	/// up after yielding `n` times. The `n`th yield contains the rest of the
	/// bit-slice. As with `.split()`, the yielded bit-slices do not contain the
	/// matched bit.
	///
	/// ## Original
	///
	/// [`slice::splitn`](https://doc.rust-lang.org/std/primitive.slice.html#method.splitn)
	///
	/// ## API Differences
	///
	/// The predicate function receives the index being tested as well as the
	/// bit value at that index. This allows the predicate to have more than one
	/// bit of information about the bit-slice being traversed.
	///
	/// ## Sibling Methods
	///
	/// - [`.splitn_mut()`] has the same splitting logic, but each yielded
	///   bit-slice is mutable.
	/// - [`.rsplitn()`] iterates from the back of the bit-slice instead of the
	///   front.
	/// - [`.split()`] has the same splitting logic, but never times out.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 0, 1, 0, 1, 0];
	/// let mut iter = bits.splitn(2, |_pos, bit| *bit);
	///
	/// assert_eq!(iter.next().unwrap(), bits![0, 0]);
	/// assert_eq!(iter.next().unwrap(), bits![0, 1, 0]);
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// [`.rsplitn()`]: Self::rsplitn
	/// [`.split()`]: Self::split
	/// [`.splitn_mut()`]: Self::splitn_mut
	#[inline]
	pub fn splitn<F>(&self, n: usize, pred: F) -> SplitN<T, O, F>
	where F: FnMut(usize, &bool) -> bool {
		SplitN::new(self, pred, n)
	}

	/// Iterates over mutable subslices separated by bits that match a
	/// predicate, giving up after yielding `n` times. The `n`th yield contains
	/// the rest of the bit-slice. As with `.split_mut()`, the yielded
	/// bit-slices do not contain the matched bit.
	///
	/// Iterators do not require that each yielded item is destroyed before the
	/// next is produced. This means that each bit-slice yielded must be marked
	/// as aliased. If you are using this in a loop that does not collect
	/// multiple yielded subslices for the same scope, then you can remove the
	/// alias marking by calling the (`unsafe`) method [`.remove_alias()`] on
	/// the iterator.
	///
	/// ## Original
	///
	/// [`slice::splitn_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.splitn_mut)
	///
	/// ## API Differences
	///
	/// The predicate function receives the index being tested as well as the
	/// bit value at that index. This allows the predicate to have more than one
	/// bit of information about the bit-slice being traversed.
	///
	/// ## Sibling Methods
	///
	/// - [`.splitn()`] has the same splitting logic, but each yielded bit-slice
	///   is immutable.
	/// - [`.rsplitn_mut()`] iterates from the back of the bit-slice instead of
	///   the front.
	/// - [`.split_mut()`] has the same splitting logic, but never times out.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 0, 1, 0, 1, 0];
	/// for group in bits.splitn_mut(2, |_pos, bit| *bit) {
	///   group.set(0, true);
	/// }
	/// assert_eq!(bits, bits![1, 0, 1, 1, 1, 0]);
	/// ```
	///
	/// [`.remove_alias()`]: crate::slice::SplitNMut::remove_alias
	/// [`.rsplitn_mut()`]: Self::rsplitn_mut
	/// [`.split_mut()`]: Self::split_mut
	/// [`.splitn()`]: Self::splitn
	#[inline]
	pub fn splitn_mut<F>(&mut self, n: usize, pred: F) -> SplitNMut<T, O, F>
	where F: FnMut(usize, &bool) -> bool {
		SplitNMut::new(self.alias_mut(), pred, n)
	}

	/// Iterates over mutable subslices separated by bits that match a
	/// predicate from the back edge, giving up after yielding `n` times. The
	/// `n`th yield contains the rest of the bit-slice. As with `.split_mut()`,
	/// the yielded bit-slices do not contain the matched bit.
	///
	/// ## Original
	///
	/// [`slice::rsplitn`](https://doc.rust-lang.org/std/primitive.slice.html#method.rsplitn)
	///
	/// ## API Differences
	///
	/// The predicate function receives the index being tested as well as the
	/// bit value at that index. This allows the predicate to have more than one
	/// bit of information about the bit-slice being traversed.
	///
	/// ## Sibling Methods
	///
	/// - [`.rsplitn_mut()`] has the same splitting logic, but each yielded
	///   bit-slice is mutable.
	/// - [`.splitn()`]: iterates from the front of the bit-slice instead of the
	///   back.
	/// - [`.rsplit()`] has the same splitting logic, but never times out.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 0, 1, 1, 0];
	/// //                        ^
	/// let mut iter = bits.rsplitn(2, |_pos, bit| *bit);
	///
	/// assert_eq!(iter.next().unwrap(), bits![0]);
	/// assert_eq!(iter.next().unwrap(), bits![0, 0, 1]);
	/// assert!(iter.next().is_none());
	/// ```
	///
	/// [`.rsplit()`]: Self::rsplit
	/// [`.rsplitn_mut()`]: Self::rsplitn_mut
	/// [`.splitn()`]: Self::splitn
	#[inline]
	pub fn rsplitn<F>(&self, n: usize, pred: F) -> RSplitN<T, O, F>
	where F: FnMut(usize, &bool) -> bool {
		RSplitN::new(self, pred, n)
	}

	/// Iterates over mutable subslices separated by bits that match a
	/// predicate from the back edge, giving up after yielding `n` times. The
	/// `n`th yield contains the rest of the bit-slice. As with `.split_mut()`,
	/// the yielded bit-slices do not contain the matched bit.
	///
	/// Iterators do not require that each yielded item is destroyed before the
	/// next is produced. This means that each bit-slice yielded must be marked
	/// as aliased. If you are using this in a loop that does not collect
	/// multiple yielded subslices for the same scope, then you can remove the
	/// alias marking by calling the (`unsafe`) method [`.remove_alias()`] on
	/// the iterator.
	///
	/// ## Original
	///
	/// [`slice::rsplitn_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.rsplitn_mut)
	///
	/// ## API Differences
	///
	/// The predicate function receives the index being tested as well as the
	/// bit value at that index. This allows the predicate to have more than one
	/// bit of information about the bit-slice being traversed.
	///
	/// ## Sibling Methods
	///
	/// - [`.rsplitn()`] has the same splitting logic, but each yielded
	///   bit-slice is immutable.
	/// - [`.splitn_mut()`] iterates from the front of the bit-slice instead of
	///   the back.
	/// - [`.rsplit_mut()`] has the same splitting logic, but never times out.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 0, 1, 0, 0, 1, 0, 0, 0];
	/// for group in bits.rsplitn_mut(2, |_idx, bit| *bit) {
	///   group.set(0, true);
	/// }
	/// assert_eq!(bits, bits![1, 0, 1, 0, 0, 1, 1, 0, 0]);
	/// //                     ^ group 2         ^ group 1
	/// ```
	///
	/// [`.remove_alias()`]: crate::slice::RSplitNMut::remove_alias
	/// [`.rsplitn()`]: Self::rsplitn
	/// [`.rsplit_mut()`]: Self::rsplit_mut
	/// [`.splitn_mut()`]: Self::splitn_mut
	#[inline]
	pub fn rsplitn_mut<F>(&mut self, n: usize, pred: F) -> RSplitNMut<T, O, F>
	where F: FnMut(usize, &bool) -> bool {
		RSplitNMut::new(self.alias_mut(), pred, n)
	}

	/// Tests if the bit-slice contains the given sequence anywhere within it.
	///
	/// This scans over `self.windows(other.len())` until one of the windows
	/// matches. The search key does not need to share type parameters with the
	/// bit-slice being tested, as the comparison is bit-wise. However, sharing
	/// type parameters will accelerate the comparison.
	///
	/// ## Original
	///
	/// [`slice::contains`](https://doc.rust-lang.org/std/primitive.slice.html#method.contains)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 0, 1, 0, 1, 1, 0, 0];
	/// assert!( bits.contains(bits![0, 1, 1, 0]));
	/// assert!(!bits.contains(bits![1, 0, 0, 1]));
	/// ```
	#[inline]
	pub fn contains<T2, O2>(&self, other: &BitSlice<T2, O2>) -> bool
	where
		T2: BitStore,
		O2: BitOrder,
	{
		self.len() >= other.len()
			&& self.windows(other.len()).any(|window| window == other)
	}

	/// Tests if the bit-slice begins with the given sequence.
	///
	/// The search key does not need to share type parameters with the bit-slice
	/// being tested, as the comparison is bit-wise. However, sharing type
	/// parameters will accelerate the comparison.
	///
	/// ## Original
	///
	/// [`slice::starts_with`](https://doc.rust-lang.org/std/primitive.slice.html#method.starts_with)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 1, 0];
	/// assert!( bits.starts_with(bits![0, 1]));
	/// assert!(!bits.starts_with(bits![1, 0]));
	/// ```
	///
	/// This always returns `true` if the needle is empty:
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0];
	/// let empty = bits![];
	/// assert!(bits.starts_with(empty));
	/// assert!(empty.starts_with(empty));
	/// ```
	#[inline]
	pub fn starts_with<T2, O2>(&self, needle: &BitSlice<T2, O2>) -> bool
	where
		T2: BitStore,
		O2: BitOrder,
	{
		self.get(.. needle.len())
			.map(|slice| slice == needle)
			.unwrap_or(false)
	}

	/// Tests if the bit-slice ends with the given sequence.
	///
	/// The search key does not need to share type parameters with the bit-slice
	/// being tested, as the comparison is bit-wise. However, sharing type
	/// parameters will accelerate the comparison.
	///
	/// ## Original
	///
	/// [`slice::ends_with`](https://doc.rust-lang.org/std/primitive.slice.html#method.ends_with)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 1, 0];
	/// assert!( bits.ends_with(bits![1, 0]));
	/// assert!(!bits.ends_with(bits![0, 1]));
	/// ```
	///
	/// This always returns `true` if the needle is empty:
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0];
	/// let empty = bits![];
	/// assert!(bits.ends_with(empty));
	/// assert!(empty.ends_with(empty));
	/// ```
	#[inline]
	pub fn ends_with<T2, O2>(&self, needle: &BitSlice<T2, O2>) -> bool
	where
		T2: BitStore,
		O2: BitOrder,
	{
		self.get(self.len() - needle.len() ..)
			.map(|slice| slice == needle)
			.unwrap_or(false)
	}

	/// Removes a prefix bit-slice, if present.
	///
	/// Like [`.starts_with()`], the search key does not need to share type
	/// parameters with the bit-slice being stripped. If
	/// `self.starts_with(suffix)`, then this returns `Some(&self[prefix.len()
	/// ..])`, otherwise it returns `None`.
	///
	/// ## Original
	///
	/// [`slice::strip_prefix`](https://doc.rust-lang.org/std/primitive.slice.html#method.strip_prefix)
	///
	/// ## API Differences
	///
	/// `BitSlice` does not support pattern searches; instead, it permits `self`
	/// and `prefix` to differ in type parameters.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0, 0, 1, 0, 1, 1, 0];
	/// assert_eq!(bits.strip_prefix(bits![0, 1]).unwrap(), bits[2 ..]);
	/// assert_eq!(bits.strip_prefix(bits![0, 1, 0, 0,]).unwrap(), bits[4 ..]);
	/// assert!(bits.strip_prefix(bits![1, 0]).is_none());
	/// ```
	///
	/// [`.starts_with()`]: Self::starts_with
	#[inline]
	pub fn strip_prefix<T2, O2>(
		&self,
		prefix: &BitSlice<T2, O2>,
	) -> Option<&Self>
	where
		T2: BitStore,
		O2: BitOrder,
	{
		if self.starts_with(prefix) {
			self.get(prefix.len() ..)
		}
		else {
			None
		}
	}

	/// Removes a suffix bit-slice, if present.
	///
	/// Like [`.ends_with()`], the search key does not need to share type
	/// parameters with the bit-slice being stripped. If
	/// `self.ends_with(suffix)`, then this returns `Some(&self[.. self.len() -
	/// suffix.len()])`, otherwise it returns `None`.
	///
	/// ## Original
	///
	/// [`slice::strip_suffix`](https://doc.rust-lang.org/std/primitive.slice.html#method.strip_suffix)
	///
	/// ## API Differences
	///
	/// `BitSlice` does not support pattern searches; instead, it permits `self`
	/// and `suffix` to differ in type parameters.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0, 0, 1, 0, 1, 1, 0];
	/// assert_eq!(bits.strip_suffix(bits![1, 0]).unwrap(), bits[.. 7]);
	/// assert_eq!(bits.strip_suffix(bits![0, 1, 1, 0]).unwrap(), bits[.. 5]);
	/// assert!(bits.strip_suffix(bits![0, 1]).is_none());
	/// ```
	///
	/// [`.ends_with()`]: Self::ends_with.
	#[inline]
	pub fn strip_suffix<T2, O2>(
		&self,
		suffix: &BitSlice<T2, O2>,
	) -> Option<&Self>
	where
		T2: BitStore,
		O2: BitOrder,
	{
		if self.ends_with(suffix) {
			self.get(.. self.len() - suffix.len())
		}
		else {
			None
		}
	}

	/// Rotates the contents of a bit-slice to the left (towards the zero
	/// index).
	///
	/// This essentially splits the bit-slice at `by`, then exchanges the two
	/// pieces. `self[.. by]` becomes the first section, and is then followed by
	/// `self[.. by]`.
	///
	/// The implementation is batch-accelerated where possible. It should have a
	/// runtime complexity much lower than `O(by)`.
	///
	/// ## Original
	///
	/// [`slice::rotate_left`](https://doc.rust-lang.org/std/primitive.slice.html#method.rotate_left)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 0, 1, 0, 1, 0];
	/// //      split occurs here ^
	/// bits.rotate_left(2);
	/// assert_eq!(bits, bits![1, 0, 1, 0, 0, 0]);
	/// ```
	#[inline]
	pub fn rotate_left(&mut self, mut by: usize) {
		let len = self.len();
		assert!(
			by <= len,
			"bit-slices cannot be rotated by more than their length",
		);
		if by == 0 || by == len {
			return;
		}
		let mut tmp = BitArray::<usize, O>::ZERO;
		while by > 0 {
			let shamt = cmp::min(mem::bits_of::<usize>(), by);
			unsafe {
				let tmp_bits = tmp.get_unchecked_mut(.. shamt);
				tmp_bits.clone_from_bitslice(self.get_unchecked(.. shamt));
				self.copy_within_unchecked(shamt .., 0);
				self.get_unchecked_mut(len - shamt ..)
					.clone_from_bitslice(tmp_bits);
			}
			by -= shamt;
		}
	}

	/// Rotates the contents of a bit-slice to the right (away from the zero
	/// index).
	///
	/// This essentially splits the bit-slice at `self.len() - by`, then
	/// exchanges the two pieces. `self[len - by ..]` becomes the first section,
	/// and is then followed by `self[.. len - by]`.
	///
	/// The implementation is batch-accelerated where possible. It should have a
	/// runtime complexity much lower than `O(by)`.
	///
	/// ## Original
	///
	/// [`slice::rotate_right`](https://doc.rust-lang.org/std/primitive.slice.html#method.rotate_right)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 0, 1, 1, 1, 0];
	/// //            split occurs here ^
	/// bits.rotate_right(2);
	/// assert_eq!(bits, bits![1, 0, 0, 0, 1, 1]);
	/// ```
	#[inline]
	pub fn rotate_right(&mut self, mut by: usize) {
		let len = self.len();
		assert!(
			by <= len,
			"bit-slices cannot be rotated by more than their length",
		);
		if by == 0 || by == len {
			return;
		}
		let mut tmp = BitArray::<usize, O>::ZERO;
		while by > 0 {
			let shamt = cmp::min(mem::bits_of::<usize>(), by);
			let mid = len - shamt;
			unsafe {
				let tmp_bits = tmp.get_unchecked_mut(.. shamt);
				tmp_bits.clone_from_bitslice(self.get_unchecked(mid ..));
				self.copy_within_unchecked(.. mid, shamt);
				self.get_unchecked_mut(.. shamt)
					.clone_from_bitslice(tmp_bits);
			}
			by -= shamt;
		}
	}

	/// Fills the bit-slice with a given bit.
	///
	/// This is a recent stabilization in the standard library. `bitvec`
	/// previously offered this behavior as the novel API `.set_all()`. That
	/// method name is now removed in favor of this standard-library analogue.
	///
	/// ## Original
	///
	/// [`slice::fill`](https://doc.rust-lang.org/std/primitive.slice.html#method.fill)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0; 5];
	/// bits.fill(true);
	/// assert_eq!(bits, bits![1; 5]);
	/// ```
	#[inline]
	pub fn fill(&mut self, value: bool) {
		let fill = if value { T::Mem::ALL } else { T::Mem::ZERO };
		match self.domain_mut() {
			Domain::Enclave(mut elem) => {
				elem.store_value(fill);
			},
			Domain::Region { head, body, tail } => {
				if let Some(mut elem) = head {
					elem.store_value(fill);
				}
				for elem in body {
					elem.store_value(fill);
				}
				if let Some(mut elem) = tail {
					elem.store_value(fill);
				}
			},
		}
	}

	/// Fills the bit-slice with bits produced by a generator function.
	///
	/// ## Original
	///
	/// [`slice::fill_with`](https://doc.rust-lang.org/std/primitive.slice.html#method.fill_with)
	///
	/// ## API Differences
	///
	/// The generator function receives the index of the bit being initialized
	/// as an argument.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0; 5];
	/// bits.fill_with(|idx| idx % 2 == 0);
	/// assert_eq!(bits, bits![1, 0, 1, 0, 1]);
	/// ```
	#[inline]
	pub fn fill_with<F>(&mut self, mut func: F)
	where F: FnMut(usize) -> bool {
		for (idx, ptr) in self.as_mut_bitptr_range().enumerate() {
			unsafe {
				ptr.write(func(idx));
			}
		}
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.clone_from_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn clone_from_slice<T2, O2>(&mut self, src: &BitSlice<T2, O2>)
	where
		T2: BitStore,
		O2: BitOrder,
	{
		self.clone_from_bitslice(src);
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.copy_from_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn copy_from_slice(&mut self, src: &Self) {
		self.copy_from_bitslice(src)
	}

	/// Copies a span of bits to another location in the bit-slice.
	///
	/// `src` is the range of bit-indices in the bit-slice to copy, and `dest is
	/// the starting index of the destination range. `src` and `dest .. dest +
	/// src.len()` are permitted to overlap; the copy will automatically detect
	/// and manage this. However, both `src` and `dest .. dest + src.len()`
	/// **must** fall within the bounds of `self`.
	///
	/// ## Original
	///
	/// [`slice::copy_within`](https://doc.rust-lang.org/std/primitive.slice.html#method.copy_within)
	///
	/// ## Panics
	///
	/// This panics if either the source or destination range exceed
	/// `self.len()`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0];
	/// bits.copy_within(1 .. 5, 8);
	/// //                        v  v  v  v
	/// assert_eq!(bits, bits![1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0]);
	/// //                                             ^  ^  ^  ^
	/// ```
	#[inline]
	pub fn copy_within<R>(&mut self, src: R, dest: usize)
	where R: RangeExt<usize> {
		let len = self.len();
		let src = src.normalize(0, len);
		self.assert_in_bounds(src.start, 0 .. len);
		self.assert_in_bounds(src.end, 0 ..= len);
		self.assert_in_bounds(dest, 0 .. len);
		self.assert_in_bounds(dest + src.len(), 0 ..= len);
		unsafe {
			self.copy_within_unchecked(src, dest);
		}
	}

	#[inline]
	#[deprecated = "use `.swap_with_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn swap_with_slice<T2, O2>(&mut self, other: &mut BitSlice<T2, O2>)
	where
		T2: BitStore,
		O2: BitOrder,
	{
		self.swap_with_bitslice(other);
	}

	/// Produces bit-slice view(s) with different underlying storage types.
	///
	/// This may have unexpected effects, and you cannot assume that
	/// `before[idx] == after[idx]`! Consult the [tables in the manual][layout]
	/// for information about memory layouts.
	///
	/// ## Original
	///
	/// [`slice::align_to`](https://doc.rust-lang.org/std/primitive.slice.html#method.align_to)
	///
	/// ## Notes
	///
	/// Unlike the standard library documentation, this explicitly guarantees
	/// that the middle bit-slice will have maximal size. You may rely on this
	/// property.
	///
	/// ## Safety
	///
	/// You may not use this to cast away alias protections. Rust does not have
	/// support for higher-kinded types, so this cannot express the relation
	/// `Outer<T> -> Outer<U> where Outer: BitStoreContainer`, but memory safety
	/// does require that you respect this rule. Reälign integers to integers,
	/// `Cell`s to `Cell`s, and atomics to atomics, but do not cross these
	/// boundaries.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bytes: [u8; 7] = [1, 2, 3, 4, 5, 6, 7];
	/// let bits = bytes.view_bits::<Lsb0>();
	/// let (pfx, mid, sfx) = unsafe {
	///   bits.align_to::<u16>()
	/// };
	/// assert!(pfx.len() <= 8);
	/// assert_eq!(mid.len(), 48);
	/// assert!(sfx.len() <= 8);
	/// ```
	///
	/// [layout]: https://bitvecto-rs.github.io/bitvec/memory-layout.html
	#[inline]
	pub unsafe fn align_to<U>(&self) -> (&Self, &BitSlice<U, O>, &Self)
	where U: BitStore {
		let (l, c, r) = self.as_bitspan().align_to::<U>();
		(
			l.into_bitslice_ref(),
			c.into_bitslice_ref(),
			r.into_bitslice_ref(),
		)
	}

	/// Produces bit-slice view(s) with different underlying storage types.
	///
	/// This may have unexpected effects, and you cannot assume that
	/// `before[idx] == after[idx]`! Consult the [tables in the manual][layout]
	/// for information about memory layouts.
	///
	/// ## Original
	///
	/// [`slice::align_to_mut`](https://doc.rust-lang.org/std/primitive.slice.html#method.align_to_mut)
	///
	/// ## Notes
	///
	/// Unlike the standard library documentation, this explicitly guarantees
	/// that the middle bit-slice will have maximal size. You may rely on this
	/// property.
	///
	/// ## Safety
	///
	/// You may not use this to cast away alias protections. Rust does not have
	/// support for higher-kinded types, so this cannot express the relation
	/// `Outer<T> -> Outer<U> where Outer: BitStoreContainer`, but memory safety
	/// does require that you respect this rule. Reälign integers to integers,
	/// `Cell`s to `Cell`s, and atomics to atomics, but do not cross these
	/// boundaries.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bytes: [u8; 7] = [1, 2, 3, 4, 5, 6, 7];
	/// let bits = bytes.view_bits_mut::<Lsb0>();
	/// let (pfx, mid, sfx) = unsafe {
	///   bits.align_to_mut::<u16>()
	/// };
	/// assert!(pfx.len() <= 8);
	/// assert_eq!(mid.len(), 48);
	/// assert!(sfx.len() <= 8);
	/// ```
	///
	/// [layout]: https://bitvecto-rs.github.io/bitvec/memory-layout.html
	#[inline]
	pub unsafe fn align_to_mut<U>(
		&mut self,
	) -> (&mut Self, &mut BitSlice<U, O>, &mut Self)
	where U: BitStore {
		let (l, c, r) = self.as_mut_bitspan().align_to::<U>();
		(
			l.into_bitslice_mut(),
			c.into_bitslice_mut(),
			r.into_bitslice_mut(),
		)
	}
}

#[cfg(feature = "alloc")]
impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	#[deprecated = "use `.to_bitvec()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn to_vec(&self) -> BitVec<T::Unalias, O> {
		self.to_bitvec()
	}

	/// Creates a bit-vector by repeating a bit-slice `n` times.
	///
	/// ## Original
	///
	/// [`slice::repeat`](https://doc.rust-lang.org/std/primitive.slice.html#method.repeat)
	///
	/// ## Panics
	///
	/// This method panics if `self.len() * n` exceeds the `BitVec` capacity.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert_eq!(bits![0, 1].repeat(3), bitvec![0, 1, 0, 1, 0, 1]);
	/// ```
	///
	/// This panics by exceeding bit-vector maximum capacity:
	///
	/// ```rust,should_panic
	/// use bitvec::prelude::*;
	///
	/// bits![0, 1].repeat(BitSlice::<usize, Lsb0>::MAX_BITS);
	/// ```
	#[inline]
	pub fn repeat(&self, n: usize) -> BitVec<T::Unalias, O> {
		let len = self.len();
		let total = len.checked_mul(n).expect("capacity overflow");

		let mut out = BitVec::repeat(false, total);

		let iter = unsafe { out.chunks_exact_mut(len).remove_alias() };
		for chunk in iter {
			chunk.clone_from_bitslice(self);
		}

		out
	}

	/* As of 1.56, the `concat` and `join` methods use still-unstable traits
	 * to govern the collection of multiple subslices into one vector. These
	 * are possible to copy over and redefine locally, but unless a user asks
	 * for it, doing so is considered a low priority.
	 */
}

#[inline]
#[allow(missing_docs, clippy::missing_docs_in_private_items)]
#[deprecated = "use `BitSlice::from_element()` instead"]
pub fn from_ref<T, O>(elem: &T) -> &BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	BitSlice::from_element(elem)
}

#[inline]
#[allow(missing_docs, clippy::missing_docs_in_private_items)]
#[deprecated = "use `BitSlice::from_element_mut()` instead"]
pub fn from_mut<T, O>(elem: &mut T) -> &mut BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	BitSlice::from_element_mut(elem)
}

#[inline]
#[doc = include_str!("../../doc/slice/from_raw_parts.md")]
pub unsafe fn from_raw_parts<'a, T, O>(
	data: BitPtr<Const, T, O>,
	len: usize,
) -> Result<&'a BitSlice<T, O>, BitSpanError<T>>
where
	O: BitOrder,
	T: 'a + BitStore,
{
	data.span(len).map(|bp| bp.into_bitslice_ref())
}

#[inline]
#[doc = include_str!("../../doc/slice/from_raw_parts_mut.md")]
pub unsafe fn from_raw_parts_mut<'a, T, O>(
	data: BitPtr<Mut, T, O>,
	len: usize,
) -> Result<&'a mut BitSlice<T, O>, BitSpanError<T>>
where
	O: BitOrder,
	T: 'a + BitStore,
{
	data.span(len).map(|bp| bp.into_bitslice_mut())
}

#[doc = include_str!("../../doc/slice/BitSliceIndex.md")]
pub trait BitSliceIndex<'a, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// The output type of immutable access.
	type Immut;

	/// The output type of mutable access.
	type Mut;

	/// Immutably indexes into a bit-slice, returning `None` if `self` is out of
	/// bounds.
	///
	/// ## Original
	///
	/// [`SliceIndex::get`](core::slice::SliceIndex::get)
	fn get(self, bits: &'a BitSlice<T, O>) -> Option<Self::Immut>;

	/// Mutably indexes into a bit-slice, returning `None` if `self` is out of
	/// bounds.
	///
	/// ## Original
	///
	/// [`SliceIndex::get_mut`](core::slice::SliceIndex::get_mut)
	fn get_mut(self, bits: &'a mut BitSlice<T, O>) -> Option<Self::Mut>;

	/// Immutably indexes into a bit-slice without doing any bounds checking.
	///
	/// ## Original
	///
	/// [`SliceIndex::get_unchecked`](core::slice::SliceIndex::get_unchecked)
	///
	/// ## Safety
	///
	/// If `self` is not in bounds, then memory accesses through it are illegal
	/// and the program becomes undefined. You must ensure that `self` is
	/// appropriately within `0 .. bits.len()` at the call site.
	unsafe fn get_unchecked(self, bits: &'a BitSlice<T, O>) -> Self::Immut;

	/// Mutably indexes into a bit-slice without doing any bounds checking.
	///
	/// ## Original
	///
	/// [`SliceIndex::get_unchecked_mut`][0]
	///
	/// ## Safety
	///
	/// If `self` is not in bounds, then memory accesses through it bare illegal
	/// and the program becomes undefined. You must ensure that `self` is
	/// appropriately within `0 .. bits.len()` at the call site.
	///
	/// [0]: core::slice::SliceIndex::get_unchecked_mut
	unsafe fn get_unchecked_mut(self, bits: &'a mut BitSlice<T, O>)
	-> Self::Mut;

	/// Immutably indexes into a bit-slice, panicking if `self` is out of
	/// bounds.
	///
	/// ## Original
	///
	/// [`SliceIndex::index`](core::slice::SliceIndex::index)
	///
	/// ## Panics
	///
	/// Implementations are required to panic if `self` exceeds `bits.len()` in
	/// any way.
	fn index(self, bits: &'a BitSlice<T, O>) -> Self::Immut;

	/// Mutably indexes into a bit-slice, panicking if `self` is out of bounds.
	///
	/// ## Original
	///
	/// [`SliceIndex::index_mut`](core::slice::SliceIndex::index_mut)
	///
	/// ## Panics
	///
	/// Implementations are required to panic if `self` exceeds `bits.len()` in
	/// any way.
	fn index_mut(self, bits: &'a mut BitSlice<T, O>) -> Self::Mut;
}

impl<'a, T, O> BitSliceIndex<'a, T, O> for usize
where
	T: BitStore,
	O: BitOrder,
{
	type Immut = BitRef<'a, Const, T, O>;
	type Mut = BitRef<'a, Mut, T, O>;

	#[inline]
	fn get(self, bits: &'a BitSlice<T, O>) -> Option<Self::Immut> {
		if self < bits.len() {
			Some(unsafe { self.get_unchecked(bits) })
		}
		else {
			None
		}
	}

	#[inline]
	fn get_mut(self, bits: &'a mut BitSlice<T, O>) -> Option<Self::Mut> {
		if self < bits.len() {
			Some(unsafe { self.get_unchecked_mut(bits) })
		}
		else {
			None
		}
	}

	#[inline]
	unsafe fn get_unchecked(self, bits: &'a BitSlice<T, O>) -> Self::Immut {
		bits.as_bitptr().add(self).as_ref().unwrap()
	}

	#[inline]
	unsafe fn get_unchecked_mut(
		self,
		bits: &'a mut BitSlice<T, O>,
	) -> Self::Mut {
		bits.as_mut_bitptr().add(self).as_mut().unwrap()
	}

	#[inline]
	fn index(self, bits: &'a BitSlice<T, O>) -> Self::Immut {
		self.get(bits).unwrap_or_else(|| {
			panic!("index {} out of bounds: {}", self, bits.len())
		})
	}

	#[inline]
	fn index_mut(self, bits: &'a mut BitSlice<T, O>) -> Self::Mut {
		let len = bits.len();
		self.get_mut(bits)
			.unwrap_or_else(|| panic!("index {} out of bounds: {}", self, len))
	}
}

/// Implements indexing on bit-slices by various range types.
macro_rules! range_impl {
	($r:ty { check $check:expr; select $select:expr; }) => {
		#[allow(clippy::redundant_closure_call)]
		impl<'a, T, O> BitSliceIndex<'a, T, O> for $r
		where
			O: BitOrder,
			T: BitStore,
		{
			type Immut = &'a BitSlice<T, O>;
			type Mut = &'a mut BitSlice<T, O>;

			#[inline]
			#[allow(
				clippy::blocks_in_if_conditions,
				clippy::redundant_closure_call
			)]
			fn get(self, bits: Self::Immut) -> Option<Self::Immut> {
				if ($check)(self.clone(), bits.as_bitspan()) {
					Some(unsafe { self.get_unchecked(bits) })
				}
				else {
					None
				}
			}

			#[inline]
			#[allow(
				clippy::blocks_in_if_conditions,
				clippy::redundant_closure_call
			)]
			fn get_mut(self, bits: Self::Mut) -> Option<Self::Mut> {
				if ($check)(self.clone(), bits.as_bitspan()) {
					Some(unsafe { self.get_unchecked_mut(bits) })
				}
				else {
					None
				}
			}

			#[inline]
			#[allow(clippy::redundant_closure_call)]
			unsafe fn get_unchecked(self, bits: Self::Immut) -> Self::Immut {
				($select)(self, bits.as_bitspan()).into_bitslice_ref()
			}

			#[inline]
			#[allow(clippy::redundant_closure_call)]
			unsafe fn get_unchecked_mut(self, bits: Self::Mut) -> Self::Mut {
				($select)(self, bits.as_mut_bitspan()).into_bitslice_mut()
			}

			#[inline]
			#[track_caller]
			fn index(self, bits: Self::Immut) -> Self::Immut {
				let r = self.clone();
				let l = bits.len();
				self.get(bits).unwrap_or_else(|| {
					panic!("range {:?} out of bounds: {}", r, l)
				})
			}

			#[inline]
			#[track_caller]
			fn index_mut(self, bits: Self::Mut) -> Self::Mut {
				let r = self.clone();
				let l = bits.len();
				self.get_mut(bits).unwrap_or_else(|| {
					panic!("range {:?} out of bounds: {}", r, l)
				})
			}
		}
	};
}

range_impl!(Range<usize> {
	check |Range { start, end }, span: BitSpan<_, _, _>| {
		let len = span.len();
		start <= len && end <= len && start <= end
	};

	select |Range { start, end }, span: BitSpan<_, _, _>| {
		span.to_bitptr().add(start).span_unchecked(end - start)
	};
});

range_impl!(RangeFrom<usize> {
	check |RangeFrom { start }, span: BitSpan<_, _, _>| {
		start <= span.len()
	};

	select |RangeFrom { start }, span: BitSpan<_, _, _>| {
		span.to_bitptr().add(start).span_unchecked(span.len() - start)
	};
});

range_impl!(RangeTo<usize> {
	check |RangeTo { end }, span: BitSpan<_, _, _>| {
		end <= span.len()
	};

	select |RangeTo { end }, mut span: BitSpan<_, _, _>| {
		span.set_len(end);
		span
	};
});

range_impl!(RangeInclusive<usize> {
	check |range: Self, span: BitSpan<_, _, _>| {
		let len = span.len();
		let start = *range.start();
		let end = *range.end();

		start < len && end < len && start <= end
	};

	select |range: Self, span: BitSpan<_, _, _>| {
		let start = *range.start();
		let end = *range.end();
		span.to_bitptr().add(start).span_unchecked(end + 1 - start)
	};
});

range_impl!(RangeToInclusive<usize> {
	check |RangeToInclusive { end }, span: BitSpan<_, _, _>| {
		end < span.len()
	};

	select |RangeToInclusive { end }, mut span: BitSpan<_, _, _>| {
		span.set_len(end + 1);
		span
	};
});

#[cfg(not(tarpaulin_include))]
impl<'a, T, O> BitSliceIndex<'a, T, O> for RangeFull
where
	T: BitStore,
	O: BitOrder,
{
	type Immut = &'a BitSlice<T, O>;
	type Mut = &'a mut BitSlice<T, O>;

	#[inline]
	fn get(self, bits: Self::Immut) -> Option<Self::Immut> {
		Some(bits)
	}

	#[inline]
	fn get_mut(self, bits: Self::Mut) -> Option<Self::Mut> {
		Some(bits)
	}

	#[inline]
	unsafe fn get_unchecked(self, bits: Self::Immut) -> Self::Immut {
		bits
	}

	#[inline]
	unsafe fn get_unchecked_mut(self, bits: Self::Mut) -> Self::Mut {
		bits
	}

	#[inline]
	fn index(self, bits: Self::Immut) -> Self::Immut {
		bits
	}

	#[inline]
	fn index_mut(self, bits: Self::Mut) -> Self::Mut {
		bits
	}
}
