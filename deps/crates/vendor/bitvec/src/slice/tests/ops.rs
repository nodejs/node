use rand::random;

use crate::{
	prelude::*,
	slice::BitSliceIndex,
};

#[test]
fn bitand() {
	let a = random::<[u32; 3]>();
	let b = random::<[u32; 3]>();
	let c = [a[0] & b[0], a[1] & b[1], a[2] & b[2]];

	let mut d = a;
	*d.view_bits_mut::<Lsb0>() &= b.view_bits::<Lsb0>();
	assert_eq!(c, d);

	d = a;
	*d.view_bits_mut::<Msb0>() &= b.view_bits::<Msb0>();
	assert_eq!(c, d);

	let d = random::<[u8; 6]>();
	let e = random::<[u16; 3]>();

	let mut f = d;
	*f.view_bits_mut::<Lsb0>() &= e.view_bits::<Msb0>();
	for ((d, e), f) in (d
		.view_bits::<Lsb0>()
		.iter()
		.by_vals()
		.zip(e.view_bits::<Msb0>().iter().by_vals()))
	.zip(f.view_bits::<Lsb0>())
	{
		assert_eq!(d & e, f);
	}
}

#[test]
fn bitor() {
	let a = random::<[u32; 3]>();
	let b = random::<[u32; 3]>();
	let c = [a[0] | b[0], a[1] | b[1], a[2] | b[2]];

	let mut d = a;
	*d.view_bits_mut::<Lsb0>() |= b.view_bits::<Lsb0>();
	assert_eq!(c, d);

	d = a;
	*d.view_bits_mut::<Msb0>() |= b.view_bits::<Msb0>();
	assert_eq!(c, d);

	let d = random::<[u8; 6]>();
	let e = random::<[u16; 3]>();

	let mut f = d;
	*f.view_bits_mut::<Lsb0>() |= e.view_bits::<Msb0>();
	for ((d, e), f) in (d
		.view_bits::<Lsb0>()
		.iter()
		.by_vals()
		.zip(e.view_bits::<Msb0>().iter().by_vals()))
	.zip(f.view_bits::<Lsb0>())
	{
		assert_eq!(d | e, f);
	}
}

#[test]
fn bitxor() {
	let a = random::<[u32; 3]>();
	let b = random::<[u32; 3]>();
	let c = [a[0] ^ b[0], a[1] ^ b[1], a[2] ^ b[2]];

	let mut d = a;
	*d.view_bits_mut::<Lsb0>() ^= b.view_bits::<Lsb0>();
	assert_eq!(c, d);

	d = a;
	*d.view_bits_mut::<Msb0>() ^= b.view_bits::<Msb0>();
	assert_eq!(c, d);

	let d = random::<[u8; 6]>();
	let e = random::<[u16; 3]>();

	let mut f = d;
	*f.view_bits_mut::<Lsb0>() ^= e.view_bits::<Msb0>();
	for ((d, e), f) in (d
		.view_bits::<Lsb0>()
		.iter()
		.by_vals()
		.zip(e.view_bits::<Msb0>().iter().by_vals()))
	.zip(f.view_bits::<Lsb0>())
	{
		assert_eq!(d ^ e, f);
	}
}

#[test]
fn bit_not() {
	let a = random::<[u32; 3]>();
	let mut b = a;
	let _ = !b.view_bits_mut::<Msb0>();
	assert_eq!([!a[0], !a[1], !a[2]], b);

	let mut c = [0u32; 3];
	let d = !&mut c.view_bits_mut::<Lsb0>()[16 .. 80];
	let _ = !&mut d[24 .. 40];
	assert_eq!(c, [0xFF_FF_00_00, 0xFF_00_00_FF, 0x00_00_FF_FF]);
}

#[test]
fn indexing() {
	let bits = bits![mut 0, 1, 0, 0, 1];

	assert!(bits[1]);
	assert!(!bits[2]);
	assert_eq!(bits[1 ..= 2], bits![1, 0]);

	assert!((10 .. 12).get(bits).is_none());
	assert!((10 .. 12).get_mut(bits).is_none());
}

#[test]
#[should_panic = "index 10 out of bounds: 5"]
fn index_mut_usize() {
	let bits = bits![mut 0, 1, 0, 0, 1];
	10.index_mut(bits);
}
