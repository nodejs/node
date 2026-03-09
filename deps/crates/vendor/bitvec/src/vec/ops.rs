//! Operator trait implementations for bit-vectors.

use core::{
	mem::ManuallyDrop,
	ops::{
		BitAnd,
		BitAndAssign,
		BitOr,
		BitOrAssign,
		BitXor,
		BitXorAssign,
		Deref,
		DerefMut,
		Index,
		IndexMut,
		Not,
	},
};

use wyz::comu::Mut;

use super::BitVec;
use crate::{
	order::BitOrder,
	ptr::BitSpan,
	slice::BitSlice,
	store::BitStore,
};

#[cfg(not(tarpaulin_include))]
impl<T, O> BitAndAssign<BitVec<T, O>> for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn bitand_assign(&mut self, rhs: BitVec<T, O>) {
		*self &= rhs.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> BitAndAssign<&BitVec<T, O>> for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn bitand_assign(&mut self, rhs: &BitVec<T, O>) {
		*self &= rhs.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Rhs> BitAnd<Rhs> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: BitAndAssign<Rhs>,
{
	type Output = Self;

	#[inline]
	fn bitand(mut self, rhs: Rhs) -> Self::Output {
		self &= rhs;
		self
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Rhs> BitAndAssign<Rhs> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: BitAndAssign<Rhs>,
{
	#[inline]
	fn bitand_assign(&mut self, rhs: Rhs) {
		*self.as_mut_bitslice() &= rhs;
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> BitOrAssign<BitVec<T, O>> for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn bitor_assign(&mut self, rhs: BitVec<T, O>) {
		*self |= rhs.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> BitOrAssign<&BitVec<T, O>> for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn bitor_assign(&mut self, rhs: &BitVec<T, O>) {
		*self |= rhs.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Rhs> BitOr<Rhs> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: BitOrAssign<Rhs>,
{
	type Output = Self;

	#[inline]
	fn bitor(mut self, rhs: Rhs) -> Self::Output {
		self |= rhs;
		self
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Rhs> BitOrAssign<Rhs> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: BitOrAssign<Rhs>,
{
	#[inline]
	fn bitor_assign(&mut self, rhs: Rhs) {
		*self.as_mut_bitslice() |= rhs;
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> BitXorAssign<BitVec<T, O>> for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn bitxor_assign(&mut self, rhs: BitVec<T, O>) {
		*self ^= rhs.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> BitXorAssign<&BitVec<T, O>> for BitSlice<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn bitxor_assign(&mut self, rhs: &BitVec<T, O>) {
		*self ^= rhs.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Rhs> BitXor<Rhs> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: BitXorAssign<Rhs>,
{
	type Output = Self;

	#[inline]
	fn bitxor(mut self, rhs: Rhs) -> Self::Output {
		self ^= rhs;
		self
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Rhs> BitXorAssign<Rhs> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: BitXorAssign<Rhs>,
{
	#[inline]
	fn bitxor_assign(&mut self, rhs: Rhs) {
		*self.as_mut_bitslice() ^= rhs;
	}
}

impl<T, O> Deref for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Target = BitSlice<T, O>;

	#[inline]
	fn deref(&self) -> &Self::Target {
		self.as_bitslice()
	}
}

impl<T, O> DerefMut for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn deref_mut(&mut self) -> &mut Self::Target {
		self.as_mut_bitslice()
	}
}

impl<T, O> Drop for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn drop(&mut self) {
		if self.bitspan != BitSpan::<Mut, T, O>::EMPTY {
			self.with_vec(|slot| unsafe { ManuallyDrop::drop(slot) });
		}
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Idx> Index<Idx> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: Index<Idx>,
{
	type Output = <BitSlice<T, O> as Index<Idx>>::Output;

	#[inline]
	fn index(&self, index: Idx) -> &Self::Output {
		&self.as_bitslice()[index]
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Idx> IndexMut<Idx> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitSlice<T, O>: IndexMut<Idx>,
{
	#[inline]
	fn index_mut(&mut self, index: Idx) -> &mut Self::Output {
		&mut self.as_mut_bitslice()[index]
	}
}

/** This implementation inverts all elements in the live buffer. You cannot rely
on the value of bits in the buffer that are outside the domain of
[`BitVec::as_mut_bitslice`].
**/
impl<T, O> Not for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Output = Self;

	#[inline]
	fn not(mut self) -> Self::Output {
		for elem in self.as_raw_mut_slice() {
			elem.store_value(!elem.load_value());
		}
		self
	}
}
