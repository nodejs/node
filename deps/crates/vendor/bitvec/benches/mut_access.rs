#![feature(test)]

extern crate test;

use bitvec::prelude::*;
use test::Bencher;

#[bench]
fn iter_mut(b: &mut Bencher) {
	let mut bits = bitarr![0; 500];
	b.iter(|| bits.iter_mut().for_each(|mut b| *b = !*b));
}

#[bench]
#[allow(deprecated)]
fn native_for_each(b: &mut Bencher) {
	let mut bits = bitarr![0; 500];
	b.iter(|| bits.for_each(|_, b| !b));
}

#[bench]
fn copy_within(b: &mut Bencher) {
	let mut a = bitarr![1, 1, 1, 1, 0, 0, 0, 0];
	b.iter(|| a.copy_within(.. 4, 2));
}
