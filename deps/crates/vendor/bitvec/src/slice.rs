#![doc = include_str!("../doc/slice.md")]

#[cfg(feature = "alloc")]
use alloc::vec::Vec;
use core::{
	marker::PhantomData,
	ops::RangeBounds,
};

use funty::Integral;
use tap::Pipe;
#[cfg(feature = "alloc")]
use tap::Tap;
use wyz::{
	bidi::BidiIterator,
	comu::{
		Const,
		Mut,
	},
	range::RangeExt,
};

#[cfg(feature = "alloc")]
use crate::vec::BitVec;
use crate::{
	domain::{
		BitDomain,
		Domain,
	},
	mem,
	order::{
		BitOrder,
		Lsb0,
		Msb0,
	},
	ptr::{
		self as bv_ptr,
		BitPtr,
		BitPtrRange,
		BitSpan,
		BitSpanError,
	},
	store::BitStore,
};

mod api;
mod iter;
mod ops;
mod specialization;
mod tests;
mod traits;

pub use self::{
	api::*,
	iter::*,
};

#[repr(transparent)]
#[doc = include_str!("../doc/slice/BitSlice.md")]
pub struct BitSlice<T = usize, O = Lsb0>
where
	T: BitStore,
	O: BitOrder,
{
	/// The ordering of bits within a `T` register.
	_ord: PhantomData<O>,
	/// The register type used for storage.
	_typ: PhantomData<[T]>,
	/// Indicate that this is a newtype wrapper over a wholly-untyped slice.
	///
	/// This is necessary in order for the Rust compiler to remove restrictions
	/// on the possible values of reference handles to this type. Any other
	/// slice type here (such as `[u8]` or `[T]`) would require that `&/mut
	/// BitSlice` handles have values that correctly describe the region, and
	/// the encoding *does not* do this. As such, reference handles to
	/// `BitSlice` must not be even implicitly dereferenceäble to real memory,
	/// and the slice must be a ZST.
	///
	/// References to a ZST have no restrictions about what the values can be,
	/// as they are never able to dereference real memory and thus both
	/// addresses and lengths are meaningless to the memory inspector.
	///
	/// See `ptr::span` for more information on the encoding scheme used in
	/// references to `BitSlice`.
	_mem: [()],
}

/// Constructors.
impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Produces an empty bit-slice with an arbitrary lifetime.
	///
	/// ## Original
	///
	/// This is equivalent to the `&[]` literal.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!(BitSlice::<u16, LocalBits>::empty().is_empty());
	/// assert_eq!(bits![], BitSlice::<u8, Msb0>::empty());
	/// ```
	#[inline]
	pub fn empty<'a>() -> &'a Self {
		unsafe { BitSpan::<Const, T, O>::EMPTY.into_bitslice_ref() }
	}

	/// Produces an empty bit-slice with an arbitrary lifetime.
	///
	/// ## Original
	///
	/// This is equivalent to the `&mut []` literal.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!(BitSlice::<u16, LocalBits>::empty_mut().is_empty());
	/// assert_eq!(bits![mut], BitSlice::<u8, Msb0>::empty_mut());
	/// ```
	#[inline]
	pub fn empty_mut<'a>() -> &'a mut Self {
		unsafe { BitSpan::<Mut, T, O>::EMPTY.into_bitslice_mut() }
	}

	/// Constructs a shared `&BitSlice` reference over a shared element.
	///
	/// The [`BitView`] trait, implemented on all [`BitStore`] implementors,
	/// provides a [`.view_bits::<O>()`] method which delegates to this function
	/// and may be more convenient for you to write.
	///
	/// ## Parameters
	///
	/// - `elem`: A shared reference to a memory element.
	///
	/// ## Returns
	///
	/// A shared `&BitSlice` over `elem`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let elem = 0u8;
	/// let bits = BitSlice::<_, Lsb0>::from_element(&elem);
	/// assert_eq!(bits.len(), 8);
	///
	/// let bits = elem.view_bits::<Lsb0>();
	/// ```
	///
	/// [`BitStore`]: crate::store::BitStore
	/// [`BitView`]: crate::view::BitView
	/// [`.view_bits::<O>()`]: crate::view::BitView::view_bits
	#[inline]
	pub fn from_element(elem: &T) -> &Self {
		unsafe {
			BitPtr::from_ref(elem)
				.span_unchecked(mem::bits_of::<T::Mem>())
				.into_bitslice_ref()
		}
	}

	/// Constructs an exclusive `&mut BitSlice` reference over an element.
	///
	/// The [`BitView`] trait, implemented on all [`BitStore`] implementors,
	/// provides a [`.view_bits_mut::<O>()`] method which delegates to this
	/// function and may be more convenient for you to write.
	///
	/// ## Parameters
	///
	/// - `elem`: An exclusive reference to a memory element.
	///
	/// ## Returns
	///
	/// An exclusive `&mut BitSlice` over `elem`.
	///
	/// Note that the original `elem` reference will be inaccessible for the
	/// duration of the returned bit-slice handle’s lifetime.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut elem = 0u8;
	/// let bits = BitSlice::<_, Lsb0>::from_element_mut(&mut elem);
	/// bits.set(1, true);
	/// assert!(bits[1]);
	/// assert_eq!(elem, 2);
	///
	/// let bits = elem.view_bits_mut::<Lsb0>();
	/// ```
	///
	/// [`BitStore`]: crate::store::BitStore
	/// [`BitView`]: crate::view::BitView
	/// [`.view_bits_mut::<O>()`]: crate::view::BitView::view_bits_mut
	#[inline]
	pub fn from_element_mut(elem: &mut T) -> &mut Self {
		unsafe {
			BitPtr::from_mut(elem)
				.span_unchecked(mem::bits_of::<T::Mem>())
				.into_bitslice_mut()
		}
	}

	/// Constructs a shared `&BitSlice` reference over a slice of elements.
	///
	/// The [`BitView`] trait, implemented on all `[T]` slices, provides a
	/// [`.view_bits::<O>()`] method which delegates to this function and may be
	/// more convenient for you to write.
	///
	/// ## Parameters
	///
	/// - `slice`: A shared reference to a slice of memory elements.
	///
	/// ## Returns
	///
	/// A shared `BitSlice` reference over all of `slice`.
	///
	/// ## Panics
	///
	/// This will panic if `slice` is too long to encode as a bit-slice view.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let data = [0u16, 1];
	/// let bits = BitSlice::<_, Lsb0>::from_slice(&data);
	/// assert!(bits[16]);
	///
	/// let bits = data.view_bits::<Lsb0>();
	/// ```
	///
	/// [`BitView`]: crate::view::BitView
	/// [`.view_bits::<O>()`]: crate::view::BitView::view_bits
	#[inline]
	pub fn from_slice(slice: &[T]) -> &Self {
		Self::try_from_slice(slice).unwrap()
	}

	/// Attempts to construct a shared `&BitSlice` reference over a slice of
	/// elements.
	///
	/// The [`BitView`], implemented on all `[T]` slices, provides a
	/// [`.try_view_bits::<O>()`] method which delegates to this function and
	/// may be more convenient for you to write.
	///
	/// This is *very hard*, if not impossible, to cause to fail. Rust will not
	/// create excessive arrays on 64-bit architectures.
	///
	/// ## Parameters
	///
	/// - `slice`: A shared reference to a slice of memory elements.
	///
	/// ## Returns
	///
	/// A shared `&BitSlice` over `slice`. If `slice` is longer than can be
	/// encoded into a `&BitSlice` (see [`MAX_ELTS`]), this will fail and return
	/// the original `slice` as an error.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let data = [0u8, 1];
	/// let bits = BitSlice::<_, Msb0>::try_from_slice(&data).unwrap();
	/// assert!(bits[15]);
	///
	/// let bits = data.try_view_bits::<Msb0>().unwrap();
	/// ```
	///
	/// [`BitView`]: crate::view::BitView
	/// [`MAX_ELTS`]: Self::MAX_ELTS
	/// [`.try_view_bits::<O>()`]: crate::view::BitView::try_view_bits
	#[inline]
	pub fn try_from_slice(slice: &[T]) -> Result<&Self, BitSpanError<T>> {
		let elts = slice.len();
		if elts >= Self::MAX_ELTS {
			elts.saturating_mul(mem::bits_of::<T::Mem>())
				.pipe(BitSpanError::TooLong)
				.pipe(Err)
		}
		else {
			Ok(unsafe { Self::from_slice_unchecked(slice) })
		}
	}

	/// Constructs an exclusive `&mut BitSlice` reference over a slice of
	/// elements.
	///
	/// The [`BitView`] trait, implemented on all `[T]` slices, provides a
	/// [`.view_bits_mut::<O>()`] method which delegates to this function and
	/// may be more convenient for you to write.
	///
	/// ## Parameters
	///
	/// - `slice`: An exclusive reference to a slice of memory elements.
	///
	/// ## Returns
	///
	/// An exclusive `&mut BitSlice` over all of `slice`.
	///
	/// ## Panics
	///
	/// This panics if `slice` is too long to encode as a bit-slice view.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut data = [0u16; 2];
	/// let bits = BitSlice::<_, Lsb0>::from_slice_mut(&mut data);
	/// bits.set(0, true);
	/// bits.set(17, true);
	/// assert_eq!(data, [1, 2]);
	///
	/// let bits = data.view_bits_mut::<Lsb0>();
	/// ```
	///
	/// [`BitView`]: crate::view::BitView
	/// [`.view_bits_mut::<O>()`]: crate::view::BitView::view_bits_mut
	#[inline]
	pub fn from_slice_mut(slice: &mut [T]) -> &mut Self {
		Self::try_from_slice_mut(slice).unwrap()
	}

	/// Attempts to construct an exclusive `&mut BitSlice` reference over a
	/// slice of elements.
	///
	/// The [`BitView`] trait, implemented on all `[T]` slices, provides a
	/// [`.try_view_bits_mut::<O>()`] method which delegates to this function
	/// and may be more convenient for you to write.
	///
	/// ## Parameters
	///
	/// - `slice`: An exclusive reference to a slice of memory elements.
	///
	/// ## Returns
	///
	/// An exclusive `&mut BitSlice` over `slice`. If `slice` is longer than can
	/// be encoded into a `&mut BitSlice` (see [`MAX_ELTS`]), this will fail and
	/// return the original `slice` as an error.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut data = [0u8; 2];
	/// let bits = BitSlice::<_, Msb0>::try_from_slice_mut(&mut data).unwrap();
	/// bits.set(7, true);
	/// bits.set(15, true);
	/// assert_eq!(data, [1; 2]);
	///
	/// let bits = data.try_view_bits_mut::<Msb0>().unwrap();
	/// ```
	///
	/// [`BitView`]: crate::view::BitView
	/// [`MAX_ELTS`]: Self::MAX_ELTS
	/// [`.try_view_bits_mut::<O>()`]: crate::view::BitView::try_view_bits_mut
	#[inline]
	pub fn try_from_slice_mut(
		slice: &mut [T],
	) -> Result<&mut Self, BitSpanError<T>> {
		let elts = slice.len();
		if elts >= Self::MAX_ELTS {
			elts.saturating_mul(mem::bits_of::<T::Mem>())
				.pipe(BitSpanError::TooLong)
				.pipe(Err)
		}
		else {
			Ok(unsafe { Self::from_slice_unchecked_mut(slice) })
		}
	}

	/// Constructs a shared `&BitSlice` over an element slice, without checking
	/// its length.
	///
	/// If `slice` is too long to encode into a `&BitSlice`, then the produced
	/// bit-slice’s length is unspecified.
	///
	/// ## Safety
	///
	/// You must ensure that `slice.len() < BitSlice::MAX_ELTS`.
	///
	/// Calling this function with an over-long slice is **library-level**
	/// undefined behavior. You may not assume anything about its implementation
	/// or behavior, and must conservatively assume that over-long slices cause
	/// compiler UB.
	#[inline]
	pub unsafe fn from_slice_unchecked(slice: &[T]) -> &Self {
		let bits = slice.len().wrapping_mul(mem::bits_of::<T::Mem>());
		BitPtr::from_slice(slice)
			.span_unchecked(bits)
			.into_bitslice_ref()
	}

	/// Constructs an exclusive `&mut BitSlice` over an element slice, without
	/// checking its length.
	///
	/// If `slice` is too long to encode into a `&mut BitSlice`, then the
	/// produced bit-slice’s length is unspecified.
	///
	/// ## Safety
	///
	/// You must ensure that `slice.len() < BitSlice::MAX_ELTS`.
	///
	/// Calling this function with an over-long slice is **library-level**
	/// undefined behavior. You may not assume anything about its implementation
	/// or behavior, and must conservatively assume that over-long slices cause
	/// compiler UB.
	#[inline]
	pub unsafe fn from_slice_unchecked_mut(slice: &mut [T]) -> &mut Self {
		let bits = slice.len().wrapping_mul(mem::bits_of::<T::Mem>());
		BitPtr::from_slice_mut(slice)
			.span_unchecked(bits)
			.into_bitslice_mut()
	}
}

/// Alternates of standard APIs.
impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Gets a raw pointer to the zeroth bit of the bit-slice.
	///
	/// ## Original
	///
	/// [`slice::as_ptr`](https://doc.rust-lang.org/std/primitive.slice.html#method.as_ptr)
	///
	/// ## API Differences
	///
	/// This is renamed in order to indicate that it is returning a `bitvec`
	/// structure, not a raw pointer.
	#[inline]
	pub fn as_bitptr(&self) -> BitPtr<Const, T, O> {
		self.as_bitspan().to_bitptr()
	}

	/// Gets a raw, write-capable pointer to the zeroth bit of the bit-slice.
	///
	/// ## Original
	///
	/// [`slice::as_mut_ptr`](https://doc.rust-lang.org/std/primitive.slice.html#method.as_mut_ptr)
	///
	/// ## API Differences
	///
	/// This is renamed in order to indicate that it is returning a `bitvec`
	/// structure, not a raw pointer.
	#[inline]
	pub fn as_mut_bitptr(&mut self) -> BitPtr<Mut, T, O> {
		self.as_mut_bitspan().to_bitptr()
	}

	/// Views the bit-slice as a half-open range of bit-pointers, to its first
	/// bit *in* the bit-slice and first bit *beyond* it.
	///
	/// ## Original
	///
	/// [`slice::as_ptr_range`](https://doc.rust-lang.org/std/primitive.slice.html#method.as_ptr_range)
	///
	/// ## API Differences
	///
	/// This is renamed to indicate that it returns a `bitvec` structure, rather
	/// than an ordinary `Range`.
	///
	/// ## Notes
	///
	/// `BitSlice` does define a [`.as_ptr_range()`], which returns a
	/// `Range<BitPtr>`. `BitPtrRange` has additional capabilities that
	/// `Range<*const T>` and `Range<BitPtr>` do not.
	///
	/// [`.as_ptr_range()`]: Self::as_ptr_range
	#[inline]
	pub fn as_bitptr_range(&self) -> BitPtrRange<Const, T, O> {
		self.as_bitspan().to_bitptr_range()
	}

	/// Views the bit-slice as a half-open range of write-capable bit-pointers,
	/// to its first bit *in* the bit-slice and the first bit *beyond* it.
	///
	/// ## Original
	///
	/// [`slice::as_mut_ptr_range`](https://doc.rust-lang.org/std/primitive.slice.html#method.as_mut_ptr_range)
	///
	/// ## API Differences
	///
	/// This is renamed to indicate that it returns a `bitvec` structure, rather
	/// than an ordinary `Range`.
	///
	/// ## Notes
	///
	/// `BitSlice` does define a [`.as_mut_ptr_range()`], which returns a
	/// `Range<BitPtr>`. `BitPtrRange` has additional capabilities that
	/// `Range<*mut T>` and `Range<BitPtr>` do not.
	#[inline]
	pub fn as_mut_bitptr_range(&mut self) -> BitPtrRange<Mut, T, O> {
		self.as_mut_bitspan().to_bitptr_range()
	}

	/// Copies the bits from `src` into `self`.
	///
	/// `self` and `src` must have the same length.
	///
	/// ## Performance
	///
	/// If `src` has the same type arguments as `self`, it will use the same
	/// implementation as [`.copy_from_bitslice()`]; if you know that this will
	/// always be the case, you should prefer to use that method directly.
	///
	/// Only `.copy_from_bitslice()` is *able* to perform acceleration; this
	/// method is *always* required to perform a bit-by-bit crawl over both
	/// bit-slices.
	///
	/// ## Original
	///
	/// [`slice::clone_from_slice`](https://doc.rust-lang.org/std/primitive.slice.html#method.clone_from_slice)
	///
	/// ## API Differences
	///
	/// This is renamed to reflect that it copies from another bit-slice, not
	/// from an element slice.
	///
	/// In order to support general usage, it allows `src` to have different
	/// type parameters than `self`, at the cost of performance optimizations.
	///
	/// ## Panics
	///
	/// This panics if the two bit-slices have different lengths.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	/// ```
	///
	/// [`.copy_from_bitslice()`]: Self::copy_from_bitslice
	#[inline]
	pub fn clone_from_bitslice<T2, O2>(&mut self, src: &BitSlice<T2, O2>)
	where
		T2: BitStore,
		O2: BitOrder,
	{
		assert_eq!(
			self.len(),
			src.len(),
			"cloning between bit-slices requires equal lengths",
		);

		if let Some(that) = src.coerce::<T, O>() {
			self.copy_from_bitslice(that);
		}
		//  TODO(myrrlyn): Test if `<T::Mem, O>` matches `<T2::Mem, O>` and
		//  specialize cloning.
		else {
			for (to, bit) in self.as_mut_bitptr_range().zip(src.iter().by_vals())
			{
				unsafe {
					to.write(bit);
				}
			}
		}
	}

	/// Copies all bits from `src` into `self`, using batched acceleration when
	/// possible.
	///
	/// `self` and `src` must have the same length.
	///
	/// ## Original
	///
	/// [`slice::copy_from_slice`](https://doc.rust-lang.org/std/primitive.slice.html#method.copy_from_slice)
	///
	/// ## Panics
	///
	/// This panics if the two bit-slices have different lengths.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	/// ```
	#[inline]
	pub fn copy_from_bitslice(&mut self, src: &Self) {
		assert_eq!(
			self.len(),
			src.len(),
			"copying between bit-slices requires equal lengths",
		);

		let (to_head, from_head) =
			(self.as_bitspan().head(), src.as_bitspan().head());
		if to_head == from_head {
			match (self.domain_mut(), src.domain()) {
				(Domain::Enclave(mut to), Domain::Enclave(from)) => {
					to.store_value(from.load_value());
				},
				(
					Domain::Region {
						head: to_head,
						body: to_body,
						tail: to_tail,
					},
					Domain::Region {
						head: from_head,
						body: from_body,
						tail: from_tail,
					},
				) => {
					if let (Some(mut to), Some(from)) = (to_head, from_head) {
						to.store_value(from.load_value());
					}
					for (to, from) in to_body.iter_mut().zip(from_body) {
						to.store_value(from.load_value());
					}
					if let (Some(mut to), Some(from)) = (to_tail, from_tail) {
						to.store_value(from.load_value());
					}
				},
				_ => unreachable!(
					"bit-slices with equal type parameters, lengths, and heads \
					 will always have equal domains"
				),
			}
		}
		if let (Some(this), Some(that)) =
			(self.coerce_mut::<T, Lsb0>(), src.coerce::<T, Lsb0>())
		{
			return this.sp_copy_from_bitslice(that);
		}
		if let (Some(this), Some(that)) =
			(self.coerce_mut::<T, Msb0>(), src.coerce::<T, Msb0>())
		{
			return this.sp_copy_from_bitslice(that);
		}
		for (to, bit) in self.as_mut_bitptr_range().zip(src.iter().by_vals()) {
			unsafe {
				to.write(bit);
			}
		}
	}

	/// Swaps the contents of two bit-slices.
	///
	/// `self` and `other` must have the same length.
	///
	/// ## Original
	///
	/// [`slice::swap_with_slice`](https://doc.rust-lang.org/std/primitive.slice.html#method.swap_with_slice)
	///
	/// ## API Differences
	///
	/// This method is renamed, as it takes a bit-slice rather than an element
	/// slice.
	///
	/// ## Panics
	///
	/// This panics if the two bit-slices have different lengths.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut one = [0xA5u8, 0x69];
	/// let mut two = 0x1234u16;
	/// let one_bits = one.view_bits_mut::<Msb0>();
	/// let two_bits = two.view_bits_mut::<Lsb0>();
	///
	/// one_bits.swap_with_bitslice(two_bits);
	///
	/// assert_eq!(one, [0x2C, 0x48]);
	/// # if cfg!(target_endian = "little") {
	/// assert_eq!(two, 0x96A5);
	/// # }
	/// ```
	#[inline]
	pub fn swap_with_bitslice<T2, O2>(&mut self, other: &mut BitSlice<T2, O2>)
	where
		T2: BitStore,
		O2: BitOrder,
	{
		assert_eq!(
			self.len(),
			other.len(),
			"swapping between bit-slices requires equal lengths",
		);

		if let (Some(this), Some(that)) =
			(self.coerce_mut::<T, Lsb0>(), other.coerce_mut::<T, Lsb0>())
		{
			return this.sp_swap_with_bitslice(that);
		}
		if let (Some(this), Some(that)) =
			(self.coerce_mut::<T, Msb0>(), other.coerce_mut::<T, Msb0>())
		{
			return this.sp_swap_with_bitslice(that);
		}
		self.as_mut_bitptr_range()
			.zip(other.as_mut_bitptr_range())
			.for_each(|(a, b)| unsafe {
				bv_ptr::swap(a, b);
			});
	}
}

/// Extensions of standard APIs.
impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Writes a new value into a single bit.
	///
	/// This is the replacement for `*slice[index] = value;`, as `bitvec` is not
	/// able to express that under the current `IndexMut` API signature.
	///
	/// ## Parameters
	///
	/// - `&mut self`
	/// - `index`: The bit-index to set. It must be in `0 .. self.len()`.
	/// - `value`: The new bit-value to write into the bit at `index`.
	///
	/// ## Panics
	///
	/// This panics if `index` is out of bounds.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 1];
	/// bits.set(0, true);
	/// bits.set(1, false);
	///
	/// assert_eq!(bits, bits![1, 0]);
	/// ```
	#[inline]
	pub fn set(&mut self, index: usize, value: bool) {
		self.replace(index, value);
	}

	/// Writes a new value into a single bit, without bounds checking.
	///
	/// ## Parameters
	///
	/// - `&mut self`
	/// - `index`: The bit-index to set. It must be in `0 .. self.len()`.
	/// - `value`: The new bit-value to write into the bit at `index`.
	///
	/// ## Safety
	///
	/// You must ensure that `index` is in the range `0 .. self.len()`.
	///
	/// This performs bit-pointer offset arithmetic without doing any bounds
	/// checks. If `index` is out of bounds, then this will issue an
	/// out-of-bounds access and will trigger memory unsafety.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut data = 0u8;
	/// let bits = &mut data.view_bits_mut::<Lsb0>()[.. 2];
	/// assert_eq!(bits.len(), 2);
	/// unsafe {
	///   bits.set_unchecked(3, true);
	/// }
	/// assert_eq!(data, 8);
	/// ```
	#[inline]
	pub unsafe fn set_unchecked(&mut self, index: usize, value: bool) {
		self.replace_unchecked(index, value);
	}

	/// Writes a new value into a bit, and returns its previous value.
	///
	/// ## Panics
	///
	/// This panics if `index` is not less than `self.len()`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0];
	/// assert!(!bits.replace(0, true));
	/// assert!(bits[0]);
	/// ```
	#[inline]
	pub fn replace(&mut self, index: usize, value: bool) -> bool {
		self.assert_in_bounds(index, 0 .. self.len());
		unsafe { self.replace_unchecked(index, value) }
	}

	/// Writes a new value into a bit, returning the previous value, without
	/// bounds checking.
	///
	/// ## Safety
	///
	/// `index` must be less than `self.len()`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 0, 0];
	/// let old = unsafe {
	///   let a = &mut bits[.. 1];
	///   a.replace_unchecked(1, true)
	/// };
	/// assert!(!old);
	/// assert!(bits[1]);
	/// ```
	#[inline]
	pub unsafe fn replace_unchecked(
		&mut self,
		index: usize,
		value: bool,
	) -> bool {
		self.as_mut_bitptr().add(index).replace(value)
	}

	/// Swaps two bits in a bit-slice, without bounds checking.
	///
	/// See [`.swap()`] for documentation.
	///
	/// ## Safety
	///
	/// You must ensure that `a` and `b` are both in the range `0 ..
	/// self.len()`.
	///
	/// This method performs bit-pointer offset arithmetic without doing any
	/// bounds checks. If `a` or `b` are out of bounds, then this will issue an
	/// out-of-bounds access and will trigger memory unsafety.
	///
	/// [`.swap()`]: Self::swap
	#[inline]
	pub unsafe fn swap_unchecked(&mut self, a: usize, b: usize) {
		let a = self.as_mut_bitptr().add(a);
		let b = self.as_mut_bitptr().add(b);
		bv_ptr::swap(a, b);
	}

	/// Splits a bit-slice at an index, without bounds checking.
	///
	/// See [`.split_at()`] for documentation.
	///
	/// ## Safety
	///
	/// You must ensure that `mid` is in the range `0 ..= self.len()`.
	///
	/// This method produces new bit-slice references. If `mid` is out of
	/// bounds, its behavior is **library-level** undefined. You must
	/// conservatively assume that an out-of-bounds split point produces
	/// compiler-level UB.
	///
	/// [`.split_at()`]: Self::split_at
	#[inline]
	pub unsafe fn split_at_unchecked(&self, mid: usize) -> (&Self, &Self) {
		let len = self.len();
		let left = self.as_bitptr();
		let right = left.add(mid);
		let left = left.span_unchecked(mid);
		let right = right.span_unchecked(len - mid);
		let left = left.into_bitslice_ref();
		let right = right.into_bitslice_ref();
		(left, right)
	}

	/// Splits a mutable bit-slice at an index, without bounds checking.
	///
	/// See [`.split_at_mut()`] for documentation.
	///
	/// ## Safety
	///
	/// You must ensure that `mid` is in the range `0 ..= self.len()`.
	///
	/// This method produces new bit-slice references. If `mid` is out of
	/// bounds, its behavior is **library-level** undefined. You must
	/// conservatively assume that an out-of-bounds split point produces
	/// compiler-level UB.
	///
	/// [`.split_at_mut()`]: Self::split_at_mut
	#[inline]
	pub unsafe fn split_at_unchecked_mut(
		&mut self,
		mid: usize,
	) -> (&mut BitSlice<T::Alias, O>, &mut BitSlice<T::Alias, O>) {
		let len = self.len();
		let left = self.alias_mut().as_mut_bitptr();
		let right = left.add(mid);
		(
			left.span_unchecked(mid).into_bitslice_mut(),
			right.span_unchecked(len - mid).into_bitslice_mut(),
		)
	}

	/// Copies bits from one region of the bit-slice to another region of
	/// itself, without doing bounds checks.
	///
	/// The regions are allowed to overlap.
	///
	/// ## Parameters
	///
	/// - `&mut self`
	/// - `src`: The range within `self` from which to copy.
	/// - `dst`: The starting index within `self` at which to paste.
	///
	/// ## Effects
	///
	/// `self[src]` is copied to `self[dest .. dest + src.len()]`. The bits of
	/// `self[src]` are in an unspecified, but initialized, state.
	///
	/// ## Safety
	///
	/// `src.end()` and `dest + src.len()` must be entirely within bounds.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut data = 0b1011_0000u8;
	/// let bits = data.view_bits_mut::<Msb0>();
	///
	/// unsafe {
	///   bits.copy_within_unchecked(.. 4, 2);
	/// }
	/// assert_eq!(data, 0b1010_1100);
	/// ```
	#[inline]
	pub unsafe fn copy_within_unchecked<R>(&mut self, src: R, dest: usize)
	where R: RangeExt<usize> {
		if let Some(this) = self.coerce_mut::<T, Lsb0>() {
			return this.sp_copy_within_unchecked(src, dest);
		}
		if let Some(this) = self.coerce_mut::<T, Msb0>() {
			return this.sp_copy_within_unchecked(src, dest);
		}
		let source = src.normalize(0, self.len());
		let source_len = source.len();
		let rev = source.contains(&dest);
		let dest = dest .. dest + source_len;
		for (from, to) in self
			.get_unchecked(source)
			.as_bitptr_range()
			.zip(self.get_unchecked_mut(dest).as_mut_bitptr_range())
			.bidi(rev)
		{
			to.write(from.read());
		}
	}

	#[inline]
	#[doc(hidden)]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.iter_mut().enumerate()`"]
	pub fn for_each(&mut self, mut func: impl FnMut(usize, bool) -> bool) {
		for (idx, ptr) in self.as_mut_bitptr_range().enumerate() {
			unsafe {
				ptr.write(func(idx, ptr.read()));
			}
		}
	}
}

/// Views of underlying memory.
impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Partitions a bit-slice into maybe-contended and known-uncontended parts.
	///
	/// The documentation of `BitDomain` goes into this in more detail. In
	/// short, this produces a `&BitSlice` that is as large as possible without
	/// requiring alias protection, as well as any bits that were not able to be
	/// included in the unaliased bit-slice.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn bit_domain(&self) -> BitDomain<Const, T, O> {
		self.domain().into_bit_domain()
	}

	/// Partitions a mutable bit-slice into maybe-contended and
	/// known-uncontended parts.
	///
	/// The documentation of `BitDomain` goes into this in more detail. In
	/// short, this produces a `&mut BitSlice` that is as large as possible
	/// without requiring alias protection, as well as any bits that were not
	/// able to be included in the unaliased bit-slice.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn bit_domain_mut(&mut self) -> BitDomain<Mut, T, O> {
		self.domain_mut().into_bit_domain()
	}

	/// Views the underlying memory of a bit-slice, removing alias protections
	/// where possible.
	///
	/// The documentation of `Domain` goes into this in more detail. In short,
	/// this produces a `&[T]` slice with alias protections removed, covering
	/// all elements that `self` completely fills. Partially-used elements on
	/// either the front or back edge of the slice are returned separately.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn domain(&self) -> Domain<Const, T, O> {
		Domain::new(self)
	}

	/// Views the underlying memory of a bit-slice, removing alias protections
	/// where possible.
	///
	/// The documentation of `Domain` goes into this in more detail. In short,
	/// this produces a `&mut [T]` slice with alias protections removed,
	/// covering all elements that `self` completely fills. Partially-used
	/// elements on the front or back edge of the slice are returned separately.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn domain_mut(&mut self) -> Domain<Mut, T, O> {
		Domain::new(self)
	}
}

/// Bit-value queries.
impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Counts the number of bits set to `1` in the bit-slice contents.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![1, 1, 0, 0];
	/// assert_eq!(bits[.. 2].count_ones(), 2);
	/// assert_eq!(bits[2 ..].count_ones(), 0);
	/// assert_eq!(bits![].count_ones(), 0);
	/// ```
	#[inline]
	pub fn count_ones(&self) -> usize {
		match self.domain() {
			Domain::Enclave(elem) => elem.load_value().count_ones() as usize,
			Domain::Region { head, body, tail } => {
				head.map_or(0, |elem| elem.load_value().count_ones() as usize)
					+ body
						.iter()
						.map(BitStore::load_value)
						.map(|elem| elem.count_ones() as usize)
						.sum::<usize>() + tail
					.map_or(0, |elem| elem.load_value().count_ones() as usize)
			},
		}
	}

	/// Counts the number of bits cleared to `0` in the bit-slice contents.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![1, 1, 0, 0];
	/// assert_eq!(bits[.. 2].count_zeros(), 0);
	/// assert_eq!(bits[2 ..].count_zeros(), 2);
	/// assert_eq!(bits![].count_zeros(), 0);
	/// ```
	#[inline]
	pub fn count_zeros(&self) -> usize {
		match self.domain() {
			Domain::Enclave(elem) => (elem.load_value()
				| !elem.mask().into_inner())
			.count_zeros() as usize,
			Domain::Region { head, body, tail } => {
				head.map_or(0, |elem| {
					(elem.load_value() | !elem.mask().into_inner()).count_zeros()
						as usize
				}) + body
					.iter()
					.map(BitStore::load_value)
					.map(|elem| elem.count_zeros() as usize)
					.sum::<usize>() + tail.map_or(0, |elem| {
					(elem.load_value() | !elem.mask().into_inner()).count_zeros()
						as usize
				})
			},
		}
	}

	/// Enumerates the index of each bit in a bit-slice set to `1`.
	///
	/// This is a shorthand for a `.enumerate().filter_map()` iterator that
	/// selects the index of each `true` bit; however, its implementation is
	/// eligible for optimizations that the individual-bit iterator is not.
	///
	/// Specializations for the `Lsb0` and `Msb0` orderings allow processors
	/// with instructions that seek particular bits within an element to operate
	/// on whole elements, rather than on each bit individually.
	///
	/// ## Examples
	///
	/// This example uses `.iter_ones()`, a `.filter_map()` that finds the index
	/// of each set bit, and the known indices, in order to show that they have
	/// equivalent behavior.
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0, 0, 1, 0, 0, 0, 1];
	///
	/// let iter_ones = bits.iter_ones();
	/// let known_indices = [1, 4, 8].iter().copied();
	/// let filter = bits.iter()
	///   .by_vals()
	///   .enumerate()
	///   .filter_map(|(idx, bit)| if bit { Some(idx) } else { None });
	/// let all = iter_ones.zip(known_indices).zip(filter);
	///
	/// for ((iter_one, known), filtered) in all {
	///   assert_eq!(iter_one, known);
	///   assert_eq!(known, filtered);
	/// }
	/// ```
	#[inline]
	pub fn iter_ones(&self) -> IterOnes<T, O> {
		IterOnes::new(self)
	}

	/// Enumerates the index of each bit in a bit-slice cleared to `0`.
	///
	/// This is a shorthand for a `.enumerate().filter_map()` iterator that
	/// selects the index of each `false` bit; however, its implementation is
	/// eligible for optimizations that the individual-bit iterator is not.
	///
	/// Specializations for the `Lsb0` and `Msb0` orderings allow processors
	/// with instructions that seek particular bits within an element to operate
	/// on whole elements, rather than on each bit individually.
	///
	/// ## Examples
	///
	/// This example uses `.iter_zeros()`, a `.filter_map()` that finds the
	/// index of each cleared bit, and the known indices, in order to show that
	/// they have equivalent behavior.
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![1, 0, 1, 1, 0, 1, 1, 1, 0];
	///
	/// let iter_zeros = bits.iter_zeros();
	/// let known_indices = [1, 4, 8].iter().copied();
	/// let filter = bits.iter()
	///   .by_vals()
	///   .enumerate()
	///   .filter_map(|(idx, bit)| if !bit { Some(idx) } else { None });
	/// let all = iter_zeros.zip(known_indices).zip(filter);
	///
	/// for ((iter_zero, known), filtered) in all {
	///   assert_eq!(iter_zero, known);
	///   assert_eq!(known, filtered);
	/// }
	/// ```
	#[inline]
	pub fn iter_zeros(&self) -> IterZeros<T, O> {
		IterZeros::new(self)
	}

	/// Finds the index of the first bit in the bit-slice set to `1`.
	///
	/// Returns `None` if there is no `true` bit in the bit-slice.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!(bits![].first_one().is_none());
	/// assert!(bits![0].first_one().is_none());
	/// assert_eq!(bits![0, 1].first_one(), Some(1));
	/// ```
	#[inline]
	pub fn first_one(&self) -> Option<usize> {
		self.iter_ones().next()
	}

	/// Finds the index of the first bit in the bit-slice cleared to `0`.
	///
	/// Returns `None` if there is no `false` bit in the bit-slice.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!(bits![].first_zero().is_none());
	/// assert!(bits![1].first_zero().is_none());
	/// assert_eq!(bits![1, 0].first_zero(), Some(1));
	/// ```
	#[inline]
	pub fn first_zero(&self) -> Option<usize> {
		self.iter_zeros().next()
	}

	/// Finds the index of the last bit in the bit-slice set to `1`.
	///
	/// Returns `None` if there is no `true` bit in the bit-slice.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!(bits![].last_one().is_none());
	/// assert!(bits![0].last_one().is_none());
	/// assert_eq!(bits![1, 0].last_one(), Some(0));
	/// ```
	#[inline]
	pub fn last_one(&self) -> Option<usize> {
		self.iter_ones().next_back()
	}

	/// Finds the index of the last bit in the bit-slice cleared to `0`.
	///
	/// Returns `None` if there is no `false` bit in the bit-slice.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!(bits![].last_zero().is_none());
	/// assert!(bits![1].last_zero().is_none());
	/// assert_eq!(bits![0, 1].last_zero(), Some(0));
	/// ```
	#[inline]
	pub fn last_zero(&self) -> Option<usize> {
		self.iter_zeros().next_back()
	}

	/// Counts the number of bits from the start of the bit-slice to the first
	/// bit set to `0`.
	///
	/// This returns `0` if the bit-slice is empty.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert_eq!(bits![].leading_ones(), 0);
	/// assert_eq!(bits![0].leading_ones(), 0);
	/// assert_eq!(bits![1, 0].leading_ones(), 1);
	/// ```
	#[inline]
	pub fn leading_ones(&self) -> usize {
		self.first_zero().unwrap_or_else(|| self.len())
	}

	/// Counts the number of bits from the start of the bit-slice to the first
	/// bit set to `1`.
	///
	/// This returns `0` if the bit-slice is empty.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert_eq!(bits![].leading_zeros(), 0);
	/// assert_eq!(bits![1].leading_zeros(), 0);
	/// assert_eq!(bits![0, 1].leading_zeros(), 1);
	/// ```
	#[inline]
	pub fn leading_zeros(&self) -> usize {
		self.first_one().unwrap_or_else(|| self.len())
	}

	/// Counts the number of bits from the end of the bit-slice to the last bit
	/// set to `0`.
	///
	/// This returns `0` if the bit-slice is empty.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert_eq!(bits![].trailing_ones(), 0);
	/// assert_eq!(bits![0].trailing_ones(), 0);
	/// assert_eq!(bits![0, 1].trailing_ones(), 1);
	/// ```
	#[inline]
	pub fn trailing_ones(&self) -> usize {
		let len = self.len();
		self.last_zero().map(|idx| len - 1 - idx).unwrap_or(len)
	}

	/// Counts the number of bits from the end of the bit-slice to the last bit
	/// set to `1`.
	///
	/// This returns `0` if the bit-slice is empty.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert_eq!(bits![].trailing_zeros(), 0);
	/// assert_eq!(bits![1].trailing_zeros(), 0);
	/// assert_eq!(bits![1, 0].trailing_zeros(), 1);
	/// ```
	#[inline]
	pub fn trailing_zeros(&self) -> usize {
		let len = self.len();
		self.last_one().map(|idx| len - 1 - idx).unwrap_or(len)
	}

	/// Tests if there is at least one bit set to `1` in the bit-slice.
	///
	/// Returns `false` when `self` is empty.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!(!bits![].any());
	/// assert!(!bits![0].any());
	/// assert!(bits![0, 1].any());
	/// ```
	#[inline]
	pub fn any(&self) -> bool {
		self.count_ones() > 0
	}

	/// Tests if every bit is set to `1` in the bit-slice.
	///
	/// Returns `true` when `self` is empty.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!( bits![].all());
	/// assert!(!bits![0].all());
	/// assert!( bits![1].all());
	/// ```
	#[inline]
	pub fn all(&self) -> bool {
		self.count_zeros() == 0
	}

	/// Tests if every bit is cleared to `0` in the bit-slice.
	///
	/// Returns `true` when `self` is empty.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!( bits![].not_any());
	/// assert!(!bits![1].not_any());
	/// assert!( bits![0].not_any());
	/// ```
	#[inline]
	pub fn not_any(&self) -> bool {
		self.count_ones() == 0
	}

	/// Tests if at least one bit is cleared to `0` in the bit-slice.
	///
	/// Returns `false` when `self` is empty.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!(!bits![].not_all());
	/// assert!(!bits![1].not_all());
	/// assert!( bits![0].not_all());
	/// ```
	#[inline]
	pub fn not_all(&self) -> bool {
		self.count_zeros() > 0
	}

	/// Tests if at least one bit is set to `1`, and at least one bit is cleared
	/// to `0`, in the bit-slice.
	///
	/// Returns `false` when `self` is empty.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// assert!(!bits![].some());
	/// assert!(!bits![0].some());
	/// assert!(!bits![1].some());
	/// assert!( bits![0, 1].some());
	/// ```
	#[inline]
	pub fn some(&self) -> bool {
		self.any() && self.not_all()
	}
}

/// Buffer manipulation.
impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Shifts the contents of a bit-slice “left” (towards the zero-index),
	/// clearing the “right” bits to `0`.
	///
	/// This is a strictly-worse analogue to taking `bits = &bits[by ..]`: it
	/// has to modify the entire memory region that `bits` governs, and destroys
	/// contained information. Unless the actual memory layout and contents of
	/// your bit-slice matters to your program, you should *probably* prefer to
	/// munch your way forward through a bit-slice handle.
	///
	/// Note also that the “left” here is semantic only, and **does not**
	/// necessarily correspond to a left-shift instruction applied to the
	/// underlying integer storage.
	///
	/// This has no effect when `by` is `0`. When `by` is `self.len()`, the
	/// bit-slice is entirely cleared to `0`.
	///
	/// ## Panics
	///
	/// This panics if `by` is not less than `self.len()`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 1];
	/// // these bits are retained ^--------------------------^
	/// bits.shift_left(2);
	/// assert_eq!(bits, bits![1, 1, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0]);
	/// // and move here       ^--------------------------^
	///
	/// let bits = bits![mut 1; 2];
	/// bits.shift_left(2);
	/// assert_eq!(bits, bits![0; 2]);
	/// ```
	#[inline]
	pub fn shift_left(&mut self, by: usize) {
		if by == 0 {
			return;
		}

		let len = self.len();
		if by == len {
			return self.fill(false);
		}
		assert!(
			by <= len,
			"shift must be less than the length of the bit-slice: {} >= {}",
			by,
			len,
		);

		unsafe {
			self.copy_within_unchecked(by .., 0);
			self.get_unchecked_mut(len - by ..).fill(false);
		}
	}

	/// Shifts the contents of a bit-slice “right” (away from the zero-index),
	/// clearing the “left” bits to `0`.
	///
	/// This is a strictly-worse analogue to taking `bits = &bits[.. bits.len()
	/// - by]`: it must modify the entire memory region that `bits` governs, and
	/// destroys contained information. Unless the actual memory layout and
	/// contents of your bit-slice matters to your program, you should
	/// *probably* prefer to munch your way backward through a bit-slice handle.
	///
	/// Note also that the “right” here is semantic only, and **does not**
	/// necessarily correspond to a right-shift instruction applied to the
	/// underlying integer storage.
	///
	/// This has no effect when `by` is `0`. When `by` is `self.len()`, the
	/// bit-slice is entirely cleared to `0`.
	///
	/// ## Panics
	///
	/// This panics if `by` is not less than `self.len()`.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![mut 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 1];
	/// // these bits stay   ^--------------------------^
	/// bits.shift_right(2);
	/// assert_eq!(bits, bits![0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1]);
	/// // and move here             ^--------------------------^
	///
	/// let bits = bits![mut 1; 2];
	/// bits.shift_right(2);
	/// assert_eq!(bits, bits![0; 2]);
	/// ```
	#[inline]
	pub fn shift_right(&mut self, by: usize) {
		if by == 0 {
			return;
		}

		let len = self.len();
		if by == len {
			return self.fill(false);
		}
		assert!(
			by <= len,
			"shift must be less than the length of the bit-slice: {} >= {}",
			by,
			len,
		);

		unsafe {
			self.copy_within_unchecked(.. len - by, by);
			self.get_unchecked_mut(.. by).fill(false);
		}
	}
}

/// Crate internals.
impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Gets the structural form of the encoded reference.
	pub(crate) fn as_bitspan(&self) -> BitSpan<Const, T, O> {
		BitSpan::from_bitslice_ptr(self)
	}

	/// Gets the structural form of the encoded reference.
	pub(crate) fn as_mut_bitspan(&mut self) -> BitSpan<Mut, T, O> {
		BitSpan::from_bitslice_ptr_mut(self)
	}

	/// Asserts that `index` is within the given bounds.
	///
	/// ## Parameters
	///
	/// - `&self`
	/// - `index`: The bit index to test against the bit-slice.
	/// - `bounds`: The bounds to check. cannot exceed `0 ..= self.len()`.
	///
	/// ## Panics
	///
	/// This panics if `bounds` is outside `index`.
	pub(crate) fn assert_in_bounds<R>(&self, index: usize, bounds: R)
	where R: RangeExt<usize> {
		let bounds = bounds.normalize(0, self.len());
		assert!(
			bounds.contains(&index),
			"index {} out of range: {:?}",
			index,
			bounds.end_bound()
		);
	}

	/// Marks an exclusive bit-slice as covering an aliased memory region.
	pub(crate) fn alias_mut(&mut self) -> &mut BitSlice<T::Alias, O> {
		unsafe { self.as_mut_bitspan().cast::<T::Alias>().into_bitslice_mut() }
	}

	/// Removes an aliasing marker from an exclusive bit-slice handle.
	///
	/// ## Safety
	///
	/// This may only be used when the bit-slice is either known to be
	/// unaliased, or this call is combined with an operation that adds an
	/// aliasing marker and the total number of aliasing markers remains
	/// unchanged.
	pub(crate) unsafe fn unalias_mut(
		this: &mut BitSlice<T::Alias, O>,
	) -> &mut Self {
		this.as_mut_bitspan().cast::<T>().into_bitslice_mut()
	}

	/// Splits a mutable bit-slice at a midpoint, without either doing bounds
	/// checks or adding an alias marker to the returned sections.
	///
	/// This method has the same behavior as [`.split_at_unchecked_mut()`],
	/// except that it does not apply an aliasing marker to the partitioned
	/// subslices.
	///
	/// ## Safety
	///
	/// See `split_at_unchecked_mut`. Additionally, this is only safe when `T`
	/// is alias-safe.
	///
	/// [`.split_at_unchecked_mut()`]: Self::split_at_unchecked_mut
	pub(crate) unsafe fn split_at_unchecked_mut_noalias(
		&mut self,
		mid: usize,
	) -> (&mut Self, &mut Self) {
		//  Split the slice at the requested midpoint, adding an alias layer
		let (head, tail) = self.split_at_unchecked_mut(mid);
		//  Remove the new alias layer.
		(Self::unalias_mut(head), Self::unalias_mut(tail))
	}
}

/// Methods available only when `T` allows shared mutability.
impl<T, O> BitSlice<T, O>
where
	T: BitStore + radium::Radium,
	O: BitOrder,
{
	/// Writes a new value into a single bit, using alias-safe operations.
	///
	/// This is equivalent to [`.set()`], except that it does not require an
	/// `&mut` reference, and allows bit-slices with alias-safe storage to share
	/// write permissions.
	///
	/// ## Parameters
	///
	/// - `&self`: This method only exists on bit-slices with alias-safe
	///   storage, and so does not require exclusive access.
	/// - `index`: The bit index to set. It must be in `0 .. self.len()`.
	/// - `value`: The new bit-value to write into the bit at `index`.
	///
	/// ## Panics
	///
	/// This panics if `index` is out of bounds.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	/// use core::cell::Cell;
	///
	/// let bits: &BitSlice<_, _> = bits![Cell<usize>, Lsb0; 0, 1];
	/// bits.set_aliased(0, true);
	/// bits.set_aliased(1, false);
	///
	/// assert_eq!(bits, bits![1, 0]);
	/// ```
	///
	/// [`.set()`]: Self::set
	#[inline]
	pub fn set_aliased(&self, index: usize, value: bool) {
		self.assert_in_bounds(index, 0 .. self.len());
		unsafe {
			self.set_aliased_unchecked(index, value);
		}
	}

	/// Writes a new value into a single bit, using alias-safe operations and
	/// without bounds checking.
	///
	/// This is equivalent to [`.set_unchecked()`], except that it does not
	/// require an `&mut` reference, and allows bit-slices with alias-safe
	/// storage to share write permissions.
	///
	/// ## Parameters
	///
	/// - `&self`: This method only exists on bit-slices with alias-safe
	///   storage, and so does not require exclusive access.
	/// - `index`: The bit index to set. It must be in `0 .. self.len()`.
	/// - `value`: The new bit-value to write into the bit at `index`.
	///
	/// ## Safety
	///
	/// The caller must ensure that `index` is not out of bounds.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	/// use core::cell::Cell;
	///
	/// let data = Cell::new(0u8);
	/// let bits = &data.view_bits::<Lsb0>()[.. 2];
	/// unsafe {
	///   bits.set_aliased_unchecked(3, true);
	/// }
	/// assert_eq!(data.get(), 8);
	/// ```
	///
	/// [`.set_unchecked()`]: Self::set_unchecked
	#[inline]
	pub unsafe fn set_aliased_unchecked(&self, index: usize, value: bool) {
		self.as_bitptr().add(index).freeze().frozen_write_bit(value);
	}
}

/// Miscellaneous information.
impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// The inclusive maximum length of a `BitSlice<_, T>`.
	///
	/// As `BitSlice` is zero-indexed, the largest possible *index* is one less
	/// than this value.
	///
	/// |CPU word width|         Value         |
	/// |-------------:|----------------------:|
	/// |   32 bits    |     `0x1fff_ffff`     |
	/// |   64 bits    |`0x1fff_ffff_ffff_ffff`|
	pub const MAX_BITS: usize = BitSpan::<Const, T, O>::REGION_MAX_BITS;
	/// The inclusive maximum length that a `[T]` slice can be for
	///  `BitSlice<_, T>` to cover it.
	///
	/// A `BitSlice<_, T>` that begins in the interior of an element and
	/// contains the maximum number of bits will extend one element past the
	/// cutoff that would occur if the bit-slice began at the zeroth bit. Such a
	/// bit-slice is difficult to manually construct, but would not otherwise
	/// fail.
	///
	/// |Type Bits|Max Elements (32-bit)| Max Elements (64-bit) |
	/// |--------:|--------------------:|----------------------:|
	/// |        8|    `0x0400_0001`    |`0x0400_0000_0000_0001`|
	/// |       16|    `0x0200_0001`    |`0x0200_0000_0000_0001`|
	/// |       32|    `0x0100_0001`    |`0x0100_0000_0000_0001`|
	/// |       64|    `0x0080_0001`    |`0x0080_0000_0000_0001`|
	pub const MAX_ELTS: usize = BitSpan::<Const, T, O>::REGION_MAX_ELTS;
}

#[cfg(feature = "alloc")]
impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Copies a bit-slice into an owned bit-vector.
	///
	/// Since the new vector is freshly owned, this gets marked as `::Unalias`
	/// to remove any guards that may have been inserted by the bit-slice’s
	/// history.
	///
	/// It does *not* use the underlying memory type, so that a `BitSlice<_,
	/// Cell<_>>` will produce a `BitVec<_, Cell<_>>`.
	///
	/// ## Original
	///
	/// [`slice::to_vec`](https://doc.rust-lang.org/std/primitive.slice.html#method.to_vec)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0, 1];
	/// let bv = bits.to_bitvec();
	/// assert_eq!(bits, bv);
	/// ```
	#[inline]
	pub fn to_bitvec(&self) -> BitVec<T::Unalias, O> {
		self.domain()
			.map(<T::Unalias as BitStore>::new)
			.collect::<Vec<_>>()
			.pipe(BitVec::from_vec)
			.tap_mut(|bv| unsafe {
				bv.set_head(self.as_bitspan().head());
				bv.set_len(self.len());
			})
	}
}

#[inline]
#[doc = include_str!("../doc/slice/from_raw_parts_unchecked.md")]
pub unsafe fn from_raw_parts_unchecked<'a, T, O>(
	ptr: BitPtr<Const, T, O>,
	len: usize,
) -> &'a BitSlice<T, O>
where
	O: BitOrder,
	T: 'a + BitStore,
{
	ptr.span_unchecked(len).into_bitslice_ref()
}

#[inline]
#[doc = include_str!("../doc/slice/from_raw_parts_unchecked_mut.md")]
pub unsafe fn from_raw_parts_unchecked_mut<'a, T, O>(
	ptr: BitPtr<Mut, T, O>,
	len: usize,
) -> &'a mut BitSlice<T, O>
where
	O: BitOrder,
	T: 'a + BitStore,
{
	ptr.span_unchecked(len).into_bitslice_mut()
}
