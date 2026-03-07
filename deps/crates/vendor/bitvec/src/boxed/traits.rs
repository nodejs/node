//! General trait implementations for boxed bit-slices.

use alloc::{
	borrow::Cow,
	boxed::Box,
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
	iter::FromIterator,
};

use tap::Pipe;

use super::BitBox;
use crate::{
	array::BitArray,
	order::BitOrder,
	slice::BitSlice,
	store::BitStore,
	vec::BitVec,
	view::BitViewSized,
};

#[cfg(not(tarpaulin_include))]
impl<T, O> Borrow<BitSlice<T, O>> for BitBox<T, O>
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
impl<T, O> BorrowMut<BitSlice<T, O>> for BitBox<T, O>
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
impl<T, O> Clone for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn clone(&self) -> Self {
		self.as_bitslice().pipe(Self::from_bitslice)
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> Eq for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

#[cfg(not(tarpaulin_include))]
impl<T, O> Ord for BitBox<T, O>
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
impl<O1, O2, T1, T2> PartialEq<BitBox<T2, O2>> for BitSlice<T1, O1>
where
	O1: BitOrder,
	O2: BitOrder,
	T1: BitStore,
	T2: BitStore,
{
	#[inline]
	fn eq(&self, other: &BitBox<T2, O2>) -> bool {
		self == other.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<O1, O2, T1, T2> PartialEq<BitBox<T2, O2>> for &BitSlice<T1, O1>
where
	O1: BitOrder,
	O2: BitOrder,
	T1: BitStore,
	T2: BitStore,
{
	#[inline]
	fn eq(&self, other: &BitBox<T2, O2>) -> bool {
		*self == other.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<O1, O2, T1, T2> PartialEq<BitBox<T2, O2>> for &mut BitSlice<T1, O1>
where
	O1: BitOrder,
	O2: BitOrder,
	T1: BitStore,
	T2: BitStore,
{
	#[inline]
	fn eq(&self, other: &BitBox<T2, O2>) -> bool {
		**self == other.as_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Rhs> PartialEq<Rhs> for BitBox<T, O>
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
impl<O1, O2, T1, T2> PartialOrd<BitBox<T2, O2>> for BitSlice<T1, O1>
where
	O1: BitOrder,
	O2: BitOrder,
	T1: BitStore,
	T2: BitStore,
{
	#[inline]
	fn partial_cmp(&self, other: &BitBox<T2, O2>) -> Option<cmp::Ordering> {
		self.partial_cmp(other.as_bitslice())
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O, Rhs> PartialOrd<Rhs> for BitBox<T, O>
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
impl<'a, O1, O2, T1, T2> PartialOrd<BitBox<T2, O2>> for &'a BitSlice<T1, O1>
where
	O1: BitOrder,
	O2: BitOrder,
	T1: BitStore,
	T2: BitStore,
{
	#[inline]
	fn partial_cmp(&self, other: &BitBox<T2, O2>) -> Option<cmp::Ordering> {
		self.partial_cmp(other.as_bitslice())
	}
}

#[cfg(not(tarpaulin_include))]
impl<'a, O1, O2, T1, T2> PartialOrd<BitBox<T2, O2>> for &'a mut BitSlice<T1, O1>
where
	O1: BitOrder,
	O2: BitOrder,
	T1: BitStore,
	T2: BitStore,
{
	#[inline]
	fn partial_cmp(&self, other: &BitBox<T2, O2>) -> Option<cmp::Ordering> {
		self.partial_cmp(other.as_bitslice())
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> AsRef<BitSlice<T, O>> for BitBox<T, O>
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
impl<T, O> AsMut<BitSlice<T, O>> for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn as_mut(&mut self) -> &mut BitSlice<T, O> {
		self.as_mut_bitslice()
	}
}

impl<T, O> From<&'_ BitSlice<T, O>> for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(slice: &BitSlice<T, O>) -> Self {
		slice.pipe(Self::from_bitslice)
	}
}

impl<A, O> From<BitArray<A, O>> for BitBox<A::Store, O>
where
	A: BitViewSized,
	O: BitOrder,
{
	#[inline]
	fn from(array: BitArray<A, O>) -> Self {
		array.as_bitslice().pipe(Self::from_bitslice)
	}
}

impl<T, O> From<Box<T>> for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(elem: Box<T>) -> Self {
		unsafe {
			Box::from_raw(Box::into_raw(elem).cast::<[T; 1]>() as *mut [T])
		}
		.pipe(Self::from_boxed_slice)
	}
}

impl<'a, T, O> From<Cow<'a, BitSlice<T, O>>> for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(cow: Cow<'a, BitSlice<T, O>>) -> Self {
		cow.into_owned().into_boxed_bitslice()
	}
}

impl<T, O> From<BitVec<T, O>> for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(bv: BitVec<T, O>) -> Self {
		bv.into_boxed_bitslice()
	}
}

impl<T, O> From<BitBox<T, O>> for Box<[T]>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn from(bb: BitBox<T, O>) -> Self {
		bb.into_boxed_slice()
	}
}

impl<T, O> TryFrom<Box<[T]>> for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	type Error = Box<[T]>;

	#[inline]
	fn try_from(boxed: Box<[T]>) -> Result<Self, Self::Error> {
		Self::try_from_boxed_slice(boxed)
	}
}

impl<T, O> Default for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn default() -> Self {
		Self::from_bitslice(BitSlice::<T, O>::empty())
	}
}

impl<T, O> Debug for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
	#[inline]
	fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
		self.bitspan.render(fmt, "Box", None)?;
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
	for BitBox
}

#[cfg(not(tarpaulin_include))]
impl<T, O, I> FromIterator<I> for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
	BitVec<T, O>: FromIterator<I>,
{
	#[inline]
	fn from_iter<II>(iter: II) -> Self
	where II: IntoIterator<Item = I> {
		BitVec::from_iter(iter).into_boxed_bitslice()
	}
}

#[cfg(not(tarpaulin_include))]
impl<T, O> Hash for BitBox<T, O>
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

unsafe impl<T, O> Send for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

unsafe impl<T, O> Sync for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
}

impl<T, O> Unpin for BitBox<T, O>
where
	T: BitStore,
	O: BitOrder,
{
}
