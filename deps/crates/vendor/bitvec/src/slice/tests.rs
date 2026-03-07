//! Unit tests for bit-slices.

#![cfg(test)]

use core::cell::Cell;

use rand::random;

use crate::{
	order::HiLo,
	prelude::*,
};

mod api;
mod iter;
mod ops;
mod traits;

#[test]
#[allow(clippy::many_single_char_names)]
fn copying() {
	let a = bits![mut u8, Lsb0; 0; 4];
	let b = bits![u16, Msb0; 0, 1, 0, 1];
	a.clone_from_bitslice(b);
	assert_eq!(a, b);

	let mut a = random::<[u32; 3]>();
	let b = random::<[u32; 3]>();

	a.view_bits_mut::<Lsb0>()[4 .. 92]
		.copy_from_bitslice(&b.view_bits::<Lsb0>()[4 .. 92]);
	assert_eq!([a[0] & 0xFF_FF_FF_F0, a[1], a[2] & 0x0F_FF_FF_FF], [
		b[0] & 0xFF_FF_FF_F0,
		b[1],
		b[2] & 0x0F_FF_FF_FF
	],);

	let mut c = random::<u32>();
	let d = random::<u32>();
	c.view_bits_mut::<Msb0>()[4 .. 28]
		.copy_from_bitslice(&d.view_bits::<Msb0>()[4 .. 28]);
	assert_eq!(c & 0x0F_FF_FF_F0, d & 0x0F_FF_FF_F0);

	let mut e = 0x01_23_45_67u32;
	let f = 0x89_AB_CD_EFu32;
	e.view_bits_mut::<HiLo>()[.. 28]
		.copy_from_bitslice(&f.view_bits::<HiLo>()[4 ..]);
	assert_eq!(e, 0x91_B8_DA_FC);
	//      28 .. 32 ^

	let mut g = random::<[u32; 3]>();
	let mut h = random::<[u32; 3]>();
	let i = g;
	let j = h;
	g.view_bits_mut::<Lsb0>()
		.swap_with_bitslice(h.view_bits_mut::<Lsb0>());
	assert_eq!((g, h), (j, i));
	g.view_bits_mut::<Msb0>()
		.swap_with_bitslice(h.view_bits_mut::<Msb0>());
	assert_eq!((g, h), (i, j));
	g.view_bits_mut::<Lsb0>()
		.swap_with_bitslice(h.view_bits_mut::<Msb0>());
	assert_eq!(g.view_bits::<Lsb0>(), j.view_bits::<Msb0>());
	assert_eq!(h.view_bits::<Msb0>(), i.view_bits::<Lsb0>());

	let mut k = random::<[u32; 3]>();
	let j = k;
	unsafe {
		k.view_bits_mut::<Lsb0>().copy_within_unchecked(32 .., 0);
		assert_eq!(k, [j[1], j[2], j[2]]);
		k.view_bits_mut::<Msb0>().copy_within_unchecked(.. 64, 32);
		assert_eq!(k, [j[1], j[1], j[2]]);
		k.view_bits_mut::<HiLo>().copy_within_unchecked(32 .., 0);
		assert_eq!(k, [j[1], j[2], j[2]]);
	}
}

#[test]
fn writing() {
	let bits = bits![mut 0; 2];

	bits.set(0, true);
	unsafe {
		bits.set_unchecked(1, true);
	}
	assert_eq!(bits, bits![1;2]);

	assert!(bits.replace(0, false));
	assert!(unsafe { bits.replace_unchecked(1, false) });
	assert_eq!(bits, bits![0;2]);
}

#[test]
fn bit_counting() {
	let data = [0x12u8, 0xFE, 0x34, 0xDC];
	let lsb0 = data.view_bits::<Lsb0>();
	let msb0 = data.view_bits::<Msb0>();

	assert_eq!(lsb0[2 .. 6].count_ones(), 1);
	assert_eq!(lsb0[2 .. 6].count_zeros(), 3);
	assert_eq!(msb0[2 .. 30].count_ones(), 17);
	assert_eq!(msb0[2 .. 30].count_zeros(), 11);

	assert!(!bits![].any());
	assert!(!bits![0, 0].any());
	assert!(bits![0, 1].any());

	assert!(bits![].all());
	assert!(!bits![0, 1].all());
	assert!(bits![1, 1].all());

	assert!(bits![].not_any());
	assert!(bits![0, 0].not_any());
	assert!(!bits![0, 1].not_any());

	assert!(!bits![].not_all());
	assert!(bits![0, 1].not_all());
	assert!(!bits![1, 1].not_all());

	assert!(!bits![0; 2].some());
	assert!(bits![0, 1].some());
	assert!(!bits![1; 2].some());

	assert!(bits![usize, Lsb0;].first_one().is_none());
	assert!(bits![usize, Msb0;].first_one().is_none());
	assert!(bits![usize, Lsb0;].last_one().is_none());
	assert!(bits![usize, Msb0;].last_one().is_none());
	assert!(bits![usize, Lsb0;].first_zero().is_none());
	assert!(bits![usize, Msb0;].first_zero().is_none());
	assert!(bits![usize, Lsb0;].last_zero().is_none());
	assert!(bits![usize, Msb0;].last_zero().is_none());

	assert!([0u8; 1].view_bits::<Lsb0>()[1 .. 7].first_one().is_none());
	assert!([0u8; 3].view_bits::<Lsb0>()[1 .. 23].first_one().is_none());
	assert!([0u8; 1].view_bits::<Msb0>()[1 .. 7].first_one().is_none());
	assert!([0u8; 3].view_bits::<Msb0>()[1 .. 23].first_one().is_none());

	assert!([0u8; 1].view_bits::<Lsb0>()[1 .. 7].last_one().is_none());
	assert!([0u8; 3].view_bits::<Lsb0>()[1 .. 23].last_one().is_none());
	assert!([0u8; 1].view_bits::<Msb0>()[1 .. 7].last_one().is_none());
	assert!([0u8; 3].view_bits::<Msb0>()[1 .. 23].last_one().is_none());

	assert!([!0u8; 1].view_bits::<Lsb0>()[1 .. 7].first_zero().is_none());
	assert!(
		[!0u8; 3].view_bits::<Lsb0>()[1 .. 23]
			.first_zero()
			.is_none()
	);
	assert!([!0u8; 1].view_bits::<Msb0>()[1 .. 7].first_zero().is_none());
	assert!(
		[!0u8; 3].view_bits::<Msb0>()[1 .. 23]
			.first_zero()
			.is_none()
	);

	assert!([!0u8; 1].view_bits::<Lsb0>()[1 .. 7].last_zero().is_none());
	assert!([!0u8; 3].view_bits::<Lsb0>()[1 .. 23].last_zero().is_none());
	assert!([!0u8; 1].view_bits::<Msb0>()[1 .. 7].last_zero().is_none());
	assert!([!0u8; 3].view_bits::<Msb0>()[1 .. 23].last_zero().is_none());

	let data = 0b0100_0100u8;
	assert_eq!(data.view_bits::<Lsb0>()[1 .. 7].first_one(), Some(1));
	assert_eq!(data.view_bits::<Lsb0>()[1 .. 7].last_one(), Some(5));
	assert_eq!(data.view_bits::<Msb0>()[1 .. 7].first_one(), Some(0));
	assert_eq!(data.view_bits::<Msb0>()[1 .. 7].last_one(), Some(4));

	let data = 0b1011_1011u8;
	assert_eq!(data.view_bits::<Lsb0>()[1 .. 7].first_zero(), Some(1));
	assert_eq!(data.view_bits::<Lsb0>()[1 .. 7].last_zero(), Some(5));
	assert_eq!(data.view_bits::<Msb0>()[1 .. 7].first_zero(), Some(0));
	assert_eq!(data.view_bits::<Msb0>()[1 .. 7].last_zero(), Some(4));

	let data = [0u8, 0b1001_0110, 0];
	assert_eq!(data.view_bits::<Lsb0>()[12 ..].first_one(), Some(0));
	assert_eq!(data.view_bits::<Lsb0>()[4 ..].first_one(), Some(5));
	assert_eq!(data.view_bits::<Lsb0>()[.. 12].first_one(), Some(9));
	assert_eq!(data.view_bits::<Msb0>()[12 ..].first_one(), Some(1));
	assert_eq!(data.view_bits::<Msb0>()[4 ..].first_one(), Some(4));
	assert_eq!(data.view_bits::<Msb0>()[.. 12].first_one(), Some(8));

	assert_eq!(data.view_bits::<Lsb0>()[12 ..].last_one(), Some(3));
	assert_eq!(data.view_bits::<Lsb0>()[4 ..].last_one(), Some(11));
	assert_eq!(data.view_bits::<Lsb0>()[.. 12].last_one(), Some(10));
	assert_eq!(data.view_bits::<Msb0>()[12 ..].last_one(), Some(2));
	assert_eq!(data.view_bits::<Msb0>()[4 ..].last_one(), Some(10));
	assert_eq!(data.view_bits::<Msb0>()[.. 12].last_one(), Some(11));

	let data = [!0u8, 0b1001_0110, !0];
	assert_eq!(data.view_bits::<Lsb0>()[12 ..].first_zero(), Some(1));
	assert_eq!(data.view_bits::<Lsb0>()[4 ..].first_zero(), Some(4));
	assert_eq!(data.view_bits::<Lsb0>()[.. 12].first_zero(), Some(8));
	assert_eq!(data.view_bits::<Msb0>()[12 ..].first_zero(), Some(0));
	assert_eq!(data.view_bits::<Msb0>()[4 ..].first_zero(), Some(5));
	assert_eq!(data.view_bits::<Msb0>()[.. 12].first_zero(), Some(9));

	assert_eq!(data.view_bits::<Lsb0>()[12 ..].last_zero(), Some(2));
	assert_eq!(data.view_bits::<Lsb0>()[4 ..].last_zero(), Some(10));
	assert_eq!(data.view_bits::<Lsb0>()[.. 12].last_zero(), Some(11));
	assert_eq!(data.view_bits::<Msb0>()[12 ..].last_zero(), Some(3));
	assert_eq!(data.view_bits::<Msb0>()[4 ..].last_zero(), Some(11));
	assert_eq!(data.view_bits::<Msb0>()[.. 12].last_zero(), Some(10));

	assert_eq!(15u8.view_bits::<Lsb0>().leading_ones(), 4);
	assert_eq!(15u8.view_bits::<Msb0>().leading_ones(), 0);

	assert_eq!(15u8.view_bits::<Lsb0>().leading_zeros(), 0);
	assert_eq!(15u8.view_bits::<Msb0>().leading_zeros(), 4);

	assert_eq!(15u8.view_bits::<Lsb0>().trailing_ones(), 0);
	assert_eq!(15u8.view_bits::<Msb0>().trailing_ones(), 4);

	assert_eq!(15u8.view_bits::<Lsb0>().trailing_zeros(), 4);
	assert_eq!(15u8.view_bits::<Msb0>().trailing_zeros(), 0);
}

#[test]
fn shunting() {
	let bits = bits![mut 0, 1, 0, 0, 1];
	bits.shift_left(0);
	bits.shift_right(0);
	assert_eq!(bits, bits![0, 1, 0, 0, 1]);

	let bits = bits![mut 1;5];
	bits.shift_left(1);
	bits.shift_right(2);
	bits.shift_left(1);
	assert_eq!(bits, bits![0, 1, 1, 1, 0]);
}

#[test]
fn aliasing() {
	let bits = bits![Cell<u32>, Lsb0; 0];

	let (a, b) = (bits, bits);
	a.set_aliased(0, true);
	assert!(bits[0]);
	b.set_aliased(0, false);
	assert!(!bits[0]);
}

#[test]
fn cooking() {
	use core::convert::TryFrom;

	use crate::{
		ptr::BitPtr,
		slice,
	};

	let mut data = [0usize; 80];
	let len = crate::mem::bits_of::<usize>() * 80;
	let ref_ptr = data.as_ptr();
	let mut_ptr = data.as_mut_ptr();

	unsafe {
		assert_eq!(
			slice::from_raw_parts_unchecked(
				BitPtr::try_from(ref_ptr).unwrap(),
				len
			)
			.as_bitspan(),
			data.view_bits::<Lsb0>().as_bitspan(),
		);
		assert_eq!(
			slice::from_raw_parts_unchecked_mut(
				BitPtr::try_from(mut_ptr).unwrap(),
				len
			)
			.as_bitspan(),
			data.view_bits_mut::<Msb0>().as_bitspan(),
		);
	}
}
