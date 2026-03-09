#![feature(test)]

extern crate test;

use bitvec::prelude::*;
use test::Bencher;

#[bench]
fn bitwise_eq(bench: &mut Bencher) {
	let a = bitarr![usize, Lsb0; 0; 500];
	let b = bitarr![usize, Msb0; 0; 500];

	bench.iter(|| {
		a.iter()
			.by_vals()
			.zip(b.iter().by_vals())
			.all(|(a, b)| a == b)
	});
}

#[bench]
fn plain_eq(bench: &mut Bencher) {
	let a = bitarr![usize, Lsb0; 0; 500];
	let b = bitarr![usize, Msb0; 0; 500];

	bench.iter(|| a == b);
}

#[bench]
fn lsb0_accel_eq(bench: &mut Bencher) {
	let a = bitarr![usize, Lsb0; 0; 500];
	let b = bitarr![usize, Lsb0; 0; 500];

	bench.iter(|| a == b);
}

#[bench]
fn msb0_accel_eq(bench: &mut Bencher) {
	let a = bitarr![usize, Msb0; 0; 500];
	let b = bitarr![usize, Msb0; 0; 500];

	bench.iter(|| a == b);
}
