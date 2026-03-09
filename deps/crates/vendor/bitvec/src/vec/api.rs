//! Port of the `Vec<bool>` inherent API.

use alloc::vec::Vec;
use core::{
	mem::ManuallyDrop,
	ops::RangeBounds,
};

use tap::Pipe;
use wyz::{
	comu::{
		Const,
		Mut,
	},
	range::RangeExt,
};

use super::{
	BitVec,
	Drain,
	Splice,
};
use crate::{
	boxed::BitBox,
	index::BitEnd,
	mem,
	order::BitOrder,
	ptr::{
		AddressExt,
		BitPtr,
		BitSpan,
	},
	slice::BitSlice,
	store::BitStore,
};

/// Port of the `Vec<T>` inherent API.
impl<T, O> BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Constructs a new, empty, bit-vector.
	///
	/// This does not allocate until bits are [`.push()`]ed into it, or space is
	/// explicitly [`.reserve()`]d.
	///
	/// ## Original
	///
	/// [`Vec::new`](alloc::vec::Vec::new)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bv = BitVec::<u8, Msb0>::new();
	/// assert!(bv.is_empty());
	/// ```
	///
	/// [`.push()`]: Self::push
	/// [`.reserve()`]: Self::reserve
	#[inline]
	pub fn new() -> Self {
		Self::EMPTY
	}

	/// Allocates a new, empty, bit-vector with space for at least `capacity`
	/// bits before reallocating.
	///
	/// ## Original
	///
	/// [`Vec::with_capacity`](alloc::vec::Vec::with_capacity)
	///
	/// ## Panics
	///
	/// This panics if the requested capacity is longer than what the bit-vector
	/// can represent. See [`BitSlice::MAX_BITS`].
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv: BitVec = BitVec::with_capacity(128);
	///
	/// assert!(bv.is_empty());
	/// assert!(bv.capacity() >= 128);
	///
	/// for i in 0 .. 128 {
	///   bv.push(i & 0xC0 == i);
	/// }
	/// assert_eq!(bv.len(), 128);
	/// assert!(bv.capacity() >= 128);
	///
	/// bv.push(false);
	/// assert_eq!(bv.len(), 129);
	/// assert!(bv.capacity() >= 129);
	/// ```
	///
	/// [`BitSlice::MAX_BITS`]: crate::slice::BitSlice::MAX_BITS
	#[inline]
	pub fn with_capacity(capacity: usize) -> Self {
		Self::assert_len_encodable(capacity);
		let mut vec = capacity
			.pipe(crate::mem::elts::<T>)
			.pipe(Vec::<T>::with_capacity)
			.pipe(ManuallyDrop::new);
		let (addr, capacity) = (vec.as_mut_ptr(), vec.capacity());
		let bitspan = BitSpan::uninhabited(unsafe { addr.into_address() });
		Self { bitspan, capacity }
	}

	/// Constructs a bit-vector handle from its constituent fields.
	///
	/// ## Original
	///
	/// [`Vec::from_raw_parts`](alloc::vec::Vec::from_raw_parts)
	///
	/// ## Safety
	///
	/// The **only** acceptable argument values for this function are those that
	/// were previously produced by calling [`.into_raw_parts()`]. Furthermore,
	/// you may only call this **at most once** on any set of arguments. Using
	/// the same arguments in more than one call to this function will result in
	/// a double- or use-after free error.
	///
	/// Attempting to conjure your own values and pass them into this function
	/// will break the allocator state.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bv = bitvec![0, 1, 0, 0, 1];
	/// let (bitptr, len, capa) = bv.into_raw_parts();
	/// let bv2 = unsafe {
	///   BitVec::from_raw_parts(bitptr, len, capa)
	/// };
	/// assert_eq!(bv2, bits![0, 1, 0, 0, 1]);
	/// ```
	///
	/// [`.into_raw_parts()`]: Self::into_raw_parts
	#[inline]
	pub unsafe fn from_raw_parts(
		bitptr: BitPtr<Mut, T, O>,
		length: usize,
		capacity: usize,
	) -> Self {
		let bitspan = bitptr.span_unchecked(length);
		Self {
			bitspan,
			capacity: mem::elts::<T>(
				capacity.saturating_add(bitspan.head().into_inner() as usize),
			),
		}
	}

	/// Decomposes a bit-vector into its constituent member fields.
	///
	/// This disarms the destructor. In order to prevent a memory leak, you must
	/// pass **these exact values** back into [`::from_raw_parts()`].
	///
	/// ## Original
	///
	/// [`Vec::into_raw_parts`](alloc::vec::Vec::into_raw_parts)
	///
	/// ## API Differences
	///
	/// This method is still unstable as of 1.54. It is provided here as a
	/// convenience, under the expectation that the standard-library method will
	/// stabilize as-is.
	///
	/// [`::from_raw_parts()`]: Self::from_raw_parts
	#[inline]
	pub fn into_raw_parts(self) -> (BitPtr<Mut, T, O>, usize, usize) {
		let this = ManuallyDrop::new(self);
		(
			this.bitspan.to_bitptr(),
			this.bitspan.len(),
			this.capacity(),
		)
	}

	/// Gets the allocation capacity, measured in bits.
	///
	/// This counts how many total bits the bit-vector can store before it must
	/// perform a reällocation to acquire more memory.
	///
	/// If the capacity is not a multiple of 8, you should call
	/// [`.force_align()`].
	///
	/// ## Original
	///
	/// [`Vec::capacity`](alloc::vec::Vec::capacity)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bv = bitvec![0, 1, 0, 0, 1];
	/// ```
	///
	/// [`.force_align()`]: Self::force_align
	#[inline]
	pub fn capacity(&self) -> usize {
		self.capacity
			.checked_mul(mem::bits_of::<T>())
			.expect("bit-vector capacity exceeded")
			.saturating_sub(self.bitspan.head().into_inner() as usize)
	}

	/// Ensures that the bit-vector has allocation capacity for *at least*
	/// `additional` more bits to be appended to it.
	///
	/// For convenience, this method *guarantees* that the underlying memory for
	/// `self[.. self.len() + additional]` is initialized, and may be safely
	/// accessed directly without requiring use of `.push()` or `.extend()` to
	/// initialize it.
	///
	/// Newly-allocated memory is always initialized to zero. It is still *dead*
	/// until the bit-vector is grown (by `.push()`, `.extend()`, or
	/// `.set_len()`), but direct access will not trigger UB.
	///
	/// ## Original
	///
	/// [`Vec::reserve`](alloc::vec::Vec::reserve)
	///
	/// ## Panics
	///
	/// This panics if the new capacity exceeds the bit-vector’s maximum.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv: BitVec = BitVec::with_capacity(80);
	/// assert!(bv.capacity() >= 80);
	/// bv.reserve(800);
	/// assert!(bv.capacity() >= 800);
	/// ```
	#[inline]
	pub fn reserve(&mut self, additional: usize) {
		Self::assert_len_encodable(self.len() + additional);
		self.do_reservation(additional, Vec::<T>::reserve);
	}

	/// Ensures that the bit-vector has allocation capacity for *at least*
	/// `additional` more bits to be appended to it.
	///
	/// This differs from [`.reserve()`] by requesting that the allocator
	/// provide the minimum capacity necessary, rather than a potentially larger
	/// amount that the allocator may find more convenient.
	///
	/// Remember that this is a *request*: the allocator provides what it
	/// provides, and you cannot rely on the new capacity to be exactly minimal.
	/// You should still prefer `.reserve()`, especially if you expect to append
	/// to the bit-vector in the future.
	///
	/// ## Original
	///
	/// [`Vec::reserve_exact`](alloc::vec::Vec::reserve_exact)
	///
	/// ## Panics
	///
	/// This panics if the new capacity exceeds the bit-vector’s maximum.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv: BitVec = BitVec::with_capacity(80);
	/// assert!(bv.capacity() >= 80);
	/// bv.reserve_exact(800);
	/// assert!(bv.capacity() >= 800);
	/// ```
	///
	/// [`.reserve()`]: Self::reserve
	#[inline]
	pub fn reserve_exact(&mut self, additional: usize) {
		self.do_reservation(additional, Vec::<T>::reserve_exact);
	}

	/// Releases excess capacity back to the allocator.
	///
	/// Like [`.reserve_exact()`], this is a *request* to the allocator, not a
	/// command. The allocator may reclaim excess memory or may not.
	///
	/// ## Original
	///
	/// [`Vec::shrink_to_fit`](alloc::vec::Vec::shrink_to_fit)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv: BitVec = BitVec::with_capacity(1000);
	/// bv.push(true);
	/// bv.shrink_to_fit();
	/// ```
	///
	/// [`.reserve_exact()`]: Self::reserve_exact
	#[inline]
	pub fn shrink_to_fit(&mut self) {
		self.with_vec(|vec| vec.shrink_to_fit());
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "prefer `.into_boxed_bitslice() instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn into_boxed_slice(self) -> BitBox<T, O> {
		self.into_boxed_bitslice()
	}

	/// Shortens the bit-vector, keeping the first `new_len` bits and discarding
	/// the rest.
	///
	/// If `len` is greater than the bit-vector’s current length, this has no
	/// effect.
	///
	/// The [`.drain()`] method can emulate `.truncate()`, except that it yields
	/// the excess bits rather than discarding them.
	///
	/// Note that this has no effect on the allocated capacity of the
	/// bit-vector, **nor does it erase truncated memory**. Bits in the
	/// allocated memory that are outside of the [`.as_bitslice()`] view are
	/// always considered to have *initialized*, but **unspecified**, values,
	/// and you cannot rely on them to be zero.
	///
	/// ## Original
	///
	/// [`Vec::truncate`](alloc::vec::Vec::truncate)
	///
	/// ## Examples
	///
	/// Truncating a five-bit vector to two bits:
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 1, 0, 0, 1];
	/// bv.truncate(2);
	/// assert_eq!(bv.len(), 2);
	/// assert!(bv.as_raw_slice()[0].count_ones() >= 2);
	/// ```
	///
	/// No truncation occurs when `len` is greater than the bit-vector’s current
	/// length:
	///
	/// [`.as_bitslice()`]: Self::as_bitslice
	/// [`.drain()`]: Self::drain
	#[inline]
	pub fn truncate(&mut self, new_len: usize) {
		if new_len < self.len() {
			unsafe {
				self.set_len_unchecked(new_len);
			}
		}
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_slice(&self) -> &BitSlice<T, O> {
		self.as_bitslice()
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.as_mut_bitslice()` instead"]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	pub fn as_mut_slice(&mut self) -> &mut BitSlice<T, O> {
		self.as_mut_bitslice()
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

	/// Resizes a bit-vector to a new length.
	///
	/// ## Original
	///
	/// [`Vec::set_len`](alloc::vec::Vec::set_len)
	///
	/// ## Safety
	///
	/// **NOT ALL MEMORY IN THE ALLOCATION IS INITIALIZED!**
	///
	/// Memory in a bit-vector’s allocation is only initialized when the
	/// bit-vector grows into it normally (through [`.push()`] or one of the
	/// various `.extend*()` methods). Setting the length to a value beyond what
	/// was previously initialized, but still within the allocation, is
	/// undefined behavior.
	///
	/// The caller is responsible for ensuring that all memory up to (but not
	/// including) the new length has already been initialized.
	///
	/// ## Panics
	///
	/// This panics if `new_len` exceeds the capacity as reported by
	/// [`.capacity()`].
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 1, 0, 0, 1];
	/// unsafe {
	///   // The default storage type, `usize`, is at least 32 bits.
	///   bv.set_len(32);
	/// }
	/// assert_eq!(bv, bits![
	///   0, 1, 0, 0, 1, 0, 0, 0,
	///   0, 0, 0, 0, 0, 0, 0, 0,
	///   0, 0, 0, 0, 0, 0, 0, 0,
	///   0, 0, 0, 0, 0, 0, 0, 0,
	/// ]);
	/// //  `BitVec` guarantees that newly-initialized memory is zeroed.
	/// ```
	///
	/// [`.push()`]: Self::push
	/// [`.capacity()`]: Self::capacity
	#[inline]
	pub unsafe fn set_len(&mut self, new_len: usize) {
		let capa = self.capacity();
		assert!(
			new_len <= capa,
			"bit-vector capacity exceeded: {} > {}",
			new_len,
			capa,
		);
		self.set_len_unchecked(new_len);
	}

	/// Takes a bit out of the bit-vector.
	///
	/// The empty slot is filled with the last bit in the bit-vector, rather
	/// than shunting `index + 1 .. self.len()` down by one.
	///
	/// ## Original
	///
	/// [`Vec::swap_remove`](alloc::vec::Vec::swap_remove)
	///
	/// ## Panics
	///
	/// This panics if `index` is out of bounds (`self.len()` or greater).
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 1, 0, 0, 1];
	/// assert!(!bv.swap_remove(2));
	/// assert_eq!(bv, bits![0, 1, 1, 0]);
	/// ```
	#[inline]
	pub fn swap_remove(&mut self, index: usize) -> bool {
		self.assert_in_bounds(index, 0 .. self.len());
		let last = self.len() - 1;
		unsafe {
			self.swap_unchecked(index, last);
			self.set_len(last);
			*self.get_unchecked(last)
		}
	}

	/// Inserts a bit at a given position, shifting all bits after it one spot
	/// to the right.
	///
	/// `index` may be any value up to *and including* `self.len()`. If it is
	/// `self.len()`, it behaves equivalently to `.push()`.
	///
	/// ## Original
	///
	/// [`Vec::insert`](alloc::vec::Vec::insert)
	///
	/// ## Panics
	///
	/// This panics if `index` is out of bounds (including `self.len()`).
	#[inline]
	pub fn insert(&mut self, index: usize, value: bool) {
		self.assert_in_bounds(index, 0 ..= self.len());
		self.push(value);
		unsafe { self.get_unchecked_mut(index ..) }.rotate_right(1);
	}

	/// Removes a bit at a given position, shifting all bits after it one spot
	/// to the left.
	///
	/// `index` may be any value up to, but **not** including, `self.len()`.
	///
	/// ## Original
	///
	/// [`Vec::remove`](alloc::vec::Vec::remove)
	///
	/// ## Panics
	///
	/// This panics if `index` is out of bounds (excluding `self.len()`).
	#[inline]
	pub fn remove(&mut self, index: usize) -> bool {
		self.assert_in_bounds(index, 0 .. self.len());
		let last = self.len() - 1;
		unsafe {
			self.get_unchecked_mut(index ..).rotate_left(1);
			let out = *self.get_unchecked(last);
			self.set_len(last);
			out
		}
	}

	/// Retains only the bits that the predicate allows.
	///
	/// Bits are deleted from the vector when the predicate function returns
	/// false. This function is linear in `self.len()`.
	///
	/// ## Original
	///
	/// [`Vec::retain`](alloc::vec::Vec::retain)
	///
	/// ## API Differences
	///
	/// The predicate receives both the index of the bit as well as its value,
	/// in order to allow the predicate to have more than one bit of
	/// keep/discard information.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 1, 0, 0, 1];
	/// bv.retain(|idx, _| idx % 2 == 0);
	/// assert_eq!(bv, bits![0,    0,    1]);
	/// ```
	#[inline]
	pub fn retain<F>(&mut self, mut func: F)
	where F: FnMut(usize, &bool) -> bool {
		let mut len = self.len();
		let mut hole_ptr = self.as_mut_bitptr();
		let mut reader = self.as_bitptr_range().enumerate();

		//  Advance until the *first* hole is created. This avoids writing into
		//  the bit-slice when no change takes place.
		for (idx, bitptr) in reader.by_ref() {
			let bit = unsafe { bitptr.read() };
			if func(idx, &bit) {
				hole_ptr = unsafe { hole_ptr.add(1) };
			}
			else {
				len -= 1;
				break;
			}
		}
		//  Now that a hole exists, switch to a loop that always writes into the
		//  hole pointer.
		for (idx, bitptr) in reader {
			let bit = unsafe { bitptr.read() };
			if func(idx, &bit) {
				hole_ptr = unsafe {
					hole_ptr.write(bit);
					hole_ptr.add(1)
				};
			}
			else {
				len -= 1;
			}
		}
		//  Discard the bits that did not survive the predicate.
		unsafe {
			self.set_len_unchecked(len);
		}
	}

	/// Appends a single bit to the vector.
	///
	/// ## Original
	///
	/// [`Vec::push`](alloc::vec::Vec::push)
	///
	/// ## Panics
	///
	/// This panics if the push would cause the bit-vector to exceed its maximum
	/// capacity.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 0];
	/// bv.push(true);
	/// assert_eq!(bv.as_bitslice(), bits![0, 0, 1]);
	/// ```
	#[inline]
	pub fn push(&mut self, value: bool) {
		let len = self.len();
		let new_len = len + 1;
		Self::assert_len_encodable(new_len);
		//  Push a new `T` into the underlying buffer if needed.
		if len == 0 || self.bitspan.tail() == BitEnd::MAX {
			self.with_vec(|vec| vec.push(T::ZERO));
		}
		//  Write `value` into the now-safely-allocated `len` slot.
		unsafe {
			self.set_len_unchecked(new_len);
			self.set_unchecked(len, value);
		}
	}

	/// Attempts to remove the trailing bit from the bit-vector.
	///
	/// This returns `None` if the bit-vector is empty.
	///
	/// ## Original
	///
	/// [`Vec::pop`](alloc::vec::Vec::pop)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 1];
	/// assert!(bv.pop().unwrap());
	/// assert!(!bv.pop().unwrap());
	/// assert!(bv.pop().is_none());
	/// ```
	#[inline]
	pub fn pop(&mut self) -> Option<bool> {
		match self.len() {
			0 => None,
			n => unsafe {
				let new_len = n - 1;
				let out = Some(*self.get_unchecked(new_len));
				self.set_len_unchecked(new_len);
				out
			},
		}
	}

	/// Moves all the bits out of `other` into the back of `self`.
	///
	/// The `other` bit-vector is emptied after this occurs.
	///
	/// ## Original
	///
	/// [`Vec::append`](alloc::vec::Vec::append)
	///
	/// ## API Differences
	///
	/// This permits `other` to have different type parameters than `self`, and
	/// does not require that it be literally `Self`.
	///
	/// ## Panics
	///
	/// This panics if `self.len() + other.len()` exceeds the maximum capacity
	/// of a bit-vector.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv1 = bitvec![u16, Msb0; 0; 10];
	/// let mut bv2 = bitvec![u32, Lsb0; 1; 10];
	///
	/// bv1.append(&mut bv2);
	///
	/// assert_eq!(bv1.count_ones(), 10);
	/// assert_eq!(bv1.count_zeros(), 10);
	/// assert!(bv2.is_empty());
	/// ```
	#[inline]
	pub fn append<T2, O2>(&mut self, other: &mut BitVec<T2, O2>)
	where
		T2: BitStore,
		O2: BitOrder,
	{
		self.extend_from_bitslice(other);
		other.clear();
	}

	/// Iterates over a portion of the bit-vector, *removing* all yielded bits
	/// from it.
	///
	/// When the iterator drops, *all* bits in its coverage are removed from
	/// `self`, even if the iterator did not yield them. If the iterator is
	/// leaked or otherwise forgotten, and its destructor never runs, then the
	/// amount of un-yielded bits removed from the bit-vector is not specified.
	///
	/// ## Original
	///
	/// [`Vec::drain`](alloc::vec::Vec::drain)
	///
	/// ## Panics
	///
	/// This panics if `range` departs `0 .. self.len()`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 1, 0, 0, 1];
	/// let bv2 = bv.drain(1 ..= 3).collect::<BitVec>();
	/// assert_eq!(bv, bits![0,          1]);
	/// assert_eq!(bv2, bits![1, 0, 0]);
	///
	/// // A full range clears the bit-vector.
	/// bv.drain(..);
	/// assert!(bv.is_empty());
	/// ```
	#[inline]
	pub fn drain<R>(&mut self, range: R) -> Drain<T, O>
	where R: RangeBounds<usize> {
		Drain::new(self, range)
	}

	/// Empties the bit-vector.
	///
	/// This does not affect the allocated capacity.
	///
	/// ## Original
	///
	/// [`Vec::clear`](alloc::vec::Vec::clear)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 1, 0, 0, 1];
	/// bv.clear();
	/// assert!(bv.is_empty());
	/// ```
	#[inline]
	pub fn clear(&mut self) {
		self.truncate(0);
	}

	/// Gets the length of the bit-vector.
	///
	/// This is equivalent to `BitSlice::len`; it is provided as an inherent
	/// method here rather than relying on `Deref` forwarding so that you can
	/// write `BitVec::len` as a named function item.
	///
	/// ## Original
	///
	/// [`Vec::len`](alloc::vec::Vec::len)
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn len(&self) -> usize {
		self.bitspan.len()
	}

	/// Tests if the bit-vector is empty.
	///
	/// This is equivalent to `BitSlice::is_empty`; it is provided as an
	/// inherent method here rather than relying on `Deref` forwarding so that
	/// you can write `BitVec::is_empty` as a named function item.
	///
	/// ## Original
	///
	/// [`Vec::is_empty`](alloc::vec::Vec::is_empty)
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn is_empty(&self) -> bool {
		self.bitspan.len() == 0
	}

	/// Splits the bit-vector in half at an index, moving `self[at ..]` out into
	/// a new bit-vector.
	///
	/// ## Original
	///
	/// [`Vec::split_off`](alloc::vec::Vec::split_off)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 1, 0, 0, 1];
	/// let bv2 = bv.split_off(2);
	/// assert_eq!((&*bv, &*bv2), (bits![0, 1], bits![0, 0, 1]));
	/// ```
	#[inline]
	pub fn split_off(&mut self, at: usize) -> Self {
		let len = self.len();
		self.assert_in_bounds(at, 0 ..= len);
		let (this, that) = unsafe {
			self.bitspan
				.into_bitslice_mut()
				.split_at_unchecked_mut_noalias(at)
		};
		self.bitspan = this.as_mut_bitspan();
		Self::from_bitslice(that)
	}

	/// Resizes the bit-vector to a new length, using a function to produce each
	/// inserted bit.
	///
	/// If `new_len` is less than `self.len()`, this is a truncate operation; if
	/// it is greater, then `self` is extended by repeatedly pushing `func()`.
	///
	/// ## Original
	///
	/// [`Vec::resize_with`](alloc::vec::Vec::resize_with)
	///
	/// ## API Differences
	///
	/// The generator function receives the index into which its bit will be
	/// placed.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![1; 2];
	/// bv.resize_with(5, |idx| idx % 2 == 1);
	/// assert_eq!(bv, bits![1, 1, 0, 1, 0]);
	/// ```
	#[inline]
	pub fn resize_with<F>(&mut self, new_len: usize, mut func: F)
	where F: FnMut(usize) -> bool {
		let old_len = self.len();
		self.resize(new_len, false);
		if new_len > old_len {
			for (bitptr, idx) in unsafe { self.get_unchecked_mut(old_len ..) }
				.as_mut_bitptr_range()
				.zip(old_len ..)
			{
				unsafe {
					bitptr.write(func(idx));
				}
			}
		}
	}

	/// Destroys the `BitVec` handle without destroying the bit-vector
	/// allocation. The allocation is returned as an `&mut BitSlice` that lasts
	/// for the remaining program lifetime.
	///
	/// You *may* call [`BitBox::from_raw`] on this slice handle exactly once in
	/// order to reap the allocation before program exit. That function takes a
	/// mutable pointer, not a mutable reference, so you must ensure that the
	/// returned reference is never used again after restoring the allocation
	/// handle.
	///
	/// ## Original
	///
	/// [`Vec::leak`](alloc::vec::Vec::leak)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bv = bitvec![0, 0, 1];
	/// let static_bits: &'static mut BitSlice = bv.leak();
	/// static_bits.set(0, true);
	/// assert_eq!(static_bits, bits![1, 0, 1]);
	///
	/// let bb = unsafe { BitBox::from_raw(static_bits) };
	/// // static_bits may no longer be used.
	/// drop(bb); // explicitly reap memory before program exit
	/// ```
	///
	/// [`BitBox::leak`]: crate::boxed::BitBox::leak
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn leak<'a>(self) -> &'a mut BitSlice<T, O> {
		self.into_boxed_bitslice().pipe(BitBox::leak)
	}

	/// Resizes the bit-vector to a new length. New bits are initialized to
	/// `value`.
	///
	/// ## Original
	///
	/// [`Vec::resize`](alloc::vec::Vec::resize)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0; 2];
	/// bv.resize(5, true);
	/// assert_eq!(bv, bits![0, 0, 1, 1, 1]);
	/// ```
	#[inline]
	pub fn resize(&mut self, new_len: usize, value: bool) {
		let len = self.len();
		if new_len > len {
			self.reserve(new_len - len);
			unsafe {
				self.set_len(new_len);
				self.get_unchecked_mut(len .. new_len).fill(value);
			}
		}
		else {
			self.truncate(new_len);
		}
	}

	#[inline]
	#[cfg(not(tarpaulin_include))]
	#[allow(missing_docs, clippy::missing_docs_in_private_items)]
	#[deprecated = "use `.extend_from_bitslice()` or `.extend_from_raw_slice()` \
	                instead"]
	pub fn extend_from_slice<T2, O2>(&mut self, other: &BitSlice<T2, O2>)
	where
		T2: BitStore,
		O2: BitOrder,
	{
		self.extend_from_bitslice(other);
	}

	/// Extends `self` by copying an internal range of its bit-slice as the
	/// region to append.
	///
	/// ## Original
	///
	/// [`Vec::extend_from_within`](alloc::vec::Vec::extend_from_within)
	///
	/// ## Panics
	///
	/// This panics if `src` is not within `0 .. self.len()`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 1, 0, 0, 1];
	/// bv.extend_from_within(1 .. 4);
	/// assert_eq!(bv, bits![0, 1, 0, 0, 1, 1, 0, 0]);
	/// ```
	#[inline]
	pub fn extend_from_within<R>(&mut self, src: R)
	where R: RangeExt<usize> {
		let old_len = self.len();
		let src = src.normalize(0, old_len);
		self.assert_in_bounds(src.end, 0 .. old_len);
		self.resize(old_len + src.len(), false);
		unsafe {
			self.copy_within_unchecked(src, old_len);
		}
	}

	/// Modifies [`self.drain()`] so that the removed bit-slice is instead
	/// replaced with the contents of another bit-stream.
	///
	/// As with `.drain()`, the specified range is always removed from the
	/// bit-vector even if the splicer is not fully consumed, and the splicer
	/// does not specify how many bits are removed if it leaks.
	///
	/// The replacement source is only consumed when the splicer drops; however,
	/// it may be pulled before then. The replacement source cannot assume that
	/// there will be a delay between creation of the splicer and when it must
	/// begin producing bits.
	///
	/// This copies the `Vec::splice` implementation; see its documentation for
	/// more details about how the replacement should act.
	///
	/// ## Original
	///
	/// [`Vec::splice`](alloc::vec::Vec::splice)
	///
	/// ## Panics
	///
	/// This panics if `range` departs `0 .. self.len()`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 1, 1];
	/// //                   a  b  c
	/// let mut yank = bv.splice(
	///   .. 2,
	///   bits![static 1, 1, 0].iter().by_vals(),
	/// //             d  e  f
	/// );
	///
	/// assert!(!yank.next().unwrap()); // a
	/// assert!(yank.next().unwrap()); // b
	/// drop(yank);
	/// assert_eq!(bv, bits![1, 1, 0, 1]);
	/// //                   d  e  f  c
	/// ```
	///
	/// [`self.drain()`]: Self::drain
	#[inline]
	pub fn splice<R, I>(
		&mut self,
		range: R,
		replace_with: I,
	) -> Splice<T, O, I::IntoIter>
	where
		R: RangeBounds<usize>,
		I: IntoIterator<Item = bool>,
	{
		Splice::new(self.drain(range), replace_with)
	}
}
