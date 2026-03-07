#![doc = include_str!("../../doc/ptr/single.md")]

use core::{
	any,
	cmp,
	convert::TryFrom,
	fmt::{
		self,
		Debug,
		Display,
		Formatter,
		Pointer,
	},
	hash::{
		Hash,
		Hasher,
	},
	marker::PhantomData,
	ptr,
};

use tap::{
	Pipe,
	TryConv,
};
use wyz::{
	comu::{
		Address,
		Const,
		Frozen,
		Mut,
		Mutability,
		NullPtrError,
	},
	fmt::FmtForward,
};

use super::{
	check_alignment,
	AddressExt,
	BitPtrRange,
	BitRef,
	BitSpan,
	BitSpanError,
	MisalignError,
};
use crate::{
	access::BitAccess,
	devel as dvl,
	index::BitIdx,
	mem,
	order::{
		BitOrder,
		Lsb0,
	},
	store::BitStore,
};

#[repr(C, packed)]
#[doc = include_str!("../../doc/ptr/BitPtr.md")]
pub struct BitPtr<M = Const, T = usize, O = Lsb0>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// Memory addresses must be well-aligned and non-null.
	///
	/// This is not actually a requirement of `BitPtr`, but it is a requirement
	/// of `BitSpan`, and it is extended across the entire crate for
	/// consistency.
	ptr: Address<M, T>,
	/// The index of the referent bit within `*addr`.
	bit: BitIdx<T::Mem>,
	/// The ordering used to select the bit at `head` in `*addr`.
	_or: PhantomData<O>,
}

impl<M, T, O> BitPtr<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// The canonical dangling pointer. This selects the starting bit of the
	/// canonical dangling pointer for `T`.
	pub const DANGLING: Self = Self {
		ptr: Address::DANGLING,
		bit: BitIdx::MIN,
		_or: PhantomData,
	};

	/// Loads the address field, sidestepping any alignment problems.
	///
	/// This is the only safe way to access `(&self).ptr`. Do not perform field
	/// access on `.ptr` through a reference except through this method.
	#[inline]
	fn get_addr(&self) -> Address<M, T> {
		unsafe { ptr::addr_of!(self.ptr).read_unaligned() }
	}

	/// Tries to construct a `BitPtr` from a memory location and a bit index.
	///
	/// ## Parameters
	///
	/// - `ptr`: The address of a memory element. `Address` wraps raw pointers
	///   or references, and enforces that they are not null. `BitPtr`
	///   additionally requires that the address be well-aligned to its type;
	///   misaligned addresses cause this to return an error.
	/// - `bit`: The index of the selected bit within `*ptr`.
	///
	/// ## Returns
	///
	/// This returns an error if `ptr` is not aligned to `T`; otherwise, it
	/// returns a new bit-pointer structure to the given element and bit.
	///
	/// You should typically prefer to use constructors that take directly from
	/// a memory reference or pointer, such as the `TryFrom<*T>`
	/// implementations, the `From<&/mut T>` implementations, or the
	/// [`::from_ref()`], [`::from_mut()`], [`::from_slice()`], or
	/// [`::from_slice_mut()`] functions.
	///
	/// [`::from_mut()`]: Self::from_mut
	/// [`::from_ref()`]: Self::from_ref
	/// [`::from_slice()`]: Self::from_slice
	/// [`::from_slice_mut()`]: Self::from_slice_mut
	#[inline]
	pub fn new(
		ptr: Address<M, T>,
		bit: BitIdx<T::Mem>,
	) -> Result<Self, MisalignError<T>> {
		Ok(Self {
			ptr: check_alignment(ptr)?,
			bit,
			..Self::DANGLING
		})
	}

	/// Constructs a `BitPtr` from an address and head index, without checking
	/// the address for validity.
	///
	/// ## Parameters
	///
	/// - `addr`: The memory address to use in the bit-pointer. See the Safety
	///   section.
	/// - `head`: The index of the bit in `*addr` that this bit-pointer selects.
	///
	/// ## Returns
	///
	/// A new bit-pointer composed of the parameters. No validity checking is
	/// performed.
	///
	/// ## Safety
	///
	/// The `Address` type imposes a non-null requirement. `BitPtr` additionally
	/// requires that `addr` is well-aligned for `T`, and presumes that the
	/// caller has ensured this with [`bv_ptr::check_alignment`][0]. If this is
	/// not the case, then the program is incorrect, and subsequent behavior is
	/// not specified.
	///
	/// [0]: crate::ptr::check_alignment.
	#[inline]
	pub unsafe fn new_unchecked(
		ptr: Address<M, T>,
		bit: BitIdx<T::Mem>,
	) -> Self {
		if cfg!(debug_assertions) {
			Self::new(ptr, bit).unwrap()
		}
		else {
			Self {
				ptr,
				bit,
				..Self::DANGLING
			}
		}
	}

	/// Gets the address of the base storage element.
	#[inline]
	pub fn address(self) -> Address<M, T> {
		self.get_addr()
	}

	/// Gets the `BitIdx` that selects the bit within the memory element.
	#[inline]
	pub fn bit(self) -> BitIdx<T::Mem> {
		self.bit
	}

	/// Decomposes a bit-pointer into its element address and bit index.
	///
	/// ## Parameters
	///
	/// - `self`
	///
	/// ## Returns
	///
	/// - `.0`: The memory address in which the referent bit is located.
	/// - `.1`: The index of the referent bit in `*.0` according to the `O` type
	///   parameter.
	#[inline]
	pub fn raw_parts(self) -> (Address<M, T>, BitIdx<T::Mem>) {
		(self.address(), self.bit())
	}

	/// Converts a bit-pointer into a span descriptor by attaching a length
	/// counter (in bits).
	///
	/// ## Parameters
	///
	/// - `self`: The base address of the produced span.
	/// - `bits`: The length, in bits, of the span.
	///
	/// ## Returns
	///
	/// A span descriptor beginning at `self` and ending (exclusive) at `self +
	/// bits`. This fails if it is unable to encode the requested span into a
	/// descriptor.
	pub(crate) fn span(
		self,
		bits: usize,
	) -> Result<BitSpan<M, T, O>, BitSpanError<T>> {
		BitSpan::new(self.ptr, self.bit, bits)
	}

	/// Converts a bit-pointer into a span descriptor, without performing
	/// encoding validity checks.
	///
	/// ## Parameters
	///
	/// - `self`: The base address of the produced span.
	/// - `bits`: The length, in bits, of the span.
	///
	/// ## Returns
	///
	/// An encoded span descriptor of `self` and `bits`. Note that no validity
	/// checks are performed!
	///
	/// ## Safety
	///
	/// The caller must ensure that the rules of `BitSpan::new` are not
	/// violated. Typically this method should only be used on parameters that
	/// have already passed through `BitSpan::new` and are known to be good.
	pub(crate) unsafe fn span_unchecked(self, bits: usize) -> BitSpan<M, T, O> {
		BitSpan::new_unchecked(self.get_addr(), self.bit, bits)
	}

	/// Produces a bit-pointer range beginning at `self` (inclusive) and ending
	/// at `self + count` (exclusive).
	///
	/// ## Safety
	///
	/// `self + count` must be within the same provenance region as `self`. The
	/// first bit past the end of an allocation is included in provenance
	/// regions, though it is not dereferenceable and will not be dereferenced.
	///
	/// It is unsound to *even construct* a pointer that departs the provenance
	/// region, even if that pointer is never dereferenced!
	pub(crate) unsafe fn range(self, count: usize) -> BitPtrRange<M, T, O> {
		(self .. self.add(count)).into()
	}

	/// Removes write permissions from a bit-pointer.
	#[inline]
	pub fn to_const(self) -> BitPtr<Const, T, O> {
		let Self {
			ptr: addr,
			bit: head,
			..
		} = self;
		BitPtr {
			ptr: addr.immut(),
			bit: head,
			..BitPtr::DANGLING
		}
	}

	/// Adds write permissions to a bit-pointer.
	///
	/// ## Safety
	///
	/// This pointer must have been derived from a `*mut` pointer.
	#[inline]
	pub unsafe fn to_mut(self) -> BitPtr<Mut, T, O> {
		let Self {
			ptr: addr,
			bit: head,
			..
		} = self;
		BitPtr {
			ptr: addr.assert_mut(),
			bit: head,
			..BitPtr::DANGLING
		}
	}

	/// Freezes a bit-pointer, forbidding direct mutation.
	///
	/// This is used as a necessary prerequisite to all mutation of memory.
	/// `BitPtr` uses an implementation scoped to `Frozen<_>` to perform
	/// alias-aware writes; see below.
	pub(crate) fn freeze(self) -> BitPtr<Frozen<M>, T, O> {
		let Self {
			ptr: addr,
			bit: head,
			..
		} = self;
		BitPtr {
			ptr: addr.freeze(),
			bit: head,
			..BitPtr::DANGLING
		}
	}
}

impl<T, O> BitPtr<Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Constructs a `BitPtr` to the zeroth bit in a single element.
	#[inline]
	pub fn from_ref(elem: &T) -> Self {
		unsafe { Self::new_unchecked(elem.into(), BitIdx::MIN) }
	}

	/// Constructs a `BitPtr` to the zeroth bit in the zeroth element of a
	/// slice.
	///
	/// This method is distinct from `Self::from_ref(&elem[0])`, because it
	/// ensures that the returned bit-pointer has provenance over the entire
	/// slice. Indexing within a slice narrows the provenance range, and makes
	/// departure from the subslice, *even within the original slice*, illegal.
	#[inline]
	pub fn from_slice(slice: &[T]) -> Self {
		unsafe {
			Self::new_unchecked(slice.as_ptr().into_address(), BitIdx::MIN)
		}
	}

	/// Gets a raw pointer to the memory element containing the selected bit.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn pointer(&self) -> *const T {
		self.get_addr().to_const()
	}
}

impl<T, O> BitPtr<Mut, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Constructs a mutable `BitPtr` to the zeroth bit in a single element.
	#[inline]
	pub fn from_mut(elem: &mut T) -> Self {
		unsafe { Self::new_unchecked(elem.into(), BitIdx::MIN) }
	}

	/// Constructs a `BitPtr` to the zeroth bit in the zeroth element of a
	/// mutable slice.
	///
	/// This method is distinct from `Self::from_mut(&mut elem[0])`, because it
	/// ensures that the returned bit-pointer has provenance over the entire
	/// slice. Indexing within a slice narrows the provenance range, and makes
	/// departure from the subslice, *even within the original slice*, illegal.
	#[inline]
	pub fn from_mut_slice(slice: &mut [T]) -> Self {
		unsafe {
			Self::new_unchecked(slice.as_mut_ptr().into_address(), BitIdx::MIN)
		}
	}

	/// Constructs a mutable `BitPtr` to the zeroth bit in the zeroth element of
	/// a slice.
	///
	/// This method is distinct from `Self::from_mut(&mut elem[0])`, because it
	/// ensures that the returned bit-pointer has provenance over the entire
	/// slice. Indexing within a slice narrows the provenance range, and makes
	/// departure from the subslice, *even within the original slice*, illegal.
	#[inline]
	pub fn from_slice_mut(slice: &mut [T]) -> Self {
		unsafe {
			Self::new_unchecked(slice.as_mut_ptr().into_address(), BitIdx::MIN)
		}
	}

	/// Gets a raw pointer to the memory location containing the selected bit.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn pointer(&self) -> *mut T {
		self.get_addr().to_mut()
	}
}

/// Port of the `*bool` inherent API.
impl<M, T, O> BitPtr<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// Tests if a bit-pointer is the null value.
	///
	/// This is always false, as a `BitPtr` is a `NonNull` internally. Use
	/// `Option<BitPtr>` to express the potential for a null pointer.
	///
	/// ## Original
	///
	/// [`pointer::is_null`](https://doc.rust-lang.org/std/primitive.pointer.html#method.is_null)
	#[inline]
	#[deprecated = "`BitPtr` is never null"]
	pub fn is_null(self) -> bool {
		false
	}

	/// Casts to a `BitPtr` with a different storage parameter.
	///
	/// This is not free! In order to maintain value integrity, it encodes a
	/// `BitSpan` encoded descriptor with its value, casts that, then decodes
	/// into a `BitPtr` of the target type. If `T` and `U` have different
	/// `::Mem` associated types, then this may change the selected bit in
	/// memory. This is an unavoidable cost of the addressing and encoding
	/// schemes.
	///
	/// ## Original
	///
	/// [`pointer::cast`](https://doc.rust-lang.org/std/primitive.pointer.html#method.cast)
	#[inline]
	pub fn cast<U>(self) -> BitPtr<M, U, O>
	where U: BitStore {
		let (addr, head, _) =
			unsafe { self.span_unchecked(1) }.cast::<U>().raw_parts();
		unsafe { BitPtr::new_unchecked(addr, head) }
	}

	/// Decomposes a bit-pointer into its address and head-index components.
	///
	/// ## Original
	///
	/// [`pointer::to_raw_parts`](https://doc.rust-lang.org/std/primitive.pointer.html#method.to_raw_parts)
	///
	/// ## API Differences
	///
	/// The original method is unstable as of 1.54.0; however, because `BitPtr`
	/// already has a similar API, the name is optimistically stabilized here.
	/// Prefer [`.raw_parts()`] until the original inherent stabilizes.
	///
	/// [`.raw_parts()`]: Self::raw_parts
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn to_raw_parts(self) -> (Address<M, T>, BitIdx<T::Mem>) {
		self.raw_parts()
	}

	/// Produces a proxy reference to the referent bit.
	///
	/// Because `BitPtr` guarantees that it is non-null and well-aligned, this
	/// never returns `None`. However, this is still unsafe to call on any
	/// bit-pointers created from conjured values rather than known references.
	///
	/// ## Original
	///
	/// [`pointer::as_ref`](https://doc.rust-lang.org/std/primitive.pointer.html#method.as_ref)
	///
	/// ## API Differences
	///
	/// This produces a proxy type rather than a true reference. The proxy
	/// implements `Deref<Target = bool>`, and can be converted to `&bool` with
	/// a reborrow `&*`.
	///
	/// ## Safety
	///
	/// Since `BitPtr` does not permit null or misaligned pointers, this method
	/// will always dereference the pointer in order to create the proxy. As
	/// such, you must ensure the following conditions are met:
	///
	/// - the pointer must be dereferenceable as defined in the standard library
	///   documentation
	/// - the pointer must point to an initialized instance of `T`
	/// - you must ensure that no other pointer will race to modify the referent
	///   location while this call is reading from memory to produce the proxy
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let data = 1u8;
	/// let ptr = BitPtr::<_, _, Lsb0>::from_ref(&data);
	/// let val = unsafe { ptr.as_ref() }.unwrap();
	/// assert!(*val);
	/// ```
	#[inline]
	pub unsafe fn as_ref<'a>(self) -> Option<BitRef<'a, Const, T, O>> {
		Some(BitRef::from_bitptr(self.to_const()))
	}

	/// Creates a new bit-pointer at a specified offset from the original.
	///
	/// `count` is in units of bits.
	///
	/// ## Original
	///
	/// [`pointer::offset`](https://doc.rust-lang.org/std/primitive.pointer.html#method.offset)
	///
	/// ## Safety
	///
	/// `BitPtr` is implemented with Rust raw pointers internally, and is
	/// subject to all of Rust’s rules about provenance and permission tracking.
	/// You must abide by the safety rules established in the original method,
	/// to which this internally delegates.
	///
	/// Additionally, `bitvec` imposes its own rules: while Rust cannot observe
	/// provenance beyond an element or byte level, `bitvec` demands that
	/// `&mut BitSlice` have exclusive view over all bits it observes. You must
	/// not produce a bit-pointer that departs a `BitSlice` region and intrudes
	/// on any `&mut BitSlice`’s handle, and you must not produce a
	/// write-capable bit-pointer that intrudes on a `&BitSlice` handle that
	/// expects its contents to be immutable.
	///
	/// Note that it is illegal to *construct* a bit-pointer that invalidates
	/// any of these rules. If you wish to defer safety-checking to the point of
	/// dereferencing, and allow the temporary construction *but not*
	/// *dereference* of illegal `BitPtr`s, use [`.wrapping_offset()`] instead.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let data = 5u8;
	/// let ptr = BitPtr::<_, _, Lsb0>::from_ref(&data);
	/// unsafe {
	///   assert!(ptr.read());
	///   assert!(!ptr.offset(1).read());
	///   assert!(ptr.offset(2).read());
	/// }
	/// ```
	///
	/// [`.wrapping_offset()`]: Self::wrapping_offset
	#[inline]
	#[must_use = "returns a new bit-pointer rather than modifying its argument"]
	pub unsafe fn offset(self, count: isize) -> Self {
		let (elts, head) = self.bit.offset(count);
		Self::new_unchecked(self.ptr.offset(elts), head)
	}

	/// Creates a new bit-pointer at a specified offset from the original.
	///
	/// `count` is in units of bits.
	///
	/// ## Original
	///
	/// [`pointer::wrapping_offset`](https://doc.rust-lang.org/std/primitive.pointer.html#method.wrapping_offset)
	///
	/// ## API Differences
	///
	/// `bitvec` makes it explicitly illegal to wrap a pointer around the high
	/// end of the address space, because it is incapable of representing a null
	/// pointer.
	///
	/// However, `<*T>::wrapping_offset` has additional properties as a result
	/// of its tolerance for wrapping the address space: it tolerates departing
	/// a provenance region, and is not unsafe to use to *create* a bit-pointer
	/// that is outside the bounds of its original provenance.
	///
	/// ## Safety
	///
	/// This function is safe to use because the bit-pointers it creates defer
	/// their provenance checks until the point of dereference. As such, you
	/// can safely use this to perform arbitrary pointer arithmetic that Rust
	/// considers illegal in ordinary arithmetic, as long as you do not
	/// dereference the bit-pointer until it has been brought in bounds of the
	/// originating provenance region.
	///
	/// This means that, to the Rust rule engine,
	/// `let z = x.wrapping_add(y as usize).wrapping_sub(x as usize);` is not
	/// equivalent to `y`, but `z` is safe to construct, and
	/// `z.wrapping_add(x as usize).wrapping_sub(y as usize)` produces a
	/// bit-pointer that *is* equivalent to `x`.
	///
	/// See the documentation of the original method for more details about
	/// provenance regions, and the distinctions that the optimizer makes about
	/// them.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let data = 0u32;
	/// let mut ptr = BitPtr::<_, _, Lsb0>::from_ref(&data);
	/// let end = ptr.wrapping_offset(32);
	/// while ptr < end {
	///   # #[cfg(feature = "std")] {
	///   println!("{}", unsafe { ptr.read() });
	///   # }
	///   ptr = ptr.wrapping_offset(3);
	/// }
	/// ```
	#[inline]
	#[must_use = "returns a new bit-pointer rather than modifying its argument"]
	pub fn wrapping_offset(self, count: isize) -> Self {
		let (elts, head) = self.bit.offset(count);
		unsafe { Self::new_unchecked(self.ptr.wrapping_offset(elts), head) }
	}

	/// Calculates the distance (in bits) between two bit-pointers.
	///
	/// This method is the inverse of [`.offset()`].
	///
	/// ## Original
	///
	/// [`pointer::offset_from`](https://doc.rust-lang.org/std/primitive.pointer.html#method.offset_from)
	///
	/// ## API Differences
	///
	/// The base pointer may have a different `BitStore` type parameter, as long
	/// as they share an underlying memory type. This is necessary in order to
	/// accommodate aliasing markers introduced between when an origin pointer
	/// was taken and when `self` compared against it.
	///
	/// ## Safety
	///
	/// Both `self` and `origin` **must** be drawn from the same provenance
	/// region. This means that they must be created from the same Rust
	/// allocation, whether with `let` or the allocator API, and must be in the
	/// (inclusive) range `base ..= base + len`. The first bit past the end of
	/// a region can be addressed, just not dereferenced.
	///
	/// See the original `<*T>::offset_from` for more details on region safety.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let data = 0u32;
	/// let base = BitPtr::<_, _, Lsb0>::from_ref(&data);
	/// let low = unsafe { base.add(10) };
	/// let high = unsafe { low.add(15) };
	/// unsafe {
	///   assert_eq!(high.offset_from(low), 15);
	///   assert_eq!(low.offset_from(high), -15);
	///   assert_eq!(low.offset(15), high);
	///   assert_eq!(high.offset(-15), low);
	/// }
	/// ```
	///
	/// While this method is safe to *construct* bit-pointers that depart a
	/// provenance region, it remains illegal to *dereference* those pointers!
	///
	/// This usage is incorrect, and a program that contains it is not
	/// well-formed.
	///
	/// ```rust,no_run
	/// use bitvec::prelude::*;
	///
	/// let a = 0u8;
	/// let b = !0u8;
	///
	/// let a_ptr = BitPtr::<_, _, Lsb0>::from_ref(&a);
	/// let b_ptr = BitPtr::<_, _, Lsb0>::from_ref(&b);
	/// let diff = (b_ptr.pointer() as isize)
	///   .wrapping_sub(a_ptr.pointer() as isize)
	///   // Remember: raw pointers are byte-stepped,
	///   // but bit-pointers are bit-stepped.
	///   .wrapping_mul(8);
	/// // This pointer to `b` has `a`’s provenance:
	/// let b_ptr_2 = a_ptr.wrapping_offset(diff);
	///
	/// // They are *arithmetically* equal:
	/// assert_eq!(b_ptr, b_ptr_2);
	/// // But it is still undefined behavior to cross provenances!
	/// assert_eq!(0, unsafe { b_ptr_2.offset_from(b_ptr) });
	/// ```
	///
	/// [`.offset()`]: Self::offset
	#[inline]
	pub unsafe fn offset_from<U>(self, origin: BitPtr<M, U, O>) -> isize
	where U: BitStore<Mem = T::Mem> {
		self.get_addr()
			.cast::<T::Mem>()
			.offset_from(origin.get_addr().cast::<T::Mem>())
			.wrapping_mul(mem::bits_of::<T::Mem>() as isize)
			.wrapping_add(self.bit.into_inner() as isize)
			.wrapping_sub(origin.bit.into_inner() as isize)
	}

	/// Adjusts a bit-pointer upwards in memory. This is equivalent to
	/// `.offset(count as isize)`.
	///
	/// `count` is in units of bits.
	///
	/// ## Original
	///
	/// [`pointer::add`](https://doc.rust-lang.org/std/primitive.pointer.html#method.add)
	///
	/// ## Safety
	///
	/// See [`.offset()`](Self::offset).
	#[inline]
	#[must_use = "returns a new bit-pointer rather than modifying its argument"]
	pub unsafe fn add(self, count: usize) -> Self {
		self.offset(count as isize)
	}

	/// Adjusts a bit-pointer downwards in memory. This is equivalent to
	/// `.offset((count as isize).wrapping_neg())`.
	///
	/// `count` is in units of bits.
	///
	/// ## Original
	///
	/// [`pointer::sub`](https://doc.rust-lang.org/std/primitive.pointer.html#method.sub)
	///
	/// ## Safety
	///
	/// See [`.offset()`](Self::offset).
	#[inline]
	#[must_use = "returns a new bit-pointer rather than modifying its argument"]
	pub unsafe fn sub(self, count: usize) -> Self {
		self.offset((count as isize).wrapping_neg())
	}

	/// Adjusts a bit-pointer upwards in memory, using wrapping semantics. This
	/// is equivalent to `.wrapping_offset(count as isize)`.
	///
	/// `count` is in units of bits.
	///
	/// ## Original
	///
	/// [`pointer::wrapping_add`](https://doc.rust-lang.org/std/primitive.pointer.html#method.wrapping_add)
	///
	/// ## Safety
	///
	/// See [`.wrapping_offset()`](Self::wrapping_offset).
	#[inline]
	#[must_use = "returns a new bit-pointer rather than modifying its argument"]
	pub fn wrapping_add(self, count: usize) -> Self {
		self.wrapping_offset(count as isize)
	}

	/// Adjusts a bit-pointer downwards in memory, using wrapping semantics.
	/// This is equivalent to
	/// `.wrapping_offset((count as isize).wrapping_neg())`.
	///
	/// `count` is in units of bits.
	///
	/// ## Original
	///
	/// [`pointer::wrapping_add`](https://doc.rust-lang.org/std/primitive.pointer.html#method.wrapping_add)
	///
	/// ## Safety
	///
	/// See [`.wrapping_offset()`](Self::wrapping_offset).
	#[inline]
	#[must_use = "returns a new bit-pointer rather than modifying its argument"]
	pub fn wrapping_sub(self, count: usize) -> Self {
		self.wrapping_offset((count as isize).wrapping_neg())
	}

	/// Reads the bit from `*self`.
	///
	/// ## Original
	///
	/// [`pointer::read`](https://doc.rust-lang.org/std/primitive.pointer.html#method.read)
	///
	/// ## Safety
	///
	/// See [`ptr::read`](crate::ptr::read).
	#[inline]
	pub unsafe fn read(self) -> bool {
		(*self.ptr.to_const()).load_value().get_bit::<O>(self.bit)
	}

	/// Reads the bit from `*self` using a volatile load.
	///
	/// Prefer using a crate such as [`voladdress`][0] to manage volatile I/O
	/// and use `bitvec` only on the local objects it provides. Individual I/O
	/// operations for individual bits are likely not the behavior you want.
	///
	/// ## Original
	///
	/// [`pointer::read_volatile`](https://doc.rust-lang.org/std/primitive.pointer.html#method.read_volatile)
	///
	/// ## Safety
	///
	/// See [`ptr::read_volatile`](crate::ptr::read_volatile).
	///
	/// [0]: https://docs.rs/voladdress/later/voladdress
	#[inline]
	pub unsafe fn read_volatile(self) -> bool {
		self.ptr.to_const().read_volatile().get_bit::<O>(self.bit)
	}

	/// Reads the bit from `*self` using an unaligned memory access.
	///
	/// `BitPtr` forbids unaligned addresses. If you have such an address, you
	/// must perform your memory accesses on the raw element, and only use
	/// `bitvec` on a well-aligned stack temporary. This method should never be
	/// necessary.
	///
	/// ## Original
	///
	/// [`pointer::read_unaligned`](https://doc.rust-lang.org/std/primitive.pointer.html#method.read_unaligned)
	///
	/// ## Safety
	///
	/// See [`ptr::read_unaligned`](crate::ptr::read_unaligned)
	#[inline]
	#[deprecated = "`BitPtr` does not have unaligned addresses"]
	pub unsafe fn read_unaligned(self) -> bool {
		self.ptr.to_const().read_unaligned().get_bit::<O>(self.bit)
	}

	/// Copies `count` bits from `self` to `dest`. The source and destination
	/// may overlap.
	///
	/// Note that overlap is only defined when `O` and `O2` are the same type.
	/// If they differ, then `bitvec` does not define overlap, and assumes that
	/// they are wholly discrete in memory.
	///
	/// ## Original
	///
	/// [`pointer::copy_to`](https://doc.rust-lang.org/std/primitive.pointer.html#method.copy_to)
	///
	/// ## Safety
	///
	/// See [`ptr::copy`](crate::ptr::copy).
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub unsafe fn copy_to<T2, O2>(self, dest: BitPtr<Mut, T2, O2>, count: usize)
	where
		T2: BitStore,
		O2: BitOrder,
	{
		super::copy(self.to_const(), dest, count);
	}

	/// Copies `count` bits from `self` to `dest`. The source and destination
	/// may *not* overlap.
	///
	/// ## Original
	///
	/// [`pointer::copy_to_nonoverlapping`](https://doc.rust-lang.org/std/primitive.pointer.html#method.copy_to_nonoverlapping)
	///
	/// ## Safety
	///
	/// See [`ptr::copy_nonoverlapping`](crate::ptr::copy_nonoverlapping).
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub unsafe fn copy_to_nonoverlapping<T2, O2>(
		self,
		dest: BitPtr<Mut, T2, O2>,
		count: usize,
	) where
		T2: BitStore,
		O2: BitOrder,
	{
		super::copy_nonoverlapping(self.to_const(), dest, count);
	}

	/// Computes the offset (in bits) that needs to be applied to the
	/// bit-pointer in order to make it aligned to the given *byte* alignment.
	///
	/// “Alignment” here means that the bit-pointer selects the starting bit of
	/// a memory location whose address satisfies the requested alignment.
	///
	/// `align` is measured in **bytes**. If you wish to align your bit-pointer
	/// to a specific fraction (½, ¼, or ⅛ of one byte), please file an issue
	/// and I will work on adding this functionality.
	///
	/// ## Original
	///
	/// [`pointer::align_offset`](https://doc.rust-lang.org/std/primitive.pointer.html#method.align_offset)
	///
	/// ## Notes
	///
	/// If the base-element address of the bit-pointer is already aligned to
	/// `align`, then this will return the bit-offset required to select the
	/// first bit of the successor element.
	///
	/// If it is not possible to align the bit-pointer, then the implementation
	/// returns `usize::MAX`.
	///
	/// The return value is measured in bits, not `T` elements or bytes. The
	/// only thing you can do with it is pass it into [`.add()`] or
	/// [`.wrapping_add()`].
	///
	/// Note from the standard library: It is permissible for the implementation
	/// to *always* return `usize::MAX`. Only your algorithm’s performance can
	/// depend on getting a usable offset here; it must be correct independently
	/// of this function providing a useful value.
	///
	/// ## Safety
	///
	/// There are no guarantees whatsoëver that offsetting the bit-pointer will
	/// not overflow or go beyond the allocation that the bit-pointer selects.
	/// It is up to the caller to ensure that the returned offset is correct in
	/// all terms other than alignment.
	///
	/// ## Panics
	///
	/// This method panics if `align` is not a power of two.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let data = [0u8; 3];
	/// let ptr = BitPtr::<_, _, Lsb0>::from_slice(&data);
	/// let ptr = unsafe { ptr.add(2) };
	/// let count = ptr.align_offset(2);
	/// assert!(count >= 6);
	/// ```
	///
	/// [`.add()`]: Self::add
	/// [`.wrapping_add()`]: Self::wrapping_add
	#[inline]
	pub fn align_offset(self, align: usize) -> usize {
		let width = mem::bits_of::<T::Mem>();
		match (
			self.ptr.to_const().align_offset(align),
			self.bit.into_inner() as usize,
		) {
			(0, 0) => 0,
			(0, head) => align * mem::bits_of::<u8>() - head,
			(usize::MAX, _) => usize::MAX,
			(elts, head) => elts.wrapping_mul(width).wrapping_sub(head),
		}
	}
}

/// Port of the `*mut bool` inherent API.
impl<T, O> BitPtr<Mut, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Produces a proxy reference to the referent bit.
	///
	/// Because `BitPtr` guarantees that it is non-null and well-aligned, this
	/// never returns `None`. However, this is still unsafe to call on any
	/// bit-pointers created from conjured values rather than known references.
	///
	/// ## Original
	///
	/// [`pointer::as_mut`](https://doc.rust-lang.org/std/primitive.pointer.html#method.as_mut)
	///
	/// ## API Differences
	///
	/// This produces a proxy type rather than a true reference. The proxy
	/// implements `DerefMut<Target = bool>`, and can be converted to
	/// `&mut bool` with a reborrow `&mut *`.
	///
	/// Writes to the proxy are not reflected in the proxied location until the
	/// proxy is destroyed, either through `Drop` or its [`.commit()`] method.
	///
	/// ## Safety
	///
	/// Since `BitPtr` does not permit null or misaligned pointers, this method
	/// will always dereference the pointer in order to create the proxy. As
	/// such, you must ensure the following conditions are met:
	///
	/// - the pointer must be dereferenceable as defined in the standard library
	///   documentation
	/// - the pointer must point to an initialized instance of `T`
	/// - you must ensure that no other pointer will race to modify the referent
	///   location while this call is reading from memory to produce the proxy
	/// - you must ensure that no other `bitvec` handle targets the referent bit
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let mut data = 0u8;
	/// let ptr = BitPtr::<_, _, Lsb0>::from_mut(&mut data);
	/// let mut val = unsafe { ptr.as_mut() }.unwrap();
	/// assert!(!*val);
	/// *val = true;
	/// assert!(*val);
	/// ```
	///
	/// [`.commit()`]: crate::ptr::BitRef::commit
	#[inline]
	pub unsafe fn as_mut<'a>(self) -> Option<BitRef<'a, Mut, T, O>> {
		Some(BitRef::from_bitptr(self))
	}

	/// Copies `count` bits from the region starting at `src` to the region
	/// starting at `self`.
	///
	/// The regions are free to overlap; the implementation will detect overlap
	/// and correctly avoid it.
	///
	/// Note: this has the *opposite* argument order from [`ptr::copy`]: `self`
	/// is the destination, not the source.
	///
	/// ## Original
	///
	/// [`pointer::copy_from`](https://doc.rust-lang.org/std/primitive.pointer.html#method.copy_from)
	///
	/// ## Safety
	///
	/// See [`ptr::copy`].
	///
	/// [`ptr::copy`]: crate::ptr::copy
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub unsafe fn copy_from<T2, O2>(
		self,
		src: BitPtr<Const, T2, O2>,
		count: usize,
	) where
		T2: BitStore,
		O2: BitOrder,
	{
		src.copy_to(self, count);
	}

	/// Copies `count` bits from the region starting at `src` to the region
	/// starting at `self`.
	///
	/// Unlike [`.copy_from()`], the two regions may *not* overlap; this method
	/// does not attempt to detect overlap and thus may have a slight
	/// performance boost over the overlap-handling `.copy_from()`.
	///
	/// Note: this has the *opposite* argument order from
	/// [`ptr::copy_nonoverlapping`]: `self` is the destination, not the source.
	///
	/// ## Original
	///
	/// [`pointer::copy_from_nonoverlapping`](https://doc.rust-lang.org/std/primitive.pointer.html#method.copy_from_nonoverlapping)
	///
	/// ## Safety
	///
	/// See [`ptr::copy_nonoverlapping`].
	///
	/// [`.copy_from()`]: Self::copy_from
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub unsafe fn copy_from_nonoverlapping<T2, O2>(
		self,
		src: BitPtr<Const, T2, O2>,
		count: usize,
	) where
		T2: BitStore,
		O2: BitOrder,
	{
		src.copy_to_nonoverlapping(self, count);
	}

	/// Runs the destructor of the referent value.
	///
	/// `bool` has no destructor; this function does nothing.
	///
	/// ## Original
	///
	/// [`pointer::drop_in_place`](https://doc.rust-lang.org/std/primitive.pointer.html#method.drop_in_place)
	///
	/// ## Safety
	///
	/// See [`ptr::drop_in_place`].
	///
	/// [`ptr::drop_in_place`]: crate::ptr::drop_in_place
	#[inline]
	#[deprecated = "this has no effect, and should not be called"]
	pub fn drop_in_place(self) {}

	/// Writes a new bit into the given location.
	///
	/// ## Original
	///
	/// [`pointer::write`](https://doc.rust-lang.org/std/primitive.pointer.html#method.write)
	///
	/// ## Safety
	///
	/// See [`ptr::write`].
	///
	/// [`ptr::write`]: crate::ptr::write
	#[inline]
	pub unsafe fn write(self, value: bool) {
		self.replace(value);
	}

	/// Writes a new bit using volatile I/O operations.
	///
	/// Because processors do not generally have single-bit read or write
	/// instructions, this must perform a volatile read of the entire memory
	/// location, perform the write locally, then perform another volatile write
	/// to the entire location. These three steps are guaranteed to be
	/// sequential with respect to each other, but are not guaranteed to be
	/// atomic.
	///
	/// Volatile operations are intended to act on I/O memory, and are *only*
	/// guaranteed not to be elided or reördered by the compiler across other
	/// I/O operations.
	///
	/// You should not use `bitvec` to act on volatile memory. You should use a
	/// crate specialized for volatile I/O work, such as [`voladdr`], and use it
	/// to explicitly manage the I/O and ask it to perform `bitvec` work only on
	/// the local snapshot of a volatile location.
	///
	/// ## Original
	///
	/// [`pointer::write_volatile`](https://doc.rust-lang.org/std/primitive.pointer.html#method.write_volatile)
	///
	/// ## Safety
	///
	/// See [`ptr::write_volatile`].
	///
	/// [`ptr::write_volatile`]: crate::ptr::write_volatile
	/// [`voladdr`]: https://docs.rs/voladdr/latest/voladdr
	#[inline]
	#[allow(clippy::needless_borrow)] // Clippy is wrong.
	pub unsafe fn write_volatile(self, value: bool) {
		let ptr = self.ptr.to_mut();
		let mut tmp = ptr.read_volatile();
		Self::new_unchecked((&mut tmp).into(), self.bit).write(value);
		ptr.write_volatile(tmp);
	}

	/// Writes a bit into memory, tolerating unaligned addresses.
	///
	/// `BitPtr` does not have unaligned addresses. `BitPtr` itself is capable
	/// of operating on misaligned addresses, but elects to disallow use of them
	/// in keeping with the rest of `bitvec`’s requirements.
	///
	/// ## Original
	///
	/// [`pointer::write_unaligned`](https://doc.rust-lang.org/std/primitive.pointer.html#method.write_unaligned)
	///
	/// ## Safety
	///
	/// See [`ptr::write_unaligned`].
	///
	/// [`ptr::write_unaligned`]: crate::ptr::write_unaligned
	#[inline]
	#[allow(clippy::needless_borrow)] // Clippy is wrong.
	#[deprecated = "`BitPtr` does not have unaligned addresses"]
	pub unsafe fn write_unaligned(self, value: bool) {
		let ptr = self.ptr.to_mut();
		let mut tmp = ptr.read_unaligned();
		Self::new_unchecked((&mut tmp).into(), self.bit).write(value);
		ptr.write_unaligned(tmp);
	}

	/// Replaces the bit at `*self` with a new value, returning the previous
	/// value.
	///
	/// ## Original
	///
	/// [`pointer::replace`](https://doc.rust-lang.org/std/primitive.pointer.html#method.replace)
	///
	/// ## Safety
	///
	/// See [`ptr::replace`].
	///
	/// [`ptr::replace`]: crate::ptr::replace
	#[inline]
	pub unsafe fn replace(self, value: bool) -> bool {
		self.freeze().frozen_write_bit(value)
	}

	/// Swaps the bits at two mutable locations.
	///
	/// ## Original
	///
	/// [`pointer::swap`](https://doc.rust-lang.org/std/primitive.pointer.html#method.swap)
	///
	/// ## Safety
	///
	/// See [`ptr::swap`].
	///
	/// [`ptr::swap`]: crate::ptr::swap
	#[inline]
	pub unsafe fn swap<T2, O2>(self, with: BitPtr<Mut, T2, O2>)
	where
		T2: BitStore,
		O2: BitOrder,
	{
		self.write(with.replace(self.read()));
	}
}

impl<M, T, O> BitPtr<Frozen<M>, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// Writes through a bit-pointer that has had its mutability permission
	/// removed.
	///
	/// This is used to allow `BitPtr<Const, _, AliasSafe<T>>` pointers, which
	/// are not `Mut` but may still modify memory, to do so.
	pub(crate) unsafe fn frozen_write_bit(self, value: bool) -> bool {
		(*self.ptr.cast::<T::Access>().to_const())
			.write_bit::<O>(self.bit, value)
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Clone for BitPtr<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn clone(&self) -> Self {
		Self {
			ptr: self.get_addr(),
			..*self
		}
	}
}

impl<M, T, O> Eq for BitPtr<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
}

impl<M, T, O> Ord for BitPtr<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn cmp(&self, other: &Self) -> cmp::Ordering {
		self.partial_cmp(other).expect(
			"BitPtr has a total ordering when type parameters are identical",
		)
	}
}

impl<M1, M2, T1, T2, O> PartialEq<BitPtr<M2, T2, O>> for BitPtr<M1, T1, O>
where
	M1: Mutability,
	M2: Mutability,
	T1: BitStore,
	T2: BitStore,
	O: BitOrder,
{
	#[inline]
	fn eq(&self, other: &BitPtr<M2, T2, O>) -> bool {
		if !dvl::match_store::<T1::Mem, T2::Mem>() {
			return false;
		}
		self.get_addr().to_const() as usize
			== other.get_addr().to_const() as usize
			&& self.bit.into_inner() == other.bit.into_inner()
	}
}

impl<M1, M2, T1, T2, O> PartialOrd<BitPtr<M2, T2, O>> for BitPtr<M1, T1, O>
where
	M1: Mutability,
	M2: Mutability,
	T1: BitStore,
	T2: BitStore,
	O: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, other: &BitPtr<M2, T2, O>) -> Option<cmp::Ordering> {
		if !dvl::match_store::<T1::Mem, T2::Mem>() {
			return None;
		}
		match (self.get_addr().to_const() as usize)
			.cmp(&(other.get_addr().to_const() as usize))
		{
			cmp::Ordering::Equal => {
				self.bit.into_inner().partial_cmp(&other.bit.into_inner())
			},
			ord => Some(ord),
		}
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> From<&T> for BitPtr<Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(elem: &T) -> Self {
		Self::from_ref(elem)
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> From<&mut T> for BitPtr<Mut, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(elem: &mut T) -> Self {
		Self::from_mut(elem)
	}
}

impl<T, O> TryFrom<*const T> for BitPtr<Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Error = BitPtrError<T>;

	#[inline]
	fn try_from(elem: *const T) -> Result<Self, Self::Error> {
		elem.try_conv::<Address<Const, T>>()?
			.pipe(|ptr| Self::new(ptr, BitIdx::MIN))?
			.pipe(Ok)
	}
}

impl<T, O> TryFrom<*mut T> for BitPtr<Mut, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Error = BitPtrError<T>;

	#[inline]
	fn try_from(elem: *mut T) -> Result<Self, Self::Error> {
		elem.try_conv::<Address<Mut, T>>()?
			.pipe(|ptr| Self::new(ptr, BitIdx::MIN))?
			.pipe(Ok)
	}
}

impl<M, T, O> Debug for BitPtr<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		write!(
			fmt,
			"{} Bit<{}, {}>",
			M::RENDER,
			any::type_name::<T>(),
			any::type_name::<O>(),
		)?;
		Pointer::fmt(self, fmt)
	}
}

impl<M, T, O> Pointer for BitPtr<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.debug_tuple("")
			.field(&self.get_addr().fmt_pointer())
			.field(&self.bit.fmt_binary())
			.finish()
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Hash for BitPtr<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn hash<H>(&self, state: &mut H)
	where H: Hasher {
		self.get_addr().hash(state);
		self.bit.hash(state);
	}
}

impl<M, T, O> Copy for BitPtr<M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
}

/// Errors produced by invalid bit-pointer components.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum BitPtrError<T>
where T: BitStore
{
	/// Attempted to construct a bit-pointer with the null element address.
	Null(NullPtrError),
	/// Attempted to construct a bit-pointer with an address not aligned for the
	/// element type.
	Misaligned(MisalignError<T>),
}

#[cfg(not(tarpaulin_include))]
impl<T> From<MisalignError<T>> for BitPtrError<T>
where T: BitStore
{
	#[inline]
	fn from(err: MisalignError<T>) -> Self {
		Self::Misaligned(err)
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> From<NullPtrError> for BitPtrError<T>
where T: BitStore
{
	#[inline]
	fn from(err: NullPtrError) -> Self {
		Self::Null(err)
	}
}

#[cfg(not(tarpaulin_include))]
impl<T> Display for BitPtrError<T>
where T: BitStore
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		match self {
			Self::Null(err) => Display::fmt(err, fmt),
			Self::Misaligned(err) => Display::fmt(err, fmt),
		}
	}
}

#[cfg(feature = "std")]
impl<T> std::error::Error for BitPtrError<T> where T: BitStore {}
