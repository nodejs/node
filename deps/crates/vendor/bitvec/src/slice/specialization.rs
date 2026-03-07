#![doc = include_str!("../../doc/slice/specialization.md")]

use funty::Integral;

use super::BitSlice;
use crate::{
	devel as dvl,
	mem,
	order::BitOrder,
	store::BitStore,
};

mod lsb0;
mod msb0;

/// Processor width, used for chunking.
const WORD_BITS: usize = mem::bits_of::<usize>();

/// Tests whether the masked portion of an integer has a `0` bit in it.
fn has_zero<T>(val: T, mask: T) -> bool
where T: Integral {
	val | !mask != !T::ZERO
}

/// Tests whether the masked portion of an integer has a `1` bit in it.
fn has_one<T>(val: T, mask: T) -> bool
where T: Integral {
	val & mask != T::ZERO
}

impl<T, O> BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	/// Forces the storage type parameter to be its accessor type.
	///
	/// Functions must use this when working with maybe-overlapping regions
	/// within a single bit-slice, as the accessor is always tolerant of
	/// aliasing.
	#[inline]
	fn as_accessor(&mut self) -> &BitSlice<T::Access, O> {
		unsafe { &*(self as *const Self as *const BitSlice<T::Access, O>) }
	}

	/// Attempts to change a bit-slice reference to caller-supplied type
	/// parameters.
	///
	/// If `<T, O>` is identical to `<T2, O2>`, this returns `Some` with the
	/// bit-slice reference unchanged in value but changed in type. If the types
	/// differ, it returns `None`. This is useful for creating statically-known
	/// bit-slice types within generic contexts.
	pub(crate) fn coerce<T2, O2>(&self) -> Option<&BitSlice<T2, O2>>
	where
		T2: BitStore,
		O2: BitOrder,
	{
		if dvl::match_types::<T, O, T2, O2>() {
			Some(unsafe { &*(self as *const Self as *const BitSlice<T2, O2>) })
		}
		else {
			None
		}
	}

	/// See [`.coerce()`].
	///
	/// [`.coerce()`]: Self::coerce
	pub(crate) fn coerce_mut<T2, O2>(&mut self) -> Option<&mut BitSlice<T2, O2>>
	where
		T2: BitStore,
		O2: BitOrder,
	{
		if dvl::match_types::<T, O, T2, O2>() {
			Some(unsafe { &mut *(self as *mut Self as *mut BitSlice<T2, O2>) })
		}
		else {
			None
		}
	}
}
