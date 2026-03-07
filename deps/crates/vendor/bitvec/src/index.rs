#![doc = include_str!("../doc/index.md")]

use core::{
	any,
	fmt::{
		self,
		Binary,
		Debug,
		Display,
		Formatter,
	},
	iter::{
		FusedIterator,
		Sum,
	},
	marker::PhantomData,
	ops::{
		BitAnd,
		BitOr,
		Not,
	},
};

use crate::{
	mem::{
		bits_of,
		BitRegister,
	},
	order::BitOrder,
};

#[repr(transparent)]
#[doc = include_str!("../doc/index/BitIdx.md")]
#[derive(Clone, Copy, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct BitIdx<R = usize>
where R: BitRegister
{
	/// Semantic index counter within a register, constrained to `0 .. R::BITS`.
	idx: u8,
	/// Marker for the register type.
	_ty: PhantomData<R>,
}

impl<R> BitIdx<R>
where R: BitRegister
{
	/// The inclusive maximum index within an `R` element.
	pub const MAX: Self = Self {
		idx: R::MASK,
		_ty: PhantomData,
	};
	/// The inclusive minimum index within an `R` element.
	pub const MIN: Self = Self {
		idx: 0,
		_ty: PhantomData,
	};

	/// Wraps a counter value as a known-good index into an `R` register.
	///
	/// ## Parameters
	///
	/// - `idx`: The counter value to mark as an index. This must be in the
	///   range `0 .. R::BITS`.
	///
	/// ## Returns
	///
	/// This returns `idx`, either marked as a valid `BitIdx` or an invalid
	/// `BitIdxError` by whether it is within the valid range `0 .. R::BITS`.
	#[inline]
	pub fn new(idx: u8) -> Result<Self, BitIdxError<R>> {
		if idx >= bits_of::<R>() as u8 {
			return Err(BitIdxError::new(idx));
		}
		Ok(unsafe { Self::new_unchecked(idx) })
	}

	/// Wraps a counter value as an assumed-good index into an `R` register.
	///
	/// ## Parameters
	///
	/// - `idx`: The counter value to mark as an index. This must be in the
	///   range `0 .. R::BITS`.
	///
	/// ## Returns
	///
	/// This unconditionally marks `idx` as a valid bit-index.
	///
	/// ## Safety
	///
	/// If the `idx` value is outside the valid range, then the program is
	/// incorrect. Debug builds will panic; release builds do not inspect the
	/// value or specify a behavior.
	#[inline]
	pub unsafe fn new_unchecked(idx: u8) -> Self {
		debug_assert!(
			idx < bits_of::<R>() as u8,
			"Bit index {} cannot exceed type width {}",
			idx,
			bits_of::<R>(),
		);
		Self {
			idx,
			_ty: PhantomData,
		}
	}

	/// Removes the index wrapper, leaving the internal counter.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn into_inner(self) -> u8 {
		self.idx
	}

	/// Increments an index counter, wrapping at the back edge of the register.
	///
	/// ## Parameters
	///
	/// - `self`: The index to increment.
	///
	/// ## Returns
	///
	/// - `.0`: The next index after `self`.
	/// - `.1`: Indicates whether the new index is in the next memory address.
	#[inline]
	pub fn next(self) -> (Self, bool) {
		let next = self.idx + 1;
		(
			unsafe { Self::new_unchecked(next & R::MASK) },
			next == bits_of::<R>() as u8,
		)
	}

	/// Decrements an index counter, wrapping at the front edge of the register.
	///
	/// ## Parameters
	///
	/// - `self`: The index to decrement.
	///
	/// ## Returns
	///
	/// - `.0`: The previous index before `self`.
	/// - `.1`: Indicates whether the new index is in the previous memory
	///   address.
	#[inline]
	pub fn prev(self) -> (Self, bool) {
		let prev = self.idx.wrapping_sub(1);
		(
			unsafe { Self::new_unchecked(prev & R::MASK) },
			self.idx == 0,
		)
	}

	/// Computes the bit position corresponding to `self` under some ordering.
	///
	/// This forwards to [`O::at::<R>`], which is the only public, safe,
	/// constructor for a position counter.
	///
	/// [`O::at::<R>`]: crate::order::BitOrder::at
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn position<O>(self) -> BitPos<R>
	where O: BitOrder {
		O::at::<R>(self)
	}

	/// Computes the bit selector corresponding to `self` under an ordering.
	///
	/// This forwards to [`O::select::<R>`], which is the only public, safe,
	/// constructor for a bit selector.
	///
	/// [`O::select::<R>`]: crate::order::BitOrder::select
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn select<O>(self) -> BitSel<R>
	where O: BitOrder {
		O::select::<R>(self)
	}

	/// Computes the bit selector for `self` as an accessor mask.
	///
	/// This is a type-cast over [`Self::select`].
	///
	/// [`Self::select`]: Self::select
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn mask<O>(self) -> BitMask<R>
	where O: BitOrder {
		self.select::<O>().mask()
	}

	/// Iterates over all indices between an inclusive start and exclusive end
	/// point.
	///
	/// Because implementation details of the range type family, including the
	/// [`RangeBounds`] trait, are not yet stable, and heterogeneous ranges are
	/// not supported, this must be an opaque iterator rather than a direct
	/// [`Range<BitIdx<R>>`].
	///
	/// # Parameters
	///
	/// - `from`: The inclusive low bound of the range. This will be the first
	///   index produced by the iterator.
	/// - `upto`: The exclusive high bound of the range. The iterator will halt
	///   before yielding an index of this value.
	///
	/// # Returns
	///
	/// An opaque iterator that is equivalent to the range `from .. upto`.
	///
	/// # Requirements
	///
	/// `from` must be no greater than `upto`.
	///
	/// [`RangeBounds`]: core::ops::RangeBounds
	/// [`Range<BitIdx<R>>`]: core::ops::Range
	#[inline]
	pub fn range(
		self,
		upto: BitEnd<R>,
	) -> impl Iterator<Item = Self>
	+ DoubleEndedIterator
	+ ExactSizeIterator
	+ FusedIterator {
		let (from, upto) = (self.into_inner(), upto.into_inner());
		debug_assert!(from <= upto, "Ranges must run from low to high");
		(from .. upto).map(|val| unsafe { Self::new_unchecked(val) })
	}

	/// Iterates over all possible index values.
	#[inline]
	pub fn range_all() -> impl Iterator<Item = Self>
	+ DoubleEndedIterator
	+ ExactSizeIterator
	+ FusedIterator {
		BitIdx::MIN.range(BitEnd::MAX)
	}

	/// Computes the jump distance for some number of bits away from a starting
	/// index.
	///
	/// This computes the number of elements by which to adjust a base pointer,
	/// and then the bit index of the destination bit in the new referent
	/// register element.
	///
	/// # Parameters
	///
	/// - `self`: An index within some element, from which the offset is
	///   computed.
	/// - `by`: The distance by which to jump. Negative values move lower in the
	///   index and element-pointer space; positive values move higher.
	///
	/// # Returns
	///
	/// - `.0`: The number of elements `R` by which to adjust a base pointer.
	///   This value can be passed directly into [`ptr::offset`].
	/// - `.1`: The index of the destination bit within the destination element.
	///
	/// [`ptr::offset`]: https://doc.rust-lang.org/stable/std/primitive.pointer.html#method.offset
	pub(crate) fn offset(self, by: isize) -> (isize, Self) {
		/* Signed-add `self.idx` to the jump distance. This will almost
		 * certainly not wrap (as the crate imposes restrictions well below
		 * `isize::MAX`), but correctness never hurts. The resulting sum is a
		 * bit distance that is then broken into an element distance and final
		 * bit index.
		 */
		let far = by.wrapping_add(self.into_inner() as isize);

		let (elts, head) = (far >> R::INDX, far as u8 & R::MASK);

		(elts, unsafe { Self::new_unchecked(head) })
	}

	/// Computes the span information for a region beginning at `self` for `len`
	/// bits.
	///
	/// The span information is the number of elements in the region that hold
	/// live bits, and the position of the tail marker after the live bits.
	///
	/// This forwards to [`BitEnd::span`], as the computation is identical for
	/// the two types. Beginning a span at any `Idx` is equivalent to beginning
	/// it at the tail of a previous span.
	///
	/// # Parameters
	///
	/// - `self`: The start bit of the span.
	/// - `len`: The number of bits in the span.
	///
	/// # Returns
	///
	/// - `.0`: The number of elements, starting in the element that contains
	///   `self`, that contain live bits of the span.
	/// - `.1`: The tail counter of the span’s end point.
	///
	/// [`BitEnd::span`]: crate::index::BitEnd::span
	pub(crate) fn span(self, len: usize) -> (usize, BitEnd<R>) {
		unsafe { BitEnd::<R>::new_unchecked(self.into_inner()) }.span(len)
	}
}

impl<R> Binary for BitIdx<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "{:0>1$b}", self.idx, R::INDX as usize)
	}
}

impl<R> Debug for BitIdx<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "BitIdx<{}>({})", any::type_name::<R>(), self)
	}
}

impl<R> Display for BitIdx<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		Binary::fmt(self, fmt)
	}
}

#[repr(transparent)]
#[doc = include_str!("../doc/index/BitIdxError.md")]
#[derive(Clone, Copy, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct BitIdxError<R = usize>
where R: BitRegister
{
	/// The value that is invalid as a [`BitIdx<R>`].
	///
	/// [`BitIdx<R>`]: crate::index::BitIdx
	err: u8,
	/// Marker for the register type.
	_ty: PhantomData<R>,
}

impl<R> BitIdxError<R>
where R: BitRegister
{
	/// Marks a counter value as invalid to be an index for an `R` register.
	///
	/// ## Parameters
	///
	/// - `err`: The counter value to mark as an error. This must be greater
	///   than `R::BITS`.
	///
	/// ## Returns
	///
	/// This returns `err`, marked as an invalid index for `R`.
	///
	/// ## Panics
	///
	/// Debug builds panic when `err` is a valid index for `R`.
	pub(crate) fn new(err: u8) -> Self {
		debug_assert!(
			err >= bits_of::<R>() as u8,
			"Bit index {} is valid for type width {}",
			err,
			bits_of::<R>(),
		);
		Self {
			err,
			_ty: PhantomData,
		}
	}

	/// Removes the error wrapper, leaving the internal counter.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn into_inner(self) -> u8 {
		self.err
	}
}

impl<R> Debug for BitIdxError<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "BitIdxError<{}>({})", any::type_name::<R>(), self.err)
	}
}

#[cfg(not(tarpaulin_include))]
impl<R> Display for BitIdxError<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(
			fmt,
			"the value {} is too large to index into {} ({} bits wide)",
			self.err,
			any::type_name::<R>(),
			bits_of::<R>(),
		)
	}
}

#[cfg(feature = "std")]
impl<R> std::error::Error for BitIdxError<R> where R: BitRegister {}

#[repr(transparent)]
#[doc = include_str!("../doc/index/BitEnd.md")]
#[derive(Clone, Copy, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct BitEnd<R = usize>
where R: BitRegister
{
	/// Semantic tail counter within or after a register, contained to `0 ..=
	/// R::BITS`.
	end: u8,
	/// Marker for the register type.
	_ty: PhantomData<R>,
}

impl<R> BitEnd<R>
where R: BitRegister
{
	/// The inclusive maximum tail within (or after) an `R` element.
	pub const MAX: Self = Self {
		end: bits_of::<R>() as u8,
		_ty: PhantomData,
	};
	/// The inclusive minimum tail within (or after) an `R` element.
	pub const MIN: Self = Self {
		end: 0,
		_ty: PhantomData,
	};

	/// Wraps a counter value as a known-good tail of an `R` register.
	///
	/// ## Parameters
	///
	/// - `end`: The counter value to mark as a tail. This must be in the range
	///   `0 ..= R::BITS`.
	///
	/// ## Returns
	///
	/// This returns `Some(end)` when it is in the valid range `0 ..= R::BITS`,
	/// and `None` when it is not.
	#[inline]
	pub fn new(end: u8) -> Option<Self> {
		if end > bits_of::<R>() as u8 {
			return None;
		}
		Some(unsafe { Self::new_unchecked(end) })
	}

	/// Wraps a counter value as an assumed-good tail of an `R` register.
	///
	/// ## Parameters
	///
	/// - `end`: The counter value to mark as a tail. This must be in the range
	///   `0 ..= R::BITS`.
	///
	/// ## Returns
	///
	/// This unconditionally marks `end` as a valid tail index.
	///
	/// ## Safety
	///
	/// If the `end` value is outside the valid range, then the program is
	/// incorrect. Debug builds will panic; release builds do not inspect the
	/// value or specify a behavior.
	pub(crate) unsafe fn new_unchecked(end: u8) -> Self {
		debug_assert!(
			end <= bits_of::<R>() as u8,
			"Bit tail {} cannot exceed type width {}",
			end,
			bits_of::<R>(),
		);
		Self {
			end,
			_ty: PhantomData,
		}
	}

	/// Removes the tail wrapper, leaving the internal counter.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn into_inner(self) -> u8 {
		self.end
	}

	/// Iterates over all tail indices at and after an inclusive starting point.
	///
	/// Because implementation details of the range type family, including the
	/// [`RangeBounds`] trait, are not yet stable, and heterogeneous ranges are
	/// not yet supported, this must be an opaque iterator rather than a direct
	/// [`Range<BitEnd<R>>`].
	///
	/// # Parameters
	///
	/// - `from`: The inclusive low bound of the range. This will be the first
	///   tail produced by the iterator.
	///
	/// # Returns
	///
	/// An opaque iterator that is equivalent to the range `from ..=
	/// Self::MAX`.
	///
	/// [`RangeBounds`]: core::ops::RangeBounds
	/// [`Range<BitEnd<R>>`]: core::ops::Range
	#[inline]
	pub fn range_from(
		from: BitIdx<R>,
	) -> impl Iterator<Item = Self>
	+ DoubleEndedIterator
	+ ExactSizeIterator
	+ FusedIterator {
		(from.idx ..= Self::MAX.end)
			.map(|tail| unsafe { BitEnd::new_unchecked(tail) })
	}

	/// Computes the span information for a region.
	///
	/// The computed span of `len` bits begins at `self` and extends upwards in
	/// memory. The return value is the number of memory elements that contain
	/// bits of the span, and the first dead bit after the span.
	///
	/// ## Parameters
	///
	/// - `self`: A dead bit which is used as the first live bit of the new
	///   span.
	/// - `len`: The number of live bits in the span starting at `self`.
	///
	/// ## Returns
	///
	/// - `.0`: The number of `R` elements that contain live bits in the
	///   computed span.
	/// - `.1`: The dead-bit tail index ending the computed span.
	///
	/// ## Behavior
	///
	/// If `len` is `0`, this returns `(0, self)`, as the span has no live bits.
	/// If `self` is [`BitEnd::MAX`], then the new region starts at
	/// [`BitIdx::MIN`] in the next element.
	///
	/// [`BitEnd::MAX`]: Self::MAX
	/// [`BitIdx::MIN`]: Self::MIN
	pub(crate) fn span(self, len: usize) -> (usize, Self) {
		if len == 0 {
			return (0, self);
		}

		let head = self.end & R::MASK;
		let bits_in_head = (bits_of::<R>() as u8 - head) as usize;

		if len <= bits_in_head {
			return (1, unsafe { Self::new_unchecked(head + len as u8) });
		}

		let bits_after_head = len - bits_in_head;
		let elts = bits_after_head >> R::INDX;
		let tail = bits_after_head as u8 & R::MASK;

		let is_zero = (tail == 0) as u8;
		let edges = 2 - is_zero as usize;
		(elts + edges, unsafe {
			Self::new_unchecked((is_zero << R::INDX) | tail)
		})
	}
}

impl<R> Binary for BitEnd<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "{:0>1$b}", self.end, R::INDX as usize + 1)
	}
}

impl<R> Debug for BitEnd<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "BitEnd<{}>({})", any::type_name::<R>(), self)
	}
}

impl<R> Display for BitEnd<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		Binary::fmt(self, fmt)
	}
}

#[repr(transparent)]
#[doc = include_str!("../doc/index/BitPos.md")]
// #[rustc_layout_scalar_valid_range_end(R::BITS)]
#[derive(Clone, Copy, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct BitPos<R = usize>
where R: BitRegister
{
	/// Electrical position counter within a register, constrained to `0 ..
	/// R::BITS`.
	pos: u8,
	/// Marker for the register type.
	_ty: PhantomData<R>,
}

impl<R> BitPos<R>
where R: BitRegister
{
	/// The position value of the most significant bit in an `R` element.
	pub const MAX: Self = Self {
		pos: R::MASK as u8,
		_ty: PhantomData,
	};
	/// The position value of the least significant bit in an `R` element.
	pub const MIN: Self = Self {
		pos: 0,
		_ty: PhantomData,
	};

	/// Wraps a counter value as a known-good position within an `R` register.
	///
	/// ## Parameters
	///
	/// - `pos`: The counter value to mark as a position. This must be in the
	///   range `0 .. R::BITS`.
	///
	/// ## Returns
	///
	/// This returns `Some(pos)` when it is in the valid range `0 .. R::BITS`,
	/// and `None` when it is not.
	#[inline]
	pub fn new(pos: u8) -> Option<Self> {
		if pos >= bits_of::<R>() as u8 {
			return None;
		}
		Some(unsafe { Self::new_unchecked(pos) })
	}

	/// Wraps a counter value as an assumed-good position within an `R`
	/// register.
	///
	/// ## Parameters
	///
	/// - `value`: The counter value to mark as a position. This must be in the
	///   range `0 .. R::BITS`.
	///
	/// ## Returns
	///
	/// This unconditionally marks `pos` as a valid bit-position.
	///
	/// ## Safety
	///
	/// If the `pos` value is outside the valid range, then the program is
	/// incorrect. Debug builds will panic; release builds do not inspect the
	/// value or specify a behavior.
	#[inline]
	pub unsafe fn new_unchecked(pos: u8) -> Self {
		debug_assert!(
			pos < bits_of::<R>() as u8,
			"Bit position {} cannot exceed type width {}",
			pos,
			bits_of::<R>(),
		);
		Self {
			pos,
			_ty: PhantomData,
		}
	}

	/// Removes the position wrapper, leaving the internal counter.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn into_inner(self) -> u8 {
		self.pos
	}

	/// Computes the bit selector corresponding to `self`.
	///
	/// This is always `1 << self.pos`.
	#[inline]
	pub fn select(self) -> BitSel<R> {
		unsafe { BitSel::new_unchecked(R::ONE << self.pos) }
	}

	/// Computes the bit selector for `self` as an accessor mask.
	///
	/// This is a type-cast over [`Self::select`].
	///
	/// [`Self::select`]: Self::select
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn mask(self) -> BitMask<R> {
		self.select().mask()
	}

	/// Iterates over all possible position values.
	pub(crate) fn range_all() -> impl Iterator<Item = Self>
	+ DoubleEndedIterator
	+ ExactSizeIterator
	+ FusedIterator {
		BitIdx::<R>::range_all()
			.map(|idx| unsafe { Self::new_unchecked(idx.into_inner()) })
	}
}

impl<R> Binary for BitPos<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "{:0>1$b}", self.pos, R::INDX as usize)
	}
}

impl<R> Debug for BitPos<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "BitPos<{}>({})", any::type_name::<R>(), self)
	}
}

impl<R> Display for BitPos<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		Binary::fmt(self, fmt)
	}
}

#[repr(transparent)]
#[doc = include_str!("../doc/index/BitSel.md")]
// #[rustc_layout_scalar_valid_range_end(R::BITS)]
#[derive(Clone, Copy, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct BitSel<R = usize>
where R: BitRegister
{
	/// A one-hot selection mask.
	sel: R,
}

impl<R> BitSel<R>
where R: BitRegister
{
	/// Wraps a selector value as a known-good selection in an `R` register.
	///
	/// ## Parameters
	///
	/// - `sel`: A one-hot selection mask of a bit in an `R` register.
	///
	/// ## Returns
	///
	/// This returns `Some(sel)` when it is a power of two (exactly one bit set
	/// and all others cleared), and `None` when it is not.
	#[inline]
	pub fn new(sel: R) -> Option<Self> {
		if sel.count_ones() != 1 {
			return None;
		}
		Some(unsafe { Self::new_unchecked(sel) })
	}

	/// Wraps a selector value as an assumed-good selection in an `R` register.
	///
	/// ## Parameters
	///
	/// - `sel`: A one-hot selection mask of a bit in an `R` register.
	///
	/// ## Returns
	///
	/// This unconditionally marks `sel` as a one-hot bit selector.
	///
	/// ## Safety
	///
	/// If the `sel` value has zero or multiple bits set, then it is invalid to
	/// be used as a `BitSel` and the program is incorrect. Debug builds will
	/// panic; release builds do not inspect the value or specify a behavior.
	#[inline]
	pub unsafe fn new_unchecked(sel: R) -> Self {
		debug_assert!(
			sel.count_ones() == 1,
			"Selections are required to have exactly one bit set: {:0>1$b}",
			sel,
			bits_of::<R>() as usize,
		);
		Self { sel }
	}

	/// Removes the one-hot selection wrapper, leaving the internal mask.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn into_inner(self) -> R {
		self.sel
	}

	/// Computes a bit-mask for `self`. This is a type-cast.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn mask(self) -> BitMask<R> {
		BitMask::new(self.sel)
	}

	/// Iterates over all possible selector values.
	#[inline]
	pub fn range_all() -> impl Iterator<Item = Self>
	+ DoubleEndedIterator
	+ ExactSizeIterator
	+ FusedIterator {
		BitPos::<R>::range_all().map(BitPos::select)
	}
}

impl<R> Binary for BitSel<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "{:0>1$b}", self.sel, bits_of::<R>() as usize)
	}
}

impl<R> Debug for BitSel<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "BitSel<{}>({})", any::type_name::<R>(), self)
	}
}

impl<R> Display for BitSel<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		Binary::fmt(self, fmt)
	}
}

#[repr(transparent)]
#[doc = include_str!("../doc/index/BitMask.md")]
#[derive(Clone, Copy, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct BitMask<R = usize>
where R: BitRegister
{
	/// A mask of any number of bits to select.
	mask: R,
}

impl<R> BitMask<R>
where R: BitRegister
{
	/// A full bit-mask with every bit set.
	pub const ALL: Self = Self { mask: R::ALL };
	/// An empty bit-mask with every bit cleared.
	pub const ZERO: Self = Self { mask: R::ZERO };

	/// Wraps any `R` value as a bit-mask.
	///
	/// This constructor is provided to explicitly declare that an operation is
	/// discarding the numeric value of an integer and instead using it only as
	/// a bit-mask.
	///
	/// ## Parameters
	///
	/// - `mask`: Some integer to use as a bit-mask.
	///
	/// ## Returns
	///
	/// The `mask` value wrapped as a bit-mask, with its numeric context
	/// discarded.
	///
	/// Prefer accumulating [`BitSel`] values using its `Sum` implementation.
	///
	/// ## Safety
	///
	/// The `mask` value must be computed from a set of valid bit positions in
	/// the caller’s context.
	///
	/// [`BitSel`]: crate::index::BitSel
	#[inline]
	pub fn new(mask: R) -> Self {
		Self { mask }
	}

	/// Removes the mask wrapper, leaving the internal value.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn into_inner(self) -> R {
		self.mask
	}

	/// Tests if a mask contains a given selector bit.
	///
	/// ## Parameters
	///
	/// - `&self`: The mask being tested.
	/// - `sel`: A selector bit to test in `self`.
	///
	/// ## Returns
	///
	/// Whether `self` has set the bit that `sel` indicates.
	#[inline]
	pub fn test(&self, sel: BitSel<R>) -> bool {
		self.mask & sel.sel != R::ZERO
	}

	/// Inserts a selector bit into a mask.
	///
	/// ## Parameters
	///
	/// - `&mut self`: The mask being modified.
	/// - `sel`: A selector bit to insert into `self`.
	///
	/// ## Effects
	///
	/// The `sel` bit is set in the mask.
	#[inline]
	pub fn insert(&mut self, sel: BitSel<R>) {
		self.mask |= sel.sel;
	}

	/// Creates a new mask with a selector bit activated.
	///
	/// ## Parameters
	///
	/// - `self`: The original mask.
	/// - `sel`: The selector bit being added into the mask.
	///
	/// ## Returns
	///
	/// A new bit-mask with `sel` activated.
	#[inline]
	pub fn combine(self, sel: BitSel<R>) -> Self {
		Self {
			mask: self.mask | sel.sel,
		}
	}
}

impl<R> Binary for BitMask<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "{:0>1$b}", self.mask, bits_of::<R>() as usize)
	}
}

impl<R> Debug for BitMask<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "BitMask<{}>({})", any::type_name::<R>(), self)
	}
}

impl<R> Display for BitMask<R>
where R: BitRegister
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		Binary::fmt(self, fmt)
	}
}

impl<R> Sum<BitSel<R>> for BitMask<R>
where R: BitRegister
{
	#[inline]
	fn sum<I>(iter: I) -> Self
	where I: Iterator<Item = BitSel<R>> {
		iter.fold(Self::ZERO, Self::combine)
	}
}

impl<R> BitAnd<R> for BitMask<R>
where R: BitRegister
{
	type Output = Self;

	#[inline]
	fn bitand(self, rhs: R) -> Self::Output {
		Self {
			mask: self.mask & rhs,
		}
	}
}

impl<R> BitOr<R> for BitMask<R>
where R: BitRegister
{
	type Output = Self;

	#[inline]
	fn bitor(self, rhs: R) -> Self::Output {
		Self {
			mask: self.mask | rhs,
		}
	}
}

impl<R> Not for BitMask<R>
where R: BitRegister
{
	type Output = Self;

	#[inline]
	fn not(self) -> Self::Output {
		Self { mask: !self.mask }
	}
}

#[cfg(test)]
mod tests {
	use super::*;
	use crate::order::Lsb0;

	#[test]
	fn index_ctors() {
		for n in 0 .. 8 {
			assert!(BitIdx::<u8>::new(n).is_ok());
		}
		assert!(BitIdx::<u8>::new(8).is_err());

		for n in 0 .. 16 {
			assert!(BitIdx::<u16>::new(n).is_ok());
		}
		assert!(BitIdx::<u16>::new(16).is_err());

		for n in 0 .. 32 {
			assert!(BitIdx::<u32>::new(n).is_ok());
		}
		assert!(BitIdx::<u32>::new(32).is_err());

		#[cfg(target_pointer_width = "64")]
		{
			for n in 0 .. 64 {
				assert!(BitIdx::<u64>::new(n).is_ok());
			}
			assert!(BitIdx::<u64>::new(64).is_err());
		}

		for n in 0 .. bits_of::<usize>() as u8 {
			assert!(BitIdx::<usize>::new(n).is_ok());
		}
		assert!(BitIdx::<usize>::new(bits_of::<usize>() as u8).is_err());
	}

	#[test]
	fn tail_ctors() {
		for n in 0 ..= 8 {
			assert!(BitEnd::<u8>::new(n).is_some());
		}
		assert!(BitEnd::<u8>::new(9).is_none());

		for n in 0 ..= 16 {
			assert!(BitEnd::<u16>::new(n).is_some());
		}
		assert!(BitEnd::<u16>::new(17).is_none());

		for n in 0 ..= 32 {
			assert!(BitEnd::<u32>::new(n).is_some());
		}
		assert!(BitEnd::<u32>::new(33).is_none());

		#[cfg(target_pointer_width = "64")]
		{
			for n in 0 ..= 64 {
				assert!(BitEnd::<u64>::new(n).is_some());
			}
			assert!(BitEnd::<u64>::new(65).is_none());
		}

		for n in 0 ..= bits_of::<usize>() as u8 {
			assert!(BitEnd::<usize>::new(n).is_some());
		}
		assert!(BitEnd::<usize>::new(bits_of::<usize>() as u8 + 1).is_none());
	}

	#[test]
	fn position_ctors() {
		for n in 0 .. 8 {
			assert!(BitPos::<u8>::new(n).is_some());
		}
		assert!(BitPos::<u8>::new(8).is_none());

		for n in 0 .. 16 {
			assert!(BitPos::<u16>::new(n).is_some());
		}
		assert!(BitPos::<u16>::new(16).is_none());

		for n in 0 .. 32 {
			assert!(BitPos::<u32>::new(n).is_some());
		}
		assert!(BitPos::<u32>::new(32).is_none());

		#[cfg(target_pointer_width = "64")]
		{
			for n in 0 .. 64 {
				assert!(BitPos::<u64>::new(n).is_some());
			}
			assert!(BitPos::<u64>::new(64).is_none());
		}

		for n in 0 .. bits_of::<usize>() as u8 {
			assert!(BitPos::<usize>::new(n).is_some());
		}
		assert!(BitPos::<usize>::new(bits_of::<usize>() as u8).is_none());
	}

	#[test]
	fn select_ctors() {
		for n in 0 .. 8 {
			assert!(BitSel::<u8>::new(1 << n).is_some());
		}
		assert!(BitSel::<u8>::new(0).is_none());
		assert!(BitSel::<u8>::new(3).is_none());

		for n in 0 .. 16 {
			assert!(BitSel::<u16>::new(1 << n).is_some());
		}
		assert!(BitSel::<u16>::new(0).is_none());
		assert!(BitSel::<u16>::new(3).is_none());

		for n in 0 .. 32 {
			assert!(BitSel::<u32>::new(1 << n).is_some());
		}
		assert!(BitSel::<u32>::new(0).is_none());
		assert!(BitSel::<u32>::new(3).is_none());

		#[cfg(target_pointer_width = "64")]
		{
			for n in 0 .. 64 {
				assert!(BitSel::<u64>::new(1 << n).is_some());
			}
			assert!(BitSel::<u64>::new(0).is_none());
			assert!(BitSel::<u64>::new(3).is_none());
		}

		for n in 0 .. bits_of::<usize>() as u8 {
			assert!(BitSel::<usize>::new(1 << n).is_some());
		}
		assert!(BitSel::<usize>::new(0).is_none());
		assert!(BitSel::<usize>::new(3).is_none());
	}

	#[test]
	fn ranges() {
		let mut range = BitIdx::<u16>::range_all();
		assert_eq!(range.next(), BitIdx::new(0).ok());
		assert_eq!(range.next_back(), BitIdx::new(15).ok());
		assert_eq!(range.count(), 14);

		let mut range = BitEnd::<u8>::range_from(BitIdx::new(1).unwrap());
		assert_eq!(range.next(), BitEnd::new(1));
		assert_eq!(range.next_back(), BitEnd::new(8));
		assert_eq!(range.count(), 6);

		let mut range = BitPos::<u8>::range_all();
		assert_eq!(range.next(), BitPos::new(0));
		assert_eq!(range.next_back(), BitPos::new(7));
		assert_eq!(range.count(), 6);

		let mut range = BitSel::<u8>::range_all();
		assert_eq!(range.next(), BitSel::new(1));
		assert_eq!(range.next_back(), BitSel::new(128));
		assert_eq!(range.count(), 6);
	}

	#[test]
	fn index_cycle() {
		let six = BitIdx::<u8>::new(6).unwrap();
		let (seven, step) = six.next();
		assert_eq!(seven, BitIdx::new(7).unwrap());
		assert!(!step);

		let (zero, step) = seven.next();
		assert_eq!(zero, BitIdx::MIN);
		assert!(step);

		let (seven, step) = zero.prev();
		assert_eq!(seven, BitIdx::new(7).unwrap());
		assert!(step);

		let (six, step) = seven.prev();
		assert_eq!(six, BitIdx::new(6).unwrap());
		assert!(!step);

		let fourteen = BitIdx::<u16>::new(14).unwrap();
		let (fifteen, step) = fourteen.next();
		assert_eq!(fifteen, BitIdx::new(15).unwrap());
		assert!(!step);
		let (zero, step) = fifteen.next();
		assert_eq!(zero, BitIdx::MIN);
		assert!(step);
		let (fifteen, step) = zero.prev();
		assert_eq!(fifteen, BitIdx::new(15).unwrap());
		assert!(step);
		let (fourteen, step) = fifteen.prev();
		assert_eq!(fourteen, BitIdx::new(14).unwrap());
		assert!(!step);
	}

	#[test]
	fn jumps() {
		let (jump, head) = BitIdx::<u8>::new(1).unwrap().offset(2);
		assert_eq!(jump, 0);
		assert_eq!(head, BitIdx::new(3).unwrap());

		let (jump, head) = BitIdx::<u8>::MAX.offset(1);
		assert_eq!(jump, 1);
		assert_eq!(head, BitIdx::MIN);

		let (jump, head) = BitIdx::<u16>::new(10).unwrap().offset(40);
		// 10 is in 0..16; 10+40 is in 48..64
		assert_eq!(jump, 3);
		assert_eq!(head, BitIdx::new(2).unwrap());

		//  .offset() wraps at the `isize` boundary
		let (jump, head) = BitIdx::<u8>::MAX.offset(isize::MAX);
		assert_eq!(jump, -(((isize::MAX as usize + 1) >> 3) as isize));
		assert_eq!(head, BitIdx::MAX.prev().0);

		let (elts, tail) = BitIdx::<u8>::new(4).unwrap().span(0);
		assert_eq!(elts, 0);
		assert_eq!(tail, BitEnd::new(4).unwrap());

		let (elts, tail) = BitIdx::<u8>::new(3).unwrap().span(3);
		assert_eq!(elts, 1);
		assert_eq!(tail, BitEnd::new(6).unwrap());

		let (elts, tail) = BitIdx::<u16>::new(10).unwrap().span(40);
		assert_eq!(elts, 4);
		assert_eq!(tail, BitEnd::new(2).unwrap());
	}

	#[test]
	fn mask_operators() {
		let mut mask = BitIdx::<u8>::new(2)
			.unwrap()
			.range(BitEnd::new(5).unwrap())
			.map(BitIdx::select::<Lsb0>)
			.sum::<BitMask<u8>>();
		assert_eq!(mask, BitMask::new(28));
		assert_eq!(mask & 25, BitMask::new(24));
		assert_eq!(mask | 32, BitMask::new(60));
		assert_eq!(!mask, BitMask::new(!28));
		let yes = BitSel::<u8>::new(16).unwrap();
		let no = BitSel::<u8>::new(64).unwrap();
		assert!(mask.test(yes));
		assert!(!mask.test(no));
		mask.insert(no);
		assert!(mask.test(no));
	}

	#[test]
	#[cfg(feature = "alloc")]
	fn render() {
		#[cfg(not(feature = "std"))]
		use alloc::format;

		assert_eq!(format!("{:?}", BitIdx::<u8>::MAX), "BitIdx<u8>(111)");
		assert_eq!(format!("{:?}", BitIdx::<u16>::MAX), "BitIdx<u16>(1111)");
		assert_eq!(format!("{:?}", BitIdx::<u32>::MAX), "BitIdx<u32>(11111)");

		assert_eq!(
			format!("{:?}", BitIdx::<u8>::new(8).unwrap_err()),
			"BitIdxError<u8>(8)"
		);
		assert_eq!(
			format!("{:?}", BitIdx::<u16>::new(16).unwrap_err()),
			"BitIdxError<u16>(16)"
		);
		assert_eq!(
			format!("{:?}", BitIdx::<u32>::new(32).unwrap_err()),
			"BitIdxError<u32>(32)"
		);

		assert_eq!(format!("{:?}", BitEnd::<u8>::MAX), "BitEnd<u8>(1000)");
		assert_eq!(format!("{:?}", BitEnd::<u16>::MAX), "BitEnd<u16>(10000)");
		assert_eq!(format!("{:?}", BitEnd::<u32>::MAX), "BitEnd<u32>(100000)");

		assert_eq!(format!("{:?}", BitPos::<u8>::MAX), "BitPos<u8>(111)");
		assert_eq!(format!("{:?}", BitPos::<u16>::MAX), "BitPos<u16>(1111)");
		assert_eq!(format!("{:?}", BitPos::<u32>::MAX), "BitPos<u32>(11111)");

		assert_eq!(
			format!("{:?}", BitSel::<u8>::new(1).unwrap()),
			"BitSel<u8>(00000001)",
		);
		assert_eq!(
			format!("{:?}", BitSel::<u16>::new(1).unwrap()),
			"BitSel<u16>(0000000000000001)",
		);
		assert_eq!(
			format!("{:?}", BitSel::<u32>::new(1).unwrap()),
			"BitSel<u32>(00000000000000000000000000000001)",
		);

		assert_eq!(
			format!("{:?}", BitMask::<u8>::new(1 | 4 | 32)),
			"BitMask<u8>(00100101)",
		);
		assert_eq!(
			format!("{:?}", BitMask::<u16>::new(1 | 4 | 32)),
			"BitMask<u16>(0000000000100101)",
		);
		assert_eq!(
			format!("{:?}", BitMask::<u32>::new(1 | 4 | 32)),
			"BitMask<u32>(00000000000000000000000000100101)",
		);

		#[cfg(target_pointer_width = "64")]
		{
			assert_eq!(
				format!("{:?}", BitIdx::<u64>::MAX),
				"BitIdx<u64>(111111)",
			);
			assert_eq!(
				format!("{:?}", BitIdx::<u64>::new(64).unwrap_err()),
				"BitIdxError<u64>(64)",
			);
			assert_eq!(
				format!("{:?}", BitEnd::<u64>::MAX),
				"BitEnd<u64>(1000000)",
			);
			assert_eq!(
				format!("{:?}", BitPos::<u64>::MAX),
				"BitPos<u64>(111111)",
			);
			assert_eq!(
				format!("{:?}", BitSel::<u64>::new(1).unwrap()),
				"BitSel<u64>(0000000000000000000000000000000000000000000000000000000000000001)",
			);
			assert_eq!(
				format!("{:?}", BitMask::<u64>::new(1 | 4 | 32)),
				"BitMask<u64>(0000000000000000000000000000000000000000000000000000000000100101)",
			);
		}
	}
}
