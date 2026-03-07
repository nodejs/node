//! Port of the `Box<[T]>` inherent API.

use core::mem;

use tap::Tap;

use super::BitBox;
use crate::{
	order::BitOrder,
	ptr::BitSpan,
	slice::BitSlice,
	store::BitStore,
	vec::BitVec,
};

impl<T, O> BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Constructs a bit-box from a raw bit-slice pointer.
	///
	/// This converts a `*mut BitSlice` pointer that had previously been
	/// produced by either [`::into_raw()`] or [`::leak()`] and restores the
	/// bit-box containing it.
	///
	/// ## Original
	///
	/// [`Box::from_raw`](alloc::boxed::Box::from_raw)
	///
	/// ## Safety
	///
	/// You must only call this function on pointers produced by leaking a prior
	/// `BitBox`; you may not modify the value of a pointer returned by
	/// [`::into_raw()`], nor may you conjure pointer values of your own. Doing
	/// so will corrupt the allocator state.
	///
	/// You must only call this function on any given leaked pointer at most
	/// once. Not calling it at all will merely render the allocated memory
	/// unreachable for the duration of the program runtime, a normal (and safe)
	/// memory leak. Calling it once restores ordinary functionality, and
	/// ensures ordinary destruction at or before program termination. However,
	/// calling it more than once on the same pointer will introduce data races,
	/// use-after-free, and/or double-free errors.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bb = bitbox![0; 80];
	/// let ptr: *mut BitSlice = BitBox::into_raw(bb);
	/// let bb = unsafe { BitBox::from_raw(ptr) };
	/// // unsafe { BitBox::from_raw(ptr) }; // UAF crash!
	/// ```
	///
	/// [`::into_raw()`]: Self::into_raw
	/// [`::leak()`]: Self::leak
	#[inline]
	pub unsafe fn from_raw(raw: *mut BitSlice<T, O>) -> Self {
		Self {
			bitspan: BitSpan::from_bitslice_ptr_mut(raw),
		}
	}

	/// Consumes the bit-box, returning a raw bit-slice pointer.
	///
	/// Bit-slice pointers are always correctly encoded and non-null. The
	/// referent region is dereferenceäble *as a `BitSlice` for the remainder of
	/// the program, or until it is first passed to [`::from_raw()`], whichever
	/// comes first. Once the pointer is first passed to `::from_raw()`, all
	/// copies of that pointer become invalid to dereference.
	///
	/// ## Original
	///
	/// [`Box::into_raw`](alloc::boxed::Box::into_raw)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bb = bitbox![0; 80];
	/// let ptr = BitBox::into_raw(bb);
	/// let bb = unsafe { BitBox::from_raw(ptr) };
	/// ```
	///
	/// You **may not** deällocate pointers produced by this function through
	/// any other means.
	///
	/// [`::from_raw()`]: Self::from_raw
	#[inline]
	pub fn into_raw(this: Self) -> *mut BitSlice<T, O> {
		Self::leak(this)
	}

	/// Deliberately leaks the allocated memory, returning an
	/// `&'static mut BitSlice` reference.
	///
	/// This differs from [`::into_raw()`] in that the reference is safe to use
	/// and can be tracked by the Rust borrow-checking system. Like the
	/// bit-slice pointer produced by `::into_raw()`, this reference can be
	/// un-leaked by passing it into [`::from_raw()`] to reclaim the memory.
	///
	/// ## Original
	///
	/// [`Box::leak`](alloc::boxed::Box::leak)
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bb = bitbox![0; 80];
	/// let static_ref: &'static mut BitSlice = BitBox::leak(bb);
	///
	/// static_ref.set(0, true);
	/// assert!(static_ref[0]);
	/// let _ = unsafe {
	///   BitBox::from_raw(static_ref)
	/// };
	/// ```
	///
	/// [`::from_raw()`]: Self::from_raw
	/// [`::into_raw()`]: Self::into_raw
	#[inline]
	pub fn leak<'a>(this: Self) -> &'a mut BitSlice<T, O>
	where T: 'a {
		unsafe { this.bitspan.into_bitslice_mut() }.tap(|_| mem::forget(this))
	}

	#[inline]
	#[doc(hidden)]
	#[cfg(not(tarpaulin_include))]
	#[deprecated = "use `.into_bitvec()` instead"]
	pub fn into_vec(self) -> BitVec<T, O> {
		self.into_bitvec()
	}
}
