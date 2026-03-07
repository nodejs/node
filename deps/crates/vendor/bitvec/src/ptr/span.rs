#![doc = include_str!("../../doc/ptr/span.md")]

use core::{
	any,
	fmt::{
		self,
		Binary,
		Debug,
		Display,
		Formatter,
		Pointer,
	},
	marker::PhantomData,
	mem,
	ptr::{
		self,
		NonNull,
	},
};

use tap::Pipe;
use wyz::{
	comu::{
		Address,
		Const,
		Mut,
		Mutability,
		NullPtrError,
		Reference,
		Referential,
	},
	fmt::FmtForward,
};

use super::{
	BitPtr,
	BitPtrError,
	BitPtrRange,
	MisalignError,
};
use crate::{
	index::{
		BitEnd,
		BitIdx,
	},
	mem::{
		bits_of,
		BitRegister,
	},
	order::{
		BitOrder,
		Lsb0,
	},
	slice::BitSlice,
	store::BitStore,
};

#[doc = include_str!("../../doc/ptr/BitSpan.md")]
pub(crate) struct BitSpan<M = Const, T = usize, O = Lsb0>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// The element address in which the base bit lives.
	ptr: NonNull<()>,
	/// The length of the span, in bits. This must be typed as `()` because it
	/// cannot be directly dereferenced, and will not have valid values for
	/// `NonNull<T>`.
	len: usize,
	/// The bit-ordering within elements used to translate indices to real bits.
	_or: PhantomData<O>,
	/// This is functionally an element-slice pointer.
	_ty: PhantomData<Address<M, [T]>>,
}

impl<M, T, O> BitSpan<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// The canonical empty span. This always uses the dangling address for `T`.
	pub(crate) const EMPTY: Self = Self {
		ptr: NonNull::<T>::dangling().cast::<()>(),
		len: 0,
		_or: PhantomData,
		_ty: PhantomData,
	};
	/// The number of least-significant bits in `.len` needed to hold the low
	/// bits of the head `BitIdx` cursor.
	///
	/// This is always 3 until Rust adds a target architecture whose bytes are
	/// not 8 bits.
	pub(crate) const LEN_HEAD_BITS: usize = 3;
	/// Marks the bits of `.len` that store some of the `.head()` logical field.
	pub(crate) const LEN_HEAD_MASK: usize = 0b111;
	/// Marks the bits of `.ptr` that store the `.addr()` logical field.
	pub(crate) const PTR_ADDR_MASK: usize = !0 << Self::PTR_HEAD_BITS;
	/// The number of least-significant bits in `.ptr` needed to hold the high
	/// bits of the head `BitIdx` cursor.
	pub(crate) const PTR_HEAD_BITS: usize =
		<T::Mem as BitRegister>::INDX as usize - Self::LEN_HEAD_BITS;
	/// Marks the bits of `.ptr` that store some of the `.head()` logical field.
	pub(crate) const PTR_HEAD_MASK: usize = !Self::PTR_ADDR_MASK;
	/// The inclusive-maximum number of bits that a `BitSpan` can cover. This
	/// value is therefore one higher than the maximum *index* that can be used
	/// to select a bit within a span.
	pub(crate) const REGION_MAX_BITS: usize = !0 >> Self::LEN_HEAD_BITS;
	/// The inclusive-maximum number of memory elements that a bit-span can
	/// cover.
	///
	/// This is the number of elements required to store `REGION_MAX_BITS` bits,
	/// plus one because a region could begin away from the zeroth bit and thus
	/// continue into the next element at the end.
	///
	/// Since the region is ⅛th the domain of a `usize` counter already, this
	/// number is guaranteed to be well below the limits of both arithmetic and
	/// Rust’s own ceiling constraints on memory region descriptors.
	pub(crate) const REGION_MAX_ELTS: usize =
		crate::mem::elts::<T::Mem>(Self::REGION_MAX_BITS) + 1;
}

/// Constructors.
impl<M, T, O> BitSpan<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// Constructs an empty `BitSpan` at an allocated address.
	///
	/// This is used when the region has no contents, but the pointer
	/// information must be retained and cannot be canonicalized.
	///
	/// ## Parameters
	///
	/// - `addr`: Some address of a `T` allocation. It must be valid in the
	///   caller’s memory regime.
	///
	/// ## Returns
	///
	/// A zero-length `BitSpan` based at `addr`.
	#[cfg(feature = "alloc")]
	pub(crate) fn uninhabited(addr: Address<M, T>) -> Self {
		Self {
			ptr: addr.into_inner().cast::<()>(),
			..Self::EMPTY
		}
	}

	/// Creates a new bit-span from its logical components.
	///
	/// ## Parameters
	///
	/// - `addr`: The base address of the memory region in which the bit-span
	///   resides.
	/// - `head`: The index of the initial bit within `*addr`.
	/// - `bits`: The number of bits contained in the bit-span.
	///
	/// ## Returns
	///
	/// This fails in the following conditions:
	///
	/// - `bits` is greater than `REGION_MAX_BITS`
	/// - `addr` is not aligned to `T`.
	/// - `addr + elts(bits)` wraps around the address space
	///
	/// The `Address` type already enforces the non-null requirement.
	pub(crate) fn new(
		addr: Address<M, T>,
		head: BitIdx<T::Mem>,
		bits: usize,
	) -> Result<Self, BitSpanError<T>> {
		if bits > Self::REGION_MAX_BITS {
			return Err(BitSpanError::TooLong(bits));
		}
		let base = BitPtr::<M, T, O>::new(addr, head)?;
		let last = base.wrapping_add(bits);
		if last < base {
			return Err(BitSpanError::TooHigh(addr.to_const()));
		}

		Ok(unsafe { Self::new_unchecked(addr, head, bits) })
	}

	/// Creates a new bit-span from its components, without any validity checks.
	///
	/// ## Safety
	///
	/// The caller must ensure that the arguments satisfy all the requirements
	/// outlined in [`::new()`]. The easiest way to ensure this is to only use
	/// this function to construct bit-spans from values extracted from
	/// bit-spans previously constructed through `::new()`.
	///
	/// This function **only** performs the value encoding. Invalid lengths will
	/// truncate, and invalid addresses may cause memory unsafety.
	///
	/// [`::new()`]: Self::new
	pub(crate) unsafe fn new_unchecked(
		addr: Address<M, T>,
		head: BitIdx<T::Mem>,
		bits: usize,
	) -> Self {
		let addr = addr.to_const().cast::<u8>();

		let head = head.into_inner() as usize;
		let ptr_data = addr as usize & Self::PTR_ADDR_MASK;
		let ptr_head = head >> Self::LEN_HEAD_BITS;

		let len_head = head & Self::LEN_HEAD_MASK;
		let len_bits = bits << Self::LEN_HEAD_BITS;

		/* See <https://github.com/bitvecto-rs/bitvec/issues/135#issuecomment-986357842>.
		 * This attempts to retain inbound provenance information and may help
		 * Miri better understand pointer operations this module performs.
		 *
		 * This performs `a + (p - a)` in `addr`’s provenance zone, which is
		 * numerically equivalent to `p` but does not require conjuring a new,
		 * uninformed, pointer value.
		 */
		let ptr_raw = ptr_data | ptr_head;
		let ptr = addr.wrapping_add(ptr_raw.wrapping_sub(addr as usize));

		Self {
			ptr: NonNull::new_unchecked(ptr.cast::<()>() as *mut ()),
			len: len_bits | len_head,
			..Self::EMPTY
		}
	}
}

/// Encoded fields.
impl<M, T, O> BitSpan<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// Gets the base element address of the referent region.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The address of the starting element of the memory region. This address
	/// is weakly typed so that it can be cast by call sites to the most useful
	/// access type.
	pub(crate) fn address(&self) -> Address<M, T> {
		Address::new(unsafe {
			NonNull::new_unchecked(
				(self.ptr.as_ptr() as usize & Self::PTR_ADDR_MASK) as *mut T,
			)
		})
	}

	/// Overwrites the data pointer with a new address. This method does not
	/// perform safety checks on the new pointer.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `ptr`: The new address of the `BitSpan`’s domain.
	///
	/// # Safety
	///
	/// None. The invariants of [`::new`] must be checked at the caller.
	///
	/// [`::new`]: Self::new
	#[cfg(feature = "alloc")]
	pub(crate) unsafe fn set_address(&mut self, addr: Address<M, T>) {
		let mut addr_value = addr.to_const() as usize;
		addr_value &= Self::PTR_ADDR_MASK;
		addr_value |= self.ptr.as_ptr() as usize & Self::PTR_HEAD_MASK;
		self.ptr = NonNull::new_unchecked(addr_value as *mut ())
	}

	/// Gets the starting bit index of the referent region.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// A [`BitIdx`] of the first live bit in the element at the
	/// [`self.address()`] address.
	///
	/// [`BitIdx`]: crate::index::BitIdx
	/// [`self.address()`]: Self::address
	pub(crate) fn head(&self) -> BitIdx<T::Mem> {
		let ptr = self.ptr.as_ptr() as usize;
		let ptr_head = (ptr & Self::PTR_HEAD_MASK) << Self::LEN_HEAD_BITS;
		let len_head = self.len & Self::LEN_HEAD_MASK;
		unsafe { BitIdx::new_unchecked((ptr_head | len_head) as u8) }
	}

	/// Writes a new `head` value into the pointer, with no other effects.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `head`: A new starting index.
	///
	/// # Effects
	///
	/// `head` is written into the `.head` logical field, without affecting
	/// `.addr` or `.bits`.
	#[cfg(feature = "alloc")]
	pub(crate) unsafe fn set_head(&mut self, head: BitIdx<T::Mem>) {
		let head = head.into_inner() as usize;
		let mut ptr = self.ptr.as_ptr() as usize;

		ptr &= Self::PTR_ADDR_MASK;
		ptr |= head >> Self::LEN_HEAD_BITS;
		self.ptr = NonNull::new_unchecked(ptr as *mut ());

		self.len &= !Self::LEN_HEAD_MASK;
		self.len |= head & Self::LEN_HEAD_MASK;
	}

	/// Gets the number of live bits in the described region.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// A count of how many live bits the region pointer describes.
	pub(crate) fn len(&self) -> usize {
		self.len >> Self::LEN_HEAD_BITS
	}

	/// Sets the `.bits` logical member to a new value.
	///
	/// # Parameters
	///
	/// - `&mut self`
	/// - `len`: A new bit length. This must not be greater than
	///   [`REGION_MAX_BITS`].
	///
	/// # Effects
	///
	/// The `new_len` value is written directly into the `.bits` logical field.
	///
	/// [`REGION_MAX_BITS`]: Self::REGION_MAX_BITS
	pub(crate) unsafe fn set_len(&mut self, new_len: usize) {
		if cfg!(debug_assertions) {
			*self = Self::new(self.address(), self.head(), new_len).unwrap();
		}
		else {
			self.len &= Self::LEN_HEAD_MASK;
			self.len |= new_len << Self::LEN_HEAD_BITS;
		}
	}

	/// Gets the three logical components of the pointer.
	///
	/// The encoding is not public API, and direct field access is never
	/// supported.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// - `.0`: The base address of the referent memory region.
	/// - `.1`: The index of the first live bit in the first element of the
	///   region.
	/// - `.2`: The number of live bits in the region.
	pub(crate) fn raw_parts(&self) -> (Address<M, T>, BitIdx<T::Mem>, usize) {
		(self.address(), self.head(), self.len())
	}
}

/// Virtual fields.
impl<M, T, O> BitSpan<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// Computes the number of elements, starting at [`self.address()`], that
	/// the region touches.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// The count of all elements, starting at [`self.address()`], that contain
	/// live bits included in the referent region.
	///
	/// [`self.address()`]: Self::address
	pub(crate) fn elements(&self) -> usize {
		crate::mem::elts::<T>(self.len() + self.head().into_inner() as usize)
	}

	/// Computes the tail index for the first dead bit after the live bits.
	///
	/// # Parameters
	///
	/// - `&self`
	///
	/// # Returns
	///
	/// A `BitEnd` that is the index of the first dead bit after the last live
	/// bit in the last element. This will almost always be in the range `1 ..=
	/// T::Mem::BITS`.
	///
	/// It will be zero only when `self` is empty.
	pub(crate) fn tail(&self) -> BitEnd<T::Mem> {
		let (head, len) = (self.head(), self.len());
		let (_, tail) = head.span(len);
		tail
	}
}

/// Conversions.
impl<M, T, O> BitSpan<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// Casts the span to another element type.
	///
	/// This does not alter the encoded value of the pointer! It only
	/// reinterprets the element type, and the encoded value may shift
	/// significantly in the result type. Use with caution.
	pub(crate) fn cast<U>(self) -> BitSpan<M, U, O>
	where U: BitStore {
		let Self { ptr, len, .. } = self;
		BitSpan {
			ptr,
			len,
			..BitSpan::EMPTY
		}
	}

	/// Reäligns a bit-span to a different base memory type.
	///
	/// ## Original
	///
	/// [`slice::align_to`](https://doc.rust-lang.org/std/primitive.slice.html#method.align_to)
	///
	/// ## Safety
	///
	/// `U` must have the same type family as `T`. It is illegal to use this
	/// method to cast away alias safeties such as an atomic or `Cell` wrapper.
	pub(crate) unsafe fn align_to<U>(self) -> (Self, BitSpan<M, U, O>, Self)
	where U: BitStore {
		/* This function body implements the algorithm locally, rather than
		 * delegating to the standard library’s `<[T]>::align_to::<U>`
		 * function, because that requires use of memory references, and
		 * `BitSpan` does not require that its values be valid for
		 * dereference.
		 */
		let this = self.to_bitptr();
		//  Counter for how many bits remain in the span.
		let mut rem = self.len();
		//  The *byte* alignment of `U`.
		let align = mem::align_of::<U>();
		//  1. Get the number of bits between `self.head()` and the start of a
		//     `[U]` region.
		let step = this.align_offset(align);
		//  If this count is more than the available bits, quit.
		if step > rem {
			return (self, BitSpan::EMPTY, Self::EMPTY);
		}
		let left = this.span_unchecked(step);
		rem -= step;

		let mid_base =
			this.add(step).address().cast::<U>().pipe(|addr| {
				BitPtr::<M, U, O>::new_unchecked(addr, BitIdx::MIN)
			});
		let mid_elts = rem >> <U::Mem as BitRegister>::INDX;
		let excess = rem & <U::Mem as BitRegister>::MASK as usize;
		let step = rem - excess;
		let mid = mid_base.span_unchecked(step);

		let right_base =
			mid_base.address().add(mid_elts).cast::<T>().pipe(|addr| {
				BitPtr::<M, T, O>::new_unchecked(addr, BitIdx::MIN)
			});
		let right = right_base.span_unchecked(excess);

		(left, mid, right)
	}

	/// Casts a mutable bit-slice pointer into its structural representation.
	pub(crate) fn from_bitslice_ptr_mut(raw: *mut BitSlice<T, O>) -> Self {
		let BitSpan { ptr, len, .. } =
			BitSpan::from_bitslice_ptr(raw as *const BitSlice<T, O>);
		Self {
			ptr,
			len,
			..Self::EMPTY
		}
	}

	/// Converts the span descriptor into a raw `BitSlice` pointer.
	///
	/// This is a noöp.
	pub(crate) fn into_bitslice_ptr(self) -> *const BitSlice<T, O> {
		let Self { ptr, len, .. } = self;
		ptr::slice_from_raw_parts(ptr.as_ptr(), len) as *const BitSlice<T, O>
	}

	/// Converts the span descriptor into a shared `BitSlice` reference.
	///
	/// This is a noöp.
	///
	/// ## Safety
	///
	/// The span must describe memory that is safe to dereference, and to which
	/// no `&mut BitSlice` references exist.
	pub(crate) unsafe fn into_bitslice_ref<'a>(self) -> &'a BitSlice<T, O> {
		&*self.into_bitslice_ptr()
	}

	/// Produces a bit-pointer to the start of the span.
	///
	/// This is **not** a noöp: the base address and starting bit index are
	/// decoded into the bit-pointer structure.
	pub(crate) fn to_bitptr(self) -> BitPtr<M, T, O> {
		unsafe { BitPtr::new_unchecked(self.address(), self.head()) }
	}

	/// Produces a bit-pointer range to either end of the span.
	///
	/// This is **not** a noöp: all three logical fields are decoded in order to
	/// construct the range.
	pub(crate) fn to_bitptr_range(self) -> BitPtrRange<M, T, O> {
		let start = self.to_bitptr();
		let end = unsafe { start.add(self.len()) };
		BitPtrRange { start, end }
	}

	/// Converts the span descriptor into an `Address<>` generic pointer.
	///
	/// This is a noöp.
	pub(crate) fn to_bitslice_addr(self) -> Address<M, BitSlice<T, O>> {
		(self.into_bitslice_ptr() as *mut BitSlice<T, O>)
			.pipe(|ptr| unsafe { NonNull::new_unchecked(ptr) })
			.pipe(Address::new)
	}

	/// Converts the span descriptor into a `Reference<>` generic handle.
	///
	/// This is a noöp.
	pub(crate) fn to_bitslice<'a>(self) -> Reference<'a, M, BitSlice<T, O>>
	where Address<M, BitSlice<T, O>>: Referential<'a> {
		unsafe { self.to_bitslice_addr().to_ref() }
	}
}

/// Conversions.
impl<T, O> BitSpan<Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Creates a `Const` span descriptor from a `const` bit-slice pointer.
	pub(crate) fn from_bitslice_ptr(raw: *const BitSlice<T, O>) -> Self {
		let slice_nn = match NonNull::new(raw as *const [()] as *mut [()]) {
			Some(nn) => nn,
			None => return Self::EMPTY,
		};
		let ptr = slice_nn.cast::<()>();
		let len = unsafe { slice_nn.as_ref() }.len();
		Self {
			ptr,
			len,
			..Self::EMPTY
		}
	}
}

/// Conversions.
impl<T, O> BitSpan<Mut, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Converts the span descriptor into a raw mutable `BitSlice` pointer.
	///
	/// This is a noöp.
	pub(crate) fn into_bitslice_ptr_mut(self) -> *mut BitSlice<T, O> {
		self.into_bitslice_ptr() as *mut BitSlice<T, O>
	}

	/// Converts the span descriptor into an exclusive `BitSlice` reference.
	///
	/// This is a noöp.
	///
	/// ## Safety
	///
	/// The span must describe memory that is safe to dereference. In addition,
	/// no other `BitSlice` reference of any kind (`&` or `&mut`) may exist.
	pub(crate) unsafe fn into_bitslice_mut<'a>(self) -> &'a mut BitSlice<T, O> {
		&mut *self.into_bitslice_ptr_mut()
	}
}

/// Utilities.
impl<M, T, O> BitSpan<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// Checks if a requested length can be encoded into the `BitSpan`.
	///
	/// This is `len <= Self::REGION_MAX_BITS`.
	#[cfg(feature = "alloc")]
	pub(crate) fn len_encodable(len: usize) -> bool {
		len <= Self::REGION_MAX_BITS
	}

	/// Renders the pointer structure into a formatter for use during
	/// higher-level type [`Debug`] implementations.
	///
	/// # Parameters
	///
	/// - `&self`
	/// - `fmt`: The formatter into which the pointer is rendered.
	/// - `name`: The suffix of the structure rendering its pointer. The `Bit`
	///   prefix is applied to the object type name in this format.
	/// - `fields`: Any additional fields in the object’s debug info to be
	///   rendered.
	///
	/// # Returns
	///
	/// The result of formatting the pointer into the receiver.
	///
	/// # Behavior
	///
	/// This function writes `Bit{name}<{ord}, {type}> {{ {fields } }}` into the
	/// `fmt` formatter, where `{fields}` includes the address, head index, and
	/// bit length of the pointer, as well as any additional fields provided by
	/// the caller.
	///
	/// Higher types in the crate should use this function to drive their
	/// [`Debug`] implementations, and then use [`BitSlice`]’s list formatters
	/// to display their buffer contents.
	///
	/// [`BitSlice`]: crate::slice::BitSlice
	/// [`Debug`]: core::fmt::Debug
	pub(crate) fn render<'a>(
		&'a self,
		fmt: &'a mut Formatter,
		name: &'a str,
		fields: impl IntoIterator<Item = &'a (&'a str, &'a dyn Debug)>,
	) -> fmt::Result {
		write!(
			fmt,
			"Bit{}<{}, {}>",
			name,
			any::type_name::<T::Mem>(),
			any::type_name::<O>(),
		)?;
		let mut builder = fmt.debug_struct("");
		builder
			.field("addr", &self.address().fmt_pointer())
			.field("head", &self.head().fmt_binary())
			.field("bits", &self.len());
		for (name, value) in fields {
			builder.field(name, value);
		}
		builder.finish()
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Clone for BitSpan<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn clone(&self) -> Self {
		*self
	}
}

impl<M1, M2, O, T1, T2> PartialEq<BitSpan<M2, T2, O>> for BitSpan<M1, T1, O>
where
	M1: Mutability,
	M2: Mutability,
	O: BitOrder,
	T1: BitStore,
	T2: BitStore,
{
	#[inline]
	fn eq(&self, other: &BitSpan<M2, T2, O>) -> bool {
		let (addr_a, head_a, bits_a) = self.raw_parts();
		let (addr_b, head_b, bits_b) = other.raw_parts();
		bits_of::<T1::Mem>() == bits_of::<T2::Mem>()
			&& addr_a.to_const() as usize == addr_b.to_const() as usize
			&& head_a.into_inner() == head_b.into_inner()
			&& bits_a == bits_b
	}
}

impl<T, O> From<&BitSlice<T, O>> for BitSpan<Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(bits: &BitSlice<T, O>) -> Self {
		Self::from_bitslice_ptr(bits)
	}
}

impl<T, O> From<&mut BitSlice<T, O>> for BitSpan<Mut, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(bits: &mut BitSlice<T, O>) -> Self {
		Self::from_bitslice_ptr_mut(bits)
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Default for BitSpan<M, T, O>
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

impl<M, T, O> Debug for BitSpan<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		self.render(fmt, "Span", None)
	}
}

impl<M, T, O> Pointer for BitSpan<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		Pointer::fmt(&self.address(), fmt)?;
		fmt.write_str("(")?;
		Binary::fmt(&self.head(), fmt)?;
		fmt.write_str(")[")?;
		Display::fmt(&self.len(), fmt)?;
		fmt.write_str("]")
	}
}

impl<M, T, O> Copy for BitSpan<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
}

/// An error produced when creating `BitSpan` encoded references.
#[derive(Clone, Copy, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum BitSpanError<T>
where T: BitStore
{
	/// A null pointer was provided.
	Null(NullPtrError),
	/// The base element pointer is not aligned.
	Misaligned(MisalignError<T>),
	/// The requested length exceeds the `BitSpan` length ceiling.
	TooLong(usize),
	/// The requested address is too high, and wraps to zero.
	TooHigh(*const T),
}

#[cfg(not(tarpaulin_include))]
impl<T> From<BitPtrError<T>> for BitSpanError<T>
where T: BitStore
{
	#[inline]
	fn from(err: BitPtrError<T>) -> Self {
		match err {
			BitPtrError::Null(err) => Self::Null(err),
			BitPtrError::Misaligned(err) => Self::Misaligned(err),
		}
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> From<MisalignError<T>> for BitSpanError<T>
where T: BitStore
{
	#[inline]
	fn from(err: MisalignError<T>) -> Self {
		Self::Misaligned(err)
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> Debug for BitSpanError<T>
where T: BitStore
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(fmt, "BitSpanError<{}>::", any::type_name::<T::Mem>())?;
		match self {
			Self::Null(err) => fmt.debug_tuple("Null").field(&err).finish(),
			Self::Misaligned(err) => {
				fmt.debug_tuple("Misaligned").field(&err).finish()
			},
			Self::TooLong(len) => fmt.debug_tuple("TooLong").field(len).finish(),
			Self::TooHigh(addr) => {
				fmt.debug_tuple("TooHigh").field(addr).finish()
			},
		}
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> Display for BitSpanError<T>
where T: BitStore
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		match self {
			Self::Null(err) => Display::fmt(err, fmt),
			Self::Misaligned(err) => Display::fmt(err, fmt),
			Self::TooLong(len) => write!(
				fmt,
				"Length {} is too long to encode in a bit-slice, which can \
				 only accept {} bits",
				len,
				BitSpan::<Const, T, Lsb0>::REGION_MAX_BITS,
			),
			Self::TooHigh(addr) => write!(
				fmt,
				"Address {:p} is too high, and produces a span that wraps \
				 around to the zero address.",
				addr,
			),
		}
	}
}

unsafe impl<T> Send for BitSpanError<T> where T: BitStore {}

unsafe impl<T> Sync for BitSpanError<T> where T: BitStore {}

#[cfg(feature = "std")]
impl<T> std::error::Error for BitSpanError<T> where T: BitStore {}
