#![doc = include_str!("../../doc/ptr/proxy.md")]

use core::{
	cell::UnsafeCell,
	cmp,
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
	mem,
	ops::{
		Deref,
		DerefMut,
		Not,
	},
};

use wyz::comu::{
	Const,
	Mut,
	Mutability,
};

use super::BitPtr;
use crate::{
	order::{
		BitOrder,
		Lsb0,
	},
	store::BitStore,
};

#[doc = include_str!("../../doc/ptr/BitRef.md")]
//  Restore alignment and sizing properties, as `BitPtr` lacks them.
#[cfg_attr(target_pointer_width = "32", repr(C, align(4)))]
#[cfg_attr(target_pointer_width = "64", repr(C, align(8)))]
#[cfg_attr(
	not(any(target_pointer_width = "32", target_pointer_width = "64")),
	repr(C)
)]
pub struct BitRef<'a, M = Const, T = usize, O = Lsb0>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// The proxied bit-address.
	bitptr: BitPtr<M, T, O>,
	/// A local cache of the proxied bit that can be referenced.
	data:   bool,
	/// Attach the lifetime and reflect the possibility of mutation.
	_ref:   PhantomData<&'a UnsafeCell<bool>>,
}

impl<M, T, O> BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	/// Converts a bit-pointer into a proxy bit-reference.
	///
	/// This reads through the pointer in order to cache the current bit value
	/// in the proxy.
	///
	/// ## Original
	///
	/// The syntax `unsafe { &* ptr }`.
	///
	/// ## Safety
	///
	/// This is equivalent to (and is!) dereferencing a raw pointer. The pointer
	/// must be well-constructed, refer to a live memory location in the program
	/// context, and not be aliased beyond its typing indicators.
	#[inline]
	pub unsafe fn from_bitptr(bitptr: BitPtr<M, T, O>) -> Self {
		let data = bitptr.read();
		Self {
			bitptr,
			data,
			_ref: PhantomData,
		}
	}

	/// Decays the bit-reference to an ordinary bit-pointer.
	///
	/// ## Original
	///
	/// The syntax `&val as *T`.
	#[inline]
	#[cfg(not(tarpaulin_include))]
	pub fn into_bitptr(self) -> BitPtr<M, T, O> {
		self.bitptr
	}

	/// Removes a layer of `::Alias` marking from a bit-reference.
	///
	/// ## Safety
	///
	/// The caller must ensure that no element-level aliasing *by `bitvec`*
	/// occurs in the scope for which the produced de-aliased proxy is alive.
	#[cfg(not(tarpaulin_include))]
	pub(crate) unsafe fn remove_alias(this: BitRef<M, T::Alias, O>) -> Self {
		Self {
			bitptr: this.bitptr.cast::<T>(),
			data:   this.data,
			_ref:   PhantomData,
		}
	}
}

impl<T, O> BitRef<'_, Mut, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Moves `src` into the referenced bit, returning the previous value.
	///
	/// ## Original
	///
	/// [`mem::replace`](core::mem::replace)
	#[inline]
	pub fn replace(&mut self, src: bool) -> bool {
		mem::replace(&mut self.data, src)
	}

	/// Swaps the bit values of two proxies.
	///
	/// ## Original
	///
	/// [`mem::swap`](core::mem::swap)
	#[inline]
	pub fn swap<T2, O2>(&mut self, other: &mut BitRef<Mut, T2, O2>)
	where
		T2: BitStore,
		O2: BitOrder,
	{
		mem::swap(&mut self.data, &mut other.data)
	}

	/// Commits a bit into the proxied location.
	///
	/// This function writes `value` directly into the proxied location,
	/// bypassing the cache and destroying the proxy. This eliminates the second
	/// write done in the destructor, and allows code to be slightly faster.
	#[inline]
	pub fn commit(self, value: bool) {
		unsafe {
			self.bitptr.write(value);
		}
		mem::forget(self);
	}

	/// Writes `value` into the proxy.
	///
	/// This does not write into the proxied location; that is deferred until
	/// the proxy destructor runs.
	#[inline]
	pub fn set(&mut self, value: bool) {
		self.data = value;
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> Clone for BitRef<'_, Const, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn clone(&self) -> Self {
		Self { ..*self }
	}
}

impl<M, T, O> Eq for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Ord for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn cmp(&self, other: &Self) -> cmp::Ordering {
		self.data.cmp(&other.data)
	}
}

#[cfg(not(tarpaulin_include))]
impl<M1, M2, O1, O2, T1, T2> PartialEq<BitRef<'_, M2, T2, O2>>
	for BitRef<'_, M1, T1, O1>
where
	M1: Mutability,
	M2: Mutability,
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline(always)]
	fn eq(&self, other: &BitRef<'_, M2, T2, O2>) -> bool {
		self.data == other.data
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> PartialEq<bool> for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline(always)]
	fn eq(&self, other: &bool) -> bool {
		self.data == *other
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> PartialEq<BitRef<'_, M, T, O>> for bool
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn eq(&self, other: &BitRef<'_, M, T, O>) -> bool {
		other == self
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> PartialEq<&bool> for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline(always)]
	fn eq(&self, other: &&bool) -> bool {
		self.data == **other
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> PartialEq<BitRef<'_, M, T, O>> for &bool
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn eq(&self, other: &BitRef<'_, M, T, O>) -> bool {
		other == *self
	}
}

#[cfg(not(tarpaulin_include))]
impl<M1, M2, O1, O2, T1, T2> PartialOrd<BitRef<'_, M2, T2, O2>>
	for BitRef<'_, M1, T1, O1>
where
	M1: Mutability,
	M2: Mutability,
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn partial_cmp(
		&self,
		other: &BitRef<'_, M2, T2, O2>,
	) -> Option<cmp::Ordering> {
		self.data.partial_cmp(&other.data)
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> PartialOrd<bool> for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, other: &bool) -> Option<cmp::Ordering> {
		self.data.partial_cmp(other)
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> PartialOrd<&bool> for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, other: &&bool) -> Option<cmp::Ordering> {
		self.data.partial_cmp(*other)
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> AsRef<bool> for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_ref(&self) -> &bool {
		&self.data
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> AsMut<bool> for BitRef<'_, Mut, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_mut(&mut self) -> &mut bool {
		&mut self.data
	}
}

impl<M, T, O> Debug for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		unsafe { self.bitptr.span_unchecked(1) }
			.render(fmt, "Ref", &[("bit", &self.data as &dyn Debug)])
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Display for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		Display::fmt(&self.data, fmt)
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Pointer for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		Pointer::fmt(&self.bitptr, fmt)
	}
}

#[cfg(not(tarpaulin_include))]
impl<M, T, O> Hash for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn hash<H>(&self, state: &mut H)
	where H: Hasher {
		self.bitptr.hash(state);
	}
}

// #[allow(clippy::non_send_fields_in_send_ty)] // I know what I’m doing
unsafe impl<M, T, O> Send for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore + Sync,
	O: BitOrder,
{
}

unsafe impl<M, T, O> Sync for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore + Sync,
	O: BitOrder,
{
}

// This cannot be implemented until `Drop` is specialized to only
// `<Mut, T, O>`.
// impl<T, O> Copy for BitRef<'_, Const, T, O>
// where O: BitOrder, T: BitStore {}

impl<M, T, O> Deref for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	type Target = bool;

	#[inline]
	fn deref(&self) -> &Self::Target {
		&self.data
	}
}

impl<T, O> DerefMut for BitRef<'_, Mut, T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn deref_mut(&mut self) -> &mut Self::Target {
		&mut self.data
	}
}

impl<M, T, O> Drop for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn drop(&mut self) {
		//  `Drop` cannot specialize on type parameters, but only mutable
		//  proxies can commit to memory.
		if M::CONTAINS_MUTABILITY {
			unsafe {
				self.bitptr.to_mut().write(self.data);
			}
		}
	}
}

impl<M, T, O> Not for BitRef<'_, M, T, O>
where
	M: Mutability,
	T: BitStore,
	O: BitOrder,
{
	type Output = bool;

	#[inline]
	fn not(self) -> Self::Output {
		!self.data
	}
}
