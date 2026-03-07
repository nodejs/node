//! Unit tests for bit-vectors.

#![cfg(test)]

use core::mem;

use rand::random;

use crate::{
	mem::bits_of,
	prelude::*,
};

mod api;
mod iter;
mod traits;

#[test]
fn make_and_resize() {
	let mut bv: BitVec = BitVec::new();
	assert!(bv.is_empty());
	assert_eq!(bv.capacity(), 0);

	bv.reserve(20);
	//  Capacity always rounds up to the storage size, which is an
	//  at-least-32-bit `usize`.
	assert!(bv.capacity() >= 32);
	bv.reserve_exact(90);
	assert!(bv.capacity() >= 96);

	bv = BitVec::with_capacity(100);
	assert!(bv.is_empty());
	assert!(bv.capacity() >= 128);

	bv.extend_from_bitslice(bits![0, 1, 0, 0, 1]);
	assert_eq!(bv.len(), 5);
	let (bitptr, length, capacity) = mem::take(&mut bv).into_raw_parts();
	bv = unsafe { BitVec::from_raw_parts(bitptr, length, capacity) };
	assert_eq!(bv, bits![0, 1, 0, 0, 1]);

	let capacity = bv.capacity();
	bv.shrink_to_fit();
	assert!(bv.capacity() <= capacity);

	bv.truncate(2);
	assert_eq!(bv.len(), 2);
	assert_eq!(bv, bits![0, 1]);
	bv.truncate(20);
	assert_eq!(bv.len(), 2);

	let capacity = bv.capacity();
	unsafe {
		bv.set_len(capacity);
		bv.set_elements((&false) as *const bool as usize);
	}
}

#[test]
fn misc() {
	let elem = random::<usize>();
	let bv: BitVec = BitVec::from_element(elem);
	assert_eq!(bv, elem.view_bits::<Lsb0>());

	let array: [usize; 10] = random();
	let mut bv: BitVec = BitVec::from_slice(&array[..]);
	assert_eq!(bv, array.view_bits::<Lsb0>());

	bv.extend_from_raw_slice(&[elem]);
	assert_eq!(bv[10 * bits_of::<usize>() ..], elem.view_bits::<Lsb0>());

	let elem = random::<u32>();
	let bits = &elem.view_bits::<Lsb0>()[4 .. 28];
	let mut bv = bits.to_bitvec();

	bv.set_uninitialized(false);
	bv.force_align();
	bv.set_uninitialized(true);
	bv.force_align();

	assert_eq!(!bitvec![0, 1], bits![1, 0]);
}
