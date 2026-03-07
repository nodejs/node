//! General trait implementations for bit-vectors.

use alloc::{
	borrow::{
		Cow,
		ToOwned,
	},
	vec::Vec,
};
use core::{
	borrow::{
		Borrow,
		BorrowMut,
	},
	cmp,
	convert::TryFrom,
	fmt::{
		self,
		Debug,
		Display,
		Formatter,
	},
	hash::{
		Hash,
		Hasher,
	},
	marker::Unpin,
};

use super::BitVec;
use crate::{
	array::BitArray,
	boxed::BitBox,
	order::BitOrder,
	slice::BitSlice,
	store::BitStore,
	view::BitViewSized,
};

#[cfg(not(tarpaulin_include))]
impl<T, O> Borrow<BitSlice<T, O>> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn borrow(&self) -> &BitSlice<T, O> {
		self.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> BorrowMut<BitSlice<T, O>> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn borrow_mut(&mut self) -> &mut BitSlice<T, O> {
		self.as_mut_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> Clone for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn clone(&self) -> Self {
		Self::from_bitslice(self.as_bitslice())
	}
}

impl<T, O> Eq for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

#[cfg(not(tarpaulin_include))]
impl<T, O> Ord for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn cmp(&self, other: &Self) -> cmp::Ordering {
		self.as_bitslice().cmp(other.as_bitslice())
	}
}

#[cfg(not(tarpaulin_include))]
impl<T1, T2, O1, O2> PartialEq<BitVec<T2, O2>> for BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn eq(&self, other: &BitVec<T2, O2>) -> bool {
		self == other.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T1, T2, O1, O2> PartialEq<BitVec<T2, O2>> for &BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn eq(&self, other: &BitVec<T2, O2>) -> bool {
		*self == other.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T1, T2, O1, O2> PartialEq<BitVec<T2, O2>> for &mut BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn eq(&self, other: &BitVec<T2, O2>) -> bool {
		**self == other.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Rhs> PartialEq<Rhs> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	Rhs: ?Sized + PartialEq<BitSlice<T, O>>,
{
	#[inline]
	fn eq(&self, other: &Rhs) -> bool {
		other == self.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T1, T2, O1, O2> PartialOrd<BitVec<T2, O2>> for BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, other: &BitVec<T2, O2>) -> Option<cmp::Ordering> {
		self.partial_cmp(other.as_bitslice())
	}
}

#[cfg(not(tarpaulin_include))]
impl<'a, T1, T2, O1, O2> PartialOrd<BitVec<T2, O2>> for &'a BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, other: &BitVec<T2, O2>) -> Option<cmp::Ordering> {
		self.partial_cmp(other.as_bitslice())
	}
}

#[cfg(not(tarpaulin_include))]
impl<'a, T1, T2, O1, O2> PartialOrd<BitVec<T2, O2>> for &'a mut BitSlice<T1, O1>
where
	T1: BitStore,
	T2: BitStore,
	O1: BitOrder,
	O2: BitOrder,
{
	#[inline]
	fn partial_cmp(&self, other: &BitVec<T2, O2>) -> Option<cmp::Ordering> {
		self.partial_cmp(other.as_bitslice())
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Rhs> PartialOrd<Rhs> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
	Rhs: ?Sized + PartialOrd<BitSlice<T, O>>,
{
	#[inline]
	fn partial_cmp(&self, other: &Rhs) -> Option<cmp::Ordering> {
		other.partial_cmp(self.as_bitslice())
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> AsRef<BitSlice<T, O>> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_ref(&self) -> &BitSlice<T, O> {
		self.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> AsMut<BitSlice<T, O>> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_mut(&mut self) -> &mut BitSlice<T, O> {
		self.as_mut_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> AsRef<BitVec<T, O>> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_ref(&self) -> &Self {
		self
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> AsMut<BitVec<T, O>> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_mut(&mut self) -> &mut Self {
		self
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> From<&'_ BitSlice<T, O>> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(slice: &BitSlice<T, O>) -> Self {
		Self::from_bitslice(slice)
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> From<&'_ mut BitSlice<T, O>> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(slice: &mut BitSlice<T, O>) -> Self {
		Self::from_bitslice(slice)
	}
}

#[cfg(not(tarpaulin_include))]
impl<A, O> From<BitArray<A, O>> for BitVec<A::Store, O>
where
	O: BitOrder,
	A: BitViewSized,
{
	#[inline]
	fn from(array: BitArray<A, O>) -> Self {
		array.as_bitslice().to_owned()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> From<BitBox<T, O>> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(boxed: BitBox<T, O>) -> Self {
		boxed.into_bitvec()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> From<BitVec<T, O>> for Vec<T>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(bv: BitVec<T, O>) -> Self {
		bv.into_vec()
	}
}

#[cfg(not(tarpaulin_include))]
impl<'a, T, O> From<Cow<'a, BitSlice<T, O>>> for BitVec<T, O>
where
	O: BitOrder,
	T: 'a + BitStore,
{
	#[inline]
	fn from(cow: Cow<'a, BitSlice<T, O>>) -> Self {
		cow.into_owned()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> TryFrom<Vec<T>> for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Error = Vec<T>;

	#[inline]
	fn try_from(vec: Vec<T>) -> Result<Self, Self::Error> {
		Self::try_from_vec(vec)
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> Default for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn default() -> Self {
		Self::new()
	}
}

impl<T, O> Debug for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		self.as_bitspan().render(fmt, "Vec", &[(
			"capacity",
			&self.capacity() as &dyn Debug,
		)])?;
		fmt.write_str(" ")?;
		Display::fmt(self, fmt)
	}
}

easy_fmt! {
	impl Binary
	impl Display
	impl LowerHex
	impl Octal
	impl Pointer
	impl UpperHex
	for BitVec
}

#[cfg(not(tarpaulin_include))]
impl<T, O> Hash for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn hash<H>(&self, state: &mut H)
	where H: Hasher {
		self.as_bitslice().hash(state)
	}
}

unsafe impl<T, O> Send for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

unsafe impl<T, O> Sync for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

impl<T, O> Unpin for BitVec<T, O>
where
	T: BitStore,
	O: BitOrder,
{
}
