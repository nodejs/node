use alloc::{
	borrow::{
		Borrow,
		BorrowMut,
		Cow,
	},
	vec::Vec,
};
use core::{
	convert::TryFrom,
	fmt::Debug,
	hash::Hash,
	iter::FromIterator,
	ops::{
		Deref,
		DerefMut,
		Index,
		Range,
	},
	panic::{
		RefUnwindSafe,
		UnwindSafe,
	},
};
#[cfg(feature = "std")]
use std::io::Write;

use static_assertions::*;

use crate::prelude::*;

#[test]
fn alloc_impl() {
	assert_impl_all!(BitVec<usize, Lsb0>:
		AsMut<BitSlice<usize, Lsb0>>,
		AsMut<BitVec<usize, Lsb0>>,
		AsRef<BitSlice<usize, Lsb0>>,
		AsRef<BitVec<usize, Lsb0>>,
		Borrow<BitSlice<usize, Lsb0>>,
		BorrowMut<BitSlice<usize, Lsb0>>,
		Clone,
		Debug,
		Default,
		Deref,
		DerefMut,
		Drop,
		Eq,
		Extend<&'static bool>,
		Extend<bool>,
		From<&'static BitSlice<usize, Lsb0>>,
		From<&'static mut BitSlice<usize, Lsb0>>,
		From<BitArray<[usize; 20], Lsb0>>,
		From<BitBox<usize, Lsb0>>,
		From<Cow<'static, BitSlice<usize, Lsb0>>>,
		FromIterator<bool>,
		Hash,
		Index<usize>,
		Index<Range<usize>>,
		IntoIterator,
		Ord,
		PartialEq<&'static BitSlice<usize, Lsb0>>,
		PartialEq<BitArray<[usize; 20], Lsb0>>,
		RefUnwindSafe,
		Send,
		Sync,
		TryFrom<Vec<usize>>,
		Unpin,
		UnwindSafe,
	);
}

#[test]
#[cfg(feature = "std")]
fn std_impl() {
	assert_impl_all!(BitVec<usize, Lsb0>: Write);
}

#[test]
fn format() {
	#[cfg(not(feature = "std"))]
	use alloc::format;

	let bv = bitvec![0, 0, 1, 1, 0, 1, 0, 1];
	assert_eq!(format!("{}", bv), format!("{}", bv.as_bitslice()));
	assert_eq!(format!("{:b}", bv), format!("{:b}", bv.as_bitslice()));
	assert_eq!(format!("{:o}", bv), format!("{:o}", bv.as_bitslice()));
	assert_eq!(format!("{:x}", bv), format!("{:x}", bv.as_bitslice()));
	assert_eq!(format!("{:X}", bv), format!("{:X}", bv.as_bitslice()));

	let text = format!("{:?}", bitvec![u8, Msb0; 0, 1, 0, 0]);
	assert!(
		text.starts_with("BitVec<u8, bitvec::order::Msb0> { addr: 0x"),
		"{}",
		text
	);
	assert!(
		text.contains(", head: 000, bits: 4, capacity: "),
		"{}",
		text
	);
	assert!(text.ends_with(" } [0, 1, 0, 0]"), "{}", text);
}
