//! Support utilities for crate development.

use core::any::TypeId;

use crate::{
	order::BitOrder,
	store::BitStore,
};

/// Constructs formatting-trait implementations by delegating.
macro_rules! easy_fmt {
	($(impl $fmt:ident)+ for BitArray) => { $(
		impl<A, O> core::fmt::$fmt for $crate::array::BitArray<A, O>
		where
			O: $crate::order::BitOrder,
			A: $crate::view::BitViewSized,
		{
			#[inline]
			#[cfg(not(tarpaulin_include))]
			fn fmt(&self, fmt: &mut core::fmt::Formatter) -> core::fmt::Result {
				core::fmt::$fmt::fmt(self.as_bitslice(), fmt)
			}
		}
	)+ };
	($(impl $fmt:ident)+ for $this:ident) => { $(
		impl<T, O> core::fmt::$fmt for $this<T, O>
		where
			O: $crate::order::BitOrder,
			T: $crate::store::BitStore,
		{
			#[inline]
			#[cfg(not(tarpaulin_include))]
			fn fmt(&self, fmt: &mut core::fmt::Formatter) -> core::fmt::Result {
				core::fmt::$fmt::fmt(self.as_bitslice(), fmt)
			}
		}
	)+ };
}

/// Implements some `Iterator` functions that have boilerplate behavior.
macro_rules! easy_iter {
	() => {
		#[inline]
		fn size_hint(&self) -> (usize, Option<usize>) {
			let len = self.len();
			(len, Some(len))
		}

		#[inline]
		fn count(self) -> usize {
			self.len()
		}

		#[inline]
		fn last(mut self) -> Option<Self::Item> {
			self.next_back()
		}
	};
}

/// Tests if two `BitOrder` implementors are the same.
#[inline]
pub fn match_order<O, P>() -> bool
where
	O: BitOrder,
	P: BitOrder,
{
	eq_types::<O, P>()
}

/// Tests if two `BitStore` implementors are the same.
#[inline]
pub fn match_store<T, U>() -> bool
where
	T: BitStore,
	U: BitStore,
{
	eq_types::<T, U>()
}

/// Tests if two `BitSlice` type parameter pairs match each other.
#[inline]
pub fn match_types<T1, O1, T2, O2>() -> bool
where
	O1: BitOrder,
	T1: BitStore,
	O2: BitOrder,
	T2: BitStore,
{
	match_order::<O1, O2>() && match_store::<T1, T2>()
}

/// Tests if a type is known to be an unsigned integer.
///
/// Returns `true` for `u{8,16,32,64,128,size}` and `false` for all others.
#[inline]
pub fn is_unsigned<T>() -> bool
where T: 'static {
	eq_types::<T, u8>()
		|| eq_types::<T, u16>()
		|| eq_types::<T, u32>()
		|| eq_types::<T, u64>()
		|| eq_types::<T, u128>()
		|| eq_types::<T, usize>()
}

/// Tests if two types are identical, even through different names.
#[inline]
fn eq_types<T, U>() -> bool
where
	T: 'static,
	U: 'static,
{
	TypeId::of::<T>() == TypeId::of::<U>()
}
