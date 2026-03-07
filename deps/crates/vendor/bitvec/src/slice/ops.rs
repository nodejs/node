#![doc = include_str!("../../doc/slice/ops.md")]

use core::ops::{
	BitAnd,
	BitAndAssign,
	BitOr,
	BitOrAssign,
	BitXor,
	BitXorAssign,
	Index,
	IndexMut,
	Not,
	Range,
	RangeFrom,
	RangeFull,
	RangeInclusive,
	RangeTo,
	RangeToInclusive,
};

use super::{
	BitSlice,
	BitSliceIndex,
};
use crate::{
	domain::Domain,
	order::{
		BitOrder,
		Lsb0,
		Msb0,
	},
	store::BitStore,
};

impl<T1, T2, O1, O2> BitAndAssign<&BitSlice<T2, O2>> for BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	#[doc = include_str!("../../doc/slice/bitop_assign.md")]
	fn bitand_assign(&mut self, rhs: &BitSlice<T2, O2>) {
		if let (Some(this), Some(that)) =
			(self.coerce_mut::<T1, Lsb0>(), rhs.coerce::<T1, Lsb0>())
		{
			return this.sp_bitop_assign(that, BitAnd::bitand, BitAnd::bitand);
		}
		if let (Some(this), Some(that)) =
			(self.coerce_mut::<T1, Msb0>(), rhs.coerce::<T1, Msb0>())
		{
			return this.sp_bitop_assign(that, BitAnd::bitand, BitAnd::bitand);
		}
		for (this, that) in self.as_mut_bitptr_range().zip(rhs.as_bitptr_range())
		{
			unsafe {
				this.write(this.read() & that.read());
			}
		}
		if let Some(rem) = self.get_mut(rhs.len() ..) {
			rem.fill(false);
		}
	}
}

impl<T1, T2, O1, O2> BitOrAssign<&BitSlice<T2, O2>> for BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	#[doc = include_str!("../../doc/slice/bitop_assign.md")]
	fn bitor_assign(&mut self, rhs: &BitSlice<T2, O2>) {
		if let (Some(this), Some(that)) =
			(self.coerce_mut::<T1, Lsb0>(), rhs.coerce::<T1, Lsb0>())
		{
			return this.sp_bitop_assign(that, BitOr::bitor, BitOr::bitor);
		}
		if let (Some(this), Some(that)) =
			(self.coerce_mut::<T1, Msb0>(), rhs.coerce::<T1, Msb0>())
		{
			return this.sp_bitop_assign(that, BitOr::bitor, BitOr::bitor);
		}
		for (this, that) in self.as_mut_bitptr_range().zip(rhs.as_bitptr_range())
		{
			unsafe {
				this.write(this.read() | that.read());
			}
		}
	}
}

impl<T1, T2, O1, O2> BitXorAssign<&BitSlice<T2, O2>> for BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	#[doc = include_str!("../../doc/slice/bitop_assign.md")]
	fn bitxor_assign(&mut self, rhs: &BitSlice<T2, O2>) {
		if let (Some(this), Some(that)) =
			(self.coerce_mut::<T1, Lsb0>(), rhs.coerce::<T1, Lsb0>())
		{
			return this.sp_bitop_assign(that, BitXor::bitxor, BitXor::bitxor);
		}
		if let (Some(this), Some(that)) =
			(self.coerce_mut::<T1, Msb0>(), rhs.coerce::<T1, Msb0>())
		{
			return this.sp_bitop_assign(that, BitXor::bitxor, BitXor::bitxor);
		}
		for (this, that) in self.as_mut_bitptr_range().zip(rhs.as_bitptr_range())
		{
			unsafe {
				this.write(this.read() ^ that.read());
			}
		}
	}
}

impl<T, O> Index<usize> for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Output = bool;

	/// Looks up a single bit by its semantic index.
	///
	/// ## Examples
	///
	/// ```rust
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![u8, Msb0; 0, 1, 0];
	/// assert!(!bits[0]); // -----^  |  |
	/// assert!( bits[1]); // --------^  |
	/// assert!(!bits[2]); // -----------^
	/// ```
	///
	/// If the index is greater than or equal to the length, indexing will
	/// panic.
	///
	/// The below test will panic when accessing index 1, as only index 0 is
	/// valid.
	///
	/// ```rust,should_panic
	/// use bitvec::prelude::*;
	///
	/// let bits = bits![0,  ];
	/// bits[1]; // --------^
	/// ```
	#[inline]
	fn index(&self, index: usize) -> &Self::Output {
		match *index.index(self) {
			true => &true,
			false => &false,
		}
	}
}

/// Implements `Index` and `IndexMut` with the given type.
macro_rules! index {
	($($t:ty),+ $(,)?) => { $(
		impl<T, O> Index<$t> for BitSlice<T, O>
		where
			O: BitOrder,
			T: BitStore,
		{
			type Output = Self;

			#[inline]
			#[track_caller]
			fn index(&self, index: $t) -> &Self::Output {
				index.index(self)
			}
		}

		impl<T, O> IndexMut<$t> for BitSlice<T, O>
		where
			O: BitOrder,
			T: BitStore,
		{
			#[inline]
			#[track_caller]
			fn index_mut(&mut self, index: $t) -> &mut Self::Output {
				index.index_mut(self)
			}
		}
	)+ };
}

index! {
	Range<usize>,
	RangeFrom<usize>,
	RangeFull,
	RangeInclusive<usize>,
	RangeTo<usize>,
	RangeToInclusive<usize>,
}

/** Inverts each bit in the bit-slice.

Unlike the `&`, `|`, and `^` operators, this implementation is guaranteed to
update each memory element only once, and is not required to traverse every live
bit in the underlying region.
**/
impl<'a, T, O> Not for &'a mut BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Output = Self;

	#[inline]
	fn not(self) -> Self::Output {
		match self.domain_mut() {
			Domain::Enclave(mut elem) => {
				elem.invert();
			},
			Domain::Region { head, body, tail } => {
				if let Some(mut elem) = head {
					elem.invert();
				}
				for elem in body {
					elem.store_value(!elem.load_value());
				}
				if let Some(mut elem) = tail {
					elem.invert();
				}
			},
		}
		self
	}
}
