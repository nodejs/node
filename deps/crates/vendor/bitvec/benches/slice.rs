#![feature(test)]

extern crate test;

use bitvec::prelude::*;
use test::{
	bench::black_box,
	Bencher,
};

/* `BitSlice::empty` is not benched, because the compiler const-folds it. It
is not a `const fn`, but it has exactly one function call, which is `const`, and
creates a value object from that function. As such, the compiler can prove that
the return value is a `const` value, and insert the value at all
`BitSlice::empty` call sites. It takes 0ns.
*/

#[bench]
fn element(b: &mut Bencher) {
	b.iter(|| BitSlice::<u8, Msb0>::from_element(&!0));
	b.iter(|| BitSlice::<u8, Lsb0>::from_element(&!0));
	b.iter(|| BitSlice::<u16, Msb0>::from_element(&!0));
	b.iter(|| BitSlice::<u16, Lsb0>::from_element(&!0));
	b.iter(|| BitSlice::<u32, Msb0>::from_element(&!0));
	b.iter(|| BitSlice::<u32, Lsb0>::from_element(&!0));

	#[cfg(target_pointer_width = "64")]
	{
		b.iter(|| BitSlice::<u64, Msb0>::from_element(&!0));
		b.iter(|| BitSlice::<u64, Lsb0>::from_element(&!0));
	}
}

#[bench]
fn slice(b: &mut Bencher) {
	b.iter(|| BitSlice::<u8, Msb0>::from_slice(&[0, 1, !0 - 1, !0][..]));
	b.iter(|| BitSlice::<u8, Lsb0>::from_slice(&[0, 1, !0 - 1, !0][..]));
	b.iter(|| BitSlice::<u16, Msb0>::from_slice(&[0, 1, !0 - 1, !0][..]));
	b.iter(|| BitSlice::<u16, Lsb0>::from_slice(&[0, 1, !0 - 1, !0][..]));
	b.iter(|| BitSlice::<u32, Msb0>::from_slice(&[0, 1, !0 - 1, !0][..]));
	b.iter(|| BitSlice::<u32, Lsb0>::from_slice(&[0, 1, !0 - 1, !0][..]));

	#[cfg(target_pointer_width = "64")]
	{
		b.iter(|| BitSlice::<u64, Msb0>::from_slice(&[0, 1, !0 - 1, !0][..]));
		b.iter(|| BitSlice::<u64, Lsb0>::from_slice(&[0, 1, !0 - 1, !0][..]));
	}
}

#[bench]
fn len(b: &mut Bencher) {
	let bsb08 = [0u8; 16].view_bits::<Msb0>();
	let bsl08 = [0u8; 16].view_bits::<Lsb0>();
	b.iter(|| bsb08.len());
	b.iter(|| bsl08.len());

	let bsb16 = [0u16; 8].view_bits::<Msb0>();
	let bsl16 = [0u16; 8].view_bits::<Lsb0>();
	b.iter(|| bsb16.len());
	b.iter(|| bsl16.len());

	let bsb32 = [0u32; 4].view_bits::<Msb0>();
	let bsl32 = [0u32; 4].view_bits::<Lsb0>();
	b.iter(|| bsb32.len());
	b.iter(|| bsl32.len());

	#[cfg(target_pointer_width = "64")]
	{
		let bsb64 = [0u64; 2].view_bits::<Msb0>();
		let bsl64 = [0u64; 2].view_bits::<Lsb0>();
		b.iter(|| bsb64.len());
		b.iter(|| bsl64.len());
	}
}

//  This index value is not only "nice", it also ensures that the hard path is
//  hit in `BitIdx::offset`.
#[bench]
fn index(b: &mut Bencher) {
	let bsb08 = [0u8; 16].view_bits::<Msb0>();
	let bsl08 = [0u8; 16].view_bits::<Lsb0>();
	b.iter(|| assert!(!black_box(bsb08)[black_box(69)]));
	b.iter(|| assert!(!black_box(bsl08)[black_box(69)]));

	let bsb16 = [0u16; 8].view_bits::<Msb0>();
	let bsl16 = [0u16; 8].view_bits::<Lsb0>();
	b.iter(|| assert!(!black_box(bsb16)[black_box(69)]));
	b.iter(|| assert!(!black_box(bsl16)[black_box(69)]));

	let bsb32 = [0u32; 4].view_bits::<Msb0>();
	let bsl32 = [0u32; 4].view_bits::<Lsb0>();
	b.iter(|| assert!(!black_box(bsb32)[black_box(69)]));
	b.iter(|| assert!(!black_box(bsl32)[black_box(69)]));

	#[cfg(target_pointer_width = "64")]
	{
		let bsb64 = [0u64; 2].view_bits::<Msb0>();
		let bsl64 = [0u64; 2].view_bits::<Lsb0>();
		b.iter(|| assert!(!black_box(bsb64)[black_box(69)]));
		b.iter(|| assert!(!black_box(bsl64)[black_box(69)]));
	}
}

/* This routine has more work to do: index, create a reference struct, and drop
it. The compiler *should* be able to properly arrange immediate drops, though.
*/
#[bench]
fn get_mut(b: &mut Bencher) {
	let mut src = [0u8; 16];
	let bsb08 = src.view_bits_mut::<Msb0>();
	b.iter(|| *bsb08.get_mut(69).unwrap() = true);
	let mut src = [0u8; 16];
	let bsl08 = src.view_bits_mut::<Lsb0>();
	b.iter(|| *bsl08.get_mut(69).unwrap() = true);

	let mut src = [0u16; 8];
	let bsb16 = src.view_bits_mut::<Msb0>();
	b.iter(|| *bsb16.get_mut(69).unwrap() = true);
	let mut src = [0u16; 8];
	let bsl16 = src.view_bits_mut::<Lsb0>();
	b.iter(|| *bsl16.get_mut(69).unwrap() = true);

	let mut src = [0u32; 4];
	let bsb32 = src.view_bits_mut::<Msb0>();
	b.iter(|| *bsb32.get_mut(69).unwrap() = true);
	let mut src = [0u32; 4];
	let bsl32 = src.view_bits_mut::<Lsb0>();
	b.iter(|| *bsl32.get_mut(69).unwrap() = true);

	#[cfg(target_pointer_width = "64")]
	{
		let mut src = [0u64; 2];
		let bsb64 = src.view_bits_mut::<Msb0>();
		b.iter(|| *bsb64.get_mut(69).unwrap() = true);
		let mut src = [0u64; 2];
		let bsl64 = src.view_bits_mut::<Lsb0>();
		b.iter(|| *bsl64.get_mut(69).unwrap() = true);
	}
}
