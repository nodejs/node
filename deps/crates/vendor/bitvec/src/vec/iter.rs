#![doc = include_str!("../../doc/vec/iter.md")]

use alloc::vec::Vec;
use core::{
	fmt::{
		self,
		Debug,
		Formatter,
	},
	iter::{
		FromIterator,
		FusedIterator,
	},
	mem,
	ops::Range,
};

use tap::{
	Pipe,
	Tap,
	TapOptional,
};
use wyz::{
	comu::{
		Mut,
		Mutability,
	},
	range::RangeExt,
};

use super::BitVec;
use crate::{
	boxed::BitBox,
	mem::bits_of,
	order::BitOrder,
	ptr::{
		BitPtrRange,
		BitRef,
	},
	slice::BitSlice,
	store::BitStore,
	view::BitView,
};

#[doc = include_str!("../../doc/vec/iter/Extend_bool.md")]
impl<T, O> Extend<bool> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn extend<I>(&mut self, iter: I)
	where I: IntoIterator<Item = bool> {
		let mut iter = iter.into_iter();
		#[allow(irrefutable_let_patterns)] // Removing the `if` is unstable.
		if let (_, Some(n)) | (n, None) = iter.size_hint() {
			self.reserve(n);
			let len = self.len();
			//  If the reservation did not panic, then this will not overflow.
			let new_len = len.wrapping_add(n);
			let new = unsafe { self.get_unchecked_mut(len .. new_len) };

			let pulled = new
				.as_mut_bitptr_range()
				.zip(iter.by_ref())
				.map(|(ptr, bit)| unsafe {
					ptr.write(bit);
				})
				.count();
			unsafe {
				self.set_len(len + pulled);
			}
		}

		//  If the iterator is well-behaved and finite, this should never
		//  enter; if the iterator is infinite, then this will eventually crash.
		iter.for_each(|bit| self.push(bit));
	}
}

#[cfg(not(tarpaulin_include))]
impl<'a, T, O> Extend<&'a bool> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn extend<I>(&mut self, iter: I)
	where I: IntoIterator<Item = &'a bool> {
		self.extend(iter.into_iter().copied());
	}
}

#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../../doc/vec/iter/Extend_BitRef.md")]
impl<'a, M, T1, T2, O1, O2> Extend<BitRef<'a, M, T2, O2>> for BitVec<T1, O1>
where
	M: Mutability,
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn extend<I>(&mut self, iter: I)
	where I: IntoIterator<Item = BitRef<'a, M, T2, O2>> {
		self.extend(iter.into_iter().map(|bit| *bit));
	}
}

impl<T, O> Extend<T> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn extend<I>(&mut self, iter: I)
	where I: IntoIterator<Item = T> {
		let iter = iter.into_iter();
		#[allow(irrefutable_let_patterns)]
		if let (_, Some(n)) | (n, None) = iter.size_hint() {
			self.reserve(n.checked_mul(bits_of::<T::Mem>()).unwrap());
		}
		iter.for_each(|elem| self.extend_from_bitslice(elem.view_bits::<O>()));
	}
}

#[cfg(not(tarpaulin_include))]
impl<'a, T, O> Extend<&'a T> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn extend<I>(&mut self, iter: I)
	where I: IntoIterator<Item = &'a T> {
		self.extend(
			iter.into_iter()
				.map(BitStore::load_value)
				.map(<T as BitStore>::new),
		);
	}
}

#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../../doc/vec/iter/FromIterator_bool.md")]
impl<T, O> FromIterator<bool> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from_iter<I>(iter: I) -> Self
	where I: IntoIterator<Item = bool> {
		Self::new().tap_mut(|bv| bv.extend(iter))
	}
}

#[cfg(not(tarpaulin_include))]
impl<'a, T, O> FromIterator<&'a bool> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from_iter<I>(iter: I) -> Self
	where I: IntoIterator<Item = &'a bool> {
		iter.into_iter().copied().collect::<Self>()
	}
}

#[cfg(not(tarpaulin_include))]
#[doc = include_str!("../../doc/vec/iter/FromIterator_BitRef.md")]
impl<'a, M, T1, T2, O1, O2> FromIterator<BitRef<'a, M, T2, O2>>
	for BitVec<T1, O1>
where
	M: Mutability,
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn from_iter<I>(iter: I) -> Self
	where I: IntoIterator<Item = BitRef<'a, M, T2, O2>> {
		iter.into_iter().map(|br| *br).pipe(Self::from_iter)
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> FromIterator<T> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from_iter<I>(iter: I) -> Self
	where I: IntoIterator<Item = T> {
		iter.into_iter().collect::<Vec<T>>().pipe(Self::from_vec)
	}
}

#[cfg(not(tarpaulin_include))]
impl<'a, T, O> FromIterator<&'a T> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from_iter<I>(iter: I) -> Self
	where I: IntoIterator<Item = &'a T> {
		iter.into_iter()
			.map(<T as BitStore>::load_value)
			.map(<T as BitStore>::new)
			.collect::<Self>()
	}
}

#[doc = include_str!("../../doc/vec/iter/IntoIterator.md")]
impl<T, O> IntoIterator for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type IntoIter = <BitBox<T, O> as IntoIterator>::IntoIter;
	type Item = <BitBox<T, O> as IntoIterator>::Item;

	#[inline]
	fn into_iter(self) -> Self::IntoIter {
		self.into_boxed_bitslice().into_iter()
	}
}

#[cfg(not(tarpaulin_include))]
/// [Original](https://doc.rust-lang.org/alloc/vec/struct.Vec.html#impl-IntoIterator-1)
impl<'a, T, O> IntoIterator for &'a BitVec<T, O>
where
	O: BitOrder,
	T: 'a + BitStore,
{
	type IntoIter = <&'a BitSlice<T, O> as IntoIterator>::IntoIter;
	type Item = <&'a BitSlice<T, O> as IntoIterator>::Item;

	#[inline]
	fn into_iter(self) -> Self::IntoIter {
		self.as_bitslice().iter()
	}
}

#[cfg(not(tarpaulin_include))]
/// [Original](https://doc.rust-lang.org/alloc/vec/struct.Vec.html#impl-IntoIterator-2)
impl<'a, T, O> IntoIterator for &'a mut BitVec<T, O>
where
	O: BitOrder,
	T: 'a + BitStore,
{
	type IntoIter = <&'a mut BitSlice<T, O> as IntoIterator>::IntoIter;
	type Item = <&'a mut BitSlice<T, O> as IntoIterator>::Item;

	#[inline]
	fn into_iter(self) -> Self::IntoIter {
		self.as_mut_bitslice().iter_mut()
	}
}

#[doc = include_str!("../../doc/vec/iter/Drain.md")]
pub struct Drain<'a, T, O>
where
	O: BitOrder,
	T: 'a + BitStore,
{
	/// Exclusive reference to the handle that created the drain.
	source: &'a mut BitVec<T, O>,
	/// The range of the source bit-vector’s buffer that is being drained.
	drain:  BitPtrRange<Mut, T, O>,
	/// The range of the source bit-vector’s preserved back section. This runs
	/// from the first bit after the `.drain` to the first bit after the
	/// original bit-vector ends.
	tail:   Range<usize>,
}

impl<'a, T, O> Drain<'a, T, O>
where
	O: BitOrder,
	T: 'a + BitStore,
{
	/// Produces a new drain over a region of a bit-vector.
	pub(super) fn new<R>(source: &'a mut BitVec<T, O>, range: R) -> Self
	where R: RangeExt<usize> {
		let len = source.len();
		let region = range.normalize(None, len);
		assert!(
			region.end <= len,
			"drains cannot extend past the length of their source bit-vector",
		);

		//  The `.tail` region is everything in the bit-vector after the drain.
		let tail = region.end .. len;
		let drain = unsafe {
			//  Artificially truncate the source bit-vector to before the drain
			//  region. This is restored in the destructor.
			source.set_len_unchecked(region.start);
			let base = source.as_mut_bitptr();
			BitPtrRange {
				start: base.add(region.start),
				end:   base.add(region.end),
			}
		};

		Self {
			source,
			drain,
			tail,
		}
	}

	/// Views the unyielded bits remaining in the drain.
	///
	/// ## Original
	///
	/// [`Drain::as_slice`](alloc::vec::Drain::as_slice)
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn as_bitslice(&self) -> &'a BitSlice<T, O> {
		unsafe { self.drain.clone().into_bitspan().into_bitslice_ref() }
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_slice(&self) -> &'a BitSlice<T, O> {
		self.as_bitslice()
	}

	/// Attempts to fill the `drain` region with the contents of another
	/// iterator.
	///
	/// The source bit-vector is extended to include each bit that the
	/// replacement iterator provides, but is *not yet* extended to include the
	/// `tail` region, even if the replacement iterator completely fills the
	/// `drain` region. That work occurs in the destructor.
	///
	/// This is only used by [`Splice`].
	///
	/// [`Splice`]: crate::vec::Splice
	#[inline]
	fn fill(&mut self, iter: &mut impl Iterator<Item = bool>) -> FillStatus {
		let bv = &mut *self.source;
		let mut len = bv.len();
		let span =
			unsafe { bv.as_mut_bitptr().add(len).range(self.tail.start - len) };

		let mut out = FillStatus::FullSpan;
		for ptr in span {
			if let Some(bit) = iter.next() {
				unsafe {
					ptr.write(bit);
				}
				len += 1;
			}
			else {
				out = FillStatus::EmptyInput;
				break;
			}
		}
		unsafe {
			bv.set_len_unchecked(len);
		}
		out
	}

	/// Reserves space for `additional` more bits at the end of the `drain`
	/// region by moving the `tail` region upwards in memory.
	///
	/// This has the same effects as [`BitVec::resize`], except that the bits
	/// are inserted between `drain` and `tail` rather than at the end.
	///
	/// This does not modify the drain iteration cursor, including its endpoint.
	/// The newly inserted bits are not available for iteration.
	///
	/// This is only used by [`Splice`], which may insert more bits than the
	/// drain removed.
	///
	/// [`BitVec::resize`]: crate::vec::BitVec::resize
	/// [`Splice`]: crate::vec::Splice
	unsafe fn move_tail(&mut self, additional: usize) {
		if additional == 0 {
			return;
		}

		let bv = &mut *self.source;
		let tail_len = self.tail.len();

		let full_len = additional + tail_len;
		bv.reserve(full_len);
		let new_tail_start = additional + self.tail.start;
		let orig_tail = mem::replace(
			&mut self.tail,
			new_tail_start .. new_tail_start + tail_len,
		);
		let len = bv.len();
		bv.set_len_unchecked(full_len);
		bv.copy_within_unchecked(orig_tail, new_tail_start);
		bv.set_len_unchecked(len);
	}
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.Drain.html#impl-AsRef%3C%5BT%5D%3E)
#[cfg(not(tarpaulin_include))]
impl<T, O> AsRef<BitSlice<T, O>> for Drain<'_, T, O>
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
impl<T, O> Debug for Drain<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_tuple("Drain").field(&self.as_bitslice()).finish()
	}
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.Drain.html#impl-Iterator)
#[cfg(not(tarpaulin_include))]
impl<T, O> Iterator for Drain<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Item = bool;

	easy_iter!();

	#[inline]
	fn next(&mut self) -> Option<Self::Item> {
		self.drain.next().map(|bp| unsafe { bp.read() })
	}

	#[inline]
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		self.drain.nth(n).map(|bp| unsafe { bp.read() })
	}
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.Drain.html#impl-DoubleEndedIterator)
#[cfg(not(tarpaulin_include))]
impl<T, O> DoubleEndedIterator for Drain<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn next_back(&mut self) -> Option<Self::Item> {
		self.drain.next_back().map(|bp| unsafe { bp.read() })
	}

	#[inline]
	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		self.drain.nth_back(n).map(|bp| unsafe { bp.read() })
	}
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.Drain.html#impl-ExactSizeIterator)
#[cfg(not(tarpaulin_include))]
impl<T, O> ExactSizeIterator for Drain<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn len(&self) -> usize {
		self.drain.len()
	}
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.Drain.html#impl-FusedIterator)
impl<T, O> FusedIterator for Drain<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.Drain.html#impl-Send)
// #[allow(clippy::non_send_fields_in_send_ty)]
unsafe impl<T, O> Send for Drain<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
	for<'a> &'a mut BitSlice<T, O>: Send,
{
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.Drain.html#impl-Sync)
unsafe impl<T, O> Sync for Drain<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: Sync,
{
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.Drain.html#impl-Drop)
impl<T, O> Drop for Drain<'_, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn drop(&mut self) {
		let tail = mem::take(&mut self.tail);
		let tail_len = tail.len();
		if tail_len == 0 {
			return;
		}

		let bv = &mut *self.source;
		let old_len = bv.len();
		unsafe {
			bv.set_len_unchecked(tail.end);
			bv.copy_within_unchecked(tail, old_len);
			bv.set_len_unchecked(old_len + tail_len);
		}
	}
}

#[repr(u8)]
#[doc = include_str!("../../doc/vec/iter/FillStatus.md")]
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
enum FillStatus {
	/// The drain span is completely filled.
	FullSpan   = 0,
	/// The replacement source is completely exhausted.
	EmptyInput = 1,
}

#[derive(Debug)]
#[doc = include_str!("../../doc/vec/iter/Splice.md")]
pub struct Splice<'a, T, O, I>
where
	O: BitOrder,
	T: 'a + BitStore,
	I: Iterator<Item = bool>,
{
	/// The region of the bit-vector being drained.
	drain:  Drain<'a, T, O>,
	/// The bitstream that replaces drained bits.
	splice: I,
}

impl<'a, T, O, I> Splice<'a, T, O, I>
where
	O: BitOrder,
	T: 'a + BitStore,
	I: Iterator<Item = bool>,
{
	/// Constructs a splice out of a drain and a replacement source.
	pub(super) fn new(
		drain: Drain<'a, T, O>,
		splice: impl IntoIterator<IntoIter = I, Item = bool>,
	) -> Self {
		let splice = splice.into_iter();
		Self { drain, splice }
	}
}

impl<T, O, I> Iterator for Splice<'_, T, O, I>
where
	T: BitStore,
	O: BitOrder,
	I: Iterator<Item = bool>,
{
	type Item = bool;

	easy_iter!();

	#[inline]
	fn next(&mut self) -> Option<Self::Item> {
		self.drain.next().tap_some(|_| unsafe {
			if let Some(bit) = self.splice.next() {
				let bv = &mut *self.drain.source;
				let len = bv.len();
				bv.set_len_unchecked(len + 1);
				bv.set_unchecked(len, bit);
			}
		})
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, I> DoubleEndedIterator for Splice<'_, T, O, I>
where
	T: BitStore,
	O: BitOrder,
	I: Iterator<Item = bool>,
{
	#[inline]
	fn next_back(&mut self) -> Option<Self::Item> {
		self.drain.next_back()
	}

	#[inline]
	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		self.drain.nth_back(n)
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, I> ExactSizeIterator for Splice<'_, T, O, I>
where
	T: BitStore,
	O: BitOrder,
	I: Iterator<Item = bool>,
{
	#[inline]
	fn len(&self) -> usize {
		self.drain.len()
	}
}

impl<T, O, I> FusedIterator for Splice<'_, T, O, I>
where
	T: BitStore,
	O: BitOrder,
	I: Iterator<Item = bool>,
{
}

/// [Original](https://doc.rust-lang.org/alloc/vec/struct.Drain.html#impl-Drop)
impl<T, O, I> Drop for Splice<'_, T, O, I>
where
	T: BitStore,
	O: BitOrder,
	I: Iterator<Item = bool>,
{
	#[inline]
	fn drop(&mut self) {
		let tail = self.drain.tail.clone();
		let tail_len = tail.len();
		let bv = &mut *self.drain.source;

		if tail_len == 0 {
			bv.extend(self.splice.by_ref());
			return;
		}

		if let FillStatus::EmptyInput = self.drain.fill(&mut self.splice) {
			return;
		}

		let len = match self.splice.size_hint() {
			(n, None) | (_, Some(n)) => n,
		};

		unsafe {
			self.drain.move_tail(len);
		}
		if let FillStatus::EmptyInput = self.drain.fill(&mut self.splice) {
			return;
		}

		/* If the `.splice` *still* has bits to provide, then its
		 * `.size_hint()` is untrustworthy. Collect the `.splice` into a
		 * bit-vector, then insert the bit-vector into the spliced region.
		 */
		let mut collected =
			self.splice.by_ref().collect::<BitVec<T, O>>().into_iter();
		let len = collected.len();
		if len > 0 {
			unsafe {
				self.drain.move_tail(len);
			}
			let filled = self.drain.fill(collected.by_ref());
			debug_assert_eq!(filled, FillStatus::EmptyInput);
			debug_assert_eq!(collected.len(), 0);
		}
	}
}
