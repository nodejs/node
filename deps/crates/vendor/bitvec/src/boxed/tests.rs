//! Unit tests for boxed bit-slices.

#![cfg(test)]

#[cfg(not(feature = "std"))]
use alloc::vec;
use alloc::{
	borrow::Cow,
	boxed::Box,
};
use core::{
	any,
	borrow::{
		Borrow,
		BorrowMut,
	},
	convert::TryFrom,
	fmt::{
		Debug,
		Display,
		Pointer,
	},
	hash::Hash,
	iter::{
		FromIterator,
		FusedIterator,
	},
	ops::{
		Deref,
		DerefMut,
	},
};

use static_assertions::*;

use crate::prelude::*;

#[test]
fn inherents() {
	let bits = bits![0, 1, 0, 0, 1];
	let mut boxed = BitBox::from_bitslice(&bits[1 ..]);
	assert_eq!(boxed, bits[1 ..]);
	assert_eq!(boxed.bitspan.head().into_inner(), 1);
	boxed.force_align();
	assert_eq!(boxed.bitspan.head().into_inner(), 0);

	let data = vec![0u8, 1, 2, 3].into_boxed_slice();
	let ptr = data.as_ptr();
	let boxed: BitBox<u8> = BitBox::from_boxed_slice(data);
	assert_eq!(boxed.len(), 32);
	assert_eq!(boxed.count_ones(), 4);
	let data = boxed.into_boxed_slice();
	assert_eq!(data.as_ptr(), ptr);

	let bv = BitBox::<u8>::from_boxed_slice(data).into_bitvec();
	assert_eq!(bv.len(), 32);

	assert_eq!(bitbox![0, 1, 0, 0, 1].as_bitslice(), bits![0, 1, 0, 0, 1]);
	assert_eq!(bitbox![0; 5].as_mut_bitslice(), bits![0; 5]);

	let mut bb = bitbox![0; 5];
	bb.fill_uninitialized(true);
	assert_eq!(bb.as_raw_slice(), &[!0usize << 5][..]);

	let ptr = BitBox::into_raw(bb);
	let bb = unsafe { BitBox::from_raw(ptr) };
	assert_eq!(ptr as *const BitSlice, bb.as_bitslice() as *const BitSlice);
}

#[test]
fn iter() {
	let bb = bitbox![0, 1, 1, 0, 0, 1];
	let mut iter = bb.into_iter();
	assert_eq!(iter.len(), 6);

	assert!(!iter.next().unwrap());
	assert_eq!(iter.as_bitslice(), bits![1, 1, 0, 0, 1]);
	assert!(iter.next_back().unwrap());
	assert_eq!(iter.as_mut_bitslice(), bits![1, 1, 0, 0]);
	assert!(iter.nth(1).unwrap());
	assert!(!iter.nth_back(1).unwrap());
	assert!(iter.next().is_none());
}

#[test]
fn traits() {
	assert_impl_all!(
		BitBox: AsMut<BitSlice>,
		AsRef<BitSlice>,
		Borrow<BitSlice>,
		BorrowMut<BitSlice>,
		Clone,
		Debug,
		Default,
		Deref,
		DerefMut,
		Display,
		Drop,
		Eq,
		From<&'static BitSlice>,
		From<BitArray>,
		From<Box<usize>>,
		From<Cow<'static, BitSlice>>,
		From<BitVec>,
		FromIterator<bool>,
		Hash,
		Ord,
		PartialEq<BitSlice>,
		PartialOrd<BitSlice>,
		Pointer,
		TryFrom<Box<[usize]>>,
		Unpin,
	);
	assert_impl_all!(
		super::IntoIter: AsRef<BitSlice>,
		Clone,
		Debug,
		DoubleEndedIterator,
		ExactSizeIterator,
		FusedIterator,
		Send,
		Sync,
	);
}

#[test]
fn conversions() {
	let bits = bits![0, 1, 0, 0, 1];
	assert_eq!(BitBox::from(bits), bits);

	let arr: BitArray = BitArray::new(rand::random());
	assert_eq!(BitBox::from(arr), arr);

	let boxed = Box::new(5usize);
	assert_eq!(
		BitBox::<_, Lsb0>::from(boxed.clone()),
		boxed.view_bits::<Lsb0>()
	);

	let cow = Cow::Borrowed([0usize, 1].view_bits::<Lsb0>());
	assert_eq!(BitBox::from(cow.clone()), &*cow);

	assert_eq!(BitBox::from(bitvec![0, 1]), bits![0, 1]);

	let boxed: Box<[usize]> = BitBox::from(cow.clone()).into();
	assert_eq!(boxed[..], [0usize, 1][..]);

	assert!(BitBox::<_, Lsb0>::try_from(boxed).is_ok());

	assert!(BitBox::<usize, Lsb0>::default().is_empty());
}

#[test]
fn ops() {
	let a = bitbox![0, 0, 1, 1];
	let b = bitbox![0, 1, 0, 1];

	let c = a.clone() & b.clone();
	assert_eq!(c, bitbox![0, 0, 0, 1]);

	let d = a.clone() | b.clone();
	assert_eq!(d, bitbox![0, 1, 1, 1]);

	let e = a.clone() ^ b;
	assert_eq!(e, bitbox![0, 1, 1, 0]);

	let mut f = !e;
	assert_eq!(f, bitbox![1, 0, 0, 1]);

	let _: &BitSlice = &a;
	let _: &mut BitSlice = &mut f;
}

#[test]
fn format() {
	#[cfg(not(feature = "std"))]
	use alloc::format;

	let render = format!("{:?}", bitbox![0, 1, 0, 0, 1]);
	assert!(
		render.starts_with(&format!(
			"BitBox<usize, {}>",
			any::type_name::<Lsb0>(),
		))
	);
	assert!(render.ends_with("[0, 1, 0, 0, 1]"));
}
