//! Unit tests for bit-arrays.

#![cfg(test)]

use core::{
	borrow::{
		Borrow,
		BorrowMut,
	},
	cell::Cell,
	convert::TryFrom,
	fmt::Debug,
	hash::Hash,
	ops::{
		BitAnd,
		BitOr,
		BitXor,
		Index,
		IndexMut,
		Range,
	},
};

use static_assertions::*;

use crate::prelude::*;

#[test]
fn core_impl() {
	assert_impl_all!(
		BitArray: AsMut<BitSlice>,
		AsRef<BitSlice>,
		Borrow<BitSlice>,
		BorrowMut<BitSlice>,
		Debug,
		Default,
		Eq,
		Hash,
		Index<usize>,
		Index<Range<usize>>,
		IndexMut<Range<usize>>,
		IntoIterator,
		Ord,
		PartialEq<&'static BitSlice>,
		PartialEq<&'static mut BitSlice>,
		PartialOrd<&'static BitSlice>,
		TryFrom<&'static BitSlice>,
	);
	assert_impl_all!(&'static BitArray: TryFrom<&'static BitSlice>);
	assert_impl_all!(&'static mut BitArray: TryFrom<&'static mut BitSlice>);
}

#[test]
fn bonus_impl() {
	assert_impl_all!(
		BitArray: BitAnd<&'static BitSlice>,
		BitAnd<BitArray>,
		BitOr<&'static BitSlice>,
		BitOr<BitArray>,
		BitXor<&'static BitSlice>,
		BitXor<BitArray>,
	);
}

#[test]
fn make_and_view() {
	let data = [1u8, 2, 3, 4];
	let bits = BitArray::<_, Msb0>::new(data);

	assert_eq!(bits.as_bitslice(), data.view_bits::<Msb0>());

	assert_eq!(bits.len(), data.view_bits::<Msb0>().len());
	assert!(!bits.is_empty());
	assert_eq!(bits.into_inner(), data);
}

#[test]
fn ops() {
	let a = bitarr![0, 0, 1, 1];
	let b = bitarr![0, 1, 0, 1];

	let c = a & b;
	assert_eq!(c, bitarr![0, 0, 0, 1]);

	let d = a | b;
	assert_eq!(d, bitarr![0, 1, 1, 1]);

	let e = a ^ b;
	assert_eq!(e, bitarr![0, 1, 1, 0]);

	let mut f = !e;
	assert_eq!(f[.. 4], bitarr![1, 0, 0, 1][.. 4]);

	let _: &BitSlice = &a;
	let _: &mut BitSlice = &mut f;
}

#[test]
fn traits() {
	let a = BitArray::<[Cell<u16>; 3], Msb0>::default();
	let b = a.clone();
	assert_eq!(a, b);

	let mut c = rand::random::<[u8; 4]>();
	let d = c.view_bits_mut::<Lsb0>();
	assert!(<&BitArray<[u8; 4], Lsb0>>::try_from(&*d).is_ok());
	assert!(<&mut BitArray<[u8; 4], Lsb0>>::try_from(&mut *d).is_ok());
	assert!(<&BitArray<[u8; 3], Lsb0>>::try_from(&d[4 .. 28]).is_err());
	assert!(<&mut BitArray<[u8; 3], Lsb0>>::try_from(&mut d[4 .. 28]).is_err());
	assert_eq!(BitArray::<[u8; 4], Lsb0>::try_from(&*d).unwrap(), *d);
}

#[test]
fn iter() {
	let data = rand::random::<[u32; 4]>();
	let bits = data.into_bitarray::<Lsb0>();
	let view = data.view_bits::<Lsb0>();

	assert!(
		bits.into_iter()
			.zip(view.iter().by_vals())
			.all(|(a, b)| a == b)
	);

	let mut iter = bits.into_iter();
	assert!(iter.next().is_some());
	assert!(iter.next_back().is_some());
	assert!(iter.nth(6).is_some());
	assert!(iter.nth_back(6).is_some());
	assert_eq!(iter.len(), 112);

	assert_eq!(iter.as_bitslice(), &view[8 .. 120]);
	assert_eq!(iter.as_mut_bitslice(), &view[8 .. 120]);
}

#[cfg(feature = "alloc")]
mod format {
	#[cfg(not(feature = "std"))]
	use alloc::format;
	use core::{
		any,
		convert::TryFrom,
	};

	use super::{
		BitArray,
		Lsb0,
	};

	#[test]
	fn render() {
		let render = format!("{:?}", BitArray::<u8, Lsb0>::ZERO);
		assert!(render.starts_with(&format!(
			"BitArray<u8, {}>",
			any::type_name::<Lsb0>(),
		)));
		assert!(render.ends_with("[0, 0, 0, 0, 0, 0, 0, 0]"));

		assert_eq!(
			format!(
				"{:?}",
				BitArray::<u8, Lsb0>::try_from(bits![u8, Lsb0; 0, 1])
					.unwrap_err(),
			),
			"TryFromBitSliceError::UnequalLen(2 != 8)",
		);
		assert_eq!(
			format!(
				"{:?}",
				BitArray::<u8, Lsb0>::try_from(&bits![u8, Lsb0; 0; 9][1 ..])
					.unwrap_err(),
			),
			"TryFromBitSliceError::Misaligned",
		);
	}
}
