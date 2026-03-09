use crate::prelude::*;

#[test]
fn extend() {
	let mut bv = bitvec![];
	bv.extend(Some(true));
	bv.extend([2usize]);

	let mut iter = bv.into_iter();
	assert!(iter.next().unwrap());
}

#[test]
fn drain() {
	let mut bv = bitvec![0, 1, 1, 1, 0];
	let mut drain = bv.drain(1 .. 4);
	assert!(drain.next().unwrap());
	assert!(drain.next_back().unwrap());
	drop(drain);
	assert_eq!(bv, bits![0; 2]);
}

#[test]
fn splice() {
	let mut bv = bitvec![0, 1, 1, 1, 0];
	let mut splice = bv.splice(1 .. 4, [false, true, true, false]);

	assert!(splice.next().unwrap());
	assert!(splice.next_back().unwrap());
	drop(splice);

	assert_eq!(bv, bits![0, 0, 1, 1, 0, 0]);

	let mut bv = bitvec![0, 1, 0, 0, 1];
	drop(bv.splice(2 .., None));
	assert_eq!(bv, bits![0, 1]);

	let mut bv = bitvec![0, 1, 0, 0, 1];
	drop(bv.splice(2 .. 2, Some(true)));
	assert_eq!(bv, bits![0, 1, 1, 0, 0, 1]);
}
