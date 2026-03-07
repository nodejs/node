#![cfg(test)]

use crate::prelude::*;

#[test]
fn properties() {
	let empty = bits![];
	assert_eq!(empty.len(), 0);
	assert!(empty.is_empty());

	let bits = bits![0, 1, 0, 0, 1];
	assert_eq!(bits.len(), 5);
	assert!(!bits.is_empty());
}

#[test]
fn getters() {
	let empty = bits![mut];
	let bits = bits![mut 0, 1, 0, 0, 1];

	assert!(empty.first().is_none());
	assert!(empty.first_mut().is_none());
	assert!(empty.last().is_none());
	assert!(empty.last_mut().is_none());
	assert!(empty.split_first().is_none());
	assert!(empty.split_first_mut().is_none());
	assert!(empty.split_last().is_none());
	assert!(empty.split_last_mut().is_none());
	assert!(!bits.first().unwrap());
	assert!(bits.last().unwrap());

	*bits.first_mut().unwrap() = true;
	*bits.last_mut().unwrap() = false;

	let (first, rest) = bits.split_first().unwrap();
	assert!(*first);
	assert_eq!(rest, bits![1, 0, 0, 0]);
	let (last, rest) = bits.split_last().unwrap();
	assert!(!*last);
	assert_eq!(rest, bits![1, 1, 0, 0]);
	drop(first);
	drop(last);

	let (first, _) = bits.split_first_mut().unwrap();
	first.commit(false);
	let (last, _) = bits.split_last_mut().unwrap();
	last.commit(true);

	*bits.get_mut(2).unwrap() = true;
	unsafe {
		assert!(*bits.get_unchecked(2));
		bits.get_unchecked_mut(2).commit(false);
	}

	bits.swap(0, 4);
	bits[1 .. 4].reverse();
	assert_eq!(bits, bits![1, 0, 0, 1, 0]);
}

#[test]
fn splitters() {
	type Bsl<T> = BitSlice<T, Lsb0>;
	let mut data = 0xF0u8;
	let bits = data.view_bits_mut::<Lsb0>();

	let (l, r): (&Bsl<u8>, &Bsl<u8>) = bits.split_at(4);
	assert_eq!(l, bits![0; 4]);
	assert_eq!(r, bits![1; 4]);

	let (l, r): (
		&mut Bsl<<u8 as BitStore>::Alias>,
		&mut Bsl<<u8 as BitStore>::Alias>,
	) = bits.split_at_mut(4);
	l.fill(true);
	r.fill(false);
	assert_eq!(data, 0x0Fu8);

	let bits = bits![0, 1, 0, 0, 1];

	assert!(bits.strip_prefix(bits![1, 0]).is_none());
	assert_eq!(bits.strip_prefix(bits![0, 1]), Some(bits![0, 0, 1]));

	assert!(bits.strip_suffix(bits![1, 0]).is_none());
	assert_eq!(bits.strip_suffix(bits![0, 1]), Some(bits![0, 1, 0]));
}

#[test]
fn rotators() {
	let bits = bits![mut 0, 1, 0, 0, 1];

	bits.rotate_left(2);
	assert_eq!(bits, bits![0, 0, 1, 0, 1]);
	bits.rotate_right(2);
	assert_eq!(bits, bits![0, 1, 0, 0, 1]);

	bits.rotate_left(0);
	bits.rotate_right(0);
	bits.rotate_left(5);
	bits.rotate_right(5);
}

#[test]
#[should_panic]
fn rotate_too_far_left() {
	bits![mut 0, 1].rotate_left(3);
}

#[test]
#[should_panic]
fn rotate_too_far_right() {
	bits![mut 0, 1].rotate_right(3);
}

#[test]
fn fillers() {
	let bits = bits![mut 0; 5];

	bits.fill(true);
	assert_eq!(bits, bits![1; 5]);
	bits.fill_with(|idx| idx % 2 == 0);
	assert_eq!(bits, bits![1, 0, 1, 0, 1]);

	bits.copy_within(1 .., 0);
	assert_eq!(bits, bits![0, 1, 0, 1, 1]);
}

#[test]
fn inspectors() {
	let bits = bits![0, 1, 0, 0, 1, 0, 1, 1, 0, 1];

	assert!(bits.contains(bits![0, 1, 0, 1]));
	assert!(!bits.contains(bits![0; 4]));

	assert!(bits.starts_with(bits![0, 1, 0, 0]));
	assert!(!bits.starts_with(bits![0, 1, 1]));

	assert!(bits.ends_with(bits![1, 0, 1]));
	assert!(!bits.ends_with(bits![0, 0, 1]));
}
