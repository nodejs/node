#![cfg(feature = "alloc")]
#![doc = include_str!("../doc/boxed.md")]

use alloc::boxed::Box;
use core::{
	mem::ManuallyDrop,
	slice,
};

use tap::{
	Pipe,
	Tap,
};
use wyz::comu::Mut;

use crate::{
	index::BitIdx,
	mem,
	order::{
		BitOrder,
		Lsb0,
	},
	ptr::{
		BitPtr,
		BitSpan,
	},
	slice::BitSlice,
	store::BitStore,
	vec::BitVec,
	view::BitView,
};

mod api;
mod iter;
mod ops;
mod tests;
mod traits;

pub use self::iter::IntoIter;

#[repr(transparent)]
#[doc = include_str!("../doc/boxed/BitBox.md")]
pub struct BitBox<T = usize, O = Lsb0>
where
	T: BitStore,
	O: BitOrder,
{
	/// Describes the region that the box owns.
	bitspan: BitSpan<Mut, T, O>,
}

impl<T, O> BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Copies a bit-slice region into a new bit-box allocation.
	///
	/// The referent memory is `memcpy`d into the heap, exactly preserving the
	/// original bit-slice’s memory layout and contents. This allows the
	/// function to run as fast as possible, but misaligned source bit-slices
	/// may result in decreased performance or unexpected layout behavior during
	/// use. You can use [`.force_align()`] to ensure that the referent
	/// bit-slice is aligned in memory.
	///
	/// ## Notes
	///
	/// Bits in the allocation of the source bit-slice, but outside its own
	/// description of that memory, have an **unspecified**, but initialized,
	/// value. You may not rely on their contents in any way, and you *should*
	/// call [`.force_align()`] and/or [`.fill_uninitialized()`] if you are
	/// going to inspect the underlying memory of the new allocation.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let data = 0b0101_1011u8;
	/// let bits = data.view_bits::<Msb0>();
	/// let bb = BitBox::from_bitslice(&bits[2 ..]);
	/// assert_eq!(bb, bits[2 ..]);
	/// ```
	///
	/// [`.fill_uninitialized()`]: Self::fill_uninitialized
	/// [`.force_align()`]: Self::force_align
	#[inline]
	pub fn from_bitslice(slice: &BitSlice<T, O>) -> Self {
		BitVec::from_bitslice(slice).into_boxed_bitslice()
	}

	/// Converts a `Box<[T]>` into a `BitBox<T, O>`, in place.
	///
	/// This does not affect the referent buffer, and only transforms the
	/// handle.
	///
	/// ## Panics
	///
	/// This panics if the provided `boxed` slice is too long to view as a
	/// bit-slice region.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let boxed: Box<[u8]> = Box::new([0; 40]);
	/// let addr = boxed.as_ptr();
	/// let bb = BitBox::<u8>::from_boxed_slice(boxed);
	/// assert_eq!(bb, bits![0; 320]);
	/// assert_eq!(addr, bb.as_raw_slice().as_ptr());
	/// ```
	#[inline]
	pub fn from_boxed_slice(boxed: Box<[T]>) -> Self {
		Self::try_from_boxed_slice(boxed)
			.expect("slice was too long to be converted into a `BitBox`")
	}

	/// Attempts to convert an ordinary boxed slice into a boxed bit-slice.
	///
	/// This does not perform a copy or reällocation; it only attempts to
	/// transform the handle. Because `Box<[T]>` can be longer than `BitBox`es,
	/// it may fail, and will return the original handle if it does.
	///
	/// It is unlikely that you have a single `Box<[_]>` that is too large to
	/// convert into a bit-box. You can find the length restrictions as the
	/// bit-slice associated constants [`MAX_BITS`] and [`MAX_ELTS`].
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let boxed: Box<[u8]> = Box::new([0u8; 40]);
	/// let addr = boxed.as_ptr();
	/// let bb = BitBox::<u8>::try_from_boxed_slice(boxed).unwrap();
	/// assert_eq!(bb, bits![0; 320]);
	/// assert_eq!(addr, bb.as_raw_slice().as_ptr());
	/// ```
	///
	/// [`MAX_BITS`]: crate::slice::BitSlice::MAX_BITS
	/// [`MAX_ELTS`]: crate::slice::BitSlice::MAX_ELTS
	#[inline]
	pub fn try_from_boxed_slice(boxed: Box<[T]>) -> Result<Self, Box<[T]>> {
		let mut boxed = ManuallyDrop::new(boxed);

		BitPtr::from_mut_slice(boxed.as_mut())
			.span(boxed.len() * mem::bits_of::<T::Mem>())
			.map(|bitspan| Self { bitspan })
			.map_err(|_| ManuallyDrop::into_inner(boxed))
	}

	/// Converts the bit-box back into an ordinary boxed element slice.
	///
	/// This does not touch the allocator or the buffer contents; it is purely a
	/// handle transform.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bb = bitbox![0; 5];
	/// let addr = bb.as_raw_slice().as_ptr();
	/// let boxed = bb.into_boxed_slice();
	/// assert_eq!(boxed[..], [0][..]);
	/// assert_eq!(addr, boxed.as_ptr());
	/// ```
	#[inline]
	pub fn into_boxed_slice(self) -> Box<[T]> {
		self.pipe(ManuallyDrop::new)
			.as_raw_mut_slice()
			.pipe(|slice| unsafe { Box::from_raw(slice) })
	}

	/// Converts the bit-box into a bit-vector.
	///
	/// This uses the Rust allocator API, and does not guarantee whether or not
	/// a reällocation occurs internally.
	///
	/// The resulting bit-vector can be converted back into a bit-box via
	/// [`BitBox::into_boxed_bitslice`][0].
	///
	/// ## Original
	///
	/// [`slice::into_vec`](https://doc.rust-lang.org/std/primitive.slice.html#method.into_vec)
	///
	/// ## API Differences
	///
	/// The original function is implemented in an `impl<T> [T]` block, despite
	/// taking a `Box<[T]>` receiver. Since `BitBox` cannot be used as an
	/// explicit receiver outside its own `impl` blocks, the method is relocated
	/// here.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bb = bitbox![0, 1, 0, 0, 1];
	/// let bv = bb.into_bitvec();
	///
	/// assert_eq!(bv, bitvec![0, 1, 0, 0, 1]);
	/// ```
	///
	/// [0]: crate::vec::BitVec::into_boxed_bitslice
	#[inline]
	pub fn into_bitvec(self) -> BitVec<T, O> {
		let bitspan = self.bitspan;
		/* This pipeline converts the underlying `Box<[T]>` into a `Vec<T>`,
		 * then converts that into a `BitVec`. This handles any changes that
		 * may occur in the allocator. Once done, the original head/span
		 * values need to be written into the `BitVec`, since the conversion
		 * from `Vec` always fully spans the live elements.
		 */
		self.pipe(ManuallyDrop::new)
			.with_box(|b| unsafe { ManuallyDrop::take(b) })
			.into_vec()
			.pipe(BitVec::from_vec)
			.tap_mut(|bv| unsafe {
				// len first! Otherwise, the descriptor might briefly go out of
				// bounds.
				bv.set_len_unchecked(bitspan.len());
				bv.set_head(bitspan.head());
			})
	}

	/// Explicitly views the bit-box as a bit-slice.
	#[inline]
	pub fn as_bitslice(&self) -> &BitSlice<T, O> {
		unsafe { self.bitspan.into_bitslice_ref() }
	}

	/// Explicitly views the bit-box as a mutable bit-slice.
	#[inline]
	pub fn as_mut_bitslice(&mut self) -> &mut BitSlice<T, O> {
		unsafe { self.bitspan.into_bitslice_mut() }
	}

	/// Views the bit-box as a slice of its underlying memory elements.
	///
	/// Because bit-boxes uniquely own their buffer, they can safely view the
	/// underlying buffer without dealing with contending neighbors.
	#[inline]
	pub fn as_raw_slice(&self) -> &[T] {
		let (data, len) =
			(self.bitspan.address().to_const(), self.bitspan.elements());
		unsafe { slice::from_raw_parts(data, len) }
	}

	/// Views the bit-box as a mutable slice of its underlying memory elements.
	///
	/// Because bit-boxes uniquely own their buffer, they can safely view the
	/// underlying buffer without dealing with contending neighbors.
	#[inline]
	pub fn as_raw_mut_slice(&mut self) -> &mut [T] {
		let (data, len) =
			(self.bitspan.address().to_mut(), self.bitspan.elements());
		unsafe { slice::from_raw_parts_mut(data, len) }
	}

	/// Sets the unused bits outside the `BitBox` buffer to a fixed value.
	///
	/// This method modifies all bits that the allocated buffer owns but which
	/// are outside the `self.as_bitslice()` view. `bitvec` guarantees that all
	/// owned bits are initialized to *some* value, but does not guarantee
	/// *which* value. This method can be used to make all such unused bits have
	/// a known value after the call, so that viewing the underlying memory
	/// directly has consistent results.
	///
	/// Note that the crate implementation guarantees that all bits owned by its
	/// handles are stably initialized according to the language and compiler
	/// rules! `bitvec` will never cause UB by using uninitialized memory.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = 0b1011_0101u8.view_bits::<Msb0>();
	/// let mut bb = BitBox::from_bitslice(&bits[2 .. 6]);
	/// assert_eq!(bb.count_ones(), 3);
	/// // Remember, the two bits on each edge are unspecified, and cannot be
	/// // observed! They must be masked away for the test to be meaningful.
	/// assert_eq!(bb.as_raw_slice()[0] & 0x3C, 0b00_1101_00u8);
	///
	/// bb.fill_uninitialized(false);
	/// assert_eq!(bb.as_raw_slice(), &[0b00_1101_00u8]);
	///
	/// bb.fill_uninitialized(true);
	/// assert_eq!(bb.as_raw_slice(), &[0b11_1101_11u8]);
	/// ```
	#[inline]
	pub fn fill_uninitialized(&mut self, value: bool) {
		let (_, head, bits) = self.bitspan.raw_parts();
		let head = head.into_inner() as usize;
		let tail = head + bits;
		let all = self.as_raw_mut_slice().view_bits_mut::<O>();
		unsafe {
			all.get_unchecked_mut(.. head).fill(value);
			all.get_unchecked_mut(tail ..).fill(value);
		}
	}

	/// Ensures that the allocated buffer has no dead bits between the start of
	/// the buffer and the start of the live bit-slice.
	///
	/// This is useful for ensuring a consistent memory layout in bit-boxes
	/// created by cloning an arbitrary bit-slice into the heap. As bit-slices
	/// can begin and end anywhere in memory, the [`::from_bitslice()`] function
	/// does not attempt to normalize them and only does a fast element-wise
	/// copy when creating the bit-box.
	///
	/// The value of dead bits that are in the allocation but not in the live
	/// region are *initialized*, but do not have a *specified* value. After
	/// calling this method, you should use [`.fill_uninitialized()`] to set the
	/// excess bits in the buffer to a fixed value.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = &0b10_1101_01u8.view_bits::<Msb0>()[2 .. 6];
	/// let mut bb = BitBox::from_bitslice(bits);
	/// // Remember, the two bits on each edge are unspecified, and cannot be
	/// // observed! They must be masked away for the test to be meaningful.
	/// assert_eq!(bb.as_raw_slice()[0] & 0x3C, 0b00_1101_00u8);
	///
	/// bb.force_align();
	/// bb.fill_uninitialized(false);
	/// assert_eq!(bb.as_raw_slice(), &[0b1101_0000u8]);
	/// ```
	///
	/// [`::from_bitslice()`]: Self::from_bitslice
	/// [`.fill_uninitialized()`]: Self::fill_uninitialized
	#[inline]
	pub fn force_align(&mut self) {
		let head = self.bitspan.head();
		if head == BitIdx::MIN {
			return;
		}
		let head = head.into_inner() as usize;
		let last = self.len() + head;
		unsafe {
			self.bitspan.set_head(BitIdx::MIN);
			self.copy_within_unchecked(head .. last, 0);
		}
	}

	/// Permits a function to modify the `Box` backing storage of a `BitBox`
	/// handle.
	///
	/// This produces a temporary `Box` view of the bit-box’s buffer and allows
	/// a function to have mutable access to it. After the callback returns, the
	/// `Box` is written back into `self` and forgotten.
	#[inline]
	fn with_box<F, R>(&mut self, func: F) -> R
	where F: FnOnce(&mut ManuallyDrop<Box<[T]>>) -> R {
		self.as_raw_mut_slice()
			.pipe(|raw| unsafe { Box::from_raw(raw) })
			.pipe(ManuallyDrop::new)
			.pipe_ref_mut(func)
	}
}
