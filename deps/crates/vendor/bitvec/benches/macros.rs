/*! Macro construction benchmarks.

This is taken from [issue #28], which noted that the `bitvec![bit; rep]`
expansion was horribly inefficient.

This benchmark crate should be used for all macro performance recording, and
compare the macros against `vec!`. While `vec!` will always be faster, because
`bitvec!` does more work than `vec!`, they should at least be close.

Original performance was 10,000x slower. Performance after the fix for #28 was
within 20ns.

[issue #28]: https://github.com/myrrlyn/bitvec/issues/28
!*/

#![feature(test)]

extern crate test;

use bitvec::prelude::*;
use test::Bencher;

#[bench]
fn bits_seq_u8(b: &mut Bencher) {
	b.iter(|| {
		bitarr![u8, LocalBits;
			0, 1, 0, 1, 0, 0, 1, 1,
			0, 1, 1, 0, 0, 0, 0, 1,
			0, 1, 1, 0, 1, 1, 0, 0,
			0, 1, 1, 1, 0, 1, 0, 1,
			0, 1, 1, 1, 0, 1, 0, 0,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 1, 1, 0, 1, 1, 1, 0,
			0, 0, 1, 0, 1, 1, 0, 0,
			0, 0, 1, 0, 0, 0, 0, 0,
			0, 1, 1, 0, 1, 1, 0, 1,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 1, 1, 0, 1, 1, 1, 0,
			0, 1, 1, 0, 0, 1, 0, 0,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 0, 1, 0, 0, 0, 0, 1,
		]
	});
}

#[bench]
fn bits_seq_u16(b: &mut Bencher) {
	b.iter(|| {
		bitarr![u16, LocalBits;
			0, 1, 0, 1, 0, 0, 1, 1,
			0, 1, 1, 0, 0, 0, 0, 1,
			0, 1, 1, 0, 1, 1, 0, 0,
			0, 1, 1, 1, 0, 1, 0, 1,
			0, 1, 1, 1, 0, 1, 0, 0,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 1, 1, 0, 1, 1, 1, 0,
			0, 0, 1, 0, 1, 1, 0, 0,
			0, 0, 1, 0, 0, 0, 0, 0,
			0, 1, 1, 0, 1, 1, 0, 1,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 1, 1, 0, 1, 1, 1, 0,
			0, 1, 1, 0, 0, 1, 0, 0,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 0, 1, 0, 0, 0, 0, 1,
		]
	});
}

#[bench]
fn bits_seq_u32(b: &mut Bencher) {
	b.iter(|| {
		bitarr![u32, LocalBits;
			0, 1, 0, 1, 0, 0, 1, 1,
			0, 1, 1, 0, 0, 0, 0, 1,
			0, 1, 1, 0, 1, 1, 0, 0,
			0, 1, 1, 1, 0, 1, 0, 1,
			0, 1, 1, 1, 0, 1, 0, 0,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 1, 1, 0, 1, 1, 1, 0,
			0, 0, 1, 0, 1, 1, 0, 0,
			0, 0, 1, 0, 0, 0, 0, 0,
			0, 1, 1, 0, 1, 1, 0, 1,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 1, 1, 0, 1, 1, 1, 0,
			0, 1, 1, 0, 0, 1, 0, 0,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 0, 1, 0, 0, 0, 0, 1,
		]
	});
}

#[bench]
#[cfg(target_pointer_width = "64")]
fn bits_seq_u64(b: &mut Bencher) {
	b.iter(|| {
		bitarr![u64, LocalBits;
			0, 1, 0, 1, 0, 0, 1, 1,
			0, 1, 1, 0, 0, 0, 0, 1,
			0, 1, 1, 0, 1, 1, 0, 0,
			0, 1, 1, 1, 0, 1, 0, 1,
			0, 1, 1, 1, 0, 1, 0, 0,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 1, 1, 0, 1, 1, 1, 0,
			0, 0, 1, 0, 1, 1, 0, 0,
			0, 0, 1, 0, 0, 0, 0, 0,
			0, 1, 1, 0, 1, 1, 0, 1,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 1, 1, 0, 1, 1, 1, 0,
			0, 1, 1, 0, 0, 1, 0, 0,
			0, 1, 1, 0, 1, 1, 1, 1,
			0, 0, 1, 0, 0, 0, 0, 1,
		]
	});
}

// The repetition macros run at compile time, so should bench at zero.

#[bench]
fn bits_rep_u8(b: &mut Bencher) {
	b.iter(|| bitarr![u8, LocalBits; 0; 120]);
	b.iter(|| bitarr![u8, LocalBits; 1; 120]);
}

#[bench]
fn bits_rep_u16(b: &mut Bencher) {
	b.iter(|| bitarr![u16, LocalBits; 0; 120]);
	b.iter(|| bitarr![u16, LocalBits; 1; 120]);
}

#[bench]
fn bits_rep_u32(b: &mut Bencher) {
	b.iter(|| bitarr![u32, LocalBits; 0; 120]);
	b.iter(|| bitarr![u32, LocalBits; 1; 120]);
}

#[bench]
#[cfg(target_pointer_width = "64")]
fn bits_rep_u64(b: &mut Bencher) {
	b.iter(|| bitarr![u64, LocalBits; 0; 120]);
	b.iter(|| bitarr![u64, LocalBits; 1; 120]);
}

#[bench]
fn bitvec_rep(b: &mut Bencher) {
	b.iter(|| bitvec![0; 16 * 16 * 9]);
	b.iter(|| bitvec![1; 16 * 16 * 9]);
}

#[bench]
fn vec_rep(b: &mut Bencher) {
	b.iter(|| vec![0u8; 16 * 16 * 9 / 8]);
	b.iter(|| vec![-1i8; 16 * 16 * 9 / 8]);
}
