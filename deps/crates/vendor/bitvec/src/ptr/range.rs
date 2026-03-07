#![doc = include_str!("../../doc/ptr/range.md")]

use core::{
	fmt::{
		self,
		Debug,
		Formatter,
	},
	hash::{
		Hash,
		Hasher,
	},
	iter::FusedIterator,
	ops::{
		Bound,
		Range,
		RangeBounds,
	},
};

use wyz::comu::{
	Const,
	Mutability,
};

use super::{
	BitPtr,
	BitSpan,
};
use crate::{
	devel as dvl,
	order::{
		BitOrder,
		Lsb0,
	},
	store::BitStore,
};

#[repr(C)]
#[doc = include_str!("../../doc/ptr/BitPtrRange.md")]
pub struct BitPtrRange<M = Const, T = usize, O = Lsb0>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// The lower, inclusive, bound of the range. The bit to which this points
	/// is considered live.
	pub start: BitPtr<M, T, O>,
	/// The higher, exclusive, bound of the range. The bit to which this points
	/// is considered dead, and the pointer may be one bit beyond the bounds of
	/// an allocation region.
	///
	/// Because Rust and LLVM both define the address of `base + (len * width)`
	/// as being within the provenance of `base`, even though that address may
	/// itself be the base address of another region in a different provenance,
	/// and bit-pointers are always composed of an ordinary memory address and a
	/// bit-counter, the ending bit-pointer is always valid.
	pub end:   BitPtr<M, T, O>,
}

impl<M, T, O> BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// The canonical empty range. All ranges with zero length (equal `.start`
	/// and `.end`) are equally empty.
	pub const EMPTY: Self = Self {
		start: BitPtr::DANGLING,
		end:   BitPtr::DANGLING,
	};

	/// Explicitly converts a `Range<BitPtr>` into a `BitPtrRange`.
	#[inline]
	pub fn from_range(Range { start, end }: Range<BitPtr<M, T, O>>) -> Self {
		Self { start, end }
	}

	/// Explicitly converts a `BitPtrRange` into a `Range<BitPtr>`.
	#[inline]
	pub fn into_range(self) -> Range<BitPtr<M, T, O>> {
		let Self { start, end } = self;
		start .. end
	}

	/// Tests if the range is empty (the distance between bit-pointers is `0`).
	///
	/// ## Original
	///
	/// [`Range::is_empty`](core::ops::Range::is_empty)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	/// use bitvec::ptr::BitPtrRange;
	///
	/// let data = 0u8;
	/// let bp = BitPtr::<_, _, Lsb0>::from_ref(&data);
	/// let mut range = BitPtrRange::from_range(bp .. bp.wrapping_add(1));
	///
	/// assert!(!range.is_empty());
	/// assert_ne!(range.start, range.end);
	///
	/// range.next();
	///
	/// assert!(range.is_empty());
	/// assert_eq!(range.start, range.end);
	/// ```
	#[inline]
	pub fn is_empty(&self) -> bool {
		self.start == self.end
	}

	/// Tests if a given bit-pointer is contained within the range.
	///
	/// Bit-pointer ordering is defined when the types have the same exact
	/// `BitOrder` type parameter and the same `BitStore::Mem` associated type
	/// (but are free to differ in alias condition!). Inclusion in a range
	/// occurs when the bit-pointer is not strictly less than the range start,
	/// and is strictly less than the range end.
	///
	/// ## Original
	///
	/// [`Range::contains`](core::ops::Range::contains)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	/// use bitvec::ptr::BitPtrRange;
	/// use core::cell::Cell;
	///
	/// let data = 0u16;
	/// let bp = BitPtr::<_, _, Lsb0>::from_ref(&data);
	///
	/// let mut range = BitPtrRange::from_range(bp .. bp.wrapping_add(16));
	/// range.nth(2);
	/// range.nth_back(2);
	///
	/// assert!(bp < range.start);
	/// assert!(!range.contains(&bp));
	///
	/// let mid = bp.wrapping_add(8);
	///
	/// let same_mem = mid.cast::<Cell<u16>>();
	/// assert!(range.contains(&mid));
	/// ```
	///
	/// Casting to a different `BitStore` type whose `Mem` parameter differs
	/// from the range always results in a `false` response, even if the pointer
	/// being tested is numerically within the range.
	#[inline]
	pub fn contains<M2, T2>(&self, pointer: &BitPtr<M2, T2, O>) -> bool
	where
		M2: Mutability,
		T2: BitStore,
	{
		dvl::match_store::<T::Mem, T2::Mem>()
			&& self.start <= *pointer
			&& *pointer < self.end
	}

	/// Converts the range into a span descriptor over all live bits.
	///
	/// The produced bit-span does *not* include the bit addressed by `.end`.
	///
	/// ## Safety
	///
	/// The `.start` and `.end` bit-pointers must both be derived from the same
	/// provenance region. `BitSpan` draws its provenance from the `.start`
	/// element pointer, and incorrectly extending it beyond the source
	/// provenance is undefined behavior.
	pub(crate) unsafe fn into_bitspan(self) -> BitSpan<M, T, O> {
		self.start.span_unchecked(self.len())
	}

	/// Snapshots `.start`, then increments it.
	///
	/// This method is only safe to call when the range is non-empty.
	#[inline]
	fn take_front(&mut self) -> BitPtr<M, T, O> {
		let start = self.start;
		self.start = start.wrapping_add(1);
		start
	}

	/// Decrements `.end`, then returns it.
	///
	/// The bit-pointer returned by this method is always to an alive bit.
	///
	/// This method is only safe to call when the range is non-empty.
	#[inline]
	fn take_back(&mut self) -> BitPtr<M, T, O> {
		let prev = self.end.wrapping_sub(1);
		self.end = prev;
		prev
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Clone for BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn clone(&self) -> Self {
		Self { ..*self }
	}
}

impl<M, T, O> Eq for BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
}

impl<M1, M2, O, T1, T2> PartialEq<BitPtrRange<M2, T2, O>>
	for BitPtrRange<M1, T1, O>
where
	M1: Mutability,
	M2: Mutability,
	O: BitOrder,
	T1: BitStore,
	T2: BitStore,
{
	#[inline]
	fn eq(&self, other: &BitPtrRange<M2, T2, O>) -> bool {
		//  Pointers over different element types are never equal
		dvl::match_store::<T1::Mem, T2::Mem>()
			&& self.start == other.start
			&& self.end == other.end
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Default for BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn default() -> Self {
		Self::EMPTY
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> From<Range<BitPtr<M, T, O>>> for BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(range: Range<BitPtr<M, T, O>>) -> Self {
		Self::from_range(range)
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> From<BitPtrRange<M, T, O>> for Range<BitPtr<M, T, O>>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(range: BitPtrRange<M, T, O>) -> Self {
		range.into_range()
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Debug for BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		let Range { start, end } = self.clone().into_range();
		Debug::fmt(&start, fmt)?;
		write!(fmt, "{0}..{0}", if fmt.alternate() { " " } else { "" })?;
		Debug::fmt(&end, fmt)
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Hash for BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn hash<H>(&self, state: &mut H)
	where H: Hasher {
		self.start.hash(state);
		self.end.hash(state);
	}
}

impl<M, T, O> Iterator for BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	type Item = BitPtr<M, T, O>;

	easy_iter!();

	#[inline]
	fn next(&mut self) -> Option<Self::Item> {
		if Self::is_empty(&*self) {
			return None;
		}
		Some(self.take_front())
	}

	#[inline]
	fn nth(&mut self, n: usize) -> Option<Self::Item> {
		if n >= self.len() {
			self.start = self.end;
			return None;
		}
		self.start = unsafe { self.start.add(n) };
		Some(self.take_front())
	}
}

impl<M, T, O> DoubleEndedIterator for BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn next_back(&mut self) -> Option<Self::Item> {
		if Self::is_empty(&*self) {
			return None;
		}
		Some(self.take_back())
	}

	#[inline]
	fn nth_back(&mut self, n: usize) -> Option<Self::Item> {
		if n >= self.len() {
			self.end = self.start;
			return None;
		}
		let out = unsafe { self.end.sub(n.wrapping_add(1)) };
		self.end = out;
		Some(out)
	}
}

impl<M, T, O> ExactSizeIterator for BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn len(&self) -> usize {
		(unsafe { self.end.offset_from(self.start) }) as usize
	}
}

impl<M, T, O> FusedIterator for BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> RangeBounds<BitPtr<M, T, O>> for BitPtrRange<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn start_bound(&self) -> Bound<&BitPtr<M, T, O>> {
		Bound::Included(&self.start)
	}

	#[inline]
	fn end_bound(&self) -> Bound<&BitPtr<M, T, O>> {
		Bound::Excluded(&self.end)
	}
}
