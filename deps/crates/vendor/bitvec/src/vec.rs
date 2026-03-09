#![doc = include_str!("../doc/vec.md")]
#![cfg(feature = "alloc")]

#[cfg(not(feature = "std"))]
use alloc::vec;
use alloc::vec::Vec;
use core::{
	mem::{
		self,
		ManuallyDrop,
	},
	ptr,
	slice,
};

use tap::Pipe;
use wyz::comu::{
	Const,
	Mut,
};

pub use self::iter::{
	Drain,
	Splice,
};
pub use crate::boxed::IntoIter;
use crate::{
	boxed::BitBox,
	index::BitIdx,
	mem::bits_of,
	order::{
		BitOrder,
		Lsb0,
	},
	ptr::{
		AddressExt,
		BitPtr,
		BitSpan,
		BitSpanError,
	},
	slice::BitSlice,
	store::BitStore,
	view::BitView,
};

mod api;
mod iter;
mod ops;
mod tests;
mod traits;

#[repr(C)]
#[doc = include_str!("../doc/vec/BitVec.md")]
pub struct BitVec<T = usize, O = Lsb0>
where
	T: BitStore,
	O: BitOrder,
{
	/// Span description of the live bits in the allocation.
	bitspan:  BitSpan<Mut, T, O>,
	/// Allocation capacity, measured in `T` elements.
	capacity: usize,
}

/// Constructors.
impl<T, O> BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// An empty bit-vector with no backing allocation.
	pub const EMPTY: Self = Self {
		bitspan:  BitSpan::EMPTY,
		capacity: 0,
	};

	/// Creates a new bit-vector by repeating a bit for the desired length.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let zeros = BitVec::<u8, Msb0>::repeat(false, 50);
	/// let ones = BitVec::<u16, Lsb0>::repeat(true, 50);
	/// ```
	#[inline]
	pub fn repeat(bit: bool, len: usize) -> Self {
		let mut out = Self::with_capacity(len);
		unsafe {
			out.set_len(len);
			out.as_raw_mut_slice().fill_with(|| {
				BitStore::new(if bit { !<T::Mem>::ZERO } else { <T::Mem>::ZERO })
			});
		}
		out
	}

	/// Copies the contents of a bit-slice into a new heap allocation.
	///
	/// This copies the raw underlying elements into a new allocation, and sets
	/// the produced bit-vector to use the same memory layout as the originating
	/// bit-slice. This means that it may begin at any bit in the first element,
	/// not just the zeroth bit. If you require this property, call
	/// [`.force_align()`].
	///
	/// Dead bits in the copied memory elements are guaranteed to be zeroed.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0, 1, 0, 0, 1];
	/// let bv = BitVec::from_bitslice(bits);
	/// assert_eq!(bv, bits);
	/// ```
	///
	/// [`.force_align()`]: Self::force_align
	#[inline]
	pub fn from_bitslice(slice: &BitSlice<T, O>) -> Self {
		let bitspan = slice.as_bitspan();

		let mut vec = bitspan
			.elements()
			.pipe(Vec::with_capacity)
			.pipe(ManuallyDrop::new);
		vec.extend(slice.domain());

		let bitspan = unsafe {
			BitSpan::new_unchecked(
				vec.as_mut_ptr().cast::<T>().into_address(),
				bitspan.head(),
				bitspan.len(),
			)
		};
		let capacity = vec.capacity();
		Self { bitspan, capacity }
	}

	/// Constructs a new bit-vector from a single element.
	///
	/// This copies `elem` into a new heap allocation, and sets the bit-vector
	/// to cover it entirely.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bv = BitVec::<_, Msb0>::from_element(1u8);
	/// assert!(bv[7]);
	/// ```
	#[inline]
	pub fn from_element(elem: T) -> Self {
		Self::from_vec(vec![elem])
	}

	/// Constructs a new bit-vector from a slice of memory elements.
	///
	/// This copies `slice` into a new heap allocation, and sets the bit-vector
	/// to cover it entirely.
	///
	/// ## Panics
	///
	/// This panics if `slice` exceeds bit-vector capacity.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let slice = &[0u8, 1, 2, 3];
	/// let bv = BitVec::<_, Lsb0>::from_slice(slice);
	/// assert_eq!(bv.len(), 32);
	/// ```
	#[inline]
	pub fn from_slice(slice: &[T]) -> Self {
		Self::try_from_slice(slice).unwrap()
	}

	/// Fallibly constructs a new bit-vector from a slice of memory elements.
	///
	/// This fails early if `slice` exceeds bit-vector capacity. If it is not,
	/// then `slice` is copied into a new heap allocation and fully spanned by
	/// the returned bit-vector.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let slice = &[0u8, 1, 2, 3];
	/// let bv = BitVec::<_, Lsb0>::try_from_slice(slice).unwrap();
	/// assert_eq!(bv.len(), 32);
	/// ```
	#[inline]
	pub fn try_from_slice(slice: &[T]) -> Result<Self, BitSpanError<T>> {
		BitSlice::<T, O>::try_from_slice(slice).map(Self::from_bitslice)
	}

	/// Converts a regular vector in-place into a bit-vector.
	///
	/// The produced bit-vector spans every bit in the original vector. No
	/// reällocation occurs; this is purely a transform of the handle.
	///
	/// ## Panics
	///
	/// This panics if the source vector is too long to view as a bit-slice.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let v = vec![0u8, 1, 2, 3];
	/// let bv = BitVec::<_, Msb0>::from_vec(v);
	/// assert_eq!(bv.len(), 32);
	/// ```
	#[inline]
	pub fn from_vec(vec: Vec<T>) -> Self {
		Self::try_from_vec(vec)
			.expect("vector was too long to be converted into a `BitVec`")
	}

	/// Attempts to convert a regular vector in-place into a bit-vector.
	///
	/// This fails if the source vector is too long to view as a bit-slice. On
	/// success, the produced bit-vector spans every bit in the original vector.
	/// No reällocation occurs; this is purely a transform of the handle.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let v = vec![0u8; 20];
	/// assert_eq!(BitVec::<_, Msb0>::try_from_vec(v).unwrap().len(), 160);
	/// ```
	///
	/// It is not practical to allocate a vector that will fail this conversion.
	#[inline]
	pub fn try_from_vec(vec: Vec<T>) -> Result<Self, Vec<T>> {
		let mut vec = ManuallyDrop::new(vec);
		let capacity = vec.capacity();

		BitPtr::from_mut_slice(vec.as_mut_slice())
			.span(vec.len() * bits_of::<T::Mem>())
			.map(|bitspan| Self { bitspan, capacity })
			.map_err(|_| ManuallyDrop::into_inner(vec))
	}

	/// Appends the contents of a bit-slice to a bit-vector.
	///
	/// This can extend from a bit-slice of any type parameters; it is not
	/// restricted to using the same parameters as `self`. However, when the
	/// type parameters *do* match, it is possible for this to use a batch-copy
	/// optimization to go faster than the individual-bit crawl that is
	/// necessary when they differ.
	///
	/// Until Rust provides extensive support for specialization in trait
	/// implementations, you should use this method whenever you are extending
	/// from a `BitSlice` proper, and only use the general [`.extend()`]
	/// implementation if you are required to use a generic `bool` source.
	///
	/// ## Original
	///
	/// [`Vec::extend_from_slice`](alloc::vec::Vec::extend_from_slice)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![0, 1];
	/// bv.extend_from_bitslice(bits![0, 1, 0, 0, 1]);
	/// assert_eq!(bv, bits![0, 1, 0, 1, 0, 0, 1]);
	/// ```
	///
	/// [`.extend()`]: https://docs.rs/bitvec/latest/bitvec/vec/struct.Vec.html#impl-Extend
	#[inline]
	pub fn extend_from_bitslice<T2, O2>(&mut self, other: &BitSlice<T2, O2>)
	where
		T2: BitStore,
		O2: BitOrder,
	{
		let len = self.len();
		let olen = other.len();
		self.resize(len + olen, false);
		unsafe { self.get_unchecked_mut(len ..) }.clone_from_bitslice(other);
	}

	/// Appends a slice of `T` elements to a bit-vector.
	///
	/// The slice is viewed as a `BitSlice<T, O>`, then appended directly to the
	/// bit-vector.
	///
	/// ## Original
	///
	/// [`Vec::extend_from_slice`](alloc::vec::Vec::extend_from_slice)
	#[inline]
	pub fn extend_from_raw_slice(&mut self, slice: &[T]) {
		self.extend_from_bitslice(slice.view_bits::<O>());
	}
}

/// Converters.
impl<T, O> BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Explicitly views the bit-vector as a bit-slice.
	#[inline]
	pub fn as_bitslice(&self) -> &BitSlice<T, O> {
		unsafe { self.bitspan.into_bitslice_ref() }
	}

	/// Explicitly views the bit-vector as a mutable bit-slice.
	#[inline]
	pub fn as_mut_bitslice(&mut self) -> &mut BitSlice<T, O> {
		unsafe { self.bitspan.into_bitslice_mut() }
	}

	/// Views the bit-vector as a slice of its underlying memory elements.
	#[inline]
	pub fn as_raw_slice(&self) -> &[T] {
		let (data, len) =
			(self.bitspan.address().to_const(), self.bitspan.elements());
		unsafe { slice::from_raw_parts(data, len) }
	}

	/// Views the bit-vector as a mutable slice of its underlying memory
	/// elements.
	#[inline]
	pub fn as_raw_mut_slice(&mut self) -> &mut [T] {
		let (data, len) =
			(self.bitspan.address().to_mut(), self.bitspan.elements());
		unsafe { slice::from_raw_parts_mut(data, len) }
	}

	/// Creates an unsafe shared bit-pointer to the start of the buffer.
	///
	/// ## Original
	///
	/// [`Vec::as_ptr`](alloc::vec::Vec::as_ptr)
	///
	/// ## Safety
	///
	/// You must initialize the contents of the underlying buffer before
	/// accessing memory through this pointer. See the `BitPtr` documentation
	/// for more details.
	#[inline]
	pub fn as_bitptr(&self) -> BitPtr<Const, T, O> {
		self.bitspan.to_bitptr().to_const()
	}

	/// Creates an unsafe writable bit-pointer to the start of the buffer.
	///
	/// ## Original
	///
	/// [`Vec::as_mut_ptr`](alloc::vec::Vec::as_mut_ptr)
	///
	/// ## Safety
	///
	/// You must initialize the contents of the underlying buffer before
	/// accessing memory through this pointer. See the `BitPtr` documentation
	/// for more details.
	#[inline]
	pub fn as_mut_bitptr(&mut self) -> BitPtr<Mut, T, O> {
		self.bitspan.to_bitptr()
	}

	/// Converts a bit-vector into a boxed bit-slice.
	///
	/// This may cause a reällocation to drop any excess capacity.
	///
	/// ## Original
	///
	/// [`Vec::into_boxed_slice`](alloc::vec::Vec::into_boxed_slice)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bv = bitvec![0, 1, 0, 0, 1];
	/// let bb = bv.into_boxed_bitslice();
	/// ```
	#[inline]
	pub fn into_boxed_bitslice(self) -> BitBox<T, O> {
		let mut bitspan = self.bitspan;
		let mut boxed =
			self.into_vec().into_boxed_slice().pipe(ManuallyDrop::new);
		unsafe {
			bitspan.set_address(boxed.as_mut_ptr().into_address());
			BitBox::from_raw(bitspan.into_bitslice_ptr_mut())
		}
	}

	/// Converts a bit-vector into a `Vec` of its underlying storage.
	///
	/// The produced vector contains all elements that contained live bits. Dead
	/// bits have an unspecified value; you should call [`.set_uninitialized()`]
	/// before converting into a vector.
	///
	/// This does not affect the allocated memory; it is purely a conversion of
	/// the handle.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bv = bitvec![u8, Msb0; 0, 1, 0, 0, 1];
	/// let v = bv.into_vec();
	/// assert_eq!(v[0] & 0xF8, 0b01001_000);
	/// ```
	///
	/// [`.set_uninitialized()`]: Self::set_uninitialized
	#[inline]
	pub fn into_vec(self) -> Vec<T> {
		let (bitspan, capacity) = (self.bitspan, self.capacity);
		mem::forget(self);
		unsafe {
			Vec::from_raw_parts(
				bitspan.address().to_mut(),
				bitspan.elements(),
				capacity,
			)
		}
	}
}

/// Utilities.
impl<T, O> BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Overwrites each element (visible in [`.as_raw_mut_slice()`]) with a new
	/// bit-pattern.
	///
	/// This unconditionally writes `element` into each element in the backing
	/// slice, without altering the bit-vector’s length or capacity.
	///
	/// This guarantees that dead bits visible in [`.as_raw_slice()`] but not
	/// [`.as_bitslice()`] are initialized according to the bit-pattern of
	/// `element.` The elements not visible in the raw slice, but present in the
	/// allocation, do *not* specify a value. You may not rely on them being
	/// zeroed *or* being set to the `element` bit-pattern.
	///
	/// ## Parameters
	///
	/// - `&mut self`
	/// - `element`: The bit-pattern with which each live element in the backing
	///   store is initialized.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = bitvec![u8, Msb0; 0; 20];
	/// assert_eq!(bv.as_raw_slice(), [0; 3]);
	/// bv.set_elements(0xA5);
	/// assert_eq!(bv.as_raw_slice(), [0xA5; 3]);
	/// ```
	///
	/// [`.as_bitslice()`]: Self::as_bitslice
	/// [`.as_raw_mut_slice()`]: Self::as_raw_mut_slice
	/// [`.as_raw_slice()`]: Self::as_raw_slice
	#[inline]
	pub fn set_elements(&mut self, element: T::Mem) {
		self.as_raw_mut_slice()
			.iter_mut()
			.for_each(|elt| elt.store_value(element));
	}

	/// Sets the uninitialized bits of a bit-vector to a known value.
	///
	/// This method modifies all bits that are observable in [`.as_raw_slice()`]
	/// but *not* observable in [`.as_bitslice()`] to a known value.
	/// Memory beyond the raw-slice view, but still within the allocation, is
	/// considered fully dead and will never be seen.
	///
	/// This can be used to zero the unused memory so that when viewed as a raw
	/// slice, unused bits have a consistent and predictable value.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut bv = 0b1101_1100u8.view_bits::<Lsb0>().to_bitvec();
	/// assert_eq!(bv.as_raw_slice()[0], 0b1101_1100u8);
	///
	/// bv.truncate(4);
	/// assert_eq!(bv.count_ones(), 2);
	/// assert_eq!(bv.as_raw_slice()[0], 0b1101_1100u8);
	///
	/// bv.set_uninitialized(false);
	/// assert_eq!(bv.as_raw_slice()[0], 0b0000_1100u8);
	///
	/// bv.set_uninitialized(true);
	/// assert_eq!(bv.as_raw_slice()[0], 0b1111_1100u8);
	/// ```
	///
	/// [`.as_bitslice()`]: Self::as_bitslice
	/// [`.as_raw_slice()`]: Self::as_raw_slice
	#[inline]
	pub fn set_uninitialized(&mut self, value: bool) {
		let head = self.bitspan.head().into_inner() as usize;
		let last = head + self.len();
		let all = self.as_raw_mut_slice().view_bits_mut::<O>();
		unsafe {
			all.get_unchecked_mut(.. head).fill(value);
			all.get_unchecked_mut(last ..).fill(value);
		}
	}

	/// Ensures that the live region of the bit-vector’s contents begin at the
	/// front edge of the buffer.
	///
	/// `BitVec` has performance optimizations where it moves its view of its
	/// buffer contents in order to avoid needless moves of its data within the
	/// buffer. This can lead to unexpected contents of the raw memory values,
	/// so this method ensures that the semantic contents of the bit-vector
	/// match its in-memory storage.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let data = 0b00_1111_00u8;
	/// let bits = data.view_bits::<Msb0>();
	///
	/// let mut bv = bits[2 .. 6].to_bitvec();
	/// assert_eq!(bv, bits![1; 4]);
	/// assert_eq!(bv.as_raw_slice()[0], data);
	///
	/// bv.force_align();
	/// assert_eq!(bv, bits![1; 4]);
	/// // BitVec does not specify the value of dead bits in its buffer.
	/// assert_eq!(bv.as_raw_slice()[0] & 0xF0, 0xF0);
	/// ```
	#[inline]
	pub fn force_align(&mut self) {
		let mut bitspan = self.bitspan;
		let len = bitspan.len();
		let head = self.bitspan.head();
		if head == BitIdx::MIN {
			return;
		}
		let head = head.into_inner() as usize;
		let last = head + len;
		unsafe {
			bitspan.set_head(BitIdx::MIN);
			bitspan.set_len(last);
			bitspan
				.into_bitslice_mut()
				.copy_within_unchecked(head .., 0);
			bitspan.set_len(len);
		}
		self.bitspan = bitspan;
	}

	/// Sets the starting-bit index of the span descriptor.
	///
	/// ## Safety
	///
	/// The new `head` value must not cause the final bits of the bit-vector to
	/// depart allocated memory.
	pub(crate) unsafe fn set_head(&mut self, new_head: BitIdx<T::Mem>) {
		self.bitspan.set_head(new_head);
	}

	/// Sets a bit-vector’s length without checking that it fits in the
	/// allocated capacity.
	///
	/// ## Safety
	///
	/// `new_len` must not exceed `self.capacity()`.
	pub(crate) unsafe fn set_len_unchecked(&mut self, new_len: usize) {
		self.bitspan.set_len(new_len);
	}

	/// Asserts that a length can be encoded into the bit-vector handle.
	///
	/// ## Panics
	///
	/// This panics if `len` is too large to encode into a `BitSpan`.
	#[inline]
	fn assert_len_encodable(len: usize) {
		assert!(
			BitSpan::<Const, T, O>::len_encodable(len),
			"bit-vector capacity exceeded: {} > {}",
			len,
			BitSlice::<T, O>::MAX_BITS,
		);
	}

	/// Reserves some memory through the underlying vector.
	///
	/// ## Parameters
	///
	/// - `&mut self`
	/// - `additional`: The amount of additional space required after
	///   `self.len()` in the allocation.
	/// - `func`: A function that manipulates the memory reservation of the
	///   underlying vector.
	///
	/// ## Behavior
	///
	/// `func` should perform the appropriate action to allocate space for at
	/// least `additional` more bits. After it returns, the underlying vector is
	/// extended with zero-initialized elements until `self.len() + additional`
	/// bits have been given initialized memory.
	#[inline]
	fn do_reservation(
		&mut self,
		additional: usize,
		func: impl FnOnce(&mut Vec<T>, usize),
	) {
		let len = self.len();
		let new_len = len.saturating_add(additional);
		Self::assert_len_encodable(new_len);

		let (head, elts) = (self.bitspan.head(), self.bitspan.elements());
		let new_elts =
			crate::mem::elts::<T>(head.into_inner() as usize + new_len);

		let extra_elts = new_elts - elts;
		self.with_vec(|vec| {
			func(&mut **vec, extra_elts);
			//  Ensure that any new elements are initialized.
			vec.resize_with(new_elts, || <T as BitStore>::ZERO);
		});
	}

	/// Briefly constructs an ordinary `Vec` controlling the buffer, allowing
	/// operations to be applied to the memory allocation.
	///
	/// ## Parameters
	///
	/// - `&mut self`
	/// - `func`: A function which may interact with the memory allocation.
	///
	/// After `func` runs, `self` is updated with the temporary `Vec`’s address
	/// and capacity.
	#[inline]
	fn with_vec<F, R>(&mut self, func: F) -> R
	where F: FnOnce(&mut ManuallyDrop<Vec<T>>) -> R {
		let mut vec = unsafe { ptr::read(self) }
			.into_vec()
			.pipe(ManuallyDrop::new);
		let out = func(&mut vec);

		unsafe {
			self.bitspan.set_address(vec.as_mut_ptr().into_address());
		}
		self.capacity = vec.capacity();
		out
	}
}
